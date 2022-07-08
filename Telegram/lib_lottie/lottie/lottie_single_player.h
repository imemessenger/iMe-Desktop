// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "lottie/lottie_player.h"
#include "lottie/lottie_animation.h"
#include "base/timer.h"

#include <rpl/event_stream.h>

namespace Lottie {

class FrameRenderer;
class FrameProvider;

struct DisplayFrameRequest {
};

struct Update {
	std::variant<
		Information,
		DisplayFrameRequest> data;
};

class SinglePlayer final : public Player {
public:
	SinglePlayer(
		const QByteArray &content,
		const FrameRequest &request,
		Quality quality = Quality::Default,
		const ColorReplacements *replacements = nullptr,
		std::shared_ptr<FrameRenderer> renderer = nullptr);
	SinglePlayer(
		FnMut<void(FnMut<void(QByteArray &&cached)>)> get, // Main thread.
		FnMut<void(QByteArray &&cached)> put, // Unknown thread.
		const QByteArray &content,
		const FrameRequest &request,
		Quality quality = Quality::Default,
		const ColorReplacements *replacements = nullptr,
		std::shared_ptr<FrameRenderer> renderer = nullptr);
	SinglePlayer( // Multi-cache version.
		int keysCount,
		FnMut<void(int, FnMut<void(QByteArray &&)>)> get,
		FnMut<void(int, QByteArray &&)> put,
		const QByteArray &content,
		const FrameRequest &request,
		Quality quality = Quality::Default,
		const ColorReplacements *replacements = nullptr,
		std::shared_ptr<FrameRenderer> renderer = nullptr);
	~SinglePlayer();

	[[nodiscard]] static std::shared_ptr<FrameProvider> SharedProvider(
		int keysCount,
		FnMut<void(int, FnMut<void(QByteArray &&)>)> get,
		FnMut<void(int, QByteArray &&)> put,
		const QByteArray &content,
		const FrameRequest &request,
		Quality quality = Quality::Default,
		const ColorReplacements *replacements = nullptr);
	explicit SinglePlayer(
		std::shared_ptr<FrameProvider> provider,
		const FrameRequest &request,
		std::shared_ptr<FrameRenderer> renderer = nullptr);

	void start(
		not_null<Animation*> animation,
		std::unique_ptr<SharedState> state) override;
	void failed(not_null<Animation*> animation, Error error) override;
	void updateFrameRequest(
		not_null<const Animation*> animation,
		const FrameRequest &request) override;
	bool markFrameShown() override;
	void checkStep() override;

	[[nodiscard]] rpl::producer<Update, Error> updates() const;

	[[nodiscard]] bool ready() const;
	[[nodiscard]] QImage frame() const;
	[[nodiscard]] QImage frame(const FrameRequest &request) const;
	[[nodiscard]] Animation::FrameInfo frameInfo(
		const FrameRequest &request) const;
	[[nodiscard]] int frameIndex() const;
	[[nodiscard]] int framesCount() const;
	[[nodiscard]] Information information() const;

	[[nodiscard]] rpl::lifetime &lifetime() {
		return _lifetime;
	}

private:
	void checkNextFrameAvailability();
	void checkNextFrameRender();
	void renderFrame(crl::time now);

	base::Timer _timer;
	const std::shared_ptr<FrameRenderer> _renderer;
	SharedState *_state = nullptr;
	crl::time _nextFrameTime = kTimeUnknown;
	rpl::event_stream<Update, Error> _updates;

	rpl::lifetime _lifetime;

	Animation _animation;

};

} // namespace Lottie
