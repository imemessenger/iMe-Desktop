// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <rpl/producer.h>
#include <rpl/filter.h>

namespace rpl {

template <typename SideEffect>
inline auto before_next(SideEffect &&method) {
	return filter([method = std::forward<SideEffect>(method)](
			const auto &value) {
		details::callable_invoke(method, value);
		return true;
	});
}

} // namespace rpl
