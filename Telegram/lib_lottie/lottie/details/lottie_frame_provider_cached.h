// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "lottie/details/lottie_frame_provider_direct.h"
#include "lottie/details/lottie_cache.h"

namespace Lottie {

struct FrameProviderCachedToken : FrameProviderToken {
	CacheReadContext context;
};

class FrameProviderCached final : public FrameProvider {
public:
	FrameProviderCached(
		const QByteArray &content,
		FnMut<void(QByteArray &&cached)> put,
		const QByteArray &cached,
		const FrameRequest &request,
		Quality quality,
		const ColorReplacements *replacements);

	QImage construct(
		std::unique_ptr<FrameProviderToken> &token,
		const FrameRequest &request) override;
	const Information &information() override;
	bool valid() override;

	int sizeRounding() override;

	std::unique_ptr<FrameProviderToken> createToken() override;

	bool render(
		const std::unique_ptr<FrameProviderToken> &token,
		QImage &to,
		const FrameRequest &request,
		int index) override;

private:
	Cache _cache;
	FrameProviderDirect _direct;
	const QByteArray _content;
	const ColorReplacements *_replacements = nullptr;

};

} // namespace Lottie
