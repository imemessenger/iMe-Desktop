/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/platform/win/base_windows_wrl.h"

#pragma warning(push)
// class has virtual functions, but destructor is not virtual
#pragma warning(disable:4265)
#pragma warning(disable:5104)
#include <wrl/module.h>
#pragma warning(pop)
