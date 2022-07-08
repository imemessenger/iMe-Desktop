// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "codegen/emoji/data_old.h"

namespace codegen {
namespace emoji {

[[nodiscard]] InputId InputIdFromString(const QString &emoji);
[[nodiscard]] QString InputIdToString(const InputId &id);
[[nodiscard]] InputData ReadData(const QString &path);

} // namespace emoji
} // namespace codegen
