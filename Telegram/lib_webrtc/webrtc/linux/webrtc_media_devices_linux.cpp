// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "webrtc/linux/webrtc_media_devices_linux.h"

namespace Webrtc {
namespace {

// Taken from DesktopCapturer::IsRunningUnderWayland
// src/modules/desktop_capture/desktop_capture.cc

#if defined(WEBRTC_USE_PIPEWIRE) || defined(WEBRTC_USE_X11)
[[nodiscard]] bool IsRunningUnderWayland() {
  const char* xdg_session_type = getenv("XDG_SESSION_TYPE");
  if (!xdg_session_type || strncmp(xdg_session_type, "wayland", 7) != 0)
    return false;

  if (!(getenv("WAYLAND_DISPLAY")))
    return false;

  return true;
}
#endif  // defined(WEBRTC_USE_PIPEWIRE) || defined(WEBRTC_USE_X11)

} // namespace

std::optional<QString> LinuxUniqueDesktopCaptureSource() {
#ifdef WEBRTC_USE_PIPEWIRE
	if (IsRunningUnderWayland()) {
		return u"desktop_capturer_pipewire"_q;
	}
#endif // WEBRTC_USE_PIPEWIRE

	return std::nullopt;
}

} // namespace Webrtc
