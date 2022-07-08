// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "spellcheck/spellcheck_types.h"

#include <QLocale>

namespace Spellchecker {

constexpr auto kMaxWordSize = 99;

QChar::Script LocaleToScriptCode(const QString &locale);
QChar::Script WordScript(QStringView word);
bool IsWordSkippable(
	QStringView word,
	bool checkSupportedScripts = true);

MisspelledWords RangesFromText(
	const QString &text,
	Fn<bool(const QString &word)> filterCallback);

// For Linux and macOS, which use RangesFromText.
bool CheckSkipAndSpell(const QString &word);

QLocale LocaleFromLangId(int langId);

void UpdateSupportedScripts(std::vector<QString> languages);
rpl::producer<> SupportedScriptsChanged();

} // namespace Spellchecker
