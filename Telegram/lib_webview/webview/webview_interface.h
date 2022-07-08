// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <memory>
#include <string>
#include <optional>
#include <functional>

#include <QtGui/QColor>

// Inspired by https://github.com/webview/webview.

class QWidget;

namespace Webview {

struct ThemeParams {
	QColor scrollBg;
	QColor scrollBgOver;
	QColor scrollBarBg;
	QColor scrollBarBgOver;
	QByteArray json;
};

class Interface {
public:
	virtual ~Interface() = default;

	virtual bool finishEmbedding() = 0;

	virtual void navigate(std::string url) = 0;
	virtual void reload() = 0;

	virtual void resizeToWindow() = 0;

	virtual void init(std::string js) = 0;
	virtual void eval(std::string js) = 0;

	virtual void *winId() = 0;

};

enum class DialogType {
	Alert,
	Confirm,
	Prompt,
};

struct DialogArgs {
	QWidget *parent = nullptr;
	DialogType type = DialogType::Alert;
	std::string value;
	std::string text;
	std::string url;
};

struct DialogResult {
	std::string text;
	bool accepted = false;
};

struct Config {
	void *window = nullptr;
	std::function<void(std::string)> messageHandler;
	std::function<bool(std::string,bool)> navigationStartHandler;
	std::function<void(bool)> navigationDoneHandler;
	std::function<DialogResult(DialogArgs)> dialogHandler;
	std::string userDataPath;
	bool debug = false;
};

struct Available {
	enum class Error {
		None,
		NoWebview2,
		NoGtkOrWebkit2Gtk,
		MutterWM,
		Wayland,
		OldWindows,
	};
	Error error = Error::None;
	std::string details;
};

[[nodiscard]] Available Availability();
[[nodiscard]] inline bool Supported() {
	return Availability().error == Available::Error::None;
}
[[nodiscard]] bool SupportsEmbedAfterCreate();

// HWND on Windows, nullptr on macOS, GtkWindow on Linux.
[[nodiscard]] std::unique_ptr<Interface> CreateInstance(Config config);

} // namespace Webview
