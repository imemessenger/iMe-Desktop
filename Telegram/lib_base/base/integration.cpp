// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/assertion.h"

#include "base/integration.h"
#include "base/platform/base_platform_file_utilities.h"
#include "base/debug_log.h"

#include <QtCore/QFileInfo>
#include <QtCore/QDir>

namespace base {
namespace {

Integration *IntegrationInstance = nullptr;

} // namespace

void Integration::Set(not_null<Integration*> instance) {
	IntegrationInstance = instance;
}

Integration &Integration::Instance() {
	Expects(IntegrationInstance != nullptr);

	return *IntegrationInstance;
}

bool Integration::Exists() {
	return (IntegrationInstance != nullptr);
}

Integration::Integration(int argc, char *argv[]) {
	const auto path = Platform::CurrentExecutablePath(argc, argv);
	if (path.isEmpty()) {
		return;
	}
	auto info = QFileInfo(path);
	if (info.isSymLink()) {
		info = QFileInfo(info.symLinkTarget());
	}
	if (!info.exists()) {
		return;
	}
	const auto dir = info.absoluteDir().absolutePath();
	_executableDir = dir.endsWith('/') ? dir : (dir + '/');
	_executableName = info.fileName();
}

void Integration::logAssertionViolation(const QString &info) {
	logMessage("Assertion Failed! " + info);
}

QString Integration::executableDir() const {
	return _executableDir;
}

QString Integration::executableName() const {
	return _executableName;
}

QString Integration::executablePath() const {
	return _executableDir + _executableName;
}

} // namespace base
