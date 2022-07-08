// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include <crl/crl_time.h>

#ifdef CRL_USE_WINAPI_TIME

#ifdef CRL_THROW_FP_EXCEPTIONS

#include <float.h>
#pragma fenv_access (on)

namespace crl {

void toggle_fp_exceptions(bool throwing) {
	// Allow throwing (and reporting) floating point exceptions.
	//
	// Otherwise x86 build behaves unpredictably on old hardware,
	// after an fp-error it may fail some benign operations, like
	// std::round(1.) giving 'nan' or double -> int64 giving int64_min.
	//
	// This results in unexpected assertion violations.
	auto state = (unsigned int)0;

    // Right now catch only division by zero and invalid operations.
    const auto bits = (throwing ? 0 : (_EM_ZERODIVIDE | _EM_INVALID))
        | _EM_DENORMAL | _EM_INEXACT | _EM_UNDERFLOW | _EM_OVERFLOW;
	_controlfp_s(&state, bits, _MCW_EM);
}

} // namespace crl

#else // CRL_THROW_FP_EXCEPTIONS

namespace crl {

void toggle_fp_exceptions(bool throwing) {
}

} // namespace crl

#endif // CRL_THROW_FP_EXCEPTIONS

#endif // CRL_USE_WINAPI_TIME
