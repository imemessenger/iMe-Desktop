// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <crl/common/crl_common_config.h>

#if defined CRL_USE_WINAPI
#include <crl/winapi/crl_winapi_async.h>
#elif defined CRL_USE_DISPATCH // CRL_USE_WINAPI
#include <crl/dispatch/crl_dispatch_async.h>
#elif defined CRL_USE_QT // CRL_USE_DISPATCH
#include <crl/qt/crl_qt_async.h>
#else // CRL_USE_QT
#error "Configuration is not supported."
#endif // !CRL_USE_WINAPI && !CRL_USE_DISPATCH && !CRL_USE_QT
