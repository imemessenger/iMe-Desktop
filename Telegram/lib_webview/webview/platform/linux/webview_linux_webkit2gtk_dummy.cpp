// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "webview/platform/linux/webview_linux_webkit2gtk.h"

namespace Webview::WebKit2Gtk {

Available Availability() {
	return Available{
		.error = Available::Error::NoGtkOrWebkit2Gtk,
		.details = "This feature was disabled at build time.",
	};
}

std::unique_ptr<Interface> CreateInstance(Config config) {
	return nullptr;
}

int Exec() {
	return 1;
}

void SetSocketPath(const std::string &socketPath) {
}

void SetDebug(const std::string &debug) {
}

} // namespace Webview::WebKit2Gtk
