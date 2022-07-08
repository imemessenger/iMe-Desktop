// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "codegen/style/processor.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include "codegen/common/cpp_file.h"
#include "codegen/style/parsed_file.h"
#include "codegen/style/generator.h"

namespace codegen {
namespace style {
namespace {

constexpr int kErrorCantWritePath = 821;

QString destFileBaseName(const structure::Module &module) {
	return "style_" + QFileInfo(module.filepath()).baseName();
}

} // namespace

Processor::Processor(const Options &options)
: options_(options) {
}

int Processor::launch() {
	auto cache = std::map<QString, std::shared_ptr<const structure::Module>>();
	for (auto i = 0; i != options_.inputPaths.size(); ++i) {
		auto parser = ParsedFile(cache, options_, i);
		if (!parser.read()) {
			return -1;
		}

		const auto module = parser.getResult();
		if (!write(*module)) {
			return -1;
		}
	}
	if (!common::TouchTimestamp(options_.timestampPath)) {
		return -1;
	}
	return 0;
}

bool Processor::write(const structure::Module &module) const {
	bool forceReGenerate = false;
	QDir dir(options_.outputPath);
	if (!dir.mkpath(".")) {
		common::logError(kErrorCantWritePath, "Command Line") << "can not open path for writing: " << dir.absolutePath().toStdString();
		return false;
	}

	QFileInfo srcFile(module.filepath());
	QString dstFilePath = dir.absolutePath() + '/' + (options_.isPalette ? "palette" : destFileBaseName(module));

	common::ProjectInfo project = {
		"codegen_style",
		srcFile.fileName(),
		forceReGenerate
	};

	Generator generator(module, dstFilePath, project, options_.isPalette);
	if (!generator.writeHeader() || !generator.writeSource()) {
		return false;
	}
	return true;
}

Processor::~Processor() = default;

} // namespace style
} // namespace codegen
