// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/last_user_input.h"

#include "base/event_filter.h"
#include "base/platform/base_platform_last_input.h"

#include <QtCore/QCoreApplication>

namespace base {

crl::time LastUserInputTime() {
	Expects(QCoreApplication::instance() != nullptr);

	if (const auto specific = base::Platform::LastUserInputTime()) {
		return *specific;
	}
	static auto result = crl::time(0);
	static const auto isInputEvent = [](not_null<QEvent*> e) {
		switch (e->type()) {
		case QEvent::MouseMove:
		case QEvent::KeyPress:
		case QEvent::MouseButtonPress:
		case QEvent::TouchBegin:
		case QEvent::Wheel: return true;
		}
		return false;
	};
	static const auto updateResult = [](not_null<QEvent*> e) {
		if (isInputEvent(e)) {
			result = crl::now();
		}
		return base::EventFilterResult::Continue;
	};
	[[maybe_unused]] static const auto watcher = base::install_event_filter(
		QCoreApplication::instance(),
		updateResult);
	return result;
}

crl::time SinceLastUserInput() {
	return crl::now() - LastUserInputTime();
}

} // namespace base
