// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "spellcheck/platform/platform_spellcheck.h"

namespace Platform::Spellchecker::ThirdParty {

[[nodiscard]] bool CheckSpelling(const QString &wordToCheck);
[[nodiscard]] bool IsWordInDictionary(const QString &wordToCheck);

std::vector<QString> ActiveLanguages();
void FillSuggestionList(
	const QString &wrongWord,
	std::vector<QString> *optionalSuggestions);

void AddWord(const QString &word);
void RemoveWord(const QString &word);
void IgnoreWord(const QString &word);

void CheckSpellingText(
	const QString &text,
	MisspelledWords *misspelledWords);

void UpdateLanguages(std::vector<int> languages);

} // namespace Platform::Spellchecker::ThirdParty
