// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/global_shortcuts_generic.h"

namespace base::Platform::GlobalShortcuts {

[[nodiscard]] bool Available();
[[nodiscard]] bool Allowed();

void Start(Fn<void(GlobalShortcutKeyGeneric descriptor, bool down)> process);
void Stop();

[[nodiscard]] QString KeyName(GlobalShortcutKeyGeneric descriptor);

} // namespace base::Platform::GlobalShortcuts
