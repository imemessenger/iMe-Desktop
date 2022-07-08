// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/random.h"

extern "C" {
#include <openssl/rand.h>
} // extern "C"

namespace base {

void RandomFill(bytes::span bytes) {
	const auto result = RAND_bytes(
		reinterpret_cast<unsigned char*>(bytes.data()),
		bytes.size());

	Ensures(result);
}

int RandomIndex(int count) {
	Expects(count > 0);

	if (count == 1) {
		return 0;
	}
	const auto max = (std::numeric_limits<uint32>::max() / count) * count;
	while (true) {
		const auto random = RandomValue<uint32>();
		if (random < max) {
			return int(random % count);
		}
	}
}

void RandomAddSeed(bytes::const_span bytes) {
	RAND_seed(bytes.data(), bytes.size());
}

} // namespace base
