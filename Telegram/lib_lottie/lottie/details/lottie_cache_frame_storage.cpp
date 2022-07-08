// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/details/lottie_cache_frame_storage.h"

#include "base/assertion.h"

#include <QtGui/QImage>
#include <lz4.h>
#include <lz4hc.h>

namespace Lottie {
namespace {

constexpr auto kAlignStorage = 16;

void DecodeYUV2RGB(
		QImage &to,
		const EncodedStorage &from,
		FFmpeg::SwscalePointer &context) {
	context = FFmpeg::MakeSwscalePointer(
		to.size(),
		AV_PIX_FMT_YUV420P,
		to.size(),
		AV_PIX_FMT_BGRA,
		&context);
	Assert(context != nullptr);

	// AV_NUM_DATA_POINTERS defined in AVFrame struct
	const uint8_t *src[AV_NUM_DATA_POINTERS] = {
		from.yData(),
		from.uData(),
		from.vData(),
		nullptr
	};
	int srcLineSize[AV_NUM_DATA_POINTERS] = {
		from.yBytesPerLine(),
		from.uBytesPerLine(),
		from.vBytesPerLine(),
		0
	};
	uint8_t *dst[AV_NUM_DATA_POINTERS] = { to.bits(), nullptr };
	int dstLineSize[AV_NUM_DATA_POINTERS] = { int(to.bytesPerLine()), 0 };

	sws_scale(
		context.get(),
		src,
		srcLineSize,
		0,
		to.height(),
		dst,
		dstLineSize);
}

void DecodeAlpha(QImage &to, const EncodedStorage &from) {
	auto bytes = to.bits();
	auto alpha = from.aData();
	const auto perLine = to.bytesPerLine();
	const auto width = to.width();
	const auto height = to.height();
	for (auto i = 0; i != height; ++i) {
		auto ints = reinterpret_cast<uint32*>(bytes);
		const auto till = ints + width;
		while (ints != till) {
			const auto value = uint32(*alpha++);
			*ints = (*ints & 0x00FFFFFFU)
				| ((value & 0xF0U) << 24)
				| ((value & 0xF0U) << 20);
			++ints;
			*ints = (*ints & 0x00FFFFFFU)
				| (value << 28)
				| ((value & 0x0FU) << 24);
			++ints;
		}
		bytes += perLine;
	}
}

void EncodeRGB2YUV(
		EncodedStorage &to,
		const QImage &from,
		FFmpeg::SwscalePointer &context) {
	context = FFmpeg::MakeSwscalePointer(
		from.size(),
		AV_PIX_FMT_BGRA,
		from.size(),
		AV_PIX_FMT_YUV420P,
		&context);
	Assert(context != nullptr);

	// AV_NUM_DATA_POINTERS defined in AVFrame struct
	const uint8_t *src[AV_NUM_DATA_POINTERS] = { from.bits(), nullptr };
	int srcLineSize[AV_NUM_DATA_POINTERS] = { int(from.bytesPerLine()), 0 };
	uint8_t *dst[AV_NUM_DATA_POINTERS] = {
		to.yData(),
		to.uData(),
		to.vData(),
		nullptr
	};
	int dstLineSize[AV_NUM_DATA_POINTERS] = {
		to.yBytesPerLine(),
		to.uBytesPerLine(),
		to.vBytesPerLine(),
		0
	};

	sws_scale(
		context.get(),
		src,
		srcLineSize,
		0,
		from.height(),
		dst,
		dstLineSize);
}

void EncodeAlpha(EncodedStorage &to, const QImage &from) {
	auto bytes = from.bits();
	auto alpha = to.aData();
	const auto perLine = from.bytesPerLine();
	const auto width = from.width();
	const auto height = from.height();
	for (auto i = 0; i != height; ++i) {
		auto ints = reinterpret_cast<const uint32*>(bytes);
		const auto till = ints + width;
		for (; ints != till; ints += 2) {
			*alpha++ = (((*ints) >> 24) & 0xF0U) | ((*(ints + 1)) >> 28);
		}
		bytes += perLine;
	}
}

int YLineSize(int width) {
	return ((width + kAlignStorage - 1) / kAlignStorage) * kAlignStorage;
}

int UVLineSize(int width) {
	return (((width / 2) + kAlignStorage - 1) / kAlignStorage) * kAlignStorage;
}

int YSize(int width, int height) {
	return YLineSize(width) * height;
}

int UVSize(int width, int height) {
	return UVLineSize(width) * (height / 2);
}

int ASize(int width, int height) {
	return (width * height) / 2;
}

} // namespace

void EncodedStorage::allocate(int width, int height) {
	Expects((width % 2) == 0 && (height % 2) == 0);

	if (YSize(width, height) != YSize(_width, _height)
		|| UVSize(width, height) != UVSize(_width, _height)
		|| ASize(width, height) != ASize(_width, _height)) {
		_width = width;
		_height = height;
		reallocate();
	}
}

void EncodedStorage::reallocate() {
	const auto total = YSize(_width, _height)
		+ 2 * UVSize(_width, _height)
		+ ASize(_width, _height);
	_data = QByteArray(total + kAlignStorage - 1, 0);
}

int EncodedStorage::width() const {
	return _width;
}

int EncodedStorage::height() const {
	return _height;
}

int EncodedStorage::size() const {
	return YSize(_width, _height)
		+ 2 * UVSize(_width, _height)
		+ ASize(_width, _height);
}

char *EncodedStorage::data() {
	const auto result = reinterpret_cast<quintptr>(_data.data());
	return reinterpret_cast<char*>(kAlignStorage
		* ((result + kAlignStorage - 1) / kAlignStorage));
}

const char *EncodedStorage::data() const {
	const auto result = reinterpret_cast<quintptr>(_data.data());
	return reinterpret_cast<const char*>(kAlignStorage
		* ((result + kAlignStorage - 1) / kAlignStorage));
}

uint8_t *EncodedStorage::yData() {
	return reinterpret_cast<uint8_t*>(data());
}

const uint8_t *EncodedStorage::yData() const {
	return reinterpret_cast<const uint8_t*>(data());
}

int EncodedStorage::yBytesPerLine() const {
	return YLineSize(_width);
}

uint8_t *EncodedStorage::uData() {
	return yData() + YSize(_width, _height);
}

const uint8_t *EncodedStorage::uData() const {
	return yData() + YSize(_width, _height);
}

int EncodedStorage::uBytesPerLine() const {
	return UVLineSize(_width);
}

uint8_t *EncodedStorage::vData() {
	return uData() + UVSize(_width, _height);
}

const uint8_t *EncodedStorage::vData() const {
	return uData() + UVSize(_width, _height);
}

int EncodedStorage::vBytesPerLine() const {
	return UVLineSize(_width);
}

uint8_t *EncodedStorage::aData() {
	return uData() + 2 * UVSize(_width, _height);
}

const uint8_t *EncodedStorage::aData() const {
	return uData() + 2 * UVSize(_width, _height);
}

int EncodedStorage::aBytesPerLine() const {
	return _width / 2;
}

void Xor(EncodedStorage &to, const EncodedStorage &from) {
	Expects(to.size() == from.size());

	using Block = std::conditional_t<
		sizeof(void*) == sizeof(uint64),
		uint64,
		uint32>;
	constexpr auto kBlockSize = sizeof(Block);
	const auto amount = from.size();
	const auto fromBytes = reinterpret_cast<const uchar*>(from.data());
	const auto toBytes = reinterpret_cast<uchar*>(to.data());
	const auto blocks = amount / kBlockSize;
	const auto fromBlocks = reinterpret_cast<const Block*>(fromBytes);
	const auto toBlocks = reinterpret_cast<Block*>(toBytes);
	for (auto i = 0; i != blocks; ++i) {
		toBlocks[i] ^= fromBlocks[i];
	}
	const auto left = amount - (blocks * kBlockSize);
	for (auto i = amount - left; i != amount; ++i) {
		toBytes[i] ^= fromBytes[i];
	}
}

void Encode(
		EncodedStorage &to,
		const QImage &from,
		QImage &cache,
		FFmpeg::SwscalePointer &context) {
	FFmpeg::UnPremultiply(cache, from);
	EncodeRGB2YUV(to, cache, context);
	EncodeAlpha(to, cache);
}

void Decode(
		QImage &to,
		const EncodedStorage &from,
		const QSize &fromSize,
		FFmpeg::SwscalePointer &context) {
	if (!FFmpeg::GoodStorageForFrame(to, fromSize)) {
		to = FFmpeg::CreateFrameStorage(fromSize);
	}
	DecodeYUV2RGB(to, from, context);
	DecodeAlpha(to, from);
	FFmpeg::PremultiplyInplace(to);
}

void CompressFromRaw(QByteArray &to, const EncodedStorage &from) {
	const auto size = from.size();
	const auto max = sizeof(qint32) + LZ4_compressBound(size);
	to.reserve(max);
	to.resize(max);
	const auto compressed = LZ4_compress_default(
		from.data(),
		to.data() + sizeof(qint32),
		size,
		to.size() - sizeof(qint32));
	Assert(compressed > 0);
	if (compressed >= size + sizeof(qint32)) {
		to.resize(size + sizeof(qint32));
		memcpy(to.data() + sizeof(qint32), from.data(), size);
	} else {
		to.resize(compressed + sizeof(qint32));
	}
	const auto length = qint32(to.size() - sizeof(qint32));
	bytes::copy(
		bytes::make_detached_span(to),
		bytes::object_as_span(&length));
}

void CompressAndSwapFrame(
		QByteArray &to,
		QByteArray *additional,
		EncodedStorage &frame,
		EncodedStorage &previous) {
	CompressFromRaw(to, frame);
	std::swap(frame, previous);
	if (!additional) {
		return;
	}

	// Check if XOR-d delta compresses better.
	Xor(frame, previous);
	CompressFromRaw(*additional, frame);
	if (additional->size() >= to.size()) {
		return;
	}
	std::swap(to, *additional);

	// Negative length means we XOR-d with the previous frame.
	const auto negativeLength = -qint32(to.size() - sizeof(qint32));
	bytes::copy(
		bytes::make_detached_span(to),
		bytes::object_as_span(&negativeLength));
}

bool UncompressToRaw(EncodedStorage &to, bytes::const_span from) {
	if (from.empty() || from.size() > to.size()) {
		return false;
	} else if (from.size() == to.size()) {
		memcpy(to.data(), from.data(), from.size());
		return true;
	}
	const auto result = LZ4_decompress_safe(
		reinterpret_cast<const char*>(from.data()),
		to.data(),
		from.size(),
		to.size());
	return (result == to.size());
}

} // namespace Lottie
