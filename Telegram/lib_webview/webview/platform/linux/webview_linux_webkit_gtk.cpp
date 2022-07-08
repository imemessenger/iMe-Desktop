// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "webview/platform/linux/webview_linux_webkit_gtk.h"

#include <dlfcn.h>
#include <memory>

#define LOAD_SYMBOL(handle, func) LoadSymbol(handle, #func, func)

namespace Webview::WebkitGtk {
namespace {

struct HandleDeleter {
	void operator()(void *handle) {
		dlclose(handle);
	}
};

using Handle = std::unique_ptr<void, HandleDeleter>;

bool LoadLibrary(Handle &handle, const char *name) {
	handle = Handle(dlopen(name, RTLD_LAZY | RTLD_NODELETE));
	if (handle) {
		return true;
	}
	return false;
}

template <typename Function>
inline bool LoadSymbol(const Handle &handle, const char *name, Function &func) {
	func = handle
		? reinterpret_cast<Function>(dlsym(handle.get(), name))
		: nullptr;
	return (func != nullptr);
}

} // namespace

bool Resolve() {
	auto webkit2gtk = Handle();
	const auto result = (LoadLibrary(webkit2gtk, "libwebkit2gtk-5.0.so.0")
			|| LoadLibrary(webkit2gtk, "libwebkit2gtk-4.1.so.0")
			|| LoadLibrary(webkit2gtk, "libwebkit2gtk-4.0.so.37"))
		&& LOAD_SYMBOL(webkit2gtk, gtk_init_check)
		&& LOAD_SYMBOL(webkit2gtk, gtk_widget_get_type)
		&& LOAD_SYMBOL(webkit2gtk, gtk_widget_grab_focus)
		&& (LOAD_SYMBOL(webkit2gtk, gtk_window_set_child)
			|| (LOAD_SYMBOL(webkit2gtk, gtk_container_get_type)
				&& LOAD_SYMBOL(webkit2gtk, gtk_container_add)))
		&& ((LOAD_SYMBOL(webkit2gtk, gtk_widget_get_native)
				&& LOAD_SYMBOL(webkit2gtk, gtk_native_get_surface))
			|| LOAD_SYMBOL(webkit2gtk, gtk_widget_get_window))
		&& LOAD_SYMBOL(webkit2gtk, gtk_window_new)
		&& (LOAD_SYMBOL(webkit2gtk, gtk_window_destroy)
			|| LOAD_SYMBOL(webkit2gtk, gtk_widget_destroy))
		&& LOAD_SYMBOL(webkit2gtk, gtk_widget_hide)
		&& (LOAD_SYMBOL(webkit2gtk, gtk_widget_show_all)
			|| LOAD_SYMBOL(webkit2gtk, gtk_widget_show))
		&& LOAD_SYMBOL(webkit2gtk, gtk_window_get_type)
		&& LOAD_SYMBOL(webkit2gtk, gtk_window_set_decorated)
		&& (LOAD_SYMBOL(webkit2gtk, gdk_x11_surface_get_xid)
			|| LOAD_SYMBOL(webkit2gtk, gdk_x11_window_get_xid))
		&& LOAD_SYMBOL(webkit2gtk, webkit_web_view_new)
		&& LOAD_SYMBOL(webkit2gtk, webkit_web_view_get_type)
		&& LOAD_SYMBOL(webkit2gtk, webkit_web_view_get_user_content_manager)
		&& LOAD_SYMBOL(webkit2gtk, webkit_user_content_manager_register_script_message_handler)
		&& LOAD_SYMBOL(webkit2gtk, webkit_web_view_get_settings)
		&& LOAD_SYMBOL(webkit2gtk, webkit_settings_set_javascript_can_access_clipboard)
		&& LOAD_SYMBOL(webkit2gtk, webkit_web_view_load_uri)
		&& LOAD_SYMBOL(webkit2gtk, webkit_web_view_reload_bypass_cache)
		&& LOAD_SYMBOL(webkit2gtk, webkit_user_script_new)
		&& LOAD_SYMBOL(webkit2gtk, webkit_user_content_manager_add_script)
		&& LOAD_SYMBOL(webkit2gtk, webkit_web_view_run_javascript)
		&& LOAD_SYMBOL(webkit2gtk, webkit_uri_request_get_uri)
		&& LOAD_SYMBOL(webkit2gtk, webkit_policy_decision_ignore)
		&& LOAD_SYMBOL(webkit2gtk, webkit_navigation_policy_decision_get_type)
		&& LOAD_SYMBOL(webkit2gtk, webkit_script_dialog_get_dialog_type)
		&& LOAD_SYMBOL(webkit2gtk, webkit_script_dialog_get_message)
		&& LOAD_SYMBOL(webkit2gtk, webkit_script_dialog_confirm_set_confirmed)
		&& LOAD_SYMBOL(webkit2gtk, webkit_script_dialog_prompt_get_default_text)
		&& LOAD_SYMBOL(webkit2gtk, webkit_script_dialog_prompt_set_text);
	if (!result) {
		return false;
	}
	{
		const auto available1 = LOAD_SYMBOL(webkit2gtk, jsc_value_to_string)
			&& LOAD_SYMBOL(webkit2gtk, webkit_javascript_result_get_js_value);

		const auto available2 = LOAD_SYMBOL(webkit2gtk, webkit_javascript_result_get_global_context)
			&& LOAD_SYMBOL(webkit2gtk, webkit_javascript_result_get_value)
			&& LOAD_SYMBOL(webkit2gtk, JSValueToStringCopy)
			&& LOAD_SYMBOL(webkit2gtk, JSStringGetMaximumUTF8CStringSize)
			&& LOAD_SYMBOL(webkit2gtk, JSStringGetUTF8CString)
			&& LOAD_SYMBOL(webkit2gtk, JSStringRelease);
		if (!available1 && !available2) {
			return false;
		}
	}
	{
		const auto available1 = LOAD_SYMBOL(webkit2gtk, webkit_navigation_policy_decision_get_navigation_action)
			&& LOAD_SYMBOL(webkit2gtk, webkit_navigation_action_get_request);

		const auto available2 = LOAD_SYMBOL(webkit2gtk, webkit_navigation_policy_decision_get_request);

		if (!available1 && !available2) {
			return false;
		}
	}
	if (LOAD_SYMBOL(webkit2gtk, gdk_set_allowed_backends)) {
		gdk_set_allowed_backends("x11");
	}
	return gtk_init_check(0, 0);
}

} // namespace Webview::WebkitGtk
