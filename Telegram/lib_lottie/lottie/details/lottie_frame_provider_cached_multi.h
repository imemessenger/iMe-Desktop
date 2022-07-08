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

struct FrameProviderCachedMultiToken : FrameProviderToken {
	CacheReadContext context;
};

class FrameProviderCachedMulti final : public FrameProvider {
public:
	FrameProviderCachedMulti(
		const QByteArray &content,
		FnMut<void(int index, QByteArray &&cached)> put,
		std::vector<QByteArray> caches,
		const FrameRequest &request,
		Quality quality,
		const ColorReplacements *replacements);

	FrameProviderCachedMulti(const FrameProviderCachedMulti &) = delete;
	FrameProviderCachedMulti &operator=(const FrameProviderCachedMulti &)
		= delete;

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
	bool validateFramesPerCache();

	const QByteArray _content;
	const ColorReplacements *_replacements = nullptr;
	FnMut<void(int index, QByteArray &&cached)> _put;
	FrameProviderDirect _direct;
	std::vector<Cache> _caches;
	int _framesPerCache = 0;

};

} // namespace Lottie
