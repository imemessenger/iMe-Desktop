// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//

#include "spellcheck/spelling_highlighter.h"

#include "ui/platform/ui_platform_utility.h"
#include "styles/style_widgets.h"
#include "styles/palette.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>

namespace Spelling::Helper {
namespace {

const auto kFormattingItem = 1;
const auto kSpellingItem = 1;

} // namespace

bool IsContextMenuTop(not_null<QMenu*> menu, QPoint mousePosition) {
	const auto &st = st::defaultMenu;
	const auto &stPopup = st::defaultPopupMenu;

	const auto itemHeight = st.itemPadding.top()
		+ st.itemStyle.font->height
		+ st.itemPadding.bottom();
	const auto sepHeight = st.separatorPadding.top()
		+ st.separatorWidth
		+ st.separatorPadding.bottom();

	const auto line = st::lineWidth;
	const auto p = Ui::Platform::TranslucentWindowsSupported(mousePosition)
		? stPopup.shadow.extend
		: style::margins(line, line, line, line);

	const auto additional = kFormattingItem + kSpellingItem;
	const auto actions = menu->actions() | ranges::to_vector;
	auto sepCount = ranges::count_if(actions, &QAction::isSeparator);
	auto itemsCount = actions.size() - sepCount;
	sepCount += additional;
	itemsCount += additional;

	const auto w = mousePosition - QPoint(0, p.top());
	const auto screen = QGuiApplication::screenAt(mousePosition);
	if (!screen) {
		return false;
	}
	const auto r = screen->availableGeometry();
	const auto height = itemHeight * itemsCount
		+ sepHeight * sepCount
		+ p.top()
		+ stPopup.scrollPadding.top()
		+ stPopup.scrollPadding.bottom()
		+ p.bottom();

	return (w.y() + height - p.bottom() > r.y() + r.height());
}

} // namespace Spelling::Helper
