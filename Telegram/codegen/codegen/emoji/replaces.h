// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "codegen/common/logging.h"
#include "codegen/emoji/data.h"
#include <QtCore/QVector>

namespace codegen {
namespace emoji {

struct Replace {
	Id id;
	QString replacement;
	QVector<QString> words;
};

struct Replaces {
	Replaces(const QString &filename) : filename(filename) {
	}
	QString filename;
	QVector<Replace> list;
};

Replaces PrepareReplaces(const QString &filename);
bool CheckAndConvertReplaces(Replaces &replaces, const Data &data);

} // namespace emoji
} // namespace codegen
