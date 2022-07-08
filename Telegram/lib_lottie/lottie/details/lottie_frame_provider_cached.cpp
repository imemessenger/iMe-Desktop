// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/details/lottie_frame_provider_cached.h"

namespace Lottie {

FrameProviderCached::FrameProviderCached(
	const QByteArray &content,
	FnMut<void(QByteArray &&cached)> put,
	const QByteArray &cached,
	const FrameRequest &request,
	Quality quality,
	const ColorReplacements *replacements)
: _cache(cached, request, std::move(put))
, _direct(quality)
, _content(content)
, _replacements(replacements) {
	if (!_cache.framesCount()
		|| (_cache.framesReady() < _cache.framesCount())) {
		if (!_direct.load(content, replacements)) {
			return;
		}
	} else {
		_direct.setInformation({
			.size = _cache.originalSize(),
			.frameRate = _cache.frameRate(),
			.framesCount = _cache.framesCount(),
		});
	}
}

QImage FrameProviderCached::construct(
		std::unique_ptr<FrameProviderToken> &token,
		const FrameRequest &request) {
	auto cover = _cache.takeFirstFrame();
	using Token = FrameProviderCachedToken;
	const auto my = static_cast<Token*>(token.get());
	if (!my || my->exclusive) {
		if (!cover.isNull()) {
			if (my) {
				_cache.keepUpContext(my->context);
			}
			return cover;
		}
		const auto &info = information();
		_cache.init(
			info.size,
			info.frameRate,
			info.framesCount,
			request);
	}
	render(token, cover, request, 0);
	return cover;
}

const Information &FrameProviderCached::information() {
	return _direct.information();
}

bool FrameProviderCached::valid() {
	return _direct.valid();
}

int FrameProviderCached::sizeRounding() {
	return _cache.sizeRounding();
}

std::unique_ptr<FrameProviderToken> FrameProviderCached::createToken() {
	auto result = std::make_unique<FrameProviderCachedToken>();
	_cache.prepareBuffers(result->context);
	return result;
}

bool FrameProviderCached::render(
		const std::unique_ptr<FrameProviderToken> &token,
		QImage &to,
		const FrameRequest &request,
		int index) {
	if (!valid()) {
		if (token) {
			token->result = FrameRenderResult::Failed;
		}
		return false;
	}

	const auto original = information().size;
	const auto size = request.box.isEmpty()
		? original
		: request.size(original, sizeRounding());
	if (!GoodStorageForFrame(to, size)) {
		to = CreateFrameStorage(size);
	}
	using Token = FrameProviderCachedToken;
	const auto my = static_cast<Token*>(token.get());
	if (my && !my->exclusive) {
		// This must be a thread-safe request.
		my->result = _cache.renderFrame(my->context, to, request, index);
		return (my->result == FrameRenderResult::Ok);
	}
	const auto result = _cache.renderFrame(to, request, index);
	if (result == FrameRenderResult::Ok) {
		if (my) {
			_cache.keepUpContext(my->context);
		}
		return true;
	} else if (result == FrameRenderResult::Failed
		// We don't support changing size on the fly for shared providers.
		|| (result == FrameRenderResult::BadCacheSize && my)
		|| (!_direct.loaded() && !_direct.load(_content, _replacements))) {
		_direct.setInformation({});
		return false;
	}
	_direct.renderToPrepared(to, index);
	_cache.appendFrame(to, request, index);
	if (_cache.framesReady() == _cache.framesCount()) {
		_direct.unload();
	}
	if (my) {
		_cache.keepUpContext(my->context);
	}
	return true;
}

} // namespace Lottie
