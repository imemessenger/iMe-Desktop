// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/base_file_utilities.h"

#include "base/platform/base_platform_file_utilities.h"

#include <QtCore/QResource>

namespace base {

QString FileNameFromUserString(QString name) {
	// We don't want LTR/RTL mark/embedding/override/isolate chars
	// in filenames, because they introduce a security issue, when
	// an executable "Fil[x]gepj.exe" may look like "Filexe.jpeg".
	const ushort kBad[] = {
		0x200E, // LTR Mark
		0x200F, // RTL Mark
		0x202A, // LTR Embedding
		0x202B, // RTL Embedding
		0x202D, // LTR Override
		0x202E, // RTL Override
		0x2066, // LTR Isolate
		0x2067, // RTL Isolate
		'/', '\\', '<', '>', ':', '"', '|', '?', '*' };
	for (auto &ch : name) {
		if (ch < 32 || ranges::find(kBad, ch.unicode()) != end(kBad)) {
			ch = '_';
		}
	}
	if (name.isEmpty() || name.endsWith(' ') || name.endsWith('.')) {
		name.append('_');
	}
	return Platform::FileNameFromUserString(std::move(name));
}

void RegisterBundledResources(const QString &name) {
	const auto location = Platform::BundledResourcesPath();
	if (!QResource::registerResource(location + '/' + name)) {
		Unexpected("Packed resources not found.");
	}
}

} // namespace base
