// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/lottie_animation.h"

#include "lottie/details/lottie_frame_renderer.h"
#include "lottie/details/lottie_frame_provider_direct.h"
#include "lottie/lottie_player.h"
#include "ui/image/image_prepare.h"
#include "base/algorithm.h"
#include "base/assertion.h"
#include "base/variant.h"

#ifdef LOTTIE_USE_CACHE
#include "lottie/details/lottie_frame_provider_cached.h"
#include "lottie/details/lottie_frame_provider_cached_multi.h"
#endif // LOTTIE_USE_CACHE

#include <QFile>
#include <rlottie.h>
#include <crl/crl_async.h>
#include <crl/crl_on_main.h>

namespace Lottie {
namespace {

const auto kIdealSize = QSize(512, 512);

details::InitData CheckSharedState(std::unique_ptr<SharedState> state) {
	Expects(state != nullptr);

	auto information = state->information();
	if (!information.frameRate
		|| information.framesCount <= 0
		|| information.size.isEmpty()) {
		return Error::NotSupported;
	}
	return state;
}

details::InitData Init(
		const QByteArray &content,
		const FrameRequest &request,
		Quality quality,
		const ColorReplacements *replacements) {
	if (const auto error = ContentError(content)) {
		return *error;
	}
	auto provider = std::make_shared<FrameProviderDirect>(quality);
	if (!provider->load(content, replacements)) {
		return Error::ParseFailed;
	}
	return CheckSharedState(std::make_unique<SharedState>(
		std::move(provider),
		request.empty() ? FrameRequest{ kIdealSize } : request));
}

#ifdef LOTTIE_USE_CACHE
details::InitData Init(
		const QByteArray &content,
		FnMut<void(QByteArray &&cached)> put,
		const QByteArray &cached,
		const FrameRequest &request,
		Quality quality,
		const ColorReplacements *replacements) {
	Expects(!request.empty());

	if (const auto error = ContentError(content)) {
		return *error;
	}
	auto provider = std::make_shared<FrameProviderCached>(
		content,
		std::move(put),
		cached,
		request,
		quality,
		replacements);
	return provider->valid()
		? CheckSharedState(std::make_unique<SharedState>(
			std::move(provider),
			request.empty() ? FrameRequest{ kIdealSize } : request))
		: Error::ParseFailed;
}

details::InitData Init(
		const QByteArray &content,
		FnMut<void(int, QByteArray &&cached)> put,
		std::vector<QByteArray> caches,
		const FrameRequest &request,
		Quality quality,
		const ColorReplacements *replacements) {
	Expects(!request.empty());

	if (const auto error = ContentError(content)) {
		return *error;
	}
	auto provider = std::make_shared<FrameProviderCachedMulti>(
		content,
		std::move(put),
		std::move(caches),
		request,
		quality,
		replacements);
	return provider->valid()
		? CheckSharedState(std::make_unique<SharedState>(
			std::move(provider),
			request.empty() ? FrameRequest{ kIdealSize } : request))
		: Error::ParseFailed;
}
#endif // LOTTIE_USE_CACHE

details::InitData Init(
		std::shared_ptr<FrameProvider> provider,
		const FrameRequest &request) {
	Expects(!request.empty());

	return provider->valid()
		? CheckSharedState(std::make_unique<SharedState>(
			std::move(provider),
			request.empty() ? FrameRequest{ kIdealSize } : request))
		: Error::ParseFailed;
}

} // namespace

std::shared_ptr<FrameRenderer> MakeFrameRenderer() {
	return FrameRenderer::CreateIndependent();
}

QImage ReadThumbnail(const QByteArray &content) {
	return v::match(Init(content, FrameRequest(), Quality::High, nullptr), [](
			const std::unique_ptr<SharedState> &state) {
		return state->frameForPaint()->original;
	}, [](Error) {
		return QImage();
	});
}

Animation::Animation(
	not_null<Player*> player,
	const QByteArray &content,
	const FrameRequest &request,
	Quality quality,
	const ColorReplacements *replacements)
: _player(player) {
	if (quality == Quality::Synchronous) {
		initDone(Init(content, request, quality, replacements));
	} else {
		const auto weak = base::make_weak(this);
		crl::async([=] {
			auto result = Init(content, request, quality, replacements);
			crl::on_main(weak, [=, data = std::move(result)]() mutable {
				initDone(std::move(data));
			});
		});
	}
}

Animation::Animation(
	not_null<Player*> player,
	FnMut<void(FnMut<void(QByteArray &&cached)>)> get, // Main thread.
	FnMut<void(QByteArray &&cached)> put, // Unknown thread.
	const QByteArray &content,
	const FrameRequest &request,
	Quality quality,
	const ColorReplacements *replacements)
#ifdef LOTTIE_USE_CACHE
: _player(player) {
	const auto weak = base::make_weak(this);
	get([=, put = std::move(put)](QByteArray &&cached) mutable {
		crl::async([=, put = std::move(put)]() mutable {
			auto result = Init(
				content,
				std::move(put),
				cached,
				request,
				quality,
				replacements);
			crl::on_main(weak, [=, data = std::move(result)]() mutable {
				initDone(std::move(data));
			});
		});
	});
#else // LOTTIE_USE_CACHE
: Animation(player, content, request, quality, replacements) {
#endif // LOTTIE_USE_CACHE
}

Animation::Animation(
	not_null<Player*> player,
	int keysCount,
	FnMut<void(int, FnMut<void(QByteArray &&)>)> get,
	FnMut<void(int, QByteArray &&cached)> put,
	const QByteArray &content,
	const FrameRequest &request,
	Quality quality,
	const ColorReplacements *replacements)
#ifdef LOTTIE_USE_CACHE
: _player(player) {
	const auto weak = base::make_weak(this);
	struct State {
		std::atomic<int> left = 0;
		std::vector<QByteArray> caches;
		FnMut<void(int, QByteArray &&cached)> put;
	};
	const auto state = std::make_shared<State>();
	state->left = keysCount;
	state->put = std::move(put);
	state->caches.resize(keysCount);
	for (auto i = 0; i != keysCount; ++i) {
		get(i, [=](QByteArray &&cached) {
			state->caches[i] = std::move(cached);
			if (--state->left) {
				return;
			}
			crl::async([=] {
				auto result = Init(
					content,
					std::move(state->put),
					std::move(state->caches),
					request,
					quality,
					replacements);
				crl::on_main(weak, [=, data = std::move(result)]() mutable {
					initDone(std::move(data));
				});
			});
		});
	}
#else // LOTTIE_USE_CACHE
: Animation(player, content, request, quality, replacements) {
#endif // LOTTIE_USE_CACHE
}

Animation::Animation(
	not_null<Player*> player,
	std::shared_ptr<FrameProvider> provider,
	const FrameRequest &request)
: _player(player) {
	const auto weak = base::make_weak(this);
	crl::async([=, provider = std::move(provider)]() mutable {
		auto result = Init(std::move(provider), request);
		crl::on_main(weak, [=, data = std::move(result)]() mutable {
			initDone(std::move(data));
		});
	});
}

bool Animation::ready() const {
	return (_state != nullptr);
}

void Animation::initDone(details::InitData &&data) {
	v::match(data, [&](std::unique_ptr<SharedState> &state) {
		parseDone(std::move(state));
	}, [&](Error error) {
		parseFailed(error);
	});
}

void Animation::parseDone(std::unique_ptr<SharedState> state) {
	Expects(state != nullptr);

	_state = state.get();
	_player->start(this, std::move(state));
}

void Animation::parseFailed(Error error) {
	_player->failed(this, error);
}

QImage Animation::frame() const {
	Expects(_state != nullptr);

	return PrepareFrameByRequest(_state->frameForPaint(), true);
}

QImage Animation::frame(const FrameRequest &request) const {
	Expects(_state != nullptr);

	const auto frame = _state->frameForPaint();
	const auto changed = (frame->request != request);
	if (changed) {
		frame->request = request;
		_player->updateFrameRequest(this, request);
	}
	return PrepareFrameByRequest(frame, !changed);
}

auto Animation::frameInfo(const FrameRequest &request) const -> FrameInfo {
	Expects(_state != nullptr);

	const auto frame = _state->frameForPaint();
	const auto changed = (frame->request != request);
	if (changed) {
		frame->request = request;
		_player->updateFrameRequest(this, request);
	}
	return {
		PrepareFrameByRequest(frame, !changed),
		frame->index % _state->framesCount()
	};
}

int Animation::frameIndex() const {
	Expects(_state != nullptr);

	const auto frame = _state->frameForPaint();
	return frame->index % _state->framesCount();
}

int Animation::framesCount() const {
	Expects(_state != nullptr);

	return _state->framesCount();
}

Information Animation::information() const {
	Expects(_state != nullptr);

	return _state->information();
}

std::optional<Error> ContentError(const QByteArray &content) {
	if (content.size() > kMaxFileSize) {
		return Error::ParseFailed;
	}
	return std::nullopt;
}

} // namespace Lottie
