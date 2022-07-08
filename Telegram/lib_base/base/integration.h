// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/basic_types.h"

namespace base {

class Integration {
public:
	static void Set(not_null<Integration*> instance);
	static Integration &Instance();
	static bool Exists();

	Integration(int argc, char *argv[]);

	virtual void enterFromEventLoop(FnMut<void()> &&method) = 0;

	virtual bool logSkipDebug() = 0;
	virtual void logMessageDebug(const QString &message) = 0;
	virtual void logMessage(const QString &message) = 0;
	virtual void logAssertionViolation(const QString &info);

	[[nodiscard]] QString executableDir() const;
	[[nodiscard]] QString executableName() const;
	[[nodiscard]] QString executablePath() const;

	virtual ~Integration() = default;

private:
	QString _executableDir;
	QString _executableName;

};

} // namespace base
