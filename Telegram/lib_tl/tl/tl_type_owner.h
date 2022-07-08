// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/algorithm.h"

namespace tl::details {

class type_data {
public:
	type_data() = default;
	type_data(const type_data &other) = delete;
	type_data(type_data &&other) = delete;
	type_data &operator=(const type_data &other) = delete;
	type_data &operator=(type_data &&other) = delete;

	virtual ~type_data() {
	}

private:
	void incrementCounter() const {
		_counter.ref();
	}
	bool decrementCounter() const {
		return _counter.deref();
	}
	friend class type_owner;

	mutable QAtomicInt _counter = { 1 };

};

class type_owner {
public:
	type_owner(type_owner &&other) : _data(base::take(other._data)) {
	}
	type_owner(const type_owner &other) : _data(other._data) {
		incrementCounter();
	}
	type_owner &operator=(type_owner &&other) {
		if (other._data != _data) {
			decrementCounter();
			_data = base::take(other._data);
		}
		return *this;
	}
	type_owner &operator=(const type_owner &other) {
		if (other._data != _data) {
			setData(other._data);
			incrementCounter();
		}
		return *this;
	}
	~type_owner() {
		decrementCounter();
	}

protected:
	type_owner() = default;
	type_owner(const type_data *data) : _data(data) {
	}

	void setData(const type_data *data) {
		decrementCounter();
		_data = data;
	}

	// Unsafe cast, type should be checked by the caller.
	template <typename DataType>
	const DataType &queryData() const {
		Expects(_data != nullptr);

		return static_cast<const DataType &>(*_data);
	}

	bool hasData() const {
		return _data != nullptr;
	}

private:
	void incrementCounter() {
		if (_data) {
			_data->incrementCounter();
		}
	}
	void decrementCounter() {
		if (_data && !_data->decrementCounter()) {
			delete base::take(_data);
		}
	}

	const type_data *_data = nullptr;

};

} // namespace tl::details
