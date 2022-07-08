// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/mac/base_global_shortcuts_mac.h"

#include "base/platform/mac/base_utilities_mac.h"
#include "base/invoke_queued.h"
#include "base/const_string.h"

#include <Carbon/Carbon.h>
#import <Foundation/Foundation.h>
#import <IOKit/hidsystem/IOHIDLib.h>

namespace base::Platform::GlobalShortcuts {
namespace {

constexpr auto kShiftMouseButton = std::numeric_limits<uint64>::max() - 100;

CFMachPortRef EventPort = nullptr;
CFRunLoopSourceRef EventPortSource = nullptr;
CFRunLoopRef ThreadRunLoop = nullptr;
std::thread Thread;
Fn<void(GlobalShortcutKeyGeneric descriptor, bool down)> ProcessCallback;

struct EventData {
	GlobalShortcutKeyGeneric descriptor = 0;
	bool down = false;
};
using MaybeEventData = std::optional<EventData>;

MaybeEventData ProcessKeyEvent(CGEventType type, CGEventRef event) {
	if (CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat)) {
		return std::nullopt;
	}

	const auto keycode = CGEventGetIntegerValueField(
		event,
		kCGKeyboardEventKeycode);

	if (keycode == 0xB3) {
		// Some KeyDown+KeyUp sent when quickly pressing and releasing Fn.
		return std::nullopt;
	}

	const auto flags = CGEventGetFlags(event);
	const auto maybeDown = [&]() -> std::optional<bool> {
		if (type == kCGEventKeyDown) {
			return true;
		} else if (type == kCGEventKeyUp) {
			return false;
		} else if (type != kCGEventFlagsChanged) {
			return std::nullopt;
		}
		switch (keycode) {
		case kVK_CapsLock:
			return (flags & kCGEventFlagMaskAlphaShift) != 0;
		case kVK_Shift:
		case kVK_RightShift:
			return (flags & kCGEventFlagMaskShift) != 0;
		case kVK_Control:
		case kVK_RightControl:
			return (flags & kCGEventFlagMaskControl) != 0;
		case kVK_Option:
		case kVK_RightOption:
			return (flags & kCGEventFlagMaskAlternate) != 0;
		case kVK_Command:
		case kVK_RightCommand:
			return (flags & kCGEventFlagMaskCommand) != 0;
		case kVK_Function:
			return (flags & kCGEventFlagMaskSecondaryFn) != 0;
		default:
			return std::nullopt;
		}
	}();
	if (!maybeDown) {
		return std::nullopt;
	}
	const auto descriptor = GlobalShortcutKeyGeneric(keycode);
	const auto down = *maybeDown;

	return EventData{ descriptor, down };
}

MaybeEventData ProcessMouseEvent(CGEventType type, CGEventRef event) {
	const auto button = CGEventGetIntegerValueField(
		event,
		kCGMouseEventButtonNumber);
	if (!button) {
		return std::nullopt;
	}
	const auto code = GlobalShortcutKeyGeneric(kShiftMouseButton
		+ button
		// Increase the value by 1, because the right button = 1.
		+ 1);

	const auto down = (type == kCGEventOtherMouseDown)
		|| (type == kCGEventRightMouseDown);

	return EventData{ code, down };
}

CGEventRef EventTapCallback(
		CGEventTapProxy,
		CGEventType type,
		CGEventRef event,
		void*) {
	const auto isKey = (type == kCGEventKeyDown)
		|| (type == kCGEventKeyUp)
		|| (type == kCGEventFlagsChanged);

	const auto maybeData = isKey
		? ProcessKeyEvent(type, event)
		: ProcessMouseEvent(type, event);

	if (maybeData) {
		ProcessCallback(maybeData->descriptor, maybeData->down);
	}
	return event;
}

} // namespace

bool Available() {
	return true;
}

bool Allowed() {
	if (@available(macOS 10.15, *)) {
		// Input Monitoring is required on macOS 10.15 an later.
		// Even if user grants access, restart is required.
		static const auto result = IOHIDCheckAccess(
			kIOHIDRequestTypeListenEvent);
		return (result == kIOHIDAccessTypeGranted);
	} else if (@available(macOS 10.14, *)) {
		// Accessibility is required on macOS 10.14.
		NSDictionary *const options=
			@{(__bridge NSString *)kAXTrustedCheckOptionPrompt: @FALSE};
		return AXIsProcessTrustedWithOptions(
			(__bridge CFDictionaryRef)options);
	}
	return true;
}

void Start(Fn<void(GlobalShortcutKeyGeneric descriptor, bool down)> process) {
	Expects(!EventPort);
	Expects(!EventPortSource);

	ProcessCallback = std::move(process);
	EventPort = CGEventTapCreate(
		kCGHIDEventTap,
		kCGHeadInsertEventTap,
		kCGEventTapOptionListenOnly,
		(CGEventMaskBit(kCGEventKeyDown)
			| CGEventMaskBit(kCGEventKeyUp)
			| CGEventMaskBit(kCGEventOtherMouseDown)
			| CGEventMaskBit(kCGEventOtherMouseUp)
			| CGEventMaskBit(kCGEventRightMouseDown)
			| CGEventMaskBit(kCGEventRightMouseUp)
			| CGEventMaskBit(kCGEventFlagsChanged)),
		EventTapCallback,
		nullptr);
	if (!EventPort) {
		ProcessCallback = nullptr;
		return;
	}
	EventPortSource = CFMachPortCreateRunLoopSource(
		kCFAllocatorDefault,
		EventPort,
		0);
	if (!EventPortSource) {
		CFMachPortInvalidate(EventPort);
		CFRelease(EventPort);
		EventPort = nullptr;
		ProcessCallback = nullptr;
		return;
	}
	Thread = std::thread([] {
		ThreadRunLoop = CFRunLoopGetCurrent();
		CFRunLoopAddSource(
			ThreadRunLoop,
			EventPortSource,
			kCFRunLoopCommonModes);
		CGEventTapEnable(EventPort, true);
		CFRunLoopRun();
	});
}

void Stop() {
	if (!EventPort) {
		return;
	}
	CFRunLoopStop(ThreadRunLoop);
	Thread.join();

	CFMachPortInvalidate(EventPort);
	CFRelease(EventPort);
	EventPort = nullptr;

	CFRelease(EventPortSource);
	EventPortSource = nullptr;

	ProcessCallback = nullptr;
}

QString KeyName(GlobalShortcutKeyGeneric descriptor) {
	static const auto KeyToString = flat_map<uint64, const_string>{
		{ kVK_Return, "\xE2\x8F\x8E" },
		{ kVK_Tab, "\xE2\x87\xA5" },
		{ kVK_Space, "\xE2\x90\xA3" },
		{ kVK_Delete, "\xE2\x8C\xAB" },
		{ kVK_Escape, "\xE2\x8E\x8B" },
		{ kVK_Command, "\xE2\x8C\x98" },
		{ kVK_Shift, "\xE2\x87\xA7" },
		{ kVK_CapsLock, "Caps Lock" },
		{ kVK_Option, "\xE2\x8C\xA5" },
		{ kVK_Control, "\xE2\x8C\x83" },
		{ kVK_RightCommand, "Right \xE2\x8C\x98" },
		{ kVK_RightShift, "Right \xE2\x87\xA7" },
		{ kVK_RightOption, "Right \xE2\x8C\xA5" },
		{ kVK_RightControl, "Right \xE2\x8C\x83" },
		{ kVK_Function, "Fn" },
		{ kVK_F17, "F17" },
		{ kVK_VolumeUp, "Volume Up" },
		{ kVK_VolumeDown, "Volume Down" },
		{ kVK_Mute, "Mute" },
		{ kVK_F18, "F18" },
		{ kVK_F19, "F19" },
		{ kVK_F20, "F20" },
		{ kVK_F5, "F5" },
		{ kVK_F6, "F6" },
		{ kVK_F7, "F7" },
		{ kVK_F3, "F3" },
		{ kVK_F8, "F8" },
		{ kVK_F9, "F9" },
		{ kVK_F11, "F11" },
		{ kVK_F13, "F13" },
		{ kVK_F16, "F16" },
		{ kVK_F14, "F14" },
		{ kVK_F10, "F10" },
		{ kVK_F12, "F12" },
		{ kVK_F15, "F15" },
		{ kVK_Help, "Help" },
		{ kVK_Home, "\xE2\x86\x96" },
		{ kVK_PageUp, "Page Up" },
		{ kVK_ForwardDelete, "\xe2\x8c\xa6" },
		{ kVK_F4, "F4" },
		{ kVK_End, "\xE2\x86\x98" },
		{ kVK_F2, "F2" },
		{ kVK_PageDown, "Page Down" },
		{ kVK_F1, "F1" },
		{ kVK_LeftArrow, "\xE2\x86\x90" },
		{ kVK_RightArrow, "\xE2\x86\x92" },
		{ kVK_DownArrow, "\xE2\x86\x93" },
		{ kVK_UpArrow, "\xE2\x86\x91" },

		{ kVK_ANSI_A, "A" },
		{ kVK_ANSI_S, "S" },
		{ kVK_ANSI_D, "D" },
		{ kVK_ANSI_F, "F" },
		{ kVK_ANSI_H, "H" },
		{ kVK_ANSI_G, "G" },
		{ kVK_ANSI_Z, "Z" },
		{ kVK_ANSI_X, "X" },
		{ kVK_ANSI_C, "C" },
		{ kVK_ANSI_V, "V" },
		{ kVK_ANSI_B, "B" },
		{ kVK_ANSI_Q, "Q" },
		{ kVK_ANSI_W, "W" },
		{ kVK_ANSI_E, "E" },
		{ kVK_ANSI_R, "R" },
		{ kVK_ANSI_Y, "Y" },
		{ kVK_ANSI_T, "T" },
		{ kVK_ANSI_1, "1" },
		{ kVK_ANSI_2, "2" },
		{ kVK_ANSI_3, "3" },
		{ kVK_ANSI_4, "4" },
		{ kVK_ANSI_6, "6" },
		{ kVK_ANSI_5, "5" },
		{ kVK_ANSI_Equal, "=" },
		{ kVK_ANSI_9, "9" },
		{ kVK_ANSI_7, "7" },
		{ kVK_ANSI_Minus, "-" },
		{ kVK_ANSI_8, "8" },
		{ kVK_ANSI_0, "0" },
		{ kVK_ANSI_RightBracket, "]" },
		{ kVK_ANSI_O, "O" },
		{ kVK_ANSI_U, "U" },
		{ kVK_ANSI_LeftBracket, "[" },
		{ kVK_ANSI_I, "I" },
		{ kVK_ANSI_P, "P" },
		{ kVK_ANSI_L, "L" },
		{ kVK_ANSI_J, "J" },
		{ kVK_ANSI_Quote, "'" },
		{ kVK_ANSI_K, "K" },
		{ kVK_ANSI_Semicolon, "/" },
		{ kVK_ANSI_Backslash, "\\" },
		{ kVK_ANSI_Comma, "," },
		{ kVK_ANSI_Slash, "/" },
		{ kVK_ANSI_N, "N" },
		{ kVK_ANSI_M, "M" },
		{ kVK_ANSI_Period, "." },
		{ kVK_ANSI_Grave, "`" },
		{ kVK_ANSI_KeypadDecimal, "Num ." },
		{ kVK_ANSI_KeypadMultiply, "Num *" },
		{ kVK_ANSI_KeypadPlus, "Num +" },
		{ kVK_ANSI_KeypadClear, "Num Clear" },
		{ kVK_ANSI_KeypadDivide, "Num /" },
		{ kVK_ANSI_KeypadEnter, "Num Enter" },
		{ kVK_ANSI_KeypadMinus, "Num -" },
		{ kVK_ANSI_KeypadEquals, "Num =" },
		{ kVK_ANSI_Keypad0, "Num 0" },
		{ kVK_ANSI_Keypad1, "Num 1" },
		{ kVK_ANSI_Keypad2, "Num 2" },
		{ kVK_ANSI_Keypad3, "Num 3" },
		{ kVK_ANSI_Keypad4, "Num 4" },
		{ kVK_ANSI_Keypad5, "Num 5" },
		{ kVK_ANSI_Keypad6, "Num 6" },
		{ kVK_ANSI_Keypad7, "Num 7" },
		{ kVK_ANSI_Keypad8, "Num 8" },
		{ kVK_ANSI_Keypad9, "Num 9" },
	};

	if (descriptor > kShiftMouseButton) {
		return QString("Mouse %1").arg(descriptor - kShiftMouseButton);
	}

	const auto i = KeyToString.find(descriptor);
	return (i != end(KeyToString))
		? i->second.utf16()
		: QString("\\x%1").arg(descriptor, 0, 16);
}

} // namespace base::Platform::GlobalShortcuts
