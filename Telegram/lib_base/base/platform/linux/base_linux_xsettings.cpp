// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/linux/base_linux_xsettings.h"

#include "base/platform/base_platform_info.h"

#include <QtCore/QByteArray>
#include <QtCore/QtEndian>
#include <QtGui/QColor>

#include <vector>
#include <algorithm>

namespace base::Platform::XCB {
/* Implementation of http://standards.freedesktop.org/xsettings-spec/xsettings-0.5.html */

class XSettings::PropertyValue {
public:
	PropertyValue() {
	}

	void updateValue(
			xcb_connection_t *connection,
			const QByteArray &name,
			const QVariant &value,
			int last_change_serial) {
		if (last_change_serial <= this->last_change_serial)
			return;
		this->value = value;
		this->last_change_serial = last_change_serial;
		for (const auto &callback : callback_links)
			callback.func(connection, name, value, callback.handle);
	}

	void addCallback(PropertyChangeFunc func, void *handle) {
		Callback callback = { func, handle };
		callback_links.push_back(callback);
	}

	QVariant value;
	int last_change_serial = -1;
	std::vector<Callback> callback_links;

};

class XSettings::Private {
public:
	Private()
	: connection(GetConnectionFromQt()) {
	}

	QByteArray getSettings() {
		int offset = 0;
		QByteArray settings;

		const auto _xsettings_atom = GetAtom(
			connection,
			"_XSETTINGS_SETTINGS");

		if (!_xsettings_atom)
			return settings;

		while (1) {
			const auto cookie = xcb_get_property(
				connection,
				false,
				x_settings_window,
				*_xsettings_atom,
				*_xsettings_atom,
				offset/4,
				8192);

			const auto reply = MakeReplyPointer(xcb_get_property_reply(
				connection,
				cookie,
				nullptr));

			bool more = false;
			if (!reply)
				return settings;

			const auto property_value_length = xcb_get_property_value_length(
				reply.get());

			settings.append(
				static_cast<const char *>(xcb_get_property_value(
					reply.get())),
				property_value_length);

			offset += property_value_length;
			more = reply->bytes_after != 0;

			if (!more)
				break;
		}

		return settings;
	}

	static int round_to_nearest_multiple_of_4(int value) {
		int remainder = value % 4;
		if (!remainder)
			return value;
		return value + 4 - remainder;
	}

	void populateSettings(const QByteArray &xSettings) {
		if (xSettings.length() < 12)
			return;
		char byteOrder = xSettings.at(0);
		if (byteOrder != XCB_IMAGE_ORDER_LSB_FIRST
			&& byteOrder != XCB_IMAGE_ORDER_MSB_FIRST) {
			qWarning("ByteOrder byte %d not 0 or 1", byteOrder);
			return;
		}

#define ADJUST_BO(b, t, x) \
		((b == XCB_IMAGE_ORDER_LSB_FIRST) ?                          \
		 qFromLittleEndian<t>(x) : \
		 qFromBigEndian<t>(x))
#define VALIDATE_LENGTH(x)    \
		if ((size_t)xSettings.length() < (offset + local_offset + 12 + x)) { \
			qWarning("Length %d runs past end of data", x); \
			return;                                                     \
		}

		uint number_of_settings = ADJUST_BO(
			byteOrder,
			quint32,
			xSettings.mid(8,4).constData());
		const char *data = xSettings.constData() + 12;
		size_t offset = 0;
		for (uint i = 0; i < number_of_settings; i++) {
			int local_offset = 0;
			VALIDATE_LENGTH(2);
			Type type = static_cast<Type>(
				*reinterpret_cast<const quint8 *>(data + offset));
			local_offset += 2;

			VALIDATE_LENGTH(2);
			quint16 name_len = ADJUST_BO(
				byteOrder,
				quint16,
				data + offset + local_offset);
			local_offset += 2;

			VALIDATE_LENGTH(name_len);
			QByteArray name(data + offset + local_offset, name_len);
			local_offset += round_to_nearest_multiple_of_4(name_len);

			VALIDATE_LENGTH(4);
			int last_change_serial = ADJUST_BO(
				byteOrder,
				qint32,
				data + offset + local_offset);
			Q_UNUSED(last_change_serial);
			local_offset += 4;

			QVariant value;
			if (type == Type::String) {
				VALIDATE_LENGTH(4);
				int value_length = ADJUST_BO(
					byteOrder,
					qint32,
					data + offset + local_offset);
				local_offset+=4;
				VALIDATE_LENGTH(value_length);
				QByteArray value_string(
					data + offset + local_offset,
					value_length);
				value.setValue(value_string);
				local_offset += round_to_nearest_multiple_of_4(value_length);
			} else if (type == Type::Integer) {
				VALIDATE_LENGTH(4);
				int value_length = ADJUST_BO(
					byteOrder,
					qint32,
					data + offset + local_offset);
				local_offset += 4;
				value.setValue(value_length);
			} else if (type == Type::Color) {
				VALIDATE_LENGTH(2*4);
				quint16 red = ADJUST_BO(
					byteOrder,
					quint16,
					data + offset + local_offset);
				local_offset += 2;
				quint16 green = ADJUST_BO(
					byteOrder,
					quint16,
					data + offset + local_offset);
				local_offset += 2;
				quint16 blue = ADJUST_BO(
					byteOrder,
					quint16,
					data + offset + local_offset);
				local_offset += 2;
				quint16 alpha = ADJUST_BO(
					byteOrder,
					quint16,
					data + offset + local_offset);
				local_offset += 2;
				QColor color_value(red,green,blue,alpha);
				value.setValue(color_value);
			}
			offset += local_offset;
			settings[name].updateValue(
				connection,
				name,
				value,
				last_change_serial);
		}

	}

	const not_null<xcb_connection_t*> connection;
	xcb_window_t x_settings_window = XCB_WINDOW_NONE;
	QMap<QByteArray, PropertyValue> settings;
	bool initialized = false;
};


XSettings::XSettings()
: _private(std::make_unique<Private>()) {
	const auto selection_owner_atom = GetAtom(
		_private->connection,
		"_XSETTINGS_S0");

	if (!selection_owner_atom)
		return;

	const auto selection_cookie = xcb_get_selection_owner(
		_private->connection,
		*selection_owner_atom);

	const auto selection_result = MakeReplyPointer(
		xcb_get_selection_owner_reply(
			_private->connection,
			selection_cookie,
			nullptr));

	if (!selection_result)
		return;

	_private->x_settings_window = selection_result->owner;
	if (!_private->x_settings_window)
		return;

	QCoreApplication::instance()->installNativeEventFilter(this);
	const uint32_t event = XCB_CW_EVENT_MASK;
	const uint32_t event_mask[] = {
		XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE
	};
	xcb_change_window_attributes(
		_private->connection,
		_private->x_settings_window,
		event,
		event_mask);

	_private->populateSettings(_private->getSettings());
	_private->initialized = true;
}

XSettings::~XSettings() = default;

XSettings *XSettings::Instance() {
	if (!::Platform::IsX11()) return nullptr;
	static XSettings instance;
	return &instance;
}

bool XSettings::initialized() const {
	return _private->initialized;
}

bool XSettings::nativeEventFilter(
		const QByteArray &eventType,
		void *message,
		NativeEventResult *result) {
	const auto event = static_cast<xcb_generic_event_t*>(message);
	const auto response_type = event->response_type & ~0x80;
	if (response_type != XCB_PROPERTY_NOTIFY)
		return false;

	const auto pn = reinterpret_cast<xcb_property_notify_event_t*>(event);
	if (pn->window != _private->x_settings_window)
		return false;

	_private->populateSettings(_private->getSettings());
	return false;
}

void XSettings::registerCallbackForProperty(
		const QByteArray &property,
		PropertyChangeFunc func,
		void *handle) {
	_private->settings[property].addCallback(func,handle);
}

void XSettings::removeCallbackForHandle(
		const QByteArray &property,
		void *handle) {
	auto &callbacks = _private->settings[property].callback_links;

	auto isCallbackForHandle = [handle](const Callback &cb) {
		return cb.handle == handle;
	};

	callbacks.erase(
		std::remove_if(
			callbacks.begin(),
			callbacks.end(),
			isCallbackForHandle),
		callbacks.end());
}

void XSettings::removeCallbackForHandle(void *handle) {
	for (auto it = _private->settings.cbegin()
		; it != _private->settings.cend(); ++it) {
		removeCallbackForHandle(it.key(), handle);
	}
}

QVariant XSettings::setting(const QByteArray &property) const {
	return _private->settings.value(property).value;
}

} // namespace base::Platform::XCB
