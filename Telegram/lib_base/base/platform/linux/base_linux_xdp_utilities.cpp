// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/linux/base_linux_xdp_utilities.h"

#include "base/platform/linux/base_linux_glibmm_helper.h"
#include "base/platform/linux/base_linux_wayland_integration.h"
#include "base/platform/base_platform_info.h"

#include <glibmm.h>
#include <giomm.h>

#include <QtGui/QWindow>

namespace base::Platform::XDP {

Glib::ustring ParentWindowID(QWindow *window) {
	std::stringstream result;
	if (!window) {
		return result.str();
	}

	if (const auto integration = WaylandIntegration::Instance()) {
		if (const auto handle = integration->nativeHandle(window)
			; !handle.isEmpty()) {
			result << "wayland:" << handle.toStdString();
		}
	} else if (::Platform::IsX11()) {
		result << "x11:" << std::hex << window->winId();
	}

	return result.str();
}

std::optional<Glib::VariantBase> ReadSetting(
		const Glib::ustring &group,
		const Glib::ustring &key) {
	try {
		const auto connection = Gio::DBus::Connection::get_sync(
			Gio::DBus::BusType::BUS_TYPE_SESSION);

		auto reply = connection->call_sync(
			std::string(kObjectPath),
			std::string(kSettingsInterface),
			"Read",
			MakeGlibVariant(std::tuple{
				group,
				key,
			}),
			std::string(kService));

		return GlibVariantCast<Glib::VariantBase>(
			GlibVariantCast<Glib::VariantBase>(reply.get_child(0)));
	} catch (...) {
	}

	return std::nullopt;
}

class SettingWatcher::Private {
public:
	Glib::RefPtr<Gio::DBus::Connection> dbusConnection;
	uint signalId = 0;
};

SettingWatcher::SettingWatcher(
		Fn<void(
			const Glib::ustring &,
			const Glib::ustring &,
			const Glib::VariantBase &)> callback)
: _private(std::make_unique<Private>()) {
	try {
		_private->dbusConnection = Gio::DBus::Connection::get_sync(
			Gio::DBus::BusType::BUS_TYPE_SESSION);
		if (!_private->dbusConnection) {
			return;
		}

		_private->signalId = _private->dbusConnection->signal_subscribe(
			[=](
				const Glib::RefPtr<Gio::DBus::Connection> &connection,
				const Glib::ustring &sender_name,
				const Glib::ustring &object_path,
				const Glib::ustring &interface_name,
				const Glib::ustring &signal_name,
				const Glib::VariantContainerBase &parameters) {
				try {
					auto parametersCopy = parameters;

					const auto group = GlibVariantCast<Glib::ustring>(
						parametersCopy.get_child(0));

					const auto key = GlibVariantCast<Glib::ustring>(
						parametersCopy.get_child(1));

					const auto value = GlibVariantCast<Glib::VariantBase>(
						parametersCopy.get_child(2));

					callback(group, key, value);
				} catch (...) {
				}
			},
			std::string(kService),
			std::string(kSettingsInterface),
			"SettingChanged",
			std::string(kObjectPath));
	} catch (...) {
	}
}

SettingWatcher::~SettingWatcher() {
	if (_private->dbusConnection && _private->signalId != 0) {
		_private->dbusConnection->signal_unsubscribe(_private->signalId);
	}
}

} // namespace base::Platform::XDP
