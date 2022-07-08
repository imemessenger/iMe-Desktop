// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/crash_report_writer.h"

#ifndef DESKTOP_APP_DISABLE_CRASH_REPORTS

#include "base/platform/base_platform_info.h"
#include "base/integration.h"
#include "base/crash_report_header.h"

#include <QtCore/QDir>
#include <QtCore/QDateTime>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <signal.h>
#include <new>
#include <mutex>

#if !defined Q_OS_MAC || defined MAC_USE_BREAKPAD
#define USE_BREAKPAD
#endif // !Q_OS_MAC || MAC_USE_BREAKPAD

// see https://blog.inventic.eu/2012/08/qt-and-google-breakpad/
#ifdef Q_OS_WIN

#include <io.h>
#include <fcntl.h>

#pragma warning(push)
#pragma warning(disable:4091)
#include <client/windows/handler/exception_handler.h>
#pragma warning(pop)

#elif defined Q_OS_MAC // Q_OS_WIN

#include <execinfo.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <unistd.h>

#ifdef USE_BREAKPAD
#include <client/mac/handler/exception_handler.h>
#else // USE_BREAKPAD
#include <client/crashpad_client.h>
#endif // USE_BREAKPAD

#elif defined Q_OS_UNIX // Q_OS_MAC

#include <execinfo.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <client/linux/handler/exception_handler.h>

#endif // Q_OS_UNIX

namespace base {
namespace {

using namespace details;

CrashReportWriter *Instance = nullptr;

QMutex AnnotationsMutex;
std::map<std::string, std::string> Annotations;

int ReportFileNo = -1;

std::atomic<Qt::HANDLE> ReportingThreadId = nullptr;
bool SkipWriteReportHeader = false;
bool ReportingHeaderWritten = false;
QMutex ReportingMutex;

#ifdef Q_OS_WIN
const wchar_t *BreakpadDumpId = nullptr;
std::wstring FinalReportPath;
#else // Q_OS_WIN
const char *BreakpadDumpId = nullptr;
std::string FinalReportPath;
#endif // Q_OS_WIN

using ReservedMemoryChunk = std::array<gsl::byte, 1024 * 1024>;
std::unique_ptr<ReservedMemoryChunk> ReservedMemory;

const char *PlatformString() {
	if (Platform::IsWindowsStoreBuild()) {
		return Platform::IsWindows64Bit()
			? "WinStore64Bit"
			: "WinStore32Bit";
	} else if (Platform::IsWindows32Bit()) {
		return "Windows32Bit";
	} else if (Platform::IsWindows64Bit()) {
		return "Windows64Bit";
	} else if (Platform::IsMacStoreBuild()) {
		return "MacAppStore";
	} else if (Platform::IsMac()) {
		return "MacOS";
	} else if (Platform::IsLinux()) {
		return "Linux";
	}
	Unexpected("Platform in CrashReports::PlatformString.");
}

void AddAnnotation(std::string key, std::string value) {
	QMutexLocker lock(&AnnotationsMutex);
	Annotations.emplace(std::move(key), std::move(value));
}

void InstallOperatorNewHandler() {
	ReservedMemory = std::make_unique<ReservedMemoryChunk>();
	std::set_new_handler([] {
		std::set_new_handler(nullptr);
		ReservedMemory.reset();
		Unexpected("Could not allocate!");
	});
}

void InstallQtMessageHandler() {
	static QtMessageHandler original = nullptr;
	original = qInstallMessageHandler([](
			QtMsgType type,
			const QMessageLogContext &context,
			const QString &message) {
		if (original) {
			original(type, context, message);
		}
		if (type == QtFatalMsg && Instance) {
			AddAnnotation("QtFatal", message.toStdString());
			Unexpected("Qt FATAL message was generated!");
		}
	});
}

#ifdef Q_OS_UNIX
struct sigaction SIG_def[32];

void SignalHandler(int signum, siginfo_t *info, void *ucontext) {
	if (signum > 0) {
		sigaction(signum, &SIG_def[signum], 0);
	}

#else // Q_OS_UNIX
void SignalHandler(int signum) {
#endif // else for Q_OS_UNIX

	const char* name = 0;
	switch (signum) {
	case SIGABRT: name = "SIGABRT"; break;
	case SIGSEGV: name = "SIGSEGV"; break;
	case SIGILL: name = "SIGILL"; break;
	case SIGFPE: name = "SIGFPE"; break;
#ifndef Q_OS_WIN
	case SIGBUS: name = "SIGBUS"; break;
	case SIGSYS: name = "SIGSYS"; break;
#endif // !Q_OS_WIN
	}

	const auto thread = QThread::currentThreadId();
	if (thread == ReportingThreadId) {
		return;
	}

	QMutexLocker lock(&ReportingMutex);
	ReportingThreadId = thread;

	if (SkipWriteReportHeader || ReportFileNo < 0) {
		return;
	}
	if (!ReportingHeaderWritten) {
		ReportingHeaderWritten = true;

		QMutexLocker lock(&AnnotationsMutex);
		for (const auto &i : Annotations) {
			ReportHeaderWriter() << i.first.c_str() << ": " << i.second.c_str() << "\n";
		}
		ReportHeaderWriter() << "\n";
	}
	if (name) {
		ReportHeaderWriter() << "Caught signal " << signum << " (" << name << ") in thread " << uint64(thread) << "\n";
	} else if (signum == -1) {
		ReportHeaderWriter() << "Google Breakpad caught a crash, minidump written in thread " << uint64(thread) << "\n";
		if (BreakpadDumpId) {
			ReportHeaderWriter() << "Minidump: " << BreakpadDumpId << "\n";
		}
	} else {
		ReportHeaderWriter() << "Caught signal " << signum << " in thread " << uint64(thread) << "\n";
	}

#ifdef Q_OS_WIN
	_write(ReportFileNo, ReportHeaderBytes(), ReportHeaderLength());
	_close(ReportFileNo);
#else // Q_OS_WIN
	[[maybe_unused]] auto result_ = write(ReportFileNo, ReportHeaderBytes(), ReportHeaderLength());
	close(ReportFileNo);
#endif // Q_OS_WIN
	ReportFileNo = -1;

#ifdef Q_OS_WIN
	if (BreakpadDumpId) {
		FinalReportPath.append(BreakpadDumpId);
		FinalReportPath.append(L".txt");
		auto handle = int();
		const auto errcode = _wsopen_s(
			&handle,
			FinalReportPath.c_str(),
			_O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY,
			_SH_DENYWR,
			_S_IWRITE);
		if (!errcode) {
			_write(handle, ReportHeaderBytes(), ReportHeaderLength());
			_close(handle);
		}
	}
#else // Q_OS_WIN
	if (BreakpadDumpId) {
		FinalReportPath.append(BreakpadDumpId);
		const auto good = int(FinalReportPath.size()) - 4;
		if (good > 0 && !strcmp(FinalReportPath.c_str() + good, ".dmp")) {
			FinalReportPath.erase(FinalReportPath.begin() + good, FinalReportPath.end());
		}
		FinalReportPath.append(".txt");
		const auto handle = open(
			FinalReportPath.c_str(),
			O_WRONLY | O_CREAT | O_TRUNC,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (handle >= 0) {
			[[maybe_unused]] auto result_ = write(handle, ReportHeaderBytes(), ReportHeaderLength());
			close(handle);
		}
	}
#endif // Q_OS_WIN

	ReportingThreadId = nullptr;
}

bool SetSignalHandlers = Platform::IsLinux() || Platform::IsMac();
bool CrashLogged = false;

#ifdef USE_BREAKPAD
google_breakpad::ExceptionHandler* BreakpadExceptionHandler = 0;

#ifdef Q_OS_WIN
bool DumpCallback(const wchar_t* _dump_dir, const wchar_t* _minidump_id, void* context, EXCEPTION_POINTERS* exinfo, MDRawAssertionInfo* assertion, bool success)
#elif defined Q_OS_MAC // Q_OS_WIN
bool DumpCallback(const char* _dump_dir, const char* _minidump_id, void *context, bool success)
#elif defined Q_OS_UNIX // Q_OS_MAC
bool DumpCallback(const google_breakpad::MinidumpDescriptor &md, void *context, bool success)
#endif // Q_OS_UNIX
{
	if (CrashLogged) return success;
	CrashLogged = true;

#ifdef Q_OS_WIN
	BreakpadDumpId = _minidump_id;
	SignalHandler(-1);
#else // Q_OS_WIN

#ifdef Q_OS_MAC
	BreakpadDumpId = _minidump_id;
#else // Q_OS_MAC
	BreakpadDumpId = md.path();
	auto afterLastSlash = BreakpadDumpId;
	for (auto ch = afterLastSlash; *ch != 0; ++ch) {
		if (*ch == '/') {
			afterLastSlash = (ch + 1);
		}
	}
	if (*afterLastSlash) {
		BreakpadDumpId = afterLastSlash;
	}
#endif // else for Q_OS_MAC
	SignalHandler(-1, 0, 0);

#endif // else for Q_OS_WIN

	return success;
}
#endif // USE_BREAKPAD

} // namespace

CrashReportWriter::CrashReportWriter(const QString &path) : _path(path) {
	Expects(Instance == nullptr);
	Expects(_path.endsWith('/'));

	Instance = this;
	_previousReport = readPreviousReport();
}

CrashReportWriter::~CrashReportWriter() {
	Expects(Instance == this);

	finishCatching();
	closeReport();

	Instance = nullptr;
}

void CrashReportWriter::start() {
	AddAnnotation(
		"Launched",
		QDateTime::currentDateTime().toString(
			"dd.MM.yyyy hh:mm:ss"
		).toStdString());
	AddAnnotation("Platform", PlatformString());

	QDir().mkpath(_path);

	openReport();
	startCatching();
}

bool CrashReportWriter::openReport() {
	if (ReportFileNo >= 0) {
		return true;
	}

	// Try to lock the report file to kill
	// all remaining processes that opened it.
	_reportFile.setFileName(reportPath());
	if (!_reportLock.lock(_reportFile, QIODevice::WriteOnly)) {
		return false;
	}
	ReportFileNo = _reportFile.handle();
	if (ReportFileNo < 0) {
		return false;
	}
#ifdef Q_OS_WIN
	FinalReportPath = _path.toStdWString();
#else // Q_OS_WIN
	FinalReportPath = QFile::encodeName(_path).toStdString();
#endif // Q_OS_WIN
	FinalReportPath.reserve(FinalReportPath.size() + 1024);

	if (SetSignalHandlers) {
#ifndef Q_OS_WIN
		struct sigaction sigact;

		sigact.sa_sigaction = SignalHandler;
		sigemptyset(&sigact.sa_mask);
		sigact.sa_flags = SA_NODEFER | SA_RESETHAND | SA_SIGINFO;

		sigaction(SIGABRT, &sigact, &SIG_def[SIGABRT]);
		sigaction(SIGSEGV, &sigact, &SIG_def[SIGSEGV]);
		sigaction(SIGILL, &sigact, &SIG_def[SIGILL]);
		sigaction(SIGFPE, &sigact, &SIG_def[SIGFPE]);
		sigaction(SIGBUS, &sigact, &SIG_def[SIGBUS]);
		sigaction(SIGSYS, &sigact, &SIG_def[SIGSYS]);
#else // !Q_OS_WIN
		signal(SIGABRT, SignalHandler);
		signal(SIGSEGV, SignalHandler);
		signal(SIGILL, SignalHandler);
		signal(SIGFPE, SignalHandler);
#endif // else for !Q_OS_WIN
	}

	InstallOperatorNewHandler();
	InstallQtMessageHandler();

	return true;
}

void CrashReportWriter::closeReport() {
	QMutexLocker lock(&ReportingMutex);
	if (SkipWriteReportHeader) {
		return;
	}
	SkipWriteReportHeader = true;
	lock.unlock();

	_reportLock.unlock();
	_reportFile.close();
	_reportFile.remove();
	ReportFileNo = -1;
}

void CrashReportWriter::startCatching() {
#ifdef Q_OS_WIN
	BreakpadExceptionHandler = new google_breakpad::ExceptionHandler(
		_path.toStdWString(),
		google_breakpad::ExceptionHandler::FilterCallback(nullptr),
		DumpCallback,
		(void*)nullptr, // callback_context
		google_breakpad::ExceptionHandler::HANDLER_ALL,
		MINIDUMP_TYPE(MiniDumpNormal),
		// MINIDUMP_TYPE(MiniDumpWithFullMemory | MiniDumpWithHandleData | MiniDumpWithThreadInfo | MiniDumpWithProcessThreadData | MiniDumpWithFullMemoryInfo | MiniDumpWithUnloadedModules | MiniDumpWithFullAuxiliaryState | MiniDumpIgnoreInaccessibleMemory | MiniDumpWithTokenInformation),
		(const wchar_t*)nullptr, // pipe_name
		(const google_breakpad::CustomClientInfo*)nullptr
	);
#elif defined Q_OS_MAC // Q_OS_WIN

#ifdef USE_BREAKPAD
#ifndef _DEBUG
	BreakpadExceptionHandler = new google_breakpad::ExceptionHandler(
		QFile::encodeName(_path).toStdString(),
		/*FilterCallback*/ 0,
		DumpCallback,
		/*context*/ 0,
		true,
		0
	);
#endif // !_DEBUG
#else // USE_BREAKPAD
	crashpad::CrashpadClient crashpad_client;
	const auto handler = (Integration::Instance().executablePath() + "/Contents/Helpers/crashpad_handler").toStdString();
	const auto database = QFile::encodeName(_path).constData();
	if (crashpad_client.StartHandler(base::FilePath(handler),
										base::FilePath(database),
										std::string(),
										Annotations,
										std::vector<std::string>(),
										false)) {
		crashpad_client.UseHandler();
	}
#endif // USE_BREAKPAD
#elif defined Q_OS_UNIX
	BreakpadExceptionHandler = new google_breakpad::ExceptionHandler(
		google_breakpad::MinidumpDescriptor(QFile::encodeName(_path).toStdString()),
		/*FilterCallback*/ 0,
		DumpCallback,
		/*context*/ 0,
		true,
		-1
	);
#endif // Q_OS_UNIX
}

void CrashReportWriter::finishCatching() {
#ifdef USE_BREAKPAD
	delete base::take(BreakpadExceptionHandler);
#endif // USE_BREAKPAD
}

void CrashReportWriter::addAnnotation(std::string key, std::string value) {
	AddAnnotation(std::move(key), std::move(value));
}

QString CrashReportWriter::reportPath() const {
	return _path + "report";
}

std::optional<QByteArray> CrashReportWriter::readPreviousReport() {
	auto file = QFile(reportPath());
	if (file.open(QIODevice::ReadOnly)) {
		return file.readAll();
	}
	return std::nullopt;
}

} // namespace CrashReports

#endif // DESKTOP_APP_DISABLE_CRASH_REPORTS
