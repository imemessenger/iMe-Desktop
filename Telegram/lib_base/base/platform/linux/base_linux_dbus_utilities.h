// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/basic_types.h"
#include "base/const_string.h"

#include <glibmm.h>
#include <giomm.h>

namespace base::Platform::DBus {

inline constexpr auto kDBusService = "org.freedesktop.DBus"_cs;
inline constexpr auto kDBusObjectPath = "/org/freedesktop/DBus"_cs;
inline constexpr auto kDBusInterface = kDBusService;

enum class StartReply {
	Success,
	AlreadyRunning,
};

bool NameHasOwner(
	const Glib::RefPtr<Gio::DBus::Connection> &connection,
	const Glib::ustring &name);

std::vector<Glib::ustring> ListActivatableNames(
	const Glib::RefPtr<Gio::DBus::Connection> &connection);

StartReply StartServiceByName(
	const Glib::RefPtr<Gio::DBus::Connection> &connection,
	const Glib::ustring &name);

void StartServiceByNameAsync(
	const Glib::RefPtr<Gio::DBus::Connection> &connection,
	const Glib::ustring &name,
	Fn<void(Fn<StartReply()>)> callback);

uint RegisterServiceWatcher(
	const Glib::RefPtr<Gio::DBus::Connection> &connection,
	const Glib::ustring &service,
	Fn<void(
		const Glib::ustring &,
		const Glib::ustring &,
		const Glib::ustring &)> callback);

class ServiceWatcher {
public:
	ServiceWatcher(
		const Glib::RefPtr<Gio::DBus::Connection> &connection,
		const Glib::ustring &service,
		Fn<void(
			const Glib::ustring &,
			const Glib::ustring &,
			const Glib::ustring &)> callback)
	: _connection(connection)
	, _signalId(RegisterServiceWatcher(connection, service, callback)) {
	}

	~ServiceWatcher() {
		if (!_connection || !_signalId) {
			return;
		}
		_connection->signal_unsubscribe(_signalId);
	}

private:
	Glib::RefPtr<Gio::DBus::Connection> _connection;
	uint _signalId = 0;
};

} // namespace base::Platform::DBus
