// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/win/base_power_save_blocker_win.h"

#include "base/platform/win/base_windows_h.h"
#include "base/platform/base_platform_info.h"

// Thanks Chromium: services/device/wake_lock/power_save_blocker

namespace base::Platform {
namespace {

using namespace ::Platform;

HANDLE GlobalHandles[kPowerSaveBlockTypeCount];

HANDLE CreatePowerRequest(
		POWER_REQUEST_TYPE type,
		const QString &description) {
	if (type == PowerRequestExecutionRequired && !IsWindows8OrGreater()) {
		return INVALID_HANDLE_VALUE;
	}

	auto context = REASON_CONTEXT{ 0 };
	context.Version = POWER_REQUEST_CONTEXT_VERSION;
	context.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;

	auto wide = description.toStdWString();
	wide.push_back(wchar_t(0)); // So that .data() will be 0-terminated.
	context.Reason.SimpleReasonString = wide.data();

	const auto handle = ::PowerCreateRequest(&context);
	if (!handle || handle == INVALID_HANDLE_VALUE) {
		return INVALID_HANDLE_VALUE;
	}

	if (::PowerSetRequest(handle, type)) {
		return handle;
	}

	// Something went wrong.
	CloseHandle(handle);
	return INVALID_HANDLE_VALUE;
}

// Takes ownership of the |handle|.
void DeletePowerRequest(POWER_REQUEST_TYPE type, HANDLE handle) {
	if (!handle || handle == INVALID_HANDLE_VALUE) {
		return;
	} else if (type == PowerRequestExecutionRequired
		&& !IsWindows8OrGreater()) {
		return;
	}
	::PowerClearRequest(handle, type);
	CloseHandle(handle);
}

POWER_REQUEST_TYPE RequestType(PowerSaveBlockType type) {
	if (type == PowerSaveBlockType::PreventDisplaySleep) {
		return PowerRequestDisplayRequired;
	} else if (!IsWindows8OrGreater()) {
		return PowerRequestSystemRequired;
	}
	return PowerRequestExecutionRequired;
}

}  // namespace

void BlockPowerSave(
		PowerSaveBlockType type,
		const QString &description,
		QPointer<QWindow> window) {
	Expects(!GlobalHandles[PowerSaveBlockTypeIndex(type)]);

	const auto requestType = RequestType(type);
	GlobalHandles[PowerSaveBlockTypeIndex(type)] = CreatePowerRequest(
		requestType,
		description);
}

void UnblockPowerSave(PowerSaveBlockType type, QPointer<QWindow> window) {
	DeletePowerRequest(
		RequestType(type),
		base::take(GlobalHandles[PowerSaveBlockTypeIndex(type)]));
}

} // namespace base::Platform
