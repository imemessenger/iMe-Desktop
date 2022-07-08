// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <xcb/xcb.h>

namespace base::Platform::XCB {

struct ConnectionDeleter {
	void operator()(xcb_connection_t *value) {
		xcb_disconnect(value);
	}
};

using ConnectionPointer = std::unique_ptr<xcb_connection_t, ConnectionDeleter>;

class CustomConnection : public ConnectionPointer {
public:
	CustomConnection()
	: ConnectionPointer(xcb_connect(nullptr, nullptr)) {
	}

	[[nodiscard]] operator xcb_connection_t*() const {
		return get();
	}
};

template <typename T>
struct ReplyDeleter {
	void operator()(T *value) {
		free(value);
	}
};

template <typename T>
using ReplyPointer = std::unique_ptr<T, ReplyDeleter<T>>;

template <typename T>
ReplyPointer<T> MakeReplyPointer(T *reply) {
	return ReplyPointer<T>(reply);
}

xcb_connection_t *GetConnectionFromQt();

std::optional<xcb_timestamp_t> GetTimestamp();

std::optional<xcb_window_t> GetRootWindow(xcb_connection_t *connection);

std::optional<xcb_atom_t> GetAtom(
		xcb_connection_t *connection,
		const QString &name);

bool IsExtensionPresent(
		xcb_connection_t *connection,
		xcb_extension_t *ext);

std::vector<xcb_atom_t> GetWMSupported(
		xcb_connection_t *connection,
		xcb_window_t root);

std::optional<xcb_window_t> GetSupportingWMCheck(
		xcb_connection_t *connection,
		xcb_window_t root);

// convenient API, checks connection for nullptr
bool IsSupportedByWM(xcb_connection_t *connection, const QString &atomName);

} // namespace base::Platform::XCB
