/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "base/platform/linux/base_linux_allocation_tracer.h"

#include "base/debug_log.h"

#include <mutex>
#include <stdio.h>
#include <unistd.h>

void SetMallocLogger(void (*logger)(size_t, void *));
void SetVallocLogger(void (*logger)(size_t, void *));
void SetPVallocLogger(void (*logger)(size_t, void *));
void SetReallocLogger(void (*logger)(void *, size_t, void *));
void SetFreeLogger(void (*logger)(void *));
void SetMemAlignLogger(void (*logger)(size_t, size_t, void *));
void SetAlignedAllocLogger(void (*logger)(size_t, size_t, void *));
void SetPosixMemAlignLogger(void (*logger)(size_t, size_t, void *));
void SetCallocLogger(void (*logger)(size_t, size_t, void *));

namespace base::Platform {
namespace {

#ifdef DESKTOP_APP_USE_ALLOCATION_TRACER

constexpr auto kBufferSize = 1024 * 1024;

char *Buffer/* = nullptr*/;
FILE *File/* = 0*/;
char *Data/* = nullptr*/;
std::mutex Mutex;

void WriteBlock() {
    if (Data > Buffer) {
        fwrite(Buffer, Data - Buffer, 1, File);
        fflush(File);
        fdatasync(fileno(File));
        Data = Buffer;
    }
}

template <size_t Size>
void AppendEntry(char (&bytes)[Size]) {
    std::unique_lock<std::mutex> lock(Mutex);
    if (!File) {
        return;
    } else if (Data + Size > Buffer + kBufferSize) {
        WriteBlock();
    }
    *reinterpret_cast<std::uint32_t*>(bytes + 1) = uint32_t(time(nullptr));
    memcpy(Data, bytes, Size);
    Data += Size;
}

void MallocLogger(size_t size, void *result) {
    char entry[5 + sizeof(std::uint64_t) * 2];
    entry[0] = 1;
    *reinterpret_cast<std::uint64_t*>(entry + 5)
        = static_cast<std::uint64_t>(size);
    *reinterpret_cast<std::uint64_t*>(entry + 5 + sizeof(std::uint64_t))
        = reinterpret_cast<std::uint64_t>(result);
    AppendEntry(entry);
}

void VallocLogger(size_t size, void *result) {
    MallocLogger(size, result);
}

void PVallocLogger(size_t size, void *result) {
    MallocLogger(size, result);
}

void CallocLogger(size_t num, size_t size, void *result) {
    MallocLogger(num * size, result);
}

void ReallocLogger(void *ptr, size_t size, void *result) {
    if (!ptr) {
        return MallocLogger(size, result);
    }
    char entry[5 + sizeof(std::uint64_t) * 3];
    entry[0] = 2;
    *reinterpret_cast<std::uint64_t*>(entry + 5)
        = reinterpret_cast<std::uint64_t>(ptr);
    *reinterpret_cast<std::uint64_t*>(entry + 5 + sizeof(std::uint64_t))
        = static_cast<std::uint64_t>(size);
    *reinterpret_cast<std::uint64_t*>(entry + 5 + sizeof(std::uint64_t) * 2)
        = reinterpret_cast<std::uint64_t>(result);
    AppendEntry(entry);
}

void MemAlignLogger(size_t alignment, size_t size, void *result) {
    MallocLogger(size, result);
}

void AlignedAllocLogger(size_t alignment, size_t size, void *result) {
    MallocLogger(size, result);
}

void PosixMemAlignLogger(size_t alignment, size_t size, void *result) {
    MallocLogger(size, result);
}

void FreeLogger(void *ptr) {
    if (ptr) {
        char entry[5 + sizeof(std::uint64_t)];
        entry[0] = 3;
        *reinterpret_cast<std::uint64_t*>(entry + 5)
            = reinterpret_cast<std::uint64_t>(ptr);
        AppendEntry(entry);
    }
}

void InstallLoggers() {
    SetMallocLogger(MallocLogger);
    SetVallocLogger(VallocLogger);
    SetPVallocLogger(PVallocLogger);
    SetCallocLogger(CallocLogger);
    SetReallocLogger(ReallocLogger);
    SetMemAlignLogger(MemAlignLogger);
    SetAlignedAllocLogger(AlignedAllocLogger);
    SetPosixMemAlignLogger(PosixMemAlignLogger);
    SetFreeLogger(FreeLogger);
}

void RemoveLoggers() {
    SetMallocLogger(nullptr);
    SetVallocLogger(nullptr);
    SetPVallocLogger(nullptr);
    SetCallocLogger(nullptr);
    SetReallocLogger(nullptr);
    SetMemAlignLogger(nullptr);
    SetAlignedAllocLogger(nullptr);
    SetPosixMemAlignLogger(nullptr);
    SetFreeLogger(nullptr);
}

#endif // DESKTOP_APP_USE_ALLOCATION_TRACER

} // namespace

void SetAllocationTracerPath(const QString &path) {
#ifdef DESKTOP_APP_USE_ALLOCATION_TRACER
    Expects(!Buffer);

    Data = Buffer = new char[kBufferSize];
    if (!Buffer) {
        return;
    }
    File = fopen(path.toStdString().c_str(), "wb");
    if (!File) {
        return;
    }
    InstallLoggers();
#endif // DESKTOP_APP_USE_ALLOCATION_TRACER
}

void FinishAllocationTracer() {
#ifdef DESKTOP_APP_USE_ALLOCATION_TRACER
    if (File) {
        RemoveLoggers();

        std::unique_lock<std::mutex> lock(Mutex);
        WriteBlock();
        fclose(File);
        File = nullptr;
    }
#endif // DESKTOP_APP_USE_ALLOCATION_TRACER
}

} // namespace base::Platform
