// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

class QString;

namespace base::Platform {

#ifdef Q_OS_MAC

std::optional<uint64> SetCustomAppIcon(QImage image);
std::optional<uint64> SetCustomAppIcon(const QString &path);
std::optional<uint64> CurrentCustomAppIconDigest();
bool ClearCustomAppIcon();

#else // Q_OS_MAC

inline std::optional<uint64> SetCustomAppIcon(QImage image) {
	return std::nullopt;
}

inline std::optional<uint64> SetCustomAppIcon(const QString &path) {
	return std::nullopt;
}

inline std::optional<uint64> CurrentCustomAppIconDigest() {
	return std::nullopt;
}

inline bool ClearCustomAppIcon() {
	return false;
}

#endif // Q_OS_MAC

} // namespace base::Platform
