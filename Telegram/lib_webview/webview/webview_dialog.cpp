// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "webview/webview_dialog.h"

#include "webview/webview_interface.h"
#include "ui/widgets/separate_panel.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/input_fields.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/integration.h"
#include "base/invoke_queued.h"
#include "base/unique_qptr.h"
#include "base/integration.h"
#include "styles/style_widgets.h"
#include "styles/style_layers.h"

#include <QtCore/QUrl>
#include <QtCore/QString>
#include <QtCore/QEventLoop>

namespace Webview {

DialogResult DefaultDialogHandler(DialogArgs args) {
	// This fixes animations in a nested event loop.
	base::Integration::Instance().enterFromEventLoop([] {});

	auto result = DialogResult();
	auto context = QObject();

	QEventLoop loop;
	auto running = true;
	auto widget = base::unique_qptr<Ui::SeparatePanel>();
	InvokeQueued(&context, [&] {
		widget = base::make_unique_q<Ui::SeparatePanel>(args.parent);
		const auto raw = widget.get();

		raw->setTitle(
			rpl::single(QUrl(QString::fromStdString(args.url)).host()));
		auto layout = base::make_unique_q<Ui::VerticalLayout>(raw);
		const auto skip = st::boxDividerHeight;
		const auto container = layout.get();
		const auto label = container->add(
			object_ptr<Ui::FlatLabel>(
				container,
				rpl::single(QString::fromStdString(args.text)),
				st::boxLabel),
			st::boxRowPadding);
		label->resizeToWidth(st::boxWideWidth
			- st::boxRowPadding.left()
			- st::boxRowPadding.right());
		const auto input = (args.type == DialogType::Prompt)
			? container->add(
				object_ptr<Ui::InputField>(
					container,
					st::defaultInputField,
					rpl::single(QString()),
					QString::fromStdString(args.value)),
				st::boxRowPadding + QMargins(0, 0, 0, skip))
			: nullptr;
		const auto buttonPadding = st::webviewDialogPadding;
		const auto buttonHeight = buttonPadding.top()
			+ st::webviewDialogSubmit.height
			+ buttonPadding.bottom();
		const auto buttons = container->add(
			object_ptr<Ui::FixedHeightWidget>(
				container,
				st::webviewDialogSubmit.height),
			QMargins(
				buttonPadding.left(),
				buttonPadding.top(),
				buttonPadding.left(),
				buttonPadding.bottom()));
		const auto submit = Ui::CreateChild<Ui::RoundButton>(
			buttons,
			rpl::single(Ui::Integration::Instance().phraseButtonOk()),
			st::webviewDialogButton);
		const auto cancel = (args.type != DialogType::Alert)
			? Ui::CreateChild<Ui::RoundButton>(
				buttons,
				rpl::single(Ui::Integration::Instance().phraseButtonCancel()),
				st::webviewDialogButton)
			: nullptr;
		buttons->widthValue(
		) | rpl::start_with_next([=](int width) {
			submit->moveToRight(0, 0, width);
			if (cancel) {
				cancel->moveToRight(
					submit->width() + st::webviewDialogPadding.right(),
					0,
					width);
			}
		}, buttons->lifetime());
		const auto height = st::separatePanelTitleHeight
			+ label->height()
			+ (input ? (skip + input->height()) : 0)
			+ buttonHeight;
		raw->setInnerSize({ st::boxWideWidth, height });
		container->resizeToWidth(st::boxWideWidth);

		const auto confirm = [=, &result] {
			result.accepted = true;
			if (input) {
				result.text = input->getLastText().toStdString();
			}
			raw->hideGetDuration();
		};

		if (input) {
			input->selectAll();
			input->setFocusFast();
			QObject::connect(input, &Ui::InputField::submitted, confirm);
		}
		container->events(
		) | rpl::start_with_next([=](not_null<QEvent*> event) {
			if (input && event->type() == QEvent::FocusIn) {
				input->setFocus();
			}
		}, container->lifetime());

		raw->setWindowFlag(Qt::WindowStaysOnTopHint, false);
		raw->setAttribute(Qt::WA_DeleteOnClose, false);
		raw->setAttribute(Qt::WA_ShowModal, true);

		raw->closeRequests() | rpl::start_with_next([=] {
			raw->hideGetDuration();
		}, raw->lifetime());

		submit->setClickedCallback(confirm);

		if (cancel) {
			cancel->setClickedCallback([=] {
				raw->hideGetDuration();
			});
		}

		const auto finish = [&] {
			if (running) {
				running = false;
				loop.quit();
			}
		};
		QObject::connect(raw, &QObject::destroyed, finish);
		raw->closeEvents() | rpl::start_with_next(finish, raw->lifetime());

		raw->showInner(std::move(layout));
	});
	loop.exec(QEventLoop::DialogExec);
	widget = nullptr;

	return result;
}

} // namespace Webview
