/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "base/platform/linux/base_linux_wayland_integration.h"

#include "base/platform/base_platform_info.h"

namespace base {
namespace Platform {

struct WaylandIntegration::Private {
};

WaylandIntegration::WaylandIntegration() {
}
	
WaylandIntegration::~WaylandIntegration() = default;

WaylandIntegration *WaylandIntegration::Instance() {
	if (!::Platform::IsWayland()) return nullptr;
	static WaylandIntegration instance;
	return &instance;
}

QString WaylandIntegration::nativeHandle(QWindow *window) {
	return {};
}

void WaylandIntegration::preventDisplaySleep(bool prevent, QWindow *window) {
}

} // namespace Platform
} // namespace base
