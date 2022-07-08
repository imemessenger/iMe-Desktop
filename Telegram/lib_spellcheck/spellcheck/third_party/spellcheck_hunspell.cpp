// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "spellcheck/third_party/spellcheck_hunspell.h"
#include "spellcheck/third_party/hunspell_controller.h"

namespace Platform::Spellchecker {

void Init() {
}

std::vector<QString> ActiveLanguages() {
	return ThirdParty::ActiveLanguages();
}

bool CheckSpelling(const QString &wordToCheck) {
	return ThirdParty::CheckSpelling(wordToCheck);
}

void FillSuggestionList(
		const QString &wrongWord,
		std::vector<QString> *variants) {
	ThirdParty::FillSuggestionList(wrongWord, variants);
}

void AddWord(const QString &word) {
	ThirdParty::AddWord(word);
}

void RemoveWord(const QString &word) {
	ThirdParty::RemoveWord(word);
}

void IgnoreWord(const QString &word) {
	ThirdParty::IgnoreWord(word);
}

bool IsWordInDictionary(const QString &wordToCheck) {
	return ThirdParty::IsWordInDictionary(wordToCheck);
}

void CheckSpellingText(
		const QString &text,
		MisspelledWords *misspelledWords) {
	ThirdParty::CheckSpellingText(text, misspelledWords);
}

bool IsSystemSpellchecker() {
	return false;
}

void UpdateLanguages(std::vector<int> languages) {
	ThirdParty::UpdateLanguages(languages);
}

} // namespace Platform::Spellchecker
