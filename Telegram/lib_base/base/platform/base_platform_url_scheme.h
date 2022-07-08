// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

namespace base::Platform {

struct UrlSchemeDescriptor {
	QString executable; // Full path.
	QString arguments; // Additional arguments.
	QString protocol; // 'myprotocol'
	QString protocolName; // "My Protocol Link"
	QString shortAppName; // "myapp"
	QString longAppName; // "MyApplication"
	QString displayAppName; // "My Application"
	QString displayAppDescription; // "My Nice Application"
};

[[nodiscard]] bool CheckUrlScheme(const UrlSchemeDescriptor &descriptor);
void RegisterUrlScheme(const UrlSchemeDescriptor &descriptor);
void UnregisterUrlScheme(const UrlSchemeDescriptor &descriptor);

} // namespace base::Platform
