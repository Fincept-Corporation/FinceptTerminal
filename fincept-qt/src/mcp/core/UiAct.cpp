// UiAct.cpp — ui_act: operate the terminal's controls like a person.
//
// One call runs up to twelve steps in order and stops at the first failure.
// Each step resolves its element (ref, or the words on it), refuses what a
// person could not do (disabled / hidden / covered / behind a modal dialog / a
// secret field), asks the user before anything risky (orders, money, deletes,
// spends, sign out), then performs the action ASYNCHRONOUSLY: the widget
// operation is posted to the event loop, so a click that opens a modal dialog
// (QDialog::exec) or a menu (QMenu::exec) never blocks the call — the settle
// watcher runs inside that nested loop, sees the dialog and reports it with its
// refs. While a step performs, file / colour / font dialogs open as Qt widget
// dialogs (not native ones), so a dialog an agent's click opens can be read and
// answered like any other.
//
// A batch may also hold observe steps — {action:"wait", until:…} and
// {action:"read", ref:…} — so "set A, set B, click RUN, wait for the result,
// read the table" is ONE call.
//
// The answer says what changed: dialogs or menus that opened or closed, the
// element's new value, what a symbol box resolved, row counts of tables in the
// same panel, new texts and toasts, a different focused panel, work still
// loading. Failures start with a code (UiErrors.h).
//
// The run's part of the order-path safety: a PAPER run refuses money steps
// outright, and one call holds at most ONE money step (see UiActRisk.cpp).

#include "core/logging/Logger.h"
#include "mcp/core/CoreApproval.h"
#include "mcp/core/CoreToolsCommon.h"
#include "mcp/core/UiActInternal.h"
#include "mcp/core/UiErrors.h"
#include "mcp/server/CallApproval.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QLineEdit>
#include <QMenu>
#include <QTimer>

#include <exception>
#include <memory>

namespace fincept::mcp::core::ui {

namespace ui_act_detail {

using namespace act;

constexpr const char* kUiActTag = "UiTools";
/// An agent hold outlives the longest settle (a symbol box resolving) a little.
constexpr int kAgentHoldMs = act::kSymbolResolveMs + 2000;

/// "code: sentence" → "code: <prefix>sentence" (the code stays first).
QString prefix_error(const QString& error, const QString& prefix) {
    const QString coded = with_error_code(error);
    const QString code = error_code_of(coded);
    return code + QStringLiteral(": ") + prefix + coded.mid(code.size() + 2);
}

struct ActRun : std::enable_shared_from_this<ActRun> {
    QList<Step> steps;
    QString origin;
    ::fincept::core::agent::AgentActor agent_actor; // the caller's stable ids, taken at entry
    CancelCheck cancelled; // the caller gave up (client cancel / watchdog): stop before the next step
    bool snapshot = false;
    Reply reply;
    QStringList report;
    int index = 0;
    int money_step = -1; // the step that already placed / changed / confirmed an order in this call
    Risk step_risk;      // the running step's risk (dialogs it opens inherit it)
    bool answered = false;
    QPointer<QWidget> last_scope;

    struct SymbolWait {
        bool watching = false;
        bool done = false;
        QString outcome;
    };

    /// The agent hold of one step: created when the step performs, released when
    /// it settles (or when the last copy of this slot goes — never leaked).
    struct AgentHoldSlot {
        bool settled = false;
        std::unique_ptr<::fincept::core::agent::AgentActionHold> hold;
    };

    /// Run a continuation; whatever it throws ends the call with an answer.
    void guarded(const char* where, const std::function<void()>& fn) {
        try {
            fn();
        } catch (const std::exception& e) {
            LOG_ERROR(kUiActTag, logf("ui_act step threw", {{"where", where}, {"error", log_scrub(QString::fromUtf8(e.what()))}}));
            finish(false, err_text(Err::Internal, QStringLiteral("the terminal failed while running this step (%1).")
                                                      .arg(QString::fromUtf8(e.what()).left(160))));
        } catch (...) {
            LOG_ERROR(kUiActTag, logf("ui_act step threw", {{"where", where}}));
            finish(false, err_text(Err::Internal, QStringLiteral("the terminal failed while running this step.")));
        }
    }

    void next() {
        if (answered)
            return;
        if (index >= steps.size()) {
            finish(true, {});
            return;
        }
        const Step s = steps[index];
        if (cancelled && cancelled()) {
            finish(false, err_text(Err::Cancelled, QStringLiteral("the caller gave up; the step was not run.")));
            return;
        }
        if (host::terminal_locked()) {
            finish(false, err_text(Err::Busy, QStringLiteral("The terminal is locked."),
                                   QStringLiteral("the user must unlock it first.")));
            return;
        }
        if (is_observe_action(s.action)) {
            observe(s);
            return;
        }
        QString error;
        std::optional<Target> found = resolve_step_target(s, &error);
        if (!found || !aim_at_point(*found, s, &error)) {
            finish(false, error);
            return;
        }
        if (found->web_view) {
            // On a web page: ask the page what the element is first (async).
            auto self = shared_from_this();
            locate_web(s, *found, [self, s](std::optional<Target> located, const QString& err) {
                self->guarded("locate_web", [&]() {
                    if (self->cancelled && self->cancelled()) {
                        self->finish(false, err_text(Err::Cancelled, QStringLiteral("the caller gave up.")));
                        return;
                    }
                    if (!located) {
                        self->finish(false, err);
                        return;
                    }
                    self->proceed(s, *located);
                });
            });
            return;
        }
        proceed(s, *found);
    }

    /// A wait / read step: the same as ui_wait / ui_read, its answer added to the report.
    void observe(const Step& s) {
        QJsonObject args = s.raw;
        args.remove(QStringLiteral("action"));
        if (!args.contains(QStringLiteral("target")) && !s.target.trimmed().isEmpty())
            args.insert(QStringLiteral("target"), s.target);
        auto self = shared_from_this();
        const bool is_wait = s.action == QLatin1String("wait");
        const int number = index + 1;
        Reply done = [self, is_wait, number, args](ToolResult r) {
            self->guarded("observe", [&]() {
                if (!r.success) {
                    self->finish(false, r.error);
                    return;
                }
                const QString head = is_wait ? QStringLiteral("%1. wait %2: ").arg(number).arg(
                                                   args.value(QStringLiteral("until")).toString())
                                             : QStringLiteral("%1. read %2:\n").arg(number).arg(
                                                   args.value(QStringLiteral("ref")).toString());
                self->report << head + r.message;
                ++self->index;
                self->next();
            });
        };
        if (is_wait) {
            if (!args.contains(QStringLiteral("until")))
                args.insert(QStringLiteral("until"), QStringLiteral("idle"));
            ui_wait(args, cancelled, std::move(done));
        } else {
            if (args.value(QStringLiteral("ref")).toString().trimmed().isEmpty()) {
                finish(false, err_text(Err::InvalidValue, QStringLiteral("a read step needs `ref`.")));
                return;
            }
            ui_read(args, std::move(done));
        }
    }

    void proceed(const Step& s, const Target& found) {
        QString error;
        const std::optional<Target> t = found;
        QWidget* host = host_widget(*t);
        if (!host && t->action) {
            // A menu / toolbar item ref: check where it shows, like any widget.
            // Triggering an item of a closed menu skipped the modal check below.
            for (QObject* o : t->action->associatedObjects()) {
                if (auto* aw = qobject_cast<QWidget*>(o); aw && aw->isVisible()) {
                    host = aw;
                    break;
                }
            }
            if (!host) {
                finish(false, err_text(Err::Obscured, QStringLiteral("%1 is not showing.").arg(describe_target(*t)),
                                       QStringLiteral("open its menu first (ui_snapshot lists what is there).")));
                return;
            }
        }
        if (host && is_hidden_from_agents(host)) {
            finish(false, err_text(Err::Denied, QStringLiteral("That element cannot be operated by an agent.")));
            return;
        }
        // A modal dialog blocks everything outside it, for agents as for people.
        if (QWidget* modal = blocking_dialog(); modal && host) {
            QWidget* top = host->window();
            const bool inside = top == modal || modal->isAncestorOf(host);
            const bool in_popup = qobject_cast<QMenu*>(top) != nullptr;
            if (!inside && !in_popup) {
                finish(false, err_text(Err::Obscured,
                                       QStringLiteral("%1 is open and blocks the rest of the terminal.")
                                           .arg(location_label(modal)),
                                       QStringLiteral("answer it first (ui_snapshot(target=\"dialog\")).")));
                return;
            }
        }
        std::optional<Prepared> prepared = prepare(s, *t, &error);
        if (!prepared) {
            finish(false, error);
            return;
        }
        const Risk risk = assess(s, *t);
        step_risk = risk; // a dialog this step opens inherits it for its accept buttons
        auto self = shared_from_this();
        const Target target = *t;
        const Prepared work = *prepared;
        if (risk.money && is_paper_run_origin(origin)) {
            // A PAPER workflow run: refused outright, never offered for approval.
            log_step(s, target, host, QStringLiteral("refused_paper"), 0, false);
            finish(false, err_text(Err::Denied,
                                   QStringLiteral("Refused: %1 — this workflow runs in PAPER mode, so it cannot place, "
                                                  "change or cancel real orders or move money.")
                                       .arg(work.note),
                                   QStringLiteral("arm LIVE in the workflow's SAVE & RUN to allow it.")));
            return;
        }
        if (is_order_kind(risk) && money_step >= 0) {
            // One order step per call: the agent must see what the terminal
            // prepared (its confirmation) before anything confirms it.
            log_step(s, target, host, QStringLiteral("refused_second_order_step"), 0, false);
            finish(false, err_text(Err::NeedsApproval,
                                   QStringLiteral("%1 would place, change or confirm an order, and step %2 of this call "
                                                  "already did — one ui_act call holds at most one order step.")
                                       .arg(work.note, QString::number(money_step + 1)), // one call: no re-scan
                                   QStringLiteral("read what the terminal shows now (ui_snapshot(target=\"dialog\")), "
                                                  "then confirm it in a separate ui_act call.")));
            return;
        }
        if (risk.risky && !carry_covers(origin, host, risk)) {
            ToolApprovalRequest req;
            req.client = origin;
            // The risk is part of what is approved: a 15-minute grant for "click Yes
            // (deletes data)" must never cover "click Yes" on a live order.
            req.tool = QStringLiteral("ui_act: %1 %2 (%3)").arg(s.action, common::clip(target.name, 60), risk.why);
            req.description = QStringLiteral("%1 in %2 — this %3.").arg(work.note, location_label(host), risk.why);
            QJsonObject preview{{QStringLiteral("action"), s.action},
                                {QStringLiteral("element"), describe_target(target)},
                                {QStringLiteral("where"), location_label(host)}};
            if (!s.text.isEmpty())
                preview.insert(QStringLiteral("text"), common::clip(s.text, 200));
            // What the control would do, in the app's own words (the crypto ticket
            // puts "LIVE BUY LIMIT 0.5 BTC on KRAKEN market BTC/USDT limit 50000"
            // in its submit button's description).
            if (host && !host->accessibleDescription().trimmed().isEmpty())
                preview.insert(QStringLiteral("details"), common::clip(host->accessibleDescription().trimmed(), 300));
            const QString dctx = dialog_context(host);
            if (!dctx.isEmpty())
                preview.insert(QStringLiteral("dialog says"), common::clip(dctx, risk.money ? 1600 : 400));
            req.args = preview;
            req.destructive = true;
            // The kind sets the dialog's heading and its grant policy (ApprovalPolicy.h);
            // an order states LIVE / PAPER only as the screen shows it.
            req.kind = risk.kind;
            req.live_trading = is_order_kind(risk);
            if (is_order_kind(risk))
                req.trading_mode = approval::trading_mode_from_text(
                    (host ? host->accessibleDescription() : QString()) + QLatin1Char('\n') + dctx + QLatin1Char('\n') +
                    trading_mode_words(host));
            req.client_id = agent_actor.client_id;
            req.run_id = agent_actor.run_id;
            req.screen = location_label(host);
            const QString fingerprint = row_fingerprint(target);
            ask_user_approval(req, [self, s, target, work, fingerprint, risk](ToolApprovalVerdict v) {
                self->guarded("approval", [&]() { self->after_approval(v, s, target, work, fingerprint, risk); });
            });
            return;
        }
        execute(s, target, work, QString(), false, is_order_kind(risk));
    }

    void after_approval(ToolApprovalVerdict v, const Step& s, const Target& target, const Prepared& work,
                        const QString& fingerprint, const Risk& risk) {
        if (answered)
            return;
        if (cancelled && cancelled()) {
            finish(false, err_text(Err::Cancelled, QStringLiteral("the caller gave up — the step was not run.")));
            return;
        }
        if (v != ToolApprovalVerdict::Approved) {
            log_step(s, target, host_widget(target),
                     v == ToolApprovalVerdict::Denied ? QStringLiteral("declined") : QStringLiteral("not_approved"), 0,
                     false);
            const QString text = approval_refusal_text(v, work.note);
            finish(false, err_text(v == ToolApprovalVerdict::Denied ? Err::Denied : Err::NeedsApproval, text));
            return;
        }
        // Up to 50 s passed: re-find the element and make sure it is still the
        // one the user approved (a live blotter re-sorts under a row ref, a web
        // page changes under a control ref).
        auto self = shared_from_this();
        auto go = [self, s, target, fingerprint, risk](std::optional<Target> now, const QString&) {
            self->guarded("approved", [&]() {
                if (!now || host_widget(*now) != host_widget(target) || now->action != target.action ||
                    now->row != target.row || row_fingerprint(*now) != fingerprint ||
                    (target.web_view && (now->role != target.role || now->name != target.name))) {
                    self->finish(false, err_text(Err::StaleRef,
                                                 QStringLiteral("%1 changed while the user was approving — nothing "
                                                                "was done.")
                                                     .arg(describe_target(target)),
                                                 QStringLiteral("take a fresh ui_snapshot and try again.")));
                    return;
                }
                QString err;
                std::optional<Prepared> fresh = prepare(s, *now, &err);
                if (!fresh) {
                    self->finish(false, err);
                    return;
                }
                arm_carry(self->origin, risk);
                self->execute(s, *now, *fresh, QStringLiteral(" (approved by the user)"), true, is_order_kind(risk));
            });
        };
        QString err;
        std::optional<Target> now = resolve_step_target(s, &err);
        if (now && !aim_at_point(*now, s, &err))
            now.reset();
        if (now && now->web_view) {
            locate_web(s, *now, go);
            return;
        }
        go(now, err);
    }

    void execute(const Step& s, const Target& t, const Prepared& work, const QString& suffix, bool approved,
                 bool money) {
        QWidget* host = host_widget(t);
        QWidget* scope = change_scope(host);
        last_scope = scope;
        if (money)
            money_step = index;
        const QString target_label = t.ref.isEmpty() ? describe_target(t) : t.ref;
        // Values are read back only for value-bearing steps.
        const bool track_value = s.action == QLatin1String("type") || s.action == QLatin1String("clear") ||
                                 s.action == QLatin1String("select") || s.action == QLatin1String("set_value") ||
                                 s.action == QLatin1String("check") || s.action == QLatin1String("uncheck");
        QPointer<QWidget> value_widget(track_value && !qobject_cast<QAbstractItemView*>(host) ? host : nullptr);
        const UiState before = capture(scope, value_widget);
        // Close a popup the step is not about (a click elsewhere closes it for a person too).
        if (QWidget* popup = open_popup(); popup && host && popup != host->window() && !t.action)
            popup->close();

        // A symbol search box resolves what was typed asynchronously: listen
        // before the keys go in, report what it resolved (or could not).
        auto symbol = std::make_shared<SymbolWait>();
        if (work.symbol_box && work.field) {
            symbol->watching = host::watch_symbol_box(work.field, kSymbolResolveMs, [symbol](const QString& outcome) {
                symbol->done = true;
                symbol->outcome = outcome;
            });
        }

        // Posted, never inline: a modal dialog or exec()'d menu opened by this
        // step must not hold the call (see the file comment). File / colour /
        // font dialogs it opens are Qt dialogs the agent can operate.
        //
        // The step runs as the agent's action (core/agent/AgentActionScope.h):
        // its synchronous work under a scope, and the deferred work it causes
        // (a follow-up posted by the click handler, a reply a moment later)
        // under a hold the settle below releases — so the order path can stamp
        // an order the step causes as an agent's ("agent:claude_code").
        auto perform_error = std::make_shared<QString>();
        auto dialogs_guard = std::make_shared<common::NonNativeDialogs>();
        auto agent_hold = std::make_shared<AgentHoldSlot>();
        const ::fincept::core::agent::AgentActor actor = agent_actor;
        const Perform perform = work.perform;
        QTimer::singleShot(0, QCoreApplication::instance(), [perform, perform_error, agent_hold, actor]() {
            if (!agent_hold->settled)
                agent_hold->hold = std::make_unique<::fincept::core::agent::AgentActionHold>(actor, kAgentHoldMs);
            ::fincept::core::agent::AgentActionScope scope(actor);
            try {
                if (perform)
                    perform();
            } catch (const std::exception& e) {
                *perform_error = QString::fromUtf8(e.what()).left(160);
                LOG_ERROR(kUiActTag, logf("ui_act action threw", {{"error", log_scrub(*perform_error)}}));
            } catch (...) {
                *perform_error = QStringLiteral("an unknown error");
                LOG_ERROR(kUiActTag, QStringLiteral("ui_act action threw an unknown exception"));
            }
        });

        auto self = shared_from_this();
        auto clock = std::make_shared<QElapsedTimer>();
        clock->start();
        QPointer<QWidget> scope_guard(scope);
        QPointer<QWidget> host_guard(host);
        // What a web page shows now is in the page, not in widgets: say where to read it.
        const QString web_hint =
            t.web_view
                ? QStringLiteral(" → ui_read(ref=\"%1\") shows the page now").arg(t.ref.section(QLatin1Char(':'), 0, 0))
                : QString();
        const int settle_ms = symbol->watching ? kSymbolResolveMs : kSettleMs;
        common::poll_until(
            50, settle_ms,
            [clock, before, symbol]() {
                if (clock->elapsed() < kMinSettleMs)
                    return false;
                // A new dialog or menu settles the step at once.
                if (open_popup() != before.popup.data())
                    return true;
                if (open_dialogs().size() != before.dialogs.size())
                    return true;
                // A symbol box settles when it has resolved; anything else after kSettleMs.
                if (symbol->watching)
                    return symbol->done;
                return clock->elapsed() >= kSettleMs;
            },
            [self, s, t, before, scope_guard, host_guard, value_widget, target_label, work, suffix, web_hint, clock,
             perform_error, dialogs_guard, symbol, approved, agent_hold](bool) {
                dialogs_guard->release();
                agent_hold->settled = true;
                agent_hold->hold.reset(); // the step is settled: what follows is no longer its doing
                self->guarded("settle", [&]() {
                    self->settled(s, t, before, scope_guard, host_guard, value_widget, target_label, work, suffix,
                                  web_hint, clock->elapsed(), *perform_error, *symbol, approved);
                });
            });
    }

    void settled(const Step& s, const Target& t, const UiState& before, const QPointer<QWidget>& scope,
                 const QPointer<QWidget>& host, const QPointer<QWidget>& value_widget, const QString& target_label,
                 const Prepared& work, const QString& suffix, const QString& web_hint, qint64 ms,
                 const QString& perform_error, const SymbolWait& symbol, bool approved) {
        if (answered)
            return;
        if (!perform_error.isEmpty()) {
            log_step(s, t, host, QStringLiteral("failed"), ms, approved);
            finish(false, err_text(Err::Internal,
                                   QStringLiteral("%1: the control's own code failed (%2).").arg(work.note, perform_error)));
            return;
        }
        if (work.failed && !work.failed->isEmpty()) {
            log_step(s, t, host, QStringLiteral("rejected"), ms, approved);
            finish(false, err_text(Err::InvalidValue, QStringLiteral("%1: %2.").arg(work.note, *work.failed)));
            return;
        }
        const UiState after = capture(scope, value_widget);
        // A confirmation this step opened (a message box, an input or file
        // dialog) is judged as this step for its accept-role buttons.
        adopt_opened_dialogs(step_risk, before.dialogs);
        QStringList changes = diff(before, after, value_widget, target_label);
        if (symbol.watching) {
            if (!symbol.done)
                changes.prepend(QStringLiteral("still resolving the symbol"));
            else if (symbol.outcome.startsWith(QLatin1String("unresolved")))
                changes.prepend(symbol.outcome);
            else
                changes.prepend(QStringLiteral("resolved %1").arg(symbol.outcome));
        }
        QString line = QStringLiteral("%1. %2%3").arg(index + 1).arg(work.note, suffix);
        if (!changes.isEmpty())
            line += QStringLiteral(" → ") + changes.join(QStringLiteral("; "));
        else
            line += web_hint;
        log_step(s, t, host, QStringLiteral("done"), ms, approved);
        if (symbol.watching && symbol.done && symbol.outcome.startsWith(QLatin1String("unresolved"))) {
            report << line;
            finish(false, err_text(Err::NotFound, QStringLiteral("%1 did not resolve to an instrument (%2).")
                                                      .arg(common::quoted(common::clip(s.text, 40)), symbol.outcome)));
            return;
        }
        report << line;
        ++index;
        next();
    }

    /// One log line per operated step: what, where, how it ended — never the
    /// text typed. Order steps the user approved also go to the audit log.
    void log_step(const Step& s, const Target& t, QWidget* host, const QString& outcome, qint64 ms, bool approved) {
        const QString name = log_scrub(common::clip(t.name, 60));
        const QString where = host ? log_scrub(location_label(host)) : QString();
        LOG_INFO(kUiActTag, logf("Agent UI action", {{"action", s.action},
                                                     {"role", t.role},
                                                     {"name", name},
                                                     {"where", where},
                                                     {"outcome", outcome},
                                                     {"approved", approved},
                                                     {"duration_ms", ms},
                                                     {"client", origin}}));
        if (approved && money_step == index && outcome == QLatin1String("done"))
            LOG_AUDIT("Agents", "Agent operated an order control", {{"action", s.action},
                                                                    {"name", name},
                                                                    {"where", where},
                                                                    {"client", origin},
                                                                    {"approved", true}});
    }

    void finish(bool ok, const QString& error) {
        if (answered)
            return;
        answered = true;
        QStringList lines;
        if (!ok) {
            // The code first: harnesses branch on it.
            const QString coded = steps.size() > 1
                                      ? prefix_error(error, QStringLiteral("step %1 of %2 failed — ")
                                                                .arg(index + 1)
                                                                .arg(steps.size()))
                                      : with_error_code(error);
            lines << coded;
            lines += report;
            if (index + 1 < steps.size())
                lines << QStringLiteral("(steps %1–%2 not run)").arg(index + 2).arg(steps.size());
        } else {
            lines = report;
        }
        const BusyState busy = busy_state(last_scope ? last_scope.data() : nullptr);
        if (busy.busy())
            lines << QStringLiteral("loading: %1 — wait until=\"idle\" before reading results").arg(busy.describe());
        if (snapshot && ok) {
            QWidget* scope = open_popup();
            if (!scope)
                scope = blocking_dialog();
            if (!scope)
                scope = last_scope;
            if (scope) {
                WalkOptions opt;
                opt.detail = Detail::Normal;
                opt.max_nodes = 120;
                opt.base_depth = 1;
                bool truncated = false;
                lines << QStringLiteral("— now: %1").arg(location_label(scope));
                for (const Node& n : collect_nodes(scope, opt, &truncated))
                    lines += format_node(n, Detail::Normal);
                if (truncated)
                    lines << QStringLiteral("  … more — ui_snapshot for the rest");
            }
        }
        const QString text = lines.join(QLatin1Char('\n'));
        reply(ok ? ToolResult::ok(text) : ToolResult::fail(text));
    }
};

} // namespace ui_act_detail

using namespace ui_act_detail;

::fincept::core::agent::AgentActor agent_actor_for_origin(const QString& origin) {
    // The caller's label: "claude-code", "codex · terminal AI", "Fincept agent:
    // <run title>", "AI assistant", each perhaps with " (paper run)".
    QString label = origin.trimmed();
    if (is_paper_run_origin(label))
        label.chop(static_cast<int>(qstrlen(kPaperRunOriginSuffix)));
    QString client = label.section(QStringLiteral(" · "), 0, 0).trimmed();
    QString run;
    if (const qsizetype colon = client.indexOf(QLatin1Char(':')); colon > 0) {
        run = client.mid(colon + 1).trimmed();
        client = client.left(colon).trimmed();
    }
    return {client, run};
}

::fincept::core::agent::AgentActor agent_actor_for_call(const QString& origin) {
    const QString client = ScopedCallClient::client_id();
    if (!client.isEmpty())
        return {client, ScopedCallClient::run_id()};
    return agent_actor_for_origin(origin);
}

void ui_act(const QJsonObject& args, const QString& origin, CancelCheck cancelled, Reply reply) {
    auto run = std::make_shared<ActRun>();
    run->origin = origin;
    run->agent_actor = agent_actor_for_call(origin); // now: later steps run on later event-loop turns
    run->cancelled = std::move(cancelled);
    run->reply = std::move(reply);
    run->snapshot = args.value(QStringLiteral("snapshot")).toBool(false);
    const QString default_target = args.value(QStringLiteral("target")).toString();
    const QJsonArray steps = args.value(QStringLiteral("steps")).toArray();
    if (!steps.isEmpty()) {
        if (steps.size() > kMaxSteps) {
            run->reply(ui_fail(Err::InvalidValue, QStringLiteral("At most %1 steps per call.").arg(kMaxSteps)));
            return;
        }
        for (const QJsonValue& v : steps) {
            if (!v.isObject()) {
                run->reply(ui_fail(Err::InvalidValue,
                                   QStringLiteral("Each step is an object like {\"action\":\"click\",\"ref\":\"e5\"}.")));
                return;
            }
            run->steps << parse_step(v.toObject(), default_target);
        }
    } else {
        if (args.value(QStringLiteral("action")).toString().isEmpty()) {
            run->reply(ui_fail(Err::InvalidValue, QStringLiteral("Pass `action` (and `ref` or `name`), or `steps`.")));
            return;
        }
        run->steps << parse_step(args, default_target);
    }
    // Refuse a batch with an unknown action before any step runs, not halfway.
    for (int i = 0; i < run->steps.size(); ++i) {
        const QString& a = run->steps[i].action;
        if (!known_actions().contains(a) && !is_observe_action(a)) {
            run->reply(ui_fail(Err::InvalidValue,
                               QStringLiteral("Step %1: unknown action \"%2\". Nothing was run.").arg(i + 1).arg(a),
                               QStringLiteral("use one of: %1, wait, read.")
                                   .arg(known_actions().join(QStringLiteral(", ")))));
            return;
        }
    }
    run->guarded("start", [run]() { run->next(); });
}

} // namespace fincept::mcp::core::ui
