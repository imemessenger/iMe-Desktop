// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <rpl/producer.h>

namespace rpl {

template <
	typename Creator,
	typename Value = typename decltype(std::declval<Creator>()())::value_type,
	typename Error = typename decltype(std::declval<Creator>()())::error_type>
inline auto deferred(Creator &&creator) {
	return make_producer<Value, Error>([
		creator = std::forward<Creator>(creator)
	](const auto &consumer) mutable {
		return std::move(creator)().start_existing(consumer);
	});
}

} // namespace rpl
