// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/base_platform_network_reachability.h"

#include "base/platform/win/base_windows_h.h"
#include "base/debug_log.h"

#include <wrl/client.h>
#include <netlistmgr.h>
#include <comdef.h>

using namespace Microsoft::WRL;

namespace base::Platform {
namespace {

std::optional<bool> Available;

QString ErrorStringFromHResult(HRESULT hr) {
	_com_error error(hr);
	return QString::fromWCharArray(error.ErrorMessage());
}

template<typename T>
bool QueryInterfaceImpl(IUnknown *from, REFIID riid, void **ppvObject) {
	if (riid == __uuidof(T)) {
		*ppvObject = static_cast<T *>(from);
		from->AddRef();
		return true;
	}
	return false;
}

class NetworkListManagerEvents : public INetworkListManagerEvents {
public:
	NetworkListManagerEvents() = default;
	virtual ~NetworkListManagerEvents() = default;

	void start();
	void finish();

	HRESULT STDMETHODCALLTYPE QueryInterface(
		REFIID riid,
		void **ppvObject) override;

	ULONG STDMETHODCALLTYPE AddRef() override { return ++_ref; }
	ULONG STDMETHODCALLTYPE Release() override {
		if (--_ref == 0) {
			delete this;
			return 0;
		}
		return _ref;
	}

	HRESULT STDMETHODCALLTYPE ConnectivityChanged(
		NLM_CONNECTIVITY newConnectivity) override;

private:
	ComPtr<INetworkListManager> _networkListManager = nullptr;
	ComPtr<IConnectionPoint> _connectionPoint = nullptr;

	std::atomic<ULONG> _ref = 0;
	DWORD _cookie = 0;

};

void NetworkListManagerEvents::start() {
	auto hr = CoCreateInstance(
		CLSID_NetworkListManager,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_INetworkListManager,
		&_networkListManager);

	if (FAILED(hr)) {
		LOG(("NetworkListManagerEvents: "
				"Could not get a NetworkListManager instance: %1").arg(
			ErrorStringFromHResult(hr)));
		return;
	}

	ComPtr<IConnectionPointContainer> connectionPointContainer;
	hr = _networkListManager.As(&connectionPointContainer);
	if (SUCCEEDED(hr)) {
		hr = connectionPointContainer->FindConnectionPoint(
			IID_INetworkListManagerEvents,
			&_connectionPoint);
	}
	if (FAILED(hr)) {
		LOG(("NetworkListManagerEvents: Failed to get connection point for "
				"network list manager events: %1").arg(
			ErrorStringFromHResult(hr)));
		return;
	}

	hr = _connectionPoint->Advise(this, &_cookie);
	if (FAILED(hr)) {
		LOG(("NetworkListManagerEvents: "
			"Failed to subscribe to network connectivity events: %1").arg(
			ErrorStringFromHResult(hr)));
		return;
	}

	// Update connectivity since it might have
	// changed since this class was constructed
	NLM_CONNECTIVITY connectivity;
	hr = _networkListManager->GetConnectivity(&connectivity);
	if (FAILED(hr)) {
		LOG(("NetworkListManagerEvents: Could not get connectivity: %1").arg(
			ErrorStringFromHResult(hr)));
		return;
	}
	Available = connectivity != NLM_CONNECTIVITY_DISCONNECTED;
}

void NetworkListManagerEvents::finish() {
	if (_connectionPoint) {
		auto hr = _connectionPoint->Unadvise(_cookie);
		if (FAILED(hr)) {
			LOG(("NetworkListManagerEvents: "
					"Failed to unsubscribe from "
					"network connectivity events: %1").arg(
				ErrorStringFromHResult(hr)));
		} else {
			_cookie = 0;
		}
	}
}

HRESULT STDMETHODCALLTYPE NetworkListManagerEvents::QueryInterface(
		REFIID riid,
		void **ppvObject) {
	if (!ppvObject) {
		return E_INVALIDARG;
	}

	return QueryInterfaceImpl<IUnknown>(this, riid, ppvObject)
			|| QueryInterfaceImpl<INetworkListManagerEvents>(
				this,
				riid,
				ppvObject)
		? S_OK
		: E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE NetworkListManagerEvents::ConnectivityChanged(
		NLM_CONNECTIVITY newConnectivity) {
	// This function is run on a different thread than 'monitor'
	// is created on, so we need to run it on that thread
	Available = newConnectivity != NLM_CONNECTIVITY_DISCONNECTED;
	NotifyNetworkAvailableChanged();
	return S_OK;
}

class NetworkListManagerEventsInitializer {
public:
	NetworkListManagerEventsInitializer() {
		const auto hr = CoInitialize(nullptr);
		if (FAILED(hr)) {
			LOG(("NetworkListManagerEventsInitializer: "
					"Failed to initialize COM: %1").arg(
				ErrorStringFromHResult(hr)));
			_comInitFailed = true;
			return;
		}

		_managerEvents = new NetworkListManagerEvents();
		_managerEvents->start();
	}

	~NetworkListManagerEventsInitializer() {
		if (!_comInitFailed) {
			_managerEvents->finish();
			_managerEvents = nullptr;
			CoUninitialize();
		}
	}

private:
	ComPtr<NetworkListManagerEvents> _managerEvents = nullptr;
	bool _comInitFailed = false;

};

} // namespace

std::optional<bool> NetworkAvailable() {
	static const NetworkListManagerEventsInitializer Initializer;
	return Available;
}

} // namespace base::Platform
