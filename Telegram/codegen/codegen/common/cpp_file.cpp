// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "codegen/common/cpp_file.h"

#include <QtCore/QFileInfo>
#include <QtCore/QDir>

namespace codegen {
namespace common {
namespace {

void writeLicense(QTextStream &stream, const ProjectInfo &project) {
	stream << "\
// WARNING! All changes made in this file will be lost!\n\
// Created from '" << project.source << "' by '" << project.name << "'\n\
//\n\
// This file is part of Desktop App Toolkit,\n\
// a set of libraries for developing nice desktop applications.\n\
//\n\
// For license and copyright information please follow this link:\n\
// https://github.com/desktop-app/legal/blob/master/LEGAL\n\
//\n";
}

} // namespace

CppFile::CppFile(const QString &path, const ProjectInfo &project)
: stream_(&content_)
, forceReGenerate_(project.forceReGenerate) {
	bool cpp = path.endsWith(".cpp", Qt::CaseInsensitive);

	QFileInfo info(path);
	info.dir().mkpath(".");
	filepath_ = info.absoluteFilePath();

	writeLicense(stream_, project);
	if (cpp) {
		include(info.baseName() + ".h").newline();
	} else {
		stream() << "#pragma once";
		newline().newline();
	}
}

CppFile &CppFile::include(const QString &header) {
	stream() << "#include \"" << header << "\"";
	return newline();
}

CppFile &CppFile::includeFromLibrary(const QString &header) {
	stream() << "#include <" << header << ">";
	return newline();
}

CppFile &CppFile::pushNamespace(const QString &name) {
	namespaces_.push_back(name);

	stream() << "namespace";
	if (!name.isEmpty()) {
		stream() << ' ' << name;
	}
	stream() << " {";
	return newline();
}

CppFile &CppFile::popNamespace() {
	if (namespaces_.isEmpty()) {
		return *this;
	}
	auto name = namespaces_.back();
	namespaces_.pop_back();

	stream() << "} // namespace";
	if (!name.isEmpty()) {
		stream() << ' ' << name;
	}
	return newline();
}

bool CppFile::finalize() {
	while (!namespaces_.isEmpty()) {
		popNamespace();
	}
	stream_.flush();

	QFile file(filepath_);
	if (!forceReGenerate_ && file.open(QIODevice::ReadOnly)) {
		if (file.readAll() == content_) {
			file.close();
			return true;
		}
		file.close();
	}

	if (!file.open(QIODevice::WriteOnly)) {
		return false;
	}
	if (file.write(content_) != content_.size()) {
		return false;
	}
	return true;
}

bool TouchTimestamp(const QString &basepath) {
	auto file = QFile(basepath + ".timestamp");
	return file.open(QIODevice::WriteOnly) && (file.write("1", 1) == 1);
}

} // namespace common
} // namespace codegen
