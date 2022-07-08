// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "webrtc/webrtc_media_devices.h"

namespace Webrtc {

[[nodiscard]] std::vector<VideoInput> MacGetVideoInputList();

class MacMediaDevices final : public MediaDevices {
public:
	MacMediaDevices(
		QString audioInput,
		QString audioOutput,
		QString videoInput);
    ~MacMediaDevices();

	rpl::producer<QString> audioInputId() override {
		return _resolvedAudioInputId.value();
	}
	rpl::producer<QString> audioOutputId() override {
		return _resolvedAudioOutputId.value();
	}
	rpl::producer<QString> videoInputId() override {
		return _resolvedVideoInputId.value();
	}

	void switchToAudioInput(QString id) override;
	void switchToAudioOutput(QString id) override;
	void switchToVideoInput(QString id) override;

private:
    void audioInputRefreshed();
    void audioOutputRefreshed();
    void clearAudioOutputCallbacks();
    void videoInputRefreshed();
    void clearAudioInputCallbacks();

	QString _audioInputId;
	QString _audioOutputId;
	QString _videoInputId;

    rpl::variable<QString> _resolvedAudioInputId;
    rpl::variable<QString> _resolvedAudioOutputId;
    rpl::variable<QString> _resolvedVideoInputId;

    Fn<void()> _defaultAudioOutputChanged;
    Fn<void()> _audioOutputDevicesChanged;
    Fn<void()> _defaultAudioInputChanged;
    Fn<void()> _audioInputDevicesChanged;

};

[[nodiscard]] bool MacDesktopCaptureAllowed();

} // namespace Webrtc
