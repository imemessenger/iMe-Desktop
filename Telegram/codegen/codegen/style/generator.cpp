// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "codegen/style/generator.h"

#include "base/crc32hash.h"

#include <set>
#include <memory>
#include <functional>
#include <QtCore/QDir>
#include <QtCore/QSet>
#include <QtCore/QBuffer>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include "codegen/style/parsed_file.h"

using Module = codegen::style::structure::Module;
using Struct = codegen::style::structure::Struct;
using Variable = codegen::style::structure::Variable;
using Tag = codegen::style::structure::TypeTag;

namespace codegen {
namespace style {
namespace {

constexpr int kErrorBadIconSize     = 861;

const auto kMustBeContrast = std::map<QString, QString>{
	{ "dialogsMenuIconFg", "dialogsBg" },
	{ "windowBoldFg", "windowBg" },
};

char hexChar(uchar ch) {
	if (ch < 10) {
		return '0' + ch;
	} else if (ch < 16) {
		return 'a' + (ch - 10);
	}
	return '0';
}

char hexSecondChar(char ch) {
	return hexChar((*reinterpret_cast<uchar*>(&ch)) & 0x0F);
}

char hexFirstChar(char ch) {
	return hexChar((*reinterpret_cast<uchar*>(&ch)) >> 4);
}

QString stringToEncodedString(const QString &str) {
	QString result, lineBreak = "\\\n";
	result.reserve(str.size() * 8);
	bool writingHexEscapedCharacters = false, startOnNewLine = false;
	int lastCutSize = 0;
	auto utf = str.toUtf8();
	for (auto ch : utf) {
		if (result.size() - lastCutSize > 80) {
			startOnNewLine = true;
			result.append(lineBreak);
			lastCutSize = result.size();
		}
		if (ch == '\n') {
			writingHexEscapedCharacters = false;
			result.append("\\n");
		} else if (ch == '\t') {
			writingHexEscapedCharacters = false;
			result.append("\\t");
		} else if (ch == '"' || ch == '\\') {
			writingHexEscapedCharacters = false;
			result.append('\\').append(ch);
		} else if (ch < 32 || static_cast<uchar>(ch) > 127) {
			writingHexEscapedCharacters = true;
			result.append("\\x").append(hexFirstChar(ch)).append(hexSecondChar(ch));
		} else {
			if (writingHexEscapedCharacters) {
				writingHexEscapedCharacters = false;
				result.append("\"\"");
			}
			result.append(ch);
		}
	}
	return '"' + (startOnNewLine ? lineBreak : QString()) + result + '"';
}

QString stringToEncodedString(const std::string &str) {
	return stringToEncodedString(QString::fromStdString(str));
}

QString stringToBinaryArray(const std::string &str) {
	QStringList rows, chars;
	chars.reserve(13);
	rows.reserve(1 + (str.size() / 13));
	for (uchar ch : str) {
		if (chars.size() > 12) {
			rows.push_back(chars.join(", "));
			chars.clear();
		}
		chars.push_back(QString("0x") + hexFirstChar(ch) + hexSecondChar(ch));
	}
	if (!chars.isEmpty()) {
		rows.push_back(chars.join(", "));
	}
	return QString("{") + ((rows.size() > 1) ? '\n' : ' ') + rows.join(",\n") + " }";
}

QString pxValueName(int value) {
	QString result = "px";
	if (value < 0) {
		value = -value;
		result += 'm';
	}
	return result + QString::number(value);
}

QString moduleBaseName(const structure::Module &module) {
	auto moduleInfo = QFileInfo(module.filepath());
	auto moduleIsPalette = (moduleInfo.suffix() == "palette");
	return moduleIsPalette ? "palette" : "style_" + moduleInfo.baseName();
}

QString colorFallbackName(structure::Value value) {
	auto copy = value.copyOf();
	if (!copy.isEmpty()) {
		return copy.back();
	}
	return value.Color().fallback;
}

QChar paletteColorPart(uchar part) {
	part = (part & 0x0F);
	if (part >= 10) {
		return 'a' + (part - 10);
	}
	return '0' + part;
}

QString paletteColorComponent(uchar value) {
	return QString() + paletteColorPart(value >> 4) + paletteColorPart(value);
}

QString paletteColorValue(const structure::data::color &value) {
	auto result = paletteColorComponent(value.red) + paletteColorComponent(value.green) + paletteColorComponent(value.blue);
	if (value.alpha != 255) result += paletteColorComponent(value.alpha);
	return result;
}

[[nodiscard]] bool IsValueInHeader(structure::Type type) {
	switch (type.tag) {
	case Tag::Int:
	case Tag::Double: return true;
	default: return false;
	}
}

} // namespace

Generator::Generator(const structure::Module &module, const QString &destBasePath, const common::ProjectInfo &project, bool isPalette)
: module_(module)
, basePath_(destBasePath)
, baseName_(QFileInfo(basePath_).baseName())
, project_(project)
, isPalette_(isPalette) {
}

bool Generator::writeHeader() {
	header_ = std::make_unique<common::CppFile>(basePath_ + ".h", project_);

	header_->include("ui/style/style_core.h").newline();

	if (!writeHeaderRequiredIncludes()) {
		return false;
	}
	if (!writeHeaderStyleNamespace()) {
		return false;
	}
	if (!writeRefsDeclarations()) {
		return false;
	}

	return header_->finalize();
}

bool Generator::writeSource() {
	source_ = std::make_unique<common::CppFile>(basePath_ + ".cpp", project_);

	writeIncludesInSource();

	if (module_.hasVariables()) {
		source_->pushNamespace().newline();
		source_->stream() << "\
bool inited = false;\n\
\n\
class Module_" << baseName_ << " : public style::internal::ModuleBase {\n\
public:\n\
	Module_" << baseName_ << "() { style::internal::registerModule(this); }\n\
\n\
	void start(int scale) override {\n\
		style::internal::init_" << baseName_ << "(scale);\n\
	}\n\
};\n\
Module_" << baseName_ << " registrator;\n";
		if (isPalette_) {
			source_->newline();
			source_->stream() << "style::palette _palette;\n";
		} else {
			if (!writeVariableDefinitions()) {
				return false;
			}
		}
		source_->newline().popNamespace();

		source_->newline().pushNamespace("st");
		if (!writeRefsDefinition()) {
			return false;
		}

		source_->popNamespace().newline().pushNamespace("style");
		if (isPalette_) {
			writeSetPaletteColor();
		}
		source_->pushNamespace("internal").newline();
		if (!writeVariableInit()) {
			return false;
		}
	}

	return source_->finalize();
}

// Empty result means an error.
QString Generator::typeToString(structure::Type type) const {
	switch (type.tag) {
	case Tag::Invalid: return QString();
	case Tag::Int: return "int";
	case Tag::Double: return "double";
	case Tag::Pixels: return "int";
	case Tag::String: return "QString";
	case Tag::Color: return "style::color";
	case Tag::Point: return "style::point";
	case Tag::Size: return "style::size";
	case Tag::Align: return "style::align";
	case Tag::Margins: return "style::margins";
	case Tag::Font: return "style::font";
	case Tag::Icon: return "style::icon";
	case Tag::Struct: return "style::" + type.name.back();
	}
	return QString();
}

// Empty result means an error.
QString Generator::typeToDefaultValue(structure::Type type) const {
	switch (type.tag) {
	case Tag::Invalid: return QString();
	case Tag::Int: return "0";
	case Tag::Double: return "0.";
	case Tag::Pixels: return "0";
	case Tag::String: return "QString()";
	case Tag::Color: return "{ Qt::Uninitialized }";
	case Tag::Point: return "{ 0, 0 }";
	case Tag::Size: return "{ 0, 0 }";
	case Tag::Align: return "style::al_topleft";
	case Tag::Margins: return "{ 0, 0, 0, 0 }";
	case Tag::Font: return "{ Qt::Uninitialized }";
	case Tag::Icon: return "{ Qt::Uninitialized }";
	case Tag::Struct: {
		if (auto realType = module_.findStruct(type.name)) {
			QStringList fields;
			for (const auto &field : realType->fields) {
				fields.push_back(typeToDefaultValue(field.type));
			}
			return "{ " + fields.join(", ") + " }";
		}
		return QString();
	} break;
	}
	return QString();
}

// Empty result means an error.
QString Generator::valueAssignmentCode(
		structure::Value value,
		bool ignoreCopy) const {
	auto copy = value.copyOf();
	if (!ignoreCopy && !copy.isEmpty()) {
		return "st::" + copy.back();
	}

	switch (value.type().tag) {
	case Tag::Invalid: return QString();
	case Tag::Int: return QString("%1").arg(value.Int());
	case Tag::Double: return QString("%1").arg(value.Double());
	case Tag::Pixels: return pxValueName(value.Int());
	case Tag::String: return QString("QString::fromUtf8(%1)").arg(stringToEncodedString(value.String()));
	case Tag::Color: {
		auto v(value.Color());
		if (v.red == v.green && v.red == v.blue && v.red == 0 && v.alpha == 255) {
			return QString("st::windowFg");
		} else if (v.red == v.green && v.red == v.blue && v.red == 255 && v.alpha == 0) {
			return QString("st::transparent");
		} else {
			common::logError(common::kErrorInternal, "") << "bad color value";
			return QString();
		}
	} break;
	case Tag::Point: {
		auto v(value.Point());
		return QString("{ %1, %2 }").arg(pxValueName(v.x), pxValueName(v.y));
	} break;
	case Tag::Size: {
		auto v(value.Size());
		return QString("{ %1, %2 }").arg(pxValueName(v.width), pxValueName(v.height));
	} break;
	case Tag::Align: return QString("style::al_%1").arg(value.String().c_str());
	case Tag::Margins: {
		auto v(value.Margins());
		return QString("{ %1, %2, %3, %4 }").arg(pxValueName(v.left), pxValueName(v.top), pxValueName(v.right), pxValueName(v.bottom));
	} break;
	case Tag::Font: {
		auto v(value.Font());
		QString family = "0";
		if (!v.family.empty()) {
			auto familyIndex = fontFamilies_.value(v.family, -1);
			if (familyIndex < 0) {
				return QString();
			}
			family = QString("font%1index").arg(familyIndex);
		}
		return QString("{ %1, %2, %3 }").arg(pxValueName(v.size)).arg(v.flags).arg(family);
	} break;
	case Tag::Icon: {
		auto v(value.Icon());
		QStringList parts = { "std::in_place" };
		for (const auto &part : v.parts) {
			auto maskIndex = iconMasks_.value(part.filename, -1);
			if (maskIndex < 0) {
				return QString();
			}
			auto color = valueAssignmentCode(part.color);
			auto offset = valueAssignmentCode(part.offset);
			parts.push_back(QString("MonoIcon{ &iconMask%1, %2, %3 }").arg(
				QString::number(maskIndex),
				color,
				offset));
		}
		return QString("{ %1 }").arg(parts.join(", "));
	} break;
	case Tag::Struct: {
		if (!value.Fields()) return QString();

		QStringList fields;
		for (const auto &field : *value.Fields()) {
			fields.push_back(valueAssignmentCode(field.variable.value));
		}
		return "{ " + fields.join(", ") + " }";
	} break;
	}
	return QString();
}

bool Generator::writeHeaderRequiredIncludes() {
	std::function<QString(const Module&, structure::FullName)> findInIncludes = [&](const Module &module, const structure::FullName &name) {
		auto result = QString();
		module.enumIncludes([&](const Module &included) {
			if (Module::findStructInModule(name, included)) {
				result = moduleBaseName(included);
				return false;
			}
			result = findInIncludes(included, name);
			return true;
		});
		return result;
	};

	auto includes = QStringList();
	const auto written = module_.enumStructs([&](const Struct &value) -> bool {
		for (const auto &field : value.fields) {
			if (field.type.tag == structure::TypeTag::Struct) {
				const auto name = field.type.name;
				if (!module_.findStructInModule(name, module_)) {
					const auto base = findInIncludes(module_, name);
					if (base.isEmpty()) {
						return false;
					}
					if (!includes.contains(base)) {
						includes.push_back(base);
					}
				}
			}
		}
		return true;
	});
	if (!written) {
		return false;
	} else if (includes.isEmpty()) {
		return true;
	}
	for (const auto &base : includes) {
		header_->include("styles/" + base + ".h");
	}
	header_->newline();
	return true;
}

bool Generator::writeHeaderStyleNamespace() {
	if (!module_.hasStructs() && !module_.hasVariables()) {
		return true;
	}
	header_->pushNamespace("style");

	if (module_.hasVariables()) {
		header_->pushNamespace("internal").newline();
		header_->stream() << "void init_" << baseName_ << "(int scale);\n\n";
		header_->popNamespace();
	}
	bool wroteForwardDeclarations = writeStructsForwardDeclarations();
	if (module_.hasStructs()) {
		if (!wroteForwardDeclarations) {
			header_->newline();
		}
		if (!writeStructsDefinitions()) {
			return false;
		}
	} else if (isPalette_) {
		if (!wroteForwardDeclarations) {
			header_->newline();
		}
		if (!writePaletteDefinition()) {
			return false;
		}
	}

	header_->popNamespace().newline();
	return true;
}

bool Generator::writePaletteDefinition() {
	header_->stream() << "\
class palette;\n\
class palette_data {\n\
public:\n\
	static constexpr auto kCount = " << (1 + module_.variablesCount()) << ";\n\
	static int32 Checksum();\n\
\n\
	inline const color &transparent() const { return _colors[0]; }; // special color\n";

	auto indexInPalette = 1;
	if (!module_.enumVariables([&](const Variable &variable) -> bool {
		auto name = variable.name.back();
		if (variable.value.type().tag != structure::TypeTag::Color) {
			return false;
		}

		auto index = (indexInPalette++);
		header_->stream() << "\tinline const color &" << name << "() const { return _colors[" << index << "]; };\n";
		return true;
	})) return false;
	const auto count = indexInPalette;

	header_->stream() << "\
\n\
protected:\n\
	void finalize(palette &that);\n\
\n\
	internal::ColorData *data(int index) {\n\
		return reinterpret_cast<internal::ColorData*>(_data) + index;\n\
	}\n\
\n\
	const internal::ColorData *data(int index) const {\n\
		return reinterpret_cast<const internal::ColorData*>(_data) + index;\n\
	}\n\
\n\
	enum class Status {\n\
		Initial,\n\
		Created,\n\
		Loaded,\n\
	};\n\
\n\
	alignas(alignof(internal::ColorData)) char _data[sizeof(internal::ColorData) * kCount];\n\
\n\
	color _colors[kCount] = {\n";
	for (int i = 0; i != count; ++i) {
		header_->stream() << "\t\tdata(" << i << "),\n";
	}
	header_->stream() << "\
	};\n\
	Status _status[kCount] = { Status::Initial };\n\
	bool _ready = false;\n\
\n\
};\n\
\n\
namespace main_palette {\n\
\n\
not_null<const palette*> get();\n\
\n\
struct row {\n\
	QLatin1String name;\n\
	QLatin1String value;\n\
	QLatin1String fallback;\n\
	QLatin1String description;\n\
};\n\
QList<row> data();\n\
\n\
} // namespace main_palette\n\
\n\
namespace internal {\n\
\n\
int GetPaletteIndex(QLatin1String name);\n\
\n\
} // namespace internal\n";

	return true;
}

bool Generator::writeStructsForwardDeclarations() {
	bool hasNoExternalStructs = module_.enumVariables([&](const Variable &value) -> bool {
		if (value.value.type().tag == structure::TypeTag::Struct) {
			if (!module_.findStructInModule(value.value.type().name, module_)) {
				return false;
			}
		}
		return true;
	});
	if (hasNoExternalStructs) {
		return false;
	}

	header_->newline();
	std::set<QString> alreadyDeclaredTypes;
	bool result = module_.enumVariables([&](const Variable &value) -> bool {
		if (value.value.type().tag == structure::TypeTag::Struct) {
			if (!module_.findStructInModule(value.value.type().name, module_)) {
				if (alreadyDeclaredTypes.find(value.value.type().name.back()) == alreadyDeclaredTypes.end()) {
					header_->stream() << "struct " << value.value.type().name.back() << ";\n";
					alreadyDeclaredTypes.emplace(value.value.type().name.back());
				}
			}
		}
		return true;
	});
	header_->newline();
	return result;
}

bool Generator::writeStructsDefinitions() {
	if (!module_.hasStructs()) {
		return true;
	}

	bool result = module_.enumStructs([&](const Struct &value) -> bool {
		header_->stream() << "\
struct " << value.name.back() << " {\n";
		for (auto &field : value.fields) {
			auto type = typeToString(field.type);
			if (type.isEmpty()) {
				return false;
			}
			header_->stream() << "\t" << type << " " << field.name.back() << ";\n";
		}
		header_->stream() << "\
};\n\n";
		return true;
	});

	return result;
}

bool Generator::writeRefsDeclarations() {
	if (!module_.hasVariables()) {
		return true;
	}

	header_->pushNamespace("st");

	if (isPalette_) {
		header_->stream() << "extern const style::color &transparent; // special color\n";
	}
	bool result = module_.enumVariables([&](const Variable &value) -> bool {
		auto name = value.name.back();
		auto type = typeToString(value.value.type());
		if (type.isEmpty()) {
			return false;
		}

		if (IsValueInHeader(value.value.type())) {
			header_->stream()
				<< "constexpr "
				<< type
				<< " "
				<< name
				<< " = "
				<< valueAssignmentCode(
					value.value,
					IsValueInHeader(value.value.type()))
				<< ";\n";
		} else {
			header_->stream()
				<< "extern const "
				<< type
				<< " &"
				<< name
				<< ";\n";
		}
		return true;
	});

	header_->popNamespace();

	return result;
}

bool Generator::writeIncludesInSource() {
	if (isPalette_) {
		source_->include("ui/style/style_core_palette.h");
		source_->newline();
	}
	if (!module_.hasIncludes()) {
		return true;
	}

	auto includes = QStringList();
	std::function<bool(const Module&)> collector = [&](const Module &module) {
		module.enumIncludes(collector);
		auto base = moduleBaseName(module);
		if (!includes.contains(base)) {
			includes.push_back(base);
		}
		return true;
	};
	auto result = module_.enumIncludes(collector);
	for (const auto &base : includes) {
		source_->include("styles/" + base + ".h");
	}
	source_->newline();
	return result;
}

bool Generator::writeVariableDefinitions() {
	if (!module_.hasVariables()) {
		return true;
	}

	source_->newline();
	bool result = module_.enumVariables([&](const Variable &variable) -> bool {
		auto name = variable.name.back();
		auto type = typeToString(variable.value.type());
		if (type.isEmpty()) {
			return false;
		}
		if (!IsValueInHeader(variable.value.type())) {
			source_->stream()
				<< type
				<< " _"
				<< name
				<< " = "
				<< typeToDefaultValue(variable.value.type())
				<< ";\n";
		}
		return true;
	});
	return result;
}

bool Generator::writeRefsDefinition() {
	if (!module_.hasVariables()) {
		return true;
	}

	if (isPalette_) {
		source_->stream() << "const style::color &transparent(_palette.transparent()); // special color\n";
	}
	bool result = module_.enumVariables([&](const Variable &variable) -> bool {
		auto name = variable.name.back();
		auto type = typeToString(variable.value.type());
		if (type.isEmpty()) {
			return false;
		}
		if (IsValueInHeader(variable.value.type())) {
			return true;
		}
		source_->stream() << "const " << type << " &" << name << "(";
		if (isPalette_) {
			source_->stream() << "_palette." << name << "()";
		} else {
			source_->stream() << "_" << name;
		}
		source_->stream() << ");\n";
		return true;
	});
	return result;
}

bool Generator::writeSetPaletteColor() {
	source_->stream() << "\n\
void palette_data::finalize(palette &that) {\n\
	that.compute(0, -1, { 255, 255, 255, 0}); // special color\n";

	QList<structure::FullName> names;
	module_.enumVariables([&](const Variable &variable) -> bool {
		names.push_back(variable.name);
		return true;
	});

	QString dataRows;
	int indexInPalette = 1;
	QByteArray checksumString;
	checksumString.append("&transparent:{ 255, 255, 255, 0 }");
	auto result = module_.enumVariables([&](const Variable &variable) -> bool {
		auto name = variable.name.back();
		auto index = indexInPalette++;
		paletteIndices_.emplace(name, index);
		if (variable.value.type().tag != structure::TypeTag::Color) {
			return false;
		}
		auto color = variable.value.Color();
		auto fallbackIterator = paletteIndices_.find(colorFallbackName(variable.value));
		auto fallbackIndex = (fallbackIterator == paletteIndices_.end()) ? -1 : fallbackIterator->second;
		auto assignment = QString("{ %1, %2, %3, %4 }").arg(color.red).arg(color.green).arg(color.blue).arg(color.alpha);
		source_->stream() << "\tthat.compute(" << index << ", " << fallbackIndex << ", " << assignment << ");\n";
		checksumString.append(('&' + name + ':' + assignment).toUtf8());

		auto isCopy = !variable.value.copyOf().isEmpty();
		auto colorString = paletteColorValue(color);
		auto fallbackName = QString();
		if (fallbackIndex > 0) {
			auto fallbackVariable = module_.findVariableInModule(names[fallbackIndex - 1], module_);
			if (fallbackVariable && fallbackVariable->value.type().tag == structure::TypeTag::Color) {
				fallbackName = fallbackVariable->name.back();
			}
		}
		auto value = isCopy ? fallbackName : '#' + colorString;
		if (value.isEmpty()) {
			return false;
		}

		dataRows.append("\tresult.push_back({ qstr(\"" + name + "\"), qstr(\"" + value + "\"), qstr(\"" + (isCopy ? QString() : fallbackName) + "\"), qstr(" + stringToEncodedString(variable.description) + ") });\n");
		return true;
	});
	if (!result) {
		return false;
	}
	auto count = indexInPalette;
	auto checksum = base::crc32(checksumString.constData(), checksumString.size());

	source_->stream() << "\n\n";
	for (const auto &[over, under] : kMustBeContrast) {
		const auto overIndex = paletteIndices_.find(over);
		const auto underIndex = paletteIndices_.find(under);
		if (overIndex == paletteIndices_.end() || underIndex == paletteIndices_.end()) {
			return false;
		}
		source_->stream() << "\tinternal::EnsureContrast(*data(" << overIndex->second << "), *data(" << underIndex->second << "));\n";
	}
	source_->stream() << "\
}\n\
\n\
int32 palette_data::Checksum() {\n\
	return " << checksum << ";\n\
}\n";

	source_->newline().pushNamespace("internal").newline();
	source_->stream() << "\
int GetPaletteIndex(QLatin1String name) {\n\
	auto size = name.size();\n\
	auto data = name.data();\n";

	auto tabs = [](int size) {
		return QString(size, '\t');
	};

	enum class UsedCheckType {
		Switch,
		If,
		UpcomingIf,
	};
	auto checkTypes = QVector<UsedCheckType>();
	auto checkLengthHistory = QVector<int>(1, 0);
	auto chars = QString();
	auto tabsUsed = 1;

	// Returns true if at least one check was finished.
	auto finishChecksTillKey = [&](const QString &key) {
		auto result = false;
		while (!chars.isEmpty() && !key.startsWith(chars)) {
			result = true;

			auto wasType = checkTypes.back();
			chars.resize(chars.size() - 1);
			checkTypes.pop_back();
			checkLengthHistory.pop_back();
			if (wasType == UsedCheckType::Switch || wasType == UsedCheckType::If) {
				--tabsUsed;
				if (wasType == UsedCheckType::Switch) {
					source_->stream() << tabs(tabsUsed) << "break;\n";
				}
				if ((!chars.isEmpty() && !key.startsWith(chars)) || key == chars) {
					source_->stream() << tabs(tabsUsed) << "}\n";
				}
			}
		}
		return result;
	};

	// Check if we can use "if" for a check on "charIndex" in "it" (otherwise only "switch")
	auto canUseIfForCheck = [](auto it, auto end, int charIndex) {
		auto key = it->first;
		auto i = it;
		auto keyStart = key.mid(0, charIndex);
		for (++i; i != end; ++i) {
			auto nextKey = i->first;
			if (nextKey.mid(0, charIndex) != keyStart) {
				return true;
			} else if (nextKey.size() > charIndex && nextKey[charIndex] != key[charIndex]) {
				return false;
			}
		}
		return true;
	};

	auto countMinimalLength = [](auto it, auto end, int charIndex) {
		auto key = it->first;
		auto i = it;
		auto keyStart = key.mid(0, charIndex);
		auto result = key.size();
		for (++i; i != end; ++i) {
			auto nextKey = i->first;
			if (nextKey.mid(0, charIndex) != keyStart) {
				break;
			} else if (nextKey.size() > charIndex && result > nextKey.size()) {
				result = nextKey.size();
			}
		}
		return result;
	};

	for (auto i = paletteIndices_.begin(), e = paletteIndices_.end(); i != e; ++i) {
		auto name = i->first;
		auto index = i->second;

		auto weContinueOldSwitch = finishChecksTillKey(name);
		while (chars.size() != name.size()) {
			auto checking = chars.size();

			auto keyChar = name[checking];
			auto usedIfForCheckCount = 0;
			auto minimalLengthCheck = countMinimalLength(i, e, checking);
			for (; checking + usedIfForCheckCount != name.size(); ++usedIfForCheckCount) {
				if (!canUseIfForCheck(i, e, checking + usedIfForCheckCount)
					|| countMinimalLength(i, e, checking + usedIfForCheckCount) != minimalLengthCheck) {
					break;
				}
			}
			auto usedIfForCheck = !weContinueOldSwitch && (usedIfForCheckCount > 0);
			auto checkLengthCondition = QString();
			if (weContinueOldSwitch) {
				weContinueOldSwitch = false;
			} else {
				checkLengthCondition = (minimalLengthCheck > checkLengthHistory.back()) ? ("size >= " + QString::number(minimalLengthCheck)) : QString();
				if (!usedIfForCheck) {
					source_->stream() << tabs(tabsUsed) << (checkLengthCondition.isEmpty() ? QString() : ("if (" + checkLengthCondition + ") ")) << "switch (data[" << checking << "]) {\n";
				}
			}
			if (usedIfForCheck) {
				auto conditions = QStringList();
				if (usedIfForCheckCount > 1) {
					conditions.push_back("!memcmp(data + " + QString::number(checking) + ", \"" + name.mid(checking, usedIfForCheckCount) + "\", " + QString::number(usedIfForCheckCount) + ")");
				} else {
					conditions.push_back("data[" + QString::number(checking) + "] == '" + keyChar + "'");
				}
				if (!checkLengthCondition.isEmpty()) {
					conditions.push_front(checkLengthCondition);
				}
				source_->stream() << tabs(tabsUsed) << "if (" << conditions.join(" && ") << ") {\n";
				checkTypes.push_back(UsedCheckType::If);
				for (auto i = 1; i != usedIfForCheckCount; ++i) {
					checkTypes.push_back(UsedCheckType::UpcomingIf);
					chars.push_back(keyChar);
					checkLengthHistory.push_back(qMax(minimalLengthCheck, checkLengthHistory.back()));
					keyChar = name[checking + i];
				}
			} else {
				source_->stream() << tabs(tabsUsed) << "case '" << keyChar << "':\n";
				checkTypes.push_back(UsedCheckType::Switch);
			}
			++tabsUsed;
			chars.push_back(keyChar);
			checkLengthHistory.push_back(qMax(minimalLengthCheck, checkLengthHistory.back()));
		}
		source_->stream() << tabs(tabsUsed) << "return (size == " << chars.size() << ") ? " << index << " : -1;\n";
	}
	finishChecksTillKey(QString());

	source_->stream() << "\
\n\
	return -1;\n\
}\n";

	source_->newline().popNamespace().newline();
	source_->stream() << "\
namespace main_palette {\n\
\n\
not_null<const palette*> get() {\n\
	return &_palette;\n\
}\n\
\n\
QList<row> data() {\n\
	auto result = QList<row>();\n\
	result.reserve(" << count << ");\n\
\n\
" << dataRows << "\n\
	return result;\n\
}\n\
\n\
} // namespace main_palette\n\
\n";

	return result;
}

bool Generator::writeVariableInit() {
	if (!module_.hasVariables()) {
		return true;
	}

	if (!collectUniqueValues()) {
		return false;
	}
	bool hasUniqueValues = (!pxValues_.isEmpty() || !fontFamilies_.isEmpty() || !iconMasks_.isEmpty());
	if (hasUniqueValues) {
		source_->pushNamespace();
		if (!writePxValuesInit()) {
			return false;
		}
		if (!writeFontFamiliesInit()) {
			return false;
		}
		if (!writeIconValues()) {
			return false;
		}
		source_->popNamespace().newline();
	}

	source_->stream() << "\
void init_" << baseName_ << "(int scale) {\n\
	if (inited) return;\n\
	inited = true;\n\n";

	if (module_.hasIncludes()) {
		bool writtenAtLeastOne = false;
		bool result = module_.enumIncludes([&](const Module &module) -> bool {
			if (module.hasVariables()) {
				source_->stream() << "\tinit_" + moduleBaseName(module) + "(scale);\n";
				writtenAtLeastOne = true;
			}
			return true;
		});
		if (!result) {
			return false;
		}
		if (writtenAtLeastOne) {
			source_->newline();
		}
	}

	if (!pxValues_.isEmpty() || !fontFamilies_.isEmpty()) {
		if (!pxValues_.isEmpty()) {
			source_->stream() << "\tinitPxValues(scale);\n";
		}
		if (!fontFamilies_.isEmpty()) {
			source_->stream() << "\tinitFontFamilies();\n";
		}
		source_->newline();
	}

	if (isPalette_) {
		source_->stream() << "\t_palette.finalize();\n";
	} else if (!module_.enumVariables([&](const Variable &variable) -> bool {
		auto name = variable.name.back();
		auto value = valueAssignmentCode(variable.value);
		if (value.isEmpty()) {
			return false;
		}
		if (!IsValueInHeader(variable.value.type())) {
			source_->stream() << "\t_" << name << " = " << value << ";\n";
		}
		return true;
	})) {
		return false;
	}
	source_->stream() << "\
}\n\n";
	return true;
}

bool Generator::writePxValuesInit() {
	if (pxValues_.isEmpty()) {
		return true;
	}

	for (auto i = pxValues_.cbegin(), e = pxValues_.cend(); i != e; ++i) {
		source_->stream() << "int " << pxValueName(i.key()) << " = " << i.key() << ";\n";
	}
	source_->stream() << "\
void initPxValues(int scale) {\n";
	for (auto it = pxValues_.cbegin(), e = pxValues_.cend(); it != e; ++it) {
		auto value = it.key();
		source_->stream() << "\t" << pxValueName(value) << " = ConvertScale(" << value << ", scale);\n";
	}
	source_->stream() << "\
}\n\n";
	return true;
}

bool Generator::writeFontFamiliesInit() {
	if (fontFamilies_.isEmpty()) {
		return true;
	}

	for (auto familyIndex : std::as_const(fontFamilies_)) {
		source_->stream() << "int font" << familyIndex << "index;\n";
	}
	source_->stream() << "void initFontFamilies() {\n";
	for (auto i = fontFamilies_.cbegin(), e = fontFamilies_.cend(); i != e; ++i) {
		auto family = stringToEncodedString(i.key());
		source_->stream() << "\tfont" << i.value() << "index = style::internal::registerFontFamily(" << family << ");\n";
	}
	source_->stream() << "}\n\n";
	return true;
}

namespace {

QByteArray iconMaskValueSize(int width, int height) {
	QByteArray result;
	QLatin1String generateTag("GENERATE:");
	result.append(generateTag.data(), generateTag.size());
	QLatin1String sizeTag("SIZE:");
	result.append(sizeTag.data(), sizeTag.size());
	{
		QDataStream stream(&result, QIODevice::Append);
		stream.setVersion(QDataStream::Qt_5_1);
		stream << qint32(width) << qint32(height);
	}
	return result;
}

QByteArray iconMaskValuePng(QString filepath) {
	QByteArray result;

	QFileInfo fileInfo(filepath);
	auto directory = fileInfo.dir();
	auto nameAndModifiers = fileInfo.fileName().split('-');
	filepath = directory.filePath(nameAndModifiers[0]);
	auto modifiers = nameAndModifiers.mid(1);

	const auto readImage = [&](const QString &postfix) {
		const auto path = filepath + postfix + ".png";
		auto result = QImage(path);
		if (result.isNull()) {
			common::logError(common::kErrorFileNotOpened, path) << "could not open icon file";
			return QImage();
		} else if (result.format() != QImage::Format_RGB32) {
			result = std::move(result).convertToFormat(QImage::Format_RGB32);
		}
		result.setDevicePixelRatio(1.);
		return result;
	};
	auto png1x = readImage("");
	auto png2x = readImage("@2x");
	auto png3x = readImage("@3x");
	if (png1x.isNull() || png2x.isNull() || png3x.isNull()) {
		return result;
	}
	if (png1x.width() * 2 != png2x.width()
		|| png1x.height() * 2 != png2x.height()
		|| png1x.width() * 3 != png3x.width()
		|| png1x.height() * 3 != png3x.height()) {
		common::logError(kErrorBadIconSize, filepath + ".png")
			<< "bad icons size, 1x: "
			<< png1x.width() << "x" << png1x.height()
			<< ", 2x: "
			<< png2x.width() << "x" << png2x.height()
			<< ", 3x: "
			<< png3x.width() << "x" << png3x.height();
		return result;
	}
	for (const auto &modifierName : modifiers) {
		if (const auto modifier = GetModifier(modifierName)) {
			modifier(png1x);
			modifier(png2x);
			modifier(png3x);
		} else {
			common::logError(common::kErrorInternal, filepath) << "modifier should be valid here, name: " << modifierName.toStdString();
			return result;
		}
	}
	QImage composed(png3x.width(), png3x.height() + png2x.height(), QImage::Format_RGB32);
	composed.fill(Qt::black);
	{
		QPainter p(&composed);
		p.drawImage(0, 0, png1x);
		p.drawImage(png1x.width(), 0, png2x);
		p.drawImage(0, png2x.height(), png3x);
	}
	{
		QBuffer buffer(&result);
		composed.save(&buffer, "PNG");
	}
	return result;
}

} // namespace

bool Generator::writeIconValues() {
	if (iconMasks_.isEmpty()) {
		return true;
	}

	for (auto i = iconMasks_.cbegin(), e = iconMasks_.cend(); i != e; ++i) {
		QString filePath = i.key();
		QByteArray maskData;
		if (filePath.startsWith("size://")) {
			QStringList dimensions = filePath.mid(7).split(',');
			if (dimensions.size() < 2 || dimensions.at(0).toInt() <= 0 || dimensions.at(1).toInt() <= 0) {
				common::logError(common::kErrorFileNotOpened, filePath) << "bad dimensions";
				return false;
			}
			maskData = iconMaskValueSize(dimensions.at(0).toInt(), dimensions.at(1).toInt());
		} else {
			maskData = iconMaskValuePng(filePath);
		}
		if (maskData.isEmpty()) {
			return false;
		}
		source_->stream() << "const uchar iconMask" << i.value() << "Data[] = " << stringToBinaryArray(std::string(maskData.constData(), maskData.size())) << ";\n";
		source_->stream() << "IconMask iconMask" << i.value() << "(iconMask" << i.value() << "Data);\n\n";
	}
	return true;
}

bool Generator::collectUniqueValues() {
	int fontFamilyIndex = 0;
	int iconMaskIndex = 0;
	std::function<bool(const Variable&)> collector = [this, &collector, &fontFamilyIndex, &iconMaskIndex](const Variable &variable) {
		auto value = variable.value;
		if (!value.copyOf().isEmpty()) {
			return true;
		}

		switch (value.type().tag) {
		case Tag::Invalid:
		case Tag::Int:
		case Tag::Double:
		case Tag::String:
		case Tag::Color:
		case Tag::Align: break;
		case Tag::Pixels: pxValues_.insert(value.Int(), true); break;
		case Tag::Point: {
			auto v(value.Point());
			pxValues_.insert(v.x, true);
			pxValues_.insert(v.y, true);
		} break;
		case Tag::Size: {
			auto v(value.Size());
			pxValues_.insert(v.width, true);
			pxValues_.insert(v.height, true);
		} break;
		case Tag::Margins: {
			auto v(value.Margins());
			pxValues_.insert(v.left, true);
			pxValues_.insert(v.top, true);
			pxValues_.insert(v.right, true);
			pxValues_.insert(v.bottom, true);
		} break;
		case Tag::Font: {
			auto v(value.Font());
			pxValues_.insert(v.size, true);
			if (!v.family.empty() && !fontFamilies_.contains(v.family)) {
				fontFamilies_.insert(v.family, ++fontFamilyIndex);
			}
		} break;
		case Tag::Icon: {
			auto v(value.Icon());
			for (auto &part : v.parts) {
				pxValues_.insert(part.offset.Point().x, true);
				pxValues_.insert(part.offset.Point().y, true);
				if (!iconMasks_.contains(part.filename)) {
					iconMasks_.insert(part.filename, ++iconMaskIndex);
				}
			}
		} break;
		case Tag::Struct: {
			auto fields = variable.value.Fields();
			if (!fields) {
				return false;
			}

			for (const auto &field : *fields) {
				if (!collector(field.variable)) {
					return false;
				}
			}
		} break;
		}
		return true;
	};
	return module_.enumVariables(collector);
}

} // namespace style
} // namespace codegen
