// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "codegen/emoji/options.h"

#include <ostream>
#include <QtCore/QCoreApplication>
#include "codegen/common/logging.h"

namespace codegen {
namespace emoji {
namespace {

constexpr int kErrorOutputPathExpected      = 902;
constexpr int kErrorReplacesPathExpected    = 903;

} // namespace

using common::logError;

Options parseOptions() {
	Options result;
	auto args = QCoreApplication::instance()->arguments();
	for (int i = 1, count = args.size(); i < count; ++i) { // skip first
		auto &arg = args.at(i);

		// Output path
		if (arg == "-o") {
			if (++i == count) {
				logError(kErrorOutputPathExpected, "Command Line") << "output path expected after -o";
				return Options();
			} else {
				result.outputPath = args.at(i);
			}
		} else if (arg.startsWith("-o")) {
			result.outputPath = arg.mid(2);
		} else if (arg == "--images") {
			if (++i == count) {
				logError(kErrorOutputPathExpected, "Command Line") << "images argument expected after --images";
				return Options();
			} else {
				result.writeImages = args.at(i);
			}
		} else if (result.dataPath.isEmpty()) {
			result.dataPath = arg;
		} else if (result.replacesPath.isEmpty()) {
			result.replacesPath = arg;
		} else {
			result.oldDataPaths.push_back(arg);
		}
	}
	if (result.outputPath.isEmpty()) {
		logError(kErrorOutputPathExpected, "Command Line") << "output path expected";
		return Options();
	} else if (result.replacesPath.isEmpty()) {
		logError(kErrorReplacesPathExpected, "Command Line") << "replaces path expected";
		return Options();
	}
	return result;
}

} // namespace emoji
} // namespace codegen
