// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <vector>
#include <algorithm>
#include "base/optional.h"

namespace base {

using std::begin;
using std::end;

template <
	typename Key,
	typename Type,
	typename Compare = std::less<>>
class flat_map;

template <
	typename Key,
	typename Type,
	typename Compare = std::less<>>
class flat_multi_map;

template <
	typename Me,
	typename Key,
	typename Type,
	typename iterator_impl,
	typename pointer_impl,
	typename reference_impl>
class flat_multi_map_iterator_base_impl;

template <typename Key, typename Value>
struct flat_multi_map_pair_type {
	using first_type = const Key;
	using second_type = Value;

	constexpr flat_multi_map_pair_type() noexcept
	: first()
	, second() {
	}

	template <typename OtherKey, typename OtherValue>
	constexpr flat_multi_map_pair_type(
		OtherKey &&key,
		OtherValue &&value) noexcept
	: first(std::forward<OtherKey>(key))
	, second(std::forward<OtherValue>(value)) {
	}

	constexpr flat_multi_map_pair_type(
		const flat_multi_map_pair_type &pair) noexcept
	: first(pair.first)
	, second(pair.second) {
	}

	constexpr flat_multi_map_pair_type(
		flat_multi_map_pair_type &&pair) noexcept
	: first(std::move(const_cast<Key&>(pair.first)))
	, second(std::move(pair.second)) {
	}

	flat_multi_map_pair_type &operator=(
		const flat_multi_map_pair_type&) = delete;
	constexpr flat_multi_map_pair_type &operator=(
			flat_multi_map_pair_type &&other) noexcept {
		const_cast<Key&>(first) = std::move(const_cast<Key&>(other.first));
		second = std::move(other.second);
		return *this;
	}

	friend inline constexpr bool operator<(
		const flat_multi_map_pair_type &a,
		const flat_multi_map_pair_type &b) noexcept {
		if (a.first < b.first) {
			return true;
		} else if (a.first > b.first) {
			return false;
		}
		return (a.second < b.second);
	}
	friend inline constexpr bool operator>(
			const flat_multi_map_pair_type &a,
			const flat_multi_map_pair_type &b) noexcept {
		return b < a;
	}
	friend inline constexpr bool operator<=(
			const flat_multi_map_pair_type &a,
			const flat_multi_map_pair_type &b) noexcept {
		return !(b < a);
	}
	friend inline constexpr bool operator>=(
			const flat_multi_map_pair_type &a,
			const flat_multi_map_pair_type &b) noexcept {
		return !(a < b);
	}
	friend inline constexpr bool operator==(
			const flat_multi_map_pair_type &a,
			const flat_multi_map_pair_type &b) noexcept {
		return (a.first == b.first) && (a.second == b.second);
	}
	friend inline constexpr bool operator!=(
			const flat_multi_map_pair_type &a,
			const flat_multi_map_pair_type &b) noexcept {
		return !(a == b);
	}

	constexpr void swap(flat_multi_map_pair_type &other) noexcept {
		using std::swap;

		if (this != &other) {
			std::swap(
				const_cast<Key&>(first),
				const_cast<Key&>(other.first));
			std::swap(second, other.second);
		}
	}

	const Key first;
	Value second;

};

template <
	typename Me,
	typename Key,
	typename Type,
	typename iterator_impl,
	typename pointer_impl,
	typename reference_impl>
class flat_multi_map_iterator_base_impl {
public:
	using iterator_category = typename iterator_impl::iterator_category;

	using pair_type = flat_multi_map_pair_type<Key, Type>;
	using value_type = pair_type;
	using difference_type = typename iterator_impl::difference_type;
	using pointer = pointer_impl;
	using reference = reference_impl;

	constexpr flat_multi_map_iterator_base_impl(
		iterator_impl impl = iterator_impl()) noexcept
	: _impl(impl) {
	}

	constexpr reference operator*() const noexcept {
		return *_impl;
	}
	constexpr pointer operator->() const noexcept {
		return std::addressof(**this);
	}
	constexpr Me &operator++() noexcept {
		++_impl;
		return static_cast<Me&>(*this);
	}
	constexpr Me operator++(int) noexcept {
		return _impl++;
	}
	constexpr Me &operator--() noexcept {
		--_impl;
		return static_cast<Me&>(*this);
	}
	constexpr Me operator--(int) noexcept {
		return _impl--;
	}
	constexpr Me &operator+=(difference_type offset) noexcept {
		_impl += offset;
		return static_cast<Me&>(*this);
	}
	constexpr Me operator+(difference_type offset) const noexcept {
		return _impl + offset;
	}
	constexpr Me &operator-=(difference_type offset) noexcept {
		_impl -= offset;
		return static_cast<Me&>(*this);
	}
	constexpr Me operator-(difference_type offset) const noexcept {
		return _impl - offset;
	}
	template <
		typename other_me,
		typename other_iterator_impl,
		typename other_pointer_impl,
		typename other_reference_impl>
	constexpr difference_type operator-(
			const flat_multi_map_iterator_base_impl<
				other_me,
				Key,
				Type,
				other_iterator_impl,
				other_pointer_impl,
				other_reference_impl> &right) const noexcept {
		return _impl - right._impl;
	}
	constexpr reference operator[](difference_type offset) const noexcept {
		return _impl[offset];
	}

	template <
		typename other_me,
		typename other_iterator_impl,
		typename other_pointer_impl,
		typename other_reference_impl>
	constexpr bool operator==(
			const flat_multi_map_iterator_base_impl<
				other_me,
				Key,
				Type,
				other_iterator_impl,
				other_pointer_impl,
				other_reference_impl> &right) const noexcept {
		return _impl == right._impl;
	}
	template <
		typename other_me,
		typename other_iterator_impl,
		typename other_pointer_impl,
		typename other_reference_impl>
	constexpr bool operator!=(
			const flat_multi_map_iterator_base_impl<
				other_me,
				Key,
				Type,
				other_iterator_impl,
				other_pointer_impl,
				other_reference_impl> &right) const noexcept {
		return _impl != right._impl;
	}
	template <
		typename other_me,
		typename other_iterator_impl,
		typename other_pointer_impl,
		typename other_reference_impl>
	constexpr bool operator<(
			const flat_multi_map_iterator_base_impl<
				other_me,
				Key,
				Type,
				other_iterator_impl,
				other_pointer_impl,
				other_reference_impl> &right) const noexcept {
		return _impl < right._impl;
	}

private:
	iterator_impl _impl;

	template <
		typename OtherKey,
		typename OtherType,
		typename OtherCompare>
	friend class flat_multi_map;

	template <
		typename OtherMe,
		typename OtherKey,
		typename OtherType,
		typename other_iterator_impl,
		typename other_pointer_impl,
		typename other_reference_impl>
	friend class flat_multi_map_iterator_base_impl;

};

template <typename Key, typename Type, typename Compare>
class flat_multi_map {
public:
	class iterator;
	class const_iterator;
	class reverse_iterator;
	class const_reverse_iterator;

private:
	using pair_type = flat_multi_map_pair_type<Key, Type>;
	using impl_t = std::vector<pair_type>;

	using iterator_base = flat_multi_map_iterator_base_impl<
		iterator,
		Key,
		Type,
		typename impl_t::iterator,
		pair_type*,
		pair_type&>;
	using const_iterator_base = flat_multi_map_iterator_base_impl<
		const_iterator,
		Key,
		Type,
		typename impl_t::const_iterator,
		const pair_type*,
		const pair_type&>;
	using reverse_iterator_base = flat_multi_map_iterator_base_impl<
		reverse_iterator,
		Key,
		Type,
		typename impl_t::reverse_iterator,
		pair_type*,
		pair_type&>;
	using const_reverse_iterator_base = flat_multi_map_iterator_base_impl<
		const_reverse_iterator,
		Key,
		Type,
		typename impl_t::const_reverse_iterator,
		const pair_type*,
		const pair_type&>;

public:
	using value_type = pair_type;
	using size_type = typename impl_t::size_type;
	using difference_type = typename impl_t::difference_type;
	using pointer = pair_type*;
	using const_pointer = const pair_type*;
	using reference = pair_type&;
	using const_reference = const pair_type&;

	class iterator : public iterator_base {
	public:
		using iterator_base::iterator_base;
		constexpr iterator() = default;
		constexpr iterator(const iterator_base &other) noexcept
		: iterator_base(other) {
		}
		friend class const_iterator;

	};
	class const_iterator : public const_iterator_base {
	public:
		using const_iterator_base::const_iterator_base;
		constexpr const_iterator() = default;
		constexpr const_iterator(const_iterator_base other) noexcept
		: const_iterator_base(other) {
		}
		constexpr const_iterator(const iterator &other) noexcept
		: const_iterator_base(other._impl) {
		}

	};
	class reverse_iterator : public reverse_iterator_base {
	public:
		using reverse_iterator_base::reverse_iterator_base;
		constexpr reverse_iterator() = default;
		constexpr reverse_iterator(reverse_iterator_base other) noexcept
		: reverse_iterator_base(other) {
		}
		friend class const_reverse_iterator;

	};
	class const_reverse_iterator : public const_reverse_iterator_base {
	public:
		using const_reverse_iterator_base::const_reverse_iterator_base;
		constexpr const_reverse_iterator() = default;
		constexpr const_reverse_iterator(
			const_reverse_iterator_base other) noexcept
		: const_reverse_iterator_base(other) {
		}
		constexpr const_reverse_iterator(
			const reverse_iterator &other) noexcept
		: const_reverse_iterator_base(other._impl) {
		}

	};

	constexpr flat_multi_map() = default;
	constexpr flat_multi_map(const flat_multi_map &other) = default;
	constexpr flat_multi_map(flat_multi_map &&other) = default;
	constexpr flat_multi_map &operator=(
			const flat_multi_map &other) noexcept {
		auto copy = other;
		return (*this = std::move(copy));
	}
	constexpr flat_multi_map &operator=(flat_multi_map &&other) = default;

	friend inline constexpr bool operator<(
			const flat_multi_map &a,
			const flat_multi_map &b) noexcept {
		return a.impl() < b.impl();
	}
	friend inline constexpr bool operator>(
			const flat_multi_map &a,
			const flat_multi_map &b) noexcept {
		return a.impl() > b.impl();
	}
	friend inline constexpr bool operator<=(
			const flat_multi_map &a,
			const flat_multi_map &b) noexcept {
		return a.impl() <= b.impl();
	}
	friend inline constexpr bool operator>=(
			const flat_multi_map &a,
			const flat_multi_map &b) noexcept {
		return a.impl() >= b.impl();
	}
	friend inline constexpr bool operator==(
			const flat_multi_map &a,
			const flat_multi_map &b) noexcept {
		return a.impl() == b.impl();
	}
	friend inline constexpr bool operator!=(
			const flat_multi_map &a,
			const flat_multi_map &b) noexcept {
		return a.impl() != b.impl();
	}

	template <
		typename Iterator,
		typename = typename std::iterator_traits<Iterator>::iterator_category>
	constexpr flat_multi_map(Iterator first, Iterator last) noexcept
	: _data(first, last) {
		std::sort(std::begin(impl()), std::end(impl()), compare());
	}

	constexpr flat_multi_map(std::initializer_list<pair_type> iter) noexcept
	: flat_multi_map(iter.begin(), iter.end()) {
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

	constexpr reference front() noexcept {
		return *begin();
	}
	constexpr const_reference front() const noexcept {
		return *begin();
	}
	constexpr reference back() noexcept {
		return *(end() - 1);
	}
	constexpr const_reference back() const noexcept {
		return *(end() - 1);
	}

	constexpr iterator insert(const value_type &value) noexcept {
		if (empty() || !compare()(value.first, back().first)) {
			impl().push_back(value);
			return (end() - 1);
		}
		auto where = getUpperBound(value.first);
		return impl().insert(where, value);
	}
	constexpr iterator insert(value_type &&value) noexcept {
		if (empty() || !compare()(value.first, back().first)) {
			impl().push_back(std::move(value));
			return (end() - 1);
		}
		auto where = getUpperBound(value.first);
		return impl().insert(where, std::move(value));
	}
	template <typename... Args>
	constexpr iterator emplace(Args&&... args) noexcept {
		return insert(value_type(std::forward<Args>(args)...));
	}

	template <typename OtherKey>
	constexpr bool removeOne(const OtherKey &key) noexcept {
		if (empty()
			|| compare()(key, front().first)
			|| compare()(back().first, key)) {
			return false;
		}
		auto where = getLowerBound(key);
		if (compare()(key, where->first)) {
			return false;
		}
		impl().erase(where);
		return true;
	}
	constexpr bool removeOne(const Key &key) noexcept {
		return removeOne<Key>(key);
	}
	template <typename OtherKey>
	constexpr int removeAll(const OtherKey &key) noexcept {
		if (empty()
			|| compare()(key, front().first)
			|| compare()(back().first, key)) {
			return 0;
		}
		auto range = getEqualRange(key);
		if (range.first == range.second) {
			return 0;
		}
		const auto result = (range.second - range.first);
		impl().erase(range.first, range.second);
		return result;
	}
	constexpr int removeAll(const Key &key) noexcept {
		return removeAll<Key>(key);
	}
	template <typename OtherKey>
	constexpr bool remove(const OtherKey &key, const Type &value) noexcept {
		if (empty()
			|| compare()(key, front().first)
			|| compare()(back().first, key)) {
			return false;
		}
		const auto e = impl().end();
		for (auto where = getLowerBound(key);
			(where != e) && !compare()(key, where->first);
			++where) {
			if (where->second == value) {
				impl().erase(where);
				return true;
			}
		}
		return false;
	}
	constexpr bool remove(const Key &key, const Type &value) noexcept {
		return remove<Key>(key, value);
	}

	constexpr iterator erase(const_iterator where) noexcept {
		return impl().erase(where._impl);
	}
	constexpr iterator erase(
			const_iterator from,
			const_iterator till) noexcept {
		return impl().erase(from._impl, till._impl);
	}
	constexpr int erase(const Key &key) {
		return removeAll(key);
	}

	template <typename OtherKey>
	constexpr iterator findFirst(const OtherKey &key) noexcept {
		if (empty()
			|| compare()(key, front().first)
			|| compare()(back().first, key)) {
			return end();
		}
		auto where = getLowerBound(key);
		return compare()(key, where->first) ? impl().end() : where;
	}
	constexpr iterator findFirst(const Key &key) noexcept {
		return findFirst<Key>(key);
	}

	template <typename OtherKey>
	constexpr const_iterator findFirst(const OtherKey &key) const noexcept {
		if (empty()
			|| compare()(key, front().first)
			|| compare()(back().first, key)) {
			return end();
		}
		auto where = getLowerBound(key);
		return compare()(key, where->first) ? impl().end() : where;
	}
	constexpr const_iterator findFirst(const Key &key) const noexcept {
		return findFirst<Key>(key);
	}

	template <typename OtherKey>
	constexpr bool contains(const OtherKey &key) const noexcept {
		return findFirst(key) != end();
	}
	constexpr bool contains(const Key &key) const noexcept {
		return contains<Key>(key);
	}

	template <typename OtherKey>
	constexpr int count(const OtherKey &key) const noexcept {
		if (empty()
			|| compare()(key, front().first)
			|| compare()(back().first, key)) {
			return 0;
		}
		auto range = getEqualRange(key);
		return (range.second - range.first);
	}
	constexpr int count(const Key &key) const noexcept {
		return count<Key>(key);
	}

private:
	friend class flat_map<Key, Type, Compare>;

	struct transparent_compare : Compare {
		constexpr const Compare &initial() const noexcept {
			return *this;
		}

		template <
			typename OtherType1,
			typename OtherType2,
			typename = std::enable_if_t<
				!std::is_same_v<std::decay_t<OtherType1>, pair_type> &&
				!std::is_same_v<std::decay_t<OtherType2>, pair_type>>>
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
		std::is_same_v<std::decay_t<OtherType1>, pair_type> &&
		std::is_same_v<std::decay_t<OtherType2>, pair_type>, bool> {
			return initial()(a.first, b.first);
		}
		template <
			typename OtherType,
			typename = std::enable_if_t<
				!std::is_same_v<std::decay_t<OtherType>, pair_type>>>
		constexpr auto operator()(
				const pair_type &a,
				OtherType &&b) const noexcept {
			return operator()(a.first, std::forward<OtherType>(b));
		}
		template <
			typename OtherType,
			typename = std::enable_if_t<
				!std::is_same_v<std::decay_t<OtherType>, pair_type>>>
		constexpr auto operator()(
				OtherType &&a,
				const pair_type &b) const noexcept {
			return operator()(std::forward<OtherType>(a), b.first);
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

	template <typename OtherKey>
	constexpr typename impl_t::iterator getLowerBound(
			const OtherKey &key) noexcept {
		return std::lower_bound(
			std::begin(impl()),
			std::end(impl()),
			key,
			compare());
	}
	template <typename OtherKey>
	constexpr typename impl_t::const_iterator getLowerBound(
			const OtherKey &key) const noexcept {
		return std::lower_bound(
			std::begin(impl()),
			std::end(impl()),
			key,
			compare());
	}
	template <typename OtherKey>
	constexpr typename impl_t::iterator getUpperBound(
			const OtherKey &key) noexcept {
		return std::upper_bound(
			std::begin(impl()),
			std::end(impl()),
			key,
			compare());
	}
	template <typename OtherKey>
	constexpr typename impl_t::const_iterator getUpperBound(
			const OtherKey &key) const noexcept {
		return std::upper_bound(
			std::begin(impl()),
			std::end(impl()),
			key,
			compare());
	}
	template <typename OtherKey>
	constexpr std::pair<
		typename impl_t::iterator,
		typename impl_t::iterator
	> getEqualRange(const OtherKey &key) noexcept {
		return std::equal_range(
			std::begin(impl()),
			std::end(impl()),
			key,
			compare());
	}
	template <typename OtherKey>
	constexpr std::pair<
		typename impl_t::const_iterator,
		typename impl_t::const_iterator
	> getEqualRange(const OtherKey &key) const noexcept {
		return std::equal_range(
			std::begin(impl()),
			std::end(impl()),
			key,
			compare());
	}

};

template <typename Key, typename Type, typename Compare>
class flat_map : private flat_multi_map<Key, Type, Compare> {
	using parent = flat_multi_map<Key, Type, Compare>;
	using pair_type = typename parent::pair_type;

public:
	using value_type = typename parent::value_type;
	using size_type = typename parent::size_type;
	using difference_type = typename parent::difference_type;
	using pointer = typename parent::pointer;
	using const_pointer = typename parent::const_pointer;
	using reference = typename parent::reference;
	using const_reference = typename parent::const_reference;
	using iterator = typename parent::iterator;
	using const_iterator = typename parent::const_iterator;
	using reverse_iterator = typename parent::reverse_iterator;
	using const_reverse_iterator = typename parent::const_reverse_iterator;

	constexpr flat_map() = default;

	template <
		typename Iterator,
		typename = typename std::iterator_traits<Iterator>::iterator_category
	>
	constexpr flat_map(Iterator first, Iterator last) noexcept
	: parent(first, last) {
		finalize();
	}

	constexpr flat_map(std::initializer_list<pair_type> iter) noexcept
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
	using parent::erase;
	using parent::contains;

	std::pair<iterator, bool> insert(const value_type &value) {
		if (this->empty()
			|| this->compare()(this->back().first, value.first)) {
			this->impl().push_back(value);
			return { this->end() - 1, true };
		}
		auto where = this->getLowerBound(value.first);
		if (this->compare()(value.first, where->first)) {
			return { this->impl().insert(where, value), true };
		}
		return { where, false };
	}
	constexpr std::pair<iterator, bool> insert(value_type &&value) {
		if (this->empty() || this->compare()(this->back().first, value.first)) {
			this->impl().push_back(std::move(value));
			return { this->end() - 1, true };
		}
		auto where = this->getLowerBound(value.first);
		if (this->compare()(value.first, where->first)) {
			return { this->impl().insert(where, std::move(value)), true };
		}
		return { where, false };
	}
	constexpr std::pair<iterator, bool> insert_or_assign(
			const Key &key,
			const Type &value) noexcept {
		if (this->empty() || this->compare()(this->back().first, key)) {
			this->impl().emplace_back(key, value);
			return { this->end() - 1, true };
		}
		auto where = this->getLowerBound(key);
		if (this->compare()(key, where->first)) {
			return {
				this->impl().insert(where, value_type(key, value)),
				true
			};
		}
		where->second = value;
		return { where, false };
	}
	constexpr std::pair<iterator, bool> insert_or_assign(
			const Key &key,
			Type &&value) noexcept {
		if (this->empty() || this->compare()(this->back().first, key)) {
			this->impl().emplace_back(key, std::move(value));
			return { this->end() - 1, true };
		}
		auto where = this->getLowerBound(key);
		if (this->compare()(key, where->first)) {
			return {
				this->impl().insert(
					where,
					value_type(key, std::move(value))),
				true
			};
		}
		where->second = std::move(value);
		return { where, false };
	}
	template <typename OtherKey, typename... Args>
	constexpr std::pair<iterator, bool> emplace(
			OtherKey &&key,
			Args&&... args) noexcept {
		return this->insert(value_type(
			std::forward<OtherKey>(key),
			Type(std::forward<Args>(args)...)));
	}
	template <typename... Args>
	constexpr std::pair<iterator, bool> emplace_or_assign(
			const Key &key,
			Args&&... args) noexcept {
		return this->insert_or_assign(
			key,
			Type(std::forward<Args>(args)...));
	}
	template <typename... Args>
	constexpr std::pair<iterator, bool> try_emplace(
			const Key &key,
			Args&&... args) noexcept {
		if (this->empty() || this->compare()(this->back().first, key)) {
			this->impl().push_back(value_type(
				key,
				Type(std::forward<Args>(args)...)));
			return { this->end() - 1, true };
		}
		auto where = this->getLowerBound(key);
		if (this->compare()(key, where->first)) {
			return {
				this->impl().insert(
					where,
					value_type(
						key,
						Type(std::forward<Args>(args)...))),
				true
			};
		}
		return { where, false };
	}

	template <typename OtherKey>
	constexpr bool remove(const OtherKey &key) noexcept {
		return this->removeOne(key);
	}
	constexpr bool remove(const Key &key) noexcept {
		return remove<Key>(key);
	}

	template <typename OtherKey>
	constexpr iterator find(const OtherKey &key) noexcept {
		return this->template findFirst<OtherKey>(key);
	}
	constexpr iterator find(const Key &key) noexcept {
		return find<Key>(key);
	}

	template <typename OtherKey>
	constexpr const_iterator find(const OtherKey &key) const noexcept {
		return this->template findFirst<OtherKey>(key);
	}
	constexpr const_iterator find(const Key &key) const noexcept {
		return find<Key>(key);
	}

	constexpr Type &operator[](const Key &key) noexcept {
		if (this->empty() || this->compare()(this->back().first, key)) {
			this->impl().push_back({ key, Type() });
			return this->back().second;
		}
		auto where = this->getLowerBound(key);
		if (this->compare()(key, where->first)) {
			return this->impl().insert(where, { key, Type() })->second;
		}
		return where->second;
	}

	template <typename OtherKey>
	constexpr std::optional<Type> take(const OtherKey &key) noexcept {
		auto it = find(key);
		if (it == this->end()) {
			return std::nullopt;
		}
		auto result = std::move(it->second);
		this->erase(it);
		return result;
	}
	constexpr std::optional<Type> take(const Key &key) noexcept {
		return take<Key>(key);
	}

	friend inline constexpr bool operator<(
			const flat_map &a,
			const flat_map &b) noexcept {
		return static_cast<const parent&>(a) < static_cast<const parent&>(b);
	}
	friend inline constexpr bool operator>(
			const flat_map &a,
			const flat_map &b) noexcept {
		return static_cast<const parent&>(a) > static_cast<const parent&>(b);
	}
	friend inline constexpr bool operator<=(
			const flat_map &a,
			const flat_map &b) noexcept {
		return static_cast<const parent&>(a) <= static_cast<const parent&>(b);
	}
	friend inline constexpr bool operator>=(
			const flat_map &a,
			const flat_map &b) noexcept {
		return static_cast<const parent&>(a) >= static_cast<const parent&>(b);
	}
	friend inline constexpr bool operator==(
			const flat_map &a,
			const flat_map &b) noexcept {
		return static_cast<const parent&>(a) == static_cast<const parent&>(b);
	}
	friend inline constexpr bool operator!=(
			const flat_map &a,
			const flat_map &b) noexcept {
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

// Structured bindings support.
namespace std {

template <typename Key, typename Value>
class tuple_size<base::flat_multi_map_pair_type<Key, Value>>
: public integral_constant<size_t, 2> {
};

template <typename Key, typename Value>
class tuple_element<0, base::flat_multi_map_pair_type<Key, Value>> {
public:
	using type = const Key;
};

template <typename Key, typename Value>
class tuple_element<1, base::flat_multi_map_pair_type<Key, Value>> {
public:
	using type = Value;
};

} // namespace std

// Structured bindings support.
namespace base {
namespace details {

template <std::size_t N, typename Key, typename Value>
using flat_multi_map_pair_element = std::tuple_element_t<
	N,
	flat_multi_map_pair_type<Key, Value>>;

} // namespace details

template <std::size_t N, typename Key, typename Value>
constexpr auto get(base::flat_multi_map_pair_type<Key, Value> &value) noexcept
-> details::flat_multi_map_pair_element<N, Key, Value> & {
	if constexpr (N == 0) {
		return value.first;
	} else {
		return value.second;
	}
}

template <std::size_t N, typename Key, typename Value>
constexpr auto get(
	const base::flat_multi_map_pair_type<Key, Value> &value) noexcept
-> const details::flat_multi_map_pair_element<N, Key, Value> & {
	if constexpr (N == 0) {
		return value.first;
	} else {
		return value.second;
	}
}

} // namespace base
