// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/assertion.h"

#include "base/integration.h"

namespace base::assertion {

void log(const char *message, const char *file, int line) {
	if (Integration::Exists()) {
		const auto info = message
			+ QString(' ')
			+ file
			+ ':'
			+ QString::number(line);
		Integration::Instance().logAssertionViolation(info);
	}
}

} // namespace base::assertion
