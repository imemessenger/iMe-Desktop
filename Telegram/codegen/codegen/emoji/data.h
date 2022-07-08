// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "codegen/common/logging.h"
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <functional>
#include <QtCore/QString>
#include <QtCore/QSet>
#include <QtCore/QMap>

namespace codegen {
namespace emoji {

using Id = QString;
struct Emoji {
	Id id;
	bool postfixed = false;
	bool variated = false;
	//bool doubleVariated = false;
	bool colored = false;
};

struct Data {
	std::vector<Emoji> list;
	std::map<Id, int, std::greater<Id>> map;
	std::set<int> postfixRequired;
	std::vector<std::vector<int>> categories;
	std::map<QString, int, std::greater<QString>> replaces;
};
[[nodiscard]] Data PrepareData(
	const QString &dataPath,
	const std::vector<QString> &oldDataPaths);

constexpr auto kPostfix = 0xFE0FU;
constexpr auto kJoiner = 0x200DU;

[[nodiscard]] common::LogStream logDataError();

} // namespace emoji
} // namespace codegen
