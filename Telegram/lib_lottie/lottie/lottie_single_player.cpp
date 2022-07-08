// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/lottie_single_player.h"

#include "lottie/details/lottie_frame_renderer.h"
#include "lottie/details/lottie_frame_provider_shared.h"
#include "lottie/details/lottie_frame_provider_direct.h"
#include "lottie/details/lottie_frame_provider_cached_multi.h"

#include <crl/crl_async.h>

namespace Lottie {

SinglePlayer::SinglePlayer(
	const QByteArray &content,
	const FrameRequest &request,
	Quality quality,
	const ColorReplacements *replacements,
	std::shared_ptr<FrameRenderer> renderer)
: _timer([=] { checkNextFrameRender(); })
, _renderer(renderer ? renderer : FrameRenderer::Instance())
, _animation(this, content, request, quality, replacements) {
}

SinglePlayer::SinglePlayer(
	FnMut<void(FnMut<void(QByteArray &&cached)>)> get, // Main thread.
	FnMut<void(QByteArray &&cached)> put, // Unknown thread.
	const QByteArray &content,
	const FrameRequest &request,
	Quality quality,
	const ColorReplacements *replacements,
	std::shared_ptr<FrameRenderer> renderer)
: _timer([=] { checkNextFrameRender(); })
, _renderer(renderer ? renderer : FrameRenderer::Instance())
, _animation(
	this,
	std::move(get),
	std::move(put),
	content,
	request,
	quality,
	replacements) {
}

SinglePlayer::SinglePlayer(
	int keysCount,
	FnMut<void(int, FnMut<void(QByteArray &&)>)> get,
	FnMut<void(int, QByteArray &&)> put,
	const QByteArray &content,
	const FrameRequest &request,
	Quality quality,
	const ColorReplacements *replacements,
	std::shared_ptr<FrameRenderer> renderer)
: _timer([=] { checkNextFrameRender(); })
, _renderer(renderer ? renderer : FrameRenderer::Instance())
, _animation(
	this,
	keysCount,
	std::move(get),
	std::move(put),
	content,
	request,
	quality,
	replacements) {
}

SinglePlayer::~SinglePlayer() {
	if (_state) {
		_renderer->remove(_state);
	}
}

std::shared_ptr<FrameProvider> SinglePlayer::SharedProvider(
		int keysCount,
		FnMut<void(int, FnMut<void(QByteArray &&)>)> get,
		FnMut<void(int, QByteArray &&)> put,
		const QByteArray &content,
		const FrameRequest &request,
		Quality quality,
		const ColorReplacements *replacements) {
	auto factory = [=, get = std::move(get), put = std::move(put)](
			FnMut<void(std::unique_ptr<FrameProvider>)> done) mutable {
#ifdef LOTTIE_USE_CACHE
		struct State {
			std::atomic<int> left = 0;
			std::vector<QByteArray> caches;
			FnMut<void(int, QByteArray &&cached)> put;
			FnMut<void(std::unique_ptr<FrameProvider>)> done;
		};
		const auto state = std::make_shared<State>();
		state->left = keysCount;
		state->put = std::move(put);
		state->done = std::move(done);
		state->caches.resize(keysCount);
		for (auto i = 0; i != keysCount; ++i) {
			get(i, [=](QByteArray &&cached) {
				state->caches[i] = std::move(cached);
				if (--state->left) {
					return;
				}
				crl::async([=, done = std::move(state->done)]() mutable {
					if (const auto error = ContentError(content)) {
						done(nullptr);
						return;
					}
					auto provider = std::make_unique<FrameProviderCachedMulti>(
						content,
						std::move(state->put),
						std::move(state->caches),
						request,
						quality,
						replacements);
					done(provider->valid() ? std::move(provider) : nullptr);
				});
			});
		}
#else // LOTTIE_USE_CACHE
		crl::async([=, done = std::move(done)]() mutable {
			if (const auto error = ContentError(content)) {
				done(nullptr);
				return;
			}
			auto provider = std::make_unique<FrameProviderDirect>(quality);
			done(provider->load(content, replacements)
				? std::move(provider)
				: nullptr);
		});
#endif // LOTTIE_USE_CACHE
	};
	return std::make_shared<FrameProviderShared>(std::move(factory));
}

SinglePlayer::SinglePlayer(
	std::shared_ptr<FrameProvider> provider,
	const FrameRequest &request,
	std::shared_ptr<FrameRenderer> renderer)
: _timer([=] { checkNextFrameRender(); })
, _renderer(renderer ? renderer : FrameRenderer::Instance())
, _animation(this, std::move(provider), request) {
}

void SinglePlayer::start(
		not_null<Animation*> animation,
		std::unique_ptr<SharedState> state) {
	Expects(animation == &_animation);

	_state = state.get();
	auto information = state->information();
	state->start(this, crl::now());
	const auto request = state->frameForPaint()->request;
	_renderer->append(std::move(state), request);

	crl::on_main_update_requests(
	) | rpl::start_with_next([=] {
		checkStep();
	}, _lifetime);

	// This may destroy the player.
	_updates.fire({ std::move(information) });
}

void SinglePlayer::failed(not_null<Animation*> animation, Error error) {
	Expects(animation == &_animation);

	_updates.fire_error(std::move(error));
}

rpl::producer<Update, Error> SinglePlayer::updates() const {
	return _updates.events();
}

bool SinglePlayer::ready() const {
	return _animation.ready();
}

QImage SinglePlayer::frame() const {
	return _animation.frame();
}

QImage SinglePlayer::frame(const FrameRequest &request) const {
	return _animation.frame(request);
}

Animation::FrameInfo SinglePlayer::frameInfo(
		const FrameRequest &request) const {
	return _animation.frameInfo(request);
}

int SinglePlayer::frameIndex() const {
	return _animation.frameIndex();
}

int SinglePlayer::framesCount() const {
	return _animation.framesCount();
}

Information SinglePlayer::information() const {
	return _animation.information();
}

void SinglePlayer::checkStep() {
	if (_nextFrameTime == kFrameDisplayTimeAlreadyDone) {
		return;
	} else if (_nextFrameTime != kTimeUnknown) {
		checkNextFrameRender();
	} else {
		checkNextFrameAvailability();
	}
}

void SinglePlayer::checkNextFrameAvailability() {
	Expects(_state != nullptr);
	Expects(_nextFrameTime == kTimeUnknown);

	_nextFrameTime = _state->nextFrameDisplayTime();
	Assert(_nextFrameTime != kFrameDisplayTimeAlreadyDone);
	if (_nextFrameTime != kTimeUnknown) {
		checkNextFrameRender();
	}
}

void SinglePlayer::checkNextFrameRender() {
	Expects(_nextFrameTime != kTimeUnknown);

	const auto now = crl::now();
	if (now < _nextFrameTime) {
		if (!_timer.isActive()) {
			_timer.callOnce(_nextFrameTime - now);
		}
	} else {
		_timer.cancel();
		renderFrame(now);
	}
}

void SinglePlayer::renderFrame(crl::time now) {
	_state->markFrameDisplayed(now);
	_state->addTimelineDelay(now - _nextFrameTime);

	_nextFrameTime = kFrameDisplayTimeAlreadyDone;
	_updates.fire({ DisplayFrameRequest() });
}

void SinglePlayer::updateFrameRequest(
		not_null<const Animation*> animation,
		const FrameRequest &request) {
	Expects(animation == &_animation);
	Expects(_state != nullptr);

	_renderer->updateFrameRequest(_state, request);
}

bool SinglePlayer::markFrameShown() {
	Expects(_state != nullptr);

	if (_nextFrameTime == kFrameDisplayTimeAlreadyDone) {
		_nextFrameTime = kTimeUnknown;
	}
	if (_state->markFrameShown()) {
		_renderer->frameShown();
		return true;
	}
	return false;
}

} // namespace Lottie
