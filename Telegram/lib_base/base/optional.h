// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <optional>

namespace base {

template <typename Type>
struct optional_wrap_once {
	using type = std::optional<Type>;
};

template <typename Type>
struct optional_wrap_once<std::optional<Type>> {
	using type = std::optional<Type>;
};

template <typename Type>
using optional_wrap_once_t = typename optional_wrap_once<std::decay_t<Type>>::type;

template <typename Type>
struct optional_chain_result {
	using type = optional_wrap_once_t<Type>;
};

template <>
struct optional_chain_result<void> {
	using type = bool;
};

template <typename Type>
using optional_chain_result_t = typename optional_chain_result<Type>::type;

template <typename Type>
optional_wrap_once_t<Type> make_optional(Type &&value) {
	return optional_wrap_once_t<Type> { std::forward<Type>(value) };
}

} // namespace base

template <typename Type, typename Method>
inline auto operator|(const std::optional<Type> &value, Method method)
-> base::optional_chain_result_t<decltype(method(*value))> {
	if constexpr (std::is_same_v<decltype(method(*value)), void>) {
		return value ? (method(*value), true) : false;
	} else {
		return value
			? base::optional_chain_result_t<decltype(method(*value))>(
				method(*value))
			: std::nullopt;
	}
}
