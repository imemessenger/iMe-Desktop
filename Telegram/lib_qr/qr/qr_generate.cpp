// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "qr/qr_generate.h"

#include "base/assertion.h"

#if __has_include(<qrcodegen.hpp>)
#include <qrcodegen.hpp>
#else
#include <QrCode.hpp>
#endif

#include <QtGui/QPainter>
#include <string>

namespace Qr {
namespace {

using namespace qrcodegen;

[[nodiscard]] int ReplaceElements(const Data &data) {
	const auto elements = [&] {
		switch (data.redundancy) {
		case Redundancy::Low: return data.size / 5;
		case Redundancy::Medium: return data.size / 4;
		case Redundancy::Quartile: return data.size / 3;
		case Redundancy::High: return data.size / 2;
		}
		Unexpected("Redundancy value in Qr::ReplaceElements.");
	}();
	const auto close = (data.redundancy != Redundancy::Quartile);
	const auto shift = (data.size - elements) % 2;
	return elements + (close ? -1 : 1) * shift;
}

[[nodiscard]] QrCode::Ecc RedundancyToEcc(Redundancy redundancy) {
	switch (redundancy) {
	case Redundancy::Low: return QrCode::Ecc::LOW;
	case Redundancy::Medium: return QrCode::Ecc::MEDIUM;
	case Redundancy::Quartile: return QrCode::Ecc::QUARTILE;
	case Redundancy::High: return QrCode::Ecc::HIGH;
	}
	Unexpected("Redundancy value in Qr::RedundancyToEcc.");
}

} // namespace

Data Encode(const QString &text, Redundancy redundancy) {
	Expects(!text.isEmpty());

	auto result = Data();
	result.redundancy = redundancy;
	const auto utf8 = text.toStdString();
	const auto qr = QrCode::encodeText(
		utf8.c_str(),
		RedundancyToEcc(redundancy));
	result.size = qr.getSize();
	Assert(result.size > 0);

	result.values.reserve(result.size * result.size);
	for (auto row = 0; row != result.size; ++row) {
		for (auto column = 0; column != result.size; ++column) {
			result.values.push_back(qr.getModule(row, column));
		}
	}
	return result;
}

void PrepareForRound(QPainter &p) {
	p.setRenderHints(QPainter::Antialiasing
		| QPainter::SmoothPixmapTransform
		| QPainter::TextAntialiasing);
	p.setPen(Qt::NoPen);
}

QImage GenerateSingle(int size, QColor bg, QColor color) {
	auto result = QImage(size, size, QImage::Format_ARGB32_Premultiplied);
	result.fill(bg);
	{
		auto p = QPainter(&result);
		p.setCompositionMode(QPainter::CompositionMode_Source);
		PrepareForRound(p);
		p.setBrush(color);
		p.drawRoundedRect(
			QRect{ 0, 0, size, size },
			size / 2.,
			size / 2.);
	}
	return result;
}

int ReplaceSize(const Data &data, int pixel) {
	return ReplaceElements(data) * pixel;
}

QImage Generate(const Data &data, int pixel, QColor fg) {
	Expects(data.size > 0);
	Expects(data.values.size() == data.size * data.size);

	const auto bg = Qt::transparent;
	const auto replaceElements = ReplaceElements(data);
	const auto replaceFrom = (data.size - replaceElements) / 2;
	const auto replaceTill = (data.size - replaceFrom);
	const auto black = GenerateSingle(pixel, bg, fg);
	const auto white = GenerateSingle(pixel, fg, bg);
	const auto value = [&](int row, int column) {
		return (row >= 0)
			&& (row < data.size)
			&& (column >= 0)
			&& (column < data.size)
			&& (row < replaceFrom
				|| row >= replaceTill
				|| column < replaceFrom
				|| column >= replaceTill)
			&& data.values[row * data.size + column];
	};
	const auto blackFull = [&](int row, int column) {
		return (value(row - 1, column) && value(row + 1, column))
			|| (value(row, column - 1) && value(row, column + 1));
	};
	const auto whiteCorner = [&](int row, int column, int dx, int dy) {
		return !value(row + dy, column)
			|| !value(row, column + dx)
			|| !value(row + dy, column + dx);
	};
	const auto whiteFull = [&](int row, int column) {
		return whiteCorner(row, column, -1, -1)
			&& whiteCorner(row, column, 1, -1)
			&& whiteCorner(row, column, 1, 1)
			&& whiteCorner(row, column, -1, 1);
	};
	auto result = QImage(
		data.size * pixel,
		data.size * pixel,
		QImage::Format_ARGB32_Premultiplied);
	result.fill(bg);
	{
		auto p = QPainter(&result);
		p.setCompositionMode(QPainter::CompositionMode_Source);
		const auto skip = pixel - pixel / 2;
		const auto brect = [&](int x, int y, int width, int height) {
			p.fillRect(x, y, width, height, fg);
		};
		const auto wrect = [&](int x, int y, int width, int height) {
			p.fillRect(x, y, width, height, bg);
		};
		const auto large = [&](int x, int y) {
			p.setBrush(fg);
			p.drawRoundedRect(
				QRect{ x, y, pixel * 7, pixel * 7 },
				pixel * 2.,
				pixel * 2.);
			p.setBrush(bg);
			p.drawRoundedRect(
				QRect{ x + pixel, y + pixel, pixel * 5, pixel * 5 },
				pixel * 1.5,
				pixel * 1.5);
			p.setBrush(fg);
			p.drawRoundedRect(
				QRect{ x + pixel * 2, y + pixel * 2, pixel * 3, pixel * 3 },
				pixel,
				pixel);

		};
		for (auto row = 0; row != data.size; ++row) {
			for (auto column = 0; column != data.size; ++column) {
				if ((row < 7 && (column < 7 || column >= data.size - 7))
					|| (column < 7 && (row < 7 || row >= data.size - 7))) {
					continue;
				}
				const auto x = column * pixel;
				const auto y = row * pixel;
				if (value(row, column)) {
					if (blackFull(row, column)) {
						brect(x, y, pixel, pixel);
					} else {
						p.drawImage(x, y, black);
						if (value(row - 1, column)) {
							brect(x, y, pixel, pixel / 2);
						} else if (value(row + 1, column)) {
							brect(x, y + skip, pixel, pixel / 2);
						}
						if (value(row, column - 1)) {
							brect(x, y, pixel / 2, pixel);
						} else if (value(row, column + 1)) {
							brect(x + skip, y, pixel / 2, pixel);
						}
					}
				} else if (whiteFull(row, column)) {
					wrect(x, y, pixel, pixel);
				} else {
					p.drawImage(x, y, white);
					if (whiteCorner(row, column, -1, -1)
						&& whiteCorner(row, column, 1, -1)) {
						wrect(x, y, pixel, pixel / 2);
					} else if (whiteCorner(row, column, -1, 1)
						&& whiteCorner(row, column, 1, 1)) {
						wrect(x, y + skip, pixel, pixel / 2);
					}
					if (whiteCorner(row, column, -1, -1)
						&& whiteCorner(row, column, -1, 1)) {
						wrect(x, y, pixel / 2, pixel);
					} else if (whiteCorner(row, column, 1, -1)
						&& whiteCorner(row, column, 1, 1)) {
						wrect(x + skip, y, pixel / 2, pixel);
					}
					if (whiteCorner(row, column, -1, -1)) {
						wrect(x, y, pixel / 2, pixel / 2);
					}
					if (whiteCorner(row, column, 1, -1)) {
						wrect(x + skip, y, pixel / 2, pixel / 2);
					}
					if (whiteCorner(row, column, 1, 1)) {
						wrect(x + skip, y + skip, pixel / 2, pixel / 2);
					}
					if (whiteCorner(row, column, -1, 1)) {
						wrect(x, y + skip, pixel / 2, pixel / 2);
					}
				}
			}
		}

		PrepareForRound(p);
		large(0, 0);
		large((data.size - 7) * pixel, 0);
		large(0, (data.size - 7) * pixel);
	}
	return result;
}

QImage ReplaceCenter(QImage qr, const QImage &center) {
	{
		auto p = QPainter(&qr);
		const auto x = (qr.width() - center.width()) / 2;
		const auto y = (qr.height() - center.height()) / 2;
		p.drawImage(x, y, center);
	}
	return qr;
}

} // namespace Qr
