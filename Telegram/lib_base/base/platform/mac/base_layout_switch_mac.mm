// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/mac/base_layout_switch_mac.h"

#include <Carbon/Carbon.h>
#include <Foundation/Foundation.h>

namespace base::Platform {

bool SwitchKeyboardLayoutToEnglish() {
	auto result = false;

	@autoreleasepool {

	const auto kEnglish = base::flat_map<NSString*, int>{
		{ @"com.apple.keylayout.USExtended", 0 },
		{ @"com.apple.keylayout.ABC", 0 },
		{ @"com.apple.keylayout.Australian", 1 },
		{ @"com.apple.keylayout.British", 2 },
		{ @"com.apple.keylayout.British-PC", 3 },
		{ @"com.apple.keylayout.Canadian", 1 },
		{ @"com.apple.keylayout.Colemak", 1 },
		{ @"com.apple.keylayout.Dvorak", 0 },
		{ @"com.apple.keylayout.Dvorak-Left", 0 },
		{ @"com.apple.keylayout.DVORAK-QWERTYCMD", 0 },
		{ @"com.apple.keylayout.Dvorak-Right", 0 },
		{ @"com.apple.keylayout.Irish", 1 },
		{ @"com.apple.keylayout.USInternational-PC", 4 },
		{ @"com.apple.keylayout.US", 5 },
	};

	auto selectedLayout = (NSObject*)NULL;
	auto selectedLevel = 0;
	const auto offer = [&](NSObject *layout, int level) {
		if (level > selectedLevel) {
			selectedLayout = layout;
			selectedLevel = level;
		}
	};

	NSArray *list = [(NSArray *)TISCreateInputSourceList(NULL,NO) autorelease];
	for (NSObject *layout in list) {
		NSString *layoutId = (NSString*)TISGetInputSourceProperty(
			(TISInputSourceRef)layout,
			kTISPropertyInputSourceID);
		for (const auto &[checkId, checkLevel] : kEnglish) {
			if ([layoutId isEqualToString:checkId]) {
				offer(layout, checkLevel);
			}
		}
	}

	if (selectedLayout != nullptr) {
		TISSelectInputSource(TISInputSourceRef(selectedLayout));
		result = true;
	}

	}
	return result;
}

} // namespace base::Platform
