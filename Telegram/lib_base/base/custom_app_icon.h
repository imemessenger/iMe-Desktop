// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/platform/base_platform_custom_app_icon.h"

namespace base {

inline std::optional<uint64> SetCustomAppIcon(QImage image) {
	return Platform::SetCustomAppIcon(std::move(image));
}

inline std::optional<uint64> SetCustomAppIcon(const QString &path) {
	return Platform::SetCustomAppIcon(path);
}

inline std::optional<uint64> CurrentCustomAppIconDigest() {
	return Platform::CurrentCustomAppIconDigest();
}

inline bool ClearCustomAppIcon() {
	return Platform::ClearCustomAppIcon();
}

} // namespace base
