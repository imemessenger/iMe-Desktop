// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "codegen/emoji/data_read.h"

#include "codegen/emoji/data.h"
#include "codegen/emoji/data_old.h"
#include "base/qt/qt_string_view.h"

#include <QFile>

namespace codegen {
namespace emoji {
namespace {

using Line = std::vector<QString>;
using Part = std::vector<Line>;
using Section = std::vector<Part>;
using File = std::vector<Section>;

[[nodiscard]] QStringView Skip(QStringView data, int endIndex) {
	return (endIndex >= 0) ? base::StringViewMid(data, endIndex + 1) : QStringView();
}

[[nodiscard]] std::pair<QString, QStringView> ReadString(QStringView data) {
	const auto endIndex = data.indexOf(',');
	auto parse = base::StringViewMid(data, 0, endIndex);
	const auto start = parse.indexOf('"');
	const auto end = parse.indexOf('"', start + 1);
	auto result = (start >= 0 && end > start)
		? base::StringViewMid(parse, start + 1, end - start - 1).toString()
		: QString();
	return { std::move(result), Skip(data, endIndex) };
}

[[nodiscard]] std::pair<Line, QStringView> ReadLine(QStringView data) {
	const auto endIndex = data.indexOf('\n');
	auto parse = base::StringViewMid(data, 0, endIndex);
	auto result = Line();
	while (true) {
		auto [string, updated] = ReadString(parse);
		if (!string.isEmpty()) {
			result.push_back(std::move(string));
		}
		if (updated.isEmpty()) {
			break;
		}
		parse = updated;
	}
	return { std::move(result), Skip(data, endIndex) };
}

[[nodiscard]] std::pair<Part, QStringView> ReadPart(QStringView data) {
	const auto endIndex1 = data.indexOf(u"\n\n");
	const auto endIndex2 = data.indexOf(u"\r\n\r\n");
	const auto endIndex = (endIndex1 >= 0) ? endIndex1 : endIndex2;
	auto parse = base::StringViewMid(data, 0, endIndex);
	auto result = Part();
	while (true) {
		auto [line, updated] = ReadLine(parse);
		if (!line.empty()) {
			result.push_back(std::move(line));
		}
		if (updated.isEmpty()) {
			break;
		}
		parse = updated;
	}
	return { std::move(result), Skip(data, endIndex) };
}

[[nodiscard]] std::pair<Section, QStringView> ReadSection(QStringView data) {
	const auto endIndex1 = data.indexOf(u"--------");
	const auto endIndex2 = data.indexOf(u"========");
	const auto endIndex = (endIndex1 >= 0 && endIndex2 >= 0)
		? std::min(endIndex1, endIndex2)
		: std::max(endIndex1, endIndex2);
	auto parse = base::StringViewMid(data, 0, endIndex);
	auto result = Section();
	while (true) {
		auto [part, updated] = ReadPart(parse);
		if (!part.empty()) {
			result.push_back(std::move(part));
		}
		if (updated.isEmpty()) {
			break;
		}
		parse = updated;
	}
	return { std::move(result), Skip(data, endIndex) };
}

[[nodiscard]] File ReadFile(const QString &path) {
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return File();
	}
	const auto bytes = QString::fromUtf8(file.readAll());
	file.close();

	auto parse = QStringView(bytes);
	auto result = File();
	while (true) {
		auto [section, updated] = ReadSection(parse);
		if (!section.empty()) {
			result.push_back(std::move(section));
		}
		if (updated.isEmpty()) {
			break;
		}
		parse = updated;
	}
	return result;
}

[[nodiscard]] const Line &FindColoredLine(const File &file, const QString &colored) {
	const auto &withColored = file[0];
	for (const auto &withColoredPart : withColored) {
		for (const auto &withColoredLine : withColoredPart) {
			if (withColoredLine[0] == colored) {
				return withColoredLine;
			}
		}
	}
	logDataError() << "Simple colored emoji not found: " << colored.toStdString();
	static auto result = Line();
	return result;
}

[[nodiscard]] QString FindFirstColored(const File &file, const QString &colored) {
	const auto &withColoredLine = FindColoredLine(file, colored);
	if (withColoredLine.empty()) {
		return {};
	} else if (withColoredLine.size() != 6) {
		logDataError() << "Wrong simple colored emoji: " << colored.toStdString();
		return {};
	}
	return withColoredLine[1];
}

struct DoubleColoredSample {
	QString original;
	QString same;
	QString different;
};
[[nodiscard]] DoubleColoredSample FindDoubleColored(const File &file, const QString &colored) {
	const auto &withColoredLine = FindColoredLine(file, colored);
	if (withColoredLine.empty()) {
		return {};
	} else if (withColoredLine.size() != 26) {
		logDataError() << "Wrong double colored emoji: " << colored.toStdString();
		return {};
	}
	return { withColoredLine[0], withColoredLine[1], withColoredLine[2] };
}

} // namespace

InputId InputIdFromString(const QString &emoji) {
	if (emoji.isEmpty()) {
		return InputId();
	}
	auto result = InputId();
	for (auto i = 0, size = int(emoji.size()); i != size; ++i) {
		result.push_back(uint32(emoji[i].unicode()));
		if (emoji[i].isHighSurrogate()) {
			if (++i == size || !emoji[i].isLowSurrogate()) {
				logDataError()
					<< "Bad surrogate pair in InputIdFromString: "
					<< emoji.toStdString();
				return InputId();
			}
			result.back() = (result.back() << 16) | uint32(emoji[i].unicode());
		}
	}
	return result;
}

QString InputIdToString(const InputId &id) {
	auto result = QString();
	for (auto i = 0, size = int(id.size()); i != size; ++i) {
		if (id[i] > 0xFFFFU) {
			result.push_back(QChar(quint16(id[i] >> 16)));
		}
		result.push_back(QChar(quint16(id[i] & 0xFFFFU)));
	}
	return result;
}

InputData ReadData(const QString &path) {
	const auto file = ReadFile(path);
	if (file.size() < 3
		|| file[0].size() != 8
		|| file[1].size() > 8) {
		logDataError() << "Wrong file parts.";
		return InputData();
	}
	auto result = InputData();
	const auto &colored = file[2][0];
	for (const auto &coloredLine : colored) {
		for (const auto &coloredString : coloredLine) {
			const auto withFirstColor = FindFirstColored(file, coloredString);
			const auto inputId = InputIdFromString(withFirstColor);
			if (inputId.empty()) {
				return InputData();
			} else if (inputId.size() < 2) {
				logDataError() << "Bad colored emoji: " << withFirstColor.toStdString();
				return InputData();
			}
			result.colored.push_back(inputId);
		}
	}
	if (file[2].size() > 1) {
		const auto &doubleColored = file[2][1];
		for (const auto &doubleColoredLine : doubleColored) {
			for (const auto &doubleColoredString : doubleColoredLine) {
				const auto [original, same, different] = FindDoubleColored(file, doubleColoredString);
				const auto originalId = InputIdFromString(original);
				const auto sameId = InputIdFromString(same);
				const auto differentId = InputIdFromString(different);
				if (originalId.empty() || sameId.empty() || differentId.empty()) {
					return InputData();
				} else if (originalId.size() < 1 || sameId.size() < 2 || differentId.size() < 7) {
					logDataError()
						<< "Bad double colored emoji: "
						<< original.toStdString()
						<< ", " << different.toStdString();
					return InputData();
				}
				result.doubleColored.push_back({ originalId, sameId, differentId });
			}
		}
	}
	auto index = 0;
	auto replacementsUsed = 0;
	for (const auto &section : file[0]) {
		const auto first = section.front().front();
		const auto replacedSection = [&]() -> const Part* {
			for (const auto &section : file[1]) {
				if (section.front().front() == first) {
					++replacementsUsed;
					return &section;
				}
			}
			return nullptr;
		}();
		const auto &sectionData = replacedSection
			? *replacedSection
			: section;
		for (const auto &line : sectionData) {
			for (const auto &string : line) {
				const auto inputId = InputIdFromString(string);
				if (inputId.empty()) {
					return InputData();
				}
				result.categories[index].push_back(inputId);
			}
		}
		if (index + 1 < std::size(result.categories)) {
			++index;
		}
	}
	if (replacementsUsed != file[1].size()) {
		logDataError() << "Could not use some non-colored section replacements!";
		return InputData();
	}
	if (file.size() > 3) {
		for (const auto &section : file[3]) {
			for (const auto &line : section) {
				for (const auto &string : line) {
					const auto inputId = InputIdFromString(string);
					if (inputId.empty()) {
						return InputData();
					}
					result.other.push_back(inputId);
				}
			}
		}
	}
	return result;
}

} // namespace emoji
} // namespace codegen
