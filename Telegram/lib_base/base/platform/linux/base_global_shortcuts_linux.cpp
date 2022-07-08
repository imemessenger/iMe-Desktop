// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/linux/base_global_shortcuts_linux.h"

#include "base/const_string.h"
#include "base/global_shortcuts_generic.h"
#include "base/platform/base_platform_info.h" // IsX11
#include "base/debug_log.h"

#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
#include "base/platform/linux/base_linux_xcb_utilities.h" // CustomConnection, IsExtensionPresent
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION

#include <QKeySequence>
#include <QSocketNotifier>

#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
#include <xcb/record.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h> // xcb_key_symbols_*
#include <xcb/xcbext.h> // xcb_poll_for_reply

#include <X11/XF86keysym.h>
#include <X11/keysym.h>
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION

namespace base::Platform::GlobalShortcuts {
namespace {

constexpr auto kShiftMouseButton = std::numeric_limits<uint64>::max() - 100;

Fn<void(GlobalShortcutKeyGeneric descriptor, bool down)> ProcessCallback;

#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
using XcbReply = xcb_record_enable_context_reply_t;

struct KeySymbolsDeleter {
	void operator()(xcb_key_symbols_t *value) {
		xcb_key_symbols_free(value);
	}
};

bool IsKeypad(xcb_keysym_t keysym) {
	return (xcb_is_keypad_key(keysym) || xcb_is_private_keypad_key(keysym));
}

bool SkipMouseButton(xcb_button_t b) {
	return (b == 1) // Ignore the left button.
		|| (b > 3 && b < 8); // Ignore the wheel.
}

class X11Manager final {
public:
	X11Manager();
	~X11Manager();

	[[nodiscard]] bool available() const;

private:
	void process(XcbReply *reply);
	xcb_keysym_t computeKeysym(xcb_keycode_t detail, uint16_t state);

	XCB::CustomConnection _connection;
	std::unique_ptr<xcb_key_symbols_t, KeySymbolsDeleter> _keySymbols;
	std::unique_ptr<QSocketNotifier> _notifier;
	std::optional<xcb_record_context_t> _context;
	std::optional<xcb_record_enable_context_cookie_t> _cookie;

};

X11Manager::X11Manager()
: _keySymbols(xcb_key_symbols_alloc(_connection)) {

	if (xcb_connection_has_error(_connection)) {
		LOG((
			"Global Shortcuts Manager: Error to open local display!"));
		return;
	}

	if (!XCB::IsExtensionPresent(_connection, &xcb_record_id)) {
		LOG(("Global Shortcuts Manager: "
			"RECORD extension not supported on this X server!"));
		return;
	}

	_context = xcb_generate_id(_connection);
	const xcb_record_client_spec_t clientSpec[] = {
		XCB_RECORD_CS_ALL_CLIENTS
	};

	const xcb_record_range_t recordRange[] = {
		[] {
			xcb_record_range_t rr;
			memset(&rr, 0, sizeof(rr));

			// XCB_KEY_PRESS = 2
			// XCB_KEY_RELEASE = 3
			// XCB_BUTTON_PRESS = 4
			// XCB_BUTTON_RELEASE = 5
			rr.device_events = { XCB_KEY_PRESS, XCB_BUTTON_RELEASE };
			return rr;
		}()
	};

	const auto createCookie = xcb_record_create_context_checked(
		_connection,
		*_context,
		0,
		sizeof(clientSpec) / sizeof(clientSpec[0]),
		sizeof(recordRange) / sizeof(recordRange[0]),
		clientSpec,
		recordRange);
	if (xcb_request_check(_connection, createCookie)) {
		LOG((
			"Global Shortcuts Manager: Could not create a record context!"));
		_context = std::nullopt;
		return;
	}

	_cookie = xcb_record_enable_context(_connection, *_context);
	xcb_flush(_connection);

	_notifier = std::make_unique<QSocketNotifier>(
		xcb_get_file_descriptor(_connection),
		QSocketNotifier::Read);

	QObject::connect(_notifier.get(), &QSocketNotifier::activated, [=] {
		while (const auto event = xcb_poll_for_event(_connection)) {
			free(event);
		}

		void *reply = nullptr;
		xcb_generic_error_t *error = nullptr;
		while (_cookie
			&& _cookie->sequence
			&& xcb_poll_for_reply(
				_connection,
				_cookie->sequence,
				&reply,
				&error)) {
			// The xcb_poll_for_reply method may set both reply and error
			// to null if connection has error.
			if (xcb_connection_has_error(_connection)) {
				break;
			}

			if (error) {
				free(error);
				break;
			}

			if (!reply) {
				continue;
			}

			process(reinterpret_cast<XcbReply*>(reply));
			free(reply);
		}
	});
	_notifier->setEnabled(true);
}


X11Manager::~X11Manager() {
	if (_cookie) {
		xcb_record_disable_context(_connection, *_context);
		_cookie = std::nullopt;
	}

	if (_context) {
		xcb_record_free_context(_connection, *_context);
		_context = std::nullopt;
	}
}

void X11Manager::process(XcbReply *reply) {
	if (!ProcessCallback) {
		return;
	}
	// Seems like xcb_button_press_event_t and xcb_key_press_event_t structs
	// are the same, so we can safely cast both of them
	// to the xcb_key_press_event_t.
	const auto events = reinterpret_cast<xcb_key_press_event_t*>(
		xcb_record_enable_context_data(reply));

	const auto countEvents = xcb_record_enable_context_data_length(reply) /
		sizeof(xcb_key_press_event_t);

	for (auto e = events; e < (events + countEvents); e++) {
		const auto type = e->response_type;
		const auto buttonPress = (type == XCB_BUTTON_PRESS);
		const auto buttonRelease = (type == XCB_BUTTON_RELEASE);
		const auto keyPress = (type == XCB_KEY_PRESS);
		const auto keyRelease = (type == XCB_KEY_RELEASE);
		const auto isButton = (buttonPress || buttonRelease);

		if (!(keyPress || keyRelease || isButton)) {
			continue;
		}
		const auto code = e->detail;
		if (isButton && SkipMouseButton(code)) {
			return;
		}
		const auto descriptor = isButton
			? (kShiftMouseButton + code)
			: GlobalShortcutKeyGeneric(computeKeysym(code, e->state));
		ProcessCallback(descriptor, keyPress || buttonPress);
	}
}

xcb_keysym_t X11Manager::computeKeysym(xcb_keycode_t detail, uint16_t state) {
	// Perhaps XCB_MOD_MASK_1-5 are needed here.
	const auto keySym1 = xcb_key_symbols_get_keysym(_keySymbols.get(), detail, 1);
	if (IsKeypad(keySym1)) {
		return keySym1;
	}
	if (keySym1 >= Qt::Key_A && keySym1 <= Qt::Key_Z) {
		if (keySym1 != XCB_NO_SYMBOL) {
			return keySym1;
		}
	}

	return xcb_key_symbols_get_keysym(_keySymbols.get(), detail, 0);
}

bool X11Manager::available() const {
	return _cookie.has_value();
}

std::unique_ptr<X11Manager> _x11Manager = nullptr;

void EnsureX11ShortcutManager() {
	if (!_x11Manager) {
		_x11Manager = std::make_unique<X11Manager>();
	}
}
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION

} // namespace

bool Available() {
#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
	if (::Platform::IsX11()) {
		EnsureX11ShortcutManager();
		return _x11Manager->available();
	}
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION

	return false;
}

bool Allowed() {
	return Available();
}

void Start(Fn<void(GlobalShortcutKeyGeneric descriptor, bool down)> process) {
	ProcessCallback = std::move(process);

#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
	EnsureX11ShortcutManager();
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION
}

void Stop() {
	ProcessCallback = nullptr;
#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
	_x11Manager = nullptr;
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION
}

QString KeyName(GlobalShortcutKeyGeneric descriptor) {
#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
	// Telegram/ThirdParty/fcitx-qt5/platforminputcontext/qtkey.cpp
	static const auto KeyToString = flat_map<uint64, int>{
		{ XK_KP_Space, Qt::Key_Space },
		{ XK_KP_Tab, Qt::Key_Tab },
		{ XK_KP_Enter, Qt::Key_Enter },
		{ XK_KP_F1, Qt::Key_F1 },
		{ XK_KP_F2, Qt::Key_F2 },
		{ XK_KP_F3, Qt::Key_F3 },
		{ XK_KP_F4, Qt::Key_F4 },
		{ XK_KP_Home, Qt::Key_Home },
		{ XK_KP_Left, Qt::Key_Left },
		{ XK_KP_Up, Qt::Key_Up },
		{ XK_KP_Right, Qt::Key_Right },
		{ XK_KP_Down, Qt::Key_Down },
		{ XK_KP_Page_Up, Qt::Key_PageUp },
		{ XK_KP_Page_Down, Qt::Key_PageDown },
		{ XK_KP_End, Qt::Key_End },
		{ XK_KP_Begin, Qt::Key_Clear },
		{ XK_KP_Insert, Qt::Key_Insert },
		{ XK_KP_Delete, Qt::Key_Delete },
		{ XK_KP_Equal, Qt::Key_Equal },
		{ XK_KP_Multiply, Qt::Key_multiply },
		{ XK_KP_Add, Qt::Key_Plus },
		{ XK_KP_Separator, Qt::Key_Comma },
		{ XK_KP_Subtract, Qt::Key_Minus },
		{ XK_KP_Decimal, Qt::Key_Period },
		{ XK_KP_Divide, Qt::Key_Slash },

		{ XK_KP_0, Qt::Key_0 },
		{ XK_KP_1, Qt::Key_1 },
		{ XK_KP_2, Qt::Key_2 },
		{ XK_KP_3, Qt::Key_3 },
		{ XK_KP_4, Qt::Key_4 },
		{ XK_KP_5, Qt::Key_5 },
		{ XK_KP_6, Qt::Key_6 },
		{ XK_KP_7, Qt::Key_7 },
		{ XK_KP_8, Qt::Key_8 },
		{ XK_KP_9, Qt::Key_9 },

		{ XK_Escape, Qt::Key_Escape },
		{ XK_Tab, Qt::Key_Tab },
		{ XK_ISO_Left_Tab, Qt::Key_Tab },
		{ XK_BackSpace, Qt::Key_Backspace },
		{ XK_Return, Qt::Key_Return },
		{ XK_KP_Enter, Qt::Key_Enter },
		{ XK_Insert, Qt::Key_Insert },
		{ XK_Delete, Qt::Key_Delete },
		{ XK_Clear, Qt::Key_Delete },
		{ XK_Pause, Qt::Key_Pause },
		{ XK_Print, Qt::Key_Print },
		{ XK_Sys_Req, Qt::Key_SysReq },
		{ 0x1005FF60, Qt::Key_SysReq },
		{ 0x1007ff00, Qt::Key_SysReq },

		{ XK_Home, Qt::Key_Home },
		{ XK_End, Qt::Key_End },
		{ XK_Left, Qt::Key_Left },
		{ XK_Up, Qt::Key_Up },
		{ XK_Right, Qt::Key_Right },
		{ XK_Down, Qt::Key_Down },
		{ XK_Page_Up, Qt::Key_PageUp },
		{ XK_Page_Down, Qt::Key_PageDown },
		{ XK_Shift_L, Qt::Key_Shift },
		{ XK_Shift_R, Qt::Key_Shift },
		{ XK_Shift_Lock, Qt::Key_Shift },
		{ XK_Control_L, Qt::Key_Control },
		{ XK_Control_R, Qt::Key_Control },
		{ XK_Meta_L, Qt::Key_Meta },
		{ XK_Meta_R, Qt::Key_Meta },
		{ XK_Alt_L, Qt::Key_Alt },
		{ XK_Alt_R, Qt::Key_Alt },
		{ XK_Caps_Lock, Qt::Key_CapsLock },
		{ XK_Num_Lock, Qt::Key_NumLock },
		{ XK_Scroll_Lock, Qt::Key_ScrollLock },
		{ XK_F1, Qt::Key_F1 },
		{ XK_F2, Qt::Key_F2 },
		{ XK_F3, Qt::Key_F3 },
		{ XK_F4, Qt::Key_F4 },
		{ XK_F5, Qt::Key_F5 },
		{ XK_F6, Qt::Key_F6 },
		{ XK_F7, Qt::Key_F7 },
		{ XK_F8, Qt::Key_F8 },
		{ XK_F9, Qt::Key_F9 },
		{ XK_F10, Qt::Key_F10 },
		{ XK_F11, Qt::Key_F11 },
		{ XK_F12, Qt::Key_F12 },
		{ XK_F13, Qt::Key_F13 },
		{ XK_F14, Qt::Key_F14 },
		{ XK_F15, Qt::Key_F15 },
		{ XK_F16, Qt::Key_F16 },
		{ XK_F17, Qt::Key_F17 },
		{ XK_F18, Qt::Key_F18 },
		{ XK_F19, Qt::Key_F19 },
		{ XK_F20, Qt::Key_F20 },
		{ XK_F21, Qt::Key_F21 },
		{ XK_F22, Qt::Key_F22 },
		{ XK_F23, Qt::Key_F23 },
		{ XK_F24, Qt::Key_F24 },
		{ XK_F25, Qt::Key_F25 },
		{ XK_F26, Qt::Key_F26 },
		{ XK_F27, Qt::Key_F27 },
		{ XK_F28, Qt::Key_F28 },
		{ XK_F29, Qt::Key_F29 },
		{ XK_F30, Qt::Key_F30 },
		{ XK_F31, Qt::Key_F31 },
		{ XK_F32, Qt::Key_F32 },
		{ XK_F33, Qt::Key_F33 },
		{ XK_F34, Qt::Key_F34 },
		{ XK_F35, Qt::Key_F35 },
		{ XK_Super_L, Qt::Key_Super_L },
		{ XK_Super_R, Qt::Key_Super_R },
		{ XK_Menu, Qt::Key_Menu },
		{ XK_Hyper_L, Qt::Key_Hyper_L },
		{ XK_Hyper_R, Qt::Key_Hyper_R },
		{ XK_Help, Qt::Key_Help },
		{ XK_ISO_Level3_Shift, Qt::Key_AltGr },
		{ XK_Multi_key, Qt::Key_Multi_key },
		{ XK_Codeinput, Qt::Key_Codeinput },
		{ XK_SingleCandidate, Qt::Key_SingleCandidate },
		{ XK_MultipleCandidate, Qt::Key_MultipleCandidate },
		{ XK_PreviousCandidate, Qt::Key_PreviousCandidate },
		{ XK_Mode_switch, Qt::Key_Mode_switch },
		{ XK_script_switch, Qt::Key_Mode_switch },
		{ XK_Kanji, Qt::Key_Kanji },
		{ XK_Muhenkan, Qt::Key_Muhenkan },
		{ XK_Henkan, Qt::Key_Henkan },
		{ XK_Romaji, Qt::Key_Romaji },
		{ XK_Hiragana, Qt::Key_Hiragana },
		{ XK_Katakana, Qt::Key_Katakana },
		{ XK_Hiragana_Katakana, Qt::Key_Hiragana_Katakana },
		{ XK_Zenkaku, Qt::Key_Zenkaku },
		{ XK_Hankaku, Qt::Key_Hankaku },
		{ XK_Zenkaku_Hankaku, Qt::Key_Zenkaku_Hankaku },
		{ XK_Touroku, Qt::Key_Touroku },
		{ XK_Massyo, Qt::Key_Massyo },
		{ XK_Kana_Lock, Qt::Key_Kana_Lock },
		{ XK_Kana_Shift, Qt::Key_Kana_Shift },
		{ XK_Eisu_Shift, Qt::Key_Eisu_Shift },
		{ XK_Eisu_toggle, Qt::Key_Eisu_toggle },
		{ XK_Kanji_Bangou, Qt::Key_Codeinput },
		{ XK_Zen_Koho, Qt::Key_MultipleCandidate },
		{ XK_Mae_Koho, Qt::Key_PreviousCandidate },
		{ XK_Hangul, Qt::Key_Hangul },
		{ XK_Hangul_Start, Qt::Key_Hangul_Start },
		{ XK_Hangul_End, Qt::Key_Hangul_End },
		{ XK_Hangul_Hanja, Qt::Key_Hangul_Hanja },
		{ XK_Hangul_Jamo, Qt::Key_Hangul_Jamo },
		{ XK_Hangul_Romaja, Qt::Key_Hangul_Romaja },
		{ XK_Hangul_Codeinput, Qt::Key_Codeinput },
		{ XK_Hangul_Jeonja, Qt::Key_Hangul_Jeonja },
		{ XK_Hangul_Banja, Qt::Key_Hangul_Banja },
		{ XK_Hangul_PreHanja, Qt::Key_Hangul_PreHanja },
		{ XK_Hangul_PostHanja, Qt::Key_Hangul_PostHanja },
		{ XK_Hangul_SingleCandidate, Qt::Key_SingleCandidate },
		{ XK_Hangul_MultipleCandidate, Qt::Key_MultipleCandidate },
		{ XK_Hangul_PreviousCandidate, Qt::Key_PreviousCandidate },
		{ XK_Hangul_Special, Qt::Key_Hangul_Special },
		{ XK_Hangul_switch, Qt::Key_Mode_switch },
		{ XK_dead_grave, Qt::Key_Dead_Grave },
		{ XK_dead_acute, Qt::Key_Dead_Acute },
		{ XK_dead_circumflex, Qt::Key_Dead_Circumflex },
		{ XK_dead_tilde, Qt::Key_Dead_Tilde },
		{ XK_dead_macron, Qt::Key_Dead_Macron },
		{ XK_dead_breve, Qt::Key_Dead_Breve },
		{ XK_dead_abovedot, Qt::Key_Dead_Abovedot },
		{ XK_dead_diaeresis, Qt::Key_Dead_Diaeresis },
		{ XK_dead_abovering, Qt::Key_Dead_Abovering },
		{ XK_dead_doubleacute, Qt::Key_Dead_Doubleacute },
		{ XK_dead_caron, Qt::Key_Dead_Caron },
		{ XK_dead_cedilla, Qt::Key_Dead_Cedilla },
		{ XK_dead_ogonek, Qt::Key_Dead_Ogonek },
		{ XK_dead_iota, Qt::Key_Dead_Iota },
		{ XK_dead_voiced_sound, Qt::Key_Dead_Voiced_Sound },
		{ XK_dead_semivoiced_sound, Qt::Key_Dead_Semivoiced_Sound },
		{ XK_dead_belowdot, Qt::Key_Dead_Belowdot },
		{ XK_dead_hook, Qt::Key_Dead_Hook },
		{ XK_dead_horn, Qt::Key_Dead_Horn },
		{ XF86XK_Back, Qt::Key_Back },
		{ XF86XK_Forward, Qt::Key_Forward },
		{ XF86XK_Stop, Qt::Key_Stop },
		{ XF86XK_Refresh, Qt::Key_Refresh },
		{ XF86XK_AudioLowerVolume, Qt::Key_VolumeDown },
		{ XF86XK_AudioMute, Qt::Key_VolumeMute },
		{ XF86XK_AudioRaiseVolume, Qt::Key_VolumeUp },
		{ XF86XK_AudioPlay, Qt::Key_MediaPlay },
		{ XF86XK_AudioStop, Qt::Key_MediaStop },
		{ XF86XK_AudioPrev, Qt::Key_MediaPrevious },
		{ XF86XK_AudioNext, Qt::Key_MediaNext },
		{ XF86XK_AudioRecord, Qt::Key_MediaRecord },
		{ XF86XK_AudioPause, Qt::Key_MediaPause },
		{ XF86XK_HomePage, Qt::Key_HomePage },
		{ XF86XK_Favorites, Qt::Key_Favorites },
		{ XF86XK_Search, Qt::Key_Search },
		{ XF86XK_Standby, Qt::Key_Standby },
		{ XF86XK_OpenURL, Qt::Key_OpenUrl },
		{ XF86XK_Mail, Qt::Key_LaunchMail },
		{ XF86XK_AudioMedia, Qt::Key_LaunchMedia },
		{ XF86XK_MyComputer, Qt::Key_Launch0 },
		{ XF86XK_Calculator, Qt::Key_Launch1 },
		{ XF86XK_Launch0, Qt::Key_Launch2 },
		{ XF86XK_Launch1, Qt::Key_Launch3 },
		{ XF86XK_Launch2, Qt::Key_Launch4 },
		{ XF86XK_Launch3, Qt::Key_Launch5 },
		{ XF86XK_Launch4, Qt::Key_Launch6 },
		{ XF86XK_Launch5, Qt::Key_Launch7 },
		{ XF86XK_Launch6, Qt::Key_Launch8 },
		{ XF86XK_Launch7, Qt::Key_Launch9 },
		{ XF86XK_Launch8, Qt::Key_LaunchA },
		{ XF86XK_Launch9, Qt::Key_LaunchB },
		{ XF86XK_LaunchA, Qt::Key_LaunchC },
		{ XF86XK_LaunchB, Qt::Key_LaunchD },
		{ XF86XK_LaunchC, Qt::Key_LaunchE },
		{ XF86XK_LaunchD, Qt::Key_LaunchF },
		{ XF86XK_MonBrightnessUp, Qt::Key_MonBrightnessUp },
		{ XF86XK_MonBrightnessDown, Qt::Key_MonBrightnessDown },
		{ XF86XK_KbdLightOnOff, Qt::Key_KeyboardLightOnOff },
		{ XF86XK_KbdBrightnessUp, Qt::Key_KeyboardBrightnessUp },
		{ XF86XK_PowerOff, Qt::Key_PowerOff },
		{ XF86XK_WakeUp, Qt::Key_WakeUp },
		{ XF86XK_Eject, Qt::Key_Eject },
		{ XF86XK_ScreenSaver, Qt::Key_ScreenSaver },
		{ XF86XK_WWW, Qt::Key_WWW },
		{ XF86XK_Memo, Qt::Key_Memo },
		{ XF86XK_LightBulb, Qt::Key_LightBulb },
		{ XF86XK_Shop, Qt::Key_Shop },
		{ XF86XK_History, Qt::Key_History },
		{ XF86XK_AddFavorite, Qt::Key_AddFavorite },
		{ XF86XK_HotLinks, Qt::Key_HotLinks },
		{ XF86XK_BrightnessAdjust, Qt::Key_BrightnessAdjust },
		{ XF86XK_Finance, Qt::Key_Finance },
		{ XF86XK_Community, Qt::Key_Community },
		{ XF86XK_AudioRewind, Qt::Key_AudioRewind },
		{ XF86XK_BackForward, Qt::Key_BackForward },
		{ XF86XK_ApplicationLeft, Qt::Key_ApplicationLeft },
		{ XF86XK_ApplicationRight, Qt::Key_ApplicationRight },
		{ XF86XK_Book, Qt::Key_Book },
		{ XF86XK_CD, Qt::Key_CD },
		{ XF86XK_Calculater, Qt::Key_Calculator },
		{ XF86XK_ToDoList, Qt::Key_ToDoList },
		{ XF86XK_ClearGrab, Qt::Key_ClearGrab },
		{ XF86XK_Close, Qt::Key_Close },
		{ XF86XK_Copy, Qt::Key_Copy },
		{ XF86XK_Cut, Qt::Key_Cut },
		{ XF86XK_Display, Qt::Key_Display },
		{ XF86XK_DOS, Qt::Key_DOS },
		{ XF86XK_Documents, Qt::Key_Documents },
		{ XF86XK_Excel, Qt::Key_Excel },
		{ XF86XK_Explorer, Qt::Key_Explorer },
		{ XF86XK_Game, Qt::Key_Game },
		{ XF86XK_Go, Qt::Key_Go },
		{ XF86XK_iTouch, Qt::Key_iTouch },
		{ XF86XK_LogOff, Qt::Key_LogOff },
		{ XF86XK_Market, Qt::Key_Market },
		{ XF86XK_Meeting, Qt::Key_Meeting },
		{ XF86XK_MenuKB, Qt::Key_MenuKB },
		{ XF86XK_MenuPB, Qt::Key_MenuPB },
		{ XF86XK_MySites, Qt::Key_MySites },
		{ XF86XK_News, Qt::Key_News },
		{ XF86XK_OfficeHome, Qt::Key_OfficeHome },
		{ XF86XK_Option, Qt::Key_Option },
		{ XF86XK_Paste, Qt::Key_Paste },
		{ XF86XK_Phone, Qt::Key_Phone },
		{ XF86XK_Calendar, Qt::Key_Calendar },
		{ XF86XK_Reply, Qt::Key_Reply },
		{ XF86XK_Reload, Qt::Key_Reload },
		{ XF86XK_RotateWindows, Qt::Key_RotateWindows },
		{ XF86XK_RotationPB, Qt::Key_RotationPB },
		{ XF86XK_RotationKB, Qt::Key_RotationKB },
		{ XF86XK_Save, Qt::Key_Save },
		{ XF86XK_Send, Qt::Key_Send },
		{ XF86XK_Spell, Qt::Key_Spell },
		{ XF86XK_SplitScreen, Qt::Key_SplitScreen },
		{ XF86XK_Support, Qt::Key_Support },
		{ XF86XK_TaskPane, Qt::Key_TaskPane },
		{ XF86XK_Terminal, Qt::Key_Terminal },
		{ XF86XK_Tools, Qt::Key_Tools },
		{ XF86XK_Travel, Qt::Key_Travel },
		{ XF86XK_Video, Qt::Key_Video },
		{ XF86XK_Word, Qt::Key_Word },
		{ XF86XK_Xfer, Qt::Key_Xfer },
		{ XF86XK_ZoomIn, Qt::Key_ZoomIn },
		{ XF86XK_ZoomOut, Qt::Key_ZoomOut },
		{ XF86XK_Away, Qt::Key_Away },
		{ XF86XK_Messenger, Qt::Key_Messenger },
		{ XF86XK_WebCam, Qt::Key_WebCam },
		{ XF86XK_MailForward, Qt::Key_MailForward },
		{ XF86XK_Pictures, Qt::Key_Pictures },
		{ XF86XK_Music, Qt::Key_Music },
		{ XF86XK_Battery, Qt::Key_Battery },
		{ XF86XK_Bluetooth, Qt::Key_Bluetooth },
		{ XF86XK_WLAN, Qt::Key_WLAN },
		{ XF86XK_UWB, Qt::Key_UWB },
		{ XF86XK_AudioForward, Qt::Key_AudioForward },
		{ XF86XK_AudioRepeat, Qt::Key_AudioRepeat },
		{ XF86XK_AudioRandomPlay, Qt::Key_AudioRandomPlay },
		{ XF86XK_Subtitle, Qt::Key_Subtitle },
		{ XF86XK_AudioCycleTrack, Qt::Key_AudioCycleTrack },
		{ XF86XK_Time, Qt::Key_Time },
		{ XF86XK_Hibernate, Qt::Key_Hibernate },
		{ XF86XK_View, Qt::Key_View },
		{ XF86XK_TopMenu, Qt::Key_TopMenu },
		{ XF86XK_PowerDown, Qt::Key_PowerDown },
		{ XF86XK_Suspend, Qt::Key_Suspend },
		{ XF86XK_ContrastAdjust, Qt::Key_ContrastAdjust },

		{ XF86XK_LaunchE, Qt::Key_LaunchG },
		{ XF86XK_LaunchF, Qt::Key_LaunchH },

		{ XF86XK_Select, Qt::Key_Select },
		{ XK_Cancel, Qt::Key_Cancel },
		{ XK_Execute, Qt::Key_Execute },
		{ XF86XK_Sleep, Qt::Key_Sleep },
	};

	// Mouse.
	// Taken from QXcbConnection::translateMouseButton.
	static const auto XcbButtonToQt = flat_map<uint64, Qt::MouseButton>{
		// { 1, Qt::LeftButton }, // Ignore the left button.
		{ 2, Qt::MiddleButton },
		{ 3, Qt::RightButton },
		// Button values 4-7 were already handled as Wheel events.
		{ 8, Qt::BackButton },
		{ 9, Qt::ForwardButton },
		{ 10, Qt::ExtraButton3 },
		{ 11, Qt::ExtraButton4 },
		{ 12, Qt::ExtraButton5 },
		{ 13, Qt::ExtraButton6 },
		{ 14, Qt::ExtraButton7 },
		{ 15, Qt::ExtraButton8 },
		{ 16, Qt::ExtraButton9 },
		{ 17, Qt::ExtraButton10 },
		{ 18, Qt::ExtraButton11 },
		{ 19, Qt::ExtraButton12 },
		{ 20, Qt::ExtraButton13 },
		{ 21, Qt::ExtraButton14 },
		{ 22, Qt::ExtraButton15 },
		{ 23, Qt::ExtraButton16 },
		{ 24, Qt::ExtraButton17 },
		{ 25, Qt::ExtraButton18 },
		{ 26, Qt::ExtraButton19 },
		{ 27, Qt::ExtraButton20 },
		{ 28, Qt::ExtraButton21 },
		{ 29, Qt::ExtraButton22 },
		{ 30, Qt::ExtraButton23 },
		{ 31, Qt::ExtraButton24 },
	};
	if (descriptor > kShiftMouseButton) {
		const auto button = descriptor - kShiftMouseButton;
		if (XcbButtonToQt.contains(button)) {
			return QString("Mouse %1").arg(button);
		}
	}
	//

	// Modifiers.
	static const auto ModifierToString = flat_map<uint64, const_string>{
		{ XK_Shift_L, "Shift" },
		{ XK_Shift_R, "Right Shift" },
		{ XK_Control_L, "Ctrl" },
		{ XK_Control_R, "Right Ctrl" },
		{ XK_Meta_L, "Meta" },
		{ XK_Meta_R, "Right Meta" },
		{ XK_Alt_L, "Alt" },
		{ XK_Alt_R, "Right Alt" },
		{ XK_Super_L, "Super" },
		{ XK_Super_R, "Right Super" },
	};
	const auto modIt = ModifierToString.find(descriptor);
	if (modIt != end(ModifierToString)) {
		return modIt->second.utf16();
	}
	//

	const auto fromSequence = [](int k) {
		return QKeySequence(k).toString(QKeySequence::NativeText);
	};

	// The conversion is not necessary,
	// if the value in the range Qt::Key_Space - Qt::Key_QuoteLeft.
	if (descriptor >= Qt::Key_Space && descriptor <= Qt::Key_QuoteLeft) {
		return fromSequence(descriptor);
	}
	const auto prefix = IsKeypad(descriptor) ? "Num " : QString();

	const auto keyIt = KeyToString.find(descriptor);
	return (keyIt != end(KeyToString))
		? prefix + fromSequence(keyIt->second)
		: QString("\\x%1").arg(descriptor, 0, 16);
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION

	return {};
}

} // namespace base::Platform::GlobalShortcuts
