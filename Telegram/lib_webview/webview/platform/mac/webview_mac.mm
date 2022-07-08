// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "webview/platform/mac/webview_mac.h"

#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

@interface Handler : NSObject<WKScriptMessageHandler, WKNavigationDelegate, WKUIDelegate> {
}

- (id) initWithMessageHandler:(std::function<void(std::string)>)messageHandler navigationStartHandler:(std::function<bool(std::string,bool)>)navigationStartHandler navigationDoneHandler:(std::function<void(bool)>)navigationDoneHandler dialogHandler:(std::function<Webview::DialogResult(Webview::DialogArgs)>)dialogHandler;
- (void) userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message;
- (void) webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler;
- (void) webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation;
- (void) webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error;
- (nullable WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures;
- (void)webView:(WKWebView *)webView runOpenPanelWithParameters:(WKOpenPanelParameters *)parameters initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSArray<NSURL *> * _Nullable URLs))completionHandler;
- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler;
- (void)webView:(WKWebView *)webView runJavaScriptConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL result))completionHandler;
- (void)webView:(WKWebView *)webView runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSString *result))completionHandler;
- (void) dealloc;

@end // @interface Handler

@implementation Handler {
	std::function<void(std::string)> _messageHandler;
	std::function<bool(std::string,bool)> _navigationStartHandler;
	std::function<void(bool)> _navigationDoneHandler;
	std::function<Webview::DialogResult(Webview::DialogArgs)> _dialogHandler;
}

- (id) initWithMessageHandler:(std::function<void(std::string)>)messageHandler navigationStartHandler:(std::function<bool(std::string,bool)>)navigationStartHandler navigationDoneHandler:(std::function<void(bool)>)navigationDoneHandler dialogHandler:(std::function<Webview::DialogResult(Webview::DialogArgs)>)dialogHandler {
	if (self = [super init]) {
		_messageHandler = std::move(messageHandler);
		_navigationStartHandler = std::move(navigationStartHandler);
		_navigationDoneHandler = std::move(navigationDoneHandler);
		_dialogHandler = std::move(dialogHandler);
	}
	return self;
}

- (void) userContentController:(WKUserContentController *)userContentController
	   didReceiveScriptMessage:(WKScriptMessage *)message {
	id body = [message body];
	if ([body isKindOfClass:[NSString class]]) {
		NSString *string = (NSString*)body;
		_messageHandler([string UTF8String]);
	}
}

- (void) webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler {
	NSString *string = [[[navigationAction request] URL] absoluteString];
	WKFrameInfo *target = [navigationAction targetFrame];
	const auto newWindow = !target || ![target isMainFrame];
	const auto url = [string UTF8String];
	if (newWindow) {
		if (_navigationStartHandler && _navigationStartHandler(url, true)) {
			QDesktopServices::openUrl(QString::fromUtf8(url));
		}
		decisionHandler(WKNavigationActionPolicyCancel);
	} else {
		if (_navigationStartHandler && !_navigationStartHandler(url, false)) {
			decisionHandler(WKNavigationActionPolicyCancel);
		} else {
			decisionHandler(WKNavigationActionPolicyAllow);
		}
	}
}

- (void) webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation {
	if (_navigationDoneHandler) {
		_navigationDoneHandler(true);
	}
}

- (void) webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error {
	if (_navigationDoneHandler) {
		_navigationDoneHandler(false);
	}
}

- (nullable WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures {
	NSString *string = [[[navigationAction request] URL] absoluteString];
	const auto url = [string UTF8String];
	if (_navigationStartHandler && _navigationStartHandler(url, true)) {
		QDesktopServices::openUrl(QString::fromUtf8(url));
	}
	return nil;
}

- (void)webView:(WKWebView *)webView runOpenPanelWithParameters:(WKOpenPanelParameters *)parameters initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSArray<NSURL *> * _Nullable URLs))completionHandler {

	NSOpenPanel *openPanel = [NSOpenPanel openPanel];

	if (@available(macOS 10.13.4, *)) {
		[openPanel setCanChooseDirectories:parameters.allowsDirectories];
	}
	[openPanel setCanChooseFiles:YES];
	[openPanel setAllowsMultipleSelection:parameters.allowsMultipleSelection];
	[openPanel setResolvesAliases:YES];

	[openPanel beginWithCompletionHandler:^(NSInteger result){
		if (result == NSFileHandlingPanelOKButton) {
			completionHandler([openPanel URLs]);
		}
	}];

}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler {
	auto text = [message UTF8String];
	auto uri = [[[frame request] URL] absoluteString];
	auto url = [uri UTF8String];
	const auto result = _dialogHandler(Webview::DialogArgs{
		.type = Webview::DialogType::Alert,
		.text = text,
		.url = url,
	});
	completionHandler();
}

- (void)webView:(WKWebView *)webView runJavaScriptConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL result))completionHandler {
	auto text = [message UTF8String];
	auto uri = [[[frame request] URL] absoluteString];
	auto url = [uri UTF8String];
	const auto result = _dialogHandler(Webview::DialogArgs{
		.type = Webview::DialogType::Confirm,
		.text = text,
		.url = url,
	});
	completionHandler(result.accepted ? YES : NO);
}

- (void)webView:(WKWebView *)webView runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSString *result))completionHandler {
	auto text = [prompt UTF8String];
	auto value = [defaultText UTF8String];
	auto uri = [[[frame request] URL] absoluteString];
	auto url = [uri UTF8String];
	const auto result = _dialogHandler(Webview::DialogArgs{
		.type = Webview::DialogType::Prompt,
		.value = value,
		.text = text,
		.url = url,
	});
	if (result.accepted) {
		completionHandler([NSString stringWithUTF8String:result.text.c_str()]);
	} else {
		completionHandler(nil);
	}
}

- (void) dealloc {
	[super dealloc];
}

@end // @implementation Handler

namespace Webview {
namespace {

class Instance final : public Interface {
public:
	explicit Instance(Config config);
	~Instance();

	bool finishEmbedding() override;

	void navigate(std::string url) override;
	void reload() override;

	void resizeToWindow() override;

	void init(std::string js) override;
	void eval(std::string js) override;

	void *winId() override;

private:
	WKUserContentController *_manager = nullptr;
	WKWebView *_webview = nullptr;
	Handler *_handler = nullptr;

};

Instance::Instance(Config config) {
	WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
	_manager = configuration.userContentController;
	_webview = [[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration];
	_handler = [[Handler alloc] initWithMessageHandler:config.messageHandler navigationStartHandler:config.navigationStartHandler navigationDoneHandler:config.navigationDoneHandler dialogHandler:config.dialogHandler];
	[_manager addScriptMessageHandler:_handler name:@"external"];
	[_webview setNavigationDelegate:_handler];
	[_webview setUIDelegate:_handler];
	[configuration release];

	init(R"(
window.external = {
	invoke: function(s) {
		window.webkit.messageHandlers.external.postMessage(s);
	}
};)");
}

Instance::~Instance() {
	[_manager removeScriptMessageHandlerForName:@"external"];
	[_webview setNavigationDelegate:nil];
	[_handler release];
	[_webview release];
}

bool Instance::finishEmbedding() {
	return true;
}

void Instance::navigate(std::string url) {
	NSString *string = [NSString stringWithUTF8String:url.c_str()];
	NSURL *native = [NSURL URLWithString:string];
	[_webview loadRequest:[NSURLRequest requestWithURL:native]];
}

void Instance::reload() {
	[_webview reload];
}

void Instance::init(std::string js) {
	NSString *string = [NSString stringWithUTF8String:js.c_str()];
	WKUserScript *script = [[WKUserScript alloc] initWithSource:string injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES];
	[_manager addUserScript:script];
}

void Instance::eval(std::string js) {
	NSString *string = [NSString stringWithUTF8String:js.c_str()];
	[_webview evaluateJavaScript:string completionHandler:nil];
}

void *Instance::winId() {
	return _webview;
}

void Instance::resizeToWindow() {
}

} // namespace

Available Availability() {
	return Available{};
}

bool SupportsEmbedAfterCreate() {
	return true;
}

std::unique_ptr<Interface> CreateInstance(Config config) {
	if (!Supported()) {
		return nullptr;
	}
	return std::make_unique<Instance>(std::move(config));
}

} // namespace Webview
