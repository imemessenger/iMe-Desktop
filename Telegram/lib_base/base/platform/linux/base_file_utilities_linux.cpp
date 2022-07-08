// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/linux/base_file_utilities_linux.h"

#include "base/platform/base_platform_file_utilities.h"
#include "base/algorithm.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>
#include <QtGui/QDesktopServices>

#ifndef DESKTOP_APP_DISABLE_DBUS_INTEGRATION
#include <glibmm.h>
#include <giomm.h>
#endif // !DESKTOP_APP_DISABLE_DBUS_INTEGRATION

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <fcntl.h>

namespace base::Platform {
namespace {

#ifndef DESKTOP_APP_DISABLE_DBUS_INTEGRATION
bool PortalShowInFolder(const QString &filepath) {
	try {
		const auto connection = Gio::DBus::Connection::get_sync(
			Gio::DBus::BusType::BUS_TYPE_SESSION);

		const auto fd = open(
			QFile::encodeName(filepath).constData(),
			O_RDONLY);

		if (fd == -1) {
			return false;
		}

		const auto guard = gsl::finally([&] { close(fd); });

		const auto fdList = Gio::UnixFDList::create();
		fdList->append(fd);
		auto outFdList = Glib::RefPtr<Gio::UnixFDList>();

		connection->call_sync(
			"/org/freedesktop/portal/desktop",
			"org.freedesktop.portal.OpenURI",
			"OpenDirectory",
			Glib::VariantContainerBase::create_tuple({
				Glib::Variant<Glib::ustring>::create({}),
				Glib::wrap(g_variant_new_handle(0)),
				Glib::Variant<
					std::map<Glib::ustring, Glib::VariantBase>>::create({}),
			}),
			fdList,
			outFdList,
			"org.freedesktop.portal.Desktop");

		return true;
	} catch (...) {
	}

	return false;
}

bool DBusShowInFolder(const QString &filepath) {
	try {
		const auto connection = Gio::DBus::Connection::get_sync(
			Gio::DBus::BusType::BUS_TYPE_SESSION);

		connection->call_sync(
			"/org/freedesktop/FileManager1",
			"org.freedesktop.FileManager1",
			"ShowItems",
			Glib::VariantContainerBase::create_tuple({
				Glib::Variant<std::vector<Glib::ustring>>::create({
					Glib::filename_to_uri(filepath.toStdString())
				}),
				Glib::Variant<Glib::ustring>::create({}),
			}),
			"org.freedesktop.FileManager1");

		return true;
	} catch (...) {
	}

	return false;
}
#endif // !DESKTOP_APP_DISABLE_DBUS_INTEGRATION

} // namespace

bool ShowInFolder(const QString &filepath) {
#ifndef DESKTOP_APP_DISABLE_DBUS_INTEGRATION
	if (DBusShowInFolder(filepath)) {
		return true;
	}

	if (PortalShowInFolder(filepath)) {
		return true;
	}

	try {
		if (Gio::AppInfo::launch_default_for_uri(
			Glib::filename_to_uri(
				Glib::path_get_dirname(filepath.toStdString())))) {
			return true;
		}
	} catch (...) {
	}
#endif // !DESKTOP_APP_DISABLE_DBUS_INTEGRATION

	const auto folder = QUrl::fromLocalFile(
		QFileInfo(filepath).absolutePath());

	if (QDesktopServices::openUrl(folder)) {
		return true;
	}

	return false;
}

QString CurrentExecutablePath(int argc, char *argv[]) {
	if (qEnvironmentVariableIsSet("APPIMAGE")) {
		const auto appimagePath = qEnvironmentVariable("APPIMAGE");
		const auto appimagePathList = appimagePath.split('/');

		if (qEnvironmentVariableIsSet("ARGV0")
			&& appimagePathList.size() >= 5
			&& appimagePathList[1] == qstr("run")
			&& appimagePathList[2] == qstr("user")
			&& appimagePathList[4] == qstr("appimagelauncherfs")) {
			return qEnvironmentVariable("ARGV0");
		}

		return appimagePath;
	}

	const auto exeLink = QFileInfo(u"/proc/%1/exe"_q.arg(getpid()));
	if (exeLink.exists() && exeLink.isSymLink()) {
		return exeLink.canonicalFilePath();
	}

	// Fallback to the first command line argument.
	if (argc) {
		const auto argv0 = QFile::decodeName(argv[0]);
		if (!argv0.isEmpty() && !QFileInfo::exists(argv0)) {
			const auto argv0InPath = QStandardPaths::findExecutable(argv0);
			if (!argv0InPath.isEmpty()) {
				return argv0InPath;
			}
		}
		return argv0;
	}

	return QString();
}

void RemoveQuarantine(const QString &path) {
}

QString BundledResourcesPath() {
	Unexpected("BundledResourcesPath not implemented.");
}

// From http://stackoverflow.com/questions/2256945/removing-a-non-empty-directory-programmatically-in-c-or-c
bool DeleteDirectory(QString path) {
	if (path.endsWith('/')) {
		path.chop(1);
	}
	const auto pathRaw = QFile::encodeName(path);
	const auto d = opendir(pathRaw.constData());
	if (!d) {
		return false;
	}

	while (struct dirent *p = readdir(d)) {
		// Skip the names "." and ".." as we don't want to recurse on them.
		if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
			continue;
		}

		const auto fname = path + '/' + p->d_name;
		const auto encoded = QFile::encodeName(fname);
		struct stat statbuf;
		if (!stat(encoded.constData(), &statbuf)) {
			if (S_ISDIR(statbuf.st_mode)) {
				if (!DeleteDirectory(fname)) {
					closedir(d);
					return false;
				}
			} else {
				if (unlink(encoded.constData())) {
					closedir(d);
					return false;
				}
			}
		}
	}
	closedir(d);

	return !rmdir(pathRaw.constData());
}

bool RenameWithOverwrite(const QString &from, const QString &to) {
	const auto fromPath = QFile::encodeName(from);
	const auto toPath = QFile::encodeName(to);
	return (rename(fromPath.constData(), toPath.constData()) == 0);
}

void FlushFileData(QFile &file) {
	file.flush();
	if (const auto descriptor = file.handle()) {
		fsync(descriptor);
	}
}

} // namespace base::Platform
