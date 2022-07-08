// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "tl/tl_basic_types.h"

namespace tl {

QString utf16(const QByteArray &v) {
	return QString::fromUtf8(v);
}

} // namespace tl
