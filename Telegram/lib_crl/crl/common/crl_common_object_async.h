// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <crl/common/crl_common_config.h>
#include <crl/crl_on_main.h>
#include <crl/crl_semaphore.h>
#include <memory>
#include <tuple>

#ifdef CRL_ENABLE_RPL_INTEGRATION
#include <rpl/producer.h>
#endif // CRL_ENABLE_RPL_INTEGRATION

namespace crl::details {

template <typename Policy>
class object_async_base
	: public std::enable_shared_from_this<object_async_base<Policy>>
	, private Policy {
public:
	template <
		typename Value,
		typename = std::enable_if_t<!std::is_reference_v<Value>>>
	void destroy(Value &&value) const;

protected:
	template <typename Callable>
	void async(Callable &&callable) const;

	template <typename Callable>
	void sync(Callable &&callable) const;

	~object_async_base() = default;

};

template <typename Type>
class object_async_storage {
protected:
	Type &value();
	const Type &value() const;

	~object_async_storage();

private:
	std::aligned_storage_t<sizeof(Type), alignof(Type)> _storage;

};

template <typename Policy, typename Type>
class object_async_data final
	: private object_async_storage<Type>
	, public object_async_base<Policy> {
public:
	using Object = Type;

	template <typename ...Args>
	void construct(Args &&...args);

	template <typename Method>
	void with(Method &&method);

	template <typename Method>
	void with(Method &&method) const;

	template <typename Method>
	void with_sync(Method &&method);

	template <typename Method>
	void with_sync(Method &&method) const;

};

template <typename Policy, typename Type>
class weak_async final {
	using data = details::object_async_data<
		Policy,
		std::remove_const_t<Type>>;
	using my_data = std::conditional_t<
		std::is_const_v<Type>,
		const data,
		data>;

public:
	weak_async() = default;
	weak_async(const std::shared_ptr<data> &strong);
	weak_async(const weak_async &other) = default;
	weak_async(weak_async &&other) = default;
	weak_async &operator=(const weak_async &other) = default;
	weak_async &operator=(weak_async &&other) = default;

	template <typename Method>
	void with(Method &&method) const;

	template <typename Method>
	void with_sync(Method &&method) const;

	template <
		typename Value,
		typename = std::enable_if_t<!std::is_reference_v<Value>>>
	void destroy(Value &&value) const;

	// Returns a lambda that runs arbitrary callable on the objects queue.
	// const auto r = runner(); r([] { make_some_work_on_queue(); });
	auto runner() const {
		return [weak = *this](auto &&method) {
			weak.with([
				method = std::forward<decltype(method)>(method)
			](Type&) mutable {
				std::move(method)();
			});
		};
	}

#ifdef CRL_ENABLE_RPL_INTEGRATION
	template <
		typename Method,
		typename Invoke,
		typename Result = decltype(
			std::declval<Method>()(std::declval<Type&>()))>
	Result producer(Method &&method, Invoke &&invoke) const;

	template <
		typename Method,
		typename Result = decltype(
			std::declval<Method>()(std::declval<Type&>()))>
	Result producer_on_main(Method &&method) const;
#endif // CRL_ENABLE_RPL_INTEGRATION

private:
	std::weak_ptr<my_data> _weak;

};

template <typename Policy, typename Type>
class object_async final {
public:
	template <typename ...Args>
	object_async(Args &&...args);

	object_async(const object_async &other) = delete;
	object_async &operator=(const object_async &other) = delete;

	template <typename Method>
	void with(Method &&method);

	template <typename Method>
	void with(Method &&method) const;

	template <typename Method>
	void with_sync(Method &&method);

	template <typename Method>
	void with_sync(Method &&method) const;

	template <
		typename Value,
		typename = std::enable_if_t<!std::is_reference_v<Value>>>
	void destroy(Value &&value) const;

#ifdef CRL_ENABLE_RPL_INTEGRATION
	template <
		typename Method,
		typename Invoke,
		typename Result = decltype(
			std::declval<Method>()(std::declval<Type&>()))>
	Result producer(Method &&method, Invoke &&invoke) const;

	template <
		typename Method,
		typename Result = decltype(
			std::declval<Method>()(std::declval<Type&>()))>
	Result producer_on_main(Method &&method) const;
#endif // CRL_ENABLE_RPL_INTEGRATION

	weak_async<Policy, Type> weak();
	weak_async<Policy, const Type> weak() const;

	~object_async();

private:
	using Data = details::object_async_data<Policy, Type>;
	std::shared_ptr<Data> _data;

};

template <typename Policy>
template <typename Callable>
void object_async_base<Policy>::async(Callable &&callable) const {
	Policy::async_plain([
		that = this->shared_from_this(),
		what = std::forward<Callable>(callable)
	]() mutable {
		std::move(what)();
	});
}

template <typename Policy>
template <typename Callable>
void object_async_base<Policy>::sync(Callable &&callable) const {
	semaphore waiter;
	Policy::async_plain([&] {
		const auto guard = details::finally([&] { waiter.release(); });
		callable();
	});
	waiter.acquire();
}

template <typename Policy>
template <typename Value, typename>
void object_async_base<Policy>::destroy(Value &&value) const {
	Policy::async_plain([moved = std::move(value)]{});
}

template <typename Type>
Type &object_async_storage<Type>::value() {
	return *reinterpret_cast<Type*>(&_storage);
}

template <typename Type>
const Type &object_async_storage<Type>::value() const {
	return *reinterpret_cast<const Type*>(&_storage);
}

template <typename Type>
object_async_storage<Type>::~object_async_storage() {
	value().~Type();
}

template <typename Policy, typename Type>
template <typename ...Args>
void object_async_data<Policy, Type>::construct(Args &&...args) {
	object_async_base<Policy>::async([arguments = std::make_tuple(
		&object_async_storage<Type>::value(),
		std::forward<Args>(args)...
	)]() mutable {
		const auto create = [](void *storage, Args &&...args) {
			new (storage) Type(std::forward<Args>(args)...);
		};
		std::apply(create, std::move(arguments));
	});
}

template <typename Policy, typename Type>
template <typename Method>
void object_async_data<Policy, Type>::with(Method &&method) {
	object_async_base<Policy>::async([
		=,
		method = std::forward<Method>(method)
	]() mutable {
		std::move(method)(object_async_storage<Type>::value());
	});
}

template <typename Policy, typename Type>
template <typename Method>
void object_async_data<Policy, Type>::with(Method &&method) const {
	object_async_base<Policy>::async([
		=,
		method = std::forward<Method>(method)
	]() mutable {
		std::move(method)(object_async_storage<Type>::value());
	});
}

template <typename Policy, typename Type>
template <typename Method>
void object_async_data<Policy, Type>::with_sync(Method &&method) {
	object_async_base<Policy>::sync([
		=,
		method = std::forward<Method>(method)
	]() mutable {
		std::move(method)(object_async_storage<Type>::value());
	});
}

template <typename Policy, typename Type>
template <typename Method>
void object_async_data<Policy, Type>::with_sync(Method &&method) const {
	object_async_base<Policy>::sync([
		=,
		method = std::forward<Method>(method)
	]() mutable {
		std::move(method)(object_async_storage<Type>::value());
	});
}

template <typename Policy, typename Type>
weak_async<Policy, Type>::weak_async(const std::shared_ptr<data> &strong)
: _weak(strong) {
}

template <typename Policy, typename Type>
template <typename Method>
void weak_async<Policy, Type>::with(Method &&method) const {
	if (auto strong = _weak.lock()) {
		const auto raw = strong.get();
		raw->with(std::move(method));
		raw->destroy(std::move(strong));
	}
}

template <typename Policy, typename Type>
template <typename Method>
void weak_async<Policy, Type>::with_sync(Method &&method) const {
	if (auto strong = _weak.lock()) {
		const auto raw = strong.get();
		raw->with_sync(std::move(method));
		raw->destroy(std::move(strong));
	}
}

template <typename Policy, typename Type>
template <typename Value, typename>
void weak_async<Policy, Type>::destroy(Value &&value) const {
	if (auto strong = _weak.lock()) {
		const auto raw = strong.get();
		raw->destroy(std::move(value));
		raw->destroy(std::move(strong));
	} else {
		[[maybe_unused]] const auto moved = std::move(value);
	}
}

#ifdef CRL_ENABLE_RPL_INTEGRATION
template <typename Policy, typename Type>
template <typename Method, typename Invoke, typename Result>
Result weak_async<Policy, Type>::producer(
		Method &&method,
		Invoke &&invoke) const {
	return [
		weak = *this,
		method = std::forward<Method>(method),
		invoke = std::forward<Invoke>(invoke)
	](auto consumer) mutable {
		auto lifetime_on_queue = std::make_shared<rpl::lifetime>();
		weak.with([
			method = std::move(method),
			invoke = std::move(invoke),
			consumer = std::move(consumer),
			lifetime_on_queue
		](const Type &that) mutable {
			method(
				that
			) | rpl::start_with_next_error_done([=](auto &&value) {
				invoke([
					consumer,
					value = std::forward<decltype(value)>(value)
				]() mutable {
					consumer.put_next(std::move(value));
				});
			}, [=](auto &&error) {
				invoke([
					consumer,
					error = std::forward<decltype(error)>(error)
				]() mutable {
					consumer.put_error(std::move(error));
				});
			}, [=] {
				invoke([=] {
					consumer.put_done();
				});
			}, *lifetime_on_queue);
		});
		return rpl::lifetime([
			lifetime_on_queue = std::move(lifetime_on_queue),
			weak = std::move(weak)
		]() mutable {
			weak.destroy(std::move(lifetime_on_queue));
		});
	};
}

template <typename Policy, typename Type>
template <typename Method, typename Result>
Result weak_async<Policy, Type>::producer_on_main(Method &&method) const {
	return producer(std::forward<Method>(method), [](auto &&callback) {
		crl::on_main(std::forward<decltype(callback)>(callback));
	});
}
#endif // CRL_ENABLE_RPL_INTEGRATION

template <typename Policy, typename Type>
template <typename ...Args>
object_async<Policy, Type>::object_async(Args &&...args)
: _data(std::make_shared<Data>()) {
	constexpr auto plain_construct = std::is_constructible_v<
		Type,
		Args...>;
	[[maybe_unused]] constexpr auto with_weak_construct = std::is_constructible_v<
		Type,
		weak_async<Policy, Type>,
		Args...>;
	if constexpr (plain_construct) {
		_data->construct(std::forward<Args>(args)...);
	} else if constexpr (with_weak_construct) {
		_data->construct(weak(), std::forward<Args>(args)...);
	} else {
		static_assert(false_t(args...), "Could not find a constructor.");
	}
}

template <typename Policy, typename Type>
template <typename Method>
void object_async<Policy, Type>::with(Method &&method) {
	_data->with(std::forward<Method>(method));
}

template <typename Policy, typename Type>
template <typename Method>
void object_async<Policy, Type>::with(Method &&method) const {
	const auto data = static_cast<const Data*>(_data.get());
	data->with(std::forward<Method>(method));
}

template <typename Policy, typename Type>
template <typename Method>
void object_async<Policy, Type>::with_sync(Method &&method) {
	_data->with_sync(std::forward<Method>(method));
}

template <typename Policy, typename Type>
template <typename Method>
void object_async<Policy, Type>::with_sync(Method &&method) const {
	const auto data = static_cast<const Data*>(_data.get());
	data->with_sync(std::forward<Method>(method));
}

template <typename Policy, typename Type>
template <typename Value, typename>
void object_async<Policy, Type>::destroy(Value &&value) const {
	_data->destroy(std::move(value));
}

#ifdef CRL_ENABLE_RPL_INTEGRATION
template <typename Policy, typename Type>
template <typename Method, typename Callback, typename Result>
Result object_async<Policy, Type>::producer(
		Method &&method,
		Callback &&callback) const {
	return weak().producer(
		std::forward<Method>(method),
		std::forward<Callback>(callback));
}

template <typename Policy, typename Type>
template <typename Method, typename Result>
Result object_async<Policy, Type>::producer_on_main(Method &&method) const {
	return weak().producer_on_main(std::forward<Method>(method));
}
#endif // CRL_ENABLE_RPL_INTEGRATION

template <typename Policy, typename Type>
auto object_async<Policy, Type>::weak() -> weak_async<Policy, Type> {
	return { _data };
}

template <typename Policy, typename Type>
auto object_async<Policy, Type>::weak() const
-> weak_async<Policy, const Type> {
	return { _data };
}

template <typename Policy, typename Type>
object_async<Policy, Type>::~object_async() {
	_data->destroy(std::move(_data));
}

} // namespace crl::details
