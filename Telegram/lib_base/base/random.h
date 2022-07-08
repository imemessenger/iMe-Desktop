// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <cstddef>
#include <type_traits>

#include "base/bytes.h"

namespace base {

void RandomFill(bytes::span bytes);

inline void RandomFill(void *data, std::size_t length) {
	RandomFill({ static_cast<bytes::type*>(data), length });
}

template <
	typename T,
	typename = std::enable_if_t<std::is_trivially_copyable_v<T>>>
[[nodiscard]] inline T RandomValue() {
	auto result = T();
	RandomFill({ reinterpret_cast<std::byte*>(&result), sizeof(T) });
	return result;
}

[[nodiscard]] int RandomIndex(int count);

void RandomAddSeed(bytes::const_span bytes);

} // namespace base
