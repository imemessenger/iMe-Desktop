// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include <QtCore/QCoreApplication>

#include "codegen/lang/options.h"
#include "codegen/lang/processor.h"

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);

	auto options = codegen::lang::parseOptions();
	if (options.inputPath.isEmpty()) {
		return -1;
	}

	codegen::lang::Processor processor(options);
	return processor.launch();
}
