// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <modules/audio_device/include/audio_device.h>
#include <modules/audio_device/audio_device_buffer.h>
#include <api/scoped_refptr.h>

#include <AudioClient.h>
#include <MMDeviceAPI.h>

#include <winrt/base.h>

typedef struct SwrContext SwrContext;

namespace rtc {
class Thread;
} // namespace rtc

namespace webrtc {
class AudioProcessing;
class AudioFrame;
} // namespace webrtc

namespace Webrtc::details {

[[nodiscard]] bool IsLoopbackCaptureActive();

void LoopbackCapturePushFarEnd(
	crl::time when,
	const QByteArray &samples,
	int frequency,
	int channels);

class AudioDeviceLoopbackWin : public webrtc::AudioDeviceModule {
public:
	explicit AudioDeviceLoopbackWin(webrtc::TaskQueueFactory *taskQueueFactory);
	~AudioDeviceLoopbackWin();

	int32_t ActiveAudioLayer(AudioLayer *audioLayer) const override;
	int32_t RegisterAudioCallback(
		webrtc::AudioTransport *audioCallback) override;

	// Main initializaton and termination
	int32_t Init() override;
	int32_t Terminate() override;
	bool Initialized() const override;

	// Device enumeration
	int16_t PlayoutDevices() override;
	int16_t RecordingDevices() override;
	int32_t PlayoutDeviceName(uint16_t index,
		char name[webrtc::kAdmMaxDeviceNameSize],
		char guid[webrtc::kAdmMaxGuidSize]) override;
	int32_t RecordingDeviceName(uint16_t index,
		char name[webrtc::kAdmMaxDeviceNameSize],
		char guid[webrtc::kAdmMaxGuidSize]) override;

	// Device selection
	int32_t SetPlayoutDevice(uint16_t index) override;
	int32_t SetPlayoutDevice(WindowsDeviceType device) override;
	int32_t SetRecordingDevice(uint16_t index) override;
	int32_t SetRecordingDevice(WindowsDeviceType device) override;

	// Audio transport initialization
	int32_t PlayoutIsAvailable(bool *available) override;
	int32_t InitPlayout() override;
	bool PlayoutIsInitialized() const override;
	int32_t RecordingIsAvailable(bool *available) override;
	int32_t InitRecording() override;
	bool RecordingIsInitialized() const override;

	// Audio transport control
	int32_t StartPlayout() override;
	int32_t StopPlayout() override;
	bool Playing() const override;
	int32_t StartRecording() override;
	int32_t StopRecording() override;
	bool Recording() const override;

	// Audio mixer initialization
	int32_t InitSpeaker() override;
	bool SpeakerIsInitialized() const override;
	int32_t InitMicrophone() override;
	bool MicrophoneIsInitialized() const override;

	// Speaker volume controls
	int32_t SpeakerVolumeIsAvailable(bool *available) override;
	int32_t SetSpeakerVolume(uint32_t volume) override;
	int32_t SpeakerVolume(uint32_t *volume) const override;
	int32_t MaxSpeakerVolume(uint32_t *maxVolume) const override;
	int32_t MinSpeakerVolume(uint32_t *minVolume) const override;

	// Microphone volume controls
	int32_t MicrophoneVolumeIsAvailable(bool *available) override;
	int32_t SetMicrophoneVolume(uint32_t volume) override;
	int32_t MicrophoneVolume(uint32_t *volume) const override;
	int32_t MaxMicrophoneVolume(uint32_t *maxVolume) const override;
	int32_t MinMicrophoneVolume(uint32_t *minVolume) const override;

	// Microphone mute control
	int32_t MicrophoneMuteIsAvailable(bool *available) override;
	int32_t SetMicrophoneMute(bool enable) override;
	int32_t MicrophoneMute(bool *enabled) const override;

	// Speaker mute control
	int32_t SpeakerMuteIsAvailable(bool *available) override;
	int32_t SetSpeakerMute(bool enable) override;
	int32_t SpeakerMute(bool *enabled) const override;

	// Stereo support
	int32_t StereoPlayoutIsAvailable(bool *available) const override;
	int32_t SetStereoPlayout(bool enable) override;
	int32_t StereoPlayout(bool *enabled) const override;
	int32_t StereoRecordingIsAvailable(bool *available) const override;
	int32_t SetStereoRecording(bool enable) override;
	int32_t StereoRecording(bool *enabled) const override;

	// Delay information and control
	int32_t PlayoutDelay(uint16_t *delayMS) const override;

	// Only supported on Android.
	bool BuiltInAECIsAvailable() const override;
	bool BuiltInAGCIsAvailable() const override;
	bool BuiltInNSIsAvailable() const override;

	// Enables the built-in audio effects. Only supported on Android.
	int32_t EnableBuiltInAEC(bool enable) override;
	int32_t EnableBuiltInAGC(bool enable) override;
	int32_t EnableBuiltInNS(bool enable) override;

private:
	void openPlaybackDeviceForCapture();
	void openAudioClient();

	void openRecordingDevice();
	void closeRecordingDevice();

	void captureFailed(const std::string &error);

	void startCaptureOnThread();
	void stopCaptureOnThread();
	void processData();

	bool setupResampler(const WAVEFORMATEX &format);
	bool setupResampler(
		int channels,
		uint64 channelLayout,
		int inputFormat, // AVSampleFormat
		int sampleRate,
		Fn<std::string()> info);
	void ensureResampleSpaceAvailable(int samples);

	static DWORD WINAPI CaptureThreadMethod(LPVOID context);
	DWORD runCaptureThread();

	webrtc::AudioDeviceBuffer _audioDeviceBuffer;

	winrt::com_ptr<IMMDevice> _endpointDevice;
	winrt::com_ptr<IAudioClient> _audioClient;
	winrt::com_ptr<IAudioClient> _audioRenderClientForLoopback;
	winrt::com_ptr<IAudioCaptureClient> _audioCaptureClient;

	rtc::scoped_refptr<webrtc::AudioProcessing> _audioProcessing;
	std::unique_ptr<webrtc::AudioFrame> _capturedFrame;
	std::unique_ptr<webrtc::AudioFrame> _renderedFrame;

	HANDLE _thread = nullptr;
	HANDLE _audioSamplesReadyEvent = nullptr;
	HANDLE _captureThreadShutdownEvent = nullptr;

	UINT32 _bufferSizeFrames = 0;
	UINT32 _deviceFrameSize = 0;
	QByteArray _deviceBuffer;
	QByteArray _resampleBuffer;
	UINT32 _bufferOffset = 0;

	double _queryPerformanceMultiplier = 0.;
	double _deviceFrequencyMultiplier = 0.;

	SwrContext *_swrContext = nullptr;
	int _swrSrcSampleRate = 0;

	bool _microphoneInitialized = false;
	bool _initialized = false;

	bool _recordingInitialized = false;
	bool _recordingFailed = false;
	bool _recording = false;

};

} // namespace Webrtc::details
