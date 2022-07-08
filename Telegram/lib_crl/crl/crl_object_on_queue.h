// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <crl/common/crl_common_object_async.h>
#include <crl/crl_queue.h>

namespace crl {
namespace details {

class queue_policy {
protected:
	template <typename Callable>
	void async_plain(Callable &&callable) const;

private:
	mutable crl::queue _queue;

};

template <typename Callable>
void queue_policy::async_plain(Callable &&callable) const {
	_queue.async(std::forward<Callable>(callable));
}

} // namespace details

template <typename Type>
using weak_on_queue = details::weak_async<details::queue_policy, Type>;

template <typename Type>
using object_on_queue = details::object_async<details::queue_policy, Type>;

} // namespace
