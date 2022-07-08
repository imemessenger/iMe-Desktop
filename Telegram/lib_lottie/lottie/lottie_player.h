// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "lottie/lottie_common.h"
#include "base/weak_ptr.h"

#include <rpl/producer.h>

namespace Lottie {

class SharedState;

class Player : public base::has_weak_ptr {
public:
	virtual void start(
		not_null<Animation*> animation,
		std::unique_ptr<SharedState> state) = 0;
	virtual void failed(not_null<Animation*> animation, Error error) = 0;
	virtual void updateFrameRequest(
		not_null<const Animation*> animation,
		const FrameRequest &request) = 0;
	virtual bool markFrameShown() = 0;
	virtual void checkStep() = 0;

	virtual ~Player() = default;

};

} // namespace Lottie
