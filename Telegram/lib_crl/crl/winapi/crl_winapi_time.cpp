// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include <crl/crl_time.h>

#ifdef CRL_USE_WINAPI_TIME

#include <crl/winapi/crl_winapi_windows_h.h>

namespace crl::details {
namespace {

double Frequency/* = 0.*/;
double ProfileFrequency/* = 0.*/;

} // namespace

void init() {
	LARGE_INTEGER value;
	QueryPerformanceFrequency(&value);
	Frequency = 1000. / double(value.QuadPart);
	ProfileFrequency = 1000000. / double(value.QuadPart);
}

inner_time_type current_value() {
	LARGE_INTEGER value;
	QueryPerformanceCounter(&value);
	return value.QuadPart;
}

time convert(inner_time_type value) {
	return time(value * Frequency);
}

inner_profile_type current_profile_value() {
	LARGE_INTEGER value;
	QueryPerformanceCounter(&value);
	return value.QuadPart;
}

profile_time convert_profile(inner_profile_type value) {
	return profile_time(value * ProfileFrequency);
}

} // namespace crl::details

#endif // CRL_USE_WINAPI_TIME
