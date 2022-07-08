// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

namespace base {

void call_delayed(crl::time delay, FnMut<void()> &&callable);

template <
	typename Guard,
	typename Callable,
	typename GuardTraits = crl::guard_traits<std::decay_t<Guard>>,
	typename = std::enable_if_t<
	sizeof(GuardTraits) != crl::details::dependent_zero<GuardTraits>>>
inline void call_delayed(
		crl::time delay,
		Guard &&object,
		Callable &&callable) {
	return call_delayed(delay, crl::guard(
		std::forward<Guard>(object),
		std::forward<Callable>(callable)));
}

} // namespace base
