// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <crl/common/crl_common_config.h>

#include <memory>

namespace crl {

template <typename T, typename Enable>
struct guard_traits;


template <typename T>
struct guard_traits<std::weak_ptr<T>, void> {
	static std::weak_ptr<T> create(const std::weak_ptr<T> &value) {
		return value;
	}
	static std::weak_ptr<T> create(std::weak_ptr<T> &&value) {
		return std::move(value);
	}
	static bool check(const std::weak_ptr<T> &guard) {
		return guard.lock() != nullptr;
	}

};

template <typename T>
struct guard_traits<std::shared_ptr<T>, void> {
	static std::weak_ptr<T> create(const std::shared_ptr<T> &value) {
		return value;
	}
	static std::weak_ptr<T> create(std::shared_ptr<T> &&value) {
		return value;
	}
	static bool check(const std::weak_ptr<T> &guard) {
		return guard.lock() != nullptr;
	}

};

} // namespace crl
