// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include <crl/crl_object_on_thread.h>

#include <crl/crl_fp_exceptions.h>
#include <thread>

namespace crl::details {

thread_policy::thread_policy() {
	std::thread([=] { run(); }).detach();
}

void thread_policy::run() {
	toggle_fp_exceptions(true);

	while (true) {
		if (!_list.process()) {
			break;
		}
		_queued.store(false);

		std::unique_lock<std::mutex> lock(_mutex);
		_variable.wait(lock, [=] { return !_list.empty(); });
	}
}

void thread_policy::wake_async() const {
	auto expected = false;
	if (_queued.compare_exchange_strong(expected, true)) {
		std::unique_lock<std::mutex> lock(_mutex);
		_variable.notify_one();
	}
}

} // namespace crl::details
