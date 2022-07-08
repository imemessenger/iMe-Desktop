// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/platform/base_platform_info.h"

namespace Platform {

inline OutdateReason WhySystemBecomesOutdated() {
	return OutdateReason::IsOld;
}

inline constexpr bool IsLinux() {
	return true;
}

inline constexpr bool IsWindows() { return false; }
inline constexpr bool IsWindows32Bit() { return false; }
inline constexpr bool IsWindows64Bit() { return false; }
inline constexpr bool IsWindowsStoreBuild() { return false; }
inline bool IsWindows7OrGreater() { return false; }
inline bool IsWindows8OrGreater() { return false; }
inline bool IsWindows8Point1OrGreater() { return false; }
inline bool IsWindows10OrGreater() { return false; }
inline bool IsWindows11OrGreater() { return false; }
inline constexpr bool IsMac() { return false; }
inline constexpr bool IsMacStoreBuild() { return false; }
inline bool IsMac10_12OrGreater() { return false; }
inline bool IsMac10_13OrGreater() { return false; }
inline bool IsMac10_14OrGreater() { return false; }
inline bool IsMac10_15OrGreater() { return false; }
inline bool IsMac11_0OrGreater() { return false; }

} // namespace Platform
