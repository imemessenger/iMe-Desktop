// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/basic_types.h"

class QDateTime;

namespace base {
namespace unixtime {

// All functions are thread-safe.

[[nodiscard]] TimeId now();
void update(TimeId now, bool force = false);
[[nodiscard]] rpl::producer<> updates(); // main thread

[[nodiscard]] QDateTime parse(TimeId value);
[[nodiscard]] TimeId serialize(const QDateTime &date);

[[nodiscard]] bool http_valid();
[[nodiscard]] TimeId http_now();
void http_update(TimeId now);
void http_invalidate();

[[nodiscard]] uint64 mtproto_msg_id();

} // namespace unixtime
} // namespace base
