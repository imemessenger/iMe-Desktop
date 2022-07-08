// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/base_platform_system_media_controls.h"

#include <unknwn.h> // Conversion from winrt::guid_of to GUID.

#include "base/integration.h"
#include "base/platform/win/base_info_win.h"
#include "base/platform/win/base_windows_winrt.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Storage.Streams.h>

#include <systemmediatransportcontrolsinterop.h>

#include <QtCore/QBuffer>
#include <QtGui/QImage>
#include <QtGui/QWindow>
#include <QtWidgets/QWidget>

namespace winrt {
namespace Streams {
	using namespace Windows::Storage::Streams;
} // namespace Streams
namespace Media {
	using namespace Windows::Media;
} // namespace Media
} // namespace winrt

namespace base::Platform {

struct SystemMediaControls::Private {
	using IReferenceStatics
		= winrt::Streams::IRandomAccessStreamReferenceStatics;
	Private()
	: controls(nullptr)
	, referenceStatics(winrt::get_activation_factory
		<winrt::Streams::RandomAccessStreamReference,
		IReferenceStatics>()) {
	}
	winrt::Media::SystemMediaTransportControls controls;
	winrt::Media::ISystemMediaTransportControlsDisplayUpdater displayUpdater;
	winrt::Media::IMusicDisplayProperties displayProperties;
	winrt::Streams::DataWriter iconDataWriter;
	const IReferenceStatics referenceStatics;
	winrt::event_token eventToken;
	bool initialized = false;

	rpl::event_stream<SystemMediaControls::Command> commandRequests;
};

namespace {

winrt::Media::MediaPlaybackStatus SmtcPlaybackStatus(
		SystemMediaControls::PlaybackStatus status) {
	switch (status) {
	case SystemMediaControls::PlaybackStatus::Playing:
		return winrt::Media::MediaPlaybackStatus::Playing;
	case SystemMediaControls::PlaybackStatus::Paused:
		return winrt::Media::MediaPlaybackStatus::Paused;
	case SystemMediaControls::PlaybackStatus::Stopped:
		return winrt::Media::MediaPlaybackStatus::Stopped;
	}
	Unexpected("SmtcPlaybackStatus in SystemMediaControls");
}


auto SMTCButtonToCommand(
		winrt::Media::SystemMediaTransportControlsButton button) {
	using SMTCButton = winrt::Media::SystemMediaTransportControlsButton;
	using Command = SystemMediaControls::Command;

	switch (button) {
	case SMTCButton::Play:
		return Command::Play;
	case SMTCButton::Pause:
		return Command::Pause;
	case SMTCButton::Next:
		return Command::Next;
	case SMTCButton::Previous:
		return Command::Previous;
	case SMTCButton::Stop:
		return Command::Stop;
	case SMTCButton::Record:
	case SMTCButton::FastForward:
	case SMTCButton::Rewind:
	case SMTCButton::ChannelUp:
	case SMTCButton::ChannelDown:
		return Command::None;
	}
	return Command::None;
}

} // namespace

SystemMediaControls::SystemMediaControls()
: _private(std::make_unique<Private>()) {
}

SystemMediaControls::~SystemMediaControls() {
	if (_private->eventToken) {
		_private->controls.ButtonPressed(base::take(_private->eventToken));
		clearMetadata();
	}
}

bool SystemMediaControls::init(std::optional<QWidget*> parent) {
	if (_private->initialized) {
		return _private->initialized;
	}
	if (!parent.has_value()) {
		return false;
	}
	const auto window = (*parent)->window()->windowHandle();
	if (!window) {
		return false;
	}

	// Should be moved to separated file.
	const auto hwnd = reinterpret_cast<HWND>(window->winId());
	if (!hwnd) {
		return false;
	}

	const auto interop = WinRT::Try([&] {
		return winrt::get_activation_factory<
			winrt::Media::SystemMediaTransportControls,
			ISystemMediaTransportControlsInterop>();
	}).value_or(nullptr);
	if (!interop) {
		return false;
	}

	winrt::com_ptr<winrt::Media::ISystemMediaTransportControls> icontrols;
	auto hr = interop->GetForWindow(
		hwnd,
		winrt::guid_of<winrt::Media::ISystemMediaTransportControls>(),
		icontrols.put_void());

	if (FAILED(hr)) {
		return false;
	}

	_private->controls = winrt::Media::SystemMediaTransportControls(
		icontrols.detach(),
		winrt::take_ownership_from_abi);

	using ButtonsEventArgs =
		winrt::Media::SystemMediaTransportControlsButtonPressedEventArgs;
	const auto result = WinRT::Try([&] {
		// Buttons handler.
		_private->eventToken = _private->controls.ButtonPressed([=](
				const auto &sender,
				const ButtonsEventArgs &args) {
			// This lambda is called in a non-main thread.
			crl::on_main([=] {
				const auto button = WinRT::Try([&] {
					return SMTCButtonToCommand(args.Button());
				}).value_or(SystemMediaControls::Command::None);
				_private->commandRequests.fire_copy(button);
			});
		});

		_private->controls.IsEnabled(true);

		auto displayUpdater = _private->controls.DisplayUpdater();
		displayUpdater.Type(winrt::Media::MediaPlaybackType::Music);
		_private->displayProperties = displayUpdater.MusicProperties();
		_private->displayUpdater = std::move(displayUpdater);
	});

	_private->initialized = result;
	return result;
}

void SystemMediaControls::setServiceName(const QString &name) {
}

void SystemMediaControls::setApplicationName(const QString &name) {
}

void SystemMediaControls::setEnabled(bool enabled) {
	WinRT::Try([&] { _private->controls.IsEnabled(enabled); });
}

void SystemMediaControls::setIsNextEnabled(bool value) {
	WinRT::Try([&] { _private->controls.IsNextEnabled(value); });
}

void SystemMediaControls::setIsPreviousEnabled(bool value) {
	WinRT::Try([&] { _private->controls.IsPreviousEnabled(value); });
}

void SystemMediaControls::setIsPlayPauseEnabled(bool value) {
	WinRT::Try([&] {
		_private->controls.IsPlayEnabled(value);
		_private->controls.IsPauseEnabled(value);
	});
}

void SystemMediaControls::setIsStopEnabled(bool value) {
	WinRT::Try([&] { _private->controls.IsStopEnabled(value); });
}

void SystemMediaControls::setPlaybackStatus(
		SystemMediaControls::PlaybackStatus status) {
	WinRT::Try([&] {
		_private->controls.PlaybackStatus(SmtcPlaybackStatus(status));
	});
}

void SystemMediaControls::setLoopStatus(LoopStatus status) {
}

void SystemMediaControls::setShuffle(bool value) {
}

void SystemMediaControls::setTitle(const QString &title) {
	const auto htitle = winrt::to_hstring(title.toStdString());
	WinRT::Try([&] { _private->displayProperties.Title(htitle); });
}

void SystemMediaControls::setArtist(const QString &artist) {
	const auto hartist = winrt::to_hstring(artist.toStdString());
	WinRT::Try([&] { _private->displayProperties.Artist(hartist); });
}

void SystemMediaControls::setThumbnail(const QImage &thumbnail) {
	auto thumbStream = winrt::Streams::InMemoryRandomAccessStream();
	_private->iconDataWriter = winrt::Streams::DataWriter(thumbStream);

	const auto bitmapRawData = [&] {
		QByteArray bytes;
		QBuffer buffer(&bytes);
		buffer.open(QIODevice::WriteOnly);
		thumbnail.save(&buffer, "JPG", 87);
		buffer.close();
		return std::vector<unsigned char>(bytes.begin(), bytes.end());
	}();

	WinRT::Try([&] {
		_private->iconDataWriter.WriteBytes(bitmapRawData);

		using namespace winrt::Windows;
		_private->iconDataWriter.StoreAsync().Completed([=,
				thumbStream = std::move(thumbStream)](
			Foundation::IAsyncOperation<uint32> asyncOperation,
			Foundation::AsyncStatus status) {

			// Check the async operation completed successfully.
			if ((status != Foundation::AsyncStatus::Completed)
				|| FAILED(asyncOperation.ErrorCode())) {
				return;
			}

			WinRT::Try([&] {
				_private->displayUpdater.Thumbnail(
					_private->referenceStatics.CreateFromStream(thumbStream));
				_private->displayUpdater.Update();
			});
		});
	});
}

void SystemMediaControls::setDuration(int duration) {
}

void SystemMediaControls::setPosition(int position) {
}

void SystemMediaControls::setVolume(float64 volume) {
}

void SystemMediaControls::clearThumbnail() {
	WinRT::Try([&] {
		_private->displayUpdater.Thumbnail(nullptr);
		_private->displayUpdater.Update();
	});
}

void SystemMediaControls::clearMetadata() {
	WinRT::Try([&] {
		_private->displayUpdater.ClearAll();
		_private->controls.IsEnabled(false);
	});
}

void SystemMediaControls::updateDisplay() {
	WinRT::Try([&] {
		_private->controls.IsEnabled(true);
		_private->displayUpdater.Type(winrt::Media::MediaPlaybackType::Music);
		_private->displayUpdater.Update();
	});
}

auto SystemMediaControls::commandRequests() const
-> rpl::producer<SystemMediaControls::Command> {
	return _private->commandRequests.events();
}

rpl::producer<float64> SystemMediaControls::seekRequests() const {
	return rpl::never<float64>();
}

rpl::producer<float64> SystemMediaControls::volumeChangeRequests() const {
	return rpl::never<float64>();
}

rpl::producer<> SystemMediaControls::updatePositionRequests() const {
	return rpl::never<>();
}

bool SystemMediaControls::seekingSupported() const {
	return false;
}

bool SystemMediaControls::volumeSupported() const {
	return false;
}

bool SystemMediaControls::Supported() {
	return ::Platform::IsWindows10OrGreater();
}

} // namespace base::Platform
