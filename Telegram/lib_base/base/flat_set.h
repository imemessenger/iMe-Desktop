// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <vector>
#include <algorithm>

namespace base {

using std::begin;
using std::end;

template <typename Type, typename Compare = std::less<>>
class flat_set;

template <typename Type, typename Compare = std::less<>>
class flat_multi_set;

template <typename Type, typename iterator_impl>
class flat_multi_set_iterator_impl;

template <typename Type, typename iterator_impl>
class flat_multi_set_iterator_impl {
public:
	using iterator_category = typename iterator_impl::iterator_category;

	using value_type = Type;
	using difference_type = typename iterator_impl::difference_type;
	using pointer = const Type*;
	using reference = const Type&;

	constexpr flat_multi_set_iterator_impl(
		iterator_impl impl = iterator_impl()) noexcept
	: _impl(impl) {
	}
	template <typename other_iterator_impl>
	constexpr flat_multi_set_iterator_impl(
		const flat_multi_set_iterator_impl<
			Type,
			other_iterator_impl> &other) noexcept
	: _impl(other._impl) {
	}

	constexpr reference operator*() const noexcept {
		return *_impl;
	}
	constexpr pointer operator->() const noexcept {
		return std::addressof(**this);
	}
	constexpr flat_multi_set_iterator_impl &operator++() noexcept {
		++_impl;
		return *this;
	}
	constexpr flat_multi_set_iterator_impl operator++(int) noexcept {
		return _impl++;
	}
	constexpr flat_multi_set_iterator_impl &operator--() noexcept {
		--_impl;
		return *this;
	}
	constexpr flat_multi_set_iterator_impl operator--(int) noexcept {
		return _impl--;
	}
	constexpr flat_multi_set_iterator_impl &operator+=(
			difference_type offset) noexcept {
		_impl += offset;
		return *this;
	}
	constexpr flat_multi_set_iterator_impl operator+(
			difference_type offset) const noexcept {
		return _impl + offset;
	}
	constexpr flat_multi_set_iterator_impl &operator-=(
			difference_type offset) noexcept {
		_impl -= offset;
		return *this;
	}
	constexpr flat_multi_set_iterator_impl operator-(
			difference_type offset) const noexcept {
		return _impl - offset;
	}
	template <typename other_iterator_impl>
	constexpr difference_type operator-(
		const flat_multi_set_iterator_impl<
			Type,
			other_iterator_impl> &right) const noexcept {
		return _impl - right._impl;
	}
	constexpr reference operator[](difference_type offset) const noexcept {
		return _impl[offset];
	}

	template <typename other_iterator_impl>
	constexpr bool operator==(
		const flat_multi_set_iterator_impl<
			Type,
			other_iterator_impl> &right) const noexcept {
		return _impl == right._impl;
	}
	template <typename other_iterator_impl>
	constexpr bool operator!=(
		const flat_multi_set_iterator_impl<
			Type,
			other_iterator_impl> &right) const noexcept {
		return _impl != right._impl;
	}
	template <typename other_iterator_impl>
	constexpr bool operator<(
		const flat_multi_set_iterator_impl<
			Type,
			other_iterator_impl> &right) const noexcept {
		return _impl < right._impl;
	}

private:
	iterator_impl _impl;

	template <typename OtherType, typename OtherCompare>
	friend class flat_multi_set;

	template <typename OtherType, typename OtherCompare>
	friend class flat_set;

	template <
		typename OtherType,
		typename other_iterator_impl>
	friend class flat_multi_set_iterator_impl;

	constexpr Type &wrapped() noexcept {
		return _impl->wrapped();
	}

};

template <typename Type>
class flat_multi_set_const_wrap {
public:
	constexpr flat_multi_set_const_wrap(const Type &value) noexcept
	: _value(value) {
	}
	constexpr flat_multi_set_const_wrap(Type &&value) noexcept
	: _value(std::move(value)) {
	}
	constexpr operator const Type&() const noexcept {
		return _value;
	}
	constexpr Type &wrapped() noexcept {
		return _value;
	}

	friend inline constexpr bool operator<(
			const flat_multi_set_const_wrap &a,
			const flat_multi_set_const_wrap &b) noexcept {
		return a._value < b._value;
	}
	friend inline constexpr bool operator>(
			const flat_multi_set_const_wrap &a,
			const flat_multi_set_const_wrap &b) noexcept {
		return a._value > b._value;
	}
	friend inline constexpr bool operator<=(
			const flat_multi_set_const_wrap &a,
			const flat_multi_set_const_wrap &b) noexcept {
		return a._value <= b._value;
	}
	friend inline constexpr bool operator>=(
			const flat_multi_set_const_wrap &a,
			const flat_multi_set_const_wrap &b) noexcept {
		return a._value >= b._value;
	}
	friend inline constexpr bool operator==(
			const flat_multi_set_const_wrap &a,
			const flat_multi_set_const_wrap &b) noexcept {
		return a._value == b._value;
	}
	friend inline constexpr bool operator!=(
			const flat_multi_set_const_wrap &a,
			const flat_multi_set_const_wrap &b) noexcept {
		return a._value != b._value;
	}

private:
	Type _value;

};

template <typename Type, typename Compare>
class flat_multi_set {
	using const_wrap = flat_multi_set_const_wrap<Type>;
	using impl_t = std::vector<const_wrap>;

public:
	using value_type = Type;
	using size_type = typename impl_t::size_type;
	using difference_type = typename impl_t::difference_type;
	using pointer = const Type*;
	using reference = const Type&;

	using iterator = flat_multi_set_iterator_impl<
		Type,
		typename impl_t::iterator>;
	using const_iterator = flat_multi_set_iterator_impl<
		Type,
		typename impl_t::const_iterator>;
	using reverse_iterator = flat_multi_set_iterator_impl<
		Type,
		typename impl_t::reverse_iterator>;
	using const_reverse_iterator = flat_multi_set_iterator_impl<
		Type,
		typename impl_t::const_reverse_iterator>;

	constexpr flat_multi_set() = default;

	template <
		typename Iterator,
		typename = typename std::iterator_traits<Iterator>::iterator_category>
	constexpr flat_multi_set(Iterator first, Iterator last) noexcept
	: _data(first, last) {
		std::sort(std::begin(impl()), std::end(impl()), compare());
	}

	constexpr flat_multi_set(std::initializer_list<Type> iter) noexcept
	: flat_multi_set(iter.begin(), iter.end()) {
	}

	constexpr size_type size() const noexcept {
		return impl().size();
	}
	constexpr bool empty() const noexcept {
		return impl().empty();
	}
	constexpr void clear() noexcept {
		impl().clear();
	}
	constexpr void reserve(size_type size) noexcept {
		impl().reserve(size);
	}

	constexpr iterator begin() noexcept {
		return impl().begin();
	}
	constexpr iterator end() noexcept {
		return impl().end();
	}
	constexpr const_iterator begin() const noexcept {
		return impl().begin();
	}
	constexpr const_iterator end() const noexcept {
		return impl().end();
	}
	constexpr const_iterator cbegin() const noexcept {
		return impl().cbegin();
	}
	constexpr const_iterator cend() const noexcept {
		return impl().cend();
	}
	constexpr reverse_iterator rbegin() noexcept {
		return impl().rbegin();
	}
	constexpr reverse_iterator rend() noexcept {
		return impl().rend();
	}
	constexpr const_reverse_iterator rbegin() const noexcept {
		return impl().rbegin();
	}
	constexpr const_reverse_iterator rend() const noexcept {
		return impl().rend();
	}
	constexpr const_reverse_iterator crbegin() const noexcept {
		return impl().crbegin();
	}
	constexpr const_reverse_iterator crend() const noexcept {
		return impl().crend();
	}

	constexpr reference front() const noexcept {
		return *begin();
	}
	constexpr reference back() const noexcept {
		return *(end() - 1);
	}

	constexpr iterator insert(const Type &value) noexcept {
		if (empty() || !compare()(value, back())) {
			impl().push_back(value);
			return (end() - 1);
		}
		auto where = getUpperBound(value);
		return impl().insert(where, value);
	}
	constexpr iterator insert(Type &&value) noexcept {
		if (empty() || !compare()(value, back())) {
			impl().push_back(std::move(value));
			return (end() - 1);
		}
		auto where = getUpperBound(value);
		return impl().insert(where, std::move(value));
	}
	template <typename... Args>
	constexpr iterator emplace(Args&&... args) noexcept {
		return insert(Type(std::forward<Args>(args)...));
	}

	template <typename OtherType>
	constexpr bool removeOne(const OtherType &value) noexcept {
		if (empty()
			|| compare()(value, front())
			|| compare()(back(), value)) {
			return false;
		}
		auto where = getLowerBound(value);
		if (compare()(value, *where)) {
			return false;
		}
		impl().erase(where);
		return true;
	}
	constexpr bool removeOne(const Type &value) noexcept {
		return removeOne<Type>(value);
	}
	template <typename OtherType>
	constexpr int removeAll(const OtherType &value) noexcept {
		if (empty()
			|| compare()(value, front())
			|| compare()(back(), value)) {
			return 0;
		}
		auto range = getEqualRange(value);
		if (range.first == range.second) {
			return 0;
		}
		const auto result = (range.second - range.first);
		impl().erase(range.first, range.second);
		return result;
	}
	constexpr int removeAll(const Type &value) noexcept {
		return removeAll<Type>(value);
	}

	constexpr iterator erase(const_iterator where) noexcept {
		return impl().erase(where._impl);
	}
	constexpr iterator erase(
			const_iterator from,
			const_iterator till) noexcept {
		return impl().erase(from._impl, till._impl);
	}
	constexpr int erase(const Type &value) noexcept {
		return removeAll(value);
	}

	template <typename OtherType>
	constexpr iterator findFirst(const OtherType &value) noexcept {
		if (empty()
			|| compare()(value, front())
			|| compare()(back(), value)) {
			return end();
		}
		auto where = getLowerBound(value);
		return compare()(value, *where) ? impl().end() : where;
	}
	constexpr iterator findFirst(const Type &value) noexcept {
		return findFirst<Type>(value);
	}

	template <typename OtherType>
	constexpr const_iterator findFirst(
			const OtherType &value) const noexcept {
		if (empty()
			|| compare()(value, front())
			|| compare()(back(), value)) {
			return end();
		}
		auto where = getLowerBound(value);
		return compare()(value, *where) ? impl().end() : where;
	}
	constexpr const_iterator findFirst(
			const Type &value) const noexcept {
		return findFirst<Type>(value);
	}

	template <typename OtherType>
	constexpr bool contains(const OtherType &value) const noexcept {
		return findFirst(value) != end();
	}
	constexpr bool contains(const Type &value) const noexcept {
		return contains<Type>(value);
	}
	template <typename OtherType>
	constexpr int count(const OtherType &value) const noexcept {
		if (empty()
			|| compare()(value, front())
			|| compare()(back(), value)) {
			return 0;
		}
		auto range = getEqualRange(value);
		return (range.second - range.first);
	}
	constexpr int count(const Type &value) const noexcept {
		return count<Type>(value);
	}

	template <typename Action>
	constexpr auto modify(iterator which, Action action) noexcept {
		auto result = action(which.wrapped());
		for (auto i = which + 1, e = end(); i != e; ++i) {
			if (compare()(*i, *which)) {
				std::swap(i.wrapped(), which.wrapped());
			} else {
				break;
			}
		}
		for (auto i = which, b = begin(); i != b;) {
			--i;
			if (compare()(*which, *i)) {
				std::swap(i.wrapped(), which.wrapped());
			} else {
				break;
			}
		}
		return result;
	}

	template <
		typename Iterator,
		typename = typename std::iterator_traits<Iterator>::iterator_category>
	constexpr void merge(Iterator first, Iterator last) noexcept {
		impl().insert(impl().end(), first, last);
		std::sort(std::begin(impl()), std::end(impl()), compare());
	}

	constexpr void merge(
			const flat_multi_set<Type, Compare> &other) noexcept {
		merge(other.begin(), other.end());
	}

	constexpr void merge(std::initializer_list<Type> list) noexcept {
		merge(list.begin(), list.end());
	}

	friend inline constexpr bool operator<(
			const flat_multi_set &a,
			const flat_multi_set &b) noexcept {
		return a.impl() < b.impl();
	}
	friend inline constexpr bool operator>(
			const flat_multi_set &a,
			const flat_multi_set &b) noexcept {
		return a.impl() > b.impl();
	}
	friend inline constexpr bool operator<=(
			const flat_multi_set &a,
			const flat_multi_set &b) noexcept {
		return a.impl() <= b.impl();
	}
	friend inline constexpr bool operator>=(
			const flat_multi_set &a,
			const flat_multi_set &b) noexcept {
		return a.impl() >= b.impl();
	}
	friend inline constexpr bool operator==(
			const flat_multi_set &a,
			const flat_multi_set &b) noexcept {
		return a.impl() == b.impl();
	}
	friend inline constexpr bool operator!=(
			const flat_multi_set &a,
			const flat_multi_set &b) noexcept {
		return a.impl() != b.impl();
	}

private:
	friend class flat_set<Type, Compare>;

	struct transparent_compare : Compare {
		constexpr const Compare &initial() const noexcept {
			return *this;
		}

		template <
			typename OtherType1,
			typename OtherType2,
			typename = std::enable_if_t<
			!std::is_same_v<std::decay_t<OtherType1>, const_wrap> &&
			!std::is_same_v<std::decay_t<OtherType2>, const_wrap>>>
		constexpr auto operator()(
				OtherType1 &&a,
				OtherType2 &&b) const noexcept {
			return initial()(
				std::forward<OtherType1>(a),
				std::forward<OtherType2>(b));
		}
		template <
			typename OtherType1,
			typename OtherType2>
		constexpr auto operator()(
				OtherType1 &&a,
				OtherType2 &&b) const noexcept -> std::enable_if_t<
		std::is_same_v<std::decay_t<OtherType1>, const_wrap> &&
		std::is_same_v<std::decay_t<OtherType2>, const_wrap>, bool> {
			return initial()(
				static_cast<const Type&>(a),
				static_cast<const Type&>(b));
		}
		template <
			typename OtherType,
			typename = std::enable_if_t<
			!std::is_same_v<std::decay_t<OtherType>, const_wrap>>>
		constexpr auto operator()(
				const const_wrap &a,
				OtherType &&b) const noexcept {
			return initial()(
				static_cast<const Type&>(a),
				std::forward<OtherType>(b));
		}
		template <
			typename OtherType,
			typename = std::enable_if_t<
			!std::is_same_v<std::decay_t<OtherType>, const_wrap>>>
		constexpr auto operator()(
				OtherType &&a,
				const const_wrap &b) const noexcept {
			return initial()(
				std::forward<OtherType>(a),
				static_cast<const Type&>(b));
		}

	};
	struct Data : transparent_compare {
		template <typename ...Args>
		constexpr Data(Args &&...args) noexcept
		: elements(std::forward<Args>(args)...) {
		}

		impl_t elements;
	};

	Data _data;

	constexpr const transparent_compare &compare() const noexcept {
		return _data;
	}
	constexpr const impl_t &impl() const noexcept {
		return _data.elements;
	}
	constexpr impl_t &impl() noexcept {
		return _data.elements;
	}

	template <typename OtherType>
	constexpr typename impl_t::iterator getLowerBound(
			const OtherType &value) noexcept {
		return std::lower_bound(
			std::begin(impl()),
			std::end(impl()),
			value,
			compare());
	}
	template <typename OtherType>
	constexpr typename impl_t::const_iterator getLowerBound(
			const OtherType &value) const noexcept {
		return std::lower_bound(
			std::begin(impl()),
			std::end(impl()),
			value,
			compare());
	}
	template <typename OtherType>
	constexpr typename impl_t::iterator getUpperBound(
			const OtherType &value) noexcept {
		return std::upper_bound(
			std::begin(impl()),
			std::end(impl()),
			value,
			compare());
	}
	template <typename OtherType>
	constexpr typename impl_t::const_iterator getUpperBound(
			const OtherType &value) const noexcept {
		return std::upper_bound(
			std::begin(impl()),
			std::end(impl()),
			value,
			compare());
	}
	template <typename OtherType>
	constexpr std::pair<
		typename impl_t::iterator,
		typename impl_t::iterator
	> getEqualRange(const OtherType &value) noexcept {
		return std::equal_range(
			std::begin(impl()),
			std::end(impl()),
			value,
			compare());
	}
	template <typename OtherType>
	constexpr std::pair<
		typename impl_t::const_iterator,
		typename impl_t::const_iterator
	> getEqualRange(const OtherType &value) const noexcept {
		return std::equal_range(
			std::begin(impl()),
			std::end(impl()),
			value,
			compare());
	}

};

template <typename Type, typename Compare>
class flat_set : private flat_multi_set<Type, Compare> {
	using parent = flat_multi_set<Type, Compare>;

public:
	using iterator = typename parent::iterator;
	using const_iterator = typename parent::const_iterator;
	using reverse_iterator = typename parent::reverse_iterator;
	using const_reverse_iterator = typename parent::const_reverse_iterator;
	using value_type = typename parent::value_type;
	using size_type = typename parent::size_type;
	using difference_type = typename parent::difference_type;
	using pointer = typename parent::pointer;
	using reference = typename parent::reference;

	constexpr flat_set() = default;

	template <
		typename Iterator,
		typename = typename std::iterator_traits<Iterator>::iterator_category
	>
	constexpr flat_set(Iterator first, Iterator last) noexcept
	: parent(first, last) {
		finalize();
	}

	constexpr flat_set(std::initializer_list<Type> iter) noexcept
	: parent(iter.begin(), iter.end()) {
		finalize();
	}

	using parent::parent;
	using parent::size;
	using parent::empty;
	using parent::clear;
	using parent::reserve;
	using parent::begin;
	using parent::end;
	using parent::cbegin;
	using parent::cend;
	using parent::rbegin;
	using parent::rend;
	using parent::crbegin;
	using parent::crend;
	using parent::front;
	using parent::back;
	using parent::contains;
	using parent::erase;

	constexpr std::pair<iterator, bool> insert(const Type &value) noexcept {
		if (this->empty() || this->compare()(this->back(), value)) {
			this->impl().push_back(value);
			return std::make_pair(this->end() - 1, true);
		}
		auto where = this->getLowerBound(value);
		if (this->compare()(value, *where)) {
			return std::make_pair(this->impl().insert(where, value), true);
		}
		return std::make_pair(where, false);
	}
	constexpr std::pair<iterator, bool> insert(Type &&value) noexcept {
		if (this->empty() || this->compare()(this->back(), value)) {
			this->impl().push_back(std::move(value));
			return std::make_pair(this->end() - 1, true);
		}
		auto where = this->getLowerBound(value);
		if (this->compare()(value, *where)) {
			return std::make_pair(
				this->impl().insert(where, std::move(value)),
				true);
		}
		return std::make_pair(where, false);
	}
	template <typename... Args>
	constexpr std::pair<iterator, bool> emplace(Args&&... args) noexcept {
		return this->insert(Type(std::forward<Args>(args)...));
	}

	template <typename OtherType>
	constexpr bool remove(const OtherType &value) noexcept {
		return this->removeOne(value);
	}
	constexpr bool remove(const Type &value) noexcept {
		return remove<Type>(value);
	}

	template <typename OtherType>
	constexpr iterator find(const OtherType &value) noexcept {
		return this->findFirst(value);
	}
	constexpr iterator find(const Type &value) noexcept {
		return find<Type>(value);
	}
	template <typename OtherType>
	constexpr const_iterator find(const OtherType &value) const noexcept {
		return this->findFirst(value);
	}
	constexpr const_iterator find(const Type &value) const noexcept {
		return find<Type>(value);
	}

	template <typename Action>
	constexpr void modify(iterator which, Action action) noexcept {
		action(which.wrapped());
		for (auto i = iterator(which + 1), e = end(); i != e; ++i) {
			if (this->compare()(*i, *which)) {
				std::swap(i.wrapped(), which.wrapped());
			} else if (!this->compare()(*which, *i)) {
				erase(which);
				return;
			} else{
				break;
			}
		}
		for (auto i = which, b = begin(); i != b;) {
			--i;
			if (this->compare()(*which, *i)) {
				std::swap(i.wrapped(), which.wrapped());
			} else if (!this->compare()(*i, *which)) {
				erase(which);
				return;
			} else {
				break;
			}
		}
	}

	template <
		typename Iterator,
		typename = typename std::iterator_traits<Iterator>::iterator_category>
	constexpr void merge(Iterator first, Iterator last) noexcept {
		parent::merge(first, last);
		finalize();
	}

	constexpr void merge(
			const flat_multi_set<Type, Compare> &other) noexcept {
		merge(other.begin(), other.end());
	}

	constexpr void merge(std::initializer_list<Type> list) noexcept {
		merge(list.begin(), list.end());
	}

	friend inline constexpr bool operator<(
			const flat_set &a,
			const flat_set &b) noexcept {
		return static_cast<const parent&>(a) < static_cast<const parent&>(b);
	}
	friend inline constexpr bool operator>(
			const flat_set &a,
			const flat_set &b) noexcept {
		return static_cast<const parent&>(a) > static_cast<const parent&>(b);
	}
	friend inline constexpr bool operator<=(
			const flat_set &a,
			const flat_set &b) noexcept {
		return static_cast<const parent&>(a) <= static_cast<const parent&>(b);
	}
	friend inline constexpr bool operator>=(
			const flat_set &a,
			const flat_set &b) noexcept {
		return static_cast<const parent&>(a) >= static_cast<const parent&>(b);
	}
	friend inline constexpr bool operator==(
			const flat_set &a,
			const flat_set &b) noexcept {
		return static_cast<const parent&>(a) == static_cast<const parent&>(b);
	}
	friend inline constexpr bool operator!=(
			const flat_set &a,
			const flat_set &b) noexcept {
		return static_cast<const parent&>(a) != static_cast<const parent&>(b);
	}

private:
	constexpr void finalize() noexcept {
		this->impl().erase(
			std::unique(
				std::begin(this->impl()),
				std::end(this->impl()),
				[&](auto &&a, auto &&b) {
					return !this->compare()(a, b);
				}
			),
			std::end(this->impl()));
	}

};

} // namespace base
