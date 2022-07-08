// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/lottie_icon.h"

#include "lottie/lottie_common.h"
#include "ui/image/image_prepare.h"
#include "ui/style/style_core.h"

#include <QtGui/QPainter>
#include <crl/crl_async.h>
#include <crl/crl_semaphore.h>
#include <crl/crl_on_main.h>
#include <rlottie.h>

namespace Lottie {
namespace {

[[nodiscard]] std::unique_ptr<rlottie::Animation> CreateFromContent(
		const QByteArray &content,
		QColor replacement) {
	auto string = ReadUtf8(Images::UnpackGzip(content));
#ifndef DESKTOP_APP_USE_PACKAGED_RLOTTIE
	auto list = std::vector<std::pair<std::uint32_t, std::uint32_t>>();
	if (replacement != Qt::white) {
		const auto value = (uint32_t(replacement.red()) << 16)
			| (uint32_t(replacement.green() << 8))
			| (uint32_t(replacement.blue()));
		list.push_back({ 0xFFFFFFU, value });
	}
	auto result = rlottie::Animation::loadFromData(
		std::move(string),
		std::string(),
		std::string(),
		false,
		std::move(list));
#else
	auto result = rlottie::Animation::loadFromData(
		std::move(string),
		std::string(),
		std::string(),
		false);
#endif
	return result;
}

[[nodiscard]] QColor RealRenderedColor(QColor color) {
#ifndef DESKTOP_APP_USE_PACKAGED_RLOTTIE
	return QColor(color.red(), color.green(), color.blue(), 255);
#else
	return Qt::white;
#endif
}

[[nodiscard]] QByteArray ReadIconContent(
		const QString &name,
		const QByteArray &json,
		const QString &path) {
	return !json.isEmpty()
		? json
		: !path.isEmpty()
		? ReadContent(json, path)
		: Images::UnpackGzip(
			ReadContent({}, u":/animations/"_q + name + u".tgs"_q));
}

} // namespace

struct Icon::Frame {
	int index = 0;
	QImage resizedImage;
	QImage renderedImage;
	QImage colorizedImage;
	QColor renderedColor;
	QColor colorizedColor;
};

class Icon::Inner final : public std::enable_shared_from_this<Inner> {
public:
	Inner(int frameIndex, base::weak_ptr<Icon> weak);

	void prepareFromAsync(
		const QString &name,
		const QString &path,
		const QByteArray &json,
		QSize sizeOverride,
		QColor color);
	void waitTillPrepared() const;

	[[nodiscard]] bool valid() const;
	[[nodiscard]] QSize size() const;
	[[nodiscard]] int framesCount() const;
	[[nodiscard]] Frame &frame();
	[[nodiscard]] const Frame &frame() const;

	[[nodiscard]] crl::time animationDuration(
		int frameFrom,
		int frameTo) const;
	void moveToFrame(int frame, QColor color, QSize updatedDesiredSize);

private:
	enum class PreloadState {
		None,
		Preloading,
		Ready,
	};

	// Called from crl::async.
	void renderPreloadFrame(const QColor &color);

	std::unique_ptr<rlottie::Animation> _rlottie;
	Frame _current;
	QSize _desiredSize;
	std::atomic<PreloadState> _preloadState = PreloadState::None;

	Frame _preloaded; // Changed on main or async depending on _preloadState.
	QSize _preloadImageSize;

	base::weak_ptr<Icon> _weak;
	int _framesCount = 0;
	mutable crl::semaphore _semaphore;
	mutable bool _ready = false;

};

Icon::Inner::Inner(int frameIndex, base::weak_ptr<Icon> weak)
: _current{ .index = frameIndex }
, _weak(weak) {
}

void Icon::Inner::prepareFromAsync(
		const QString &name,
		const QString &path,
		const QByteArray &json,
		QSize sizeOverride,
		QColor color) {
	const auto guard = gsl::finally([&] { _semaphore.release(); });
	if (!_weak) {
		return;
	}
	auto rlottie = CreateFromContent(
		ReadIconContent(name, json, path),
		color);
	if (!rlottie || !_weak) {
		return;
	}
	auto width = size_t();
	auto height = size_t();
	rlottie->size(width, height);
	_framesCount = rlottie->totalFrame();
	if (!_framesCount || !width || !height) {
		return;
	}
	_rlottie = std::move(rlottie);
	while (_current.index < 0) {
		_current.index += _framesCount;
	}
	const auto size = sizeOverride.isEmpty()
		? style::ConvertScale(QSize{ int(width), int(height) })
		: sizeOverride;
	auto &image = _current.renderedImage;
	image = CreateFrameStorage(size * style::DevicePixelRatio());
	image.fill(Qt::transparent);
	auto surface = rlottie::Surface(
		reinterpret_cast<uint32_t*>(image.bits()),
		image.width(),
		image.height(),
		image.bytesPerLine());
	_rlottie->renderSync(_current.index, std::move(surface));
	_current.renderedColor = RealRenderedColor(color);
	_current.renderedImage = std::move(image);
	_desiredSize = size;
}

void Icon::Inner::waitTillPrepared() const {
	if (!_ready) {
		_semaphore.acquire();
		_ready = true;
	}
}

bool Icon::Inner::valid() const {
	waitTillPrepared();
	return (_rlottie != nullptr);
}

QSize Icon::Inner::size() const {
	waitTillPrepared();
	return _desiredSize;
}

int Icon::Inner::framesCount() const {
	waitTillPrepared();
	return _framesCount;
}

Icon::Frame &Icon::Inner::frame() {
	waitTillPrepared();
	return _current;
}

const Icon::Frame &Icon::Inner::frame() const {
	waitTillPrepared();
	return _current;
}

crl::time Icon::Inner::animationDuration(int frameFrom, int frameTo) const {
	waitTillPrepared();
	const auto rate = _rlottie ? _rlottie->frameRate() : 0.;
	const auto frames = std::abs(frameTo - frameFrom);
	return (rate >= 1.)
		? crl::time(base::SafeRound(frames / rate * 1000.))
		: 0;
}

void Icon::Inner::moveToFrame(
		int frame,
		QColor color,
		QSize updatedDesiredSize) {
	waitTillPrepared();
	if (frame < 0) {
		frame += _framesCount;
	}
	const auto state = _preloadState.load();
	const auto shown = _current.index;
	if (!updatedDesiredSize.isEmpty()) {
		_desiredSize = updatedDesiredSize;
	}
	const auto desiredImageSize = _desiredSize * style::DevicePixelRatio();
	if (!_rlottie
		|| state == PreloadState::Preloading
		|| (shown == frame
			&& (_current.renderedImage.size() == desiredImageSize))) {
		return;
	} else if (state == PreloadState::Ready) {
		if (_preloaded.index == frame
			&& (shown != frame
				|| _preloaded.renderedImage.size() == desiredImageSize)) {
			std::swap(_current, _preloaded);
			if (_current.renderedImage.size() == desiredImageSize) {
				return;
			}
		} else if ((shown < _preloaded.index && _preloaded.index < frame)
			|| (shown > _preloaded.index && _preloaded.index > frame)) {
			std::swap(_current, _preloaded);
		}
	}
	_preloadImageSize = desiredImageSize;
	_preloaded.index = frame;
	_preloadState = PreloadState::Preloading;
	crl::async([
		guard = shared_from_this(),
		color = RealRenderedColor(color)
	] {
		guard->renderPreloadFrame(color);
	});
}

void Icon::Inner::renderPreloadFrame(const QColor &color) {
	if (!_weak) {
		return;
	}
	auto &image = _preloaded.renderedImage;
	const auto &size = _preloadImageSize;
	if (!GoodStorageForFrame(image, size)) {
		image = GoodStorageForFrame(_preloaded.resizedImage, size)
			? base::take(_preloaded.resizedImage)
			: CreateFrameStorage(size);
	}
	image.fill(Qt::black);
	auto surface = rlottie::Surface(
		reinterpret_cast<uint32_t*>(image.bits()),
		image.width(),
		image.height(),
		image.bytesPerLine());
	_rlottie->renderSync(_preloaded.index, std::move(surface));
	_preloaded.renderedColor = color;
	_preloaded.resizedImage = QImage();
	_preloadState = PreloadState::Ready;
	crl::on_main(_weak, [=] {
		_weak->frameJumpFinished();
	});
}

Icon::Icon(IconDescriptor &&descriptor)
: _inner(std::make_shared<Inner>(descriptor.frame, base::make_weak(this)))
, _color(descriptor.color)
, _animationFrameTo(descriptor.frame) {
	crl::async([
		inner = _inner,
		name = descriptor.name,
		path = descriptor.path,
		bytes = descriptor.json,
		sizeOverride = descriptor.sizeOverride,
		color = (_color ? (*_color)->c : Qt::white)
	] {
		inner->prepareFromAsync(name, path, bytes, sizeOverride, color);
	});
}

void Icon::wait() const {
	_inner->waitTillPrepared();
}

bool Icon::valid() const {
	return _inner->valid();
}

int Icon::frameIndex() const {
	preloadNextFrame();
	return _inner->frame().index;
}

int Icon::framesCount() const {
	return _inner->framesCount();
}

QImage Icon::frame() const {
	return frame(QSize(), nullptr).image;
}

Icon::ResizedFrame Icon::frame(
		QSize desiredSize,
		Fn<void()> updateWithPerfect) const {
	preloadNextFrame(desiredSize);

	const auto desired = size() * style::DevicePixelRatio();
	auto &frame = _inner->frame();
	if (frame.renderedImage.isNull()) {
		return { frame.renderedImage };
	} else if (!_color) {
		if (frame.renderedImage.size() == desired) {
			return { frame.renderedImage };
		} else if (frame.resizedImage.size() != desired) {
			frame.resizedImage = frame.renderedImage.scaled(
				desired,
				Qt::IgnoreAspectRatio,
				Qt::SmoothTransformation);
		}
		if (updateWithPerfect) {
			_repaint = std::move(updateWithPerfect);
		}
		return { frame.resizedImage, true };
	}
	Assert(frame.renderedImage.size() == desired);
	const auto color = (*_color)->c;
	if (color == frame.renderedColor) {
		return { frame.renderedImage };
	} else if (!frame.colorizedImage.isNull()
		&& color == frame.colorizedColor) {
		return { frame.colorizedImage };
	}
	if (frame.colorizedImage.isNull()) {
		frame.colorizedImage = CreateFrameStorage(desired);
	}
	frame.colorizedColor = color;
	style::colorizeImage(frame.renderedImage, color, &frame.colorizedImage);
	return { frame.colorizedImage };
}

int Icon::width() const {
	return size().width();
}

int Icon::height() const {
	return size().height();
}

QSize Icon::size() const {
	return _inner->size();
}

void Icon::paint(
		QPainter &p,
		int x,
		int y,
		std::optional<QColor> colorOverride) {
	preloadNextFrame();
	auto &frame = _inner->frame();
	const auto color = colorOverride.value_or(
		_color ? (*_color)->c : Qt::white);
	if (frame.renderedImage.isNull() || color.alpha() == 0) {
		return;
	}
	const auto rect = QRect{ QPoint(x, y), size() };
	if (color == frame.renderedColor || !_color) {
		p.drawImage(rect, frame.renderedImage);
	} else if (color.alphaF() < 1.
		&& (QColor(color.red(), color.green(), color.blue())
			== frame.renderedColor)) {
		const auto o = p.opacity();
		p.setOpacity(o * color.alphaF());
		p.drawImage(rect, frame.renderedImage);
		p.setOpacity(o);
	} else if (!frame.colorizedImage.isNull()
		&& color == frame.colorizedColor) {
		p.drawImage(rect, frame.colorizedImage);
	} else if (!frame.colorizedImage.isNull()
		&& color.alphaF() < 1.
		&& (QColor(color.red(), color.green(), color.blue())
			== frame.colorizedColor)) {
		const auto o = p.opacity();
		p.setOpacity(o * color.alphaF());
		p.drawImage(rect, frame.colorizedImage);
		p.setOpacity(o);
	} else {
		if (frame.colorizedImage.isNull()) {
			frame.colorizedImage = CreateFrameStorage(
				frame.renderedImage.size());
		}
		frame.colorizedColor = color;
		style::colorizeImage(
			frame.renderedImage,
			color,
			&frame.colorizedImage);
		p.drawImage(rect, frame.colorizedImage);
	}
}

void Icon::paintInCenter(
		QPainter &p,
		QRect rect,
		std::optional<QColor> colorOverride) {
	const auto my = size();
	paint(
		p,
		rect.x() + (rect.width() - my.width()) / 2,
		rect.y() + (rect.height() - my.height()) / 2,
		colorOverride);
}

void Icon::animate(
		Fn<void()> update,
		int frameFrom,
		int frameTo,
		std::optional<crl::time> duration) {
	jumpTo(frameFrom, std::move(update));
	if (frameFrom != frameTo) {
		_animationFrameTo = frameTo;
		_animation.start(
			[=] { preloadNextFrame(); if (_repaint) _repaint(); },
			frameFrom,
			frameTo,
			(duration
				? *duration
				: _inner->animationDuration(frameFrom, frameTo)));
	}
}

void Icon::jumpTo(int frame, Fn<void()> update) {
	_animation.stop();
	_repaint = std::move(update);
	_animationFrameTo = frame;
	preloadNextFrame();
}

void Icon::frameJumpFinished() {
	if (_repaint && !animating()) {
		_repaint();
		_repaint = nullptr;
	}
}

int Icon::wantedFrameIndex() const {
	return int(base::SafeRound(_animation.value(_animationFrameTo)));
}

void Icon::preloadNextFrame(QSize updatedDesiredSize) const {
	_inner->moveToFrame(
		wantedFrameIndex(),
		_color ? (*_color)->c : Qt::white,
		updatedDesiredSize);
	if (_animationFrameTo < 0) {
		_animationFrameTo += framesCount();
	}
}

bool Icon::animating() const {
	return _animation.animating();
}

std::unique_ptr<Icon> MakeIcon(IconDescriptor &&descriptor) {
	return std::make_unique<Icon>(std::move(descriptor));
}

} // namespace Lottie
