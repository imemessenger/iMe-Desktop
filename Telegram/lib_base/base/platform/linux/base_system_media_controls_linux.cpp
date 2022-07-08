// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/base_platform_system_media_controls.h"

#include "base/integration.h"
#include "base/const_string.h"
#include "base/platform/base_platform_info.h" // IsWayland
#include "base/platform/linux/base_linux_glibmm_helper.h"

#include <glibmm.h>
#include <giomm.h>

#include <QtCore/QBuffer>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QWidget>

namespace base::Platform {

namespace {

using Metadata = std::map<Glib::ustring, Glib::VariantBase>;

constexpr auto kFakeTrackPath = "/org/desktop_app/track/0"_cs;

constexpr auto kIntrospectionXML = R"INTROSPECTION(<node>
	<interface name='org.mpris.MediaPlayer2'>
		<method name='Raise'/>
		<method name='Quit'/>
		<property name='CanQuit' type='b' access='read'/>
		<property name='CanRaise' type='b' access='read'/>
		<property name='HasTrackList' type='b' access='read'/>
		<property name='Identity' type='s' access='read'/>
		<property name='DesktopEntry' type='s' access='read'/>
		<property name='SupportedUriSchemes' type='as' access='read'/>
		<property name='SupportedMimeTypes' type='as' access='read'/>
		<property name='Fullscreen' type='b' access='readwrite'/>
		<property name='CanSetFullscreen' type='b' access='read'/>
	</interface>
</node>)INTROSPECTION"_cs;

constexpr auto kPlayerIntrospectionXML = R"INTROSPECTION(<node>
	<interface name='org.mpris.MediaPlayer2.Player'>
		<method name='Next'/>
		<method name='Previous'/>
		<method name='Pause'/>
		<method name='PlayPause'/>
		<method name='Stop'/>
		<method name='Play'/>
		<method name='Seek'>
			<arg direction='in' name='Offset' type='x'/>
		</method>
		<method name='SetPosition'>
			<arg direction='in' name='TrackId' type='o'/>
			<arg direction='in' name='Position' type='x'/>
		</method>
		<method name='OpenUri'>
			<arg direction='in' name='Uri' type='s'/>
		</method>
		<signal name='Seeked'>
			<arg name='Position' type='x'/>
		</signal>
		<property name='PlaybackStatus' type='s' access='read'/>
		<property name='LoopStatus' type='s' access='readwrite'/>
		<property name='Rate' type='d' access='readwrite'/>
		<property name='Shuffle' type='b' access='readwrite'/>
		<property name='Metadata' type='a{sv}' access='read'>
			<annotation
				name="org.qtproject.QtDBus.QtTypeName"
				value="QVariantMap"/>
		</property>
		<property name='Volume' type='d' access='readwrite'/>
		<property name='Position' type='x' access='read'/>
		<property name='MinimumRate' type='d' access='read'/>
		<property name='MaximumRate' type='d' access='read'/>
		<property name='CanGoNext' type='b' access='read'/>
		<property name='CanGoPrevious' type='b' access='read'/>
		<property name='CanPlay' type='b' access='read'/>
		<property name='CanPause' type='b' access='read'/>
		<property name='CanSeek' type='b' access='read'/>
		<property name='CanControl' type='b' access='read'/>
	</interface>
</node>)INTROSPECTION"_cs;

// QString to Glib::Variant<Glib::ustring>.
inline auto Q2VUString(const QString &s) {
	return MakeGlibVariant<Glib::ustring>(s.toStdString());
}

auto ConvertPlaybackStatus(SystemMediaControls::PlaybackStatus status) {
	using Status = SystemMediaControls::PlaybackStatus;
	switch (status) {
	case Status::Playing: return Glib::ustring("Playing");
	case Status::Paused: return Glib::ustring("Paused");
	case Status::Stopped: return Glib::ustring("Stopped");
	}
	Unexpected("ConvertPlaybackStatus in SystemMediaControls");
}

auto ConvertLoopStatus(SystemMediaControls::LoopStatus status) {
	using Status = SystemMediaControls::LoopStatus;
	switch (status) {
	case Status::None: return Glib::ustring("None");
	case Status::Track: return Glib::ustring("Track");
	case Status::Playlist: return Glib::ustring("Playlist");
	}
	Unexpected("ConvertLoopStatus in SystemMediaControls");
}

auto EventToCommand(const Glib::ustring &event) {
	using Command = SystemMediaControls::Command;
	if (event == "Pause") {
		return Command::Pause;
	} else if (event == "Play") {
		return Command::Play;
	} else if (event == "Stop") {
		return Command::Stop;
	} else if (event == "PlayPause") {
		return Command::PlayPause;
	} else if (event == "Next") {
		return Command::Next;
	} else if (event == "Previous") {
		return Command::Previous;
	} else if (event == "Quit") {
		return Command::Quit;
	} else if (event == "Raise") {
		return Command::Raise;
	} else if (event == "Shuffle") {
		return Command::Shuffle;
	}
	return Command::None;
}

auto LoopStatusToCommand(const Glib::ustring &status) {
	using Command = SystemMediaControls::Command;
	if (status == "None") {
		return Command::LoopNone;
	} else if (status == "Track") {
		return Command::LoopTrack;
	} else if (status == "Playlist") {
		return Command::LoopPlaylist;
	}
	return Command::None;
}

void Noexcept(Fn<void()> callback) noexcept {
	try {
		callback();
	} catch (...) {
	}
}

} // namespace

struct SystemMediaControls::Private {
public:
	struct Player {
		Metadata metadata;

		bool enabled = false;
		bool shuffle = false;
		gint64 position = 0;
		gint64 duration = 0;
		float64 volume = 0.;
		PlaybackStatus playbackStatus = PlaybackStatus::Stopped;
		LoopStatus loopStatus = LoopStatus::None;

		bool canGoNext = false;
		bool canGoPrevious = false;

		Glib::ustring serviceName;
		Glib::ustring applicationName;
	};

	Private();

	[[nodiscard]] bool init();
	void deinit();

	[[nodiscard]] bool dbusAvailable();

	void handleGetProperty(
		Glib::VariantBase &property,
		const Glib::RefPtr<Gio::DBus::Connection> &connection,
		const Glib::ustring &sender,
		const Glib::ustring &currentObjectPath,
		const Glib::ustring &interfaceName,
		const Glib::ustring &propertyName);

	void handleMethodCall(
		const Glib::RefPtr<Gio::DBus::Connection> &connection,
		const Glib::ustring &sender,
		const Glib::ustring &currentObjectPath,
		const Glib::ustring &interfaceName,
		const Glib::ustring &methodName,
		const Glib::VariantContainerBase &parameters,
		const Glib::RefPtr<Gio::DBus::MethodInvocation> &invocation);

	[[nodiscard]] bool handleSetProperty(
		const Glib::RefPtr<Gio::DBus::Connection> &connection,
		const Glib::ustring &sender,
		const Glib::ustring &currentObjectPath,
		const Glib::ustring &interfaceName,
		const Glib::ustring &propertyName,
		const Glib::VariantBase &value);

	void signalPropertyChanged(
			const Glib::ustring &name,
			const Glib::VariantBase &value);
	void signalSeeked(gint64 position);

	[[nodiscard]] Player &player();

	[[nodiscard]] rpl::producer<Command> commandRequests() const;
	[[nodiscard]] rpl::producer<gint64> seekRequests() const;
	[[nodiscard]] rpl::producer<float64> volumeChangeRequests() const;
	[[nodiscard]] rpl::producer<> updatePositionRequests() const;

private:
	const Gio::DBus::InterfaceVTable _interfaceVTable;
	const Glib::ustring _objectPath;
	const Glib::ustring _playerInterface;
	const Glib::ustring _propertiesInterface;

	const Glib::ustring _signalPropertyChangedName;
	const Glib::ustring _signalSeekedName;

	Glib::RefPtr<Gio::DBus::Connection> _dbusConnection;

	struct {
		Glib::RefPtr<Gio::DBus::NodeInfo> introspectionData;
		Glib::RefPtr<Gio::DBus::NodeInfo> playerIntrospectionData;

		uint ownId = 0;
		uint registerId = 0;
		uint playerRegisterId = 0;
	} _dbus;

	Player _player;

	rpl::event_stream<Command> _commandRequests;
	rpl::event_stream<gint64> _seekRequests;
	rpl::event_stream<float64> _volumeChangeRequests;
	rpl::event_stream<> _updatePositionRequests;
};

SystemMediaControls::Private::Private()
: _interfaceVTable(Gio::DBus::InterfaceVTable(
	sigc::mem_fun(this, &Private::handleMethodCall),
	sigc::mem_fun(this, &Private::handleGetProperty),
	sigc::mem_fun(this, &Private::handleSetProperty)))
, _objectPath("/org/mpris/MediaPlayer2")
, _playerInterface("org.mpris.MediaPlayer2.Player")
, _propertiesInterface("org.freedesktop.DBus.Properties")
, _signalPropertyChangedName("PropertiesChanged")
, _signalSeekedName("Seeked") {
	Noexcept([&] {
		_dbusConnection = Gio::DBus::Connection::get_sync(
			Gio::DBus::BusType::BUS_TYPE_SESSION);
	});
}

bool SystemMediaControls::Private::init() {
	if (!_dbusConnection) {
		return false;
	}
	Noexcept([&] {
		_dbus.introspectionData = Gio::DBus::NodeInfo::create_for_xml(
			std::string(kIntrospectionXML));
	});
	if (!_dbus.introspectionData) {
		return false;
	}
	Noexcept([&] {
		_dbus.playerIntrospectionData = Gio::DBus::NodeInfo::create_for_xml(
			std::string(kPlayerIntrospectionXML));
	});
	if (!_dbus.playerIntrospectionData) {
		return false;
	}
	Noexcept([&] {
		_dbus.ownId = Gio::DBus::own_name(
			Gio::DBus::BusType::BUS_TYPE_SESSION,
			_player.serviceName);
	});
	if (!_dbus.ownId) {
		return false;
	}
	Noexcept([&] {
		_dbus.registerId = _dbusConnection->register_object(
			_objectPath,
			_dbus.introspectionData->lookup_interface(),
			_interfaceVTable);
	});
	if (!_dbus.registerId) {
		return false;
	}
	Noexcept([&] {
		_dbus.playerRegisterId = _dbusConnection->register_object(
			_objectPath,
			_dbus.playerIntrospectionData->lookup_interface(),
			_interfaceVTable);
	});
	if (!_dbus.playerRegisterId) {
		return false;
	}
	return true;
}

void SystemMediaControls::Private::deinit() {
	if (_dbusConnection) {
		if (_dbus.playerRegisterId) {
			_dbusConnection->unregister_object(_dbus.playerRegisterId);
		}

		if (_dbus.registerId) {
			_dbusConnection->unregister_object(_dbus.registerId);
		}
	}

	if (_dbus.ownId) {
		Gio::DBus::unown_name(_dbus.ownId);
	}
}

bool SystemMediaControls::Private::dbusAvailable() {
	return static_cast<bool>(_dbusConnection);
}

void SystemMediaControls::Private::handleGetProperty(
		Glib::VariantBase &property,
		const Glib::RefPtr<Gio::DBus::Connection> &connection,
		const Glib::ustring &sender,
		const Glib::ustring &currentObjectPath,
		const Glib::ustring &interfaceName,
		const Glib::ustring &propertyName) {
	base::Integration::Instance().enterFromEventLoop([&] {
		if (propertyName == "CanQuit") {
			property = MakeGlibVariant(true);
		} else if (propertyName == "CanRaise") {
			property = MakeGlibVariant(!::Platform::IsWayland());
		} else if (propertyName == "CanSetFullscreen") {
			property = MakeGlibVariant(false);
		} else if (propertyName == "DesktopEntry") {
			property = MakeGlibVariant<Glib::ustring>(
				QGuiApplication::desktopFileName().chopped(8).toStdString());
		} else if (propertyName == "Fullscreen") {
			property = MakeGlibVariant(false);
		} else if (propertyName == "HasTrackList") {
			property = MakeGlibVariant(false);
		} else if (propertyName == "Identity") {
			property = MakeGlibVariant(_player.applicationName.empty()
				? Glib::ustring{
					QGuiApplication::desktopFileName()
						.chopped(8)
						.toStdString() }
				: _player.applicationName);
		} else if (propertyName == "SupportedMimeTypes") {
			property = MakeGlibVariant<std::vector<Glib::ustring>>({});
		} else if (propertyName == "SupportedUriSchemes") {
			property = MakeGlibVariant<std::vector<Glib::ustring>>({});
		} else if (propertyName == "CanControl") {
			property = MakeGlibVariant(true);
		} else if (propertyName == "CanGoNext") {
			property = MakeGlibVariant<bool>(_player.canGoNext);
		} else if (propertyName == "CanGoPrevious") {
			property = MakeGlibVariant<bool>(_player.canGoPrevious);
		} else if (propertyName == "CanPause") {
			property = MakeGlibVariant(true);
		} else if (propertyName == "CanPlay") {
			property = MakeGlibVariant(true);
		} else if (propertyName == "CanSeek") {
			property = MakeGlibVariant(true);
		} else if (propertyName == "MaximumRate") {
			property = MakeGlibVariant<float64>(1.0);
		} else if (propertyName == "Metadata") {
			property = MakeGlibVariant<Metadata>(_player.metadata);
		} else if (propertyName == "MinimumRate") {
			property = MakeGlibVariant<float64>(1.0);
		} else if (propertyName == "PlaybackStatus") {
			property = MakeGlibVariant(
				ConvertPlaybackStatus(_player.playbackStatus));
		} else if (propertyName == "LoopStatus") {
			property = MakeGlibVariant(
				ConvertLoopStatus(_player.loopStatus));
		} else if (propertyName == "Position") {
			_updatePositionRequests.fire({});
			property = MakeGlibVariant<gint64>(_player.position);
		} else if (propertyName == "Rate") {
			property = MakeGlibVariant<float64>(1.0);
		} else if (propertyName == "Shuffle") {
			property = MakeGlibVariant<bool>(_player.shuffle);
		} else if (propertyName == "Volume") {
			property = MakeGlibVariant<float64>(_player.volume);
		}
	});
}

void SystemMediaControls::Private::handleMethodCall(
		const Glib::RefPtr<Gio::DBus::Connection> &connection,
		const Glib::ustring &sender,
		const Glib::ustring &currentObjectPath,
		const Glib::ustring &interfaceName,
		const Glib::ustring &methodName,
		const Glib::VariantContainerBase &parameters,
		const Glib::RefPtr<Gio::DBus::MethodInvocation> &invocation) {

	if (methodName == "Seek") {
		// Seek (x: Offset);
		Glib::Variant<gint64> offset;
		parameters.get_child(offset, 0);

		base::Integration::Instance().enterFromEventLoop([&] {
			_player.position += offset.get();
			_seekRequests.fire_copy(_player.position);
		});
	} else if (methodName == "SetPosition") {
		// SetPosition (o: TrackId, x: Position);
		Glib::Variant<gint64> newPosition;
		parameters.get_child(newPosition, 1);

		base::Integration::Instance().enterFromEventLoop([&] {
			_player.position = newPosition.get();
			_seekRequests.fire_copy(_player.position);
		});
	} else {
		const auto command = EventToCommand(methodName);
		if (command == Command::None) {
			return;
		} else {
			base::Integration::Instance().enterFromEventLoop([&] {
				_commandRequests.fire_copy(command);
			});
		}
	}

	Noexcept([&] {
		invocation->return_value({});
	});
}

bool SystemMediaControls::Private::handleSetProperty(
		const Glib::RefPtr<Gio::DBus::Connection> &connection,
		const Glib::ustring &sender,
		const Glib::ustring &currentObjectPath,
		const Glib::ustring &interfaceName,
		const Glib::ustring &propertyName,
		const Glib::VariantBase &value) {
	if (propertyName == "Fullscreen") {
	} else if (propertyName == "LoopStatus") {
		base::Integration::Instance().enterFromEventLoop([&] {
			Noexcept([&] {
				_commandRequests.fire_copy(LoopStatusToCommand(
					GlibVariantCast<Glib::ustring>(value)));
			});
		});
	} else if (propertyName == "Rate") {
	} else if (propertyName == "Shuffle") {
		base::Integration::Instance().enterFromEventLoop([&] {
			_commandRequests.fire_copy(EventToCommand(propertyName));
		});
	} else if (propertyName == "Volume") {
		base::Integration::Instance().enterFromEventLoop([&] {
			Noexcept([&] {
				_volumeChangeRequests.fire_copy(
					GlibVariantCast<float64>(value));
			});
		});
	} else {
		return false;
	}

	return true;
}

void SystemMediaControls::Private::signalPropertyChanged(
		const Glib::ustring &name,
		const Glib::VariantBase &value) {
	Noexcept([&] {
		_dbusConnection->emit_signal(
			_objectPath,
			_propertiesInterface,
			_signalPropertyChangedName,
			{},
			MakeGlibVariant(std::tuple{
				_playerInterface,
				std::map<Glib::ustring, Glib::VariantBase>{
					{ name, value },
				},
				std::vector<Glib::ustring>{},
			}));
	});
}

void SystemMediaControls::Private::signalSeeked(gint64 position) {
	Noexcept([&] {
		_dbusConnection->emit_signal(
			_objectPath,
			_playerInterface,
			_signalSeekedName,
			{},
			MakeGlibVariant(std::tuple{ position }));
	});
}

SystemMediaControls::Private::Player &SystemMediaControls::Private::player() {
	return _player;
}

auto SystemMediaControls::Private::commandRequests() const
-> rpl::producer<SystemMediaControls::Command> {
	return _commandRequests.events();
}

rpl::producer<gint64> SystemMediaControls::Private::seekRequests() const {
	return _seekRequests.events();
}

auto SystemMediaControls::Private::volumeChangeRequests() const
-> rpl::producer<float64> {
	return _volumeChangeRequests.events();
}

rpl::producer<> SystemMediaControls::Private::updatePositionRequests() const {
	return _updatePositionRequests.events();
}

SystemMediaControls::SystemMediaControls()
: _private(std::make_unique<Private>()) {
}

SystemMediaControls::~SystemMediaControls() {
	_private->deinit();
}

bool SystemMediaControls::init(std::optional<QWidget*> parent) {
	clearMetadata();

	return _private->dbusAvailable();
}

void SystemMediaControls::setServiceName(const QString &name) {
	_private->player().serviceName = name.toStdString();
}

void SystemMediaControls::setApplicationName(const QString &name) {
	_private->player().applicationName = name.toStdString();
}

void SystemMediaControls::setEnabled(bool enabled) {
	if (_private->player().enabled == enabled) {
		return;
	}
	_private->player().enabled = enabled;
	if (enabled) {
		const auto inited = _private->init();
		if (!inited) {
			_private->deinit();
		}
		_private->player().enabled = inited;
	} else {
		_private->deinit();
	}
	updateDisplay();
}

void SystemMediaControls::setIsNextEnabled(bool value) {
	_private->player().canGoNext = value;
	_private->signalPropertyChanged(
		"CanGoNext",
		MakeGlibVariant<bool>(value));
}

void SystemMediaControls::setIsPreviousEnabled(bool value) {
	_private->player().canGoPrevious = value;
	_private->signalPropertyChanged(
		"CanGoPrevious",
		MakeGlibVariant<bool>(value));
}

void SystemMediaControls::setIsPlayPauseEnabled(bool value) {
}

void SystemMediaControls::setIsStopEnabled(bool value) {
}

void SystemMediaControls::setPlaybackStatus(PlaybackStatus status) {
	_private->player().playbackStatus = status;
	_private->signalPropertyChanged(
		"PlaybackStatus",
		MakeGlibVariant<Glib::ustring>(ConvertPlaybackStatus(status)));
}

void SystemMediaControls::setLoopStatus(LoopStatus status) {
	_private->player().loopStatus = status;
	_private->signalPropertyChanged(
		"LoopStatus",
		MakeGlibVariant<Glib::ustring>(ConvertLoopStatus(status)));
}

void SystemMediaControls::setShuffle(bool value) {
	_private->player().shuffle = value;
	_private->signalPropertyChanged(
		"Shuffle",
		MakeGlibVariant<bool>(value));
}

void SystemMediaControls::setTitle(const QString &title) {
	_private->player().metadata["xesam:title"] = Q2VUString(title);
}

void SystemMediaControls::setArtist(const QString &artist) {
	_private->player().metadata["xesam:artist"] = Q2VUString(artist);
}

void SystemMediaControls::setThumbnail(const QImage &thumbnail) {
	QByteArray thumbnailData;
	QBuffer thumbnailBuffer(&thumbnailData);
	thumbnail.save(&thumbnailBuffer, "JPG", 87);

	_private->player().metadata["mpris:artUrl"] =
		Q2VUString("data:image/jpeg;base64," + thumbnailData.toBase64());

	updateDisplay();
}

void SystemMediaControls::setDuration(int duration) {
	_private->player().duration = duration * 1000;
	_private->player().metadata["mpris:length"] = MakeGlibVariant<gint64>(
		_private->player().duration);
}

void SystemMediaControls::setPosition(int position) {
	auto &playerPosition = _private->player().position;
	const auto was = _private->player().position;
	playerPosition = position * 1000;

	const auto positionDifference = was - playerPosition;
	if (positionDifference > 1000000 || positionDifference < -1000000) {
		_private->signalSeeked(playerPosition);
	}
}

void SystemMediaControls::setVolume(float64 volume) {
	_private->player().volume = volume;
	_private->signalPropertyChanged(
		"Volume",
		MakeGlibVariant<float64>(volume));
}

void SystemMediaControls::clearThumbnail() {
	_private->player().metadata["mpris:artUrl"] = MakeGlibVariant(
		Glib::ustring{});

	updateDisplay();
}

void SystemMediaControls::clearMetadata() {
	_private->player().metadata.clear();

	Glib::VariantStringBase path;
	Glib::VariantStringBase::create_object_path(
		path,
		std::string(kFakeTrackPath));

	_private->player().metadata["mpris:trackid"] = path;
}

void SystemMediaControls::updateDisplay() {
	_private->signalPropertyChanged(
		"Metadata",
		MakeGlibVariant<Metadata>(_private->player().metadata));
}

auto SystemMediaControls::commandRequests() const
-> rpl::producer<SystemMediaControls::Command> {
	return _private->commandRequests();
}

rpl::producer<float64> SystemMediaControls::seekRequests() const {
	return _private->seekRequests(
	) | rpl::map([=](gint64 position) {
		return float64(position) / (_private->player().duration);
	});
}

rpl::producer<float64> SystemMediaControls::volumeChangeRequests() const {
	return _private->volumeChangeRequests();
}

rpl::producer<> SystemMediaControls::updatePositionRequests() const {
	return _private->updatePositionRequests();
}

bool SystemMediaControls::seekingSupported() const {
	return true;
}

bool SystemMediaControls::volumeSupported() const {
	return true;
}

bool SystemMediaControls::Supported() {
	return true;
}

} // namespace base::Platform
