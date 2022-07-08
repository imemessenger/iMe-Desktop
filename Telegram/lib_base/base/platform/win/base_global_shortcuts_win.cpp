// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/win/base_global_shortcuts_win.h"

#include "base/platform/win/base_windows_h.h"

namespace base::Platform::GlobalShortcuts {
namespace {

constexpr auto kShiftMouseButton = std::numeric_limits<uint64>::max() - 100;

HHOOK GlobalHookKeyboard = nullptr;
HHOOK GlobalHookMouse = nullptr;
HANDLE ThreadHandle = nullptr;
HANDLE ThreadEvent = nullptr;
DWORD ThreadId = 0;
Fn<void(GlobalShortcutKeyGeneric descriptor, bool down)> ProcessCallback;

[[nodiscard]] GlobalShortcutKeyGeneric MakeDescriptor(
		uint32 virtualKeyCode,
		uint32 lParam) {
	return GlobalShortcutKeyGeneric(
		(uint64(virtualKeyCode) << 32) | uint64(lParam));
}

[[nodiscard]] GlobalShortcutKeyGeneric MakeMouseDescriptor(uint8 button) {
	Expects(button > 0 && button < 100);

	return kShiftMouseButton + button;
}

[[nodiscard]] uint32 GetVirtualKeyCode(GlobalShortcutKeyGeneric descriptor) {
	return uint32(uint64(descriptor) >> 32);
}

[[nodiscard]] uint32 GetLParam(GlobalShortcutKeyGeneric descriptor) {
	return uint32(uint64(descriptor) & 0xFFFFFFFFULL);
}

void ProcessHookedKeyboardEvent(WPARAM wParam, LPARAM lParam) {
	const auto press = (PKBDLLHOOKSTRUCT)lParam;
	const auto repeatCount = uint32(0);
	const auto extendedBit = ((press->flags & LLKHF_EXTENDED) != 0);
	const auto contextBit = ((press->flags & LLKHF_ALTDOWN) != 0);
	const auto transitionState = ((press->flags & LLKHF_UP) != 0);
	const auto lParamForEvent = (repeatCount & 0x0000FFFFU)
		| ((uint32(press->scanCode) & 0xFFU) << 16)
		| (extendedBit ? (uint32(KF_EXTENDED) << 16) : 0);
		//| (contextBit ? (uint32(KF_ALTDOWN) << 16) : 0); // Alt pressed.
		//| (transitionState ? (uint32(KF_UP) << 16) : 0); // Is pressed.
	const auto descriptor = MakeDescriptor(press->vkCode, lParamForEvent);
	const auto down = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);

	ProcessCallback(descriptor, down);
}

void ProcessHookedMouseEvent(WPARAM wParam, LPARAM lParam) {
	if (wParam != WM_RBUTTONDOWN
		&& wParam != WM_RBUTTONUP
		&& wParam != WM_MBUTTONDOWN
		&& wParam != WM_MBUTTONUP
		&& wParam != WM_XBUTTONDOWN
		&& wParam != WM_XBUTTONUP) {
		return;
	}
	const auto button = [&] {
		if (wParam == WM_RBUTTONDOWN || wParam == WM_RBUTTONUP) {
			return 2;
		} else if (wParam == WM_MBUTTONDOWN || wParam == WM_MBUTTONUP) {
			return 3;
		}
		const auto press = (PMSLLHOOKSTRUCT)lParam;
		const auto xbutton = ((press->mouseData >> 16) & 0xFFU);
		return (xbutton >= 0x01 && xbutton <= 0x18) ? int(xbutton + 3) : 0;
	}();
	if (!button) {
		return;
	}
	const auto descriptor = MakeMouseDescriptor(button);
	const auto down = (wParam == WM_RBUTTONDOWN)
		|| (wParam == WM_MBUTTONDOWN)
		|| (wParam == WM_XBUTTONDOWN);

	ProcessCallback(descriptor, down);
}

LRESULT CALLBACK LowLevelKeyboardProc(
		_In_ int nCode,
		_In_ WPARAM wParam,
		_In_ LPARAM lParam) {
	if (nCode == HC_ACTION) {
		ProcessHookedKeyboardEvent(wParam, lParam);
	}
	return CallNextHookEx(GlobalHookKeyboard, nCode, wParam, lParam);
}

LRESULT CALLBACK LowLevelMouseProc(
		_In_ int nCode,
		_In_ WPARAM wParam,
		_In_ LPARAM lParam) {
	if (nCode == HC_ACTION) {
		ProcessHookedMouseEvent(wParam, lParam);
	}
	return CallNextHookEx(GlobalHookMouse, nCode, wParam, lParam);
}

DWORD WINAPI RunThread(LPVOID) {
	auto message = MSG();

	// Force message loop creation.
	PeekMessage(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
	SetEvent(ThreadEvent);

	const auto guard = gsl::finally([&] {
		if (GlobalHookKeyboard) {
			UnhookWindowsHookEx(GlobalHookKeyboard);
			GlobalHookKeyboard = nullptr;
		}
		if (GlobalHookMouse) {
			UnhookWindowsHookEx(GlobalHookMouse);
			GlobalHookMouse = nullptr;
		}
	});
	GlobalHookKeyboard = SetWindowsHookEx(
		WH_KEYBOARD_LL,
		LowLevelKeyboardProc,
		nullptr,
		0);
	GlobalHookMouse = SetWindowsHookEx(
		WH_MOUSE_LL,
		LowLevelMouseProc,
		nullptr,
		0);
	if (!GlobalHookKeyboard || !GlobalHookMouse) {
		return -1;
	}

	while (GetMessage(&message, nullptr, 0, 0)) {
		if (message.message == WM_QUIT) {
			break;
		}
		TranslateMessage(&message);
		DispatchMessage(&message);
	}
	return 0;
}

} // namespace

bool Available() {
	return true;
}

bool Allowed() {
	return true;
}

void Start(Fn<void(GlobalShortcutKeyGeneric descriptor, bool down)> process) {
	Expects(!ThreadHandle);
	Expects(!ThreadId);

	ProcessCallback = std::move(process);
	ThreadEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!ThreadEvent) {
		ProcessCallback = nullptr;
		return;
	}
	ThreadHandle = CreateThread(
		nullptr,
		0,
		&RunThread,
		nullptr,
		0,
		&ThreadId);
	if (!ThreadHandle) {
		CloseHandle(ThreadEvent);
		ThreadEvent = nullptr;
		ThreadId = 0;
		ProcessCallback = nullptr;
	}
}

void Stop() {
	if (!ThreadHandle) {
		return;
	}
	WaitForSingleObject(ThreadEvent, INFINITE);
	PostThreadMessage(ThreadId, WM_QUIT, 0, 0);
	WaitForSingleObject(ThreadHandle, INFINITE);
	CloseHandle(ThreadHandle);
	CloseHandle(ThreadEvent);
	ThreadHandle = ThreadEvent = nullptr;
	ThreadId = 0;
	ProcessCallback = nullptr;
}

QString KeyName(GlobalShortcutKeyGeneric descriptor) {
	if (descriptor > kShiftMouseButton) {
		return QString("Mouse %1").arg(descriptor - kShiftMouseButton);
	}

	constexpr auto kLimit = 1024;

	WCHAR buffer[kLimit + 1] = { 0 };

	// Remove 25 bit, we want to differentiate between left and right Ctrl-s.
	auto lParam = LONG(GetLParam(descriptor) & ~(1U << 25));

	return GetKeyNameText(lParam, buffer, kLimit)
		? QString::fromWCharArray(buffer)
		: (GetVirtualKeyCode(descriptor) == VK_RSHIFT)
		? QString("Right Shift")
		: QString("\\x%1").arg(GetVirtualKeyCode(descriptor), 0, 16);
}

} // namespace base::Platform::GlobalShortcuts
