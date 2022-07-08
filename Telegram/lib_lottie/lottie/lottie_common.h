// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/basic_types.h"

#include <QSize>
#include <QColor>
#include <QImage>
#include <crl/crl_time.h>
#include <vector>
#include <optional>

namespace Lottie {

inline constexpr auto kTimeUnknown = std::numeric_limits<crl::time>::min();
inline constexpr auto kMaxFileSize = 2 * 1024 * 1024;

constexpr auto kImageFormat = QImage::Format_ARGB32_Premultiplied;

class Animation;

struct Information {
	QSize size;
	int frameRate = 0;
	int framesCount = 0;
};

enum class Error {
	ParseFailed,
	NotSupported,
};

struct FrameRequest {
	QSize box;
	QColor colored = QColor(0, 0, 0, 0);
	bool mirrorHorizontal = false;

	[[nodiscard]] bool empty() const {
		return box.isEmpty();
	}
	[[nodiscard]] QSize size(
		const QSize &original,
		int sizeRounding) const;

	[[nodiscard]] bool operator==(const FrameRequest &other) const {
		return (box == other.box)
			&& (colored == other.colored)
			&& (mirrorHorizontal == other.mirrorHorizontal);
	}
	[[nodiscard]] bool operator!=(const FrameRequest &other) const {
		return !(*this == other);
	}
};

enum class Quality : char {
	Default,
	High,
	Synchronous
};

enum class SkinModifier {
	None,
	Color1,
	Color2,
	Color3,
	Color4,
	Color5,
};

struct ColorReplacements {
	std::vector<std::pair<std::uint32_t, std::uint32_t>> replacements;
	SkinModifier modifier = SkinModifier::None;
	uint8 tag = 0;
};

[[nodiscard]] QByteArray ReadContent(
	const QByteArray &data,
	const QString &filepath);
[[nodiscard]] std::string ReadUtf8(const QByteArray &data);
[[nodiscard]] bool GoodStorageForFrame(const QImage &storage, QSize size);
[[nodiscard]] QImage CreateFrameStorage(QSize size);

enum class FrameRenderResult {
	Ok,
	NotReady,
	BadCacheSize,
	Failed,
};

} // namespace Lottie
