// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/unique_function.h"
#include <vector>

namespace rpl {
namespace details {

template <typename Type>
inline Type take(Type &value) {
	return std::exchange(value, Type{});
}

} // namespace details

class lifetime {
public:
	lifetime() = default;
	lifetime(lifetime &&other);
	lifetime &operator=(lifetime &&other);
	~lifetime() { destroy(); }

	template <typename Destroy, typename = decltype(std::declval<Destroy>()())>
	lifetime(Destroy &&destroy);

	explicit operator bool() const { return !_callbacks.empty(); }

	template <typename Destroy, typename = decltype(std::declval<Destroy>()())>
	void add(Destroy &&destroy);
	void add(lifetime &&other);
	void destroy();

	void release();

	template <typename Type, typename... Args>
	Type *make_state(Args&& ...args) {
		const auto result = new Type(std::forward<Args>(args)...);
		add([=] {
			static_assert(sizeof(Type) > 0, "Can't delete unknown type.");
			delete result;
		});
		return result;
	}

private:
	std::vector<base::unique_function<void()>> _callbacks;

};

inline lifetime::lifetime(lifetime &&other)
: _callbacks(details::take(other._callbacks)) {
}

inline lifetime &lifetime::operator=(lifetime &&other) {
	std::swap(_callbacks, other._callbacks);
	other.destroy();
	return *this;
}

template <typename Destroy, typename>
inline lifetime::lifetime(Destroy &&destroy) {
	_callbacks.emplace_back(std::forward<Destroy>(destroy));
}

template <typename Destroy, typename>
inline void lifetime::add(Destroy &&destroy) {
	_callbacks.emplace_back(std::forward<Destroy>(destroy));
}

inline void lifetime::add(lifetime &&other) {
	auto callbacks = details::take(other._callbacks);
	_callbacks.insert(
		_callbacks.end(),
		std::make_move_iterator(callbacks.begin()),
		std::make_move_iterator(callbacks.end()));
}

inline void lifetime::destroy() {
	auto callbacks = details::take(_callbacks);
	for (auto i = callbacks.rbegin(), e = callbacks.rend(); i != e; ++i) {
		(*i)();
	}
}

inline void lifetime::release() {
	_callbacks.clear();
}

} // namespace rpl
