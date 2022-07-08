// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "spellcheck/spellcheck_value.h"

namespace ph {

phrase lng_spellchecker_submenu = "Spelling";
phrase lng_spellchecker_add = "Add to Dictionary";
phrase lng_spellchecker_remove = "Remove from Dictionary";
phrase lng_spellchecker_ignore = "Ignore word";

} // namespace ph

namespace Spellchecker {
namespace {

QString WorkingDir = QString();

} // namespace

QString WorkingDirPath() {
	return WorkingDir;
}

void SetWorkingDirPath(const QString &path) {
	WorkingDir = path;
}

} // namespace Spellchecker
