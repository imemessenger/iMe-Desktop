/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "base/platform/linux/base_linux_wayland_integration.h"

#include "base/platform/base_platform_info.h"
#include "base/qt_signal_producer.h"
#include "base/flat_map.h"
#include "qwayland-xdg-foreign-unstable-v2.h"
#include "qwayland-idle-inhibit-unstable-v1.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#include <qpa/qplatformnativeinterface.h>
#include <wayland-client.h>

namespace base {
namespace Platform {
namespace {

struct WlRegistryDeleter {
	void operator()(wl_registry *value) {
		wl_registry_destroy(value);
	}
};

struct IdleInhibitorDeleter {
	void operator()(zwp_idle_inhibitor_v1 *value) {
		zwp_idle_inhibitor_v1_destroy(value);
	}
};

template <typename T>
class QtWaylandAutoDestroyer : public T {
public:
	QtWaylandAutoDestroyer() = default;

	~QtWaylandAutoDestroyer() {
		if (!this->isInitialized()) {
			return;
		}

		static constexpr auto HasDestroy = requires(const T &t) {
			t.destroy();
		};

		if constexpr (HasDestroy) {
			this->destroy();
		} else {
			free(this->object());
			this->init(nullptr);
		}
	}
};

class XdgExported
	: public QObject
	, public QtWayland::zxdg_exported_v2 {
public:
	XdgExported(struct ::zxdg_exported_v2 *object, QObject *parent = nullptr)
	: QObject(parent)
	, zxdg_exported_v2(object) {
		_loop.exec();
	}

	~XdgExported() {
		destroy();
	}

	QString handle() {
		return _handle;
	}

protected:
	void zxdg_exported_v2_handle(const QString &handle) override {
		_handle = handle;
		_loop.quit();
	}

private:
	QEventLoop _loop;
	QString _handle;
};

} // namespace

struct WaylandIntegration::Private {
	std::unique_ptr<wl_registry, WlRegistryDeleter> registry;
	QtWaylandAutoDestroyer<QtWayland::zxdg_exporter_v2> xdgExporter;
	uint32_t xdgExporterName = 0;
	QtWaylandAutoDestroyer<
		QtWayland::zwp_idle_inhibit_manager_v1> idleInhibitManager;
	uint32_t idleInhibitManagerName = 0;
	base::flat_map<QWindow*, std::unique_ptr<
		zwp_idle_inhibitor_v1,
		IdleInhibitorDeleter>> idleInhibitors;
	rpl::lifetime lifetime;

	static const struct wl_registry_listener RegistryListener;
};

const struct wl_registry_listener WaylandIntegration::Private::RegistryListener = {
	decltype(wl_registry_listener::global)(+[](
			Private *data,
			wl_registry *registry,
			uint32_t name,
			const char *interface,
			uint32_t version) {
		if (interface == qstr("zxdg_exporter_v2")) {
			data->xdgExporter.init(registry, name, version);
			data->xdgExporterName = name;
		} else if (interface == qstr("zwp_idle_inhibit_manager_v1")) {
			data->idleInhibitManager.init(registry, name, version);
			data->idleInhibitManagerName = name;
		}
	}),
	decltype(wl_registry_listener::global_remove)(+[](
			Private *data,
			wl_registry *registry,
			uint32_t name) {
		if (name == data->xdgExporterName) {
			free(data->xdgExporter.object());
			data->xdgExporter.init(nullptr);
			data->xdgExporterName = 0;
		} else if (name == data->idleInhibitManagerName) {
			free(data->idleInhibitManager.object());
			data->idleInhibitManager.init(nullptr);
			data->idleInhibitManagerName = 0;
		}
	}),
};

WaylandIntegration::WaylandIntegration()
: _private(std::make_unique<Private>()) {
	const auto native = QGuiApplication::platformNativeInterface();
	if (!native) {
		return;
	}

	const auto display = reinterpret_cast<wl_display*>(
		native->nativeResourceForIntegration(QByteArray("wl_display")));

	if (!display) {
		return;
	}

	_private->registry.reset(wl_display_get_registry(display));
	wl_registry_add_listener(
		_private->registry.get(),
		&Private::RegistryListener,
		_private.get());

	base::qt_signal_producer(
		native,
		&QObject::destroyed
	) | rpl::start_with_next([=] {
		// too late for standard destructors, just free
		for (auto it = _private->idleInhibitors.begin()
			; it != _private->idleInhibitors.cend()
			; ++it) {
			free(it->second.release());
			_private->idleInhibitors.erase(it);
		}
		free(_private->idleInhibitManager.object());
		_private->idleInhibitManager.init(nullptr);
		free(_private->xdgExporter.object());
		_private->xdgExporter.init(nullptr);
		free(_private->registry.release());
	}, _private->lifetime);
}

WaylandIntegration::~WaylandIntegration() = default;

WaylandIntegration *WaylandIntegration::Instance() {
	if (!::Platform::IsWayland()) return nullptr;
	static WaylandIntegration instance;
	return &instance;
}

QString WaylandIntegration::nativeHandle(QWindow *window) {
	if (!_private->xdgExporter.isInitialized()) {
		return {};
	}

	const auto native = QGuiApplication::platformNativeInterface();
	if (!native) {
		return {};
	}

	const auto surface = reinterpret_cast<wl_surface*>(
		native->nativeResourceForWindow(QByteArray("surface"), window));
	
	if (!surface) {
		return {};
	}

	const auto exported = new XdgExported(
		_private->xdgExporter.export_toplevel(surface),
		window);

	if (!exported->isInitialized()) {
		return {};
	}

	return exported->handle();
}

void WaylandIntegration::preventDisplaySleep(bool prevent, QWindow *window) {
	const auto deleter = [=] {
		auto it = _private->idleInhibitors.find(window);
		if (it != _private->idleInhibitors.cend()) {
			_private->idleInhibitors.erase(it);
		}
	};

	if (!prevent) {
		deleter();
		return;
	}

	if (_private->idleInhibitors.contains(window)) {
		return;
	}

	if (!_private->idleInhibitManager.isInitialized()) {
		return;
	}

	const auto native = QGuiApplication::platformNativeInterface();
	if (!native) {
		return;
	}

	const auto surface = reinterpret_cast<wl_surface*>(
		native->nativeResourceForWindow(QByteArray("surface"), window));
	
	if (!surface) {
		return;
	}

	const auto inhibitor = _private->idleInhibitManager.create_inhibitor(
		surface);
	
	if (!inhibitor) {
		return;
	}

	_private->idleInhibitors.emplace(window, inhibitor);

	base::qt_signal_producer(
		window,
		&QObject::destroyed
	) | rpl::start_with_next(deleter, _private->lifetime);
}

} // namespace Platform
} // namespace base
