// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <rpl/producer.h>

namespace rpl {

template <typename Value = empty_value, typename Error = no_error>
inline auto complete() {
	return make_producer<Value, Error>([](const auto &consumer) {
		consumer.put_done();
		return lifetime();
	});
}

} // namespace rpl
