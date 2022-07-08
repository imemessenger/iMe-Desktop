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

inline constexpr bool IsWindows() {
	return true;
}

inline constexpr bool IsWindows32Bit() {
#ifdef Q_PROCESSOR_X86_32
	return true;
#else // Q_PROCESSOR_X86_32
	return false;
#endif // Q_PROCESSOR_X86_32
}

inline constexpr bool IsWindows64Bit() {
#ifdef Q_PROCESSOR_X86_64
	return true;
#else // Q_PROCESSOR_X86_64
	return false;
#endif // Q_PROCESSOR_X86_64
}

inline constexpr bool IsWindowsStoreBuild() {
#ifdef OS_WIN_STORE
	return true;
#else // OS_WIN_STORE
	return false;
#endif // OS_WIN_STORE
}

inline constexpr bool IsMac() { return false; }
inline constexpr bool IsMacStoreBuild() { return false; }
inline bool IsMac10_12OrGreater() { return false; }
inline bool IsMac10_13OrGreater() { return false; }
inline bool IsMac10_14OrGreater() { return false; }
inline bool IsMac10_15OrGreater() { return false; }
inline bool IsMac11_0OrGreater() { return false; }
inline constexpr bool IsLinux() { return false; }
inline bool IsX11() { return false; }
inline bool IsWayland() { return false; }
inline QString GetLibcName() { return QString(); }
inline QString GetLibcVersion() { return QString(); }
inline QString GetWindowManager() { return QString(); }

} // namespace Platform
