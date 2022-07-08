// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <QtCore/QObject>
#include <QtCore/QThread>
#include "base/flat_map.h"

#include <crl/crl_time.h>

namespace base {

void CheckLocalTime();

class Timer final : private QObject {
public:
	explicit Timer(
		not_null<QThread*> thread,
		Fn<void()> callback = nullptr);
	explicit Timer(Fn<void()> callback = nullptr);

	static Qt::TimerType DefaultType(crl::time timeout) {
		constexpr auto kThreshold = crl::time(240);
		return (timeout > kThreshold) ? Qt::CoarseTimer : Qt::PreciseTimer;
	}

	void setCallback(Fn<void()> callback) {
		_callback = std::move(callback);
	}

	void callOnce(crl::time timeout) {
		callOnce(timeout, DefaultType(timeout));
	}

	void callEach(crl::time timeout) {
		callEach(timeout, DefaultType(timeout));
	}

	void callOnce(crl::time timeout, Qt::TimerType type) {
		start(timeout, type, Repeat::SingleShot);
	}

	void callEach(crl::time timeout, Qt::TimerType type) {
		start(timeout, type, Repeat::Interval);
	}

	bool isActive() const {
		return (_timerId != 0);
	}

	void cancel();
	crl::time remainingTime() const;

	static void Adjust();

protected:
	void timerEvent(QTimerEvent *e) override;

private:
	enum class Repeat : unsigned {
		Interval   = 0,
		SingleShot = 1,
	};
	void start(crl::time timeout, Qt::TimerType type, Repeat repeat);
	void adjust();

	void setTimeout(crl::time timeout);
	int timeout() const;

	void setRepeat(Repeat repeat) {
		_repeat = static_cast<unsigned>(repeat);
	}
	Repeat repeat() const {
		return static_cast<Repeat>(_repeat);
	}

	Fn<void()> _callback;
	crl::time _next = 0;
	int _timeout = 0;
	int _timerId = 0;

	Qt::TimerType _type : 2;
	bool _adjusted : 1 = false;
	unsigned _repeat : 1 = 0;

};

class DelayedCallTimer final : private QObject {
public:
	int call(crl::time timeout, FnMut<void()> callback) {
		return call(
			timeout,
			std::move(callback),
			Timer::DefaultType(timeout));
	}

	int call(
		crl::time timeout,
		FnMut<void()> callback,
		Qt::TimerType type);
	void cancel(int callId);

protected:
	void timerEvent(QTimerEvent *e) override;

private:
	base::flat_map<int, FnMut<void()>> _callbacks;

};

} // namespace base
