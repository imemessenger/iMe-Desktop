// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "webrtc/webrtc_audio_input_tester.h"

#include "webrtc/webrtc_create_adm.h"
#include "crl/crl_object_on_thread.h"
#include "crl/crl_async.h"

#include <media/engine/webrtc_media_engine.h>
#include <api/task_queue/default_task_queue_factory.h>

namespace Webrtc {

class AudioInputTester::Impl : public webrtc::AudioTransport {
public:
	Impl(
		const Backend &backend,
		const QString &deviceId,
		const std::shared_ptr<std::atomic<int>> &maxSample);
	~Impl();

	void setDeviceId(const QString &deviceId);

private:
	void init();

	int32_t RecordedDataIsAvailable(
		const void* audioSamples,
		const size_t nSamples,
		const size_t nBytesPerSample,
		const size_t nChannels,
		const uint32_t samplesPerSec,
		const uint32_t totalDelayMS,
		const int32_t clockDrift,
		const uint32_t currentMicLevel,
		const bool keyPressed,
		uint32_t& newMicLevel) override;

	// Implementation has to setup safe values for all specified out parameters.
	int32_t NeedMorePlayData(
		const size_t nSamples,
		const size_t nBytesPerSample,
		const size_t nChannels,
		const uint32_t samplesPerSec,
		void* audioSamples,
		size_t& nSamplesOut,  // NOLINT
		int64_t* elapsed_time_ms,
		int64_t* ntp_time_ms) override;

	// Method to pull mixed render audio data from all active VoE channels.
	// The data will not be passed as reference for audio processing internally.
	void PullRenderData(
		int bits_per_sample,
		int sample_rate,
		size_t number_of_channels,
		size_t number_of_frames,
		void* audio_data,
		int64_t* elapsed_time_ms,
		int64_t* ntp_time_ms) override;

	std::shared_ptr<std::atomic<int>> _maxSample;
	std::unique_ptr<webrtc::TaskQueueFactory> _taskQueueFactory;
	rtc::scoped_refptr<webrtc::AudioDeviceModule> _adm;

};

AudioInputTester::Impl::Impl(
	const Backend &backend,
	const QString &deviceId,
	const std::shared_ptr<std::atomic<int>> &maxSample)
: _maxSample(std::move(maxSample))
, _taskQueueFactory(webrtc::CreateDefaultTaskQueueFactory())
, _adm(CreateAudioDeviceModule(_taskQueueFactory.get(), backend)) {
	init();
	setDeviceId(deviceId);
}

AudioInputTester::Impl::~Impl() {
	if (_adm) {
		_adm->StopRecording();
		_adm->RegisterAudioCallback(nullptr);
		_adm->Terminate();
	}
}

void AudioInputTester::Impl::init() {
	if (!_adm) {
		return;
	}
	_adm->Init();
	_adm->RegisterAudioCallback(this);
}

void AudioInputTester::Impl::setDeviceId(const QString &deviceId) {
	if (!_adm) {
		return;
	}
	auto specific = false;
	_adm->StopRecording();
	const auto guard = gsl::finally([&] {
		if (!specific) {
			_adm->SetRecordingDevice(
				webrtc::AudioDeviceModule::kDefaultCommunicationDevice);
		}
		if (_adm->InitRecording() == 0) {
			_adm->StartRecording();
		}
	});
	if (deviceId == u"default"_q || deviceId.isEmpty()) {
		return;
	}
	const auto count = _adm
		? _adm->RecordingDevices()
		: int16_t(-666);
	if (count <= 0) {
		return;
	}
	for (auto i = 0; i != count; ++i) {
		char name[webrtc::kAdmMaxDeviceNameSize + 1] = { 0 };
		char guid[webrtc::kAdmMaxGuidSize + 1] = { 0 };
		_adm->RecordingDeviceName(i, name, guid);
		if (deviceId == guid) {
			if (_adm->SetRecordingDevice(i) == 0) {
				specific = true;
			}
			return;
		}
	}
}

int32_t AudioInputTester::Impl::RecordedDataIsAvailable(
		const void* audioSamples,
		const size_t nSamples,
		const size_t nBytesPerSample,
		const size_t nChannels,
		const uint32_t samplesPerSec,
		const uint32_t totalDelayMS,
		const int32_t clockDrift,
		const uint32_t currentMicLevel,
		const bool keyPressed,
		uint32_t& newMicLevel) {
	const auto channels = nBytesPerSample / sizeof(int16_t);
	if (channels > 0 && !(nBytesPerSample % sizeof(int16_t))) {
		auto max = 0;
		auto values = static_cast<const int16_t*>(audioSamples);
		for (auto i = size_t(); i != nSamples * channels; ++i) {
			if (max < values[i]) {
				max = values[i];
			}
		}
		const auto now = _maxSample->load();
		if (max > now) {
			_maxSample->store(max);
		}
	}
	newMicLevel = currentMicLevel;
	return 0;
}

int32_t AudioInputTester::Impl::NeedMorePlayData(const size_t nSamples,
		const size_t nBytesPerSample,
		const size_t nChannels,
		const uint32_t samplesPerSec,
		void* audioSamples,
		size_t& nSamplesOut,
		int64_t* elapsed_time_ms,
		int64_t* ntp_time_ms) {
	nSamplesOut = 0;
	return 0;
}

void AudioInputTester::Impl::PullRenderData(int bits_per_sample,
		int sample_rate,
		size_t number_of_channels,
		size_t number_of_frames,
		void* audio_data,
		int64_t* elapsed_time_ms,
		int64_t* ntp_time_ms) {
}

AudioInputTester::AudioInputTester(
	const Backend &backend,
	const QString &deviceId)
: _maxSample(std::make_shared<std::atomic<int>>(0))
, _impl(backend, deviceId, std::as_const(_maxSample)) {
}

AudioInputTester::~AudioInputTester() = default;

void AudioInputTester::setDeviceId(const QString &deviceId) {
	_impl.with([=](Impl &impl) {
		impl.setDeviceId(deviceId);
	});
}

float AudioInputTester::getAndResetLevel() {
	return _maxSample->exchange(0) / float(INT16_MAX);
}

} // namespace Webrtc
