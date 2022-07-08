// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "codegen/common/checked_utf8_string.h"

#include "codegen/common/const_utf8_string.h"

namespace codegen {
namespace common {

CheckedUtf8String::CheckedUtf8String(const char *string, int size) {
	if (size < 0) {
		size = strlen(string);
	}
	if (!size) { // Valid empty string
		return;
	}

	string_ = QString::fromUtf8(string, size);
	if (string_.contains(QChar::ReplacementCharacter)) {
		valid_ = false;
	}
}

CheckedUtf8String::CheckedUtf8String(const QByteArray &string) : CheckedUtf8String(string.constData(), string.size()) {
}

CheckedUtf8String::CheckedUtf8String(const ConstUtf8String &string) : CheckedUtf8String(string.data(), string.size()) {
}

} // namespace common
} // namespace codegen
