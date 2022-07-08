// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "webview/platform/win/webview_windows_edge_chromium.h"

#include "base/basic_types.h"

#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>

#include <string>
#include <locale>
#include <shlwapi.h>
#include <webview2.h>

namespace Webview::EdgeChromium {
namespace {

[[nodiscard]] std::wstring ToWide(std::string_view string) {
	const auto length = MultiByteToWideChar(
		CP_UTF8,
		0,
		string.data(),
		string.size(),
		nullptr,
		0);
	auto result = std::wstring(length, wchar_t{});
	MultiByteToWideChar(
		CP_UTF8,
		0,
		string.data(),
		string.size(),
		result.data(),
		result.size());
	return result;
}

[[nodiscard]] std::string FromWide(std::wstring_view string) {
	const auto length = WideCharToMultiByte(
		CP_UTF8,
		0,
		string.data(),
		string.size(),
		nullptr,
		0,
		nullptr,
		nullptr);
	auto result = std::string(length, char{});
	WideCharToMultiByte(
		CP_UTF8,
		0,
		string.data(),
		string.size(),
		result.data(),
		result.size(),
		nullptr,
		nullptr);
	return result;
}

class Handler final
	: public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
	, public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler
	, public ICoreWebView2WebMessageReceivedEventHandler
	, public ICoreWebView2PermissionRequestedEventHandler
	, public ICoreWebView2NavigationStartingEventHandler
	, public ICoreWebView2NavigationCompletedEventHandler
	, public ICoreWebView2NewWindowRequestedEventHandler
	, public ICoreWebView2ScriptDialogOpeningEventHandler {

public:
	Handler(
		Config config,
		std::function<void(ICoreWebView2Controller*)> readyHandler);

	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID *ppv);
	HRESULT STDMETHODCALLTYPE Invoke(
		HRESULT res,
		ICoreWebView2Environment *env) override;
	HRESULT STDMETHODCALLTYPE Invoke(
		HRESULT res,
		ICoreWebView2Controller *controller) override;
	HRESULT STDMETHODCALLTYPE Invoke(
		ICoreWebView2 *sender,
		ICoreWebView2WebMessageReceivedEventArgs *args) override;
	HRESULT STDMETHODCALLTYPE Invoke(
		ICoreWebView2 *sender,
		ICoreWebView2PermissionRequestedEventArgs *args) override;
	HRESULT STDMETHODCALLTYPE Invoke(
		ICoreWebView2 *sender,
		ICoreWebView2NavigationStartingEventArgs *args) override;
	HRESULT STDMETHODCALLTYPE Invoke(
		ICoreWebView2 *sender,
		ICoreWebView2NavigationCompletedEventArgs *args) override;
	HRESULT STDMETHODCALLTYPE Invoke(
		ICoreWebView2 *sender,
		ICoreWebView2NewWindowRequestedEventArgs *args) override;
	HRESULT STDMETHODCALLTYPE Invoke(
		ICoreWebView2 *sender,
		ICoreWebView2ScriptDialogOpeningEventArgs *args) override;

private:
	HWND _window = nullptr;
	std::function<void(std::string)> _messageHandler;
	std::function<bool(std::string, bool)> _navigationStartHandler;
	std::function<void(bool)> _navigationDoneHandler;
	std::function<DialogResult(DialogArgs)> _dialogHandler;
	std::function<void(ICoreWebView2Controller*)> _readyHandler;

};

Handler::Handler(
	Config config,
	std::function<void(ICoreWebView2Controller*)> readyHandler)
: _window(static_cast<HWND>(config.window))
, _messageHandler(std::move(config.messageHandler))
, _navigationStartHandler(std::move(config.navigationStartHandler))
, _navigationDoneHandler(std::move(config.navigationDoneHandler))
, _dialogHandler(std::move(config.dialogHandler))
, _readyHandler(std::move(readyHandler)) {
}

ULONG STDMETHODCALLTYPE Handler::AddRef() {
	return 1;
}

ULONG STDMETHODCALLTYPE Handler::Release() {
	return 1;
}

HRESULT STDMETHODCALLTYPE Handler::QueryInterface(REFIID riid, LPVOID *ppv) {
	return S_OK;
}

HRESULT STDMETHODCALLTYPE Handler::Invoke(
		HRESULT res,
		ICoreWebView2Environment *env) {
	if (!env) {
		return S_FALSE;
	}
	env->CreateCoreWebView2Controller(_window, this);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE Handler::Invoke(
		HRESULT res,
		ICoreWebView2Controller *controller) {
	if (!_readyHandler) {
		return S_FALSE;
	}
	const auto guard = gsl::finally([&] {
		const auto ready = _readyHandler;
		_readyHandler = nullptr;
		ready(controller);
	});
	if (!controller) {
		return S_FALSE;
	}

	auto webview = (ICoreWebView2*)nullptr;
	controller->get_CoreWebView2(&webview);
	if (!webview) {
		return S_FALSE;
	}
	auto token = ::EventRegistrationToken();
	webview->add_WebMessageReceived(this, &token);
	webview->add_PermissionRequested(this, &token);
	webview->add_NavigationStarting(this, &token);
	webview->add_NavigationCompleted(this, &token);
	webview->add_NewWindowRequested(this, &token);
	webview->add_ScriptDialogOpening(this, &token);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE Handler::Invoke(
		ICoreWebView2 *sender,
		ICoreWebView2WebMessageReceivedEventArgs *args) {
	auto message = LPWSTR{};
	const auto result = args->TryGetWebMessageAsString(&message);

	if (result == S_OK && message) {
		_messageHandler(FromWide(message));
		sender->PostWebMessageAsString(message);
	}

	CoTaskMemFree(message);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE Handler::Invoke(
		ICoreWebView2 *sender,
		ICoreWebView2PermissionRequestedEventArgs *args) {
	auto kind = COREWEBVIEW2_PERMISSION_KIND{};
	const auto result = args->get_PermissionKind(&kind);
	if (result == S_OK) {
		if (kind == COREWEBVIEW2_PERMISSION_KIND_CLIPBOARD_READ) {
			args->put_State(COREWEBVIEW2_PERMISSION_STATE_ALLOW);
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE Handler::Invoke(
		ICoreWebView2 *sender,
		ICoreWebView2NavigationStartingEventArgs *args) {
	auto uri = LPWSTR{};
	const auto result = args->get_Uri(&uri);

	if (result == S_OK && uri) {
		if (_navigationStartHandler && !_navigationStartHandler(FromWide(uri), false)) {
			args->put_Cancel(TRUE);
		}
	}

	CoTaskMemFree(uri);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE Handler::Invoke(
		ICoreWebView2 *sender,
		ICoreWebView2NavigationCompletedEventArgs *args) {
	auto isSuccess = BOOL(FALSE);
	const auto result = args->get_IsSuccess(&isSuccess);

	if (_navigationDoneHandler) {
		_navigationDoneHandler(result == S_OK && isSuccess);
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE Handler::Invoke(
		ICoreWebView2 *sender,
		ICoreWebView2NewWindowRequestedEventArgs *args) {
	auto uri = LPWSTR{};
	const auto result = args->get_Uri(&uri);
	auto isUserInitiated = BOOL{};
	args->get_IsUserInitiated(&isUserInitiated);
	args->put_Handled(TRUE);

	if (result == S_OK && uri && isUserInitiated) {
		const auto url = FromWide(uri);
		if (_navigationStartHandler && _navigationStartHandler(url, true)) {
			QDesktopServices::openUrl(QString::fromStdString(url));
		}
	}

	CoTaskMemFree(uri);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE Handler::Invoke(
		ICoreWebView2 *sender,
		ICoreWebView2ScriptDialogOpeningEventArgs *args) {
	auto kind = COREWEBVIEW2_SCRIPT_DIALOG_KIND_ALERT;
	auto hr = args->get_Kind(&kind);
	if (hr != S_OK) {
		return S_OK;
	}

	auto uri = LPWSTR{};
	hr = args->get_Uri(&uri);
	if (hr != S_OK || !uri) {
		return S_OK;
	}
	const auto uriGuard = gsl::finally([&] { CoTaskMemFree(uri); });

	auto text = LPWSTR{};
	hr = args->get_Message(&text);
	if (hr != S_OK || !text) {
		return S_OK;
	}
	const auto textGuard = gsl::finally([&] { CoTaskMemFree(text); });

	auto value = LPWSTR{};
	hr = args->get_DefaultText(&value);
	if (hr != S_OK || !value) {
		return S_OK;
	}
	const auto valueGuard = gsl::finally([&] { CoTaskMemFree(value); });

	const auto type = [&] {
		switch (kind) {
		case COREWEBVIEW2_SCRIPT_DIALOG_KIND_ALERT:
			return DialogType::Alert;
		case COREWEBVIEW2_SCRIPT_DIALOG_KIND_CONFIRM:
			return DialogType::Confirm;
		case COREWEBVIEW2_SCRIPT_DIALOG_KIND_PROMPT:
			return DialogType::Prompt;
		}
		return DialogType::Alert;
	}();
	const auto result = _dialogHandler(DialogArgs{
		.type = type,
		.value = FromWide(value),
		.text = FromWide(text),
		.url = FromWide(uri),
	});

	if (result.accepted) {
		args->Accept();
		if (kind == COREWEBVIEW2_SCRIPT_DIALOG_KIND_PROMPT) {
			const auto wide = ToWide(result.text);
			args->put_ResultText(wide.c_str());
		}
	}
	return S_OK;
}

class Instance final : public Interface {
public:
	Instance(
		void *window,
		ICoreWebView2Controller *controller,
		ICoreWebView2 *webview,
		std::unique_ptr<Handler> handler);
	~Instance();

	bool finishEmbedding() override;

	void navigate(std::string url) override;
	void reload() override;

	void resizeToWindow() override;

	void init(std::string js) override;
	void eval(std::string js) override;

	void *winId() override;

private:
	HWND _window = nullptr;
	ICoreWebView2Controller *_controller = nullptr;
	ICoreWebView2 *_webview = nullptr;
	std::unique_ptr<Handler> _handler;

};

Instance::Instance(
	void *window,
	ICoreWebView2Controller *controller,
	ICoreWebView2 *webview,
	std::unique_ptr<Handler> handler)
: _window(static_cast<HWND>(window))
, _controller(controller)
, _webview(webview)
, _handler(std::move(handler)) {
	init("window.external={invoke:s=>window.chrome.webview.postMessage(s)}");
}

Instance::~Instance() {
	CoUninitialize();
	_webview->Release();
	_controller->Release();
}

bool Instance::finishEmbedding() {
	_controller->put_IsVisible(TRUE);
	return true;
}

void Instance::navigate(std::string url) {
	const auto wide = ToWide(url);
	_webview->Navigate(wide.c_str());
}

void Instance::reload() {
	_webview->Reload();
}

void Instance::resizeToWindow() {
	auto bounds = RECT{};
	GetClientRect(_window, &bounds);
	_controller->put_Bounds(bounds);
}

void Instance::init(std::string js) {
	const auto wide = ToWide(js);
	_webview->AddScriptToExecuteOnDocumentCreated(wide.c_str(), nullptr);
}

void Instance::eval(std::string js) {
	const auto wide = ToWide(js);
	_webview->ExecuteScript(wide.c_str(), nullptr);
}

void *Instance::winId() {
	return nullptr;
}

} // namespace

bool Supported() {
	auto version = LPWSTR(nullptr);
	const auto result = GetAvailableCoreWebView2BrowserVersionString(
		nullptr,
		&version);
	return (result == S_OK);
}

std::unique_ptr<Interface> CreateInstance(Config config) {
	if (!Supported()) {
		return nullptr;
	}
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	auto controller = (ICoreWebView2Controller*)nullptr;
	auto webview = (ICoreWebView2*)nullptr;
	const auto event = CreateEvent(nullptr, false, false, nullptr);
	const auto guard = gsl::finally([&] { CloseHandle(event); });
	const auto debug = config.debug;
	const auto ready = [&](ICoreWebView2Controller *created) {
		const auto guard = gsl::finally([&] { SetEvent(event); });
		controller = created;
		if (!controller) {
			return;
		}
		controller->get_CoreWebView2(&webview);
		if (!webview) {
			return;
		}
		auto settings = (ICoreWebView2Settings*)nullptr;
		const auto result = webview->get_Settings(&settings);
		if (result != S_OK || !settings) {
			return;
		}
		settings->put_AreDefaultContextMenusEnabled(debug);
		settings->put_AreDevToolsEnabled(debug);
		settings->put_AreDefaultScriptDialogsEnabled(FALSE);
		settings->put_IsStatusBarEnabled(FALSE);

		controller->AddRef();
		webview->AddRef();
	};
	auto handler = std::make_unique<Handler>(config, ready);
	const auto wpath = ToWide(config.userDataPath);
	const auto result = CreateCoreWebView2EnvironmentWithOptions(
		nullptr,
		wpath.empty() ? nullptr : wpath.c_str(),
		nullptr,
		handler.get());
	if (result != S_OK) {
		CoUninitialize();
		return nullptr;
	}
	HANDLE handles[] = { event };
	auto index = DWORD{};
	const auto flags = COWAIT_DISPATCH_WINDOW_MESSAGES |
		COWAIT_DISPATCH_CALLS |
		COWAIT_INPUTAVAILABLE;
	CoWaitForMultipleHandles(flags, INFINITE, 1, handles, &index);

	if (!controller || !webview) {
		CoUninitialize();
		return nullptr;
	}
	return std::make_unique<Instance>(
		config.window,
		controller,
		webview,
		std::move(handler));
}

} // namespace Webview::EdgeChromium

