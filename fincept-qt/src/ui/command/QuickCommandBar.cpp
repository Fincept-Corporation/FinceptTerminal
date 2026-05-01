#include "ui/command/QuickCommandBar.h"

#include "app/TerminalShell.h"
#include "app/WindowFrame.h"
#include "core/actions/ActionDef.h"
#include "core/actions/ActionRegistry.h"
#include "core/keys/WindowCycler.h"
#include "core/logging/Logger.h"
#include "ui/command/CommandParser.h"
#include "ui/notifications/NotificationService.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>

namespace fincept::ui {

QuickCommandBar::QuickCommandBar(QWidget* parent) : QFrame(parent) {
    setObjectName("QuickCommandBar");
    setFrameShape(QFrame::NoFrame);
    setStyleSheet(
        "QFrame#QuickCommandBar {"
        "  background: #0f172a;"
        "  border-top: 1px solid #374151;"
        "}"
        "QLineEdit {"
        "  background: #111827; color: #e5e7eb;"
        "  border: 1px solid #374151; padding: 4px 8px;"
        "  font-family: 'Consolas', monospace;"
        "}"
        "QLabel { color: #9ca3af; }");
    setFixedHeight(28);

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(8, 2, 8, 2);
    hl->setSpacing(8);

    auto* prompt = new QLabel(">", this);
    prompt->setStyleSheet("color: #d97706; font-weight: 700;");
    hl->addWidget(prompt);

    input_ = new QLineEdit(this);
    input_->setPlaceholderText("Type a command (e.g. 'layout switch \"Morning\"', AAPL, ?). Esc to dismiss.");
    connect(input_, &QLineEdit::returnPressed, this, &QuickCommandBar::on_submit);
    hl->addWidget(input_, /*stretch=*/1);

    hint_ = new QLabel(this);
    hint_->setStyleSheet("color: #9ca3af; font-size: 10px;");
    hl->addWidget(hint_);

    hide(); // toggle to show
}

void QuickCommandBar::toggle_visible() {
    if (isVisible())
        hide();
    else
        surface();
}

void QuickCommandBar::surface() {
    show();
    if (input_) {
        input_->clear();
        input_->setFocus(Qt::ShortcutFocusReason);
    }
    if (hint_)
        hint_->clear();
}

void QuickCommandBar::show_hint(const QString& text, bool is_error) {
    if (!hint_) return;
    hint_->setText(text);
    hint_->setStyleSheet(is_error ? "color: #dc2626; font-size: 10px;"
                                  : "color: #9ca3af; font-size: 10px;");
}

void QuickCommandBar::on_submit() {
    const QString raw = input_->text().trimmed();
    if (raw.isEmpty())
        return;

    const auto parsed = CommandParser::parse(raw);
    switch (parsed.kind) {
        case ParsedCommand::Kind::Empty:
            return;
        case ParsedCommand::Kind::Help:
            show_hint("Help: type any verb (e.g. 'layout switch') or a ticker (AAPL).", false);
            return;
        case ParsedCommand::Kind::Symbol: {
            // Route to link.publish_to_group with the first enabled group
            // — pragmatic v1 UX: typing a ticker into the command bar
            // broadcasts to the current focused panel's link group when
            // possible, else to group A. Phase 9 follow-up adds smarter
            // routing (focused panel's group preferred).
            CommandContext ctx;
            ctx.shell = &TerminalShell::instance();
            ctx.focused_frame = WindowCycler::instance().focused_frame();
            ctx.args.insert(QStringLiteral("group"), "A");
            ctx.args.insert(QStringLiteral("symbol"), parsed.args.value("symbol").toString());
            auto r = ActionRegistry::instance().invoke("link.publish_to_group", ctx);
            if (r.is_err()) {
                show_hint(QString::fromStdString(r.error()), true);
            } else {
                show_hint(QString("Published %1 to group A").arg(parsed.args.value("symbol").toString()), false);
                input_->clear();
            }
            return;
        }
        case ParsedCommand::Kind::Action: {
            CommandContext ctx;
            ctx.shell = &TerminalShell::instance();
            ctx.focused_frame = WindowCycler::instance().focused_frame();
            ctx.args = parsed.args;
            auto r = ActionRegistry::instance().invoke(parsed.action_id, ctx);
            if (r.is_err()) {
                show_hint(QString::fromStdString(r.error()), true);
                ToastService::instance().post(
                    ToastService::Severity::Warning,
                    QString::fromStdString(r.error()),
                    "command_bar");
            } else {
                show_hint(QString("✓ %1").arg(parsed.action_id), false);
                input_->clear();
            }
            return;
        }
        case ParsedCommand::Kind::Unknown:
            show_hint(parsed.error.isEmpty() ? "Unknown command" : parsed.error, true);
            return;
    }
}

} // namespace fincept::ui
