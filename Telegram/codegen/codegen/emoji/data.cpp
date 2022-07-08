// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "codegen/emoji/data.h"

#include "codegen/emoji/data_old.h"
#include "codegen/emoji/data_read.h"

namespace codegen {
namespace emoji {
namespace {

using std::vector;
using std::map;
using std::set;
using std::find;
using std::make_pair;
using std::move;
using std::begin;
using std::end;

// copied from emoji_box.cpp
struct Replace {
	InputId code;
	const char *replace;
};

Replace Replaces[] = {
	{ { 0xD83DDE0AU }, ":-)" },
	{ { 0xD83DDE0DU }, "8-)" },
	{ { 0x2764U }, "<3" },
//	{ { 0xD83DDC8BU }, ":kiss:" },
//	{ { 0xD83DDE01U }, ":grin:" },
//	{ { 0xD83DDE02U }, ":joy:" },
	{ { 0xD83DDE1AU }, ":-*" },
//	{ { 0xD83DDE06U }, "xD" }, // Conflicts with typing xDDD...
//	{ { 0xD83DDC4DU }, ":like:" },
//	{ { 0xD83DDC4EU }, ":dislike:" },
//	{ { 0x261DU }, ":up:" },
//	{ { 0x270CU }, ":v:" },
//	{ { 0xD83DDC4CU }, ":ok:" },
	{ { 0xD83DDE0EU }, "B-)" },
	{ { 0xD83DDE03U }, ":-D" },
	{ { 0xD83DDE09U }, ";-)" },
	{ { 0xD83DDE1CU }, ";-P" },
	{ { 0xD83DDE0BU }, ":-p" },
	{ { 0xD83DDE14U }, "3(" },
	{ { 0xD83DDE1EU }, ":-(" },
	{ { 0xD83DDE0FU }, ":]" },
	{ { 0xD83DDE22U }, ":'(" },
	{ { 0xD83DDE2DU }, ":_(" },
	{ { 0xD83DDE29U }, ":((" },
//	{ { 0xD83DDE28U }, ":o" }, // Conflicts with typing :ok...
	{ { 0xD83DDE10U }, ":|" },
	{ { 0xD83DDE0CU }, "3-)" },
	{ { 0xD83DDE20U }, ">(" },
	{ { 0xD83DDE21U }, ">((" },
	{ { 0xD83DDE07U }, "O:)" },
	{ { 0xD83DDE30U }, ";o" },
	{ { 0xD83DDE33U }, "8|" },
	{ { 0xD83DDE32U }, "8o" },
	{ { 0xD83DDE37U }, ":X" },
	{ { 0xD83DDE08U }, "}:)" },
};

InputCategory PostfixRequired = {
 { 0x2122U, 0xFE0FU, },
 { 0xA9U, 0xFE0FU, },
 { 0xAEU, 0xFE0FU, },
};

using ColorId = uint32;
ColorId Colors[] = {
	0xD83CDFFBU,
	0xD83CDFFCU,
	0xD83CDFFDU,
	0xD83CDFFEU,
	0xD83CDFFFU,
};

constexpr auto ColorMask = 0xD83CDFFBU;

// Original data has those emoji only with gender symbols.
// But they should be displayed as emoji even without gender symbols.
// So we map which gender symbol to use for an emoji without one.
std::map<InputId, uint32> WithoutGenderAliases = {
// { { 0xD83EDD26U, }, 0x2642U },
// { { 0xD83EDD37U, }, 0x2640U },
// { { 0xD83EDD38U, }, 0x2642U },
// { { 0xD83EDD3CU, }, 0x2640U },
// { { 0xD83EDD3DU, }, 0x2642U },
// { { 0xD83EDD3EU, }, 0x2640U },
// { { 0xD83EDD39U, }, 0x2642U },
// { { 0xD83EDDB8U, }, 0x2640U },
// { { 0xD83EDDB9U, }, 0x2640U },
// { { 0xD83EDDD6U, }, 0x2642U },
// { { 0xD83EDDD7U, }, 0x2640U },
// { { 0xD83EDDD8U, }, 0x2640U },
// { { 0xD83EDDD9U, }, 0x2640U },
// { { 0xD83EDDDAU, }, 0x2640U },
// { { 0xD83EDDDBU, }, 0x2640U },
// { { 0xD83EDDDCU, }, 0x2642U },
// { { 0xD83EDDDDU, }, 0x2642U },
// { { 0xD83EDDDEU, }, 0x2642U },
// { { 0xD83EDDDFU, }, 0x2642U },
};

// Some flags are sent as one string, but are rendered as a different too.
std::map<InputId, InputId> FlagAliases = {
// { { 0xD83CDDE8U, 0xD83CDDF5U, }, { 0xD83CDDEBU, 0xD83CDDF7U, } },
// { { 0xD83CDDE7U, 0xD83CDDFBU, }, { 0xD83CDDF3U, 0xD83CDDF4U, } },
// { { 0xD83CDDE6U, 0xD83CDDE8U, }, { 0xD83CDDF8U, 0xD83CDDEDU, } },
//
// // This is different flag, but macOS shows that glyph :(
// { { 0xD83CDDE9U, 0xD83CDDECU, }, { 0xD83CDDEEU, 0xD83CDDF4U, } },
//
// { { 0xD83CDDF9U, 0xD83CDDE6U, }, { 0xD83CDDF8U, 0xD83CDDEDU, } },
// { { 0xD83CDDF2U, 0xD83CDDEBU, }, { 0xD83CDDEBU, 0xD83CDDF7U, } },
// { { 0xD83CDDEAU, 0xD83CDDE6U, }, { 0xD83CDDEAU, 0xD83CDDF8U, } },
  { { 0xD83CDDE8U, 0xD83CDDF5U, }, { 0xD83CDDEBU, 0xD83CDDF7U, } },
};

std::map<Id, std::vector<Id>> Aliases; // original -> list of aliased
std::set<Id> AliasesAdded;

void AddAlias(const Id &original, const Id &aliased) {
	auto &aliases = Aliases[original];
	if (std::find(aliases.begin(), aliases.end(), aliased) == aliases.end()) {
		aliases.push_back(aliased);
	}
}

constexpr auto kErrorBadData = 401;

void append(Id &id, uint32 code) {
	if (auto first = static_cast<uint16>((code >> 16) & 0xFFFFU)) {
		id.append(QChar(first));
	}
	id.append(QChar(static_cast<uint16>(code & 0xFFFFU)));
}

[[nodiscard]] Id BareIdFromInput(const InputId &id) {
	auto result = Id();
	for (const auto unicode : id) {
		if (unicode != kPostfix) {
			append(result, unicode);
		}
	}
	return result;
}

[[nodiscard]] set<Id> FillVariatedIds(const InputData &data) {
	auto result = set<Id>();
	for (const auto &row : data.colored) {
		auto variatedId = Id();
		if (row.size() < 2) {
			logDataError() << "colored string should have at least two characters.";
			return {};
		}
		for (auto i = size_t(0), size = row.size(); i != size; ++i) {
			auto code = row[i];
			if (i == 1) {
				if (code != ColorMask) {
					logDataError() << "color code should appear at index 1.";
					return {};
				}
			} else if (code == ColorMask) {
				logDataError() << "color code should appear only at index 1.";
				return {};
			} else if (code != kPostfix) {
				append(variatedId, code);
			}
		}
		result.emplace(variatedId);
	}
	return result;
}

[[nodiscard]] map<Id, InputId> FillDoubleVariatedIds(const InputData &data) {
	auto result = map<Id, InputId>();
	for (const auto &[original, same, different] : data.doubleColored) {
		auto variatedId = Id();
		if (original.size() < 1) {
			logDataError() << "original string should have at least one character.";
			return {};
		} else if (same.size() < 2) {
			logDataError() << "colored string should have at least two characters.";
			return {};
		}
		if (same.size() == 2) {
			// original: 1
			// same: original + color
			// different: some1 + color1 + sep + some2 + sep + some3 + color2
			if (same[1] != ColorMask) {
				logDataError() << "color code should appear at index 1.";
				return {};
			} else if (same[0] == ColorMask) {
				logDataError() << "color code should appear only at index 1.";
				return {};
			} else if (same[0] == kPostfix) {
				logDataError() << "postfix in double colored is not supported.";
				return {};
			} else {
				append(variatedId, same[0]);
			}
			// add an alias to 'same' in the form ..
			// .. of 'some1 + color + sep + some2 + sep + some3 + color'
			if (std::count(different.begin(), different.end(), kJoiner) != 2) {
				logDataError() << "complex double colored bad different.";
				return {};
			}
			for (const auto color : Colors) {
				auto copy = same;
				copy[1] = color;
				auto sameWithColor = BareIdFromInput(copy);
				copy = different;
				for (auto &entry : copy) {
					if (entry == Colors[0] || entry == Colors[1]) {
						entry = color;
					}
				}
				auto differentWithColor = BareIdFromInput(copy);
				AddAlias(sameWithColor, differentWithColor);
			}
		} else {
			// same: som1 + color + sep + some2 + sep + some3 + color
			// different: some1 + color1 + sep + some2 + sep + some3 + color2
			auto copy = different;
			if (copy.size() < 7 || copy[1] != Colors[0] || copy[copy.size() - 1] != Colors[1]) {
				logDataError() << "complex double colored bad different.";
				return {};
			}
			copy[copy.size() - 1] = Colors[0];
			if (copy != same) {
				logDataError() << "complex double colored should colorize all the same.";
				return {};
			}
			if (original.size() == 1) {
				// original: 1
				// add an alias to 'same' in the form of 'original + color'
				for (const auto color : Colors) {
					auto copy = original;
					copy.push_back(color);
					auto originalWithColor = BareIdFromInput(copy);
					copy = same;
					for (auto &entry : copy) {
						if (entry == ColorMask) {
							entry = color;
						}
					}
					auto sameWithColor = BareIdFromInput(copy);
					AddAlias(originalWithColor, sameWithColor);
				}
			}
			variatedId = BareIdFromInput(original);
		}
		result.emplace(variatedId, different);
	}
	return result;
}

[[nodiscard]] set<Id> FillPostfixRequiredIds() {
	auto result = set<Id>();
	for (const auto &row : PostfixRequired) {
		result.emplace(BareIdFromInput(row));
	}
	return result;
}

void appendCategory(
		Data &result,
		const InputCategory &category,
		const set<Id> &variatedIds,
		const map<Id, InputId> &doubleVariatedIds,
		const set<Id> &postfixRequiredIds) {
	result.categories.emplace_back();
	for (auto &id : category) {
		auto emoji = Emoji();
		auto bareId = BareIdFromInput(id);
		auto from = id.cbegin(), to = id.cend();
		if (to - from == 2 && *(to - 1) == kPostfix) {
			emoji.postfixed = true;
			--to;
		}
		for (auto i = from; i != to; ++i) {
			auto code = *i;
			if (find(begin(Colors), end(Colors), code) != end(Colors)) {
				logDataError() << "color code found in a category emoji.";
				result = Data();
				return;
			}
			append(emoji.id, code);
		}
		if (bareId.isEmpty()) {
			logDataError() << "empty emoji id found.";
			result = Data();
			return;
		}

		const auto addOne = [&](const Id &bareId, Emoji &&emoji) {
			auto it = result.map.find(bareId);
			if (it == result.map.cend()) {
				const auto index = result.list.size();
				it = result.map.emplace(bareId, index).first;
				result.list.push_back(move(emoji));
				if (const auto a = Aliases.find(bareId); a != end(Aliases)) {
					AliasesAdded.emplace(bareId);
					for (const auto &alias : a->second) {
						const auto ok = result.map.emplace(alias, index).second;
						if (!ok) {
							logDataError() << "some emoji alias already in the map.";
							result = Data();
							return result.map.end();
						}
					}
				}
				if (postfixRequiredIds.find(bareId) != end(postfixRequiredIds)) {
					result.postfixRequired.emplace(index);
				}
			} else if (result.list[it->second].postfixed != emoji.postfixed) {
				logDataError() << "same emoji found with different postfixed property.";
				result = Data();
				return result.map.end();
			} else if (result.list[it->second].id != emoji.id) {
				logDataError() << "same emoji found with different id.";
				result = Data();
				return result.map.end();
			}
			return it;
		};

		auto it = addOne(bareId, move(emoji));
		if (it == result.map.end()) {
			return;
		}
		if (variatedIds.find(bareId) != end(variatedIds)) {
			result.list[it->second].variated = true;

			auto baseId = Id();
			if (*from == kPostfix) {
				logDataError() << "bad first symbol in emoji.";
				result = Data();
				return;
			}
			append(baseId, *from++);
			// A few uncolored emoji contain two kPostfix.
			// (:(wo)man_lifting_weights:, :(wo)man_golfing:
			//  :(wo)man_bouncing_ball:, :(wo)man_detective:)
			// But colored emoji should have only one kPostfix.
			if (std::count(from, to, kPostfix) == 2 && *from == kPostfix) {
				from++;
			}
			for (auto color : Colors) {
				auto colored = Emoji();
				colored.colored = true;
				colored.id = baseId;
				append(colored.id, color);
				auto bareColoredId = colored.id;
				for (auto i = from; i != to; ++i) {
					append(colored.id, *i);
					if (*i != kPostfix) {
						append(bareColoredId, *i);
					}
				}
				if (addOne(bareColoredId, std::move(colored)) == result.map.end()) {
					return;
				}
			}
		} else if (const auto d = doubleVariatedIds.find(bareId); d != end(doubleVariatedIds)) {
			//result.list[it->second].doubleVariated = true;

			const auto baseId = bareId;
			const auto &different = d->second;
			if (different.size() < 4
				|| different[1] != Colors[0]
				|| different[different.size() - 1] != Colors[1]) {
				logDataError() << "bad data in double-colored emoji.";
				result = Data();
				return;
			}
			for (auto color1 : Colors) {
				for (auto color2 : Colors) {
					auto colored = Emoji();
					//colored.colored = true;
					if (color1 == color2 && baseId.size() == 2) {
						colored.id = baseId;
						append(colored.id, color1);
					} else {
						auto copy = different;
						copy[1] = color1;
						copy[copy.size() - 1] = color2;
						for (const auto code : copy) {
							append(colored.id, code);
						}
					}
					auto bareColoredId = colored.id.replace(QChar(kPostfix), QString());
					if (addOne(bareColoredId, move(colored)) == result.map.end()) {
						return;
					}
				}
			}

		}
		result.categories.back().push_back(it->second);
	}
}

void fillReplaces(Data &result) {
	for (auto &replace : Replaces) {
		auto id = Id();
		for (auto code : replace.code) {
			append(id, code);
		}
		auto it = result.map.find(id);
		if (it == result.map.cend()) {
			logDataError() << "emoji from replaces not found in the map.";
			result = Data();
			return;
		}
		result.replaces.insert(make_pair(QString::fromUtf8(replace.replace), it->second));
	}
}

bool CheckOldInCurrent(
		const InputData &data,
		const InputData &old,
		const std::set<Id> &variatedIds) {
	const auto genders = { 0x2640U, 0x2642U };
	const auto addGender = [](const InputId &was, uint32 gender) {
		auto result = was;
		result.push_back(0x200DU);
		result.push_back(gender);
		result.push_back(kPostfix);
		return result;
	};
	const auto addGenderByIndex = [&](const InputId &was, int index) {
		return addGender(was, *(begin(genders) + index));
	};
	static const auto find = [](
			const InputCategory &list,
			const InputId &id) {
		return (std::find(begin(list), end(list), id) != end(list));
	};
	static const auto findInMany = [](
			const InputData &data,
			const InputId &id) {
		for (const auto &current : data.categories) {
			if (find(current, id)) {
				return true;
			}
		}
		return find(data.other, id);
	};
	const auto emplaceColoredAlias = [](const InputId &real, const InputId &alias, uint32_t color) {
		if (real.size() < 2 || alias.size() < 2 || real[1] != Colors[0] || alias[1] != Colors[0]) {
			return false;
		}
		auto key = real;
		key[1] = color;
		auto value = alias;
		value[1] = color;
		AddAlias(BareIdFromInput(key), BareIdFromInput(value));
		return true;
	};
	auto result = true;
	for (auto c = begin(old.categories); c != end(old.categories); ++c) {
		const auto &category = *c;
		for (auto i = begin(category); i != end(category); ++i) {
			if (findInMany(data, *i)) {
				continue;
			}

			// Some emoji were ending with kPostfix and now are not.
			if (i->back() == kPostfix) {
				auto other = *i;
				other.pop_back();
				if (findInMany(data, other)) {
					continue;
				}
			}

			// Some emoji were not ending with kPostfix and now are.
			if (i->back() != kPostfix) {
				auto other = *i;
				other.push_back(kPostfix);
				if (findInMany(data, other)) {
					continue;
				}
			}

			common::logError(kErrorBadData, "input")
				<< "Bad data: old emoji '"
				<< InputIdToString(*i).toStdString()
				<< "' (category "
				<< (c - begin(old.categories))
				<< ", index "
				<< (i - begin(category))
				<< ") not found in current.";
			result = false;
			continue;

			// Old way - auto add genders. New way - add gender neutral images,
			// but don't add them to any category in the picker.
//			// Some emoji were without gender symbol and now have gender symbol.
//			// Try adding 0x200DU, 0x2640U, 0xFE0FU or 0x200DU, 0x2642U, 0xFE0FU.
//			const auto otherGenderIndex = [&] {
//				for (auto g = begin(genders); g != end(genders); ++g) {
//					auto altered = *i;
//					altered.push_back(kJoiner);
//					altered.push_back(*g);
//					altered.push_back(kPostfix);
//					if (findInMany(old, altered)) {
//						return int(g - begin(genders));
//					}
//				}
//				return -1;
//			}();
//			if (otherGenderIndex < 0) {
//				common::logError(kErrorBadData, "input")
//					<< "Bad data: old emoji '"
//					<< InputIdToString(*i).toStdString()
//					<< "' (category "
//					<< (c - begin(old.categories))
//					<< ", index "
//					<< (i - begin(category))
//					<< ") not found in current.";
//				result = false;
//				continue;
//			}
//
//			const auto genderIndex = (1 - otherGenderIndex);
//			const auto real = addGenderByIndex(*i, genderIndex);
//			const auto bare = BareIdFromInput(real);
//			if (!findInMany(data, real)) {
//				common::logError(kErrorBadData, "input")
//					<< "Bad data: old emoji '"
//					<< InputIdToString(*i).toStdString()
//					<< "' (category "
//					<< (c - begin(old.categories))
//					<< ", index "
//					<< (i - begin(category))
//					<< ") not found in current with added gender: "
//					<< genderIndex
//					<< ".";
//				result = false;
//			} else {
//				AddAlias(bare, BareIdFromInput(*i));
//			}
		}
	}
	for (auto i = begin(old.doubleColored); i != end(old.doubleColored); ++i) {
		auto found = false;
		for (auto j = begin(data.doubleColored); j != end(data.doubleColored); ++j) {
			if (j->original == i->original) {
				found = true;
				if (j->same != i->same || j->different != i->different) {
					common::logError(kErrorBadData, "input")
						<< "Bad data: old double colored emoji (index "
						<< (i - begin(old.doubleColored))
						<< ") not equal to current.";
					result = false;
				}
				break;
			}
		}
		if (!found) {
			common::logError(kErrorBadData, "input")
				<< "Bad data: old double colored emoji (index "
				<< (i - begin(old.doubleColored))
				<< ") not found in current.";
			result = false;
		}
	}
	for (auto i = begin(old.colored); i != end(old.colored); ++i) {
		if (find(data.colored, *i)) {
			continue;
		}

		const auto otherGenderIndex = [&] {
			for (auto g = begin(genders); g != end(genders); ++g) {
				auto altered = *i;
				altered.push_back(kJoiner);
				altered.push_back(*g);
				altered.push_back(kPostfix);
				if (find(old.colored, altered)) {
					return int(g - begin(genders));
				}
			}
			return -1;
		}();
		if (otherGenderIndex < 0) {
			common::logError(kErrorBadData, "input")
				<< "Bad data: old colored emoji (index "
				<< (i - begin(old.colored))
				<< ") not found in current.";
			result = false;
			continue;
		}

		const auto genderIndex = (1 - otherGenderIndex);
		const auto real = addGenderByIndex(*i, genderIndex);
		if (!find(data.colored, real)) {
			common::logError(kErrorBadData, "input")
				<< "Bad data: old colored emoji (index "
				<< (i - begin(old.colored))
				<< ") not found in current with added gender: "
				<< genderIndex
				<< ".";
			result = false;
			continue;
		} else {
			for (const auto color : Colors) {
				if (!emplaceColoredAlias(real, *i, color)) {
					common::logError(kErrorBadData, "input")
						<< "Bad data: bad colored emoji.";
					result = false;
					break;
				}
			}
		}
	}

	for (const auto &entry : WithoutGenderAliases) {
		const auto &inputId = entry.first;
		const auto &gender = entry.second;
		if (findInMany(data, inputId)) {
			continue;
		}
		const auto real = [&] {
			auto result = addGender(inputId, gender);
			if (findInMany(data, result)) {
				return result;
			}
			result.push_back(kPostfix);
			return result;
		}();
		const auto bare = BareIdFromInput(real);
		if (!findInMany(data, real)) {
			common::logError(kErrorBadData, "input")
				<< "Bad data: without gender alias not found with gender.";
			result = false;
		} else {
			AddAlias(bare, BareIdFromInput(inputId));
		}
		if (variatedIds.find(bare) != variatedIds.end()) {
			auto colorReal = real;
			colorReal.insert(colorReal.begin() + 1, Colors[0]);
			auto colorInput = inputId;
			colorInput.insert(colorInput.begin() + 1, Colors[0]);
			for (const auto color : Colors) {
				if (!emplaceColoredAlias(colorReal, colorInput, color)) {
					common::logError(kErrorBadData, "input")
						<< "Bad data: bad colored emoji.";
					result = false;
					break;
				}
			}

		}
	}

	for (const auto &[inputId, real] : FlagAliases) {
		AddAlias(BareIdFromInput(real), BareIdFromInput(inputId));
	}

	return result;
}

bool CheckOldInCurrent(
		const InputData &data,
		const std::set<Id> &variatedIds,
		const std::vector<QString> &oldDataPaths) {
	if (!CheckOldInCurrent(data, GetDataOld1(), variatedIds)
		|| !CheckOldInCurrent(data, GetDataOld2(), variatedIds)) {
		return false;
	}
	for (const auto &path : oldDataPaths) {
		const auto old = ReadData(path);
		if (old.colored.empty() || old.doubleColored.empty()) {
			return false;
		} else if (!CheckOldInCurrent(data, old, variatedIds)) {
			return false;
		}
	}
	return true;
}

} // namespace

common::LogStream logDataError() {
	return common::logError(kErrorBadData, "input") << "Bad data: ";
}

Data PrepareData(const QString &dataPath, const std::vector<QString> &oldDataPaths) {
	Data result;

	auto input = ReadData(dataPath);
	const auto variatedIds = FillVariatedIds(input);
	const auto doubleVariatedIds = FillDoubleVariatedIds(input);
	const auto postfixRequiredIds = FillPostfixRequiredIds();
	if (variatedIds.empty() || doubleVariatedIds.empty() || postfixRequiredIds.empty()) {
		return Data();
	}

	if (!CheckOldInCurrent(input, variatedIds, oldDataPaths)) {
		return Data();
	}

	for (const auto &category : input.categories) {
		appendCategory(result, category, variatedIds, doubleVariatedIds, postfixRequiredIds);
		if (result.list.empty()) {
			return Data();
		}
	}
	appendCategory(result, input.other, variatedIds, doubleVariatedIds, postfixRequiredIds);
	if (result.list.empty()) {
		return Data();
	}
	if (AliasesAdded.size() != Aliases.size()) {
		for (const auto &[key, list] : Aliases) {
			if (AliasesAdded.find(key) == AliasesAdded.end()) {
				QStringList expanded;
				for (const auto &ch : key) {
					expanded.push_back(QString::number(ch.unicode()));
				}
				common::logError(kErrorBadData, "input")
					<< "Bad data: Not added aliases list for: "
					<< expanded.join(QChar(',')).toStdString();
			}
		}
		return Data();
	}

	fillReplaces(result);
	if (result.list.empty()) {
		return Data();
	}

	return result;
}

} // namespace emoji
} // namespace codegen
