// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include <QtGui/QGuiApplication>

#include "codegen/emoji/options.h"
#include "codegen/emoji/generator.h"

int main(int argc, char *argv[]) {
	const auto generateImages = [&] {
		for (auto i = 0; i != argc; ++i) {
			if (argv[i] == std::string("--images")) {
				return true;
			}
		}
		return false;
	}();
	const auto app = generateImages
		? std::make_unique<QGuiApplication>(argc, argv)
		: std::make_unique<QCoreApplication>(argc, argv);
	const auto options = codegen::emoji::parseOptions();
	codegen::emoji::Generator generator(options);
	return generator.generate();
}
