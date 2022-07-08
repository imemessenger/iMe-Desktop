// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <QtCore/QLibrary>

#define LOAD_LIBRARY_SYMBOL(lib, func) \
	::base::Platform::LoadSymbol(lib, #func, func)

namespace base {
namespace Platform {

bool LoadLibrary(
	QLibrary &lib,
	const char *name,
	std::optional<int> version = std::nullopt);

[[nodiscard]] QFunctionPointer LoadSymbolGeneric(
	QLibrary &lib,
	const char *name);

template <typename Function>
inline bool LoadSymbol(QLibrary &lib, const char *name, Function &func) {
	func = reinterpret_cast<Function>(LoadSymbolGeneric(lib, name));
	return (func != nullptr);
}

} // namespace Platform
} // namespace base
