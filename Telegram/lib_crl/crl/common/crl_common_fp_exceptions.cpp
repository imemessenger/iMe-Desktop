// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include <crl/crl_time.h>

#ifndef CRL_USE_WINAPI_TIME

#include <float.h>

namespace crl {

void toggle_fp_exceptions(bool throwing) {
    // We activate them only on Windows right now.
}

} // namespace crl

#endif // !CRL_USE_WINAPI_TIME
