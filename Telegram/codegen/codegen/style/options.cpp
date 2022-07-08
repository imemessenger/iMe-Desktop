// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "codegen/style/options.h"

#include <ostream>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include "codegen/common/logging.h"

namespace codegen {
namespace style {
namespace {

constexpr int kErrorIncludePathExpected     = 901;
constexpr int kErrorOutputPathExpected      = 902;
constexpr int kErrorInputPathExpected       = 903;
constexpr int kErrorWorkingPathExpected     = 905;

} // namespace

using common::logError;

Options parseOptions() {
	Options result;
	auto args = QCoreApplication::instance()->arguments();
	for (int i = 1, count = args.size(); i < count; ++i) { // skip first
		auto &arg = args.at(i);

		// Include paths
		if (arg == "-I") {
			if (++i == count) {
				logError(kErrorIncludePathExpected, "Command Line") << "include path expected after -I";
				return Options();
			} else {
				result.includePaths.push_back(args.at(i));
			}
		} else if (arg.startsWith("-I")) {
			result.includePaths.push_back(arg.mid(2));

		// Output path
		} else if (arg == "-o") {
			if (++i == count) {
				logError(kErrorOutputPathExpected, "Command Line") << "output path expected after -o";
				return Options();
			} else {
				result.outputPath = args.at(i);
			}
		} else if (arg.startsWith("-o")) {
			result.outputPath = arg.mid(2);

		// Timestamp path
		} else if (arg == "-t") {
			if (++i == count) {
				logError(kErrorOutputPathExpected, "Command Line") << "timestamp path expected after -t";
				return Options();
			} else {
				result.timestampPath = args.at(i);
			}
		} else if (arg.startsWith("-t")) {
			result.timestampPath = arg.mid(2);

		// Working path
		} else if (arg == "-w") {
			if (++i == count) {
				logError(kErrorWorkingPathExpected, "Command Line") << "working path expected after -w";
				return Options();
			} else {
				common::logSetWorkingPath(args.at(i));
			}
		} else if (arg.startsWith("-w")) {
			common::logSetWorkingPath(arg.mid(2));

		// Input path
		} else {
			result.inputPaths.push_back(arg);
		}
	}
	if (result.timestampPath.isEmpty()) {
		logError(kErrorInputPathExpected, "Command Line") << "timestamp path expected";
		return Options();
	}
	if (result.inputPaths.isEmpty()) {
		logError(kErrorInputPathExpected, "Command Line") << "input path expected";
		return Options();
	}
	result.isPalette = (result.inputPaths.size() == 1)
		&& (QFileInfo(result.inputPaths.front()).suffix() == "palette");
	return result;
}

} // namespace style
} // namespace codegen
