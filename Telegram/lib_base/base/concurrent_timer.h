// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/binary_guard.h"
#include <crl/crl_time.h>
#include <crl/crl_object_on_queue.h>
#include <QtCore/QThread>

namespace base {
namespace details {

class TimerObject;

class TimerObjectWrap {
public:
	explicit TimerObjectWrap(Fn<void()> adjust);
	~TimerObjectWrap();

	void call(
		crl::time timeout,
		Qt::TimerType type,
		FnMut<void()> method);
	void cancel();

private:
	void sendEvent(std::unique_ptr<QEvent> event);

	std::unique_ptr<TimerObject> _value;

};

} // namespace details

class ConcurrentTimerEnvironment {
public:
	ConcurrentTimerEnvironment();
	~ConcurrentTimerEnvironment();

	std::unique_ptr<details::TimerObject> createTimer(Fn<void()> adjust);

	static void Adjust();

private:
	void acquire();
	void release();
	void adjustTimers();

	QThread _thread;
	QObject _adjuster;

};

class ConcurrentTimer {
public:
	explicit ConcurrentTimer(
		Fn<void(FnMut<void()>)> runner,
		Fn<void()> callback = nullptr);

	template <typename Policy, typename Object>
	explicit ConcurrentTimer(
		crl::details::weak_async<Policy, Object> weak,
		Fn<void()> callback = nullptr);

	static Qt::TimerType DefaultType(crl::time timeout) {
		constexpr auto kThreshold = crl::time(1000);
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
		return _running.alive();
	}

	void cancel();
	crl::time remainingTime() const;

private:
	enum class Repeat : unsigned {
		Interval = 0,
		SingleShot = 1,
	};
	Fn<void()> createAdjuster();
	void start(crl::time timeout, Qt::TimerType type, Repeat repeat);
	void adjust();

	void cancelAndSchedule(int timeout);

	void setTimeout(crl::time timeout);
	int timeout() const;

	void timerEvent();

	void setRepeat(Repeat repeat) {
		_repeat = static_cast<unsigned>(repeat);
	}
	Repeat repeat() const {
		return static_cast<Repeat>(_repeat);
	}

	Fn<void(FnMut<void()>)> _runner;
	std::shared_ptr<bool> _guard; // Must be before _object.
	details::TimerObjectWrap _object;
	Fn<void()> _callback;
	base::binary_guard _running;
	crl::time _next = 0;
	int _timeout = 0;

	Qt::TimerType _type : 2;
	bool _adjusted : 1 = false;
	unsigned _repeat : 1 = 0;

};

template <typename Policy, typename Object>
ConcurrentTimer::ConcurrentTimer(
	crl::details::weak_async<Policy, Object> weak,
	Fn<void()> callback)
: ConcurrentTimer(weak.runner(), std::move(callback)) {
}

} // namespace base
