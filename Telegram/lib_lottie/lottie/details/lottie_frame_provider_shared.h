// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "lottie/details/lottie_frame_provider.h"
#include "base/weak_ptr.h"

#include <QtCore/QReadWriteLock>

namespace Lottie {

class FrameProviderShared final
	: public FrameProvider
	, public base::has_weak_ptr {
public:
	explicit FrameProviderShared(
		FnMut<void(FnMut<void(std::unique_ptr<FrameProvider>)>)> factory);

	QImage construct(
		std::unique_ptr<FrameProviderToken> &token,
		const FrameRequest &request) override;
	const Information &information() override;
	bool valid() override;

	int sizeRounding() override;

	std::unique_ptr<FrameProviderToken> createToken() override;

	bool render(
		const std::unique_ptr<FrameProviderToken> &token,
		QImage &to,
		const FrameRequest &request,
		int index) override;

private:
	std::unique_ptr<FrameProvider> _shared;
	QReadWriteLock _mutex;
	bool _constructed = false;

};

} // namespace Lottie
