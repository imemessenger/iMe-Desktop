// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

namespace base {

class NetworkReachability final {
public:
	NetworkReachability();
	~NetworkReachability();

	[[nodiscard]] static std::shared_ptr<NetworkReachability> Instance();

	[[nodiscard]] bool available() const;
	[[nodiscard]] rpl::producer<bool> availableChanges() const;
	[[nodiscard]] rpl::producer<bool> availableValue() const;

private:
	struct Private;
	const std::unique_ptr<Private> _private;
};

} // namespace base
