// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

class QWidget;

namespace base::Platform {

class SystemMediaControls {
public:
	enum class Command {
		PlayPause,
		Play,
		Pause,
		Next,
		Previous,
		Stop,
		Quit,
		Raise,
		LoopNone,
		LoopTrack,
		LoopPlaylist,
		Shuffle,
		None,
	};

	enum class PlaybackStatus {
		Playing,
		Paused,
		Stopped,
	};

	enum class LoopStatus {
		None,
		Track,
		Playlist,
	};

	SystemMediaControls();
	~SystemMediaControls();

	bool init(std::optional<QWidget*> parent);

	[[nodiscard]] bool seekingSupported() const;
	[[nodiscard]] bool volumeSupported() const;

	void setServiceName(const QString &name);
	void setApplicationName(const QString &name);

	void setEnabled(bool enabled);
	void setIsNextEnabled(bool value);
	void setIsPreviousEnabled(bool value);
	void setIsPlayPauseEnabled(bool value);
	void setIsStopEnabled(bool value);
	void setPlaybackStatus(PlaybackStatus status);
	void setLoopStatus(LoopStatus status);
	void setShuffle(bool value);
	void setTitle(const QString &title);
	void setArtist(const QString &artist);
	void setThumbnail(const QImage &thumbnail);
	void setDuration(int duration);
	void setPosition(int position);
	void setVolume(float64 volume);
	void clearThumbnail();
	void clearMetadata();
	void updateDisplay();

	[[nodiscard]] rpl::producer<Command> commandRequests() const;
	[[nodiscard]] rpl::producer<float64> seekRequests() const;
	[[nodiscard]] rpl::producer<float64> volumeChangeRequests() const;
	[[nodiscard]] rpl::producer<> updatePositionRequests() const;

	static bool Supported();

private:
	struct Private;

	const std::unique_ptr<Private> _private;
};

} // namespace base::Platform
