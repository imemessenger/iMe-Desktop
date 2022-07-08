// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <stdint.h>

// thanks Chromium

// Compiler detection.
#if defined(__clang__)
#define COMPILER_CLANG 1
#elif defined(__GNUC__) // __clang__
#define COMPILER_GCC 1
#elif defined(_MSC_VER) // __clang__ || __GNUC__
#define COMPILER_MSVC 1
#endif // _MSC_VER || __clang__ || __GNUC__

// Processor architecture detection.
#if defined(_M_X64) || defined(__x86_64__)
#define ARCH_CPU_X86_FAMILY 1
#define ARCH_CPU_X86_64 1
#elif defined(_M_IX86) || defined(__i386__)
#define ARCH_CPU_X86_FAMILY 1
#define ARCH_CPU_X86 1
#endif
// _LP64 is defined by GCC, others by MSVC
#if defined _LP64 || defined _M_X64 || defined _M_ARM64 || defined _M_ALPHA
#define ARCH_CPU_64_BITS 1
#else
#define ARCH_CPU_32_BITS 1
#endif

#if defined(__GNUC__)
#define TG_FORCE_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define TG_FORCE_INLINE __forceinline
#else
#define TG_FORCE_INLINE inline
#endif

#include <climits>
static_assert(CHAR_BIT == 8, "Not supported char size.");
