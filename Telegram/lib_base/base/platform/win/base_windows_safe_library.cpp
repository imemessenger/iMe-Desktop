// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/win/base_windows_safe_library.h"

#include <string>
#include <array>

#define LOAD_SYMBOL(lib, name) ::base::Platform::LoadMethod(lib, #name, name)

namespace base {
namespace Platform {
namespace {

constexpr auto kMaxPathLong = 32767;

__declspec(noreturn) void FatalError(
		const std::wstring &text,
		DWORD error = 0) {
	const auto lastError = error ? error : GetLastError();
	const auto full = lastError
		? (text + L"\n\nError Code: " + std::to_wstring(lastError))
		: text;
	MessageBox(nullptr, full.c_str(), L"Fatal Error", MB_ICONERROR);
	std::abort();
}

void CheckDynamicLibraries() {
	auto exePath = std::array<WCHAR, kMaxPathLong + 1>{ 0 };
	const auto exeLength = GetModuleFileName(
		nullptr,
		exePath.data(),
		kMaxPathLong + 1);
	if (!exeLength || exeLength >= kMaxPathLong + 1) {
		FatalError(L"Could not get executable path!");
	}
	const auto exe = std::wstring(exePath.data());
	const auto last1 = exe.find_last_of('\\');
	const auto last2 = exe.find_last_of('/');
	const auto last = std::max(
		(last1 == std::wstring::npos) ? -1 : int(last1),
		(last2 == std::wstring::npos) ? -1 : int(last2));
	if (last < 0) {
		FatalError(L"Could not get executable directory!");
	}
	const auto search = exe.substr(0, last + 1) + L"*.dll";

	auto findData = WIN32_FIND_DATA();
	const auto findHandle = FindFirstFile(search.c_str(), &findData);
	if (findHandle == INVALID_HANDLE_VALUE) {
		const auto error = GetLastError();
		if (error == ERROR_FILE_NOT_FOUND) {
			return;
		}
		FatalError(L"Could not enumerate executable path!", error);
	}

	do {
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			continue;
		}
		const auto me = exe.substr(last + 1);
		FatalError(L"Unknown DLL library \"\
" + std::wstring(findData.cFileName) + L"\" found \
in the directory with " + me + L".\n\n\
This may be a virus or a malicious program. \n\n\
Please remove all DLL libraries from this directory:\n\n\
" + exe.substr(0, last) + L"\n\n\
Alternatively, you can move " + me + L" to a new directory.");
	} while (FindNextFile(findHandle, &findData));
}

BOOL (__stdcall *SetDefaultDllDirectories)(_In_ DWORD DirectoryFlags);

} // namespace

void InitDynamicLibraries() {
	static const auto Inited = [] {
		const auto kernel = LoadLibrary(L"kernel32.dll");
		if (LOAD_SYMBOL(kernel, SetDefaultDllDirectories)) {
			SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
		} else {
			CheckDynamicLibraries();
		}
		return true;
	}();
}

HINSTANCE SafeLoadLibrary(LPCWSTR name, bool required) {
	InitDynamicLibraries();

	if (const auto result = HINSTANCE(LoadLibrary(name))) {
		return result;
	} else if (required) {
		FatalError(L"Could not load required DLL '"
			+ std::wstring(name)
			+ L"'!");
	}
	return nullptr;
}

} // namespace Platform
} // namespace base
