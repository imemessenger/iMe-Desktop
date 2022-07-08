// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

namespace base {

template <typename T>
struct required {
	template <
		typename U,
		typename = std::enable_if_t<std::is_constructible_v<T, U&&>>>
	constexpr required(U &&value) noexcept : _value(std::forward<U>(value)) {
	}
	template <
		typename U,
		typename = std::enable_if_t<std::is_assignable_v<T, U&&>>>
	constexpr required &operator=(U &&value) noexcept {
		_value = std::forward<U>(value);
		return *this;
	}
	constexpr required(required &&) = default;
	constexpr required(const required &) = default;
	constexpr required &operator=(required &&) = default;
	constexpr required &operator=(const required &) = default;

	[[nodiscard]] constexpr T &operator*() noexcept {
		return _value;
	}
	[[nodiscard]] constexpr const T &operator*() const noexcept {
		return _value;
	}
	[[nodiscard]] constexpr T &value() noexcept {
		return _value;
	}
	[[nodiscard]] constexpr const T &value() const noexcept {
		return _value;
	}
	[[nodiscard]] constexpr T *operator->() noexcept {
		return &_value;
	}
	[[nodiscard]] constexpr const T *operator->() const noexcept {
		return &_value;
	}
	[[nodiscard]] constexpr T *get() noexcept {
		return &_value;
	}
	[[nodiscard]] constexpr const T *get() const noexcept {
		return &_value;
	}
	[[nodiscard]] operator T&() noexcept {
		return _value;
	}
	[[nodiscard]] operator const T&() const noexcept {
		return _value;
	}

private:
	T _value;

};

} // namespace base
