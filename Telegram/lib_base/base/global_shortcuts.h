// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

namespace base {

class GlobalShortcutValue {
public:
	[[nodiscard]] virtual QString toDisplayString() = 0;
	[[nodiscard]] virtual QByteArray serialize() = 0;

	virtual ~GlobalShortcutValue() = default;
};

using GlobalShortcut = std::shared_ptr<GlobalShortcutValue>;

// Callbacks are called from unspecified thread.
class GlobalShortcutManager {
public:
	virtual void startRecording(
		Fn<void(GlobalShortcut)> progress,
		Fn<void(GlobalShortcut)> done) = 0;
	virtual void stopRecording() = 0;
	virtual void startWatching(
		GlobalShortcut shortcut,
		Fn<void(bool pressed)> callback) = 0;
	virtual void stopWatching(GlobalShortcut shortcut) = 0;

	[[nodiscard]] virtual GlobalShortcut shortcutFromSerialized(
		QByteArray serialized) = 0;

	virtual ~GlobalShortcutManager() = default;
};

[[nodiscard]] bool GlobalShortcutsAvailable();
[[nodiscard]] bool GlobalShortcutsAllowed();
[[nodiscard]] std::unique_ptr<GlobalShortcutManager> CreateGlobalShortcutManager();

} // namespace base
