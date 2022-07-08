// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/assertion.h" // SOURCE_FILE_BASENAME

#include <QtCore/QString>

namespace base {

void LogWriteMain(const QString &message);
void LogWriteDebug(const QString &message, const char *file, int line);
[[nodiscard]] bool LogSkipDebug();

[[nodiscard]] QString LogProfilePrefix();

} // namespace base

#define LOG(message) (::base::LogWriteMain(QString message))
//usage LOG(("log: %1 %2").arg(1).arg(2))

#define PROFILE_LOG(message) {\
	if (!::base::LogSkipDebug()) {\
		::base::LogWriteMain(::base::LogProfilePrefix() + QString message);\
	}\
}
//usage PROFILE_LOG(("step: %1 %2").arg(1).arg(2))

#define DEBUG_LOG(message) {\
	if (!::base::LogSkipDebug()) {\
		::base::LogWriteDebug(QString message, SOURCE_FILE_BASENAME, __LINE__);\
	}\
}
//usage DEBUG_LOG(("log: %1 %2").arg(1).arg(2))
