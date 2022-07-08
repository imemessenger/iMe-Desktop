// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <crl/common/crl_common_config.h>

#ifdef CRL_USE_WINAPI

#include <memory>

namespace crl {

class semaphore {
public:
	semaphore() : _handle(implementation::create()) {
	}
	semaphore(const semaphore &other) = delete;
	semaphore &operator=(const semaphore &other) = delete;
	semaphore(semaphore &&other) noexcept
	: _handle(std::move(other._handle)) {
	}
	semaphore &operator=(semaphore &&other) noexcept {
		_handle = std::move(other._handle);
		return *this;
	}

	void acquire();
	void release();

private:
	// Hide WinAPI HANDLE
	struct implementation {
		using pointer = void*;
		static pointer create();
		void operator()(pointer value);
	};
	std::unique_ptr<implementation::pointer, implementation> _handle;

};

} // namespace crl

#endif // CRL_USE_WINAPI
