// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <QtCore/QString>

class QByteArray;

namespace codegen {
namespace common {

class ConstUtf8String;

// Parses a char sequence to a QString using UTF-8 codec.
// You can check for invalid UTF-8 sequence by isValid() method.
class CheckedUtf8String {
public:
	CheckedUtf8String(const CheckedUtf8String &other) = default;
	CheckedUtf8String &operator=(const CheckedUtf8String &other) = default;

	explicit CheckedUtf8String(const char *string, int size = -1);
	explicit CheckedUtf8String(const QByteArray &string);
	explicit CheckedUtf8String(const ConstUtf8String &string);

	bool isValid() const {
		return valid_;
	}
	const QString &toString() const {
		return string_;
	}

private:
	QString string_;
	bool valid_ = true;

};

} // namespace common
} // namespace codegen
