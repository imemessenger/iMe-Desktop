// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ui/ph.h"

namespace ph {

extern phrase lng_spellchecker_submenu;
extern phrase lng_spellchecker_add;
extern phrase lng_spellchecker_remove;
extern phrase lng_spellchecker_ignore;

} // namespace ph

namespace Spellchecker {

////// Phrases.

inline constexpr auto kPhrasesCount = 4;

inline void SetPhrases(ph::details::phrase_value_array<kPhrasesCount> data) {
	ph::details::set_values(std::move(data));
}

//////

[[nodiscard]] QString WorkingDirPath();
void SetWorkingDirPath(const QString &path);

} // namespace Spellchecker
