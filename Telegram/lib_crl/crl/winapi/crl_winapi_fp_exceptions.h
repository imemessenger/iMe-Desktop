// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <crl/common/crl_common_config.h>

#ifdef CRL_USE_WINAPI

#include <crl/common/crl_common_utils.h>
#include <crl/common/crl_common_sync.h>
#include <type_traits>

namespace crl {

void toggle_fp_exceptions(bool throwing);

} // namespace crl

#endif // CRL_USE_WINAPI
