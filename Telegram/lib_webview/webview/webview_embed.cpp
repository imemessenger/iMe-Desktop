// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "webview/webview_embed.h"

#include "webview/webview_interface.h"
#include "webview/webview_dialog.h"
#include "base/event_filter.h"
#include "base/options.h"
#include "base/invoke_queued.h"
#include "base/platform/base_platform_info.h"
#include "base/integration.h"

#include <QtWidgets/QWidget>
#include <QtGui/QWindow>
#include <QtCore/QJsonDocument>

namespace Webview {
namespace {

[[nodiscard]] QWindow *CreateContainerWindow() {
	if constexpr (Platform::IsWindows()) {
		const auto result = new QWindow();
		result->setFlag(Qt::FramelessWindowHint);
		return result;
	} else {
		return nullptr;
	}
}

[[nodiscard]] QWindow *CreateContainerWindow(not_null<Interface*> webview) {
	const auto id = webview->winId();
	return id ? QWindow::fromWinId(WId(id)) : nullptr;
}

base::options::toggle OptionWebviewDebugEnabled({
	.id = kOptionWebviewDebugEnabled,
	.name = "Enable webview inspecting",
	.description = "Right click and choose Inspect in the webview windows.",
	.scope = base::options::windows | base::options::linux,
});

} // namespace

const char kOptionWebviewDebugEnabled[] = "webview-debug-enabled";

Window::Window(QWidget *parent, WindowConfig config)
: _window(CreateContainerWindow()) {
	if (SupportsEmbedAfterCreate()) {
		if (!createWebView(config)) {
			return;
		}
		if (!_window) {
			_window = CreateContainerWindow(_webview.get());
		}
	}
	if (!_window) {
		return;
	}
	_widget.reset(
		QWidget::createWindowContainer(
			_window,
			parent,
			Qt::FramelessWindowHint));
	_widget->show();
	if (!createWebView(config) || !finishWebviewEmbedding()) {
		return;
	}
	_webview->resizeToWindow();
	base::install_event_filter(_widget, [=](not_null<QEvent*> e) {
		if (e->type() == QEvent::Resize || e->type() == QEvent::Move) {
			InvokeQueued(_widget.get(), [=] { _webview->resizeToWindow(); });
		}
		return base::EventFilterResult::Continue;
	});
	setDialogHandler(nullptr);
}

Window::~Window() = default;

bool Window::createWebView(const WindowConfig &config) {
	if (!_webview) {
		_webview = CreateInstance({
			.window = _window ? (void*)_window->winId() : nullptr,
			.messageHandler = messageHandler(),
			.navigationStartHandler = navigationStartHandler(),
			.navigationDoneHandler = navigationDoneHandler(),
			.dialogHandler = dialogHandler(),
			.userDataPath = config.userDataPath.toStdString(),
			.debug = OptionWebviewDebugEnabled.value(),
		});
	}
	if (_webview) {
		return true;
	}
	delete _window;
	_window = nullptr;
	_widget = nullptr;
	return false;
}

bool Window::finishWebviewEmbedding() {
	Expects(_webview != nullptr);
	Expects(_widget != nullptr);
	Expects(_window != nullptr);

	if (_webview->finishEmbedding()) {
		return true;
	}
	delete _window;
	_window = nullptr;
	_widget = nullptr;
	_webview = nullptr;
	return false;
}

void Window::updateTheme(
		QColor scrollBg,
		QColor scrollBgOver,
		QColor scrollBarBg,
		QColor scrollBarBgOver) {
	if (!_webview) {
		return;
	}
	const auto wrap = [](QColor color) {
		return u"rgba(%1, %2, %3, %4)"_q
			.arg(color.red())
			.arg(color.green())
			.arg(color.blue())
			.arg(color.alphaF()).toStdString();
	};
	const auto function = R"(
function() {
	const style = document.createElement('style');
	style.textContent = ' \
::-webkit-scrollbar { \
	border-radius: 5px !important; \
	border: 3px solid transparent !important; \
	background-color: )" + wrap(scrollBg) + R"( !important; \
	background-clip: content-box !important; \
	width: 10px !important; \
} \
::-webkit-scrollbar:hover { \
	background-color: )" + wrap(scrollBgOver) + R"( !important; \
} \
::-webkit-scrollbar-thumb { \
	border-radius: 5px !important; \
	border: 3px solid transparent !important; \
	background-color: )" + wrap(scrollBarBg) + R"( !important; \
	background-clip: content-box !important; \
} \
::-webkit-scrollbar-thumb:hover { \
	background-color: )" + wrap(scrollBarBgOver) + R"( !important; \
} \
';
  document.head.append(style);
}
)";
	_webview->init(
		"document.addEventListener('DOMContentLoaded', "
		+ function
		+ ", false);");
	_webview->eval("(" + function + "());");
}

void Window::navigate(const QString &url) {
	Expects(_webview != nullptr);

	_webview->navigate(url.toStdString());
}

void Window::reload() {
	Expects(_webview != nullptr);

	_webview->reload();
}

void Window::init(const QByteArray &js) {
	Expects(_webview != nullptr);

	_webview->init(js.toStdString());
}

void Window::eval(const QByteArray &js) {
	Expects(_webview != nullptr);

	_webview->eval(js.toStdString());
}

void Window::setMessageHandler(Fn<void(std::string)> handler) {
	_messageHandler = std::move(handler);
}

void Window::setMessageHandler(Fn<void(const QJsonDocument&)> handler) {
	if (!handler) {
		setMessageHandler(Fn<void(std::string)>());
		return;
	}
	setMessageHandler([=](std::string text) {
		auto error = QJsonParseError();
		auto document = QJsonDocument::fromJson(
			QByteArray::fromRawData(text.data(), text.size()),
			&error);
		if (error.error == QJsonParseError::NoError) {
			handler(std::move(document));
		}
	});
}

Fn<void(std::string)> Window::messageHandler() const {
	return [=](std::string message) {
		if (_messageHandler) {
			base::Integration::Instance().enterFromEventLoop([&] {
				_messageHandler(std::move(message));
			});
		}
	};
}

void Window::setNavigationStartHandler(Fn<bool(QString,bool)> handler) {
	if (!handler) {
		_navigationStartHandler = nullptr;
		return;
	}
	_navigationStartHandler = [=](std::string uri, bool newWindow) {
		return handler(QString::fromStdString(uri), newWindow);
	};
}

void Window::setNavigationDoneHandler(Fn<void(bool)> handler) {
	_navigationDoneHandler = std::move(handler);
}

void Window::setDialogHandler(Fn<DialogResult(DialogArgs)> handler) {
	_dialogHandler = handler ? handler : DefaultDialogHandler;
}

Fn<bool(std::string,bool)> Window::navigationStartHandler() const {
	return [=](std::string message, bool newWindow) {
		const auto lower = QString::fromStdString(message).toLower();
		if (!lower.startsWith("http://") && !lower.startsWith("https://")) {
			return false;
		}
		auto result = true;
		if (_navigationStartHandler) {
			base::Integration::Instance().enterFromEventLoop([&] {
				result = _navigationStartHandler(
					std::move(message),
					newWindow);
			});
		}
		return result;
	};
}

Fn<void(bool)> Window::navigationDoneHandler() const {
	return [=](bool success) {
		if (_navigationDoneHandler) {
			base::Integration::Instance().enterFromEventLoop([&] {
				_navigationDoneHandler(success);
			});
		}
	};
}

Fn<DialogResult(DialogArgs)> Window::dialogHandler() const {
	return [=](DialogArgs args) {
		auto result = DialogResult();
		if (_dialogHandler) {
			base::Integration::Instance().enterFromEventLoop([&] {
				args.parent = _widget ? _widget->window() : nullptr;
				result = _dialogHandler(std::move(args));
			});
		}
		return result;
	};
}

} // namespace Webview
