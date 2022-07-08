// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "webview/platform/win/webview_windows_edge_html.h"

#include "base/platform/win/base_windows_winrt.h"

#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.UI.Interop.h>

namespace Webview::EdgeHtml {
namespace {

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Web::UI;
using namespace Windows::Web::UI::Interop;
using namespace base::WinRT;

class Instance final : public Interface {
public:
	Instance(Config config, WebViewControl webview);

	bool finishEmbedding() override;

	void navigate(std::string url) override;
	void reload() override;

	void resizeToWindow() override;

	void init(std::string js) override;
	void eval(std::string js) override;

	void *winId() override;

private:
	HWND _window = nullptr;
	WebViewControl _webview = nullptr;
	std::string _initScript;

};

Instance::Instance(Config config, WebViewControl webview)
: _window(static_cast<HWND>(config.window))
, _webview(std::move(webview)) {
	_webview.Settings().IsScriptNotifyAllowed(true);
	_webview.IsVisible(true);
	_webview.ScriptNotify([handler = config.messageHandler](
			const auto &sender,
			const WebViewControlScriptNotifyEventArgs &args) {
		if (handler) {
			handler(winrt::to_string(args.Value()));
		}
	});
	_webview.NavigationStarting([=, handler = config.navigationStartHandler](
			const auto &sender,
			const WebViewControlNavigationStartingEventArgs &args) {
		if (handler
			&& !handler(winrt::to_string(args.Uri().AbsoluteUri()), false)) {
			args.Cancel(true);
		}
		_webview.AddInitializeScript(winrt::to_hstring(_initScript));
	});
	_webview.NavigationCompleted([=, handler = config.navigationDoneHandler](
			const auto &sender,
			const WebViewControlNavigationCompletedEventArgs &args) {
		if (handler) {
			handler(args.IsSuccess());
		}
	});
	_webview.NewWindowRequested([=, handler = config.navigationStartHandler](
			const auto &sender,
			const WebViewControlNewWindowRequestedEventArgs &args) {
		const auto url = winrt::to_string(args.Uri().AbsoluteUri());
		if (handler && handler(url, true)) {
			QDesktopServices::openUrl(QString::fromStdString(url));
		}
	});
	init("window.external.invoke = s => window.external.notify(s)");
}

bool Instance::finishEmbedding() {
	return true;
}

void Instance::navigate(std::string url) {
	_webview.Navigate(Uri(winrt::to_hstring(url)));
}

void Instance::reload() {
	_webview.Refresh();
}

void Instance::init(std::string js) {
	_initScript = _initScript + "(function(){" + js + "})();";
}

void Instance::eval(std::string js) {
	_webview.InvokeScriptAsync(
		L"eval",
		single_threaded_vector<hstring>({ winrt::to_hstring(js) }));
}

void *Instance::winId() {
	return nullptr;
}

void Instance::resizeToWindow() {
	RECT r;
	GetClientRect(_window, &r);
	Rect bounds(r.left, r.top, r.right - r.left, r.bottom - r.top);
	_webview.Bounds(bounds);
}

} // namespace

bool Supported() {
	return Try([&] {
		return (WebViewControlProcess() != nullptr);
	}).value_or(false);
}

std::unique_ptr<Interface> CreateInstance(Config config) {
	return Try([&]() -> std::unique_ptr<Interface> {
		init_apartment(apartment_type::single_threaded);
		auto process = WebViewControlProcess();
		auto op = process.CreateWebViewControlAsync(
			reinterpret_cast<int64_t>(config.window),
			Rect());
		if (op.Status() == AsyncStatus::Started) {
			const auto event = handle(
				CreateEvent(nullptr, false, false, nullptr));
			op.Completed([handle = event.get()](auto, auto) {
				SetEvent(handle);
			});
			HANDLE handles[] = { event.get() };
			auto index = DWORD{};
			const auto flags = COWAIT_DISPATCH_WINDOW_MESSAGES |
				COWAIT_DISPATCH_CALLS |
				COWAIT_INPUTAVAILABLE;
			CoWaitForMultipleHandles(flags, INFINITE, 1, handles, &index);
		}
		auto webview = op.GetResults();
		if (!webview) {
			return nullptr;
		}
		return std::make_unique<Instance>(
			std::move(config),
			std::move(webview));
	}).value_or(nullptr);
}

} // namespace Webview::EdgeHtml
