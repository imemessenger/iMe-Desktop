// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/linux/base_last_input_linux.h"

#include "base/platform/base_platform_info.h"
#include "base/debug_log.h"

#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
#include "base/platform/linux/base_linux_xcb_utilities.h"
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION

#ifndef DESKTOP_APP_DISABLE_DBUS_INTEGRATION
#include "base/platform/linux/base_linux_glibmm_helper.h"

#include <glibmm.h>
#include <giomm.h>
#endif // !DESKTOP_APP_DISABLE_DBUS_INTEGRATION

#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
#include <xcb/screensaver.h>
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION

namespace base::Platform {
namespace {

#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
std::optional<crl::time> XCBLastUserInputTime() {
	const auto connection = XCB::GetConnectionFromQt();
	if (!connection) {
		return std::nullopt;
	}

	if (!XCB::IsExtensionPresent(connection, &xcb_screensaver_id)) {
		return std::nullopt;
	}

	const auto root = XCB::GetRootWindow(connection);
	if (!root.has_value()) {
		return std::nullopt;
	}

	const auto cookie = xcb_screensaver_query_info(
		connection,
		*root);

	const auto reply = XCB::MakeReplyPointer(
		xcb_screensaver_query_info_reply(
			connection,
			cookie,
			nullptr));

	if (!reply) {
		return std::nullopt;
	}

	return (crl::now() - static_cast<crl::time>(reply->ms_since_user_input));
}
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION

#ifndef DESKTOP_APP_DISABLE_DBUS_INTEGRATION
std::optional<crl::time> FreedesktopDBusLastUserInputTime() {
	static auto NotSupported = false;

	if (NotSupported) {
		return std::nullopt;
	}

	try {
		const auto connection = [] {
			try {
				return Gio::DBus::Connection::get_sync(
					Gio::DBus::BusType::BUS_TYPE_SESSION);
			} catch (...) {
				return Glib::RefPtr<Gio::DBus::Connection>();
			}
		}();

		if (!connection) {
			NotSupported = true;
			return std::nullopt;
		}

		auto reply = connection->call_sync(
			"/org/freedesktop/ScreenSaver",
			"org.freedesktop.ScreenSaver",
			"GetSessionIdleTime",
			{},
			"org.freedesktop.ScreenSaver");

		const auto value = GlibVariantCast<uint>(reply.get_child(0));
		return (crl::now() - static_cast<crl::time>(value));
	} catch (const Glib::Error &e) {
		static const auto NotSupportedErrors = {
			"org.freedesktop.DBus.Error.ServiceUnknown",
			"org.freedesktop.DBus.Error.NotSupported",
		};

		static const auto NotSupportedErrorsToLog = {
			"org.freedesktop.DBus.Error.AccessDenied",
		};

		const auto errorName = Gio::DBus::ErrorUtils::get_remote_error(e);
		if (ranges::contains(NotSupportedErrors, errorName)) {
			NotSupported = true;
			return std::nullopt;
		} else if (ranges::contains(NotSupportedErrorsToLog, errorName)) {
			NotSupported = true;
		}

		LOG(("Unable to get last user input time "
			"from org.freedesktop.ScreenSaver: %1")
			.arg(QString::fromStdString(e.what())));
	} catch (const std::exception &e) {
		LOG(("Unable to get last user input time "
			"from org.freedesktop.ScreenSaver: %1")
			.arg(QString::fromStdString(e.what())));
	}

	return std::nullopt;
}

std::optional<crl::time> MutterDBusLastUserInputTime() {
	static auto NotSupported = false;

	if (NotSupported) {
		return std::nullopt;
	}

	try {
		const auto connection = [] {
			try {
				return Gio::DBus::Connection::get_sync(
					Gio::DBus::BusType::BUS_TYPE_SESSION);
			} catch (...) {
				return Glib::RefPtr<Gio::DBus::Connection>();
			}
		}();

		if (!connection) {
			NotSupported = true;
			return std::nullopt;
		}

		auto reply = connection->call_sync(
			"/org/gnome/Mutter/IdleMonitor/Core",
			"org.gnome.Mutter.IdleMonitor",
			"GetIdletime",
			{},
			"org.gnome.Mutter.IdleMonitor");

		const auto value = GlibVariantCast<guint64>(reply.get_child(0));
		return (crl::now() - static_cast<crl::time>(value));
	} catch (const Glib::Error &e) {
		static const auto NotSupportedErrors = {
			"org.freedesktop.DBus.Error.ServiceUnknown",
		};

		static const auto NotSupportedErrorsToLog = {
			"org.freedesktop.DBus.Error.AccessDenied",
		};

		const auto errorName = Gio::DBus::ErrorUtils::get_remote_error(e);
		if (ranges::contains(NotSupportedErrors, errorName)) {
			NotSupported = true;
			return std::nullopt;
		} else if (ranges::contains(NotSupportedErrorsToLog, errorName)) {
			NotSupported = true;
		}

		LOG(("Unable to get last user input time "
			"from org.gnome.Mutter.IdleMonitor: %1")
			.arg(QString::fromStdString(e.what())));
	} catch (const std::exception &e) {
		LOG(("Unable to get last user input time "
			"from org.gnome.Mutter.IdleMonitor: %1")
			.arg(QString::fromStdString(e.what())));
	}

	return std::nullopt;
}
#endif // !DESKTOP_APP_DISABLE_DBUS_INTEGRATION

} // namespace

std::optional<crl::time> LastUserInputTime() {
#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
	if (::Platform::IsX11()) {
		const auto xcbResult = XCBLastUserInputTime();
		if (xcbResult.has_value()) {
			return xcbResult;
		}
	}
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION

#ifndef DESKTOP_APP_DISABLE_DBUS_INTEGRATION
	const auto freedesktopResult = FreedesktopDBusLastUserInputTime();
	if (freedesktopResult.has_value()) {
		return freedesktopResult;
	}

	const auto mutterResult = MutterDBusLastUserInputTime();
	if (mutterResult.has_value()) {
		return mutterResult;
	}
#endif // !DESKTOP_APP_DISABLE_DBUS_INTEGRATION

	return std::nullopt;
}

} // namespace base::Platform
