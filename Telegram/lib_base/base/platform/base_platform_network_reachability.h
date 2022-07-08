// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

namespace base::Platform {

[[nodiscard]] std::optional<bool> NetworkAvailable();
[[nodiscard]] rpl::producer<> NetworkAvailableChanged();
void NotifyNetworkAvailableChanged();
[[nodiscard]] inline bool NetworkAvailableSupported() {
	return NetworkAvailable().has_value();
}

} // namespace base::Platform
