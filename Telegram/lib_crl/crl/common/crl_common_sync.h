// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <crl/crl_semaphore.h>

namespace crl {

template <typename Callable>
inline void sync(Callable &&callable) {
	semaphore waiter;
	async([&] {
		const auto guard = details::finally([&] { waiter.release(); });
		callable();
	});
	waiter.acquire();
}

} // namespace crl
