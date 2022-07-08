// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

namespace Storage {

using CollectGoodFiles = Fn<void(FnMut<void(base::flat_set<QString>&&)>)>;

void ClearLegacyFiles(const QString &base, CollectGoodFiles filter);

namespace details {

std::vector<QString> CollectFiles(
	const QString &base,
	size_type limit,
	const base::flat_set<QString> &skip);

bool RemoveLegacyFile(const QString &path);

} // namespace details
} // namespace Storage
