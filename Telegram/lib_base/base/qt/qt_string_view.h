// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <QtCore/QString>

namespace base {
namespace details {

struct ContainerImplHelper {
	enum CutResult { Null, Empty, Full, Subset };
	static constexpr CutResult mid(
			qsizetype originalLength,
			qsizetype *_position,
			qsizetype *_length) {
		qsizetype &position = *_position;
		qsizetype &length = *_length;
		if (position > originalLength) {
			position = 0;
			length = 0;
			return Null;
		}

		if (position < 0) {
			if (length < 0 || length + position >= originalLength) {
				position = 0;
				length = originalLength;
				return Full;
			}
			if (length + position <= 0) {
				position = length = 0;
				return Null;
			}
			length += position;
			position = 0;
		} else if (size_t(length) > size_t(originalLength - position)) {
			length = originalLength - position;
		}

		if (position == 0 && length == originalLength)
			return Full;

		return length > 0 ? Subset : Empty;
	}
};

} // namespace details

[[nodiscard]] inline QStringView StringViewMid(
		QStringView view,
		qsizetype pos,
		qsizetype n = -1) {
	const auto result = details::ContainerImplHelper::mid(
		view.size(),
		&pos,
		&n);
	return (result == details::ContainerImplHelper::Null)
		? QStringView()
		: view.mid(pos, n);
}

} // namespace base
