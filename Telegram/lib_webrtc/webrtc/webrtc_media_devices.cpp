// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "webrtc/webrtc_media_devices.h"

#include "webrtc/webrtc_create_adm.h"
#include "webrtc/mac/webrtc_media_devices_mac.h"
#include "webrtc/linux/webrtc_media_devices_linux.h"
#include "base/platform/base_platform_info.h"
#include "crl/crl_async.h"

#include <api/task_queue/default_task_queue_factory.h>
#include <modules/video_capture/video_capture_factory.h>
#include <modules/audio_device/include/audio_device_factory.h>

#ifdef WEBRTC_USE_PIPEWIRE
#include <modules/desktop_capture/linux/wayland/shared_screencast_stream.h>
#endif // WEBRTC_USE_PIPEWIRE

#ifdef WEBRTC_MAC
//#define MAC_TRACK_MEDIA_DEVICES
#endif // WEBRTC_MAC

namespace Webrtc {
namespace {

#ifndef MAC_TRACK_MEDIA_DEVICES

class MediaDevicesSimple final : public MediaDevices {
public:
	MediaDevicesSimple(
		QString audioInput,
		QString audioOutput,
		QString videoInput)
	: _audioInputId(audioInput)
	, _audioOutputId(audioOutput)
	, _videoInputId(videoInput) {
	}

	rpl::producer<QString> audioInputId() override {
		return _audioInputId.value();
	}
	rpl::producer<QString> audioOutputId() override {
		return _audioOutputId.value();
	}
	rpl::producer<QString> videoInputId() override {
		return _videoInputId.value();
	}

	void switchToAudioInput(QString id) override {
		_audioInputId = id;
	}
	void switchToAudioOutput(QString id) override {
		_audioOutputId = id;
	}
	void switchToVideoInput(QString id) override {
		_videoInputId = id;
	}

private:
	rpl::variable<QString> _audioInputId;
	rpl::variable<QString> _audioOutputId;
	rpl::variable<QString> _videoInputId;

};

#endif // !MAC_TRACK_MEDIA_DEVICES

} // namespace

std::vector<VideoInput> GetVideoInputList() {
#ifdef WEBRTC_MAC
	return MacGetVideoInputList();
#else // WEBRTC_MAC
	const auto info = std::unique_ptr<
		webrtc::VideoCaptureModule::DeviceInfo
	>(webrtc::VideoCaptureFactory::CreateDeviceInfo());

	auto result = std::vector<VideoInput>();
	if (!info) {
		return result;
	}
	const auto count = info->NumberOfDevices();
	for (auto i = uint32_t(); i != count; ++i) {
		constexpr auto kLengthLimit = 256;
		auto id = std::string(kLengthLimit, char(0));
		auto name = std::string(kLengthLimit, char(0));
		info->GetDeviceName(
			i,
			name.data(),
			name.size(),
			id.data(),
			id.size());
		const auto utfName = QString::fromUtf8(name.c_str());
		const auto utfId = id[0] ? QString::fromUtf8(id.c_str()) : utfName;
		result.push_back({
			.id = utfId,
			.name = utfName,
		});
	}
	return result;
#endif // WEBRTC_MAC
}

std::vector<AudioInput> GetAudioInputList(Backend backend) {
	auto result = std::vector<AudioInput>();
	const auto resolve = [&] {
		const auto queueFactory = webrtc::CreateDefaultTaskQueueFactory();
		const auto info = CreateAudioDeviceModule(
			queueFactory.get(),
			backend);
		if (!info) {
			return;
		}
		const auto count = info->RecordingDevices();
		if (count <= 0) {
			return;
		}
		for (auto i = int16_t(); i != count; ++i) {
			char name[webrtc::kAdmMaxDeviceNameSize + 1] = { 0 };
			char id[webrtc::kAdmMaxGuidSize + 1] = { 0 };
			info->RecordingDeviceName(i, name, id);
			const auto utfName = QString::fromUtf8(name);
			const auto utfId = id[0] ? QString::fromUtf8(id) : utfName;
#ifdef WEBRTC_WIN
			if (utfName.startsWith("Default - ")
				|| utfName.startsWith("Communication - ")) {
				continue;
			}
#elif defined WEBRTC_MAC
			if (utfName.startsWith("default (") && utfName.endsWith(")")) {
				continue;
			}
#endif // WEBRTC_WIN || WEBRTC_MAC
			result.push_back({
				.id = utfId,
				.name = utfName,
			});
		}
	};
	if constexpr (Platform::IsWindows()) {
		// Windows version requires MultiThreaded COM apartment.
		crl::sync(resolve);
	} else {
		resolve();
	}
	return result;
}

std::vector<AudioOutput> GetAudioOutputList(Backend backend) {
	auto result = std::vector<AudioOutput>();
	const auto resolve = [&] {
		const auto queueFactory = webrtc::CreateDefaultTaskQueueFactory();
		const auto info = CreateAudioDeviceModule(
			queueFactory.get(),
			backend);
		if (!info) {
			return;
		}
		const auto count = info->PlayoutDevices();
		if (count <= 0) {
			return;
		}
		for (auto i = int16_t(); i != count; ++i) {
			char name[webrtc::kAdmMaxDeviceNameSize + 1] = { 0 };
			char id[webrtc::kAdmMaxGuidSize + 1] = { 0 };
			info->PlayoutDeviceName(i, name, id);
			const auto utfName = QString::fromUtf8(name);
			const auto utfId = id[0] ? QString::fromUtf8(id) : utfName;
#ifdef WEBRTC_WIN
			if (utfName.startsWith("Default - ")
				|| utfName.startsWith("Communication - ")) {
				continue;
			}
#elif defined WEBRTC_MAC
			if (utfName.startsWith("default (") && utfName.endsWith(")")) {
				continue;
			}
#endif // WEBRTC_WIN || WEBRTC_MAC
			result.push_back({
				.id = utfId,
				.name = utfName,
			});
		}
	};
	if constexpr (Platform::IsWindows()) {
		// Windows version requires MultiThreaded COM apartment.
		crl::sync(resolve);
	} else {
		resolve();
	}
	return result;
}

std::unique_ptr<MediaDevices> CreateMediaDevices(
		QString audioInput,
		QString audioOutput,
		QString videoInput) {
#ifdef MAC_TRACK_MEDIA_DEVICES
	return std::make_unique<MacMediaDevices>(
		audioInput,
		audioOutput,
		videoInput);
#else // MAC_TRACK_MEDIA_DEVICES
	return std::make_unique<MediaDevicesSimple>(
		audioInput,
		audioOutput,
		videoInput);
#endif // MAC_TRACK_MEDIA_DEVICES
}

bool DesktopCaptureAllowed() {
#ifdef WEBRTC_MAC
	return MacDesktopCaptureAllowed();
#else // WEBRTC_MAC
	return true;
#endif // WEBRTC_MAC
}

std::optional<QString> UniqueDesktopCaptureSource() {
#ifdef WEBRTC_USE_PIPEWIRE
	return LinuxUniqueDesktopCaptureSource();
#else // WEBRTC_USE_PIPEWIRE
	return std::nullopt;
#endif // WEBRTC_USE_PIPEWIRE
}

bool InitPipewireStubs() {
#ifdef WEBRTC_USE_PIPEWIRE
	return webrtc::InitPipewireStubs();
#else // WEBRTC_USE_PIPEWIRE
	return true;
#endif // WEBRTC_USE_PIPEWIRE
}

} // namespace Webrtc
