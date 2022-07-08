// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/timer.h"

namespace base {

[[nodiscard]] inline auto timer_once(crl::time delay) {
	return rpl::make_producer<>([=](const auto &consumer) {
		auto result = rpl::lifetime();
		result.make_state<base::Timer>([=] {
			consumer.put_next_copy(rpl::empty);
			consumer.put_done();
		})->callOnce(delay);
		return result;
	});
}

[[nodiscard]] inline auto timer_each(crl::time delay) {
	return rpl::make_producer<>([=](const auto &consumer) {
		auto result = rpl::lifetime();
		result.make_state<base::Timer>([=] {
			consumer.put_next_copy(rpl::empty);
		})->callEach(delay);
		return result;
	});
}

} // namespace base
