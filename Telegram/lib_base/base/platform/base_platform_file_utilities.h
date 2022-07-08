// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

class QString;
class QFile;

namespace base::Platform {

bool ShowInFolder(const QString &filepath);
[[nodiscard]] QString FileNameFromUserString(QString name);

bool DeleteDirectory(QString path);
void RemoveQuarantine(const QString &path);

[[nodiscard]] QString CurrentExecutablePath(int argc, char *argv[]);
[[nodiscard]] QString BundledResourcesPath();

bool RenameWithOverwrite(const QString &from, const QString &to);
void FlushFileData(QFile &file);

} // namespace base::Platform

#ifdef Q_OS_MAC
#include "base/platform/mac/base_file_utilities_mac.h"
#elif defined Q_OS_UNIX // Q_OS_MAC
#include "base/platform/linux/base_file_utilities_linux.h"
#elif defined Q_OS_WINRT || defined Q_OS_WIN // Q_OS_MAC || Q_OS_UNIX
#include "base/platform/win/base_file_utilities_win.h"
#endif // Q_OS_MAC || Q_OS_UNIX || Q_OS_WINRT || Q_OS_WIN
