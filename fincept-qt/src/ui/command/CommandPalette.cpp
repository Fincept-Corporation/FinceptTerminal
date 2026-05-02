#include "ui/command/CommandPalette.h"

#include "app/TerminalShell.h"
#include "app/WindowFrame.h"
#include "core/actions/ActionDef.h"
#include "core/actions/ActionRegistry.h"
#include "core/keys/WindowCycler.h"
#include "ui/command/CommandParser.h"
#include "ui/command/SuggestionIndex.h"
#include "ui/notifications/NotificationService.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>

namespace fincept::ui {

void CommandPalette::show_for(QWidget* parent_frame) {
    auto* dlg = new CommandPalette(parent_frame);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(560, 380);
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
    setStyleSheet(
        "QDialog { background: #111827; border: 1px solid #374151; }"
        "QLineEdit { background: #1f2937; color: #e5e7eb; border: 1px solid #374151; "
        "            padding: 8px 12px; font-size: 13px; font-family: 'Consolas', monospace; }"
        "QListWidget { background: #111827; color: #e5e7eb; border: none; }"
        "QListWidget::item { padding: 6px 10px; }"
        "QListWidget::item:hover { background: #1f2937; }"
        "QListWidget::item:selected { background: #1f2937; color: #d97706; }");

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    input_ = new QLineEdit(this);
    input_->setPlaceholderText("Search actions, layouts… (Esc to cancel, Enter to run)");
    connect(input_, &QLineEdit::textChanged, this, &CommandPalette::on_text_changed);
    connect(input_, &QLineEdit::returnPressed, this, &CommandPalette::on_accept);
    vl->addWidget(input_);

    suggestions_ = new QListWidget(this);
    suggestions_->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(suggestions_, &QListWidget::itemActivated, this,
            [this](QListWidgetItem*) { on_accept(); });
    vl->addWidget(suggestions_, /*stretch=*/1);

    // Initial population: first ~25 actions so the user sees something.
    on_text_changed({});
}

void CommandPalette::on_text_changed(const QString& text) {
    if (!suggestions_) return;
    suggestions_->clear();
    auto matches = SuggestionIndex::instance().query(
        text.isEmpty() ? QStringLiteral("a") : text, /*limit=*/25);
    if (matches.isEmpty() && !text.isEmpty()) {
        // Empty query branch: still show top actions by walking the registry.
        for (const QString& id : ActionRegistry::instance().all_ids()) {
            const auto* def = ActionRegistry::instance().find(id);
            if (!def) continue;
            auto* item = new QListWidgetItem(def->display.isEmpty() ? def->id : def->display);
            item->setData(Qt::UserRole, def->id);
            suggestions_->addItem(item);
            if (suggestions_->count() >= 25) break;
        }
    } else {
        for (const auto& m : matches) {
            auto* item = new QListWidgetItem(m.display);
            item->setData(Qt::UserRole, m.id);
            // Decorate the item with category in a smaller secondary line.
            item->setToolTip(QString("%1  [%2]").arg(m.id, m.category));
            suggestions_->addItem(item);
        }
    }
    if (suggestions_->count() > 0)
        suggestions_->setCurrentRow(0);
}

void CommandPalette::on_accept() {
    auto* item = suggestions_->currentItem();
    QString action_id;
    if (item) {
        action_id = item->data(Qt::UserRole).toString();
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
    CommandContext ctx;
    ctx.shell = &TerminalShell::instance();
    ctx.focused_frame = WindowCycler::instance().focused_frame();
    auto r = ActionRegistry::instance().invoke(action_id, ctx);
    if (r.is_err()) {
        ToastService::instance().post(
            ToastService::Severity::Warning,
            QString::fromStdString(r.error()),
            "palette");
    }
    accept();
}

} // namespace fincept::ui
