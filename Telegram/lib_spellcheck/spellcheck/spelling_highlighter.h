// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//

#include <QtWidgets/QWidget> // input_fields.h

#include "base/timer.h"
#include "spellcheck/platform/platform_spellcheck.h"
#include "spellcheck/spellcheck_types.h"
#include "ui/widgets/input_fields.h"

#include <QtGui/QSyntaxHighlighter>
#include <QtGui/QTextBlock>
#include <QtWidgets/QMenu>
#include <QtWidgets/QTextEdit>

#include <rpl/event_stream.h>

namespace Ui {
struct ExtendedContextMenu;
} // namespace Ui

namespace Spellchecker {

class SpellingHighlighter final : public QSyntaxHighlighter {

public:
	struct CustomContextMenuItem {
		QString title;
		Fn<void()> callback;
	};

	SpellingHighlighter(
		not_null<Ui::InputField*> field,
		rpl::producer<bool> enabled,
		std::optional<CustomContextMenuItem> customContextMenuItem
			= std::nullopt);

	void contentsChange(int pos, int removed, int added);
	void checkCurrentText();
	bool enabled();

	auto contextMenuCreated() {
		return _contextMenuCreated.events();
	}

	// Windows system spellchecker forces us to perform spell operations
	// In another thread, so the word check and getting a list of suggestions
	// Are run asynchronously.
	// And then the context menu is filled in the main thread.
	void addSpellcheckerActions(
		not_null<QMenu*> parentMenu,
		QTextCursor cursorForPosition,
		Fn<void()> showMenuCallback,
		QPoint mousePosition);

protected:
	void highlightBlock(const QString &text) override;
	bool eventFilter(QObject *o, QEvent *e) override;

private:
	void updatePalette();
	void setEnabled(bool enabled);
	void checkText(const QString &text);

	void invokeCheckText(
		int textPosition,
		int textLength,
		Fn<void(MisspelledWords &&ranges)> callback);

	void checkChangedText();
	void checkSingleWord(const MisspelledWord &singleWord);
	MisspelledWords filterSkippableWords(MisspelledWords &ranges);
	bool isSkippableWord(const MisspelledWord &range);
	bool isSkippableWord(int position, int length);

	bool hasUnspellcheckableTag(int begin, int length);
	MisspelledWord getWordUnderPosition(int position);

	QString documentText();
	void updateDocumentText();
	QString partDocumentText(int pos, int length);
	int compareDocumentText(const QString &text, int textPos, int textLen);
	QString _lastPlainText;

	std::vector<QTextBlock> blocksFromRange(int pos, int length);

	int size();
	QTextBlock findBlock(int pos);

	int _countOfCheckingTextAsync = 0;

	QTextCharFormat _misspelledFormat;
	QTextCursor _cursor;

	MisspelledWords _cachedRanges;
	EntitiesInText _cachedSkippableEntities;

	int _addedSymbols = 0;
	int _removedSymbols = 0;
	int _lastPosition = 0;
	bool _enabled = true;

	bool _isLastKeyRepeat = false;

	base::Timer _coldSpellcheckingTimer;

	not_null<Ui::InputField*> _field;
	not_null<QTextEdit*> _textEdit;

	const std::optional<CustomContextMenuItem> _customContextMenuItem;

	rpl::lifetime _lifetime;

	using ContextMenu = Ui::InputField::ExtendedContextMenu;
	rpl::event_stream<ContextMenu> _contextMenuCreated;

};

} // namespace Spellchecker
