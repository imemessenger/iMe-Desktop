// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/algorithm.h"

#include "base/debug_log.h"

#include <cfenv>

namespace base {

[[nodiscard]] double SafeRound(double value) {
	Expects(!std::isnan(value));

	if (const auto result = std::round(value); !std::isnan(result)) {
		return result;
	}
	const auto errors = std::fetestexcept(FE_ALL_EXCEPT);
	if (const auto result = std::round(value); !std::isnan(result)) {
		return result;
	}
	LOG(("Streaming Error: Got second NAN in std::round(%1), fe: %2."
		).arg(value
		).arg(errors));
	std::feclearexcept(FE_ALL_EXCEPT);
	if (const auto result = std::round(value); !std::isnan(result)) {
		return result;
	}
	Unexpected("NAN after third std::round.");
}

QString CleanAndSimplify(QString text) {
	for (auto &ch : text) {
		if (ch.unicode() < 32) {
			ch = QChar(' ');
		}
	}
	return text.simplified();
}

} // namespace base
