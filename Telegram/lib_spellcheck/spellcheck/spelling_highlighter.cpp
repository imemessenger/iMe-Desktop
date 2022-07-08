// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//

#include "spellcheck/spelling_highlighter.h"

#include "spellcheck/spellcheck_value.h"
#include "spellcheck/spellcheck_utils.h"
#include "spellcheck/spelling_highlighter_helper.h"
#include "styles/palette.h"
#include "ui/text/text_entity.h"
#include "ui/text/text_utilities.h"
#include "ui/ui_utility.h"
#include "base/qt/qt_common_adapters.h"

namespace Spellchecker {

namespace {

constexpr auto kTagProperty = QTextFormat::UserProperty + 4;
const auto kUnspellcheckableTags = {
	&Ui::InputField::kTagCode,
	&Ui::InputField::kTagPre,
	&Ui::InputField::kTagUnderline
};

constexpr auto kColdSpellcheckingTimeout = crl::time(1000);

constexpr auto kMaxDeadKeys = 1;

constexpr auto kSkippableFlags = 0
	| TextParseLinks
	| TextParseMentions
	| TextParseHashtags
	| TextParseBotCommands;

const auto kKeysToCheck = {
	Qt::Key_Up,
	Qt::Key_Down,
	Qt::Key_Left,
	Qt::Key_Right,
	Qt::Key_PageUp,
	Qt::Key_PageDown,
	Qt::Key_Home,
	Qt::Key_End,
};

inline int EndOfWord(const MisspelledWord &range) {
	return range.first + range.second;
}

inline bool IntersectsWordRanges(
	const MisspelledWord &range,
	int pos2,
	int len2) {
	const auto l1 = range.first;
	const auto r1 = EndOfWord(range) - 1;
	const auto l2 = pos2;
	const auto r2 = pos2 + len2 - 1;
	return !(l1 > r2 || l2 > r1);
}

inline bool IntersectsWordRanges(
	const MisspelledWord &range,
	const MisspelledWord &range2) {
	const auto l1 = range.first;
	const auto r1 = EndOfWord(range) - 1;
	const auto l2 = range2.first;
	const auto r2 = EndOfWord(range2) - 1;
	return !(l1 > r2 || l2 > r1);
}

inline bool IntersectsWordRanges(const EntityInText &e,	int pos2, int len2) {
	return IntersectsWordRanges({ e.offset(), e.length() }, pos2, len2);
}

inline bool IsTagUnspellcheckable(const QString &tag) {
	if (tag.isEmpty()) {
		return false;
	}
	for (const auto &single : TextUtilities::SplitTags(tag)) {
		const auto isCommonFormatting = ranges::any_of(
			kUnspellcheckableTags,
			[&](const auto *t) { return (*t) == single; });

		if (isCommonFormatting) {
			return true;
		}

		if (Ui::InputField::IsValidMarkdownLink(single)) {
			return true;
		}

		if (TextUtilities::IsMentionLink(single)) {
			return true;
		}
	}
	return false;
}

inline auto FindEntities(const QString &text) {
	return TextUtilities::ParseEntities(text, kSkippableFlags).entities;
}

inline auto IntersectsAnyOfEntities(
	int pos,
	int len,
	EntitiesInText entities) {
	return !entities.empty() && ranges::any_of(entities, [&](const auto &e) {
		return IntersectsWordRanges(e, pos, len);
	});
}

inline QChar AddedSymbol(QStringView text, int position, int added) {
	if (added != 1 || position >= text.size()) {
		return QChar();
	}
	return text.at(position);
}

inline MisspelledWord CorrectAccentValues(
		const QString &oldText,
		const QString &newText) {
	auto diff = std::vector<int>();
	const auto sizeOfDiff = newText.size() - oldText.size();
	if (sizeOfDiff <= 0 || sizeOfDiff > kMaxDeadKeys) {
		return MisspelledWord();
	}
	for (auto i = 0; i < oldText.size(); i++) {
		if (oldText.at(i) != newText.at(i + diff.size())) {
			diff.push_back(i);
			if (diff.size() > kMaxDeadKeys) {
				return MisspelledWord();
			}
		}
	}
	if (diff.size() == 0) {
		return MisspelledWord(oldText.size(), sizeOfDiff);
	}
	return MisspelledWord(diff.front(), diff.size() > 1 ? diff.back() : 1);
}

inline MisspelledWord RangeFromCursorSelection(const QTextCursor &cursor) {
	const auto start = cursor.selectionStart();
	return MisspelledWord(start, cursor.selectionEnd() - start);
}

} // namespace

SpellingHighlighter::SpellingHighlighter(
	not_null<Ui::InputField*> field,
	rpl::producer<bool> enabled,
	std::optional<CustomContextMenuItem> customContextMenuItem)
: QSyntaxHighlighter(field->rawTextEdit()->document())
, _cursor(QTextCursor(document()))
, _coldSpellcheckingTimer([=] { checkChangedText(); })
, _field(field)
, _textEdit(field->rawTextEdit())
, _customContextMenuItem(customContextMenuItem) {
#ifdef Q_OS_WIN
	Platform::Spellchecker::Init();
#endif // !Q_OS_WIN

	_cachedRanges = MisspelledWords();

	// Use the patched SpellCheckUnderline style.
	_misspelledFormat.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
	style::PaletteChanged(
	) | rpl::start_with_next([=] {
		updatePalette();
		rehighlight();
	}, _lifetime);
	updatePalette();

	_field->documentContentsChanges(
	) | rpl::start_with_next([=](const auto &value) {
		const auto &[pos, removed, added] = value;
		contentsChange(pos, removed, added);
	}, _lifetime);

	_field->markdownTagApplies(
	) | rpl::start_with_next([=](auto markdownTag) {
		if (!IsTagUnspellcheckable(markdownTag.tag)) {
			return;
		}
		_cachedRanges = ranges::views::all(
			_cachedRanges
		) | ranges::views::filter([&](const auto &range) {
			return !IntersectsWordRanges(
				range,
				markdownTag.internalStart,
				markdownTag.internalLength);
		}) | ranges::to_vector;
		rehighlight();
	}, _lifetime);

	updateDocumentText();

	std::move(
		enabled
	) | rpl::start_with_next([=](bool value) {
		setEnabled(value);
		if (_enabled) {
			_textEdit->installEventFilter(this);
			_textEdit->viewport()->installEventFilter(this);
		} else {
			_textEdit->removeEventFilter(this);
			_textEdit->viewport()->removeEventFilter(this);
		}
	}, _lifetime);

	Spellchecker::SupportedScriptsChanged(
	) | rpl::start_with_next([=] {
		checkCurrentText();
	}, _lifetime);
}

void SpellingHighlighter::updatePalette() {
	_misspelledFormat.setUnderlineColor(st::spellUnderline->c);
}

void SpellingHighlighter::contentsChange(int pos, int removed, int added) {
	if (!_enabled) {
		return;
	}
	if (document()->isEmpty()) {
		updateDocumentText();
		_cachedRanges.clear();
		return;
	}

	{
		const auto oldText = documentText().mid(
			pos,
			documentText().indexOf(QChar::ParagraphSeparator, pos));
		updateDocumentText();

		const auto b = findBlock(pos);
		const auto bLen = (document()->blockCount() > 1)
			? b.length()
			: b.text().size();

		// This is a workaround for typing accents.
		// For example, when the user press the dead key (e.g. ` or ´),
		// Qt sends wrong values. E.g. if a text length is 100,
		// then values will be 0, 100, 100.
		// This invokes to re-check the entire text.
		// The Mac's accent menu has a pretty similar behavior.
		if ((b.position() == pos) && (bLen == added)) {
			const auto newText = b.text();
			const auto diff = added - removed;
			// The plain text of the document cannot contain dead keys.
			if (!diff) {
				if (!oldText.compare(newText, Qt::CaseSensitive)) {
					const auto c = RangeFromCursorSelection(
						_textEdit->textCursor());
					// If the cursor has a selection for the entire text,
					// we probably just changed its formatting.
					// So if we find the unspellcheckable tag,
					// we can clear cached ranges of misspelled words.
					if (!c.first && c.second == bLen) {
						if (hasUnspellcheckableTag(pos, added)) {
							_cachedRanges.clear();
							rehighlight();
						} else {
							checkCurrentText();
						}
					}
					return;
				}
			} else if (diff > 0 && diff <= kMaxDeadKeys) {
				const auto [p, l] = CorrectAccentValues(oldText, newText);
				if (l) {
					pos = p + b.position();
					added = l;
					removed = 0;
				}
			}
		}
	}

	const auto shift = [&](auto chars) {
		ranges::for_each(_cachedRanges, [&](auto &range) {
			if (range.first >= pos + removed) {
				range.first += chars;
			}
		});
	};

	// Shift to the right all words after the cursor, when adding text.
	if (added > 0) {
		shift(added);
	}

	// Remove all words that are in the selection.
	// Remove the word that is under the cursor.
	const auto wordUnderPos = getWordUnderPosition(pos);

	// If the cursor is between spaces,
	// QTextCursor::WordUnderCursor highlights the word on the left
	// even if the word is not under the cursor.
	// Example: "super  |  test", where | is the cursor position.
	// In this example QTextCursor::WordUnderCursor will select "super".
	const auto isPosNotInWord = pos > EndOfWord(wordUnderPos);

	_cachedRanges = (
		_cachedRanges
	) | ranges::views::filter([&](const auto &range) {
		const auto isIntersected = IntersectsWordRanges(range, wordUnderPos);
		if (isIntersected) {
			return isPosNotInWord;
		}
		return !(isIntersected
			|| (removed > 0 && IntersectsWordRanges(range, pos, removed)));
	}) | ranges::to_vector;

	// Shift to the left all words after the cursor, when deleting text.
	if (removed > 0) {
		shift(-removed);
	}

	// Normally we should to invoke rehighlighting to immediately apply
	// shifting of ranges. But we don't have to do this because the call of
	// contentsChange() is performed before the application's call of
	// highlightBlock().

	_addedSymbols += added;
	_removedSymbols += removed;

	// The typing of text character by character should produce
	// the same _lastPosition, _addedSymbols and _removedSymbols values
	// as removing and pasting several characters at a time.
	if (!_lastPosition || (removed == 1)) {
		_lastPosition = pos;
	}

	const auto addedSymbol = AddedSymbol(documentText(), pos, added);

	if ((removed == 1) || addedSymbol.isLetterOrNumber()) {
		if (_coldSpellcheckingTimer.isActive()) {
			_coldSpellcheckingTimer.cancel();
		}
		_coldSpellcheckingTimer.callOnce(kColdSpellcheckingTimeout);
	} else {
		// We forcefully increase the range of check
		// when inserting a non-char. This can help when the user inserts
		// a non-char in the middle of a word.
		if (!(addedSymbol.isNull()
			|| addedSymbol.isSpace()
			|| addedSymbol.isLetterOrNumber())) {
			_lastPosition--;
			_addedSymbols++;
		}
		if (_isLastKeyRepeat) {
			return;
		}
		checkChangedText();
	}
}

void SpellingHighlighter::checkChangedText() {
	const auto pos = _lastPosition;
	const auto added = _addedSymbols;
	const auto removed = _removedSymbols;

	_lastPosition = 0;
	_removedSymbols = 0;
	_addedSymbols = 0;

	if (_coldSpellcheckingTimer.isActive()) {
		_coldSpellcheckingTimer.cancel();
	}

	const auto wordUnderCursor = getWordUnderPosition(pos);
	// If the length of the word is 0, there is no sense in checking it.
	if (!wordUnderCursor.second) {
		return;
	}

	const auto wordInCacheIt = [=] {
		return ranges::find_if(_cachedRanges, [&](auto &&w) {
			return w.first >= wordUnderCursor.first;
		});
	};

	if (added > 0) {
		const auto lastWordNewSelection = getWordUnderPosition(pos + added);

		// This is the same word.
		if (wordUnderCursor == lastWordNewSelection) {
			checkSingleWord(wordUnderCursor);
			return;
		}

		const auto beginNewSelection = wordUnderCursor.first;
		const auto endNewSelection = EndOfWord(lastWordNewSelection);

		auto callback = [=](MisspelledWords &&r) {
			ranges::insert(_cachedRanges, wordInCacheIt(), std::move(r));
		};
		invokeCheckText(
			beginNewSelection,
			endNewSelection - beginNewSelection,
			std::move(callback));
		return;
	}

	if (removed > 0) {
		checkSingleWord(wordUnderCursor);
	}
}

MisspelledWords SpellingHighlighter::filterSkippableWords(
		MisspelledWords &ranges) {
	const auto text = documentText();
	if (text.isEmpty()) {
		return MisspelledWords();
	}
	return ranges | ranges::views::filter([&](const auto &range) {
		return !isSkippableWord(range);
	}) | ranges::to_vector;
}

bool SpellingHighlighter::isSkippableWord(const MisspelledWord &range) {
	return isSkippableWord(range.first, range.second);
}

bool SpellingHighlighter::isSkippableWord(int position, int length) {
	if (hasUnspellcheckableTag(position, length)) {
		return true;
	}
	const auto text = documentText();
	const auto ref = base::StringViewMid(text, position, length);
	if (ref.isNull()) {
		return true;
	}
	return IsWordSkippable(ref);
}

void SpellingHighlighter::checkCurrentText() {
	if (document()->isEmpty()) {
		_cachedRanges.clear();
		return;
	}
	invokeCheckText(0, size(), [&](MisspelledWords &&ranges) {
		_cachedRanges = std::move(ranges);
	});
}

void SpellingHighlighter::invokeCheckText(
		int textPosition,
		int textLength,
		Fn<void(MisspelledWords &&ranges)> callback) {
	if (!_enabled) {
		return;
	}

	const auto rangesOffset = textPosition;
	const auto text = partDocumentText(textPosition, textLength);
	const auto weak = Ui::MakeWeak(this);
	_countOfCheckingTextAsync++;
	crl::async([=,
		text = std::move(text),
		callback = std::move(callback)]() mutable {
		MisspelledWords misspelledWordRanges;
		Platform::Spellchecker::CheckSpellingText(
			text,
			&misspelledWordRanges);
		if (rangesOffset) {
			ranges::for_each(misspelledWordRanges, [&](auto &&range) {
				range.first += rangesOffset;
			});
		}
		crl::on_main(weak, [=,
				text = std::move(text),
				ranges = std::move(misspelledWordRanges),
				callback = std::move(callback)]() mutable {
			_countOfCheckingTextAsync--;
			// Checking a large part of text can take an unknown amount of
			// time. So we have to compare the text before and after async
			// work.
			// If the text has changed during async and we have more async,
			// we don't perform further refreshing of cache and underlines.
			// But if it was the last async, we should invoke a new one.
			if (compareDocumentText(text, textPosition, textLength)) {
				if (!_countOfCheckingTextAsync) {
					checkCurrentText();
				}
				return;
			}
			auto filtered = filterSkippableWords(ranges);

			// When we finish checking the text, the user can
			// supplement the last word and there may be a situation where
			// a part of the last word may not be underlined correctly.
			// Example:
			// 1. We insert a text with an incomplete last word.
			// "Time in a bottl".
			// 2. We don't wait for the check to be finished
			// and end the last word with the letter "e".
			// 3. invokeCheckText() will mark the last word "bottl" as
			// misspelled.
			// 4. checkSingleWord() will mark the "bottle" as correct and
			// leave it as it is.
			// 5. The first five letters of the "bottle" will be underlined
			// and the sixth will not be underlined.
			// We can fix it with a check of completeness of the last word.
			if (filtered.size()) {
				const auto lastWord = filtered.back();
				if (const auto endOfText = textPosition + textLength;
					EndOfWord(lastWord) == endOfText) {
					const auto word = getWordUnderPosition(endOfText);
					if (EndOfWord(word) != endOfText) {
						filtered.pop_back();
						checkSingleWord(word);
					}
				}
			}

			callback(std::move(filtered));
			for (const auto &b : blocksFromRange(textPosition, textLength)) {
				rehighlightBlock(b);
			}
		});
	});
}

void SpellingHighlighter::checkSingleWord(const MisspelledWord &singleWord) {
	const auto weak = Ui::MakeWeak(this);
	auto w = partDocumentText(singleWord.first, singleWord.second);
	if (isSkippableWord(singleWord)) {
		return;
	}
	crl::async([=,
		w = std::move(w),
		singleWord = std::move(singleWord)]() mutable {
		if (Platform::Spellchecker::CheckSpelling(std::move(w))) {
			return;
		}

		crl::on_main(weak, [=,
				singleWord = std::move(singleWord)]() mutable {
			const auto posOfWord = singleWord.first;
			ranges::insert(
				_cachedRanges,
				ranges::find_if(_cachedRanges, [&](auto &&w) {
					return w.first >= posOfWord;
				}),
				singleWord);
			rehighlightBlock(findBlock(posOfWord));
		});
	});
}

bool SpellingHighlighter::hasUnspellcheckableTag(int begin, int length) {
	// This method is called only in the context of separate words,
	// so it is not supposed that the word can be in more than one block.
	const auto block = findBlock(begin);
	length = std::min(block.position() + block.length() - begin, length);
	for (auto it = block.begin(); !(it.atEnd()); ++it) {
		const auto fragment = it.fragment();
		if (!fragment.isValid()) {
			continue;
		}
		const auto frPos = fragment.position();
		const auto frLen = fragment.length();
		if (!IntersectsWordRanges({ frPos, frLen }, begin, length)) {
			continue;
		}
		const auto format = fragment.charFormat();
		if (!format.hasProperty(kTagProperty)) {
			continue;
		}
		const auto tag = format.property(kTagProperty).toString();
		if (IsTagUnspellcheckable(tag)) {
			return true;
		}
	}

	return false;
}

MisspelledWord SpellingHighlighter::getWordUnderPosition(int position) {
	_cursor.setPosition(std::min(position, size()));
	_cursor.select(QTextCursor::WordUnderCursor);
	return RangeFromCursorSelection(_cursor);
}

void SpellingHighlighter::highlightBlock(const QString &text) {
	if (_cachedRanges.empty() || !_enabled || text.isEmpty()) {
		return;
	}
	const auto entities = FindEntities(text);
	const auto bPos = currentBlock().position();
	const auto bLen = currentBlock().length();
	ranges::for_each((
		_cachedRanges
	// Skip the all words outside the current block.
	) | ranges::views::filter([&](const auto &range) {
		return IntersectsWordRanges(range, bPos, bLen);
	}), [&](const auto &range) {
		const auto posInBlock = range.first - bPos;
		if (IntersectsAnyOfEntities(posInBlock, range.second, entities)) {
			return;
		}
		setFormat(posInBlock, range.second, _misspelledFormat);
	});

	setCurrentBlockState(0);
}

bool SpellingHighlighter::eventFilter(QObject *o, QEvent *e) {
	if (!_enabled) {
		return false;
	}
	if (e->type() == QEvent::ContextMenu) {
		const auto c = static_cast<QContextMenuEvent*>(e);
		const auto menu = _textEdit->createStandardContextMenu();
		if (!menu || !c) {
			return false;
		}
		// Copy of QContextMenuEvent.
		auto copyEvent = std::make_shared<QContextMenuEvent>(
			c->reason(),
			c->pos(),
			c->globalPos());
		auto showMenu = [=, copyEvent = std::move(copyEvent)] {
			_contextMenuCreated.fire({ menu, copyEvent });
		};
		addSpellcheckerActions(
			std::move(menu),
			_textEdit->cursorForPosition(c->pos()),
			std::move(showMenu),
			c->globalPos());
		return true;
	} else if (e->type() == QEvent::KeyPress) {
		const auto k = static_cast<QKeyEvent*>(e);

		if (ranges::contains(kKeysToCheck, k->key())) {
			if (_addedSymbols + _removedSymbols + _lastPosition) {
				checkCurrentText();
			}
		} else if ((o == _textEdit) && k->isAutoRepeat()) {
			_isLastKeyRepeat = true;
		}
	} else if (_isLastKeyRepeat && (o == _textEdit)) {
		if (e->type() == QEvent::FocusOut) {
			_isLastKeyRepeat = false;
			if (_addedSymbols + _removedSymbols + _lastPosition) {
				checkCurrentText();
			}
		} else if (e->type() == QEvent::KeyRelease) {
			const auto k = static_cast<QKeyEvent*>(e);
			if (!k->isAutoRepeat()) {
				_isLastKeyRepeat = false;
				_coldSpellcheckingTimer.callOnce(kColdSpellcheckingTimeout);
			}
		}
	} else if ((o == _textEdit->viewport())
			&& (e->type() == QEvent::MouseButtonPress)) {
		if (_addedSymbols + _removedSymbols + _lastPosition) {
			checkCurrentText();
		}
	}
	return false;
}

bool SpellingHighlighter::enabled() {
	return _enabled;
}

void SpellingHighlighter::setEnabled(bool enabled) {
	_enabled = enabled;
	if (_enabled) {
		updateDocumentText();
		checkCurrentText();
	} else {
		_cachedRanges.clear();
		rehighlight();
	}
}

QString SpellingHighlighter::documentText() {
	return _lastPlainText;
}

void SpellingHighlighter::updateDocumentText() {
	_lastPlainText = document()->toRawText();
}

QString SpellingHighlighter::partDocumentText(int pos, int length) {
	return _lastPlainText.mid(pos, length);
}

int SpellingHighlighter::size() {
	return document()->characterCount() - 1;
}

QTextBlock SpellingHighlighter::findBlock(int pos) {
	return document()->findBlock(pos);
}

std::vector<QTextBlock> SpellingHighlighter::blocksFromRange(
		int pos,
		int length) {
	auto b = findBlock(pos);
	auto blocks = std::vector<QTextBlock>{b};
	const auto end = pos + length;
	while (!b.contains(end) && (b != document()->end())) {
		if ((b = b.next()).isValid()) {
			blocks.push_back(b);
		}
	}
	return blocks;
}

int SpellingHighlighter::compareDocumentText(
		const QString &text,
		int textPos,
		int textLen) {
	if (_lastPlainText.size() < textPos + textLen) {
		return -1;
	}
	const auto p = base::StringViewMid(_lastPlainText, textPos, textLen);
	if (p.isNull()) {
		return -1;
	}
	return text.compare(p, Qt::CaseSensitive);
}

void SpellingHighlighter::addSpellcheckerActions(
		not_null<QMenu*> parentMenu,
		QTextCursor cursorForPosition,
		Fn<void()> showMenuCallback,
		QPoint mousePosition) {

	const auto customItem = !Platform::Spellchecker::IsSystemSpellchecker()
		&& _customContextMenuItem.has_value();

	cursorForPosition.select(QTextCursor::WordUnderCursor);

	// There is no reason to call async work if the word is skippable.
	const auto skippable = [&] {
		const auto &[p, l] = RangeFromCursorSelection(cursorForPosition);
		const auto e = FindEntities(findBlock(p).text());
		return (!l
			|| isSkippableWord(p, l)
			|| IntersectsAnyOfEntities(p, l, e));
	}();
	const auto menu = new QMenu(
		ph::lng_spellchecker_submenu(ph::now),
		parentMenu);

	auto addToParentAndShow = [=] {
		if (!menu->isEmpty()) {
			using namespace Spelling::Helper;
			if (IsContextMenuTop(parentMenu, mousePosition)) {
				parentMenu->addSeparator();
				parentMenu->addMenu(menu);
			} else {
				const auto first = parentMenu->actions().first();
				parentMenu->insertMenu(first, menu);
				parentMenu->insertSeparator(first);
			}
		}
		showMenuCallback();
	};

	if (customItem) {
		menu->addAction(
			_customContextMenuItem->title,
			_customContextMenuItem->callback);
	}

	if (skippable) {
		addToParentAndShow();
		return;
	}

	const auto word = cursorForPosition.selectedText();

	const auto fillMenu = [=,
		addToParentAndShow = std::move(addToParentAndShow),
		menu = std::move(menu)](
			const auto isCorrect,
			const auto suggestions,
			const auto newTextCursor) {

		const auto guard = gsl::finally(addToParentAndShow);

		const auto addSeparator = [&] {
			if (!menu->isEmpty()) {
				menu->addSeparator();
			}
		};

		if (isCorrect) {
			if (Platform::Spellchecker::IsWordInDictionary(word)) {
				addSeparator();
				auto remove = [=] {
					Platform::Spellchecker::RemoveWord(word);
					checkCurrentText();
				};
				menu->addAction(
					ph::lng_spellchecker_remove(ph::now),
					std::move(remove));
			}
			return;
		}

		addSeparator();

		auto add = [=] {
			Platform::Spellchecker::AddWord(word);
			checkCurrentText();
		};
		menu->addAction(ph::lng_spellchecker_add(ph::now), std::move(add));

		auto ignore = [=] {
			Platform::Spellchecker::IgnoreWord(word);
			checkCurrentText();
		};
		menu->addAction(
			ph::lng_spellchecker_ignore(ph::now),
			std::move(ignore));

		if (suggestions.empty()) {
			return;
		}

		addSeparator();
		for (const auto &suggestion : suggestions) {
			auto replaceWord = [=] {
				const auto oldTextCursor = _textEdit->textCursor();
				_textEdit->setTextCursor(newTextCursor);
				_textEdit->textCursor().insertText(suggestion);
				_textEdit->setTextCursor(oldTextCursor);
			};
			menu->addAction(suggestion, std::move(replaceWord));
		}
	};

	const auto weak = Ui::MakeWeak(this);
	crl::async([=,
		newTextCursor = std::move(cursorForPosition),
		fillMenu = std::move(fillMenu),
		word = std::move(word)]() mutable {

		const auto isCorrect = Platform::Spellchecker::CheckSpelling(word);
		std::vector<QString> suggestions;
		if (!isCorrect) {
			Platform::Spellchecker::FillSuggestionList(word, &suggestions);
		}

		crl::on_main(weak, [=,
				newTextCursor = std::move(newTextCursor),
				suggestions = std::move(suggestions),
				fillMenu = std::move(fillMenu)]() mutable {
			fillMenu(
				isCorrect,
				std::move(suggestions),
				std::move(newTextCursor));
		});
	});
}

} // namespace Spellchecker
