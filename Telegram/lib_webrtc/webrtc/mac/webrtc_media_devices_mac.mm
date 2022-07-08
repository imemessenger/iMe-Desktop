// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "webrtc/mac/webrtc_media_devices_mac.h"

#include <modules/audio_device/include/audio_device_defines.h>

#import <AVFoundation/AVFoundation.h>
#import <IOKit/hidsystem/IOHIDLib.h>

namespace Webrtc {
namespace {

[[nodiscard]] QString NS2QString(NSString *text) {
	return QString::fromUtf8([text cStringUsingEncoding:NSUTF8StringEncoding]);
}

[[nodiscard]] bool IsDefault(const QString &id) {
	return id.isEmpty() || (id == "default");
}

auto AudioOutputDevicePropertyAddress = AudioObjectPropertyAddress{
	.mSelector = kAudioHardwarePropertyDefaultOutputDevice,
	.mScope = kAudioObjectPropertyScopeGlobal,
	.mElement = kAudioObjectPropertyElementMaster,
};

auto AudioInputDevicePropertyAddress = AudioObjectPropertyAddress{
	.mSelector = kAudioHardwarePropertyDefaultInputDevice,
	.mScope = kAudioObjectPropertyScopeGlobal,
	.mElement = kAudioObjectPropertyElementMaster,
};

auto AudioDeviceListPropertyAddress = AudioObjectPropertyAddress{
    .mSelector = kAudioHardwarePropertyDevices,
	.mScope = kAudioObjectPropertyScopeGlobal,
    .mElement = kAudioObjectPropertyElementMaster,
};

auto AudioDeviceUIDAddress = AudioObjectPropertyAddress{
	.mSelector = kAudioDevicePropertyDeviceUID,
	.mScope = kAudioObjectPropertyScopeGlobal,
	.mElement = kAudioObjectPropertyElementMaster,
};

OSStatus PropertyChangedCallback(
		AudioObjectID inObjectID,
		UInt32 inNumberAddresses,
		const AudioObjectPropertyAddress *inAddresses,
		void *inClientData) {
	(*reinterpret_cast<Fn<void()>*>(inClientData))();
	return 0;
}

[[nodiscard]] QString GetDeviceUID(AudioDeviceID deviceId) {
	if (deviceId == kAudioDeviceUnknown) {
		return QString();
	}
    CFStringRef uid = NULL;
    UInt32 size = sizeof(uid);
    AudioObjectGetPropertyData(
		deviceId,
		&AudioDeviceUIDAddress,
		0,
		nil,
		&size,
		&uid);

	if (!uid) {
		return QString();
	}

	const auto kLengthLimit = 128;
	char buffer[kLengthLimit + 1] = { 0 };
    const CFIndex kCStringSize = kLengthLimit;
    CFStringGetCString(uid, buffer, kCStringSize, kCFStringEncodingUTF8);
    CFRelease(uid);

	return QString::fromUtf8(buffer);
}

[[nodiscard]] AudioDeviceID GetDeviceByUID(const QString &id) {
	const auto utf = id.toUtf8();
	auto uid = CFStringCreateWithCString(NULL, utf.data(), kCFStringEncodingUTF8);
	if (!uid) {
		return kAudioObjectUnknown;
	}
	AudioObjectPropertyAddress address = {
		.mSelector = kAudioHardwarePropertyDeviceForUID,
		.mScope = kAudioObjectPropertyScopeGlobal,
		.mElement = kAudioObjectPropertyElementMaster,
	};
	AudioDeviceID deviceId = kAudioObjectUnknown;
	UInt32 deviceSize = sizeof(deviceId);

	AudioValueTranslation value;
	value.mInputData = &uid;
	value.mInputDataSize = sizeof(CFStringRef);
	value.mOutputData = &deviceId;
	value.mOutputDataSize = deviceSize;
	UInt32 valueSize = sizeof(AudioValueTranslation);

	OSStatus result = AudioObjectGetPropertyData(
		kAudioObjectSystemObject,
		&address,
		0,
		0,
		&valueSize,
		&value);
	CFRelease(uid);

	return (result == noErr) ? deviceId : kAudioObjectUnknown;
}

[[nodiscard]] bool DeviceHasScope(
		AudioDeviceID deviceId,
		AudioObjectPropertyScope scope) {
	UInt32 size = 0;
	AudioObjectPropertyAddress address = {
		.mSelector = kAudioDevicePropertyStreamConfiguration,
		.mScope = scope,
		.mElement = kAudioObjectPropertyElementMaster,
	};
	OSStatus result = AudioObjectGetPropertyDataSize(
		deviceId,
		&address,
		0,
		0,
		&size);
	if (result == kAudioHardwareBadDeviceError) {
        // This device doesn't actually exist; continue iterating.
		return false;
	} else if (result != noErr) {
		return false;
	}

	AudioBufferList *bufferList = (AudioBufferList*)malloc(size);
	result = AudioObjectGetPropertyData(
		deviceId,
		&address,
		0,
		nil,
		&size,
		bufferList);
	const auto good = (result == noErr) && (bufferList->mNumberBuffers > 0);
	free(bufferList);
	return good;
}

[[nodiscard]] std::vector<AudioDeviceID> GetAllDeviceIds() {
	auto listSize = UInt32();
	auto listAddress = AudioObjectPropertyAddress{
		.mSelector = kAudioHardwarePropertyDevices,
		.mScope = kAudioObjectPropertyScopeGlobal,
		.mElement = kAudioObjectPropertyElementMaster,
	};

	OSStatus result = AudioObjectGetPropertyDataSize(
		kAudioObjectSystemObject,
		&listAddress,
		0,
		nil,
		&listSize);
	if (result != noErr) {
		return {};
	}

	const auto count = listSize / sizeof(AudioDeviceID);
	auto list = std::vector<AudioDeviceID>(count, kAudioDeviceUnknown);

	result = AudioObjectGetPropertyData(
		kAudioObjectSystemObject,
		&listAddress,
		0,
		nil,
		&listSize,
		list.data());
	return (result == noErr) ? list : std::vector<AudioDeviceID>();
}

[[nodiscard]] QString ComputeDefaultAudioDevice(
		AudioObjectPropertyScope scope) {
	auto deviceId = AudioDeviceID(kAudioDeviceUnknown);
	auto deviceIdSize = UInt32(sizeof(AudioDeviceID));
	const auto address = (scope == kAudioDevicePropertyScopeInput)
		? &AudioInputDevicePropertyAddress
		: &AudioOutputDevicePropertyAddress;
	AudioObjectGetPropertyData(
		AudioObjectID(kAudioObjectSystemObject),
		address,
		0,
		nil,
		&deviceIdSize,
		&deviceId);
	return GetDeviceUID(deviceId);
}

[[nodiscard]] QString ResolveAudioDevice(
		const QString &id,
		AudioObjectPropertyScope scope) {
	if (!IsDefault(id)) {
		// Try to choose specific, if asked.
		const auto deviceId = GetDeviceByUID(id);
		if (deviceId != kAudioObjectUnknown
			&& DeviceHasScope(deviceId, scope)) {
			return id;
		}
	}

	// Try to choose default.
	const auto desired = ComputeDefaultAudioDevice(scope);
	if (!desired.isEmpty()) {
		return desired;
	}

	// Try to choose any.
	for (const auto deviceId : GetAllDeviceIds()) {
		if (DeviceHasScope(deviceId, scope)) {
			if (const auto uid = GetDeviceUID(deviceId); !uid.isEmpty()) {
				return uid;
			}
		}
	}
	return id;
}

[[nodiscard]] QString ResolveAudioInput(const QString &id) {
	return ResolveAudioDevice(id, kAudioDevicePropertyScopeInput);
}

[[nodiscard]] QString ResolveAudioOutput(const QString &id) {
	return ResolveAudioDevice(id, kAudioDevicePropertyScopeOutput);
}

[[nodiscard]] QString ResolveVideoInput(const QString &id) {
	return id;
}

} // namespace

std::vector<VideoInput> MacGetVideoInputList() {
	auto result = std::vector<VideoInput>();
	const auto add = [&](AVCaptureDevice *device) {
		if (!device) {
			return;
		}
		const auto id = NS2QString([device uniqueID]);
		if (ranges::contains(result, id, &VideoInput::id)) {
			return;
		}
		result.push_back(VideoInput{
			.id = id,
			.name = NS2QString([device localizedName]),
		});
	};
    add([AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo]);

    NSArray<AVCaptureDevice*> *devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
    for (AVCaptureDevice *device in devices) {
		add(device);
	}
	return result;
}

MacMediaDevices::MacMediaDevices(
	QString audioInput,
	QString audioOutput,
	QString videoInput)
: _audioInputId(audioInput)
, _audioOutputId(audioOutput)
, _videoInputId(videoInput) {
	audioInputRefreshed();
	audioOutputRefreshed();
	videoInputRefreshed();
}

MacMediaDevices::~MacMediaDevices() {
	clearAudioOutputCallbacks();
	clearAudioInputCallbacks();
}

void MacMediaDevices::switchToAudioInput(QString id) {
	if (_audioInputId == id) {
		return;
	}
	_audioInputId = id;
	audioInputRefreshed();
}

void MacMediaDevices::switchToAudioOutput(QString id) {
	if (_audioOutputId == id) {
		return;
	}
	_audioOutputId = id;
	audioOutputRefreshed();
}

void MacMediaDevices::switchToVideoInput(QString id) {
	if (_videoInputId == id) {
		return;
	}
	_videoInputId = id;
	videoInputRefreshed();
}

void MacMediaDevices::audioInputRefreshed() {
	clearAudioInputCallbacks();
	const auto refresh = [=] {
		_resolvedAudioInputId = ResolveAudioInput(_audioInputId);
	};
	if (IsDefault(_audioInputId)) {
		_defaultAudioInputChanged = refresh;
		AudioObjectAddPropertyListener(
			AudioObjectID(kAudioObjectSystemObject),
			&AudioInputDevicePropertyAddress,
			PropertyChangedCallback,
			&_defaultAudioInputChanged);
		_defaultAudioInputChanged();
	} else {
		_audioInputDevicesChanged = refresh;
		AudioObjectAddPropertyListener(
			AudioObjectID(kAudioObjectSystemObject),
			&AudioDeviceListPropertyAddress,
			PropertyChangedCallback,
			&_audioInputDevicesChanged);
	}
	refresh();
}

void MacMediaDevices::audioOutputRefreshed() {
	clearAudioOutputCallbacks();
	const auto refresh = [=] {
		_resolvedAudioOutputId = ResolveAudioOutput(_audioOutputId);
	};
	if (IsDefault(_audioOutputId)) {
		_defaultAudioOutputChanged = refresh;
		AudioObjectAddPropertyListener(
			AudioObjectID(kAudioObjectSystemObject),
			&AudioOutputDevicePropertyAddress,
			PropertyChangedCallback,
			&_defaultAudioOutputChanged);
		_defaultAudioOutputChanged();
	} else {
		_audioOutputDevicesChanged = refresh;
		AudioObjectAddPropertyListener(
			AudioObjectID(kAudioObjectSystemObject),
			&AudioDeviceListPropertyAddress,
			PropertyChangedCallback,
			&_audioOutputDevicesChanged);
	}
	refresh();
}

void MacMediaDevices::clearAudioOutputCallbacks() {
	if (_defaultAudioOutputChanged) {
		AudioObjectRemovePropertyListener(
			AudioObjectID(kAudioObjectSystemObject),
			&AudioOutputDevicePropertyAddress,
			PropertyChangedCallback,
			&_defaultAudioOutputChanged);
		_defaultAudioOutputChanged = nullptr;
	}
	if (_audioOutputDevicesChanged) {
		AudioObjectAddPropertyListener(
			AudioObjectID(kAudioObjectSystemObject),
			&AudioDeviceListPropertyAddress,
			PropertyChangedCallback,
			&_audioOutputDevicesChanged);
		_audioOutputDevicesChanged = nullptr;
	}
}

void MacMediaDevices::clearAudioInputCallbacks() {
	if (_defaultAudioInputChanged) {
		AudioObjectRemovePropertyListener(
			AudioObjectID(kAudioObjectSystemObject),
			&AudioInputDevicePropertyAddress,
			PropertyChangedCallback,
			&_defaultAudioInputChanged);
		_defaultAudioInputChanged = nullptr;
	}
	if (_audioInputDevicesChanged) {
		AudioObjectAddPropertyListener(
			AudioObjectID(kAudioObjectSystemObject),
			&AudioDeviceListPropertyAddress,
			PropertyChangedCallback,
			&_audioInputDevicesChanged);
		_audioInputDevicesChanged = nullptr;
	}
}

void MacMediaDevices::videoInputRefreshed() {
	_resolvedVideoInputId = ResolveVideoInput(_videoInputId);
}

bool MacDesktopCaptureAllowed() {
	if (@available(macOS 11.0, *)) {
		// Screen Recording is required on macOS 10.15 an later.
		// Even if user grants access, restart is required.
		static const auto result = CGPreflightScreenCaptureAccess();
		return result;
	} else if (@available(macOS 10.15, *)) {
		const auto stream = CGDisplayStreamCreate(
			CGMainDisplayID(),
			1,
			1,
			kCVPixelFormatType_32BGRA,
			CFDictionaryRef(),
			^(
				CGDisplayStreamFrameStatus status,
				uint64_t display_time,
				IOSurfaceRef frame_surface,
				CGDisplayStreamUpdateRef updateRef) {
			});
		if (!stream) {
			return false;
		}
		CFRelease(stream);
		return true;
	}
	return true;
}

} // namespace Webrtc
