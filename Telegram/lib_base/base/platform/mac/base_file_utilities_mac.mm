// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/mac/base_file_utilities_mac.h"

#include "base/platform/mac/base_utilities_mac.h"

#include <QtCore/QFileInfo>
#include <sys/xattr.h>
#include <stdio.h>
#include <unistd.h>

namespace base::Platform {

using namespace ::Platform;

bool ShowInFolder(const QString &filepath) {
	const auto folder = QFileInfo(filepath).absolutePath();
	BOOL result = NO;

	@autoreleasepool {

	result = [[NSWorkspace sharedWorkspace] selectFile:Q2NSString(filepath) inFileViewerRootedAtPath:Q2NSString(folder)];

	}

	return (result != NO);
}

void RemoveQuarantine(const QString &path) {
	const auto kQuarantineAttribute = "com.apple.quarantine";

	const auto local = QFile::encodeName(path);
	removexattr(local.data(), kQuarantineAttribute, 0);
}

QString BundledResourcesPath() {
	@autoreleasepool {

	NSString *path = @"";
	@try {
		path = [[NSBundle mainBundle] bundlePath];
		if (!path) {
			Unexpected("Could not get bundled path!");
		}
		path = [path stringByAppendingString:@"/Contents/Resources"];
		return QFile::decodeName([path fileSystemRepresentation]);
	}
	@catch (NSException *exception) {
		Unexpected("Exception in resource registering.");
	}

	}
}

bool DeleteDirectory(QString path) {
	if (path.endsWith('/')) {
		path.chop(1);
	}

	BOOL result = NO;

	@autoreleasepool {

	result = [[NSFileManager defaultManager] removeItemAtPath:Q2NSString(path) error:nil];

	}

	return (result != NO);
}

QString CurrentExecutablePath(int argc, char *argv[]) {
	return NS2QString([[NSBundle mainBundle] bundlePath]);
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
