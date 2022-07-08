// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/platform/linux/base_linux_xcb_utilities.h"
#include "base/qt/qt_common_adapters.h"

#include <QtCore/QAbstractNativeEventFilter>

namespace base::Platform::XCB {

class XSettings : public QAbstractNativeEventFilter {
public:
	static XSettings *Instance();
	bool initialized() const;

	QVariant setting(const QByteArray &property) const;

	typedef void (*PropertyChangeFunc)(
		xcb_connection_t *connection,
		const QByteArray &name,
		const QVariant &property,
		void *handle);

	void registerCallbackForProperty(
		const QByteArray &property,
		PropertyChangeFunc func,
		void *handle);

	void removeCallbackForHandle(const QByteArray &property, void *handle);
	void removeCallbackForHandle(void *handle);

private:
	XSettings();
	~XSettings();

	bool nativeEventFilter(
		const QByteArray &eventType,
		void *message,
		NativeEventResult *result) override;

	enum class Type {
		Integer,
		String,
		Color,
	};

	struct Callback {
		PropertyChangeFunc func;
		void *handle;
	};

	class PropertyValue;

	class Private;
	const std::unique_ptr<Private> _private;
};

} // namespace base::Platform::XCB
