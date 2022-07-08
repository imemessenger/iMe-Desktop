// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/linux/base_power_save_blocker_linux.h"

#include "base/platform/base_platform_info.h"
#include "base/platform/linux/base_linux_wayland_integration.h"
#include "base/timer_rpl.h"
#include "base/random.h"

#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
#include "base/platform/linux/base_linux_xcb_utilities.h"
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION

#ifndef DESKTOP_APP_DISABLE_DBUS_INTEGRATION
#include "base/platform/linux/base_linux_xdp_utilities.h"

#include <giomm.h>
#endif // !DESKTOP_APP_DISABLE_DBUS_INTEGRATION

#include <QtGui/QWindow>
#include <QtWidgets/QWidget>

namespace base::Platform {
namespace {

#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
constexpr auto kResetScreenSaverTimeout = 10 * crl::time(1000);

// Use the basic reset API
// due to https://gitlab.freedesktop.org/xorg/xserver/-/issues/363
void XCBPreventDisplaySleep(bool prevent) {
	static rpl::lifetime lifetime;
	if (!prevent) {
		lifetime.destroy();
		return;
	} else if (lifetime) {
		return;
	}

	base::timer_each(
		kResetScreenSaverTimeout
	) | rpl::start_with_next([] {
		const auto connection = XCB::GetConnectionFromQt();
		if (!connection) {
			return;
		}
		xcb_force_screen_saver(connection, XCB_SCREEN_SAVER_RESET);
	}, lifetime);
}
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION

#if 0 //ndef DESKTOP_APP_DISABLE_DBUS_INTEGRATION
void PortalPreventAppSuspension(
	bool prevent,
	const QString &description,
	QWindow *window) {
	try {
		const auto connection = Gio::DBus::Connection::get_sync(
			Gio::DBus::BusType::BUS_TYPE_SESSION);

		static Glib::ustring requestPath;
		if (!prevent && !requestPath.empty()) {
			connection->call_sync(
				requestPath,
				std::string(XDP::kRequestInterface),
				"Close",
				{},
				std::string(XDP::kService));
			requestPath = "";
			return;
		} else if (!(prevent && requestPath.empty())) {
			return;
		}

		const auto handleToken = Glib::ustring("desktop_app")
			+ std::to_string(base::RandomValue<uint>());

		auto uniqueName = connection->get_unique_name();
		uniqueName.erase(0, 1);
		uniqueName.replace(uniqueName.find('.'), 1, 1, '_');

		requestPath = Glib::ustring(
				"/org/freedesktop/portal/desktop/request/")
			+ uniqueName
			+ '/'
			+ handleToken;

		const auto loop = Glib::MainLoop::create();

		const auto signalId = connection->signal_subscribe(
			[&](
				const Glib::RefPtr<Gio::DBus::Connection> &connection,
				const Glib::ustring &sender_name,
				const Glib::ustring &object_path,
				const Glib::ustring &interface_name,
				const Glib::ustring &signal_name,
				const Glib::VariantContainerBase &parameters) {
				loop->quit();
			},
			std::string(XDP::kService),
			std::string(XDP::kRequestInterface),
			"Response",
			requestPath);

		const auto signalGuard = gsl::finally([&] {
			if (signalId != 0) {
				connection->signal_unsubscribe(signalId);
			}
		});

		connection->call_sync(
			std::string(XDP::kObjectPath),
			"org.freedesktop.portal.Inhibit",
			"Inhibit",
			Glib::VariantContainerBase::create_tuple({
				Glib::Variant<Glib::ustring>::create(
					XDP::ParentWindowID(window)),
				Glib::Variant<uint>::create(4), // Suspend
				Glib::Variant<std::map<
					Glib::ustring,
					Glib::VariantBase
				>>::create({
					{
						"handle_token",
						Glib::Variant<Glib::ustring>::create(handleToken)
					},
					{
						"reason",
						Glib::Variant<Glib::ustring>::create(
							description.toStdString())
					},
				}),
			}),
			std::string(XDP::kService));

		if (signalId != 0) {
			QWidget tempWindow;
			tempWindow.setAttribute(Qt::WA_DontShowOnScreen);
			tempWindow.setWindowModality(Qt::ApplicationModal);
			tempWindow.show();
			loop->run();
		}
	} catch (...) {
	}
}
#endif // !DESKTOP_APP_DISABLE_DBUS_INTEGRATION

} // namespace

void BlockPowerSave(
	PowerSaveBlockType type,
	const QString &description,
	QPointer<QWindow> window) {
	switch (type) {
	case PowerSaveBlockType::PreventAppSuspension:
#if 0 // ndef DESKTOP_APP_DISABLE_DBUS_INTEGRATION
		PortalPreventAppSuspension(true, description, window);
#endif // !DESKTOP_APP_DISABLE_DBUS_INTEGRATION
		break;
	case PowerSaveBlockType::PreventDisplaySleep:
		if (const auto integration = WaylandIntegration::Instance()) {
			integration->preventDisplaySleep(true, window);
		} else if (::Platform::IsX11()) {
#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
			XCBPreventDisplaySleep(true);
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION
		}
		break;
	}
}

void UnblockPowerSave(PowerSaveBlockType type, QPointer<QWindow> window) {
	switch (type) {
	case PowerSaveBlockType::PreventAppSuspension:
#if 0 // ndef DESKTOP_APP_DISABLE_DBUS_INTEGRATION
		PortalPreventAppSuspension(false, {}, window);
#endif // !DESKTOP_APP_DISABLE_DBUS_INTEGRATION
		break;
	case PowerSaveBlockType::PreventDisplaySleep:
		if (const auto integration = WaylandIntegration::Instance()) {
			integration->preventDisplaySleep(false, window);
		} else if (::Platform::IsX11()) {
#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
			XCBPreventDisplaySleep(false);
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION
		}
		break;
	}
}

} // namespace base::Platform
