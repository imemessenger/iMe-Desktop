// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/file_lock.h"

#include <variant>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>

namespace base {
namespace {

bool KillProcess(pid_t pid) {
	auto signal = SIGTERM;
	auto attempts = 0;
	while (true) {
		const auto result = kill(pid, signal);
		if (result < 0) {
			return (errno == ESRCH);
		}
		usleep(10000);
		if (++attempts == 50) {
			signal = SIGKILL;
		}
	}
}

} // namespace

struct FileLock::Descriptor {
	int value;
};

struct FileLock::LockingPid {
	pid_t value;
};

class FileLock::Lock {
public:
	using Result = std::variant<Descriptor, LockingPid>;
	static Result Acquire(const QFile &file);

	explicit Lock(int descriptor);
	~Lock();

private:
	int _descriptor = 0;

};

FileLock::Lock::Result FileLock::Lock::Acquire(const QFile &file) {
	const auto descriptor = file.handle();
	if (!descriptor || !file.isOpen()) {
		return Descriptor{ 0 };
	}
	while (true) {
		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_whence = SEEK_SET;
		lock.l_start = kLockOffset;
		lock.l_len = kLockLimit;
		if (fcntl(descriptor, F_SETLK, &lock) == 0) {
			return Descriptor{ descriptor };
		} else if (fcntl(descriptor, F_GETLK, &lock) < 0) {
			return LockingPid{ 0 };
		} else if (lock.l_type != F_UNLCK) {
			return LockingPid{ lock.l_pid };
		}
	}
}

FileLock::Lock::Lock(int descriptor) : _descriptor(descriptor) {
}

FileLock::Lock::~Lock() {
	struct flock unlock;
	unlock.l_type = F_UNLCK;
	unlock.l_whence = SEEK_SET;
	unlock.l_start = kLockOffset;
	unlock.l_len = kLockLimit;
	fcntl(_descriptor, F_SETLK, &unlock);
}

FileLock::FileLock() = default;

bool FileLock::lock(QFile &file, QIODevice::OpenMode mode) {
	Expects(_lock == nullptr || file.isOpen());

	unlock();
	file.close();
	if (!file.open(mode)) {
		return false;
	}
	while (true) {
		const auto result = Lock::Acquire(file);
		if (const auto descriptor = std::get_if<Descriptor>(&result)) {
			if (descriptor->value > 0) {
				_lock = std::make_unique<Lock>(descriptor->value);
				return true;
			}
			break;
		} else if (const auto pid = std::get_if<LockingPid>(&result)) {
			if (pid->value <= 0 || !KillProcess(pid->value)) {
				break;
			}
		}
	}
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
