// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <crl/common/crl_common_config.h>

#ifdef CRL_USE_QT

#include <crl/common/crl_common_utils.h>
#include <crl/common/crl_common_sync.h>
#include <type_traits>

#include <QtCore/QThreadPool>

namespace crl::details {

template <typename Callable>
class Runnable : public QRunnable {
public:
	Runnable(Callable &&callable) : _callable(std::move(callable)) {
	}

	void run() override {
		_callable();
	}

private:
	Callable _callable;

};

template <typename Callable>
inline auto create_runnable(Callable &&callable) {
	if constexpr (std::is_reference_v<Callable>) {
		auto copy = callable;
		return create_runnable(std::move(copy));
	} else {
		return new Runnable<Callable>(std::move(callable));
	}
}

template <typename Callable>
inline void async_any(Callable &&callable) {
	if (const auto pool = QThreadPool::globalInstance()) {
		pool->start(create_runnable(std::forward<Callable>(callable)));
	}
}

inline void async_plain(void (*callable)(void*), void *argument) {
	async_any([=] {
		callable(argument);
	});
}

} // namespace crl::details

namespace crl {

template <
	typename Callable,
	typename Return = decltype(std::declval<Callable>()())>
inline void async(Callable &&callable) {
	details::async_any(std::forward<Callable>(callable));
}

} // namespace crl

#endif // CRL_USE_QT
