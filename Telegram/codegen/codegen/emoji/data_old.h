// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "codegen/common/logging.h"
#include <vector>

namespace codegen {
namespace emoji {

using uint16 = quint16;
using uint32 = quint32;
using uint64 = quint64;

using InputId = std::vector<uint32>;
using InputCategory = std::vector<InputId>;
struct DoubleColored {
	InputId original;
	InputId same;
	InputId different;
};
struct InputData {
	InputCategory colored;
	std::vector<DoubleColored> doubleColored;
	InputCategory categories[7];
	InputCategory other;
};

[[nodiscard]] InputData GetDataOld1();
[[nodiscard]] InputData GetDataOld2();

} // namespace emoji
} // namespace codegen
