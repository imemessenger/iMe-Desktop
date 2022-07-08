// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

namespace base::details {

inline constexpr auto kReportHeaderSizeLimit = 64 * 1024;

struct ReportHeaderWriter {
};

ReportHeaderWriter operator<<(ReportHeaderWriter, const char *str);
ReportHeaderWriter operator<<(ReportHeaderWriter, const wchar_t *str);
ReportHeaderWriter operator<<(ReportHeaderWriter, int num);
ReportHeaderWriter operator<<(ReportHeaderWriter, unsigned int num);
ReportHeaderWriter operator<<(ReportHeaderWriter, unsigned long num);
ReportHeaderWriter operator<<(ReportHeaderWriter, unsigned long long num);
ReportHeaderWriter operator<<(ReportHeaderWriter, double num);

[[nodiscard]] const char *ReportHeaderBytes();
[[nodiscard]] int ReportHeaderLength();

} // namespace base::details
