// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <crl/common/crl_common_config.h>

#ifdef CRL_USE_QT

#include <memory>
#include <QtCore/QSemaphore>

namespace crl {

class semaphore {
public:
	semaphore() = default;
	semaphore(const semaphore &other) = delete;
	semaphore &operator=(const semaphore &other) = delete;
	semaphore(semaphore &&other) = delete;
	semaphore &operator=(semaphore &&other) = delete;

	void acquire() {
		_impl.acquire();
	}
	void release() {
		_impl.release();
	}

private:
	QSemaphore _impl;

};

} // namespace crl

#endif // CRL_USE_QT
