// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/win/base_process_win.h"

#include "base/platform/win/base_windows_h.h"

namespace base::Platform {
namespace {

struct FullWindowId {
	int64 pid = 0;
	WId windowId = 0;
};

BOOL CALLBACK ActivateProcess(HWND hWnd, LPARAM lParam) {
	const auto fullId = reinterpret_cast<const FullWindowId*>(lParam);
	if (reinterpret_cast<WId>(hWnd) != fullId->windowId) {
		return TRUE;
	}

	DWORD dwProcessId;
	::GetWindowThreadProcessId(hWnd, &dwProcessId);

	if (static_cast<int64>(dwProcessId) != fullId->pid) {
		return TRUE;
	}
	::SetForegroundWindow(hWnd);
	::SetFocus(hWnd);
	return FALSE;
}

} // namespace

void ActivateProcessWindow(int64 pid, WId windowId) {
	const auto fullId = FullWindowId{ pid, windowId };
	::EnumWindows(
		reinterpret_cast<WNDENUMPROC>(ActivateProcess),
		reinterpret_cast<LPARAM>(&fullId));
}

void ActivateThisProcessWindow(WId windowId) {
	if (const auto handle = reinterpret_cast<HWND>(windowId)) {
		::SetFocus(handle);
	}
}

} // namespace base::Platform
