// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "webrtc/webrtc_create_adm.h"

#include "webrtc/details/webrtc_openal_adm.h"

#include <rtc_base/ref_counted_object.h>
#include <modules/audio_device/include/audio_device_factory.h>

#ifdef WEBRTC_WIN
#include "webrtc/win/webrtc_loopback_adm_win.h"
#endif // WEBRTC_WIN

namespace Webrtc {

rtc::scoped_refptr<webrtc::AudioDeviceModule> CreateAudioDeviceModule(
		webrtc::TaskQueueFactory *factory,
		Backend backend) {
//	const auto create = [&](webrtc::AudioDeviceModule::AudioLayer layer) {
//		return webrtc::AudioDeviceModule::Create(layer, factory);
//	};
	const auto check = [&](
			const rtc::scoped_refptr<webrtc::AudioDeviceModule> &result) {
		return (result && (result->Init() == 0)) ? result : nullptr;
	};
	if (true || backend == Backend::OpenAL) {
		if (auto result = check(rtc::make_ref_counted<details::AudioDeviceOpenAL>(factory))) {
			return result;
		}
	}
//#ifdef WEBRTC_WIN
//	if (backend == Backend::ADM2) {
//		if (auto result = check(webrtc::CreateWindowsCoreAudioAudioDeviceModule(factory))) {
//			return result;
//		}
//	}
//#endif // WEBRTC_WIN
//	if (backend == Backend::ADM) {
//		if (auto result = check(create(webrtc::AudioDeviceModule::kPlatformDefaultAudio))) {
//			return result;
//		}
//#ifdef WEBRTC_LINUX
//		if (auto result = check(create(webrtc::AudioDeviceModule::kLinuxAlsaAudio))) {
//			return result;
//		}
//#endif // WEBRTC_LINUX
//	}
	return nullptr;
}

auto AudioDeviceModuleCreator(Backend backend)
-> std::function<AudioDeviceModulePtr(webrtc::TaskQueueFactory*)> {
	return [=](webrtc::TaskQueueFactory *factory) {
		return CreateAudioDeviceModule(factory, backend);
	};
}

AudioDeviceModulePtr CreateLoopbackAudioDeviceModule(
		webrtc::TaskQueueFactory* factory) {
#ifdef WEBRTC_WIN
	auto result = rtc::make_ref_counted<details::AudioDeviceLoopbackWin>(
		factory);
	if (result->Init() == 0) {
		return result;
	}
#endif // WEBRTC_WIN
	return nullptr;
}

auto LoopbackAudioDeviceModuleCreator()
-> std::function<AudioDeviceModulePtr(webrtc::TaskQueueFactory*)> {
	return CreateLoopbackAudioDeviceModule;
}

} // namespace Webrtc
