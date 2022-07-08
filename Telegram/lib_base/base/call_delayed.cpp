// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/call_delayed.h"

#include "base/timer.h"

#include <QtCore/QCoreApplication>

namespace base {
namespace {

DelayedCallTimer *GlobalTimer = nullptr;
bool Finished = false;

void CreateGlobalTimer() {
	Expects(QCoreApplication::instance() != nullptr);
	Expects(!GlobalTimer);

	const auto instance = QCoreApplication::instance();
	Assert(instance != nullptr);

	GlobalTimer = new DelayedCallTimer();
	instance->connect(instance, &QCoreApplication::aboutToQuit, [] {
		Finished = true;
	});
	instance->connect(instance, &QCoreApplication::destroyed, [] {
		Finished = true;
		delete GlobalTimer;
		GlobalTimer = nullptr;
	});
}

} // namespace

void call_delayed(crl::time delay, FnMut<void()> &&callable) {
	if (Finished) {
		return;
	}
	if (!GlobalTimer) {
		CreateGlobalTimer();
	}
	GlobalTimer->call(delay, std::move(callable));
}

} // namespace base
