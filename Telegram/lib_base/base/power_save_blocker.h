// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <QtCore/QPointer>

class QWindow;

namespace base {

enum class PowerSaveBlockType {
	PreventAppSuspension,
	PreventDisplaySleep,

	kCount,
};

class PowerSaveBlocker final {
public:
	PowerSaveBlocker(
		PowerSaveBlockType type,
		const QString &description,
		QWindow *window);
	~PowerSaveBlocker();

	[[nodiscard]] PowerSaveBlockType type() const {
		return _type;
	}
	[[nodiscard]] const QString &description() const {
		return _description;
	}
	[[nodiscard]] QPointer<QWindow> window() const;

private:
	const PowerSaveBlockType _type = {};
	const QString _description;
	const QPointer<QWindow> _window;

};

// DescriptionResolver -> QString, WindowResolver -> QPointer<QWindow>.
template <typename DescriptionResolver, typename WindowResolver>
void UpdatePowerSaveBlocker(
		std::unique_ptr<PowerSaveBlocker> &blocker,
		bool block,
		PowerSaveBlockType type,
		DescriptionResolver &&description,
		WindowResolver &&window) {
	if (block && !blocker) {
		blocker = std::make_unique<PowerSaveBlocker>(
			type,
			description(),
			window());
	} else if (!block && blocker) {
		blocker = nullptr;
	}
}

} // namespace base
