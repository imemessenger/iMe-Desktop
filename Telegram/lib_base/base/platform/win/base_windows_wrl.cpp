// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/win/base_windows_wrl.h"

#include "base/platform/win/base_windows_safe_library.h"
#include "base/platform/base_platform_info.h"

#define LOAD_SYMBOL(lib, name) ::base::Platform::LoadMethod(lib, #name, name)

namespace base::Platform {
namespace {

using namespace ::Platform;

HRESULT(__stdcall *RoGetActivationFactory)(
	_In_ HSTRING activatableClassId,
	_In_ REFIID iid,
	_COM_Outptr_ void ** factory);

HRESULT(__stdcall *RoActivateInstance)(
	_In_ HSTRING activatableClassId,
	_COM_Outptr_ IInspectable** instance);

HRESULT(__stdcall *RoRegisterActivationFactories)(
	_In_reads_(count) HSTRING* activatableClassIds,
	_In_reads_(count) PFNGETACTIVATIONFACTORY* activationFactoryCallbacks,
	_In_ UINT32 count,
	_Out_ RO_REGISTRATION_COOKIE* cookie);

void(__stdcall *RoRevokeActivationFactories)(
	_In_ RO_REGISTRATION_COOKIE cookie);

HRESULT(__stdcall *WindowsCreateString)(
	_In_reads_opt_(length) PCNZWCH sourceString,
	UINT32 length,
	_Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING* string);

HRESULT(__stdcall *WindowsCreateStringReference)(
	_In_reads_opt_(length + 1) PCWSTR sourceString,
	UINT32 length,
	_Out_ HSTRING_HEADER * hstringHeader,
	_Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING *string);

HRESULT(__stdcall *WindowsDeleteString)(
	_In_opt_ HSTRING string);

PCWSTR(__stdcall *WindowsGetStringRawBuffer)(
	_In_opt_ HSTRING string,
	_Out_opt_ UINT32* length);

BOOL(__stdcall *WindowsIsStringEmpty)(
	_In_opt_ HSTRING string);

HRESULT(__stdcall *WindowsStringHasEmbeddedNull)(
	_In_opt_ HSTRING string,
	_Out_ BOOL* hasEmbedNull);

BOOL(__stdcall *RoOriginateErrorW)(
	_In_ HRESULT error,
	_In_ UINT cchMax,
	_When_(cchMax == 0, _In_reads_or_z_opt_(MAX_ERROR_MESSAGE_CHARS))
	_When_(cchMax > 0 && cchMax < MAX_ERROR_MESSAGE_CHARS, _In_reads_or_z_(cchMax))
	_When_(cchMax >= MAX_ERROR_MESSAGE_CHARS, _In_reads_or_z_(MAX_ERROR_MESSAGE_CHARS)) PCWSTR message);

BOOL(__stdcall *RoOriginateError)(
	_In_ HRESULT error,
	_In_opt_ HSTRING message);

} // namespace

bool SupportsWRL() {
	struct State {
		bool inited = false;
		bool supported = false;
	};
	static auto state = State();
	if (state.inited) {
		return state.supported;
	}
	state.inited = true;
	if (IsWindows8OrGreater()) {
		const auto combase = SafeLoadLibrary(L"combase.dll");
		state.supported = combase
			&& LOAD_SYMBOL(combase, RoGetActivationFactory)
			&& LOAD_SYMBOL(combase, RoActivateInstance)
			&& LOAD_SYMBOL(combase, RoRegisterActivationFactories)
			&& LOAD_SYMBOL(combase, RoRevokeActivationFactories)
			&& LOAD_SYMBOL(combase, WindowsCreateString)
			&& LOAD_SYMBOL(combase, WindowsCreateStringReference)
			&& LOAD_SYMBOL(combase, WindowsDeleteString)
			&& LOAD_SYMBOL(combase, WindowsGetStringRawBuffer)
			&& LOAD_SYMBOL(combase, WindowsIsStringEmpty)
			&& LOAD_SYMBOL(combase, WindowsStringHasEmbeddedNull);
	}
	return state.supported;
}

} // namespace base::Platform

namespace P = base::Platform;

extern "C" {

_Check_return_ HRESULT WINAPI RoActivateInstance(
	_In_ HSTRING activatableClassId,
	_COM_Outptr_ IInspectable** instance
) {
	return P::SupportsWRL()
		? P::RoActivateInstance(activatableClassId, instance)
		: CO_E_DLLNOTFOUND;
}

_Check_return_ HRESULT WINAPI RoGetActivationFactory(
	_In_ HSTRING activatableClassId,
	_In_ REFIID iid,
	_COM_Outptr_ void** factory
) {
	return P::SupportsWRL()
		? P::RoGetActivationFactory(activatableClassId, iid, factory)
		: CO_E_DLLNOTFOUND;
}

_Check_return_ HRESULT WINAPI RoRegisterActivationFactories(
	_In_reads_(count) HSTRING* activatableClassIds,
	_In_reads_(count) PFNGETACTIVATIONFACTORY* activationFactoryCallbacks,
	_In_ UINT32 count,
	_Out_ RO_REGISTRATION_COOKIE* cookie
) {
	return P::SupportsWRL()
		? P::RoRegisterActivationFactories(
			activatableClassIds,
			activationFactoryCallbacks,
			count,
			cookie)
		: CO_E_DLLNOTFOUND;
}

void
WINAPI
RoRevokeActivationFactories(
	_In_ RO_REGISTRATION_COOKIE cookie
) {
	if (P::SupportsWRL()) {
		P::RoRevokeActivationFactories(cookie);
	}
}

STDAPI
WindowsCreateString(
	_In_reads_opt_(length) PCNZWCH sourceString,
	UINT32 length,
	_Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING* string
) {
	return P::SupportsWRL()
		? P::WindowsCreateString(sourceString, length, string)
		: CO_E_DLLNOTFOUND;
}

STDAPI
WindowsCreateStringReference(
	_In_reads_opt_(length + 1) PCWSTR sourceString,
	UINT32 length,
	_Out_ HSTRING_HEADER* hstringHeader,
	_Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING* string
) {
	return P::SupportsWRL()
		? P::WindowsCreateStringReference(
			sourceString,
			length,
			hstringHeader,
			string)
		: CO_E_DLLNOTFOUND;
}

STDAPI
WindowsDeleteString(
	_In_opt_ HSTRING string
) {
	return P::SupportsWRL()
		? P::WindowsDeleteString(string)
		: CO_E_DLLNOTFOUND;
}

STDAPI_(PCWSTR)
WindowsGetStringRawBuffer(
		_In_opt_ HSTRING string,
		_Out_opt_ UINT32* length) {
	return P::SupportsWRL()
		? P::WindowsGetStringRawBuffer(string, length)
		: nullptr;
}

STDAPI_(BOOL)
WindowsIsStringEmpty(
	_In_opt_ HSTRING string
) {
	return P::SupportsWRL() && P::WindowsIsStringEmpty(string);
}

STDAPI
WindowsStringHasEmbeddedNull(
	_In_opt_ HSTRING string,
	_Out_ BOOL* hasEmbedNull
) {
	return P::SupportsWRL()
		? P::WindowsStringHasEmbeddedNull(string, hasEmbedNull)
		: CO_E_DLLNOTFOUND;
}

STDAPI_(BOOL)
RoOriginateErrorW(
	_In_ HRESULT error,
	_In_ UINT cchMax,
	_When_(cchMax == 0, _In_reads_or_z_opt_(MAX_ERROR_MESSAGE_CHARS))
	_When_(cchMax > 0 && cchMax < MAX_ERROR_MESSAGE_CHARS, _In_reads_or_z_(cchMax))
	_When_(cchMax >= MAX_ERROR_MESSAGE_CHARS, _In_reads_or_z_(MAX_ERROR_MESSAGE_CHARS)) PCWSTR message
) {
	return P::SupportsWRL()
		&& P::RoOriginateErrorW(error, cchMax, message);
}

STDAPI_(BOOL)
RoOriginateError(
	_In_ HRESULT error,
	_In_opt_ HSTRING message
) {
	return P::SupportsWRL() && P::RoOriginateError(error, message);
}

} // extern "C"
