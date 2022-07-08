// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/details/lottie_frame_provider_cached_multi.h"

#include "base/assertion.h"

#include <range/v3/numeric/accumulate.hpp>

namespace Lottie {

FrameProviderCachedMulti::FrameProviderCachedMulti(
	const QByteArray &content,
	FnMut<void(int index, QByteArray &&cached)> put,
	std::vector<QByteArray> caches,
	const FrameRequest &request,
	Quality quality,
	const ColorReplacements *replacements)
: _content(content)
, _replacements(replacements)
, _put(std::move(put))
, _direct(quality) {
	Expects(!caches.empty());

	_caches.reserve(caches.size());
	const auto emplace = [&](const QByteArray &cached) {
		const auto index = int(_caches.size());
		_caches.emplace_back(cached, request, [=](QByteArray &&v) {
			// We capture reference to _put, so the provider is not movable.
			_put(index, std::move(v));
		});
	};
	const auto load = [&] {
		if (_direct.loaded() || _direct.load(content, replacements)) {
			return true;
		}
		_caches.clear();
		return false;
	};
	const auto fill = [&] {
		if (!load()) {
			return false;
		}
		while (_caches.size() < caches.size()) {
			emplace({});
		}
		return true;
	};
	for (const auto &cached : caches) {
		emplace(cached);
		auto &cache = _caches.back();
		const auto &first = _caches.front();
		Assert(cache.sizeRounding() == first.sizeRounding());

		if (!cache.framesCount()) {
			if (!fill()) {
				return;
			}
			break;
		} else if (cache.framesReady() < cache.framesCount() && !load()) {
			return;
		} else if (cache.frameRate() != first.frameRate()
			|| cache.originalSize() != first.originalSize()) {
			_caches.pop_back();
			if (!fill()) {
				return;
			}
			break;
		}
	}
	if (!_direct.loaded()) {
		_direct.setInformation({
			.size = _caches.front().originalSize(),
			.frameRate = _caches.front().frameRate(),
			.framesCount = ranges::accumulate(
				_caches,
				0,
				std::plus<>(),
				&Cache::framesCount),
		});
	}
	if (!validateFramesPerCache() && _framesPerCache > 0) {
		fill();
	}
}

bool FrameProviderCachedMulti::validateFramesPerCache() {
	const auto &info = information();
	const auto count = int(_caches.size());
	_framesPerCache = (info.framesCount + count - 1) / count;
	if (!_framesPerCache
		|| (info.framesCount <= (count - 1) * _framesPerCache)) {
		_framesPerCache = 0;
		return false;
	}
	for (auto i = 0; i != count; ++i) {
		const auto cacheFramesCount = _caches[i].framesCount();
		if (!cacheFramesCount) {
			break;
		}
		const auto shouldBe = (i + 1 == count
			? (info.framesCount - (count - 1) * _framesPerCache)
			: _framesPerCache);
		if (cacheFramesCount != shouldBe) {
			_caches.erase(begin(_caches) + i, end(_caches));
			return false;
		}
	}
	return true;
}

QImage FrameProviderCachedMulti::construct(
		std::unique_ptr<FrameProviderToken> &token,
		const FrameRequest &request) {
	if (!_framesPerCache) {
		if (token) {
			token->result = FrameRenderResult::Failed;
		}
		return QImage();
	}
	auto cover = QImage();
	using Token = FrameProviderCachedMultiToken;
	const auto my = static_cast<Token*>(token.get());
	if (!my || my->exclusive) {
		const auto &info = information();
		const auto count = int(_caches.size());
		for (auto i = 0; i != count; ++i) {
			auto cacheCover = _caches[i].takeFirstFrame();
			if (cacheCover.isNull()) {
				_caches[i].init(
					info.size,
					info.frameRate,
					(i + 1 == count
						? (info.framesCount - (count - 1) * _framesPerCache)
						: _framesPerCache),
					request);
			} else if (!i) {
				cover = std::move(cacheCover);
			}
		}
		if (!cover.isNull()) {
			if (my) {
				_caches[0].keepUpContext(my->context);
			}
			return cover;
		}
	}
	render(token, cover, request, 0);
	return cover;
}

const Information &FrameProviderCachedMulti::information() {
	return _direct.information();
}

bool FrameProviderCachedMulti::valid() {
	return _direct.valid() && (_framesPerCache > 0);
}

int FrameProviderCachedMulti::sizeRounding() {
	return _caches.front().sizeRounding();
}

std::unique_ptr<FrameProviderToken> FrameProviderCachedMulti::createToken() {
	auto result = std::make_unique<FrameProviderCachedMultiToken>();
	if (!_caches.empty()) {
		_caches.front().prepareBuffers(result->context);
	}
	return result;
}

bool FrameProviderCachedMulti::render(
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
	const auto cacheIndex = index / _framesPerCache;
	const auto indexInCache = index % _framesPerCache;
	Assert(cacheIndex < _caches.size());
	auto &cache = _caches[cacheIndex];
	using Token = FrameProviderCachedMultiToken;
	const auto my = static_cast<Token*>(token.get());
	if (my && !my->exclusive) {
		// Many threads may get here simultaneously.
		my->result = cache.renderFrame(
			my->context,
			to,
			request,
			indexInCache);
		return (my->result == FrameRenderResult::Ok);
	}
	const auto result = cache.renderFrame(to, request, indexInCache);
	if (result == FrameRenderResult::Ok) {
		if (my) {
			cache.keepUpContext(my->context);
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
	cache.appendFrame(to, request, indexInCache);
	if (cache.framesReady() == cache.framesCount()
		&& cacheIndex + 1 == _caches.size()) {
		_direct.unload();
	}
	if (my) {
		cache.keepUpContext(my->context);
	}
	return true;
}

} // namespace Lottie
