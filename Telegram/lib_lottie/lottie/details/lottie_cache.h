// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ffmpeg/ffmpeg_utility.h"
#include "lottie/details/lottie_cache_frame_storage.h"
#include "lottie/lottie_common.h"

#include <QImage>
#include <QSize>
#include <QByteArray>

namespace Lottie {

struct FrameRequest;

struct CacheReadContext {
	EncodedStorage uncompressed;
	EncodedStorage previous;
	FFmpeg::SwscalePointer decodeContext;
	int offset = 0;
	int offsetFrameIndex = 0;

	[[nodiscard]] bool ready() const {
		return (offset != 0);
	}
};

class Cache {
public:
	enum class Encoder : qint8 {
		YUV420A4_LZ4,
	};

	Cache(
		const QByteArray &data,
		const FrameRequest &request,
		FnMut<void(QByteArray &&cached)> put);
	Cache(Cache &&);
	Cache &operator=(Cache&&);
	~Cache();

	void init(
		QSize original,
		int frameRate,
		int framesCount,
		const FrameRequest &request);
	[[nodiscard]] int sizeRounding() const;
	[[nodiscard]] int frameRate() const;
	[[nodiscard]] int framesReady() const;
	[[nodiscard]] int framesCount() const;
	[[nodiscard]] QSize originalSize() const;
	[[nodiscard]] QImage takeFirstFrame();

	void prepareBuffers(CacheReadContext &context) const;
	void keepUpContext(CacheReadContext &context) const;

	[[nodiscard]] FrameRenderResult renderFrame(
		QImage &to,
		const FrameRequest &request,
		int index);
	[[nodiscard]] FrameRenderResult renderFrame(
		CacheReadContext &context,
		QImage &to,
		const FrameRequest &request,
		int index) const;
	void appendFrame(
		const QImage &frame,
		const FrameRequest &request,
		int index);

private:
	struct ReadResult {
		bool ok = false;
		bool xored = false;
	};
	struct EncodeFields {
		std::vector<QByteArray> compressedFrames;
		QByteArray compressBuffer;
		QByteArray xorCompressBuffer;
		QImage cache;
		FFmpeg::SwscalePointer context;
		int totalSize = 0;
	};
	int headerSize() const;
	void prepareBuffers();
	void finalizeEncoding();

	void writeHeader();
	void updateFramesReadyCount();
	[[nodiscard]] bool readHeader(const FrameRequest &request);
	[[nodiscard]] ReadResult readCompressedFrame(
		CacheReadContext &context) const;

	QByteArray _data;
	EncodeFields _encode;
	QSize _size;
	QSize _original;
	CacheReadContext _readContext;
	QImage _firstFrame;
	int _frameRate = 0;
	int _framesCount = 0;
	int _framesReady = 0;
	int _framesInData = 0;
	Encoder _encoder = Encoder::YUV420A4_LZ4;
	FnMut<void(QByteArray &&cached)> _put;

};

} // namespace Lottie
