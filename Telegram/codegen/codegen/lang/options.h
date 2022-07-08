// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>

namespace codegen {
namespace lang {

struct Options {
	QString outputPath = ".";
	QString inputPath;
};

// Parsing failed if inputPath is empty in the result.
Options parseOptions();

} // namespace lang
} // namespace codegen
