// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <crl/common/crl_common_config.h>

#if defined CRL_USE_DISPATCH && !defined CRL_USE_COMMON_QUEUE

#include <crl/common/crl_common_utils.h>
#include <memory>
#include <atomic>

namespace crl {

class queue {
public:
	queue();

	template <
		typename Callable,
		typename Return = decltype(std::declval<Callable>()())>
	void async(Callable &&callable) {
		using Function = std::decay_t<Callable>;

		if constexpr (details::is_plain_function_v<Function, Return>) {
			using Plain = Return(*)();
			const auto copy = static_cast<Plain>(callable);
			async_plain([](void *passed) {
				const auto callable = reinterpret_cast<Plain>(passed);
				(*callable)();
			}, reinterpret_cast<void*>(copy));
		} else {
			const auto copy = new Function(std::forward<Callable>(callable));
			async_plain([](void *passed) {
				const auto callable = static_cast<Function*>(passed);
				const auto guard = details::finally([=] { delete callable; });
				(*callable)();
			}, static_cast<void*>(copy));
		}
	}

	template <
		typename Callable,
		typename Return = decltype(std::declval<Callable>()())>
	void sync(Callable &&callable) {
		using Function = std::decay_t<Callable>;

		if constexpr (details::is_plain_function_v<Function, Return>) {
			using Plain = Return(*)();
			const auto copy = static_cast<Plain>(callable);
			sync_plain([](void *passed) {
				const auto callable = reinterpret_cast<Plain>(passed);
				(*callable)();
			}, reinterpret_cast<void*>(copy));
		} else {
			const auto copy = new Function(std::forward<Callable>(callable));
			sync_plain([](void *passed) {
				const auto callable = static_cast<Function*>(passed);
				const auto guard = details::finally([=] { delete callable; });
				(*callable)();
			}, static_cast<void*>(copy));
		}
	}

private:
	// Hide dispatch_queue_t
	struct implementation {
		using pointer = void*;
		static pointer create();
		void operator()(pointer value);
	};

	void async_plain(void (*callable)(void*), void *argument);
	void sync_plain(void (*callable)(void*), void *argument);

	std::unique_ptr<implementation::pointer, implementation> _handle;

};

} // namespace crl

#endif // CRL_USE_DISPATCH && !CRL_USE_COMMON_QUEUE
