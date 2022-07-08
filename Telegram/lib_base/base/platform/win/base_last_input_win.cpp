// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/win/base_last_input_win.h"

#include "base/platform/win/base_windows_h.h"
#include "base/call_delayed.h"
#include "base/integration.h"

namespace base::Platform {
namespace {

constexpr auto kRefreshBadLastUserInputTimeout = 10 * crl::time(1000);

} // namespace

std::optional<crl::time> LastUserInputTime() {
	auto lii = LASTINPUTINFO{ 0 };
	lii.cbSize = sizeof(LASTINPUTINFO);
	if (!GetLastInputInfo(&lii)) {
		return std::nullopt;
	}
	const auto now = crl::now();
	const auto input = crl::time(lii.dwTime);
	static auto LastTrackedInput = input;
	static auto LastTrackedWhen = now;

	const auto ticks32 = crl::time(GetTickCount());
	const auto ticks64 = crl::time(GetTickCount64());
	const auto elapsed = std::max(ticks32, ticks64) - input;
	const auto good = (std::abs(ticks32 - ticks64) <= crl::time(1000))
		&& (elapsed >= 0);
	if (good) {
		LastTrackedInput = input;
		LastTrackedWhen = now;
		return (now > elapsed) ? (now - elapsed) : crl::time(0);
	}

	static auto WaitingDelayed = false;
	if (!WaitingDelayed) {
		WaitingDelayed = true;
		base::call_delayed(kRefreshBadLastUserInputTimeout, [=] {
			WaitingDelayed = false;
			[[maybe_unused]] const auto cheked = LastUserInputTime();
		});
	}
	constexpr auto OverrunLimit = std::numeric_limits<DWORD>::max();
	constexpr auto OverrunThreshold = OverrunLimit / 4;
	if (LastTrackedInput == input) {
		return LastTrackedWhen;
	}
	const auto guard = gsl::finally([&] {
		LastTrackedInput = input;
		LastTrackedWhen = now;
	});
	if (input > LastTrackedInput) {
		const auto add = input - LastTrackedInput;
		return std::min(LastTrackedWhen + add, now);
	} else if (crl::time(OverrunLimit) + input - LastTrackedInput
		< crl::time(OverrunThreshold)) {
		const auto add = crl::time(OverrunLimit) + input - LastTrackedInput;
		return std::min(LastTrackedWhen + add, now);
	}
	return LastTrackedWhen;
}

} // namespace base::Platform
