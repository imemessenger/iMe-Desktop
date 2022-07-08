// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// Author: Nicholas Guriev <guriev-ns@ya.ru>, public domain, 2019
// License: CC0, https://creativecommons.org/publicdomain/zero/1.0/legalcode

#include <set>
#include <QLocale>

#include "spellcheck/platform/linux/linux_enchant.h"

#include "spellcheck/platform/linux/spellcheck_linux.h"
#include "base/debug_log.h"

namespace Platform::Spellchecker {
namespace {

constexpr auto kHspell = "hspell";
constexpr auto kMySpell = "myspell";
constexpr auto kHunspell = "hunspell";
constexpr auto kOrdering = "hspell,aspell,hunspell,myspell";
constexpr auto kMaxValidators = 10;
constexpr auto kMaxMySpellCount = 3;
constexpr auto kMaxWordLength = 15;

using DictPtr = std::unique_ptr<enchant::Dict>;

auto CheckProvider(DictPtr &validator, const std::string &provider) {
	auto p = validator->get_provider_name();
	std::transform(begin(p), end(p), begin(p), ::tolower);
	return (p.find(provider) == 0); // startsWith.
}

auto IsHebrew(const QString &word) {
	// Words with mixed scripts will be automatically ignored,
	// so this check should be fine.
	return ::Spellchecker::WordScript(&word) == QChar::Script_Hebrew;
}

class EnchantSpellChecker {
public:
	auto knownLanguages();
	bool checkSpelling(const QString &word);
	auto findSuggestions(const QString &word);
	void addWord(const QString &wordToAdd);
	void ignoreWord(const QString &word);
	void removeWord(const QString &word);
	bool isWordInDictionary(const QString &word);
	static EnchantSpellChecker *instance();

private:
	EnchantSpellChecker();
	EnchantSpellChecker(const EnchantSpellChecker&) = delete;
	EnchantSpellChecker& operator =(const EnchantSpellChecker&) = delete;

	std::unique_ptr<enchant::Broker> _brokerHandle;
	std::vector<DictPtr> _validators;

	std::vector<not_null<enchant::Dict*>> _hspells;
};

EnchantSpellChecker::EnchantSpellChecker() {
	if (!enchant::loader::do_explicit_linking()) return;
	std::set<std::string> langs;
	_brokerHandle = std::make_unique<enchant::Broker>();
	_brokerHandle->list_dicts([](
			const char *language,
			const char *provider,
			const char *description,
			const char *filename,
			void *our_payload) {
		static_cast<decltype(langs)*>(our_payload)->insert(language);
	}, &langs);
	_validators.reserve(langs.size());
	try {
		std::string langTag = QLocale::system().name().toStdString();
		_brokerHandle->set_ordering(langTag, kOrdering);
		_validators.push_back(DictPtr(_brokerHandle->request_dict(langTag)));
		langs.erase(langTag);
	} catch (const enchant::Exception &e) {
		// no first dictionary found
	}
	auto mySpellCount = 0;
	for (const std::string &language : langs) {
		try {
			_brokerHandle->set_ordering(language, kOrdering);
			auto validator = DictPtr(_brokerHandle->request_dict(language));
			if (!validator) {
				continue;
			}
			if (CheckProvider(validator, kHspell)) {
				_hspells.push_back(validator.get());
			}
			if (CheckProvider(validator, kMySpell)
				|| CheckProvider(validator, kHunspell)) {
				if (mySpellCount > kMaxMySpellCount) {
					continue;
				} else {
					mySpellCount++;
				}
			}
			_validators.push_back(std::move(validator));
			if (_validators.size() > kMaxValidators) {
				break;
			}
		} catch (const enchant::Exception &e) {
			DEBUG_LOG(("Catch after request_dict: %1").arg(e.what()));
		}
	}
}

EnchantSpellChecker *EnchantSpellChecker::instance() {
	static EnchantSpellChecker capsule;
	return &capsule;
}

auto EnchantSpellChecker::knownLanguages() {
	return _validators | ranges::views::transform([](const auto &validator) {
		return QString(validator->get_lang().c_str());
	}) | ranges::to_vector;
}

bool EnchantSpellChecker::checkSpelling(const QString &word) {
	auto w = word.toStdString();

	const auto checkWord = [&](const auto &validator, auto w) {
		try {
			return validator->check(w);
		} catch (const enchant::Exception &e) {
			DEBUG_LOG(("Catch after check '%1': %2").arg(word, e.what()));
			return true;
		}
	};

	if (IsHebrew(word) && _hspells.size()) {
		return ranges::any_of(_hspells, [&](const auto &validator) {
			return checkWord(validator, w);
		});
	}
	return ranges::any_of(_validators, [&](const auto &validator) {
		// Hspell is the spell checker that only checks words in Hebrew.
		// It returns 'true' for any non-Hebrew word,
		// so we should skip Hspell if a word is not in Hebrew.
		if (ranges::any_of(_hspells, [&](auto &v) {
				return v == validator.get();
			})) {
			return false;
		}
		if (validator->get_lang().find("uk") == 0) {
			return false;
		}
		return checkWord(validator, w);
	}) || _validators.empty();
}

auto EnchantSpellChecker::findSuggestions(const QString &word) {
	const auto wordScript = ::Spellchecker::WordScript(&word);
	auto w = word.toStdString();
	std::vector<QString> result;
	if (!_validators.size()) {
		return result;
	}

	const auto convertSuggestions = [&](auto suggestions) {
		for (const auto &replacement : suggestions) {
			if (result.size() >= kMaxSuggestions) {
				break;
			}
			if (!replacement.empty()) {
				result.push_back(replacement.c_str());
			}
		}
	};

	if (word.size() >= kMaxWordLength) {
		// The first element is the validator of the system language.
		auto *v = _validators[0].get();
		const auto lang = QString::fromStdString(v->get_lang());
		if (wordScript == ::Spellchecker::LocaleToScriptCode(lang)) {
			convertSuggestions(v->suggest(w));
		}
		return result;
	}

	if (IsHebrew(word) && _hspells.size()) {
		for (const auto &h : _hspells) {
			convertSuggestions(h->suggest(w));
			if (result.size()) {
				return result;
			}
		}
	}
	for (const auto &validator : _validators) {
		const auto lang = QString::fromStdString(validator->get_lang());
		if (wordScript != ::Spellchecker::LocaleToScriptCode(lang)) {
			continue;
		}
		convertSuggestions(validator->suggest(w));
		if (!result.empty()) {
			break;
		}
	}
	return result;
}

void EnchantSpellChecker::addWord(const QString &wordToAdd) {
	auto word = wordToAdd.toStdString();
	auto &&first = _validators.at(0);
	first->add(word);
	first->add_to_session(word);
}

void EnchantSpellChecker::ignoreWord(const QString &word) {
	_validators.at(0)->add_to_session(word.toStdString());
}

void EnchantSpellChecker::removeWord(const QString &word) {
	auto w = word.toStdString();
	for (const auto &validator : _validators) {
		validator->remove_from_session(w);
		validator->remove(w);
	}
}

bool EnchantSpellChecker::isWordInDictionary(const QString &word) {
	auto w = word.toStdString();
	return ranges::any_of(_validators, [&w](const auto &validator) {
		return validator->is_added(w);
	});
}

} // namespace

void Init() {
}

std::vector<QString> ActiveLanguages() {
	return EnchantSpellChecker::instance()->knownLanguages();
}

void UpdateLanguages(std::vector<int> languages) {
	::Spellchecker::UpdateSupportedScripts(ActiveLanguages());
	crl::async([=] {
		const auto result = ActiveLanguages();
		crl::on_main([=] {
			::Spellchecker::UpdateSupportedScripts(result);
		});
	});
}

bool CheckSpelling(const QString &wordToCheck) {
	return EnchantSpellChecker::instance()->checkSpelling(wordToCheck);
}

void FillSuggestionList(
		const QString &wrongWord,
		std::vector<QString> *variants) {
	*variants = EnchantSpellChecker::instance()->findSuggestions(wrongWord);
}

void AddWord(const QString &word) {
	EnchantSpellChecker::instance()->addWord(word);
}

void RemoveWord(const QString &word) {
	EnchantSpellChecker::instance()->removeWord(word);
}

void IgnoreWord(const QString &word) {
	EnchantSpellChecker::instance()->ignoreWord(word);
}

bool IsWordInDictionary(const QString &wordToCheck) {
	return EnchantSpellChecker::instance()->isWordInDictionary(wordToCheck);
}

void CheckSpellingText(
		const QString &text,
		MisspelledWords *misspelledWords) {
	*misspelledWords = ::Spellchecker::RangesFromText(
		text,
		::Spellchecker::CheckSkipAndSpell);
}

bool IsSystemSpellchecker() {
	return true;
}

} // namespace Platform::Spellchecker
