// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/mac/base_url_scheme_mac.h"

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

namespace base::Platform {

bool CheckUrlScheme(const UrlSchemeDescriptor &descriptor) {
	const auto name = descriptor.protocol.toStdString();
	const auto str = CFStringCreateWithCString(nullptr, name.c_str(), kCFStringEncodingASCII);
	const auto current = LSCopyDefaultHandlerForURLScheme(str);
	const auto result = CFStringCompare(
		current,
		(CFStringRef)[[NSBundle mainBundle] bundleIdentifier],
		kCFCompareCaseInsensitive);
	CFRelease(str);
	return (result == kCFCompareEqualTo);
}

void RegisterUrlScheme(const UrlSchemeDescriptor &descriptor) {
	const auto name = descriptor.protocol.toStdString();
	const auto str = CFStringCreateWithCString(nullptr, name.c_str(), kCFStringEncodingASCII);
	LSSetDefaultHandlerForURLScheme(
		str,
		(CFStringRef)[[NSBundle mainBundle] bundleIdentifier]);
	CFRelease(str);
}

void UnregisterUrlScheme(const UrlSchemeDescriptor &descriptor) {
	// TODO
}

} // namespace base::Platform
