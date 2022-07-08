// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/mac/base_last_input_mac.h"

#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

namespace base::Platform {

// Taken from https://github.com/trueinteractions/tint/issues/53.
std::optional<crl::time> LastUserInputTime() {
	CFMutableDictionaryRef properties = 0;
	CFTypeRef obj;
	mach_port_t masterPort;
	io_iterator_t iter;
	io_registry_entry_t curObj;

	IOMasterPort(MACH_PORT_NULL, &masterPort);

	/* Get IOHIDSystem */
	IOServiceGetMatchingServices(masterPort, IOServiceMatching("IOHIDSystem"), &iter);
	if (iter == 0) {
		return std::nullopt;
	} else {
		curObj = IOIteratorNext(iter);
	}
	if (IORegistryEntryCreateCFProperties(curObj, &properties, kCFAllocatorDefault, 0) == KERN_SUCCESS && properties != NULL) {
		obj = CFDictionaryGetValue(properties, CFSTR("HIDIdleTime"));
		CFRetain(obj);
	} else {
		return std::nullopt;
	}

	uint64 err = ~0L, idleTime = err;
	if (obj) {
		CFTypeID type = CFGetTypeID(obj);

		if (type == CFDataGetTypeID()) {
			CFDataGetBytes((CFDataRef) obj, CFRangeMake(0, sizeof(idleTime)), (UInt8*)&idleTime);
		} else if (type == CFNumberGetTypeID()) {
			CFNumberGetValue((CFNumberRef)obj, kCFNumberSInt64Type, &idleTime);
		} else {
			// error
		}

		CFRelease(obj);

		if (idleTime != err) {
			idleTime /= 1000000; // return as ms
		}
	} else {
		// error
	}

	CFRelease((CFTypeRef)properties);
	IOObjectRelease(curObj);
	IOObjectRelease(iter);
	if (idleTime == err) {
		return std::nullopt;
	}
	return (crl::now() - static_cast<crl::time>(idleTime));
}

} // namespace base::Platform
