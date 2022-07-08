// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <memory>
#include <QtCore/QString>
#include "codegen/style/options.h"

namespace codegen {
namespace style {
namespace structure {
class Module;
} // namespace structure
class ParsedFile;

// Walks through a file, parses it and parses dependency files if necessary.
// Uses Generator class to produce the final output.
class Processor {
public:
	explicit Processor(const Options &options);
	Processor(const Processor &other) = delete;
	Processor &operator=(const Processor &other) = delete;

	// Returns 0 on success.
	int launch();

	~Processor();

private:
	bool write(const structure::Module &module) const;

	const Options &options_;

};

} // namespace style
} // namespace codegen
