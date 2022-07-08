// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QTextStream>

namespace codegen {
namespace common {

struct ProjectInfo {
	QString name;
	QString source;
	bool forceReGenerate;
};

// Creates a file with license header and codegen warning.
class CppFile {
public:
	// If "basepath" is empty the folder containing "path" will be chosen.
	// File ending with .cpp will be treated as source, otherwise like header.
	CppFile(const QString &path, const ProjectInfo &project);

	QTextStream &stream() {
		return stream_;
	}

	CppFile &newline() {
		stream() << "\n";
		return *this;
	}
	CppFile &include(const QString &header);
	CppFile &includeFromLibrary(const QString &header);

	// Empty name adds anonymous namespace.
	CppFile &pushNamespace(const QString &name = QString());
	CppFile &popNamespace();

	bool finalize();

private:
	QString filepath_;
	QByteArray content_;
	QTextStream stream_;
	QVector<QString> namespaces_;
	bool forceReGenerate_;

};

bool TouchTimestamp(const QString &basepath);

} // namespace common
} // namespace codegen
