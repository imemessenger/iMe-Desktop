// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <combaseapi.h>

namespace base {

template <typename Item>
struct FreePolicyNullTerminated {
	void destroy(Item *data) {
	}

	struct Sentinel {
		friend inline bool operator==(Item *data, Sentinel) {
			return !*data;
		}
		friend inline bool operator!=(Item *data, Sentinel) {
			return !!*data;
		}
	};
	[[nodiscard]] Sentinel sentinel(const Item *data) const {
		return Sentinel();
	}
};

template <typename Item>
struct FreePolicyCounted {
	void destroy(Item *data) {
		if (data) {
			for (auto i = 0; i != count; ++i) {
				data[i].~Item();
			}
		}
	}

	[[nodiscard]] Item *sentinel(Item *data) const {
		return data + count;
	}
	[[nodiscard]] const Item *sentinel(const Item *data) const {
		return data + count;
	}

	UINT32 count = 0;
};

template <typename, template <typename...> class>
struct instance_of_t : std::false_type {
};

template <template <typename...> class Template, typename ...Args>
struct instance_of_t<Template<Args...>, Template> : std::true_type {
};

template <typename Type, template <typename...> class Template>
inline constexpr bool instance_of_v = instance_of_t<Type, Template>::value;

template <typename Item, typename FreePolicy>
class CoTaskMemArray final : private FreePolicy {
public:
	CoTaskMemArray() = default;
	~CoTaskMemArray() {
		if (_data) {
			FreePolicy::destroy(_data);
			CoTaskMemFree(_data);
		}
	}

	[[nodiscard]] Item *data() const {
		return _data;
	}
	[[nodiscard]] bool valid() const {
		return (_data != nullptr);
	}
	explicit operator bool() const {
		return _data != nullptr;
	}

	[[nodiscard]] Item &operator[](int index) {
		return _data[index];
	}
	[[nodiscard]] const Item &operator[](int index) const {
		return _data[index];
	}

	[[nodiscard]] Item *begin() {
		return _data;
	}
	[[nodiscard]] auto end() {
		return FreePolicy::sentinel(_data);
	}
	[[nodiscard]] const Item *begin() const {
		return _data;
	}
	[[nodiscard]] auto end() const {
		return FreePolicy::sentinel(_data);
	}
	[[nodiscard]] const Item *cbegin() const {
		return _data;
	}
	[[nodiscard]] auto cend() const {
		return FreePolicy::sentinel(_data);
	}

	[[nodiscard]] auto put() {
		if constexpr (instance_of_v<Item, CoTaskMemArray>) {
			static_assert(sizeof(*_data) == sizeof(void*));
			return reinterpret_cast<decltype(_data->put())*>(&_data);
		} else {
			return &_data;
		}
	}
	[[nodiscard]] UINT32 *put_size() {
		return &static_cast<FreePolicy*>(this)->count;
	}
	[[nodiscard]] int size() const {
		return int(static_cast<FreePolicy*>(this)->count);
	}

private:
	Item *_data = nullptr;

};

using CoTaskMemString = CoTaskMemArray<
	WCHAR,
	FreePolicyNullTerminated<WCHAR>>;

using CoTaskMemStringArray = CoTaskMemArray<
	CoTaskMemString,
	FreePolicyCounted<CoTaskMemString>>;

} // namespace base
