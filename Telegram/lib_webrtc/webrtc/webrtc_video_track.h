// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <rpl/variable.h>
#include <QtCore/QSize>
#include <QtGui/QImage>

namespace rtc {
template <typename VideoFrameT>
class VideoSinkInterface;
} // namespace rtc

namespace webrtc {
class VideoFrame;
} // namespace webrtc

namespace Webrtc {

using SinkInterface = rtc::VideoSinkInterface<webrtc::VideoFrame>;

struct FrameRequest {
	QSize resize;
	QSize outer;
	//ImageRoundRadius radius = ImageRoundRadius();
	//RectParts corners = RectPart::AllCorners;
	bool strict = true;

	static FrameRequest NonStrict() {
		auto result = FrameRequest();
		result.strict = false;
		return result;
	}

	[[nodiscard]] bool empty() const {
		return resize.isEmpty();
	}

	[[nodiscard]] bool operator==(const FrameRequest &other) const {
		return (resize == other.resize)
			&& (outer == other.outer)/*
			&& (radius == other.radius)
			&& (corners == other.corners)*/;
	}
	[[nodiscard]] bool operator!=(const FrameRequest &other) const {
		return !(*this == other);
	}

	[[nodiscard]] bool goodFor(const FrameRequest &other) const {
		return (*this == other) || (strict && !other.strict);
	}
};

enum class VideoState {
	Inactive,
	Paused,
	Active,
};

enum class FrameFormat {
	None,
	ARGB32,
	YUV420,
};

struct FrameChannel {
	const void *data = nullptr;
	int stride = 0;
};

struct FrameYUV420 {
	QSize size;
	QSize chromaSize;
	FrameChannel y;
	FrameChannel u;
	FrameChannel v;
};

struct FrameWithInfo {
	QImage original;
	FrameYUV420 *yuv420 = nullptr;
	FrameFormat format = FrameFormat::None;
	int rotation = 0;
	int index = -1;
};

class VideoTrack final {
public:
	// Called from the main thread.
	explicit VideoTrack(
		VideoState state,
		bool requireARGB32 = true);
	~VideoTrack();

	void markFrameShown();
	[[nodiscard]] QImage frame(const FrameRequest &request);
	[[nodiscard]] FrameWithInfo frameWithInfo(bool requireARGB32) const;
	[[nodiscard]] QSize frameSize() const;
	[[nodiscard]] rpl::producer<> renderNextFrame() const;
	[[nodiscard]] std::shared_ptr<SinkInterface> sink();

	[[nodiscard]] VideoState state() const;
	[[nodiscard]] rpl::producer<VideoState> stateValue() const;
	[[nodiscard]] rpl::producer<VideoState> stateChanges() const;
	void setState(VideoState state);

private:
	class Sink;
	struct Frame;

	static void PrepareFrameByRequests(not_null<Frame*> frame, int rotation);

	std::shared_ptr<Sink> _sink;
	crl::time _inactiveFrom = 0;
	rpl::variable<VideoState> _state;
};

} // namespace Webrtc
