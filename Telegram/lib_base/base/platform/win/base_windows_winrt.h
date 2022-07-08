// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/debug_log.h"

// class has virtual functions, but destructor is not virtual
#pragma warning(push)
#pragma warning(disable:4265)
#include <winrt/base.h>
#pragma warning(pop)

namespace base::WinRT {
namespace details {

template <typename Method>
inline constexpr bool ReturnsVoid = std::is_same_v<
	std::invoke_result_t<Method>,
	void>;

template <typename Method>
using TryResult = std::conditional_t<
	ReturnsVoid<Method>,
	bool,
	std::optional<std::invoke_result_t<Method>>>;

} // namespace details

[[nodiscard]] bool Supported();

template <typename Method>
inline details::TryResult<Method> TryNoCheck(Method &&method) noexcept {
	try {
		if constexpr (details::ReturnsVoid<Method>) {
			method();
			return true;
		} else {
			return method();
		}
	} catch (const std::bad_alloc &) {
		Unexpected("Could not allocate in WinRT.");
	} catch (const winrt::hresult_error &error) {
		LOG(("WinRT Error: %1 (%2)"
			).arg(error.code()
			).arg(QString::fromWCharArray(error.message().c_str())));
		return {};
	} catch (...) {
		LOG(("WinRT Error: Unknown."));
		return {};
	}
}

template <typename Method>
inline details::TryResult<Method> Try(Method &&method) noexcept {
	if (!Supported()) {
		return {};
	}
	return TryNoCheck(std::forward<Method>(method));
}

template <typename Interface>
auto TryCreateInstance(
		const winrt::guid &clsid,
		uint32_t context = 0x1 /*CLSCTX_INPROC_SERVER*/,
		void *outer = nullptr) {
	return TryNoCheck([&] {
		return winrt::create_instance<Interface>(clsid, context, outer);
	}).value_or(winrt::com_ptr<Interface>());
}

} // namespace base::WinRT
