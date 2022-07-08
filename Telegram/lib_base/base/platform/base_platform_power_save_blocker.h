// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/power_save_blocker.h"

namespace base::Platform {

inline constexpr int kPowerSaveBlockTypeCount = static_cast<int>(
	PowerSaveBlockType::kCount);

[[nodiscard]] inline constexpr int PowerSaveBlockTypeIndex(
		PowerSaveBlockType type) {
	Expects(static_cast<int>(type) >= 0);
	Expects(type < PowerSaveBlockType::kCount);

	return static_cast<int>(type);
}

// window may be null.
void BlockPowerSave(
	PowerSaveBlockType type,
	const QString &description,
	QPointer<QWindow> window);
void UnblockPowerSave(PowerSaveBlockType type, QPointer<QWindow> window);

} // namespace base::Platform
