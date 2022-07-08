// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/event_filter.h"

namespace base {
namespace details {

EventFilter::EventFilter(
	not_null<QObject*> parent,
	not_null<QObject*> object,
	Fn<EventFilterResult(not_null<QEvent*>)> filter)
: QObject(parent)
, _filter(std::move(filter)) {
	object->installEventFilter(this);
}

bool EventFilter::eventFilter(QObject *watched, QEvent *event) {
	return (_filter(event) == EventFilterResult::Cancel);
}

} // namespace details

not_null<QObject*> install_event_filter(
		not_null<QObject*> object,
		Fn<EventFilterResult(not_null<QEvent*>)> filter) {
	return install_event_filter(object, object, std::move(filter));
}

not_null<QObject*> install_event_filter(
		not_null<QObject*> context,
		not_null<QObject*> object,
		Fn<EventFilterResult(not_null<QEvent*>)> filter) {
	return new details::EventFilter(context, object, std::move(filter));
}

} // namespace Core
