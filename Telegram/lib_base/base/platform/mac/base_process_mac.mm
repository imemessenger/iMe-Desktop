// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/mac/base_process_mac.h"

#include <Cocoa/Cocoa.h>

namespace base::Platform {

void ActivateProcessWindow(int64 pid, WId windowId) {
}

void ActivateThisProcessWindow(WId windowId) {
	[NSApp activateIgnoringOtherApps:YES];
	if (const auto view = reinterpret_cast<NSView*>(windowId)) {
		[[view window] makeKeyAndOrderFront:NSApp];
	}
}

} // namespace base::Platform
