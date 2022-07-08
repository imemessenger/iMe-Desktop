// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "spellcheck/platform/win/spellcheck_win.h"

#include "base/platform/base_platform_info.h"
#include "spellcheck/third_party/hunspell_controller.h"

#include <wrl/client.h>
#include <spellcheck.h>

#include <QtCore/QDir>
#include <QtCore/QLocale>
#include <QVector>

using namespace Microsoft::WRL;

namespace Platform::Spellchecker {

namespace {

constexpr auto kChunk = 5000;

// Seems like ISpellChecker API has bugs for Persian language (aka Farsi).
[[nodiscard]] inline bool IsPersianLanguage(const QString &langTag) {
	return langTag.startsWith(QStringLiteral("fa"));
}

[[nodiscard]] inline LPCWSTR Q2WString(QStringView string) {
	return (LPCWSTR)string.utf16();
}

[[nodiscard]] inline auto SystemLanguages() {
	const auto appdata = qEnvironmentVariable("appdata");
	const auto dir = QDir(appdata + QString("\\Microsoft\\Spelling"));
	auto list = QStringList(SystemLanguage());
	list << (dir.exists()
		? dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)
		: QLocale::system().uiLanguages());
	list.removeDuplicates();
	return list | ranges::to_vector;
}

// WindowsSpellChecker class is used to store all the COM objects and
// control their lifetime. The class also provides wrappers for
// ISpellCheckerFactory and ISpellChecker APIs. All COM calls are on the
// background thread.
class WindowsSpellChecker {
public:
	WindowsSpellChecker();

	void addWord(LPCWSTR word);
	void removeWord(LPCWSTR word);
	void ignoreWord(LPCWSTR word);
	[[nodiscard]] bool checkSpelling(LPCWSTR word);
	void fillSuggestionList(
		LPCWSTR wrongWord,
		std::vector<QString> *optionalSuggestions);
	void checkSpellingText(
		LPCWSTR text,
		MisspelledWords *misspelledWordRanges,
		int offset);
	[[nodiscard]] std::vector<QString> systemLanguages();
	void chunkedCheckSpellingText(
		QStringView textView,
		MisspelledWords *misspelledWords);

private:
	void createFactory();
	[[nodiscard]] bool isLanguageSupported(const LPCWSTR &lang);
	void createSpellCheckers();

	ComPtr<ISpellCheckerFactory> _spellcheckerFactory;
	std::vector<std::pair<QString, ComPtr<ISpellChecker>>> _spellcheckerMap;

};

WindowsSpellChecker::WindowsSpellChecker() {
	createFactory();
	createSpellCheckers();
}

void WindowsSpellChecker::createFactory() {
	if (FAILED(CoCreateInstance(
		__uuidof(SpellCheckerFactory),
		nullptr,
		(CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER),
		IID_PPV_ARGS(&_spellcheckerFactory)))) {
		_spellcheckerFactory = nullptr;
	}
}

void WindowsSpellChecker::createSpellCheckers() {
	if (!_spellcheckerFactory) {
		return;
	}
	for (const auto &lang : SystemLanguages()) {
		const auto wlang = Q2WString(lang);
		if (!isLanguageSupported(wlang)) {
			continue;
		}
		if (ranges::contains(ranges::views::keys(_spellcheckerMap), lang)) {
			continue;
		}
		auto spellchecker = ComPtr<ISpellChecker>();
		auto hr = _spellcheckerFactory->CreateSpellChecker(
			wlang,
			&spellchecker);
		if (SUCCEEDED(hr)) {
			_spellcheckerMap.push_back({ lang, spellchecker });
		}
	}
}

bool WindowsSpellChecker::isLanguageSupported(const LPCWSTR &lang) {
	if (!_spellcheckerFactory) {
		return false;
	}

	auto isSupported = (BOOL)false;
	auto hr = _spellcheckerFactory->IsSupported(lang, &isSupported);
	return SUCCEEDED(hr) && isSupported;
}

void WindowsSpellChecker::fillSuggestionList(
		LPCWSTR wrongWord,
		std::vector<QString> *optionalSuggestions) {
	auto i = 0;
	for (const auto &[langTag, spellchecker] : _spellcheckerMap) {
		if (IsPersianLanguage(langTag)) {
			continue;
		}
		auto suggestions = ComPtr<IEnumString>();
		auto hr = spellchecker->Suggest(wrongWord, &suggestions);
		if (hr != S_OK) {
			continue;
		}

		while (true) {
			wchar_t *suggestion = nullptr;
			hr = suggestions->Next(1, &suggestion, nullptr);
			if (hr != S_OK) {
				break;
			}
			const auto guess = QString::fromWCharArray(
				suggestion,
				wcslen(suggestion));
			CoTaskMemFree(suggestion);
			if (!guess.isEmpty()) {
				optionalSuggestions->push_back(guess);
				if (++i >= kMaxSuggestions) {
					return;
				}
			}
		}
	}
}

bool WindowsSpellChecker::checkSpelling(LPCWSTR word) {
	for (const auto &[_, spellchecker] : _spellcheckerMap) {
		auto spellingErrors = ComPtr<IEnumSpellingError>();
		auto hr = spellchecker->Check(word, &spellingErrors);

		if (SUCCEEDED(hr) && spellingErrors) {
			auto spellingError = ComPtr<ISpellingError>();
			auto startIndex = ULONG(0);
			auto errorLength = ULONG(0);
			auto action = CORRECTIVE_ACTION_NONE;
			hr = spellingErrors->Next(&spellingError);
			if (SUCCEEDED(hr) &&
				spellingError &&
				SUCCEEDED(spellingError->get_StartIndex(&startIndex)) &&
				SUCCEEDED(spellingError->get_Length(&errorLength)) &&
				SUCCEEDED(spellingError->get_CorrectiveAction(&action)) &&
				(action == CORRECTIVE_ACTION_GET_SUGGESTIONS ||
					action == CORRECTIVE_ACTION_REPLACE)) {
			} else {
				return true;
			}
		}
	}
	return false;
}

void WindowsSpellChecker::checkSpellingText(
		LPCWSTR text,
		MisspelledWords *misspelledWordRanges,
		int offset) {
	// The spellchecker marks words not from its own language as misspelled.
	// So we only return words that are marked
	// as misspelled in all spellcheckers.
	auto misspelledWords = MisspelledWords();

	constexpr auto isActionGood = [](auto action) {
		return action == CORRECTIVE_ACTION_GET_SUGGESTIONS
			|| action == CORRECTIVE_ACTION_REPLACE;
	};

	for (const auto &[langTag, spellchecker] : _spellcheckerMap) {
		auto spellingErrors = ComPtr<IEnumSpellingError>();

		auto hr = IsPersianLanguage(langTag)
			? spellchecker->Check(text, &spellingErrors)
			: spellchecker->ComprehensiveCheck(text, &spellingErrors);
		if (!(SUCCEEDED(hr) && spellingErrors)) {
			continue;
		}

		auto tempMisspelled = MisspelledWords();
		auto spellingError = ComPtr<ISpellingError>();
		for (; hr == S_OK; hr = spellingErrors->Next(&spellingError)) {
			auto startIndex = ULONG(0);
			auto errorLength = ULONG(0);
			auto action = CORRECTIVE_ACTION_NONE;

			if (!(SUCCEEDED(hr)
				&& spellingError
				&& SUCCEEDED(spellingError->get_StartIndex(&startIndex))
				&& SUCCEEDED(spellingError->get_Length(&errorLength))
				&& SUCCEEDED(spellingError->get_CorrectiveAction(&action))
				&& isActionGood(action))) {
				continue;
			}
			const auto word = std::pair(
				(int)startIndex + offset,
				(int)errorLength);
			if (misspelledWords.empty()
				|| ranges::contains(misspelledWords, word)) {
				tempMisspelled.push_back(std::move(word));
			}
		}
		// If the tempMisspelled vector is empty at least once,
		// it means that the all words will be correct in the end
		// and it makes no sense to check other languages.
		if (tempMisspelled.empty()) {
			return;
		}
		misspelledWords = std::move(tempMisspelled);
	}
	if (offset) {
		for (auto &m : misspelledWords) {
			misspelledWordRanges->push_back(std::move(m));
		}
	} else {
		*misspelledWordRanges = misspelledWords;
	}
}

void WindowsSpellChecker::addWord(LPCWSTR word) {
	for (const auto &[_, spellchecker] : _spellcheckerMap) {
		spellchecker->Add(word);
	}
}

void WindowsSpellChecker::removeWord(LPCWSTR word) {
	for (const auto &[_, spellchecker] : _spellcheckerMap) {
		auto spellchecker2 = ComPtr<ISpellChecker2>();
		spellchecker->QueryInterface(IID_PPV_ARGS(&spellchecker2));
		if (spellchecker2) {
			spellchecker2->Remove(word);
		}
	}
}

void WindowsSpellChecker::ignoreWord(LPCWSTR word) {
	for (const auto &[_, spellchecker] : _spellcheckerMap) {
		spellchecker->Ignore(word);
	}
}

std::vector<QString> WindowsSpellChecker::systemLanguages() {
	return ranges::views::keys(_spellcheckerMap) | ranges::to_vector;
}

void WindowsSpellChecker::chunkedCheckSpellingText(
		QStringView textView,
		MisspelledWords *misspelledWords) {
	auto i = 0;
	auto chunkBuffer = std::vector<wchar_t>();

	while (i != textView.size()) {
		const auto provisionalChunkSize = std::min(
			kChunk,
			int(textView.size() - i));
		const auto chunkSize = [&] {
			const auto until = std::max(
				0,
				provisionalChunkSize - ::Spellchecker::kMaxWordSize);
			for (auto n = provisionalChunkSize; n > until; n--) {
				if (textView.at(i + n - 1).isLetterOrNumber()) {
					continue;
				} else {
					return n;
				}
			}
			return provisionalChunkSize;
		}();
		const auto chunk = textView.mid(i, chunkSize);

		chunkBuffer.resize(chunk.size() + 1);
		const auto count = chunk.toWCharArray(chunkBuffer.data());
		chunkBuffer[count] = '\0';

		checkSpellingText(
			(LPCWSTR)chunkBuffer.data(),
			misspelledWords,
			i);
		i += chunk.size();
	}
}

////// End of WindowsSpellChecker class.

WindowsSpellChecker &SharedSpellChecker() {
	static auto spellchecker = WindowsSpellChecker();
	return spellchecker;
}

} // namespace

// TODO: Add a better work with the Threading Models.
// All COM objects should be created asynchronously
// if we want to work with them asynchronously.
// Some calls can be made in the main thread before spellchecking
// (e.g. KnownLanguages), so we have to init it asynchronously first.
void Init() {
	if (IsSystemSpellchecker()) {
		crl::async(SharedSpellChecker);
	}
}

bool IsSystemSpellchecker() {
	// Windows 7 does not support spellchecking.
	// https://docs.microsoft.com/en-us/windows/win32/api/spellcheck/nn-spellcheck-ispellchecker
	return IsWindows8OrGreater();
}

std::vector<QString> ActiveLanguages() {
	if (IsSystemSpellchecker()) {
		return SharedSpellChecker().systemLanguages();
	}
	return ThirdParty::ActiveLanguages();
}

bool CheckSpelling(const QString &wordToCheck) {
	if (!IsSystemSpellchecker()) {
		return ThirdParty::CheckSpelling(wordToCheck);
	}
	return SharedSpellChecker().checkSpelling(Q2WString(wordToCheck));
}

void FillSuggestionList(
		const QString &wrongWord,
		std::vector<QString> *optionalSuggestions) {
	if (IsSystemSpellchecker()) {
		SharedSpellChecker().fillSuggestionList(
			Q2WString(wrongWord),
			optionalSuggestions);
		return;
	}
	ThirdParty::FillSuggestionList(
		wrongWord,
		optionalSuggestions);
}

void AddWord(const QString &word) {
	if (IsSystemSpellchecker()) {
		SharedSpellChecker().addWord(Q2WString(word));
	} else {
		ThirdParty::AddWord(word);
	}
}

void RemoveWord(const QString &word) {
	if (IsSystemSpellchecker()) {
		SharedSpellChecker().removeWord(Q2WString(word));
	} else {
		ThirdParty::RemoveWord(word);
	}
}

void IgnoreWord(const QString &word) {
	if (IsSystemSpellchecker()) {
		SharedSpellChecker().ignoreWord(Q2WString(word));
	} else {
		ThirdParty::IgnoreWord(word);
	}
}

bool IsWordInDictionary(const QString &wordToCheck) {
	if (IsSystemSpellchecker()) {
		// ISpellChecker can't check if a word is in the dictionary.
		return false;
	}
	return ThirdParty::IsWordInDictionary(wordToCheck);
}

void UpdateLanguages(std::vector<int> languages) {
	if (!IsSystemSpellchecker()) {
		ThirdParty::UpdateLanguages(languages);
		return;
	}
	crl::async([=] {
		const auto result = ActiveLanguages();
		crl::on_main([=] {
			::Spellchecker::UpdateSupportedScripts(result);
		});
	});
}

void CheckSpellingText(
		const QString &text,
		MisspelledWords *misspelledWords) {
	if (IsSystemSpellchecker()) {
		// There are certain strings with a lot of 'paragraph separators'
		// that crash the native Windows spellchecker. We replace them
		// with spaces (no difference for the checking), they don't crash.
		const auto check = QString(text).replace(QChar(8233), QChar(32));
		if (check.size() > kChunk) {
			// On some versions of Windows 10,
			// checking large text with specific characters (e.g. @)
			// will throw the std::regex_error::error_complexity exception,
			// so we have to split the text.
			SharedSpellChecker().chunkedCheckSpellingText(
				check,
				misspelledWords);
		} else {
			SharedSpellChecker().checkSpellingText(
				(LPCWSTR)check.utf16(),
				misspelledWords,
				0);
		}

		return;
	}
	ThirdParty::CheckSpellingText(text, misspelledWords);
}


} // namespace Platform::Spellchecker
