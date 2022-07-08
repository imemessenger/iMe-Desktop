// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <QtGui/QGuiApplication>

namespace base {

[[nodiscard]] inline bool IsCtrlPressed() {
	return (QGuiApplication::keyboardModifiers() == Qt::ControlModifier);
}

[[nodiscard]] inline bool IsAltPressed() {
	return (QGuiApplication::keyboardModifiers() == Qt::AltModifier);
}

} // namespace base
