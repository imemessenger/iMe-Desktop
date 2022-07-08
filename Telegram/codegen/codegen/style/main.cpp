// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include <QtCore/QCoreApplication>

#include "codegen/style/options.h"
#include "codegen/style/processor.h"

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);

	auto options = codegen::style::parseOptions();
	if (options.inputPaths.isEmpty()) {
		return -1;
	}

	codegen::style::Processor processor(options);
	return processor.launch();
}
