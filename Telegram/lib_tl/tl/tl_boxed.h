// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/basic_types.h"

#include <QtCore/QVector>

namespace tl {

template <typename Accumulator>
struct Writer;

template <typename bare>
class boxed : public bare {
public:
	using bare::bare;

	boxed() = default;
	boxed(const boxed<bare> &v) = default;
	boxed<bare> &operator=(const boxed<bare> &v) = default;
	boxed(const bare &v) : bare(v) {
	}
	boxed<bare> &operator=(const bare &v) {
		*((bare*)this) = v;
		return *this;
	}

	template <typename Prime>
	[[nodiscard]] bool read(const Prime *&from, const Prime *end, uint32 cons = 0) {
		if (!Reader<Prime>::Has(1, from, end)) {
			return false;
		}
		cons = Reader<Prime>::Get(from, end);
		return bare::read(from, end, cons);
	}
	template <typename Accumulator>
	void write(Accumulator &to) const {
		Writer<Accumulator>::Put(to, bare::type());
		bare::write(to);
	}

	using Unboxed = bare;

};

template <typename T>
class boxed<boxed<T> > {
	using Unboxed = typename T::CantMakeBoxedBoxedType;
};

template <typename T>
struct is_boxed : std::false_type {
};

template <typename T>
struct is_boxed<boxed<T>> : std::true_type {
};

template <typename T>
inline constexpr bool is_boxed_v = is_boxed<T>::value;

} // namespace tl
