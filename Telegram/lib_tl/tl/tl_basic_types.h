// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/basic_types.h"
#include "base/flags.h"
#include "base/bytes.h"

#include <QtCore/QVector>

namespace tl {
namespace details {

struct zero_flags_helper {
};

struct LengthCounter {
	uint32 length = 0;
};

} // namespace details

template <typename Accumulator>
struct Writer;

template <typename Prime>
struct Reader;

template <>
struct Writer<details::LengthCounter> final {
	static void PutBytes(details::LengthCounter &to, const void *bytes, uint32 count) noexcept {
		constexpr auto kPrime = sizeof(uint32);
		const auto primes = (count / kPrime) + (count % kPrime ? 1 : 0);
		to.length += primes * kPrime;
	}
	static void Put(details::LengthCounter &to, uint32 value) noexcept {
		to.length += sizeof(uint32);
	}
};

template <
	typename T,
	typename = decltype(
		std::declval<T>().write(std::declval<details::LengthCounter&>()))>
uint32 count_length(const T &value) noexcept {
	auto counter = details::LengthCounter();
	value.write(counter);
	return counter.length;
}

enum {
	id_int = 0xa8509bda,
	id_long = 0x22076cba,
	id_int128 = 0x4bb5362b,
	id_int256 = 0x929c32f,
	id_double = 0x2210c154,
	id_string = 0xb5286e24,
	id_vector = 0x1cb5c415,

	id_bytes = id_string,
	id_flags = id_int,
	id_flags64 = id_long,
};

class int_type {
public:
	int32 v = 0;

	constexpr int_type() noexcept = default;

	constexpr uint32 type() const noexcept {
		return id_int;
	}
	template <typename Prime>
	[[nodiscard]] bool read(const Prime *&from, const Prime *end, uint32 cons = id_int) noexcept {
		if (!Reader<Prime>::Has(1, from, end) || cons != id_int) {
			return false;
		}
		v = static_cast<int32>(Reader<Prime>::Get(from, end));
		return true;
	}
	template <typename Accumulator>
	void write(Accumulator &to) const noexcept {
		Writer<Accumulator>::Put(to, static_cast<uint32>(v));
	}

private:
	explicit constexpr int_type(int32 val) noexcept : v(val) {
	}

	friend constexpr int_type make_int(int32 v) noexcept;
};
inline constexpr int_type make_int(int32 v) noexcept {
	return int_type(v);
}

template <typename Flags>
class flags_type {
public:
	Flags v = 0;
	static_assert(
		sizeof(Flags) == sizeof(int32) || sizeof(Flags) == sizeof(int64),
		"flags_type are allowed only wrapping int32 or int64 flag types!");

	constexpr flags_type() noexcept = default;
	constexpr flags_type(details::zero_flags_helper helper) noexcept {
	}

	constexpr uint32 type() const noexcept {
		return (sizeof(Flags) == sizeof(int32)) ? id_flags : id_flags64;
	}
	template <typename Prime>
	[[nodiscard]] bool read(const Prime *&from, const Prime *end, uint32 cons = id_flags) noexcept {
		if (!Reader<Prime>::Has(1, from, end) || cons != id_flags) {
			return false;
		}
		v = Flags::from_raw(static_cast<typename Flags::Type>(Reader<Prime>::Get(from, end)));
		if constexpr (sizeof(Flags) != sizeof(int32)) {
			if (!Reader<Prime>::Has(1, from, end)) {
				return false;
			}
			v |= Flags::from_raw(static_cast<typename Flags::Type>(Reader<Prime>::Get(from, end)) << 32);
		}
		return true;
	}
	template <typename Accumulator>
	void write(Accumulator &to) const noexcept {
		if constexpr (sizeof(Flags) == sizeof(int32)) {
			Writer<Accumulator>::Put(to, static_cast<uint32>(v.value()));
		} else {
			Writer<Accumulator>::Put(to, static_cast<uint32>(v.value() & 0xFFFFFFFFULL));
			Writer<Accumulator>::Put(to, static_cast<uint32>(v.value() >> 32));
		}
	}

private:
	explicit constexpr flags_type(Flags val) noexcept : v(val) {
	}

	template <typename T>
	friend constexpr flags_type<base::flags<T>> make_flags(base::flags<T> v) noexcept;

	template <typename T, typename>
	friend constexpr flags_type<base::flags<T>> make_flags(T v) noexcept;

};

template <typename T>
inline constexpr flags_type<base::flags<T>> make_flags(base::flags<T> v) noexcept {
	return flags_type<base::flags<T>>(v);
}

template <typename T, typename = std::enable_if_t<!std::is_same<T, int>::value>>
inline constexpr flags_type<base::flags<T>> make_flags(T v) noexcept {
	return flags_type<base::flags<T>>(v);
}

inline constexpr details::zero_flags_helper make_flags(void(details::zero_flags_helper::*)()) noexcept {
	return details::zero_flags_helper();
}

inline constexpr bool operator==(const int_type &a, const int_type &b) noexcept {
	return a.v == b.v;
}
inline constexpr bool operator!=(const int_type &a, const int_type &b) noexcept {
	return a.v != b.v;
}

class long_type {
public:
	uint64 v = 0;

	constexpr long_type() noexcept = default;

	constexpr uint32 type() const noexcept {
		return id_long;
	}
	template <typename Prime>
	[[nodiscard]] bool read(const Prime *&from, const Prime *end, uint32 cons = id_long) noexcept {
		if (!Reader<Prime>::Has(2, from, end) || cons != id_long) {
			return false;
		}
		v = static_cast<uint64>(Reader<Prime>::Get(from, end));
		v |= static_cast<uint64>(Reader<Prime>::Get(from, end)) << 32;
		return true;
	}
	template <typename Accumulator>
	void write(Accumulator &to) const noexcept {
		Writer<Accumulator>::Put(to, static_cast<uint32>(v & 0xFFFFFFFFULL));
		Writer<Accumulator>::Put(to, static_cast<uint32>(v >> 32));
	}

private:
	explicit constexpr long_type(uint64 val) noexcept : v(val) {
	}

	friend constexpr long_type make_long(uint64 v) noexcept;
};
inline constexpr long_type make_long(uint64 v) noexcept {
	return long_type(v);
}

inline constexpr bool operator==(const long_type &a, const long_type &b) noexcept {
	return a.v == b.v;
}
inline constexpr bool operator!=(const long_type &a, const long_type &b) noexcept {
	return a.v != b.v;
}

class int64_type {
public:
	int64 v = 0;

	constexpr int64_type() noexcept = default;

	constexpr uint32 type() const noexcept {
		return id_long;
	}
	template <typename Prime>
	[[nodiscard]] bool read(const Prime *&from, const Prime *end, uint32 cons = id_long) noexcept {
		if (!Reader<Prime>::Has(2, from, end) || cons != id_long) {
			return false;
		}
		auto data = static_cast<uint64>(Reader<Prime>::Get(from, end));
		data |= static_cast<uint64>(Reader<Prime>::Get(from, end)) << 32;
		v = static_cast<int64>(data);
		return true;
	}
	template <typename Accumulator>
	void write(Accumulator &to) const noexcept {
		const auto data = static_cast<uint64>(v);
		Writer<Accumulator>::Put(to, static_cast<uint32>(data & 0xFFFFFFFFULL));
		Writer<Accumulator>::Put(to, static_cast<uint32>(data >> 32));
	}

private:
	explicit constexpr int64_type(int64 val) noexcept : v(val) {
	}

	friend constexpr int64_type make_int64(int64 v) noexcept;
};
inline constexpr int64_type make_int64(int64 v) noexcept {
	return int64_type(v);
}

inline constexpr bool operator==(const int64_type &a, const int64_type &b) noexcept {
	return a.v == b.v;
}
inline constexpr bool operator!=(const int64_type &a, const int64_type &b) noexcept {
	return a.v != b.v;
}

class int128_type {
public:
	uint64 l = 0;
	uint64 h = 0;

	constexpr int128_type() noexcept = default;

	constexpr uint32 type() const noexcept {
		return id_int128;
	}
	template <typename Prime>
	[[nodiscard]] bool read(const Prime *&from, const Prime *end, uint32 cons = id_int128) noexcept {
		if (!Reader<Prime>::Has(4, from, end) || cons != id_int128) {
			return false;
		}
		l = static_cast<uint64>(Reader<Prime>::Get(from, end));
		l |= static_cast<uint64>(Reader<Prime>::Get(from, end)) << 32;
		h = static_cast<uint64>(Reader<Prime>::Get(from, end));
		h |= static_cast<uint64>(Reader<Prime>::Get(from, end)) << 32;
		return true;
	}
	template <typename Accumulator>
	void write(Accumulator &to) const noexcept {
		Writer<Accumulator>::Put(to, static_cast<uint32>(l & 0xFFFFFFFFULL));
		Writer<Accumulator>::Put(to, static_cast<uint32>(l >> 32));
		Writer<Accumulator>::Put(to, static_cast<uint32>(h & 0xFFFFFFFFULL));
		Writer<Accumulator>::Put(to, static_cast<uint32>(h >> 32));
	}

private:
	explicit constexpr int128_type(uint64 low, uint64 high) noexcept : l(low), h(high) {
	}

	friend constexpr int128_type make_int128(uint64 l, uint64 h) noexcept;
};
inline constexpr int128_type make_int128(uint64 l, uint64 h) noexcept {
	return int128_type(l, h);
}

inline constexpr bool operator==(const int128_type &a, const int128_type &b) noexcept {
	return a.l == b.l && a.h == b.h;
}
inline constexpr bool operator!=(const int128_type &a, const int128_type &b) noexcept {
	return a.l != b.l || a.h != b.h;
}

class int256_type {
public:
	int128_type l;
	int128_type h;

	constexpr int256_type() noexcept = default;

	constexpr uint32 type() const noexcept {
		return id_int256;
	}
	template <typename Prime>
	[[nodiscard]] bool read(const Prime *&from, const Prime *end, uint32 cons = id_int256) noexcept {
		if (cons != id_int256) {
			return false;
		}
		return l.read(from, end) && h.read(from, end);
	}
	template <typename Accumulator>
	void write(Accumulator &to) const noexcept {
		l.write(to);
		h.write(to);
	}

private:
	explicit constexpr int256_type(int128_type low, int128_type high) noexcept : l(low), h(high) {
	}

	friend constexpr int256_type make_int256(const int128_type &l, const int128_type &h) noexcept;
};
inline constexpr int256_type make_int256(const int128_type &l, const int128_type &h) noexcept {
	return int256_type(l, h);
}

inline constexpr bool operator==(const int256_type &a, const int256_type &b) noexcept {
	return a.l == b.l && a.h == b.h;
}
inline constexpr bool operator!=(const int256_type &a, const int256_type &b) noexcept {
	return a.l != b.l || a.h != b.h;
}

class double_type {
public:
	float64 v = 0.;

	constexpr double_type() noexcept = default;

	constexpr uint32 type() const noexcept {
		return id_double;
	}
	template <typename Prime>
	[[nodiscard]] bool read(const Prime *&from, const Prime *end, uint32 cons = id_double) noexcept {
		if (!Reader<Prime>::Has(2, from, end) || cons != id_double) {
			return false;
		}
		auto nonaliased = static_cast<uint64>(Reader<Prime>::Get(from, end));
		nonaliased |= static_cast<uint64>(Reader<Prime>::Get(from, end)) << 32;
		static_assert(sizeof(v) == sizeof(nonaliased));
		std::memcpy(&v, &nonaliased, sizeof(v));
		return true;
	}
	template <typename Accumulator>
	void write(Accumulator &to) const noexcept {
		auto nonaliased = uint64();
		static_assert(sizeof(v) == sizeof(nonaliased));
		std::memcpy(&nonaliased, &v, sizeof(v));
		Writer<Accumulator>::Put(to, static_cast<uint32>(nonaliased & 0xFFFFFFFFULL));
		Writer<Accumulator>::Put(to, static_cast<uint32>(nonaliased >> 32));
	}

private:
	explicit constexpr double_type(float64 val) noexcept : v(val) {
	}

	friend constexpr double_type make_double(float64 v) noexcept;
};
inline constexpr double_type make_double(float64 v) noexcept {
	return double_type(v);
}

inline constexpr bool operator==(const double_type &a, const double_type &b) noexcept {
	return a.v == b.v;
}
inline constexpr bool operator!=(const double_type &a, const double_type &b) noexcept {
	return a.v != b.v;
}

class string_type;
using bytes_type = string_type;

class string_type {
public:
	string_type() noexcept = default;

	uint32 type() const noexcept {
		return id_string;
	}
	template <typename Prime>
	[[nodiscard]] bool read(const Prime *&from, const Prime *end, uint32 cons = id_string) noexcept {
		if (!Reader<Prime>::Has(1, from, end) || cons != id_string) {
			return false;
		}
		const auto first = Reader<Prime>::Get(from, end);
		const auto last = (first & 0xFFU);
		if (last > 254) {
			return false;
		} else if (last == 0) {
			v = QByteArray();
		} else if (last == 1) {
			v = QByteArray(1, static_cast<char>((first >> 8) & 0xFFU));
		} else if (last == 2) {
			v = QByteArray(2, Qt::Uninitialized);
			v[0] = static_cast<char>((first >> 8) & 0xFFU);
			v[1] = static_cast<char>((first >> 16) & 0xFFU);
		} else if (last < 254) {
			v = QByteArray(last, Qt::Uninitialized);
			const auto remaining = last - 3;
			if (!Reader<Prime>::HasBytes(remaining, from, end)) {
				return false;
			}
			v[0] = static_cast<char>((first >> 8) & 0xFFU);
			v[1] = static_cast<char>((first >> 16) & 0xFFU);
			v[2] = static_cast<char>((first >> 24) & 0xFFU);
			Reader<Prime>::GetBytes(v.data() + 3, remaining, from, end);
		} else {
			const auto length = (first >> 8);
			if (!Reader<Prime>::HasBytes(length, from, end)) {
				return false;
			}
			v = QByteArray(length, Qt::Uninitialized);
			Reader<Prime>::GetBytes(v.data(), length, from, end);
		}
		return true;
	}
	template <typename Accumulator>
	void write(Accumulator &to) const noexcept {
		Expects(v.size() < 0x1000000);

		const auto size = uint32(v.size());
		if (size == 0) {
			Writer<Accumulator>::Put(to, size);
		} else if (size == 1) {
			Writer<Accumulator>::Put(to, size
				| (static_cast<uint32>(static_cast<uchar>(v[0])) << 8));
		} else if (size == 2) {
			Writer<Accumulator>::Put(to, size
				| (static_cast<uint32>(static_cast<uchar>(v[0])) << 8)
				| (static_cast<uint32>(static_cast<uchar>(v[1])) << 16));
		} else if (size < 254) {
			Writer<Accumulator>::Put(to, size
				| (static_cast<uint32>(static_cast<uchar>(v[0])) << 8)
				| (static_cast<uint32>(static_cast<uchar>(v[1])) << 16)
				| (static_cast<uint32>(static_cast<uchar>(v[2])) << 24));
			Writer<Accumulator>::PutBytes(to, v.data() + 3, size - 3);
		} else {
			const auto encoded = (size << 8) | 254U;
			Writer<Accumulator>::Put(to, encoded);
			Writer<Accumulator>::PutBytes(to, v.data(), size);
		}
	}

	QByteArray v;

private:
	explicit string_type(QByteArray &&data) noexcept : v(std::move(data)) {
	}

	friend string_type make_string(const std::string &v);
	friend string_type make_string(const QByteArray &v);
	friend string_type make_string(QByteArray &&v);
	friend string_type make_string(const QString &v);
	friend string_type make_string(const char *v);
	friend string_type make_string();

	friend bytes_type make_bytes(const QByteArray &v);
	friend bytes_type make_bytes(QByteArray &&v);
	friend bytes_type make_bytes();

};

inline string_type make_string(const std::string &v) {
	return string_type(QByteArray(v.data(), v.size()));
}
inline string_type make_string(const QByteArray &v) {
	return string_type(QByteArray(v));
}
inline string_type make_string(QByteArray &&v) {
	return string_type(std::move(v));
}
inline string_type make_string(const QString &v) {
	return string_type(v.toUtf8());
}
inline string_type make_string(const char *v) {
	return string_type(QByteArray(v, strlen(v)));
}
inline string_type make_string() {
	return string_type(QByteArray());
}
inline bytes_type make_bytes(const QByteArray &v) {
	return bytes_type(QByteArray(v));
}
inline bytes_type make_bytes(QByteArray &&v) {
	return bytes_type(std::move(v));
}
inline bytes_type make_bytes() {
	return bytes_type(QByteArray());
}
inline bytes_type make_bytes(bytes::const_span buffer) {
	return make_bytes(QByteArray(
		reinterpret_cast<const char*>(buffer.data()),
		buffer.size()));
}
inline bytes_type make_bytes(const bytes::vector &buffer) {
	return make_bytes(bytes::make_span(buffer));
}

inline bool operator==(const string_type &a, const string_type &b) {
	return a.v == b.v;
}
inline bool operator!=(const string_type &a, const string_type &b) {
	return a.v != b.v;
}

QString utf16(const QByteArray &v);

inline QByteArray utf8(const QByteArray &v) {
	return v;
}

inline QString utf16(const string_type &v) {
	return utf16(v.v);
}

inline QByteArray utf8(const string_type &v) {
	return utf8(v.v);
}

template <typename T>
class vector_type {
public:
	vector_type() noexcept = default;

	uint32 type() const noexcept {
		return id_vector;
	}
	template <typename Prime>
	[[nodiscard]] bool read(const Prime *&from, const Prime *end, uint32 cons = id_vector) noexcept {
		if (!Reader<Prime>::Has(1, from, end) || cons != id_vector) {
			return false;
		}
		auto count = Reader<Prime>::Get(from, end);

		auto vector = QVector<T>(count, T());
		for (auto &item : vector) {
			if (!item.read(from, end)) {
				return false;
			}
		}
		v = std::move(vector);
		return true;
	}
	template <typename Accumulator>
	void write(Accumulator &to) const noexcept {
		Writer<Accumulator>::Put(to, static_cast<int32>(v.size()));
		for (const auto &item : v) {
			item.write(to);
		}
	}

	QVector<T> v;

private:
	explicit vector_type(QVector<T> &&data) noexcept : v(std::move(data)) {
	}

	template <typename U>
	friend vector_type<U> make_vector(uint32 count);
	template <typename U>
	friend vector_type<U> make_vector(uint32 count, const U &value);
	template <typename U>
	friend vector_type<U> make_vector(const QVector<U> &v);
	template <typename U>
	friend vector_type<U> make_vector(QVector<U> &&v);
	template <typename U>
	friend vector_type<U> make_vector();

};
template <typename T>
inline vector_type<T> make_vector(uint32 count) {
	return vector_type<T>(QVector<T>(count));
}
template <typename T>
inline vector_type<T> make_vector(uint32 count, const T &value) {
	return vector_type<T>(QVector<T>(count, value));
}
template <typename T>
inline vector_type<T> make_vector(const QVector<T> &v) {
	return vector_type<T>(QVector<T>(v));
}
template <typename T>
inline vector_type<T> make_vector(QVector<T> &&v) {
	return vector_type<T>(std::move(v));
}
template <typename T>
inline vector_type<T> make_vector() {
	return vector_type<T>();
}

template <typename T>
inline bool operator==(const vector_type<T> &a, const vector_type<T> &b) {
	return a.c_vector().v == b.c_vector().v;
}
template <typename T>
inline bool operator!=(const vector_type<T> &a, const vector_type<T> &b) {
	return a.c_vector().v != b.c_vector().v;
}

namespace details {

template <typename Type>
struct repeat_helper {
	using type = Type;
};
template <typename Type>
using repeat = typename repeat_helper<Type>::type;

struct inner_helper {
	static void Check(...);
	template <typename Type, typename Result = decltype(std::declval<Type>().v)>
	static Result Check(const Type&);

	template <typename Type>
	using type = std::decay_t<decltype(Check(std::declval<Type>()))>;
};

} // namespace details

template <typename T>
class conditional {
public:
	constexpr conditional() noexcept = default;
	constexpr conditional(const T *value) noexcept : _value(value) {
	}

	constexpr operator const T*() const noexcept {
		return _value;
	}
	constexpr const T *operator->() const noexcept {
		Expects(_value != nullptr);

		return _value;
	}
	constexpr const T &operator*() const noexcept {
		Expects(_value != nullptr);

		return *_value;
	}

	template <
		typename Inner = details::inner_helper::type<T>,
		typename = std::enable_if_t<!std::is_same_v<Inner, void>>>
	constexpr Inner value_or(details::repeat<Inner> fallback) const noexcept {
		return _value ? _value->v : fallback;
	}

	template <
		typename Inner = details::inner_helper::type<T>,
		typename = std::enable_if_t<!std::is_same_v<Inner, void>>>
	constexpr Inner value_or_empty() const noexcept {
		return _value ? _value->v : Inner();
	}

private:
	const T *_value = nullptr;

};

} // namespace tl
