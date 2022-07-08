// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "webview/platform/linux/webview_linux_webkit2gtk.h"

#include "webview/platform/linux/webview_linux_webkit_gtk.h"
#include "base/platform/linux/base_linux_glibmm_helper.h"
#include "base/platform/base_platform_info.h"
#include "base/const_string.h"
#include "base/integration.h"

#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>

#include <giomm.h>

namespace Webview::WebKit2Gtk {
namespace {

using namespace WebkitGtk;

constexpr auto kObjectPath = "/org/desktop_app/GtkIntegration/WebviewHelper"_cs;
constexpr auto kInterface = "org.desktop_app.GtkIntegration.WebviewHelper"_cs;

constexpr auto kIntrospectionXML = R"INTROSPECTION(<node>
	<interface name='org.desktop_app.GtkIntegration.WebviewHelper'>
		<method name='Create'/>
		<method name='Reload'/>
		<method name='Resolve'/>
		<method name='FinishEmbedding'/>
		<method name='Navigate'>
			<arg type='s' name='url' direction='in'/>
		</method>
		<method name='ResizeToWindow'/>
		<method name='Init'>
			<arg type='ay' name='js' direction='in'/>
		</method>
		<method name='Eval'>
			<arg type='ay' name='js' direction='in'/>
		</method>
		<method name='GetWinId'>
			<arg type='t' name='result' direction='out'/>
		</method>
		<signal name='MessageReceived'>
			<arg type='ay' name='message' direction='out'/>
		</signal>
		<signal name='NavigationStarted'>
			<arg type='s' name='uri' direction='out'/>
			<arg type='b' name='newWindow' direction='out'/>
		</signal>
		<signal name='ScriptDialog'>
			<arg type='i' name='type' direction='out'/>
			<arg type='s' name='text' direction='out'/>
			<arg type='s' name='value' direction='out'/>
		</signal>
		<signal name='NavigationDone'>
			<arg type='b' name='success' direction='out'/>
		</signal>
	</interface>
</node>)INTROSPECTION"_cs;

template <typename T>
struct GObjectDeleter {
	void operator()(T *value) {
		g_object_unref(value);
	}
};

template <typename T>
using GObjectPtr = std::unique_ptr<T, GObjectDeleter<T>>;

std::string SocketPath;
bool DebugMode/* = false*/;

inline std::string SocketPathToDBusAddress(const std::string &socketPath) {
	return "unix:path=" + socketPath;
}

class Instance final : public Interface {
public:
	Instance(Config config = {}, bool remoting = true);
	~Instance();

	void create();

	bool resolve();

	bool finishEmbedding() override;

	void navigate(std::string url) override;
	void reload() override;

	void resizeToWindow() override;

	void init(std::string js) override;
	void eval(std::string js) override;

	void *winId() override;

	int exec();

private:
	void scriptMessageReceived(WebKitJavascriptResult *result);

	bool loadFailed(
		WebKitLoadEvent loadEvent,
		std::string failingUri,
		Glib::Error error);

	void loadChanged(WebKitLoadEvent loadEvent);

	bool decidePolicy(
		WebKitPolicyDecision *decision,
		WebKitPolicyDecisionType decisionType);
	GtkWidget *createAnother(WebKitNavigationAction *action);
	bool scriptDialog(WebKitScriptDialog *dialog);

	void startProcess();
	void connectToRemoteSignals();

	void handleMethodCall(
		const Glib::RefPtr<Gio::DBus::Connection> &connection,
		const Glib::ustring &sender,
		const Glib::ustring &object_path,
		const Glib::ustring &interface_name,
		const Glib::ustring &method_name,
		const Glib::VariantContainerBase &parameters,
		const Glib::RefPtr<Gio::DBus::MethodInvocation> &invocation);

	bool _debug = false;
	bool _remoting = false;
	Glib::RefPtr<Gio::DBus::Connection> _dbusConnection;
	const Gio::DBus::InterfaceVTable _interfaceVTable;
	GObjectPtr<GSubprocess> _serviceProcess;
	uint _registerId = 0;
	uint _messageHandlerId = 0;
	uint _navigationStartHandlerId = 0;
	uint _navigationDoneHandlerId = 0;
	uint _scriptDialogHandlerId = 0;

	GtkWidget *_window = nullptr;
	GtkWidget *_webview = nullptr;
	std::function<void(std::string)> _messageHandler;
	std::function<bool(std::string,bool)> _navigationStartHandler;
	std::function<void(bool)> _navigationDoneHandler;
	std::function<DialogResult(DialogArgs)> _dialogHandler;
	bool _loadFailed = false;

};

Instance::Instance(Config config, bool remoting)
: _debug(DebugMode || config.debug)
, _remoting(remoting)
, _interfaceVTable(sigc::mem_fun(this, &Instance::handleMethodCall))
, _messageHandler(std::move(config.messageHandler))
, _navigationStartHandler(std::move(config.navigationStartHandler))
, _navigationDoneHandler(std::move(config.navigationDoneHandler))
, _dialogHandler(std::move(config.dialogHandler)) {
	if (_remoting) {
		startProcess();
	}
}

Instance::~Instance() {
	if (_serviceProcess) {
		g_subprocess_send_signal(_serviceProcess.get(), SIGTERM);
	}
	if (_dbusConnection) {
		if (_scriptDialogHandlerId != 0) {
			_dbusConnection->signal_unsubscribe(_scriptDialogHandlerId);
		}
		if (_navigationDoneHandlerId != 0) {
			_dbusConnection->signal_unsubscribe(_navigationDoneHandlerId);
		}
		if (_navigationStartHandlerId != 0) {
			_dbusConnection->signal_unsubscribe(_navigationStartHandlerId);
		}
		if (_messageHandlerId != 0) {
			_dbusConnection->signal_unsubscribe(_messageHandlerId);
		}
		if (_registerId != 0) {
			_dbusConnection->unregister_object(_registerId);
		}
	}
	if (_webview) {
		if (!gtk_widget_destroy) {
			g_object_unref(_webview);
		} else {
			gtk_widget_destroy(_webview);
		}
	}
	if (_window) {
		if (gtk_window_destroy) {
			gtk_window_destroy(GTK_WINDOW(_window));
		} else {
			gtk_widget_destroy(_window);
		}
	}
}

void Instance::create() {
	if (_remoting) {
		if (!_dbusConnection) {
			return;
		}

		try {
			auto reply = _dbusConnection->call_sync(
				std::string(kObjectPath),
				std::string(kInterface),
				"Create",
				{});

			return;
		} catch (...) {
		}
	}

	if (!resolve()) {
		return;
	}

	_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_decorated(GTK_WINDOW(_window), false);
	if (gtk_widget_show) {
		gtk_widget_show(_window);
	} else {
		gtk_widget_show_all(_window);
	}
	_webview = webkit_web_view_new();
	WebKitUserContentManager *manager =
		webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(_webview));
	g_signal_connect_swapped(
		manager,
		"script-message-received::external",
		G_CALLBACK(+[](
			Instance *instance,
			WebKitJavascriptResult *result) {
			instance->scriptMessageReceived(result);
		}),
		this);
	g_signal_connect_swapped(
		_webview,
		"load-failed",
		G_CALLBACK(+[](
			Instance *instance,
			WebKitLoadEvent loadEvent,
			char *failingUri,
			GError *error) -> gboolean {
			return instance->loadFailed(
				loadEvent,
				failingUri,
				Glib::Error(error));
		}),
		this);
	g_signal_connect_swapped(
		_webview,
		"load-changed",
		G_CALLBACK(+[](
			Instance *instance,
			WebKitLoadEvent loadEvent) {
			instance->loadChanged(loadEvent);
		}),
		this);
	g_signal_connect_swapped(
		_webview,
		"decide-policy",
		G_CALLBACK(+[](
			Instance *instance,
			WebKitPolicyDecision *decision,
			WebKitPolicyDecisionType decisionType) -> gboolean {
			return instance->decidePolicy(decision, decisionType);
		}),
		this);
	g_signal_connect_swapped(
		_webview,
		"create",
		G_CALLBACK(+[](
			Instance *instance,
			WebKitNavigationAction *action) -> GtkWidget* {
			return instance->createAnother(action);
		}),
		this);
	g_signal_connect_swapped(
		_webview,
		"script-dialog",
		G_CALLBACK(+[](
			Instance *instance,
			WebKitScriptDialog *dialog) -> gboolean {
			return instance->scriptDialog(dialog);
		}),
		this);
	webkit_user_content_manager_register_script_message_handler(
		manager,
		"external");
	init(R"(
window.external = {
	invoke: function(s) {
		window.webkit.messageHandlers.external.postMessage(s);
	}
};)");
}

void Instance::scriptMessageReceived(WebKitJavascriptResult *result) {
	auto message = std::string();
	if (webkit_javascript_result_get_js_value && jsc_value_to_string) {
		JSCValue *value = webkit_javascript_result_get_js_value(result);
		const auto s = jsc_value_to_string(value);
		message = s;
		g_free(s);
	} else {
		JSGlobalContextRef ctx
			= webkit_javascript_result_get_global_context(result);
		JSValueRef value = webkit_javascript_result_get_value(result);
		JSStringRef js = JSValueToStringCopy(ctx, value, NULL);
		size_t n = JSStringGetMaximumUTF8CStringSize(js);
		message.resize(n, char(0));
		JSStringGetUTF8CString(js, message.data(), n);
		JSStringRelease(js);
	}
	if (!_dbusConnection) {
		return;
	}
	try {
		_dbusConnection->emit_signal(
			std::string(kObjectPath),
			std::string(kInterface),
			"MessageReceived",
			{},
			base::Platform::MakeGlibVariant(std::tuple{
				message,
			}));
	} catch (...) {
	}
}

bool Instance::loadFailed(
		WebKitLoadEvent loadEvent,
		std::string failingUri,
		Glib::Error error) {
	_loadFailed = true;
	return false;
}

void Instance::loadChanged(WebKitLoadEvent loadEvent) {
	if (loadEvent == WEBKIT_LOAD_FINISHED) {
		const auto success = !_loadFailed;
		_loadFailed = false;
		if (!_dbusConnection) {
			return;
		}
		try {
			_dbusConnection->emit_signal(
				std::string(kObjectPath),
				std::string(kInterface),
				"NavigationDone",
				{},
				base::Platform::MakeGlibVariant(std::tuple{
					success,
				}));
		} catch (...) {
		}
	}
}

bool Instance::decidePolicy(
		WebKitPolicyDecision *decision,
		WebKitPolicyDecisionType decisionType) {
	if (decisionType != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
		return false;
	}
	WebKitURIRequest *request = nullptr;
	WebKitNavigationPolicyDecision *navigationDecision
		= WEBKIT_NAVIGATION_POLICY_DECISION(decision);
	if (webkit_navigation_policy_decision_get_navigation_action
		&& webkit_navigation_action_get_request) {
		WebKitNavigationAction *action
			= webkit_navigation_policy_decision_get_navigation_action(
				navigationDecision);
		request = webkit_navigation_action_get_request(action);
	} else {
		request = webkit_navigation_policy_decision_get_request(
			navigationDecision);
	}
	const gchar *uri = webkit_uri_request_get_uri(request);
	if (_dbusConnection) {
		try {
			const auto loop = Glib::MainLoop::create();
			bool result = false;

			const auto resultId = _dbusConnection->signal_subscribe(
				[&](
					const Glib::RefPtr<Gio::DBus::Connection> &connection,
					const Glib::ustring &sender_name,
					const Glib::ustring &object_path,
					const Glib::ustring &interface_name,
					const Glib::ustring &signal_name,
					Glib::VariantContainerBase parameters) {
					try {
						auto parametersCopy = parameters;

						result = base::Platform::GlibVariantCast<
							bool>(parametersCopy.get_child(0));
					} catch (...) {
					}

					loop->quit();
				},
				{},
				std::string(kInterface),
				"NavigationStartedResult",
				std::string(kObjectPath));

			const auto resultGuard = gsl::finally([&] {
				if (resultId != 0) {
					_dbusConnection->signal_unsubscribe(resultId);
				}
			});

			_dbusConnection->emit_signal(
				std::string(kObjectPath),
				std::string(kInterface),
				"NavigationStarted",
				{},
				base::Platform::MakeGlibVariant(std::tuple{
					Glib::ustring(uri),
					false
				}));

			if (resultId != 0) {
				loop->run();
			}

			return !result;
		} catch (...) {
		}
	}
	webkit_policy_decision_ignore(decision);
	return true;
}

GtkWidget *Instance::createAnother(WebKitNavigationAction *action) {
	WebKitURIRequest *request = webkit_navigation_action_get_request(action);
	const gchar *uri = webkit_uri_request_get_uri(request);
	if (_dbusConnection) {
		try {
			_dbusConnection->emit_signal(
				std::string(kObjectPath),
				std::string(kInterface),
				"NavigationStarted",
				{},
				base::Platform::MakeGlibVariant(std::tuple{
					Glib::ustring(uri),
					true
				}));
		} catch (...) {
		}
	}
	return nullptr;
}

bool Instance::scriptDialog(WebKitScriptDialog *dialog) {
	const auto type = webkit_script_dialog_get_dialog_type(dialog);
	const auto text = webkit_script_dialog_get_message(dialog);
	const auto value = (type == WEBKIT_SCRIPT_DIALOG_PROMPT)
		? webkit_script_dialog_prompt_get_default_text(dialog)
		: nullptr;
	if (_dbusConnection) {
		try {
			const auto loop = Glib::MainLoop::create();
			bool accepted = false;
			Glib::ustring result;

			const auto resultId = _dbusConnection->signal_subscribe(
				[&](
					const Glib::RefPtr<Gio::DBus::Connection> &connection,
					const Glib::ustring &sender_name,
					const Glib::ustring &object_path,
					const Glib::ustring &interface_name,
					const Glib::ustring &signal_name,
					Glib::VariantContainerBase parameters) {
					try {
						auto parametersCopy = parameters;

						accepted = base::Platform::GlibVariantCast<
							bool>(parametersCopy.get_child(0));
						result = base::Platform::GlibVariantCast<
							Glib::ustring>(parametersCopy.get_child(1));
					} catch (...) {
					}

					loop->quit();
				},
				{},
				std::string(kInterface),
				"ScriptDialogResult",
				std::string(kObjectPath));

			const auto resultGuard = gsl::finally([&] {
				if (resultId != 0) {
					_dbusConnection->signal_unsubscribe(resultId);
				}
			});

			_dbusConnection->emit_signal(
				std::string(kObjectPath),
				std::string(kInterface),
				"ScriptDialog",
				{},
				base::Platform::MakeGlibVariant(std::tuple{
					std::int32_t(type),
					Glib::ustring(text ? text : ""),
					Glib::ustring(value ? value : "")
				}));

			if (resultId != 0) {
				loop->run();
			}

			if (type == WEBKIT_SCRIPT_DIALOG_PROMPT) {
				webkit_script_dialog_prompt_set_text(
					dialog,
					accepted ? result.c_str() : nullptr);
			} else if (type != WEBKIT_SCRIPT_DIALOG_ALERT) {
				webkit_script_dialog_confirm_set_confirmed(dialog, accepted);
			}
			return true;
		} catch (...) {
		}
	}
	if (type == WEBKIT_SCRIPT_DIALOG_PROMPT) {
		webkit_script_dialog_prompt_set_text(dialog, nullptr);
	} else if (type != WEBKIT_SCRIPT_DIALOG_ALERT) {
		webkit_script_dialog_confirm_set_confirmed(dialog, false);
	}
	return true;
}

bool Instance::resolve() {
	if (_remoting) {
		if (!_dbusConnection) {
			return false;
		}

		try {
			auto reply = _dbusConnection->call_sync(
				std::string(kObjectPath),
				std::string(kInterface),
				"Resolve",
				{});

			return true;
		} catch (...) {
		}
	}

	return Resolve();
}

bool Instance::finishEmbedding() {
	if (_remoting) {
		if (!_dbusConnection) {
			return false;
		}

		try {
			auto reply = _dbusConnection->call_sync(
				std::string(kObjectPath),
				std::string(kInterface),
				"FinishEmbedding",
				{});

			return true;
		} catch (...) {
		}

		return false;
	}

	if (gtk_window_set_child) {
		gtk_window_set_child(GTK_WINDOW(_window), GTK_WIDGET(_webview));
	} else {
		gtk_container_add(GTK_CONTAINER(_window), GTK_WIDGET(_webview));
	}

	if (_debug) {
		WebKitSettings *settings = webkit_web_view_get_settings(
			WEBKIT_WEB_VIEW(_webview));
		//webkit_settings_set_javascript_can_access_clipboard(settings, true);
		g_object_set(
			G_OBJECT(settings),
			"enable-developer-extras",
			TRUE,
			NULL);
	}
	gtk_widget_hide(_window);
	if (gtk_widget_show) {
		gtk_widget_show(_window);
	} else {
		gtk_widget_show_all(_window);
	}
	gtk_widget_grab_focus(GTK_WIDGET(_webview));

	return true;
}

void Instance::navigate(std::string url) {
	if (_remoting) {
		if (!_dbusConnection) {
			return;
		}

		try {
			auto reply = _dbusConnection->call_sync(
				std::string(kObjectPath),
				std::string(kInterface),
				"Navigate",
				base::Platform::MakeGlibVariant(std::tuple{
					Glib::ustring(url),
				}));
		} catch (...) {
		}

		return;
	}

	webkit_web_view_load_uri(WEBKIT_WEB_VIEW(_webview), url.c_str());
}

void Instance::reload() {
	if (_remoting) {
		if (!_dbusConnection) {
			return;
		}

		try {
			auto reply = _dbusConnection->call_sync(
				std::string(kObjectPath),
				std::string(kInterface),
				"Reload",
				{});
		} catch (...) {
		}

		return;
	}

	webkit_web_view_reload_bypass_cache(WEBKIT_WEB_VIEW(_webview));
}

void Instance::init(std::string js) {
	if (_remoting) {
		if (!_dbusConnection) {
			return;
		}

		try {
			auto reply = _dbusConnection->call_sync(
				std::string(kObjectPath),
				std::string(kInterface),
				"Init",
				base::Platform::MakeGlibVariant(std::tuple{
					js,
				}));
		} catch (...) {
		}

		return;
	}

	WebKitUserContentManager *manager
		= webkit_web_view_get_user_content_manager(
			WEBKIT_WEB_VIEW(_webview));
	webkit_user_content_manager_add_script(
		manager,
		webkit_user_script_new(
			js.c_str(),
			WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
			WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
			nullptr,
			nullptr));
}

void Instance::eval(std::string js) {
	if (_remoting) {
		if (!_dbusConnection) {
			return;
		}

		try {
			auto reply = _dbusConnection->call_sync(
				std::string(kObjectPath),
				std::string(kInterface),
				"Eval",
				base::Platform::MakeGlibVariant(std::tuple{
					js,
				}));
		} catch (...) {
		}

		return;
	}

	webkit_web_view_run_javascript(
		WEBKIT_WEB_VIEW(_webview),
		js.c_str(),
		nullptr,
		nullptr,
		nullptr);
}

void *Instance::winId() {
	if (_remoting) {
		if (!_dbusConnection) {
			return nullptr;
		}

		try {
			auto reply = _dbusConnection->call_sync(
				std::string(kObjectPath),
				std::string(kInterface),
				"GetWinId",
				{});

			return reinterpret_cast<void*>(
				base::Platform::GlibVariantCast<guint64>(
					reply.get_child(0)));
		} catch (...) {
		}

		return nullptr;
	}

	if (gdk_x11_surface_get_xid
		&& gtk_widget_get_native
		&& gtk_native_get_surface) {
		return reinterpret_cast<void*>(gdk_x11_surface_get_xid(
			gtk_native_get_surface(
				gtk_widget_get_native(_window))));
	} else {
		return reinterpret_cast<void*>(gdk_x11_window_get_xid(
			gtk_widget_get_window(_window)));
	}
}

void Instance::resizeToWindow() {
	if (_remoting) {
		if (!_dbusConnection) {
			return;
		}

		try {
			auto reply = _dbusConnection->call_sync(
				std::string(kObjectPath),
				std::string(kInterface),
				"ResizeToWindow",
				{});
		} catch (...) {
		}

		return;
	}
}

void Instance::startProcess() {
	const auto executablePath = base::Integration::Instance()
		.executablePath()
		.toUtf8();

	_serviceProcess = GObjectPtr<GSubprocess>(g_subprocess_new(
		G_SUBPROCESS_FLAGS_NONE,
		nullptr,
		executablePath.constData(),
		"-webviewhelper",
		_debug ? "1" : "0",
		SocketPath.c_str(),
		nullptr));

	const auto socketPath = [&]() -> std::string {
		try {
			return Glib::Regex::create("%1")->replace(
				SocketPath,
				0,
				g_subprocess_get_identifier(_serviceProcess.get()),
				static_cast<Glib::RegexMatchFlags>(0));
		} catch (...) {
			return {};
		}
	}();

	const auto socketFile = Gio::File::create_for_path(socketPath);

	try {
		socketFile->remove();
	} catch (...) {
	}

	const auto loop = Glib::MainLoop::create();
	const auto socketMonitor = socketFile->monitor();
	socketMonitor->signal_changed().connect([&](
		const Glib::RefPtr<Gio::File> &file,
		const Glib::RefPtr<Gio::File> &otherFile,
		Gio::FileMonitorEvent eventType) {
		if (eventType == Gio::FILE_MONITOR_EVENT_CREATED) {
			loop->quit();
		}
	});

	// timeout in case something goes wrong
	const auto timeout = Glib::TimeoutSource::create(5000);
	timeout->connect([=] {
		if (loop->is_running()) {
			loop->quit();
		}
		return false;
	});
	timeout->attach();

	loop->run();

	_dbusConnection = [&] {
		try {
			return Gio::DBus::Connection::create_for_address_sync(
				SocketPathToDBusAddress(socketPath),
				Gio::DBus::CONNECTION_FLAGS_AUTHENTICATION_CLIENT);
		} catch (...) {
			return Glib::RefPtr<Gio::DBus::Connection>();
		}
	}();

	connectToRemoteSignals();
}

void Instance::connectToRemoteSignals() {
	if (!_dbusConnection) {
		return;
	}

	_dbusConnection->signal_closed().connect([=](
		bool remotePeerVanished,
		const Glib::Error &error) {
		startProcess();
	});

	_messageHandlerId = _dbusConnection->signal_subscribe(
		[=](
			const Glib::RefPtr<Gio::DBus::Connection> &connection,
			const Glib::ustring &sender_name,
			const Glib::ustring &object_path,
			const Glib::ustring &interface_name,
			const Glib::ustring &signal_name,
			Glib::VariantContainerBase parameters) {
			try {
				auto parametersCopy = parameters;

				const auto message = base::Platform::GlibVariantCast<
					std::string>(parametersCopy.get_child(0));

				_messageHandler(message);
			} catch (...) {
			}
		},
		{},
		std::string(kInterface),
		"MessageReceived",
		std::string(kObjectPath));

	if (_navigationStartHandler) {
		_navigationStartHandlerId = _dbusConnection->signal_subscribe(
			[=](
				const Glib::RefPtr<Gio::DBus::Connection> &connection,
				const Glib::ustring &sender_name,
				const Glib::ustring &object_path,
				const Glib::ustring &interface_name,
				const Glib::ustring &signal_name,
				Glib::VariantContainerBase parameters) {
				try {
					auto parametersCopy = parameters;

					const auto uri = base::Platform::GlibVariantCast<
						Glib::ustring>(parametersCopy.get_child(0));
					const auto newWindow = base::Platform::GlibVariantCast<
						bool>(parametersCopy.get_child(1));
					const auto result = [&] {
						if (newWindow) {
							if (_navigationStartHandler
								&& _navigationStartHandler(uri, true)) {
								QDesktopServices::openUrl(
									QString::fromUtf8(uri.c_str()));
							}
							return false;
						}
						return !_navigationStartHandler
							|| _navigationStartHandler(uri, false);
					}();

					_dbusConnection->emit_signal(
						std::string(kObjectPath),
						std::string(kInterface),
						"NavigationStartedResult",
						{},
						base::Platform::MakeGlibVariant(std::tuple{
							result,
						}));
				} catch (...) {
				}
			},
			{},
			std::string(kInterface),
			"NavigationStarted",
			std::string(kObjectPath));
	}

	if (_navigationDoneHandler) {
		_navigationDoneHandlerId = _dbusConnection->signal_subscribe(
			[=](
				const Glib::RefPtr<Gio::DBus::Connection> &connection,
				const Glib::ustring &sender_name,
				const Glib::ustring &object_path,
				const Glib::ustring &interface_name,
				const Glib::ustring &signal_name,
				Glib::VariantContainerBase parameters) {
				try {
					auto parametersCopy = parameters;

					const auto success = base::Platform::GlibVariantCast<
						bool>(parametersCopy.get_child(0));

					_navigationDoneHandler(success);
				} catch (...) {
				}
			},
			{},
			std::string(kInterface),
			"NavigationDone",
			std::string(kObjectPath));
	}
	if (_dialogHandler) {
		_scriptDialogHandlerId = _dbusConnection->signal_subscribe(
			[=](
				const Glib::RefPtr<Gio::DBus::Connection> &connection,
				const Glib::ustring &sender_name,
				const Glib::ustring &object_path,
				const Glib::ustring &interface_name,
				const Glib::ustring &signal_name,
				Glib::VariantContainerBase parameters) {
				try {
					auto parametersCopy = parameters;

					const auto type = base::Platform::GlibVariantCast<
						int>(parametersCopy.get_child(0));
					const auto text = base::Platform::GlibVariantCast<
						Glib::ustring>(parametersCopy.get_child(1));
					const auto value = base::Platform::GlibVariantCast<
						Glib::ustring>(parametersCopy.get_child(2));

					const auto dialogType = (type == WEBKIT_SCRIPT_DIALOG_PROMPT)
						? DialogType::Prompt
						: (type == WEBKIT_SCRIPT_DIALOG_ALERT)
						? DialogType::Alert
						: DialogType::Confirm;
					const auto result = _dialogHandler(DialogArgs{
						.type = dialogType,
						.value = value,
						.text = text,
					});
					_dbusConnection->emit_signal(
						std::string(kObjectPath),
						std::string(kInterface),
						"ScriptDialogResult",
						{},
						base::Platform::MakeGlibVariant(std::tuple{
							result.accepted,
							Glib::ustring(result.text),
						}));
				} catch (...) {
				}
			},
			{},
			std::string(kInterface),
			"ScriptDialog",
			std::string(kObjectPath));
	}
}

int Instance::exec() {
	const auto app = Gio::Application::create();
	app->hold();

	const auto introspectionData = Gio::DBus::NodeInfo::create_for_xml(
		std::string(kIntrospectionXML));

	const auto socketPath = Glib::Regex::create("%1")->replace(
		SocketPath,
		0,
		std::to_string(getpid()),
		static_cast<Glib::RegexMatchFlags>(0));

	const auto authObserver = Gio::DBus::AuthObserver::create();
	authObserver->signal_authorize_authenticated_peer().connect([](
		const Glib::RefPtr<const Gio::IOStream> &stream,
		const Glib::RefPtr<const Gio::Credentials> &credentials) {
		return credentials->get_unix_pid() == getppid();
	});

	const auto dbusServer = Gio::DBus::Server::create_sync(
		SocketPathToDBusAddress(socketPath),
		Gio::DBus::generate_guid(),
		authObserver);

	dbusServer->start();
	dbusServer->signal_new_connection().connect([=](
		const Glib::RefPtr<Gio::DBus::Connection> &connection) {
		if (_dbusConnection) {
			return false;
		}

		_dbusConnection = connection;

		_registerId = _dbusConnection->register_object(
			std::string(kObjectPath),
			introspectionData->lookup_interface(),
			_interfaceVTable);

		_dbusConnection->signal_closed().connect([=](
			bool remotePeerVanished,
			const Glib::Error &error) {
			app->quit();
		});

		return true;
	});

	return app->run(0, nullptr);
}

void Instance::handleMethodCall(
		const Glib::RefPtr<Gio::DBus::Connection> &connection,
		const Glib::ustring &sender,
		const Glib::ustring &object_path,
		const Glib::ustring &interface_name,
		const Glib::ustring &method_name,
		const Glib::VariantContainerBase &parameters,
		const Glib::RefPtr<Gio::DBus::MethodInvocation> &invocation) {
	try {
		auto parametersCopy = parameters;

		if (method_name == "Create") {
			create();
			invocation->return_value({});
			return;
		} else if (method_name == "Reload") {
			reload();
			invocation->return_value({});
			return;
		} else if (method_name == "Resolve") {
			if (resolve()) {
				invocation->return_value({});
				return;
			}
		} else if (method_name == "FinishEmbedding") {
			if (finishEmbedding()) {
				invocation->return_value({});
				return;
			}
		} else if (method_name == "Navigate") {
			const auto url = base::Platform::GlibVariantCast<
				Glib::ustring>(parametersCopy.get_child(0));

			navigate(url);
			invocation->return_value({});
			return;
		} else if (method_name == "ResizeToWindow") {
			resizeToWindow();
			invocation->return_value({});
			return;
		} else if (method_name == "Init") {
			const auto js = base::Platform::GlibVariantCast<
				std::string>(parametersCopy.get_child(0));

			init(js);
			invocation->return_value({});
			return;
		} else if (method_name == "Eval") {
			const auto js = base::Platform::GlibVariantCast<
				std::string>(parametersCopy.get_child(0));

			eval(js);
			invocation->return_value({});
			return;
		} else if (method_name == "GetWinId") {
			invocation->return_value(
				Glib::VariantContainerBase::create_tuple(
					Glib::Variant<guint64>::create(
						reinterpret_cast<guint64>(winId()))));

			return;
		}
	} catch (...) {
	}

	Gio::DBus::Error error(
		Gio::DBus::Error::UNKNOWN_METHOD,
		"Method does not exist.");

	invocation->return_error(error);
}

} // namespace

Available Availability() {
	if (Platform::IsWayland()) {
		return Available{
			.error = Available::Error::Wayland,
			.details = "There is no way to embed WebView window "
			"on Wayland. Please switch to X11."
		};
	} else if (const auto platform = Platform::GetWindowManager().toLower()
		; platform.contains("mutter") || platform.contains("gnome")) {
		return Available{
			.error = Available::Error::MutterWM,
			.details = "Qt's window embedding doesn't work well "
			"with Mutter window manager. Please switch to another "
			"window manager or desktop environment."
		};
	} else if (!Instance().resolve()) {
		return Available{
			.error = Available::Error::NoGtkOrWebkit2Gtk,
			.details = "Please install WebKitGTK "
			"(webkit2gtk-5.0/webkit2gtk-4.1/webkit2gtk-4.0) "
			"from your package manager.",
		};
	}
	return Available{};
}

std::unique_ptr<Interface> CreateInstance(Config config) {
	if (!Supported()) {
		return nullptr;
	}
	auto result = std::make_unique<Instance>(std::move(config));
	result->create();
	return result;
}

int Exec() {
	return Instance({}, false).exec();
}

void SetSocketPath(const std::string &socketPath) {
	SocketPath = socketPath;
}

void SetDebug(const std::string &debug) {
	DebugMode = !debug.empty() && (debug[0] == '1');
}

} // namespace Webview::WebKit2Gtk
