// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/win/base_layout_switch_win.h"

#include "base/platform/win/base_windows_h.h"

#include <WinUser.h>
#include <minwindef.h>

namespace base::Platform {
namespace {

constexpr auto kMaxLayoutsCount = 1024;

} // namespace

bool SwitchKeyboardLayoutToEnglish() {
	auto listSize = 0;
	listSize = GetKeyboardLayoutList(0, nullptr);
	if (!listSize || listSize > kMaxLayoutsCount) {
		return false;
	}
	const auto current = GetKeyboardLayout(0);
	const auto value = quintptr(current) & 0xFF;
	auto list = std::vector<HKL>(listSize, nullptr);
	GetKeyboardLayoutList(list.size(), list.data());
	auto selectedLayout = HKL();
	auto selectedLevel = 0;
	const auto offer = [&](HKL layout, int level) {
		if (level > selectedLevel) {
			selectedLayout = layout;
			selectedLevel = level;
		}
	};
	for (const auto layout : list) {
		const auto value = quintptr(layout);
		const auto languageId = reinterpret_cast<quintptr>(layout) & 0xFFFF;
		const auto primaryId = languageId & 0x03FF;
		const auto sublanguageId = (languageId >> 10);
		if (primaryId == LANG_ENGLISH) {
			if (sublanguageId == SUBLANG_ENGLISH_US) {
				offer(layout, 3);
			} else if (sublanguageId == SUBLANG_ENGLISH_UK) {
				offer(layout, 2);
			} else {
				offer(layout, 1);
			}
		}
	}
	if (!selectedLayout) {
		return false;
	}
	const auto previous = ActivateKeyboardLayout(
		selectedLayout,
		KLF_SETFORPROCESS);
	return (previous != nullptr);
}

} // namespace base::Platform
