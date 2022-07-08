// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/basic_types.h"

#include <QtCore/QObject>

namespace base {

enum class EventFilterResult {
	Continue,
	Cancel,
};

not_null<QObject*> install_event_filter(
	not_null<QObject*> object,
	Fn<EventFilterResult(not_null<QEvent*>)> filter);

not_null<QObject*> install_event_filter(
	not_null<QObject*> context,
	not_null<QObject*> object,
	Fn<EventFilterResult(not_null<QEvent*>)> filter);

namespace details {

class EventFilter : public QObject {
public:
	EventFilter(
		not_null<QObject*> parent,
		not_null<QObject*> object,
		Fn<EventFilterResult(not_null<QEvent*>)> filter);

protected:
	bool eventFilter(QObject *watched, QEvent *event);

private:
	Fn<EventFilterResult(not_null<QEvent*>)> _filter;

};

} // namespace details
} // namespace base
