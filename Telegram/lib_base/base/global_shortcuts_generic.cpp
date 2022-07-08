// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/global_shortcuts_generic.h"

#include "base/platform/base_platform_global_shortcuts.h"
#include "base/invoke_queued.h"

namespace base {
namespace {

constexpr auto kShortcutLimit = 4;

std::mutex GlobalMutex;
std::vector<not_null<GlobalShortcutManagerGeneric*>> Managers;

[[nodiscard]] GlobalShortcut MakeShortcut(
		std::vector<GlobalShortcutKeyGeneric> descriptors) {
	return std::make_shared<GlobalShortcutValueGeneric>(
		std::move(descriptors));
}

[[nodiscard]] bool Matches(
		const std::vector<GlobalShortcutKeyGeneric> &sorted,
		const flat_set<GlobalShortcutKeyGeneric> &down) {
	if (sorted.size() > down.size()) {
		return false;
	}
	auto j = begin(down);
	for (const auto descriptor : sorted) {
		while (true) {
			if (*j > descriptor) {
				return false;
			} else if (*j < descriptor) {
				++j;
				if (j == end(down)) {
					return false;
				}
			} else {
				break;
			}
		}
	}
	return true;
}

void ScheduleForAll(GlobalShortcutKeyGeneric descriptor, bool down) {
	std::unique_lock lock{ GlobalMutex };
	for (const auto manager : Managers) {
		manager->schedule(descriptor, down);
	}
}

} // namespace

std::unique_ptr<GlobalShortcutManager> CreateGlobalShortcutManager() {
	return std::make_unique<GlobalShortcutManagerGeneric>();
}

bool GlobalShortcutsAvailable() {
	return Platform::GlobalShortcuts::Available();
}

bool GlobalShortcutsAllowed() {
	return Platform::GlobalShortcuts::Allowed();
}

GlobalShortcutValueGeneric::GlobalShortcutValueGeneric(
	std::vector<GlobalShortcutKeyGeneric> descriptors)
: _descriptors(std::move(descriptors)) {
	Expects(!_descriptors.empty());
}

QString GlobalShortcutValueGeneric::toDisplayString() {
	auto result = QStringList();
	result.reserve(_descriptors.size());
	for (const auto descriptor : _descriptors) {
		result.push_back(Platform::GlobalShortcuts::KeyName(descriptor));
	}
	return result.join(" + ");
}

QByteArray GlobalShortcutValueGeneric::serialize() {
	static_assert(sizeof(GlobalShortcutKeyGeneric) == sizeof(uint64));

	const auto size = sizeof(GlobalShortcutKeyGeneric) * _descriptors.size();
	auto result = QByteArray(size, Qt::Uninitialized);
	memcpy(result.data(), _descriptors.data(), size);
	return result;
}

GlobalShortcutManagerGeneric::GlobalShortcutManagerGeneric() {
	std::unique_lock lock{ GlobalMutex };
	const auto start = Managers.empty();
	Managers.push_back(this);
	lock.unlock();

	if (start) {
		Platform::GlobalShortcuts::Start(ScheduleForAll);
	}
}

GlobalShortcutManagerGeneric::~GlobalShortcutManagerGeneric() {
	std::unique_lock lock{ GlobalMutex };
	Managers.erase(ranges::remove(Managers, not_null{ this }), end(Managers));
	const auto stop = Managers.empty();
	lock.unlock();

	if (stop) {
		Platform::GlobalShortcuts::Stop();
	}
}

void GlobalShortcutManagerGeneric::startRecording(
		Fn<void(GlobalShortcut)> progress,
		Fn<void(GlobalShortcut)> done) {
	Expects(done != nullptr);

	_recordingDown.clear();
	_recordingUp.clear();
	_recording = true;
	_recordingProgress = std::move(progress);
	_recordingDone = std::move(done);
}

void GlobalShortcutManagerGeneric::stopRecording() {
	_recordingDown.clear();
	_recordingUp.clear();
	_recording = false;
	_recordingDone = nullptr;
	_recordingProgress = nullptr;
}

void GlobalShortcutManagerGeneric::startWatching(
		GlobalShortcut shortcut,
		Fn<void(bool pressed)> callback) {
	Expects(shortcut != nullptr);
	Expects(callback != nullptr);

	const auto i = ranges::find(_watchlist, shortcut, &Watch::shortcut);
	if (i != end(_watchlist)) {
		i->callback = std::move(callback);
	} else {
		auto sorted = static_cast<GlobalShortcutValueGeneric*>(
			shortcut.get())->descriptors();
		std::sort(begin(sorted), end(sorted));
		_watchlist.push_back(Watch{
			std::move(shortcut),
			std::move(sorted),
			std::move(callback)
		});
	}
}

void GlobalShortcutManagerGeneric::stopWatching(GlobalShortcut shortcut) {
	const auto i = ranges::find(_watchlist, shortcut, &Watch::shortcut);
	if (i != end(_watchlist)) {
		_watchlist.erase(i);
	}
	_pressed.erase(ranges::find(_pressed, shortcut), end(_pressed));
}

GlobalShortcut GlobalShortcutManagerGeneric::shortcutFromSerialized(
		QByteArray serialized) {
	const auto single = sizeof(GlobalShortcutKeyGeneric);
	if (serialized.isEmpty() || serialized.size() % single) {
		return nullptr;
	}
	auto count = serialized.size() / single;
	auto list = std::vector<GlobalShortcutKeyGeneric>(count);
	memcpy(list.data(), serialized.constData(), serialized.size());
	return MakeShortcut(std::move(list));
}

void GlobalShortcutManagerGeneric::schedule(
		GlobalShortcutKeyGeneric descriptor,
		bool down) {
	InvokeQueued(this, [=] { process(descriptor, down); });
}

void GlobalShortcutManagerGeneric::process(
		GlobalShortcutKeyGeneric descriptor,
		bool down) {
	if (!down) {
		_down.remove(descriptor);
	}
	if (_recording) {
		processRecording(descriptor, down);
		return;
	}
	auto scheduled = std::vector<Fn<void(bool pressed)>>();
	if (down) {
		_down.emplace(descriptor);
		for (const auto &watch : _watchlist) {
			if (watch.sorted.size() > _down.size()
				|| ranges::contains(_pressed, watch.shortcut)) {
				continue;
			} else if (Matches(watch.sorted, _down)) {
				_pressed.push_back(watch.shortcut);
				scheduled.push_back(watch.callback);
			}
		}
	} else {
		_down.remove(descriptor);
		for (auto i = begin(_pressed); i != end(_pressed);) {
			const auto generic = static_cast<GlobalShortcutValueGeneric*>(
				i->get());
			if (!ranges::contains(generic->descriptors(), descriptor)) {
				++i;
			} else {
				const auto j = ranges::find(
					_watchlist,
					*i,
					&Watch::shortcut);
				Assert(j != end(_watchlist));
				scheduled.push_back(j->callback);

				i = _pressed.erase(i);
			}
		}
	}
	for (const auto &callback : scheduled) {
		callback(down);
	}
}

void GlobalShortcutManagerGeneric::processRecording(
		GlobalShortcutKeyGeneric descriptor,
		bool down) {
	if (down) {
		processRecordingPress(descriptor);
	} else {
		processRecordingRelease(descriptor);
	}
}

void GlobalShortcutManagerGeneric::processRecordingPress(
		GlobalShortcutKeyGeneric descriptor) {
	auto changed = false;
	_recordingUp.remove(descriptor);
	for (const auto descriptor : _recordingUp) {
		const auto i = ranges::remove(_recordingDown, descriptor);
		if (i != end(_recordingDown)) {
			_recordingDown.erase(i, end(_recordingDown));
			changed = true;
		}
	}
	_recordingUp.clear();

	const auto i = std::find(
		begin(_recordingDown),
		end(_recordingDown),
		descriptor);
	if (i == _recordingDown.end()) {
		_recordingDown.push_back(descriptor);
		changed = true;
	}
	if (!changed) {
		return;
	} else if (_recordingDown.size() == kShortcutLimit) {
		finishRecording();
	} else if (const auto onstack = _recordingProgress) {
		onstack(MakeShortcut(_recordingDown));
	}
}

void GlobalShortcutManagerGeneric::processRecordingRelease(
		GlobalShortcutKeyGeneric descriptor) {
	const auto i = std::find(
		begin(_recordingDown),
		end(_recordingDown),
		descriptor);
	if (i == end(_recordingDown)) {
		return;
	}
	_recordingUp.emplace(descriptor);
	Assert(_recordingUp.size() <= _recordingDown.size());
	if (_recordingUp.size() == _recordingDown.size()) {
		// All keys are up, we got the shortcut.
		// Some down keys are not up yet.
		finishRecording();
	}
}

void GlobalShortcutManagerGeneric::finishRecording() {
	Expects(!_recordingDown.empty());

	auto result = MakeShortcut(std::move(_recordingDown));
	_recordingDown.clear();
	_recordingUp.clear();
	_recording = false;
	const auto done = _recordingDone;
	_recordingDone = nullptr;
	_recordingProgress = nullptr;

	done(std::move(result));
}

} // namespace base
