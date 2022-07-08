// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#if defined _MSC_VER || defined __MINGW32__
#define CRL_USE_WINAPI_TIME
#elif defined __APPLE__ // _MSC_VER
#define CRL_USE_MAC_TIME
#else // __APPLE__
#define CRL_USE_LINUX_TIME
#endif // !_MSC_VER && !__APPLE__

#if defined _MSC_VER && !defined CRL_FORCE_QT

#if defined _WIN64
#define CRL_USE_WINAPI
#define CRL_WINAPI_X64
#elif defined _M_IX86 // _WIN64
#define CRL_USE_WINAPI
#define CRL_WINAPI_X86
//#define CRL_THROW_FP_EXCEPTIONS
#else // _M_IX86
#error "Configuration is not supported."
#endif // !_WIN64 && !_M_IX86

#ifdef CRL_FORCE_STD_LIST
#define CRL_USE_COMMON_LIST
#else // CRL_FORCE_STD_LIST
#define CRL_USE_WINAPI_LIST
#endif // !CRL_FORCE_STD_LIST

#elif __has_include(<dispatch/dispatch.h>) && !defined CRL_FORCE_QT // _MSC_VER && !CRL_FORCE_QT

// gcc compatibility
#ifndef __has_feature
#define __has_feature(x) 0
#endif // !__has_feature

#ifndef __has_extension
#define __has_extension __has_feature
#endif // !__has_extension

#define CRL_USE_DISPATCH
#define CRL_USE_COMMON_LIST

#elif __has_include(<QtCore/QThreadPool>) // dispatch && !CRL_FORCE_QT

#define CRL_USE_QT
#define CRL_USE_COMMON_LIST

#else // Qt
#error "Configuration is not supported."
#endif // !_MSC_VER && !__APPLE__ && !Qt

#if __has_include(<rpl/producer.h>)
#define CRL_ENABLE_RPL_INTEGRATION
#endif // __has_include(<rpl/producer.h>)
