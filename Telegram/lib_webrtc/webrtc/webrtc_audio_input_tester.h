// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <crl/crl_object_on_thread.h>

namespace Webrtc {

enum class Backend;

class AudioInputTester {
public:
	AudioInputTester(
		const Backend &backend,
		const QString &deviceId);
	~AudioInputTester();

	void setDeviceId(const QString &deviceId);

	[[nodiscard]] float getAndResetLevel();

private:
	class Impl;

	std::shared_ptr<std::atomic<int>> _maxSample;
	crl::object_on_thread<Impl> _impl;

};

} // namespace Webrtc
