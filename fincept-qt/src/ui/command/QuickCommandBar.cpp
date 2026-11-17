#include "ui/command/QuickCommandBar.h"

#include "app/DockScreenRouter.h"
#include "app/TerminalShell.h"
#include "ui/theme/Theme.h"
#include "app/WindowFrame.h"
#include "core/actions/ActionDef.h"
#include "core/actions/ActionRegistry.h"
#include "core/keys/WindowCycler.h"
#include "core/logging/Logger.h"
#include "services/instruments/InstrumentResolver.h"
#include "ui/command/CommandParser.h"
#include "ui/notifications/NotificationService.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>

namespace fincept::ui {

QuickCommandBar::QuickCommandBar(QWidget* parent) : QFrame(parent) {
    setObjectName("QuickCommandBar");
    setFrameShape(QFrame::NoFrame);
    setStyleSheet(QString("QFrame#QuickCommandBar {"
                          "  background: %1;"
                          "  border-top: 1px solid %2;"
                          "}"
                          "QLineEdit {"
                          "  background: %3; color: %4;"
                          "  border: 1px solid %2; padding: 4px 8px;"
                          "}"
                          "QLabel { color: %5; }")
                      .arg(colors::BG_SURFACE(), colors::BORDER_MED(), colors::BG_RAISED(), colors::TEXT_PRIMARY(),
                           colors::TEXT_SECONDARY()));
    metrics::follow_fixed_height(this, 28); // grows with the user's font

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(8, 2, 8, 2);
    hl->setSpacing(8);

    auto* prompt = new QLabel(">", this);
    prompt->setStyleSheet(QString("color: %1; font-weight: 700;").arg(colors::AMBER()));
    hl->addWidget(prompt);

    input_ = new QLineEdit(this);
    input_->setPlaceholderText(tr("Type a command (e.g. 'layout switch \"Morning\"', AAPL, ?). Esc to dismiss."));
    connect(input_, &QLineEdit::returnPressed, this, &QuickCommandBar::on_submit);
    hl->addWidget(input_, /*stretch=*/1);

    hint_ = new QLabel(this);
    hint_->setStyleSheet(QString("color: %1; font-size: " + fonts::px(-4) + "px;").arg(colors::TEXT_SECONDARY()));
    hl->addWidget(hint_);

    hide(); // toggle to show
}

void QuickCommandBar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QFrame::changeEvent(event);
}

void QuickCommandBar::retranslateUi() {
    // Only the persistent placeholder is re-applied here. Hint text is transient
    // (set by show_hint in response to user actions) and picks up the new locale
    // the next time it is shown.
    if (input_)
        input_->setPlaceholderText(tr("Type a command (e.g. 'layout switch \"Morning\"', AAPL, ?). Esc to dismiss."));
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

void QuickCommandBar::surface_with(const QString& text, const QString& hint) {
    surface();
    if (input_) {
        input_->setText(text);
        input_->setCursorPosition(text.size());
    }
    if (!hint.isEmpty())
        show_hint(hint, false);
}

void QuickCommandBar::show_hint(const QString& text, bool is_error) {
    if (!hint_)
        return;
    hint_->setText(text);
    hint_->setStyleSheet(QString("color: %1; font-size: " + fonts::px(-4) + "px;")
                             .arg(is_error ? colors::DANGER() : colors::TEXT_SECONDARY()));
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
            show_hint(tr("Help: type any verb (e.g. 'layout switch') or a ticker (AAPL)."), false);
            return;
        case ParsedCommand::Kind::Symbol: {
            // A single word that names a screen ("portfolio", "settings", "help") opens it.
            // Every such word used to be published as a ticker to link group A, overwriting
            // that group's symbol and reporting success.
            const QString word = parsed.args.value("symbol").toString();
            WindowFrame* frame = WindowCycler::instance().focused_frame();
            if (frame && frame->dock_router()) {
                for (const QString& id : frame->dock_router()->all_screen_ids()) {
                    if (id.contains(QLatin1Char('#')))
                        continue;
                    const QString title = DockScreenRouter::title_for_id(id);
                    QString compact_title = title;
                    compact_title.remove(QLatin1Char(' '));
                    if (id.compare(word, Qt::CaseInsensitive) == 0 || title.compare(word, Qt::CaseInsensitive) == 0 ||
                        compact_title.compare(word, Qt::CaseInsensitive) == 0) {
                        frame->dock_router()->navigate(id);
                        show_hint(tr("Opened %1").arg(title.isEmpty() ? id : title), false);
                        input_->clear();
                        return;
                    }
                }
            }
            // Longer than any exchange ticker: a typo'd command, not a symbol.
            if (word.size() > 12) {
                show_hint(tr("Unknown command: '%1'").arg(raw), true);
                return;
            }
            // Otherwise a ticker: resolve it through the shared resolver first (local index at
            // once, backend / Yahoo after — asynchronous, never blocks), then publish the
            // instrument's market-data symbol to group A (pragmatic v1 UX; Phase 9 follow-up
            // adds smarter routing). Typed text is never published as is: "TCS" is also TECSYS
            // on the TSX, "RELIANCE" quotes nothing on Yahoo without ".NS".
            const QString typed = parsed.args.value("symbol").toString();
            show_hint(tr("Looking up %1…").arg(typed), false);
            if (resolve_id_ != 0)
                fincept::services::InstrumentResolver::instance().cancel(resolve_id_);
            QPointer<WindowFrame> frame_guard = frame;
            resolve_id_ = fincept::services::InstrumentResolver::instance().resolve(
                typed, fincept::services::ResolveOptions{}, this,
                [this, typed, frame_guard](const fincept::services::ResolveResult& res) {
                    resolve_id_ = 0;
                    if (!res.exact || res.matches.isEmpty() || !fincept::services::is_quotable(res.matches.first())) {
                        QStringList options;
                        for (int i = 0; i < res.matches.size() && options.size() < 3; ++i)
                            if (fincept::services::is_quotable(res.matches.at(i)))
                                options << res.matches.at(i).symbol;
                        show_hint(options.isEmpty()
                                      ? tr("No symbol matches '%1'").arg(typed)
                                      : tr("'%1' is more than one listing (%2): type the exchange form")
                                            .arg(typed, options.join(QStringLiteral(", "))),
                                  true);
                        return;
                    }
                    const auto& inst = res.matches.first();
                    fincept::services::InstrumentResolver::instance().remember_pick(QStringLiteral("quick_command"),
                                                                                     inst);
                    CommandContext ctx;
                    ctx.shell = &TerminalShell::instance();
                    ctx.focused_frame = frame_guard.data();
                    ctx.args.insert(QStringLiteral("group"), "A");
                    ctx.args.insert(QStringLiteral("symbol"), inst.symbol);
                    auto r = ActionRegistry::instance().invoke("link.publish_to_group", ctx);
                    if (r.is_err()) {
                        show_hint(QString::fromStdString(r.error()), true);
                    } else {
                        // "TATAMOTORS is now TMPV" and similar notes ride along.
                        show_hint(res.note.isEmpty() ? tr("Published %1 to group A").arg(inst.symbol)
                                                     : tr("Published %1 to group A · %2").arg(inst.symbol, res.note),
                                  false);
                        input_->clear();
                    }
                });
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
                ToastService::instance().post(ToastService::Severity::Warning, QString::fromStdString(r.error()),
                                              "command_bar");
            } else {
                show_hint(QString("✓ %1").arg(parsed.action_id), false);
                input_->clear();
            }
            return;
        }
        case ParsedCommand::Kind::Unknown:
            show_hint(parsed.error.isEmpty() ? tr("Unknown command") : parsed.error, true);
            return;
    }
}

} // namespace fincept::ui
