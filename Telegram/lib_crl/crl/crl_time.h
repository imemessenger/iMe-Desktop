// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <crl/common/crl_common_config.h>

#include <cstdint>

// Use after main() started.

namespace crl {

using time = std::int64_t;
using profile_time = std::int64_t;

namespace details {

using inner_time_type = std::int64_t;
using inner_profile_type = std::int64_t;

void init();

inner_time_type current_value();
time convert(inner_time_type value);

inner_profile_type current_profile_value();
profile_time convert_profile(inner_profile_type);

} // namespace details

// Thread-safe.
time now();
profile_time profile();

// Returns true if some adjustment was made.
bool adjust_time();

} // namespace crl
