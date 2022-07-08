// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/match_method.h"
#include "base/assertion.h"

#include <rpl/details/type_list.h>
#include <variant>

namespace v {
namespace details {

template <typename Type, typename ...Types>
struct contains;

template <typename Type, typename ...Types>
inline constexpr bool contains_v = contains<Type, Types...>::value;

template <typename Type, typename Head, typename ...Tail>
struct contains<Type, Head, Tail...>
	: std::bool_constant<
	std::is_same_v<Head, Type> || contains_v<Type, Tail...>> {
};

template <typename Type>
struct contains<Type> : std::bool_constant<false> {
};

} // namespace details

using null_t = std::monostate;
inline constexpr null_t null{};

namespace type_list = rpl::details::type_list;

template <typename ...Types>
struct normalized_variant {
	using list = type_list::list<Types...>;
	using distinct = type_list::distinct_t<list>;
	using type = std::conditional_t<
		type_list::size_v<distinct> == 1,
		type_list::get_t<0, distinct>,
		type_list::extract_to_t<distinct, std::variant>>;
};

template <typename ...Types>
using normalized_variant_t
	= typename normalized_variant<Types...>::type;

template <typename TypeList, typename Variant, typename ...Methods>
struct match_helper;

template <
	typename Type,
	typename ...Types,
	typename Variant,
	typename ...Methods>
struct match_helper<type_list::list<Type, Types...>, Variant, Methods...> {
	static decltype(auto) call(Variant &value, Methods &&...methods) {
		if (const auto v = std::get_if<Type>(&value)) {
			return base::match_method(
				*v,
				std::forward<Methods>(methods)...);
		}
		return match_helper<
			type_list::list<Types...>,
			Variant,
			Methods...>::call(
				value,
				std::forward<Methods>(methods)...);
	}
};

template <
	typename Type,
	typename Variant,
	typename ...Methods>
struct match_helper<type_list::list<Type>, Variant, Methods...> {
	static decltype(auto) call(Variant &value, Methods &&...methods) {
		if (const auto v = std::get_if<Type>(&value)) {
			return base::match_method(
				*v,
				std::forward<Methods>(methods)...);
		}
		Unexpected("Valueless variant in base::match().");
	}
};

template <typename ...Types, typename ...Methods>
inline decltype(auto) match(
		std::variant<Types...> &value,
		Methods &&...methods) {
	return match_helper<
		type_list::list<Types...>,
		std::variant<Types...>,
		Methods...>::call(value, std::forward<Methods>(methods)...);
}

template <typename ...Types, typename ...Methods>
inline decltype(auto) match(
		const std::variant<Types...> &value,
		Methods &&...methods) {
	return match_helper<
		type_list::list<Types...>,
		const std::variant<Types...>,
		Methods...>::call(value, std::forward<Methods>(methods)...);
}

template <typename Type, typename ...Types>
[[nodiscard]] inline bool is(const std::variant<Types...> &value) {
	return std::holds_alternative<Type>(value);
}

template <typename ...Types>
[[nodiscard]] inline bool is_null(
		const std::variant<null_t, Types...> &value) {
	return is<null_t>(value);
}

// On macOS std::get is macOS 10.14+ because of the exception type.
// So we use our own, implemented using std::get_if.

template <typename Type, typename ...Types>
[[nodiscard]] inline Type &get(std::variant<Types...> &value) {
	const auto result = std::get_if<Type>(&value);

	Ensures(result != nullptr);
	return *result;
}

template <typename Type, typename ...Types>
[[nodiscard]] inline const Type &get(const std::variant<Types...> &value) {
	const auto result = std::get_if<Type>(&value);

	Ensures(result != nullptr);
	return *result;
}

} // namespace v

template <
	typename ...Types,
	typename Type,
	typename = std::enable_if_t<v::details::contains_v<Type, Types...>>>
inline bool operator==(const std::variant<Types...> &a, const Type &b) {
	return (a == std::variant<Types...>(b));
}

template <
	typename ...Types,
	typename Type,
	typename = std::enable_if_t<v::details::contains_v<Type, Types...>>>
inline bool operator==(const Type &a, const std::variant<Types...> &b) {
	return (std::variant<Types...>(a) == b);
}

template <
	typename ...Types,
	typename Type,
	typename = std::enable_if_t<v::details::contains_v<Type, Types...>>>
inline bool operator!=(const std::variant<Types...> &a, const Type &b) {
	return (a != std::variant<Types...>(b));
}

template <
	typename ...Types,
	typename Type,
	typename = std::enable_if_t<v::details::contains_v<Type, Types...>>>
inline bool operator!=(const Type &a, const std::variant<Types...> &b) {
	return (std::variant<Types...>(a) != b);
}
