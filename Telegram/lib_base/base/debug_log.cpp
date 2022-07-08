// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/debug_log.h"

#include "base/integration.h"

namespace base {

void LogWriteMain(const QString &message) {
	if (Integration::Exists()) {
		Integration::Instance().logMessage(message);
	}
}

void LogWriteDebug(const QString &message, const char *file, int line) {
	Expects(!LogSkipDebug());

	Integration::Instance().logMessageDebug(QString("%1 (%2 : %3)").arg(
		message,
		QString::fromUtf8(file),
		QString::number(__LINE__)));
}

bool LogSkipDebug() {
	return !Integration::Exists() || Integration::Instance().logSkipDebug();
}

QString LogProfilePrefix() {
	const auto now = crl::profile();
	return '[' + QString::number(now / 1000., 'f', 3) + "] ";
}

} // namespace base
