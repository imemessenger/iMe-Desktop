// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <QtCore/QLatin1String>
#include <memory>

namespace base {

template <typename Type>
inline constexpr Type take(Type &value) noexcept {
	return std::exchange(value, Type {});
}

template <typename Type>
inline constexpr Type duplicate(const Type &value) {
	return value;
}

template <typename Type, size_t Size>
inline constexpr size_t array_size(const Type(&)[Size]) noexcept {
	return Size;
}

template <typename Container, typename T>
inline bool contains(const Container &container, const T &value) {
	const auto end = std::end(container);
	return std::find(std::begin(container), end, value) != end;
}

template <typename Container>
inline void reorder(Container &container, int oldPosition, int newPosition) {
	const auto b = begin(container);
	if (oldPosition < newPosition) {
		std::rotate(
			b + oldPosition,
			b + oldPosition + 1,
			b + newPosition + 1);
	} else if (newPosition < oldPosition) {
		std::rotate(
			b + newPosition,
			b + oldPosition,
			b + oldPosition + 1);
	}

}

template <typename D, typename T>
inline constexpr D up_cast(T object) {
	using DV = std::decay_t<decltype(*D())>;
	using TV = std::decay_t<decltype(*T())>;
	if constexpr (std::is_base_of_v<DV, TV>) {
		return object;
	} else {
		return nullptr;
	}
}

// We need a custom comparator for set<std::unique_ptr<T>>::find to work with pointers.
// thanks to http://stackoverflow.com/questions/18939882/raw-pointer-lookup-for-sets-of-unique-ptrs
template <typename T>
struct pointer_comparator {
	using is_transparent = std::true_type;

	// helper does some magic in order to reduce the number of
	// pairs of types we need to know how to compare: it turns
	// everything into a pointer, and then uses `std::less<T*>`
	// to do the comparison:
	struct helper {
		const T *ptr = nullptr;
		helper() = default;
		helper(const helper &other) = default;
		helper(const T *p) : ptr(p) {
		}
		template <typename ...Ts>
		helper(const std::shared_ptr<Ts...> &other) : ptr(other.get()) {
		}
		template <typename ...Ts>
		helper(const std::unique_ptr<Ts...> &other) : ptr(other.get()) {
		}
		bool operator<(helper other) const {
			return std::less<const T*>()(ptr, other.ptr);
		}
	};

	// without helper, we'd need 2^n different overloads, where
	// n is the number of types we want to support (so, 8 with
	// raw pointers, unique pointers, and shared pointers).  That
	// seems silly.
	// && helps enforce rvalue use only
	bool operator()(const helper &&lhs, const helper &&rhs) const {
		return lhs < rhs;
	}

};

inline QString FromUtf8Safe(const char *string, int size = -1) {
	if (!string || !size) {
		return QString();
	} else if (size < 0) {
		size = strlen(string);
	}
	const auto result = QString::fromUtf8(string, size);
	const auto back = result.toUtf8();
	return (back.size() != size || memcmp(back.constData(), string, size))
		? QString::fromLocal8Bit(string, size)
		: result;
}

inline QString FromUtf8Safe(const QByteArray &string) {
	return FromUtf8Safe(string.constData(), string.size());
}

[[nodiscard]] double SafeRound(double value);

[[nodiscard]] QString CleanAndSimplify(QString text);

} // namespace base

template <typename T>
inline void accumulate_max(T &a, const T &b) { if (a < b) a = b; }

template <typename T>
inline void accumulate_min(T &a, const T &b) { if (a > b) a = b; }

template <size_t Size>
QLatin1String qstr(const char(&string)[Size]) {
	return QLatin1String(string, int(Size) - 1);
}
