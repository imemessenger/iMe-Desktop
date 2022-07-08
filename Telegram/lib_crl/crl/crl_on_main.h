// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <crl/common/crl_common_config.h>

#if defined CRL_USE_WINAPI || defined CRL_USE_COMMON_QUEUE
#include <crl/common/crl_common_on_main.h>
#elif defined CRL_USE_DISPATCH // CRL_USE_WINAPI
#include <crl/dispatch/crl_dispatch_on_main.h>
#elif defined CRL_USE_QT // CRL_USE_DISPATCH
#include <crl/common/crl_common_on_main.h>
#else // CRL_USE_QT
#error "Configuration is not supported."
#endif // !CRL_USE_WINAPI && !CRL_USE_DISPATCH && !CRL_USE_QT

#include <crl/common/crl_common_on_main_guarded.h>
#include <crl/common/crl_common_guards.h>
#include <crl/qt/crl_qt_guards.h>

#ifdef CRL_ENABLE_RPL_INTEGRATION

#include <rpl/producer.h>

namespace crl {

rpl::producer<> on_main_update_requests();

} // namespace crl

#endif // CRL_ENABLE_RPL_INTEGRATION
