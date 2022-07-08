// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "lottie/lottie_common.h"

namespace Lottie {

struct FrameProviderToken {
	virtual ~FrameProviderToken() = default;

	FrameRenderResult result = FrameRenderResult::Ok;
	bool exclusive = false;
};

class FrameProvider {
public:
	virtual ~FrameProvider() = default;

	virtual QImage construct(
		std::unique_ptr<FrameProviderToken> &token,
		const FrameRequest &request) = 0;
	[[nodiscard]] virtual const Information &information() = 0;
	[[nodiscard]] virtual bool valid() = 0;

	[[nodiscard]] virtual int sizeRounding() = 0;

	[[nodiscard]] virtual std::unique_ptr<FrameProviderToken> createToken() {
		// Used for shared frame provider.
		return nullptr;
	}

	virtual bool render(
		const std::unique_ptr<FrameProviderToken> &token,
		QImage &to,
		const FrameRequest &request,
		int index) = 0;

};

} // namespace Lottie
