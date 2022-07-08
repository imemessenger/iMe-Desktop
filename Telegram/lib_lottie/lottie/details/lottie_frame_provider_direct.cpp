// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/details/lottie_frame_provider_direct.h"

#include "lottie/details/lottie_frame_renderer.h"
#include "ui/image/image_prepare.h"

#include <rlottie.h>

namespace Lottie {
namespace {

int GetLottieFrameRate(not_null<rlottie::Animation*> animation, Quality quality) {
	const auto rate = int(qRound(animation->frameRate()));
	return (quality == Quality::Default && rate == 60) ? (rate / 2) : rate;
}

int GetLottieFramesCount(not_null<rlottie::Animation*> animation, Quality quality) {
	const auto rate = int(qRound(animation->frameRate()));
	const auto count = int(animation->totalFrame());
	return (quality == Quality::Default && rate == 60) ? (count / 2) : count;
}

int GetLottieFrameIndex(not_null<rlottie::Animation*> animation, Quality quality, int index) {
	const auto rate = int(qRound(animation->frameRate()));
	return (quality == Quality::Default && rate == 60) ? (index * 2) : index;
}

#ifndef DESKTOP_APP_USE_PACKAGED_RLOTTIE

[[nodiscard]] rlottie::FitzModifier MapModifier(SkinModifier modifier) {
	using Result = rlottie::FitzModifier;
	switch (modifier) {
	case SkinModifier::None: return Result::None;
	case SkinModifier::Color1: return Result::Type12;
	case SkinModifier::Color2: return Result::Type3;
	case SkinModifier::Color3: return Result::Type4;
	case SkinModifier::Color4: return Result::Type5;
	case SkinModifier::Color5: return Result::Type6;
	}
	Unexpected("Unexpected modifier in MapModifier.");
}

#endif // DESKTOP_APP_USE_PACKAGED_RLOTTIE

} // namespace

FrameProviderDirect::FrameProviderDirect(Quality quality)
: _quality(quality) {
}

FrameProviderDirect::~FrameProviderDirect() = default;

bool FrameProviderDirect::load(
		const QByteArray &content,
		const ColorReplacements *replacements) {
	_information = Information();

	const auto string = ReadUtf8(Images::UnpackGzip(content));
	if (string.size() > kMaxFileSize) {
		return false;
	}

#ifndef DESKTOP_APP_USE_PACKAGED_RLOTTIE
	_animation = rlottie::Animation::loadFromData(
		string,
		std::string(),
		std::string(),
		false,
		(replacements
			? replacements->replacements
			: std::vector<std::pair<std::uint32_t, std::uint32_t>>()),
		(replacements
			? MapModifier(replacements->modifier)
			: rlottie::FitzModifier::None));
#else
	_animation = rlottie::Animation::loadFromData(
		string,
		std::string(),
		std::string(),
		false);
#endif
	if (!_animation) {
		return false;
	}
	auto width = size_t(0);
	auto height = size_t(0);
	_animation->size(width, height);
	const auto rate = GetLottieFrameRate(_animation.get(), _quality);
	const auto count = GetLottieFramesCount(_animation.get(), _quality);
	return setInformation({
		.size = QSize(int(width), int(height)),
		.frameRate = int(rate),
		.framesCount = int(count),
	});
}

bool FrameProviderDirect::loaded() const {
	return (_animation != nullptr);
}

void FrameProviderDirect::unload() {
	_animation = nullptr;
}

bool FrameProviderDirect::setInformation(Information information) {
	if (information.size.isEmpty()
		|| information.size.width() > kMaxSize
		|| information.size.height() > kMaxSize
		|| !information.frameRate
		|| information.frameRate > kMaxFrameRate
		|| !information.framesCount
		|| information.framesCount > kMaxFramesCount) {
		return false;
	}
	_information = information;
	return true;
}

const Information &FrameProviderDirect::information() {
	return _information;
}

bool FrameProviderDirect::valid() {
	return _information.framesCount > 0;
}

QImage FrameProviderDirect::construct(
		std::unique_ptr<FrameProviderToken> &token,
		const FrameRequest &request) {
	auto cover = QImage();
	render(token, cover, request, 0);
	return cover;
}

int FrameProviderDirect::sizeRounding() {
	return 2;
}

bool FrameProviderDirect::render(
		const std::unique_ptr<FrameProviderToken> &token,
		QImage &to,
		const FrameRequest &request,
		int index) {
	if (token && !token->exclusive) {
		token->result = FrameRenderResult::NotReady;
		return false;
	} else if (!valid()) {
		return false;
	}
	const auto original = information().size;
	const auto size = request.box.isEmpty()
		? original
		: request.size(original, sizeRounding());
	if (!GoodStorageForFrame(to, size)) {
		to = CreateFrameStorage(size);
	}
	renderToPrepared(to, index);
	return true;
}

void FrameProviderDirect::renderToPrepared(
		QImage &to,
		int index) const {
	to.fill(Qt::transparent);
	auto surface = rlottie::Surface(
		reinterpret_cast<uint32_t*>(to.bits()),
		to.width(),
		to.height(),
		to.bytesPerLine());
	_animation->renderSync(
		GetLottieFrameIndex(_animation.get(), _quality, index),
		surface);
}

} // namespace Lottie
