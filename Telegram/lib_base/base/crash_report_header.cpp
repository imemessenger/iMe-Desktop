// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/crash_report_header.h"

namespace base::details {
namespace {

std::array<char, kReportHeaderSizeLimit> Bytes;
int Length = 0;

void SafeWriteChar(char ch) {
	if (Length < kReportHeaderSizeLimit) {
		Bytes[Length++] = ch;
	}
}

template <typename Type>
void SafeWriteNumber(Type number) {
	if constexpr (Type(-1) < Type(0)) {
		if (number < 0) {
			SafeWriteChar('-');
			number = -number;
		}
	}
	Type upper = 1, prev = number / 10;
	while (prev >= upper) {
		upper *= 10;
	}
	while (upper > 0) {
		int digit = (number / upper);
		SafeWriteChar('0' + digit);
		number -= digit * upper;
		upper /= 10;
	}
}

} // namespace

ReportHeaderWriter operator<<(ReportHeaderWriter, const char *str) {
	if (str) {
		while (const auto ch = *str++) {
			SafeWriteChar(ch);
		}
	}
	return ReportHeaderWriter();
}

ReportHeaderWriter operator<<(ReportHeaderWriter, const wchar_t *str) {
	for (int i = 0, l = wcslen(str); i < l; ++i) {
		if (
#if !defined(__WCHAR_UNSIGNED__)
			str[i] >= 0 &&
#endif
			str[i] < 128) {
			SafeWriteChar(char(str[i]));
		} else {
			SafeWriteChar('?');
		}
	}
	return ReportHeaderWriter();
}

ReportHeaderWriter operator<<(ReportHeaderWriter, int num) {
	SafeWriteNumber(num);
	return ReportHeaderWriter();
}

ReportHeaderWriter operator<<(ReportHeaderWriter, unsigned int num) {
	SafeWriteNumber(num);
	return ReportHeaderWriter();
}

ReportHeaderWriter operator<<(ReportHeaderWriter, unsigned long num) {
	SafeWriteNumber(num);
	return ReportHeaderWriter();
}

ReportHeaderWriter operator<<(ReportHeaderWriter, unsigned long long num) {
	SafeWriteNumber(num);
	return ReportHeaderWriter();
}

ReportHeaderWriter operator<<(ReportHeaderWriter, double num) {
	if (num < 0) {
		SafeWriteChar('-');
		num = -num;
	}
	SafeWriteNumber(uint64(floor(num)));
	SafeWriteChar('.');
	num -= floor(num);
	for (int i = 0; i < 4; ++i) {
		num *= 10;
		int digit = int(floor(num));
		SafeWriteChar('0' + digit);
		num -= digit;
	}
	return ReportHeaderWriter();
}

const char *ReportHeaderBytes() {
	return Bytes.data();
}

int ReportHeaderLength() {
	return Length;
}

} // namespace base::details
