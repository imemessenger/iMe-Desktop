// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/global_shortcuts.h"

namespace base {

using GlobalShortcutKeyGeneric = uint64;

class GlobalShortcutValueGeneric final : public GlobalShortcutValue {
public:
	GlobalShortcutValueGeneric(
		std::vector<GlobalShortcutKeyGeneric> descriptors);

	QString toDisplayString() override;
	QByteArray serialize() override;

	const std::vector<GlobalShortcutKeyGeneric> &descriptors() const {
		return _descriptors;
	}

private:
	std::vector<GlobalShortcutKeyGeneric> _descriptors;

};

class GlobalShortcutManagerGeneric final
	: public GlobalShortcutManager
	, public QObject {
public:
	GlobalShortcutManagerGeneric();
	~GlobalShortcutManagerGeneric();

	void startRecording(
		Fn<void(GlobalShortcut)> progress,
		Fn<void(GlobalShortcut)> done) override;
	void stopRecording() override;
	void startWatching(
		GlobalShortcut shortcut,
		Fn<void(bool pressed)> callback) override;
	void stopWatching(GlobalShortcut shortcut) override;

	GlobalShortcut shortcutFromSerialized(QByteArray serialized) override;

	// Thread-safe.
	void schedule(GlobalShortcutKeyGeneric descriptor, bool down);

private:
	struct Watch {
		GlobalShortcut shortcut;
		std::vector<GlobalShortcutKeyGeneric> sorted;
		Fn<void(bool pressed)> callback;
	};
	void process(GlobalShortcutKeyGeneric descriptor, bool down);
	void processRecording(GlobalShortcutKeyGeneric descriptor, bool down);
	void processRecordingPress(GlobalShortcutKeyGeneric descriptor);
	void processRecordingRelease(GlobalShortcutKeyGeneric descriptor);
	void finishRecording();

	Fn<void(GlobalShortcut)> _recordingProgress;
	Fn<void(GlobalShortcut)> _recordingDone;
	std::vector<GlobalShortcutKeyGeneric> _recordingDown;
	flat_set<GlobalShortcutKeyGeneric> _recordingUp;
	flat_set<GlobalShortcutKeyGeneric> _down;
	std::vector<Watch> _watchlist;
	std::vector<GlobalShortcut> _pressed;

	bool _recording = false;

};

} // namespace base
