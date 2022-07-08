// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/power_save_blocker.h"

#include "base/platform/base_platform_power_save_blocker.h"

#include <QtCore/QMutex>
#include <QtGui/QWindow>

namespace base {
namespace {

struct BlockersSet {
	flat_set<not_null<PowerSaveBlocker*>> list;
	QPointer<QWindow> window;
	QString description;
};

flat_map<PowerSaveBlockType, BlockersSet> Blockers;
QMutex Mutex;

void Add(not_null<PowerSaveBlocker*> blocker) {
	const auto lock = QMutexLocker(&Mutex);
	auto &set = Blockers[blocker->type()];
	if (set.list.empty()) {
		set.description = blocker->description();
		set.window = blocker->window();
		Platform::BlockPowerSave(
			blocker->type(),
			set.description,
			set.window);
	}
	set.list.emplace(blocker);
}

void Remove(not_null<PowerSaveBlocker*> blocker) {
	const auto lock = QMutexLocker(&Mutex);
	auto &set = Blockers[blocker->type()];
	set.list.remove(blocker);
	if (set.description != blocker->description()
		|| set.window != blocker->window()) {
		return;
	}
	Platform::UnblockPowerSave(blocker->type(), blocker->window());
	if (!set.list.empty()) {
		const auto good = ranges::find_if(set.list, [](
				not_null<PowerSaveBlocker*> blocker) {
			return blocker->window() != nullptr;
		});
		const auto use = (good != end(set.list)) ? good : begin(set.list);
		set.description = (*use)->description();
		set.window = (*use)->window();
		Platform::BlockPowerSave(
			blocker->type(),
			set.description,
			set.window);
	}
}

} // namespace

PowerSaveBlocker::PowerSaveBlocker(
	PowerSaveBlockType type,
	const QString &description,
	QWindow *window)
: _type(type)
, _description(description)
, _window(window) {
	Add(this);
}

QPointer<QWindow> PowerSaveBlocker::window() const {
	return _window;
}

PowerSaveBlocker::~PowerSaveBlocker() {
	Remove(this);
}

} // namespace base
