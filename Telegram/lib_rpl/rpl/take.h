// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

namespace rpl {
namespace details {

class take_helper {
public:
	take_helper(int count) : _count(count) {
	}

	template <
		typename Value,
		typename Error,
		typename Generator>
	auto operator()(producer<Value, Error, Generator> &&initial) {
		return make_producer<Value, Error>([
			initial = std::move(initial),
			limit = _count
		](const auto &consumer) mutable {
			auto count = consumer.template make_state<int>(limit);
			auto initial_consumer = make_consumer<Value, Error>(
			[consumer, count](auto &&value) {
				auto left = (*count)--;
				if (left) {
					consumer.put_next_forward(
						std::forward<decltype(value)>(value));
					--left;
				}
				if (!left) {
					consumer.put_done();
				}
			}, [consumer](auto &&error) {
				consumer.put_error_forward(
					std::forward<decltype(error)>(error));
			}, [consumer] {
				consumer.put_done();
			});
			consumer.add_lifetime(initial_consumer.terminator());
			return std::move(initial).start_existing(initial_consumer);
		});
	}

private:
	int _count = 0;

};

} // namespace details

inline auto take(int count)
-> details::take_helper {
	Expects(count >= 0);

	return details::take_helper(count);
}

namespace details {

template <typename Predicate>
class take_while_helper {
public:
	template <typename OtherPredicate>
	take_while_helper(OtherPredicate &&predicate)
	: _predicate(std::forward<OtherPredicate>(predicate)) {
	}

	template <
		typename Value,
		typename Error,
		typename Generator,
		typename = std::enable_if_t<
		details::is_callable_v<Predicate, Value>>>
	auto operator()(producer<Value, Error, Generator> &&initial) {
	return make_producer<Value, Error>([
			initial = std::move(initial),
			predicate = std::move(_predicate)
		](const auto &consumer) mutable {
			return std::move(initial).start(
			[
				consumer,
				predicate = std::move(predicate)
			](auto &&value) {
				const auto &immutable = value;
				if (details::callable_invoke(
					predicate,
					immutable)
				) {
					consumer.put_next_forward(
						std::forward<decltype(value)>(value));
				} else {
					consumer.put_done();
				}
			}, [consumer](auto &&error) {
				consumer.put_error_forward(
					std::forward<decltype(error)>(error));
			}, [consumer] {
				consumer.put_done();
			});
		});
	}

private:
	Predicate _predicate;

};

} // namespace details

template <typename Predicate>
inline auto take_while(Predicate &&predicate)
-> details::take_while_helper<std::decay_t<Predicate>> {
	return details::take_while_helper<std::decay_t<Predicate>>(
		std::forward<Predicate>(predicate));
}

} // namespace rpl
