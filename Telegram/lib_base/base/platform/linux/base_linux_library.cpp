// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/linux/base_linux_library.h"

#include "base/debug_log.h"

namespace base {
namespace Platform {

bool LoadLibrary(
		QLibrary &lib,
		const char *name,
		std::optional<int> version) {
	const auto versionStr = version ? QString::number(*version) : "'nullopt'";
	DEBUG_LOG(("Loading '%1' with version %2...")
		.arg(QLatin1String(name))
		.arg(versionStr));
	if (version) {
		lib.setFileNameAndVersion(QLatin1String(name), *version);
	} else {
		lib.setFileName(QLatin1String(name));
	}
	if (lib.load()) {
		DEBUG_LOG(("Loaded '%1' with version %2!")
			.arg(QLatin1String(name))
			.arg(versionStr));
		return true;
	} else {
		DEBUG_LOG(("Could not load '%1' with version %2! Error: %3")
			.arg(QLatin1String(name))
			.arg(versionStr)
			.arg(lib.errorString()));
	}
	lib.setFileNameAndVersion(QLatin1String(name), QString());
	if (lib.load()) {
		DEBUG_LOG(("Loaded '%1' without version!").arg(QLatin1String(name)));
		return true;
	} else {
		LOG(("Could not load '%1' without version! Error: %2")
			.arg(QLatin1String(name))
			.arg(lib.errorString()));
	}
	return false;
}

QFunctionPointer LoadSymbolGeneric(QLibrary &lib, const char *name) {
	if (!lib.isLoaded()) {
		return nullptr;
	} else if (const auto result = lib.resolve(name)) {
		return result;
	}
	LOG(("Error: failed to load '%1' function!").arg(name));
	return nullptr;
}

} // namespace Platform
} // namespace base
