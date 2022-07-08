// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/lottie_common.h"

#include "base/algorithm.h"

#include <QFile>

namespace Lottie {
namespace {

QByteArray ReadFile(const QString &filepath) {
	auto f = QFile(filepath);
	return (f.size() <= kMaxFileSize && f.open(QIODevice::ReadOnly))
		? f.readAll()
		: QByteArray();
}

} // namespace

QSize FrameRequest::size(
		const QSize &original,
		int sizeRounding) const {
	Expects(!empty());
	Expects(sizeRounding != 0);

	const auto result = original.scaled(box, Qt::KeepAspectRatio);
	const auto skipw = result.width() % sizeRounding;
	const auto skiph = result.height() % sizeRounding;
	return QSize(
		std::max(result.width() - skipw, sizeRounding),
		std::max(result.height() - skiph, sizeRounding));
}

QByteArray ReadContent(const QByteArray &data, const QString &filepath) {
	return data.isEmpty() ? ReadFile(filepath) : base::duplicate(data);
}

std::string ReadUtf8(const QByteArray &data) {
	//00 00 FE FF  UTF-32BE
	//FF FE 00 00  UTF-32LE
	//FE FF        UTF-16BE
	//FF FE        UTF-16LE
	//EF BB BF     UTF-8
	if (data.size() < 4) {
		return data.toStdString();
	}
	const auto bom = uint32(uint8(data[0]))
		| (uint32(uint8(data[1])) << 8)
		| (uint32(uint8(data[2])) << 16)
		| (uint32(uint8(data[3])) << 24);
	const auto skip = ((bom == 0xFFFE0000U) || (bom == 0x0000FEFFU))
		? 4
		: (((bom & 0xFFFFU) == 0xFFFEU) || ((bom & 0xFFFFU) == 0xFEFFU))
		? 2
		: ((bom & 0xFFFFFFU) == 0xBFBBEFU)
		? 3
		: 0;
	const auto bytes = data.data() + skip;
	const auto length = data.size() - skip;
	// Old RapidJSON didn't convert encoding, just skipped BOM.
	// We emulate old behavior here, so don't convert as well.
	return std::string(bytes, length);
}

bool GoodStorageForFrame(const QImage &storage, QSize size) {
	return !storage.isNull()
		&& (storage.format() == kImageFormat)
		&& (storage.size() == size)
		&& storage.isDetached();
}

QImage CreateFrameStorage(QSize size) {
	return QImage(size, kImageFormat);
}

} // namespace Lottie
