// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/mac/base_power_save_blocker_mac.h"

#include "base/debug_log.h"

#include <crl/crl_object_on_thread.h>

// Thanks Chromium: services/device/wake_lock/power_save_blocker

#include <IOKit/pwr_mgt/IOPMLib.h>

namespace base::Platform {
namespace {

// Power management cannot be done on the UI thread. IOPMAssertionCreate does a
// synchronous MIG call to configd, so if it is called on the main thread the UI
// is at the mercy of another process. See http://crbug.com/79559 and
// http://www.opensource.apple.com/source/IOKitUser/IOKitUser-514.16.31/pwr_mgt.subproj/IOPMLibPrivate.c .

class BlockManager final {
public:
	explicit BlockManager(crl::weak_on_thread<BlockManager> weak);

	void block(PowerSaveBlockType type, const QString &description);
	void unblock(PowerSaveBlockType type);

private:
	crl::weak_on_thread<BlockManager> _weak;
	IOPMAssertionID _assertions[kPowerSaveBlockTypeCount] = {};

};

BlockManager::BlockManager(crl::weak_on_thread<BlockManager> weak)
: _weak(weak) {
}

void BlockManager::block(PowerSaveBlockType type, const QString &description) {
	const auto level = CFStringRef([&] {
		// See QA1340 <http://developer.apple.com/library/mac/#qa/qa1340/> for more
		// details.
		switch (type) {
		case PowerSaveBlockType::PreventAppSuspension:
			return kIOPMAssertionTypeNoIdleSleep;
		case PowerSaveBlockType::PreventDisplaySleep:
			return kIOPMAssertionTypeNoDisplaySleep;
		}
		Unexpected("Type in BlockManager::block.");
	}());
	const auto reason = description.toCFString();
	IOReturn result = IOPMAssertionCreateWithName(
		level,
		kIOPMAssertionLevelOn,
		reason,
		&_assertions[PowerSaveBlockTypeIndex(type)]);
	CFRelease(reason);
	if (result != kIOReturnSuccess) {
		LOG(("System Error: IOPMAssertionCreate: %1").arg(result));
	}
}

void BlockManager::unblock(PowerSaveBlockType type) {
	const auto index = PowerSaveBlockTypeIndex(type);
	if (_assertions[index] != kIOPMNullAssertionID) {
		IOReturn result = IOPMAssertionRelease(_assertions[index]);
		_assertions[index] = kIOPMNullAssertionID;
		if (result != kIOReturnSuccess) {
			LOG(("System Error: IOPMAssertionRelease: %1").arg(result));
		}
	}
}

[[nodiscard]] crl::object_on_thread<BlockManager> &Manager() {
	static auto result = crl::object_on_thread<BlockManager>();
	return result;
}

} // namespace

void BlockPowerSave(
		PowerSaveBlockType type,
		const QString &description,
		QPointer<QWindow> window) {
	Manager().with([=](BlockManager &instance) {
		instance.block(type, description);
	});
}

void UnblockPowerSave(PowerSaveBlockType type, QPointer<QWindow> window) {
	Manager().with([=](BlockManager &instance) {
		instance.unblock(type);
	});
}

} // namespace base::Platform
