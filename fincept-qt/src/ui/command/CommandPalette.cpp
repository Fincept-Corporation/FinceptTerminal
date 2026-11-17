#include "ui/command/CommandPalette.h"
#include "ui/theme/Theme.h"

#include "app/DockScreenRouter.h"
#include "app/TerminalShell.h"
#include "app/WindowFrame.h"
#include "core/actions/ActionDef.h"
#include "core/actions/ActionRegistry.h"
#include "core/keys/WindowCycler.h"
#include "core/layout/LayoutCatalog.h"
#include "core/layout/WorkspaceShell.h"
#include "ui/command/CommandParser.h"
#include "ui/command/QuickCommandBar.h"
#include "ui/command/SuggestionIndex.h"
#include "ui/notifications/NotificationService.h"
#include "ui/widgets/ThemeHelpers.h"
#include "ui/workspace/LayoutOpenDialog.h"
#include "ui/workspace/LayoutSaveAsDialog.h"

#include <QDir>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>

namespace fincept::ui {

void CommandPalette::show_for(QWidget* parent_frame) {
    auto* dlg = new CommandPalette(parent_frame);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(metrics::width(560), metrics::width(380)); // widened with the user's font
    if (parent_frame) {
        const QPoint centre = parent_frame->mapToGlobal(parent_frame->rect().center());
        dlg->move(centre - QPoint(dlg->width() / 2, dlg->height() / 3));
    }
    dlg->show();
    dlg->input_->setFocus(Qt::ShortcutFocusReason);
}

CommandPalette::CommandPalette(QWidget* parent) : QDialog(parent) {
    setWindowFlag(Qt::FramelessWindowHint);
    setWindowFlag(Qt::Popup);
    setStyleSheet(QString("QDialog { background: %1; border: 1px solid %2; }"
                          "QLineEdit { background: %3; color: %4; border: 1px solid %2; "
                          "            padding: 8px 12px; font-size: " +
                          fonts::px(-1) +
                          "px; }"
                          "QLineEdit:focus { border-color: %6; }"
                          "QListWidget { background: %1; color: %4; border: none; }"
                          "QListWidget::item { padding: 6px 10px; }"
                          "QListWidget::item:hover { background: %5; }"
                          "QListWidget::item:selected { background: %5; color: %6; }")
                      .arg(colors::BG_SURFACE(), colors::BORDER_MED(), colors::BG_RAISED(), colors::TEXT_PRIMARY(),
                           colors::BG_HOVER(), colors::AMBER()));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    input_ = new QLineEdit(this);
    input_->setPlaceholderText(tr("Search actions, layouts… (Esc to cancel, Enter to run)"));
    connect(input_, &QLineEdit::textChanged, this, &CommandPalette::on_text_changed);
    connect(input_, &QLineEdit::returnPressed, this, &CommandPalette::on_accept);
    vl->addWidget(input_);

    suggestions_ = new QListWidget(this);
    suggestions_->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(suggestions_, &QListWidget::itemActivated, this, [this](QListWidgetItem*) { on_accept(); });
    vl->addWidget(suggestions_, /*stretch=*/1);

    // Initial population: first ~25 actions so the user sees something.
    on_text_changed({});
}

void CommandPalette::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void CommandPalette::retranslateUi() {
    if (input_)
        input_->setPlaceholderText(tr("Search actions, layouts… (Esc to cancel, Enter to run)"));
}

namespace {

// What a palette row runs. Layout rows used to carry a layout UUID that was
// invoked as an ACTION id ("Unknown action: <uuid>").
constexpr int kKindRole = Qt::UserRole + 1;
enum PaletteKind : int { kKindAction = 0, kKindLayout = 1, kKindScreen = 2 };

// The frame the palette was opened over (see on_accept for why not the cycler alone).
WindowFrame* palette_frame(QWidget* parent) {
    if (auto* f = qobject_cast<WindowFrame*>(parent))
        return f;
    return WindowCycler::instance().focused_frame();
}

// Arguments the palette has to ask for before running `action_id` (it has no argument
// row of its own). Layout actions get the same pickers the File menu uses; any other
// action with a required argument is handed to the quick command bar, prefilled, so
// the user can type it. Returns false when there is nothing to run now (cancelled, or
// handed over).
bool collect_action_args(const QString& action_id, WindowFrame* frame, CommandContext& ctx) {
    QWidget* parent = frame;
    if (action_id == QLatin1String("layout.save")) {
        const QString current = fincept::layout::WorkspaceShell::current_name();
        if (!current.isEmpty()) {
            ctx.args.insert(QStringLiteral("name"), current);
            return true;
        }
    }
    if (action_id == QLatin1String("layout.save") || action_id == QLatin1String("layout.save_as") ||
        action_id == QLatin1String("layout.new")) {
        // "New Layout" used to add another empty layout called "New Layout" every time.
        LayoutSaveAsDialog dlg(parent);
        if (dlg.exec() != QDialog::Accepted || dlg.name().trimmed().isEmpty())
            return false;
        ctx.args.insert(QStringLiteral("name"), dlg.name().trimmed());
        return true;
    }
    if (action_id == QLatin1String("layout.switch") || action_id == QLatin1String("layout.export")) {
        LayoutOpenDialog dlg(parent);
        if (dlg.exec() != QDialog::Accepted || dlg.selected_id().is_null())
            return false;
        if (action_id == QLatin1String("layout.switch")) {
            ctx.args.insert(QStringLiteral("name"), dlg.selected_id().to_string());
            return true;
        }
        // layout.export looks the layout up by name.
        QString name;
        const auto layouts = LayoutCatalog::instance().list_layouts();
        if (layouts.is_ok()) {
            for (const auto& e : layouts.value()) {
                if (e.id == dlg.selected_id()) {
                    name = e.name;
                    break;
                }
            }
        }
        if (name.isEmpty())
            return false;
        const QString path = QFileDialog::getSaveFileName(parent, CommandPalette::tr("Export Layout"),
                                                          QDir::homePath() + QLatin1Char('/') + name + ".json",
                                                          CommandPalette::tr("Fincept Layout (*.json);;All Files (*)"));
        if (path.isEmpty())
            return false;
        ctx.args.insert(QStringLiteral("name"), name);
        ctx.args.insert(QStringLiteral("path"), path);
        return true;
    }
    if (action_id == QLatin1String("layout.import")) {
        const QString path =
            QFileDialog::getOpenFileName(parent, CommandPalette::tr("Import Layout"), QDir::homePath(),
                                         CommandPalette::tr("Fincept Layout (*.json *.flayout *.fwsp);;All Files (*)"));
        if (path.isEmpty())
            return false;
        ctx.args.insert(QStringLiteral("path"), path);
        return true;
    }

    const ActionDef* def = ActionRegistry::instance().find(action_id);
    if (!def)
        return true; // invoke() reports the unknown id
    QStringList missing;
    for (const ParameterSlot& slot : def->parameter_slots) {
        if (ctx.args.contains(slot.name))
            continue;
        if (slot.default_value.isValid()) {
            ctx.args.insert(slot.name, slot.default_value);
            continue;
        }
        if (slot.required)
            missing << (slot.display.isEmpty() ? slot.name : slot.display);
    }
    if (missing.isEmpty())
        return true;
    const QString typed = QString(action_id).replace(QLatin1Char('.'), QLatin1Char(' ')) + QLatin1Char(' ');
    const QString hint = CommandPalette::tr("Type: %1").arg(missing.join(QStringLiteral(", ")));
    if (auto* bar = frame ? frame->findChild<QuickCommandBar*>() : nullptr)
        bar->surface_with(typed, hint);
    else
        ToastService::instance().post(ToastService::Severity::Info,
                                      CommandPalette::tr("%1 needs: %2").arg(def->display, missing.join(", ")),
                                      "palette");
    return false;
}

} // namespace

void CommandPalette::on_text_changed(const QString& text) {
    if (!suggestions_)
        return;
    suggestions_->clear();
    auto matches = SuggestionIndex::instance().query(text.isEmpty() ? QStringLiteral("a") : text, /*limit=*/25);
    if (matches.isEmpty() && text.isEmpty()) {
        // Empty query (nothing typed): still show top actions by walking the
        // registry. A non-empty query with no match must stay EMPTY — listing
        // unrelated actions made Enter run whichever happened to be first.
        for (const QString& id : ActionRegistry::instance().all_ids()) {
            const auto* def = ActionRegistry::instance().find(id);
            if (!def)
                continue;
            auto* item = new QListWidgetItem(def->display.isEmpty() ? def->id : def->display);
            item->setData(Qt::UserRole, def->id);
            item->setData(kKindRole, kKindAction);
            suggestions_->addItem(item);
            if (suggestions_->count() >= 25)
                break;
        }
    } else {
        for (const auto& m : matches) {
            auto* item = new QListWidgetItem(m.display);
            item->setData(Qt::UserRole, m.id);
            item->setData(kKindRole, m.source == SuggestionIndex::SourceKind::Layout ? kKindLayout : kKindAction);
            // Decorate the item with category in a smaller secondary line.
            item->setToolTip(QString("%1  [%2]").arg(m.id, m.category));
            suggestions_->addItem(item);
        }
    }

    // Screens: the palette had no way to open one ("portfolio", "settings" found nothing).
    const QString q = text.trimmed();
    WindowFrame* frame = palette_frame(parentWidget());
    if (!q.isEmpty() && frame && frame->dock_router()) {
        for (const QString& id : frame->dock_router()->all_screen_ids()) {
            if (suggestions_->count() >= 30)
                break;
            if (id.contains(QLatin1Char('#')))
                continue;
            const QString title = DockScreenRouter::title_for_id(id);
            if (!title.contains(q, Qt::CaseInsensitive) && !id.contains(q, Qt::CaseInsensitive))
                continue;
            auto* item = new QListWidgetItem(tr("Go to %1").arg(title.isEmpty() ? id : title));
            item->setData(Qt::UserRole, id);
            item->setData(kKindRole, kKindScreen);
            item->setToolTip(QString("%1  [%2]").arg(id, tr("Screen")));
            suggestions_->addItem(item);
        }
    }

    if (suggestions_->count() > 0)
        suggestions_->setCurrentRow(0);
}

void CommandPalette::on_accept() {
    auto* item = suggestions_->currentItem();
    QString action_id;
    int kind = kKindAction;
    if (item) {
        action_id = item->data(Qt::UserRole).toString();
        kind = item->data(kKindRole).toInt();
    } else {
        // No suggestion selected — try parsing the raw input as a command.
        const auto parsed = CommandParser::parse(input_->text());
        if (parsed.kind == ParsedCommand::Kind::Action)
            action_id = parsed.action_id;
    }
    if (action_id.isEmpty()) {
        accept();
        return;
    }
    // The palette is parented to the frame it was opened over; run the action
    // there. WindowCycler::focused_frame() cannot be used on its own: while this
    // popup is the active window it falls back to the FIRST registered frame,
    // so in a multi-window setup the action ran against the wrong window.
    CommandContext ctx;
    ctx.shell = &TerminalShell::instance();
    ctx.focused_frame = palette_frame(parentWidget());
    // Dismiss the popup first so actions that open their own dialogs (layout
    // save-as, file pickers, ...) don't do so from inside a live Qt::Popup.
    accept();

    if (kind == kKindScreen) {
        if (ctx.focused_frame && ctx.focused_frame->dock_router())
            ctx.focused_frame->dock_router()->navigate(action_id);
        return;
    }
    if (kind == kKindLayout) {
        // A layout row opens that layout (layout.switch accepts its UUID).
        ctx.args.insert(QStringLiteral("name"), action_id);
        action_id = QStringLiteral("layout.switch");
    } else if (!collect_action_args(action_id, ctx.focused_frame, ctx)) {
        return;
    }
    auto r = ActionRegistry::instance().invoke(action_id, ctx);
    if (r.is_err()) {
        ToastService::instance().post(ToastService::Severity::Warning, QString::fromStdString(r.error()), "palette");
    }
}

} // namespace fincept::ui
