// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#ifndef DESKTOP_APP_DISABLE_CRASH_REPORTS

#include "base/file_lock.h"

namespace base {

class CrashReportWriter final {
public:
	CrashReportWriter(const QString &path);
	~CrashReportWriter();

	void start();

	void addAnnotation(std::string key, std::string value);

private:
	[[nodiscard]] QString reportPath() const;
	[[nodiscard]] std::optional<QByteArray> readPreviousReport();
	bool openReport();
	void closeReport();
	void startCatching();
	void finishCatching();

	const QString _path;
	FileLock _reportLock;
	QFile _reportFile;
	std::optional<QByteArray> _previousReport;
	std::map<std::string, std::string> _annotations;

};

} // namespace base

#endif // DESKTOP_APP_DISABLE_CRASH_REPORTS