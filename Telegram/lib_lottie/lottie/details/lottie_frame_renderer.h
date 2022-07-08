// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/basic_types.h"
#include "base/weak_ptr.h"
#include "lottie/lottie_common.h"

#include <QImage>
#include <QSize>
#include <crl/crl_time.h>
#include <crl/crl_object_on_queue.h>
#include <limits>

namespace Lottie {

// Frame rate can be 1, 2, ... , 29, 30 or 60.
inline constexpr auto kNormalFrameRate = 30;
inline constexpr auto kMaxFrameRate = 60;
inline constexpr auto kMaxSize = 4096;
inline constexpr auto kMaxFramesCount = 210;
inline constexpr auto kFrameDisplayTimeAlreadyDone
	= std::numeric_limits<crl::time>::max();
inline constexpr auto kDisplayedInitial = crl::time(-1);

class Player;
class FrameProvider;
struct FrameProviderToken;

struct Frame {
	QImage original;
	crl::time displayed = kDisplayedInitial;
	crl::time display = kTimeUnknown;
	int index = 0;
	int sizeRounding = 0;

	FrameRequest request;
	QImage prepared;
};

QImage PrepareFrameByRequest(
	not_null<Frame*> frame,
	bool useExistingPrepared);

class SharedState {
public:
	SharedState(
		std::shared_ptr<FrameProvider> provider,
		const FrameRequest &request);

	void start(
		not_null<Player*> owner,
		crl::time now,
		crl::time delay = 0,
		int skippedFrames = 0);

	[[nodiscard]] Information information() const;
	[[nodiscard]] bool initialized() const;

	[[nodiscard]] not_null<Frame*> frameForPaint();
	[[nodiscard]] int framesCount() const;
	[[nodiscard]] crl::time nextFrameDisplayTime() const;
	void addTimelineDelay(crl::time delayed, int skippedFrames = 0);
	void markFrameDisplayed(crl::time now);
	bool markFrameShown();

	struct RenderResult {
		bool rendered = false;
		base::weak_ptr<Player> notify;
	};
	[[nodiscard]] RenderResult renderNextFrame(const FrameRequest &request);

	~SharedState();

private:
	void init(QImage cover, const FrameRequest &request);
	void renderNextFrame(
		not_null<Frame*> frame,
		const FrameRequest &request);
	[[nodiscard]] int sizeRounding() const;
	[[nodiscard]] crl::time countFrameDisplayTime(int index) const;
	[[nodiscard]] not_null<Frame*> getFrame(int index);
	[[nodiscard]] not_null<const Frame*> getFrame(int index) const;
	[[nodiscard]] int counter() const;

	// crl::queue changes 0,2,4,6 to 1,3,5,7.
	// main thread changes 1,3,5,7 to 2,4,6,0.
	static constexpr auto kCounterUninitialized = -1;
	std::atomic<int> _counter = kCounterUninitialized;

	static constexpr auto kFramesCount = 4;
	std::array<Frame, kFramesCount> _frames;

	base::weak_ptr<Player> _owner;
	crl::time _started = kTimeUnknown;

	// (_counter % 2) == 1 main thread can write _delay.
	// (_counter % 2) == 0 crl::queue can read _delay.
	crl::time _delay = kTimeUnknown;

	int _frameIndex = 0;
	int _framesCount = 0;
	int _skippedFrames = 0;
	const std::shared_ptr<FrameProvider> _provider;
	std::unique_ptr<FrameProviderToken> _token;

};

class FrameRendererObject;

class FrameRenderer final {
public:
	static std::shared_ptr<FrameRenderer> CreateIndependent();
	static std::shared_ptr<FrameRenderer> Instance();

	void append(
		std::unique_ptr<SharedState> entry,
		const FrameRequest &request);

	void updateFrameRequest(
		not_null<SharedState*> entry,
		const FrameRequest &request);
	void frameShown();
	void remove(not_null<SharedState*> state);

private:
	using Implementation = FrameRendererObject;
	crl::object_on_queue<Implementation> _wrapped;

};

} // namespace Lottie
