// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <rpl/producer.h>
#include <optional>

namespace Webrtc {

enum class Backend;

struct VideoInput {
	QString id;
	QString name;
};

[[nodiscard]] std::vector<VideoInput> GetVideoInputList();

struct AudioInput {
	QString id;
	QString name;
};

[[nodiscard]] std::vector<AudioInput> GetAudioInputList(Backend backend);

struct AudioOutput {
	QString id;
	QString name;
};

[[nodiscard]] std::vector<AudioOutput> GetAudioOutputList(Backend backend);

class MediaDevices {
public:
	virtual ~MediaDevices() = default;

	[[nodiscard]] virtual rpl::producer<QString> audioInputId() = 0;
	[[nodiscard]] virtual rpl::producer<QString> audioOutputId() = 0;
	[[nodiscard]] virtual rpl::producer<QString> videoInputId() = 0;

	virtual void switchToAudioInput(QString id) = 0;
	virtual void switchToAudioOutput(QString id) = 0;
	virtual void switchToVideoInput(QString id) = 0;
};

[[nodiscard]] std::unique_ptr<MediaDevices> CreateMediaDevices(
	QString audioInput,
	QString audioOutput,
	QString videoInput);

[[nodiscard]] bool DesktopCaptureAllowed();
[[nodiscard]] std::optional<QString> UniqueDesktopCaptureSource();

[[nodiscard]] bool InitPipewireStubs();

} // namespace Webrtc
