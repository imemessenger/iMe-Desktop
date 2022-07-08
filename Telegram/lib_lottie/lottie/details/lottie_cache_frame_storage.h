// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ffmpeg/ffmpeg_utility.h"

namespace Lottie {

class EncodedStorage {
public:
	void allocate(int width, int height);

	int width() const;
	int height() const;

	char *data();
	const char *data() const;
	int size() const;

	uint8_t *yData();
	const uint8_t *yData() const;
	int yBytesPerLine() const;
	uint8_t *uData();
	const uint8_t *uData() const;
	int uBytesPerLine() const;
	uint8_t *vData();
	const uint8_t *vData() const;
	int vBytesPerLine() const;
	uint8_t *aData();
	const uint8_t *aData() const;
	int aBytesPerLine() const;

private:
	void reallocate();

	int _width = 0;
	int _height = 0;
	QByteArray _data;

};

void Xor(EncodedStorage &to, const EncodedStorage &from);

void Encode(
	EncodedStorage &to,
	const QImage &from,
	QImage &cache,
	FFmpeg::SwscalePointer &context);

void Decode(
	QImage &to,
	const EncodedStorage &from,
	const QSize &fromSize,
	FFmpeg::SwscalePointer &context);

void CompressFromRaw(QByteArray &to, const EncodedStorage &from);
void CompressAndSwapFrame(
	QByteArray &to,
	QByteArray *additional,
	EncodedStorage &frame,
	EncodedStorage &previous);

bool UncompressToRaw(EncodedStorage &to, bytes::const_span from);

} // namespace Lottie
