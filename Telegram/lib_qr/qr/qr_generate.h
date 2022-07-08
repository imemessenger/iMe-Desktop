// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <QtGui/QImage>
#include <QtGui/QColor>
#include <QtCore/QString>

#include <vector>

namespace Qr {

enum class Redundancy {
	Low,
	Medium,
	Quartile,
	High,

	Default = Medium
};

struct Data {
	int size = 0;
	Redundancy redundancy = Redundancy::Default;
	std::vector<bool> values; // size x size
};

[[nodiscard]] Data Encode(
	const QString &text,
	Redundancy redundancy = Redundancy::Default);
[[nodiscard]] QImage Generate(
	const Data &data,
	int pixel,
	QColor fg = Qt::black);
[[nodiscard]] int ReplaceSize(const Data &data, int pixel);
[[nodiscard]] QImage ReplaceCenter(QImage qr, const QImage &center);

} // namespace Qr
