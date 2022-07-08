// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "storage/cache/storage_cache_types.h"
#include <crl/crl_object_on_queue.h>
#include <base/binary_guard.h>

namespace Storage {
class EncryptionKey;
namespace Cache {
namespace details {

class CompactorObject;
class DatabaseObject;

class Compactor {
public:
	struct Info {
		int64 till = 0;
		uint32 systemTime = 0;
		size_type keysCount = 0;
	};

	Compactor(
		crl::weak_on_queue<DatabaseObject> database,
		base::binary_guard guard,
		const QString &base,
		const Settings &settings,
		EncryptionKey &&key,
		const Info &info);

	~Compactor();

private:
	using Implementation = CompactorObject;
	crl::object_on_queue<Implementation> _wrapped;

};

int64 CatchUp(
	const QString &compactPath,
	const QString &binlogPath,
	const EncryptionKey &key,
	int64 from,
	size_type block);

} // namespace details
} // namespace Cache
} // namespace Storage
