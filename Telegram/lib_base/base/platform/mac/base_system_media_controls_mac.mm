// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/base_platform_system_media_controls.h"

#include "base/integration.h"
#include "base/platform/mac/base_utilities_mac.h"

#import <MediaPlayer/MediaPlayer.h>

#include <QtGui/QImage>
#include <QtWidgets/QWidget>

namespace {

using Command = base::Platform::SystemMediaControls::Command;

using ::Platform::Q2NSString;
using ::Platform::Q2NSImage;

inline auto CommandCenter() {
	return [MPRemoteCommandCenter sharedCommandCenter];
}

MPNowPlayingPlaybackState ConvertPlaybackStatus(
		base::Platform::SystemMediaControls::PlaybackStatus status) {
	using Status = base::Platform::SystemMediaControls::PlaybackStatus;
	switch (status) {
	case Status::Playing: return MPNowPlayingPlaybackStatePlaying;
	case Status::Paused: return MPNowPlayingPlaybackStatePaused;
	case Status::Stopped: return MPNowPlayingPlaybackStateStopped;
	}
	Unexpected("ConvertPlaybackStatus in SystemMediaControls");
}

auto EventToCommand(MPRemoteCommandEvent *event) {
	const auto commandCenter = CommandCenter();
	const auto command = event.command;
	if (command == commandCenter.pauseCommand) {
		return Command::Pause;
	} else if (command == commandCenter.playCommand) {
		return Command::Play;
	} else if (command == commandCenter.stopCommand) {
		return Command::Stop;
	} else if (command == commandCenter.togglePlayPauseCommand) {
		return Command::PlayPause;
	} else if (command == commandCenter.nextTrackCommand) {
		return Command::Next;
	} else if (command == commandCenter.previousTrackCommand) {
		return Command::Previous;
	}
	return Command::None;
}

struct RemoteCommand {
	const MPRemoteCommand *command;
	bool lastEnabled = false;
};

} // namespace

#pragma mark - CommandHandler

@interface CommandHandler : NSObject  {
}
@end // @interface CommandHandler

@implementation CommandHandler {
	rpl::event_stream<Command> _commandRequests;
	rpl::event_stream<int> _seekRequests;
	std::vector<RemoteCommand> _commands;
}

- (id)init {
	self = [super init];

	const auto center = CommandCenter();

	_commands = {
		{ .command = center.pauseCommand },
		{ .command = center.playCommand },
		{ .command = center.stopCommand },
		{ .command = center.togglePlayPauseCommand },
		{ .command = center.nextTrackCommand },
		{ .command = center.previousTrackCommand },
		{ .command = center.changeRepeatModeCommand },
		{ .command = center.changeShuffleModeCommand },
		{ .command = center.changePlaybackRateCommand },
		{ .command = center.seekBackwardCommand },
		{ .command = center.seekForwardCommand },
		{ .command = center.skipBackwardCommand },
		{ .command = center.skipForwardCommand },
		{ .command = center.changePlaybackPositionCommand },
		{ .command = center.ratingCommand },
		{ .command = center.likeCommand },
		{ .command = center.dislikeCommand },
		{ .command = center.bookmarkCommand },
		{ .command = center.enableLanguageOptionCommand },
		{ .command = center.disableLanguageOptionCommand },
	};

	[self initCommands];

	return self;
}

- (void)initCommands {
	for (const auto &c : _commands) {
		c.command.enabled = c.lastEnabled;
	}

	const auto center = CommandCenter();
	const auto selector = @selector(onCommand:);
	[center.pauseCommand addTarget:self action:selector];
	[center.playCommand addTarget:self action:selector];
	[center.stopCommand addTarget:self action:selector];
	[center.togglePlayPauseCommand addTarget:self action:selector];
	[center.nextTrackCommand addTarget:self action:selector];
	[center.previousTrackCommand addTarget:self action:selector];

	[center.changePlaybackPositionCommand
		addTarget:self
		action:@selector(onSeek:)];
}

- (void)clearCommands {
	const auto center = CommandCenter();

	for (auto &c : _commands) {
		c.lastEnabled = c.command.enabled;
		c.command.enabled = false;
	}

	[center.pauseCommand removeTarget:self];
	[center.playCommand removeTarget:self];
	[center.stopCommand removeTarget:self];
	[center.togglePlayPauseCommand removeTarget:self];
	[center.nextTrackCommand removeTarget:self];
	[center.previousTrackCommand removeTarget:self];
	[center.changePlaybackPositionCommand removeTarget:self];
}

- (rpl::producer<Command>)commandRequests {
	return _commandRequests.events();
}

- (rpl::producer<int>)seekRequests {
	return _seekRequests.events();
}

- (MPRemoteCommandHandlerStatus)onCommand:(MPRemoteCommandEvent*)event {
	base::Integration::Instance().enterFromEventLoop([&] {
		self->_commandRequests.fire_copy(EventToCommand(event));
	});

	return MPRemoteCommandHandlerStatusSuccess;
}

- (MPRemoteCommandHandlerStatus)onSeek:(
		MPChangePlaybackPositionCommandEvent*)event {
	base::Integration::Instance().enterFromEventLoop([&] {
		self->_seekRequests.fire(event.positionTime * 1000);
	});

	return MPRemoteCommandHandlerStatusSuccess;
}

- (void)dealloc {
	[self clearCommands];
	[super dealloc];
}

@end // @@implementation CommandHandler

namespace base::Platform {

struct SystemMediaControls::Private {
	Private(
		not_null<NSMutableDictionary*> info,
		not_null<CommandHandler*> commandHandler)
	: info(info)
	, commandHandler(commandHandler) {
	}

	[[nodiscard]] float64 duration() const {
		return ((NSNumber*)[info
			objectForKey:MPMediaItemPropertyPlaybackDuration]).doubleValue;
	}

	const not_null<NSMutableDictionary*> info;
	const not_null<CommandHandler*> commandHandler;
	bool enabled = false;
};

SystemMediaControls::SystemMediaControls()
: _private(std::make_unique<Private>(
	[[NSMutableDictionary alloc] init],
	[[CommandHandler alloc] init])) {
}

SystemMediaControls::~SystemMediaControls() {
	setEnabled(false);
	[_private->info release];
	[_private->commandHandler release];
}

bool SystemMediaControls::init(std::optional<QWidget*> parent) {
	clearMetadata();
	updateDisplay();

	return true;
}

void SystemMediaControls::setServiceName(const QString &name) {
}

void SystemMediaControls::setApplicationName(const QString &name) {
}

void SystemMediaControls::setEnabled(bool enabled) {
	if (_private->enabled == enabled) {
		return;
	}
	_private->enabled = enabled;
	if (enabled) {
		[_private->commandHandler initCommands];
	} else {
		[_private->commandHandler clearCommands];
	}
	updateDisplay();
}

void SystemMediaControls::setIsNextEnabled(bool value) {
	CommandCenter().nextTrackCommand.enabled = value;
}

void SystemMediaControls::setIsPreviousEnabled(bool value) {
	CommandCenter().previousTrackCommand.enabled = value;
}

void SystemMediaControls::setIsPlayPauseEnabled(bool value) {
	CommandCenter().togglePlayPauseCommand.enabled = value;
}

void SystemMediaControls::setIsStopEnabled(bool value) {
	CommandCenter().stopCommand.enabled = value;
}

void SystemMediaControls::setPlaybackStatus(
		SystemMediaControls::PlaybackStatus status) {
	[MPNowPlayingInfoCenter defaultCenter].playbackState =
		ConvertPlaybackStatus(status);
}

void SystemMediaControls::setLoopStatus(LoopStatus status) {
}

void SystemMediaControls::setShuffle(bool value) {
}

void SystemMediaControls::setTitle(const QString &title) {
	[_private->info
		setObject:Q2NSString(title)
		forKey:MPMediaItemPropertyTitle];
}

void SystemMediaControls::setArtist(const QString &artist) {
	[_private->info
		setObject:Q2NSString(artist)
		forKey:MPMediaItemPropertyArtist];
}

void SystemMediaControls::setThumbnail(const QImage &thumbnail) {
	if (thumbnail.isNull()) {
		return;
	}
	if (@available(macOS 10.13.2, *)) {
		const auto copy = thumbnail;
		[_private->info
			setObject:[[[MPMediaItemArtwork alloc]
				initWithBoundsSize:CGSizeMake(copy.width(), copy.height())
				requestHandler:^NSImage *(CGSize size) {
					return Q2NSImage(copy.scaled(
						int(size.width),
						int(size.height)));
				}] autorelease]
			forKey:MPMediaItemPropertyArtwork];
		updateDisplay();
	}
}

void SystemMediaControls::setDuration(int duration) {
	[_private->info
		setObject:[NSNumber numberWithDouble:(duration / 1000.)]
		forKey:MPMediaItemPropertyPlaybackDuration];
}

void SystemMediaControls::setPosition(int position) {
	[_private->info
		setObject:[NSNumber numberWithDouble:(position / 1000.)]
		forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
}

void SystemMediaControls::setVolume(float64 volume) {
}

void SystemMediaControls::clearThumbnail() {
	if (@available(macOS 10.13.2, *)) {
		[_private->info removeObjectForKey:MPMediaItemPropertyArtwork];
		updateDisplay();
	}
}

void SystemMediaControls::clearMetadata() {
	const auto zeroNumber = [NSNumber numberWithInt:0];
	const auto oneNumber = [NSNumber numberWithInt:1];
	const auto &info = _private->info;
	[info
		setObject:zeroNumber
		forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
	[info setObject:zeroNumber forKey:MPMediaItemPropertyPlaybackDuration];
	[info setObject:oneNumber forKey:MPNowPlayingInfoPropertyPlaybackRate];
	[info
		setObject:oneNumber
		forKey:MPNowPlayingInfoPropertyDefaultPlaybackRate];
	[info setObject:@"" forKey:MPMediaItemPropertyTitle];
	[info setObject:@"" forKey:MPMediaItemPropertyArtist];

	[info
		setObject:@(MPNowPlayingInfoMediaTypeAudio)
		forKey:MPNowPlayingInfoPropertyMediaType];
}

void SystemMediaControls::updateDisplay() {
	[[MPNowPlayingInfoCenter defaultCenter]
		performSelectorOnMainThread:@selector(setNowPlayingInfo:)
		withObject:((_private->enabled && _private->duration())
			? _private->info
			: nil)
		waitUntilDone:false];
}

auto SystemMediaControls::commandRequests() const
-> rpl::producer<SystemMediaControls::Command> {
	return [_private->commandHandler commandRequests];
}

rpl::producer<float64> SystemMediaControls::seekRequests() const {
	return (
		[_private->commandHandler seekRequests]
	) | rpl::map([=](int position) {
		return float64(position) / (_private->duration() * 1000);
	});
}

rpl::producer<float64> SystemMediaControls::volumeChangeRequests() const {
	return rpl::never<float64>();
}

rpl::producer<> SystemMediaControls::updatePositionRequests() const {
	return rpl::never<>();
}

bool SystemMediaControls::seekingSupported() const {
	return true;
}

bool SystemMediaControls::volumeSupported() const {
	return false;
}

bool SystemMediaControls::Supported() {
	if (@available(macOS 10.12.2, *)) {
		return true;
	}
	return false;
}

} // namespace base::Platform
