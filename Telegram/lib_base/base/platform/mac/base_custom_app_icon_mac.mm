// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/mac/base_custom_app_icon_mac.h"

#include "base/debug_log.h"
#include "base/platform/mac/base_utilities_mac.h"

#include <QtGui/QImage>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryFile>

#include <sys/xattr.h>
#include <xxhash.h>

namespace base::Platform {
namespace {

using namespace ::Platform;

constexpr auto kFinderInfo = "com.apple.FinderInfo";
constexpr auto kResourceFork = "com.apple.ResourceFork";

// We want to write [8], so just in case.
constexpr auto kFinderInfoMinSize = 16;

// Usually.
constexpr auto kFinderInfoSize = 32;

// Just in case.
constexpr auto kFinderInfoMaxSize = 256;

// Limit custom icons to 10 MB.
constexpr auto kResourceForkMaxSize = 10 * 1024 * 1024;

[[nodiscard]] QString BundlePath() {
	@autoreleasepool {

	NSString *path = @"";
	@try {
		path = [[NSBundle mainBundle] bundlePath];
		if (!path) {
			Unexpected("Could not get bundled path!");
		}
		return QFile::decodeName([path fileSystemRepresentation]);
	}
	@catch (NSException *exception) {
		Unexpected("Exception in resource registering.");
	}

	}
}

int Launch(const QString &command, const QStringList &arguments) {
	@autoreleasepool {

	@try {

	NSMutableArray *list = [NSMutableArray arrayWithCapacity:arguments.size()];
	for (const auto &argument : arguments) {
		[list addObject:Q2NSString(argument)];
	}

	NSTask *task = [[NSTask alloc] init];
	task.launchPath = Q2NSString(command);
	task.arguments = list;

	[task launch];
	[task waitUntilExit];

	return [task terminationStatus];

	}
	@catch (NSException *exception) {
		return -888;
	}
	@finally {
	}

	}
}

[[nodiscard]] std::optional<std::string> ReadCustomIconAttribute(const QString &bundle) {
	const auto native = QFile::encodeName(bundle);
	auto info = std::array<char, kFinderInfoMaxSize>();
	const auto result = getxattr(
		native.data(),
		kFinderInfo,
		info.data(),
		info.size(),
		0, // position
		XATTR_NOFOLLOW);
	const auto error = (result < 0) ? errno : 0;
	if (result < 0) {
		if (error == ENOATTR) {
			return std::string();
		} else {
			LOG(("Icon Error: Could not get %1 xattr, error: %2."
				).arg(kFinderInfo
				).arg(error));
			return std::nullopt;
		}
	} else if (result < kFinderInfoMinSize) {
		LOG(("Icon Error: Bad existing %1 xattr size: %2."
			).arg(kFinderInfo
			).arg(error));
		return std::nullopt;
	}
	return std::string(info.data(), result);
}

[[nodiscard]] bool WriteCustomIconAttribute(
		const QString &bundle,
		const std::string &value) {
	const auto native = QFile::encodeName(bundle);
	const auto result = setxattr(
		native.data(),
		kFinderInfo,
		value.data(),
		value.size(),
		0, // position
		XATTR_NOFOLLOW);
	if (result != 0) {
		LOG(("Icon Error: Could not set %1 xattr, error: %2."
			).arg(kFinderInfo
			).arg(errno));
		return false;
	}
	return true;
}

[[nodiscard]] bool DeleteCustomIconAttribute(const QString &bundle) {
	const auto native = QFile::encodeName(bundle);
	const auto result = removexattr(
		native.data(),
		kFinderInfo,
		XATTR_NOFOLLOW);
	if (result != 0) {
		LOG(("Icon Error: Could not remove %1 xattr, error: %2."
			).arg(kFinderInfo
			).arg(errno));
		return false;
	}
	return true;
}

[[nodiscard]] bool EnableCustomIcon(const QString &bundle) {
	auto info = ReadCustomIconAttribute(bundle);
	if (!info.has_value()) {
		return false;
	} else if (info->empty()) {
		*info = std::string(kFinderInfoSize, char(0));
	}
	if ((*info)[8] & 0x04) {
		(*info)[8] &= ~0x04;
		if (!WriteCustomIconAttribute(bundle, *info)) {
			return false;
		}
	}
	(*info)[8] |= 4;
	return WriteCustomIconAttribute(bundle, *info);
}

[[nodiscard]] bool DisableCustomIcon(const QString &bundle) {
	auto info = ReadCustomIconAttribute(bundle);
	if (!info.has_value()) {
		return false;
	} else if (info->empty()) {
		return true;
	}
	return DeleteCustomIconAttribute(bundle);
}

[[nodiscard]] bool RefreshDock() {
	Launch("/bin/bash", { "-c", "rm /var/folders/*/*/*/com.apple.dock.iconcache" });

	const auto killall = Launch("/usr/bin/killall", { "Dock" });
	if (killall != 0) {
		LOG(("Icon Error: Failed to run `killall Dock`, result: %1.").arg(killall));
		return false;
	}
	return true;
}

[[nodiscard]] QString TempPath(const QString &extension) {
	auto file = QTemporaryFile(
		QDir::tempPath() + "/custom_icon_XXXXXX." + extension);
	file.setAutoRemove(false);
	const auto result = file.open() ? file.fileName() : QString();
	if (result.isEmpty()) {
		LOG(("Icon Error: Could not obtain a temporary file name."));
	}
	return result;
}

[[nodiscard]] std::optional<std::string> ReadResourceFork(
		const QString &path) {
	const auto native = QFile::encodeName(path);
	auto buffer = std::string(kResourceForkMaxSize + 1, char(0));
	const auto result = getxattr(
		native.data(),
		kResourceFork,
		buffer.data(),
		buffer.size(),
		0, // position
		XATTR_NOFOLLOW);
	const auto error = (result < 0) ? errno : 0;
	if (result < 0) {
		if (error == ENOATTR) {
			return std::string();
		} else {
			LOG(("Icon Error: Could not get %1 xattr, error: %2."
				).arg(kResourceFork
				).arg(error));
			return std::nullopt;
		}
	} else if (result > kResourceForkMaxSize) {
		LOG(("Icon Error: Got too large %1 xattr, size: %2."
			).arg(kResourceFork
			).arg(result));
		return std::nullopt;
	}
	buffer.resize(result);
	return buffer;
}

[[nodiscard]] bool WriteResourceFork(
		const QString &path,
		const std::string &data) {
	const auto native = QFile::encodeName(path);
	const auto result = setxattr(
		native.data(),
		kResourceFork,
		data.data(),
		data.size(),
		0, // position
		XATTR_NOFOLLOW);
	if (result != 0) {
		LOG(("Icon Error: Could not set %1 xattr, error: %2."
			).arg(kResourceFork
			).arg(errno));
		return false;
	}
	return true;
}

[[nodiscard]] uint64 Digest(const std::string &data) {
	return XXH64(data.data(), data.size(), 0);
}

[[nodiscard]] std::optional<uint64> SetPreparedIcon(const QString &path) {
	const auto sips = Launch("/usr/bin/sips", {
		"-i",
		path
	});
	if (sips != 0) {
		LOG(("Icon Error: Failed to run `sips -i \"%1\"`, result: %2."
			).arg(path
			).arg(sips));
		return std::nullopt;
	}
	const auto bundle = BundlePath();
	const auto icon = bundle + "/Icon\r";
	const auto touch = Launch("/usr/bin/touch", { icon });
	if (touch != 0) {
		LOG(("Icon Error: Failed to run `touch \"%1\"`, result: %2."
			).arg(icon
			).arg(touch));
		return std::nullopt;
	}
#if 0 // Faster, but without a digest.
	const auto from = path + "/..namedfork/rsrc";
	const auto to = icon + "/..namedfork/rsrc";
	const auto cp = Launch("/bin/cp", { from, to });
	if (cp != 0) {
		LOG(("Icon Error: Failed to run `cp \"%1\" \"%2\"`, result: %3."
			).arg(from
			).arg(to
			).arg(cp));
		return false;
	}
#endif
	auto rsrc = ReadResourceFork(path);
	if (!rsrc) {
		return false;
	} else if (rsrc->empty()) {
		LOG(("Icon Error: Empty resource fork after sips in \"%1\".").arg(path));
		return false;
	} else if (!WriteResourceFork(icon, *rsrc) || !EnableCustomIcon(bundle)) {
		return std::nullopt;
	}
	return RefreshDock()
		? std::make_optional(Digest(*rsrc))
		: std::nullopt;
}

} // namespace

std::optional<uint64> SetCustomAppIcon(QImage image) {
	if (image.isNull()) {
		LOG(("Icon Error: Null image received."));
		return std::nullopt;
	}
	if (image.format() != QImage::Format_ARGB32_Premultiplied
		&& image.format() != QImage::Format_ARGB32
		&& image.format() != QImage::Format_RGB32) {
		image = std::move(image).convertToFormat(QImage::Format_ARGB32);
		if (image.isNull()) {
			LOG(("Icon Error: Failed to convert image to ARGB32."));
			return std::nullopt;
		}
	}
	const auto temp = TempPath("icns");
	if (temp.isEmpty()) {
		return std::nullopt;
	}
	const auto guard = gsl::finally([&] { QFile::remove(temp); });
	if (!image.save(temp, "PNG")) {
		LOG(("Icon Error: Failed to save image to \"%1\".").arg(temp));
		return std::nullopt;
	}
	return SetPreparedIcon(temp);
}

std::optional<uint64> SetCustomAppIcon(const QString &path) {
	const auto icns = path.endsWith(".icns", Qt::CaseInsensitive);
	if (!icns) {
		auto image = QImage(path);
		if (image.isNull()) {
			LOG(("Icon Error: Failed to read image from \"%1\".").arg(path));
			return std::nullopt;
		}
		return SetCustomAppIcon(std::move(image));
	}
	const auto temp = TempPath("icns");
	if (temp.isEmpty()) {
		return std::nullopt;
	}
	const auto guard = gsl::finally([&] { QFile::remove(temp); });
	QFile::remove(temp);
	if (!QFile(path).copy(temp)) {
		LOG(("Icon Error: Failed to copy icon from \"%1\" to \"%2\"."
			).arg(path
			).arg(temp));
		return std::nullopt;
	}
	return SetPreparedIcon(temp);
}

std::optional<uint64> CurrentCustomAppIconDigest() {
	const auto bundle = BundlePath();
	const auto icon = bundle + "/Icon\r";
	const auto attr = ReadCustomIconAttribute(bundle);
	if (!attr) {
		return std::nullopt;
	} else if (attr->empty()) {
		return 0;
	}
	const auto value = ReadResourceFork(icon);
	if (!value) {
		return std::nullopt;
	} else if (value->empty()) {
		return 0;
	}
	return Digest(*value);
}

bool ClearCustomAppIcon() {
	const auto bundle = BundlePath();
	const auto icon = bundle + "/Icon\r";
	Launch("/bin/rm", { icon });
	auto info = ReadCustomIconAttribute(bundle);
	if (!info.has_value()) {
		return false;
	} else if (info->empty()) {
		return true;
	} else if (!DeleteCustomIconAttribute(bundle)) {
		return false;
	}
	return RefreshDock();
}

} // namespace base::Platform
