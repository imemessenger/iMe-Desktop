// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/file_lock.h"

#include "base/platform/win/base_windows_h.h"
#include "base/platform/win/base_file_utilities_win.h"

#include <QtCore/QDir>
#include <io.h>
#include <fileapi.h>

namespace base {

class FileLock::Lock {
public:
	static int Acquire(const QFile &file);

	explicit Lock(int descriptor);
	~Lock();

private:
	static constexpr auto offsetLow = DWORD(kLockOffset);
	static constexpr auto offsetHigh = DWORD(0);
	static constexpr auto limitLow = DWORD(kLockLimit);
	static constexpr auto limitHigh = DWORD(0);

	int _descriptor = 0;

};

int FileLock::Lock::Acquire(const QFile &file) {
	const auto descriptor = file.handle();
	if (!descriptor || !file.isOpen()) {
		return false;
	}
	const auto handle = HANDLE(_get_osfhandle(descriptor));
	if (!handle) {
		return false;
	}
	return LockFile(handle, offsetLow, offsetHigh, limitLow, limitHigh)
		? descriptor
		: 0;
}

FileLock::Lock::Lock(int descriptor) : _descriptor(descriptor) {
}

FileLock::Lock::~Lock() {
	if (const auto handle = HANDLE(_get_osfhandle(_descriptor))) {
		UnlockFile(handle, offsetLow, offsetHigh, limitLow, limitHigh);
	}
}

FileLock::FileLock() = default;

bool FileLock::lock(QFile &file, QIODevice::OpenMode mode) {
	Expects(_lock == nullptr || file.isOpen());

	unlock();
	file.close();
	do {
		if (!file.open(mode)) {
			return false;
		} else if (const auto descriptor = Lock::Acquire(file)) {
			_lock = std::make_unique<Lock>(descriptor);
			return true;
		}
		file.close();
	} while (Platform::CloseProcesses(file.fileName()));

	return false;
}

bool FileLock::locked() const {
	return (_lock != nullptr);
}

void FileLock::unlock() {
	_lock = nullptr;
}

FileLock::~FileLock() = default;

} // namespace base
