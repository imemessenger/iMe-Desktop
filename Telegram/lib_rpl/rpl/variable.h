// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <rpl/producer.h>
#include <rpl/event_stream.h>

namespace rpl {
namespace details {

template <typename A, typename B>
struct supports_equality_compare {
	template <typename U, typename V>
	static auto test(const U *u, const V *v)
		-> decltype(*u == *v, true_t());
	static false_t test(...);
	static constexpr bool value
		= (sizeof(test((const A*)nullptr, (const B*)nullptr))
			== sizeof(true_t));
};

template <typename A, typename B>
constexpr bool supports_equality_compare_v
	= supports_equality_compare<std::decay_t<A>, std::decay_t<B>>::value;

} // namespace details

template <typename Type, typename Error = no_error>
class variable final {
public:
	variable() : _data{} {
	}
	variable(variable &&other) : _data(std::move(other._data)) {
	}
	variable &operator=(variable &&other) {
		return (*this = std::move(other._data));
	}
	variable(const variable &other) : _data(other._data) {
	}
	variable &operator=(const variable &other) {
		return (*this = other._data);
	}

	template <
		typename OtherType,
		typename = std::enable_if_t<
			std::is_constructible_v<Type, OtherType&&>>>
	variable(OtherType &&data) : _data(std::forward<OtherType>(data)) {
	}

	template <
		typename OtherType,
		typename = std::enable_if_t<
			std::is_assignable_v<Type&, OtherType&&>>>
	variable &operator=(OtherType &&data) {
		_lifetime.destroy();
		return assign(std::forward<OtherType>(data));
	}

	template <
		typename OtherType,
		typename = std::enable_if_t<
		std::is_assignable_v<Type&, OtherType&&>>>
	void force_assign(OtherType &&data) {
		_lifetime.destroy();
		_data = std::forward<OtherType>(data);
		_changes.fire_copy(_data);
	}

	template <
		typename OtherType,
		typename OtherError,
		typename Generator,
		typename = std::enable_if_t<
			std::is_assignable_v<Type&, OtherType>>>
	variable(producer<OtherType, OtherError, Generator> &&stream) {
		std::move(stream)
			| start_with_next([=](auto &&data) {
				assign(std::forward<decltype(data)>(data));
			}, _lifetime);
	}

	template <
		typename OtherType,
		typename OtherError,
		typename Generator,
		typename = std::enable_if_t<
			std::is_assignable_v<Type&, OtherType>>>
	variable &operator=(
			producer<OtherType, OtherError, Generator> &&stream) {
		_lifetime.destroy();
		std::move(stream)
			| start_with_next([=](auto &&data) {
				assign(std::forward<decltype(data)>(data));
			}, _lifetime);
		return *this;
	}

	std::conditional_t<
			(std::is_trivially_copyable_v<Type> && sizeof(Type) <= 16),
			Type,
			const Type &> current() const {
		return _data;
	}
	auto value() const {
		return _changes.events_starting_with_copy(_data);
	}
	auto changes() const {
		return _changes.events();
	}

	// Send 'done' to all subscribers and unsubscribe them.
	template <
		typename OtherType,
		typename = std::enable_if_t<
			std::is_assignable_v<Type&, OtherType>>>
	void reset(OtherType &&data) {
		_data = std::forward<OtherType>(data);
		_changes = event_stream<Type, Error>();
	}
	void reset() {
		reset(Type());
	}

	template <
		typename OtherError,
		typename = std::enable_if_t<
			std::is_constructible_v<Error, OtherError&&>>>
	void reset_with_error(OtherError &&error) {
		_changes.fire_error(std::forward<OtherError>(error));
	}
	void reset_with_error() {
		reset_with_error(Error());
	}

private:
	template <typename OtherType>
	variable &assign(OtherType &&data) {
		if constexpr (details::supports_equality_compare_v<Type, OtherType>) {
			if (!(_data == data)) {
				_data = std::forward<OtherType>(data);
				_changes.fire_copy(_data);
			}
		} else if constexpr (details::supports_equality_compare_v<Type, Type>) {
			auto old = std::move(_data);
			_data = std::forward<OtherType>(data);
			if (!(_data == old)) {
				_changes.fire_copy(_data);
			}
		} else {
			_data = std::forward<OtherType>(data);
			_changes.fire_copy(_data);
		}
		return *this;
	}

	Type _data{};
	event_stream<Type, Error> _changes;
	lifetime _lifetime;

};

} // namespace rpl
