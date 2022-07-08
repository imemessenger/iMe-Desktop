// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/linux/base_linux_xcb_utilities.h"

#include "base/qt/qt_common_adapters.h"

#include <QtCore/QAbstractNativeEventFilter>
#include <QtGui/QGuiApplication>

#if QT_VERSION < QT_VERSION_CHECK(6, 2, 0)
#include <qpa/qplatformnativeinterface.h>
#endif // Qt < 6.2.0

namespace base::Platform::XCB {
namespace {

class TimestampGetter : public QAbstractNativeEventFilter {
public:
	TimestampGetter() {
	}

	std::optional<xcb_timestamp_t> get() {
		_connection = GetConnectionFromQt();
		if (!_connection) {
			return std::nullopt;
		}

		const auto window = GetRootWindow(_connection);
		if (!window.has_value()) {
			return std::nullopt;
		}

		const auto atom = GetAtom(_connection, "CLIP_TEMPORARY");
		if (!atom.has_value()) {
			return std::nullopt;
		}

		_window = *window;
		_atom = *atom;

		QCoreApplication::instance()->installNativeEventFilter(this);

		xcb_change_property(
			_connection,
			XCB_PROP_MODE_APPEND,
			_window,
			_atom,
			XCB_ATOM_INTEGER,
			32,
			0,
			nullptr);
		
		xcb_flush(_connection);
		sync();
		_loop.exec();
		xcb_delete_property(_connection, _window, _atom);

		return _timestamp;
	}

private:
	bool nativeEventFilter(
			const QByteArray &eventType,
			void *message,
			NativeEventResult *result) override {
		const auto guard = gsl::finally([&] {
			_connection = GetConnectionFromQt();
			if (!_connection || xcb_connection_has_error(_connection)) {
				_loop.quit();
			}

			if (_loop.isRunning()) {
				sync();
			}
		});

		const auto event = reinterpret_cast<xcb_generic_event_t*>(message);
		if ((event->response_type & ~0x80) != XCB_PROPERTY_NOTIFY) {
			return false;
		}

		const auto pn = reinterpret_cast<xcb_property_notify_event_t*>(event);
		if (pn->window != _window || pn->atom != _atom) {
			return false;
		}

		_timestamp = pn->time;
		_loop.quit();
		return false;
	}

	void sync() {
		const auto cookie = xcb_get_input_focus(_connection);
		free(xcb_get_input_focus_reply(_connection, cookie, nullptr));
	}

	QEventLoop _loop;
	xcb_connection_t *_connection = nullptr;
	xcb_window_t _window = XCB_WINDOW_NONE;
	xcb_atom_t _atom = XCB_ATOM_NONE;
	std::optional<xcb_timestamp_t> _timestamp;
};

} // namespace

xcb_connection_t *GetConnectionFromQt() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
	using namespace QNativeInterface;
	const auto native = qApp->nativeInterface<QX11Application>();
#else // Qt >= 6.2.0
	const auto native = QGuiApplication::platformNativeInterface();
#endif // Qt < 6.2.0
	if (!native) {
		return nullptr;
	}

#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
	return native->connection();
#else // Qt >= 6.2.0
	return reinterpret_cast<xcb_connection_t*>(
		native->nativeResourceForIntegration(QByteArray("connection")));
#endif // Qt < 6.2.0
}

std::optional<xcb_timestamp_t> GetTimestamp() {
	return TimestampGetter().get();
}

std::optional<xcb_window_t> GetRootWindow(xcb_connection_t *connection) {
	const auto screen = xcb_setup_roots_iterator(
		xcb_get_setup(connection)).data;

	if (!screen) {
		return std::nullopt;
	}

	return screen->root;
}

std::optional<xcb_atom_t> GetAtom(
		xcb_connection_t *connection,
		const QString &name) {
	const auto cookie = xcb_intern_atom(
		connection,
		0,
		name.size(),
		name.toUtf8().constData());

	const auto reply = MakeReplyPointer(xcb_intern_atom_reply(
		connection,
		cookie,
		nullptr));

	if (!reply) {
		return std::nullopt;
	}

	return reply->atom;
}

bool IsExtensionPresent(
		xcb_connection_t *connection,
		xcb_extension_t *ext) {
	const auto reply = xcb_get_extension_data(
		connection,
		ext);

	if (!reply) {
		return false;
	}

	return reply->present;
}

std::vector<xcb_atom_t> GetWMSupported(
		xcb_connection_t *connection,
		xcb_window_t root) {
	auto netWmAtoms = std::vector<xcb_atom_t>{};

	const auto supportedAtom = GetAtom(connection, "_NET_SUPPORTED");
	if (!supportedAtom.has_value()) {
		return netWmAtoms;
	}

	auto offset = 0;
	auto remaining = 0;

	do {
		const auto cookie = xcb_get_property(
			connection,
			false,
			root,
			*supportedAtom,
			XCB_ATOM_ATOM,
			offset,
			1024);

		const auto reply = MakeReplyPointer(xcb_get_property_reply(
			connection,
			cookie,
			nullptr));

		if (!reply) {
			break;
		}

		remaining = 0;

		if (reply->type == XCB_ATOM_ATOM && reply->format == 32) {
			const auto len = xcb_get_property_value_length(reply.get())
				/ sizeof(xcb_atom_t);

			const auto atoms = reinterpret_cast<xcb_atom_t*>(
				xcb_get_property_value(reply.get()));

			const auto s = netWmAtoms.size();
			netWmAtoms.resize(s + len);
			memcpy(netWmAtoms.data() + s, atoms, len * sizeof(xcb_atom_t));

			remaining = reply->bytes_after;
			offset += len;
		}
	} while (remaining > 0);

	return netWmAtoms;
}

std::optional<xcb_window_t> GetSupportingWMCheck(
		xcb_connection_t *connection,
		xcb_window_t root) {
	const auto supportingAtom = base::Platform::XCB::GetAtom(
		connection,
		"_NET_SUPPORTING_WM_CHECK");

	if (!supportingAtom.has_value()) {
		return std::nullopt;
	}

	const auto cookie = xcb_get_property(
		connection,
		false,
		root,
		*supportingAtom,
		XCB_ATOM_WINDOW,
		0,
		1024);

	const auto reply = MakeReplyPointer(xcb_get_property_reply(
		connection,
		cookie,
		nullptr));

	if (!reply) {
		return std::nullopt;
	}

	return (reply->format == 32 && reply->type == XCB_ATOM_WINDOW)
		? std::optional<xcb_window_t>{
			*reinterpret_cast<xcb_window_t*>(
				xcb_get_property_value(reply.get()))
		} : std::nullopt;
}

bool IsSupportedByWM(xcb_connection_t *connection, const QString &atomName) {
	// for inline GetConnectionFromQt use
	if (!connection) {
		return false;
	}

	const auto root = GetRootWindow(connection);
	if (!root.has_value()) {
		return false;
	}

	const auto atom = GetAtom(connection, atomName);
	if (!atom.has_value()) {
		return false;
	}

	return ranges::contains(GetWMSupported(connection, *root), *atom);
}

} // namespace base::Platform::XCB
