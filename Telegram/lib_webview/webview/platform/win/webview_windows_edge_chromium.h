// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "webview/platform/win/webview_win.h"

namespace Webview::EdgeChromium {

[[nodiscard]] bool Supported();
[[nodiscard]] std::unique_ptr<Interface> CreateInstance(Config config);

} // namespace Webview::EdgeChromium
