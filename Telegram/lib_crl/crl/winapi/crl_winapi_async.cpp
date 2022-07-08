// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include <crl/winapi/crl_winapi_async.h>

#ifdef CRL_USE_WINAPI

#include <concrt.h>

namespace crl::details {

void async_plain(void (*callable)(void*), void *argument) {
	Concurrency::CurrentScheduler::ScheduleTask(callable, argument);
}

} // namespace crl::details

#endif // CRL_USE_WINAPI
