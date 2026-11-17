// RunStatusBar.cpp — the strip under Chat Mode (WP2; on ui::ScreenStatusBar since
// the 2026-11-17 revamp).
//
//   CLOUD · LITE | [RUNNING] | STEPS 2/5     title · activity     TOK 12,345 | CREDITS 12.5 / 50 | TIME 3m 12s |
//   LAST 4.2 s | TOOLS 12 ready | MCP 2/3 | BAL 4,980 | [PAPER]
//
// The run is the one ChatModeScreen says this window shows (set_run); without one
// the engine item says what the next message runs on (set_next_engine). Items have
// priorities: a narrow window drops the least important first (MCP, last response,
// tokens …), never the state or the PAPER badge. Tones are ScreenStatusBar's own
// (a dynamic property its one stylesheet covers), so nothing is re-styled here on a
// theme change. One 1 s timer drives the elapsed time, only while the bar is
// visible and the run is working. Balance: ChatModeService / AuthManager signals.
// Tools: the app-level TerminalToolBridge. Runs are paper-only in this version, so
// the trading-mode badge always reads PAPER.

#include "screens/chat_mode/RunStatusBar.h"

#include "auth/AuthManager.h"
#include "screens/chat_mode/ChatModeService.h"
#include "screens/chat_mode/RunViewLogic.h"
#include "screens/chat_mode/RunViewWidgets.h"
#include "screens/chat_mode/TerminalToolBridge.h"
#include "services/agent_tasks/TaskRunService.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ScreenStatusBar.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QTimer>

#include <algorithm>

namespace fincept::chat_mode {

namespace agt = fincept::agent_tasks;
namespace rv = fincept::chat_mode::run_view;
using SB = fincept::ui::ScreenStatusBar;

namespace run_status_bar_detail {

constexpr int kRsbTickMs = 1000;

// Item ids.
constexpr const char* kRsbEngine = "engine";
constexpr const char* kRsbState = "state";
constexpr const char* kRsbSteps = "steps";
constexpr const char* kRsbTitle = "title";
constexpr const char* kRsbTokens = "tokens";
constexpr const char* kRsbCredits = "credits";
constexpr const char* kRsbElapsed = "elapsed";
constexpr const char* kRsbLast = "last";
constexpr const char* kRsbTools = "tools";
constexpr const char* kRsbMcp = "mcp";
constexpr const char* kRsbBalance = "balance";
constexpr const char* kRsbPaper = "paper";

QString rsb_id(const char* id) {
    return QString::fromLatin1(id);
}

agt::TaskRunService& rsb_svc() {
    return agt::TaskRunService::instance();
}

SB::Tone rsb_tone(rv::Tone t) {
    switch (t) {
        case rv::Tone::Info:
            return SB::Tone::Info;
        case rv::Tone::Success:
            return SB::Tone::Ok;
        case rv::Tone::Warning:
            return SB::Tone::Warn;
        case rv::Tone::Danger:
            return SB::Tone::Danger;
        case rv::Tone::Neutral:
            break;
    }
    return SB::Tone::Dim;
}

struct RsbTools {
    QString text;
    QString tip;
    rv::Tone tone = rv::Tone::Neutral;
};

RsbTools rsb_tools(const ToolBridgeStatus& s) {
    using State = ToolBridgeStatus::State;
    RsbTools t;
    switch (s.state) {
        case State::Off:
            t.text = RunStatusBar::tr("off");
            t.tip = RunStatusBar::tr("The terminal's tools are not offered to the cloud agent right now. They are "
                                     "registered while Chat Mode is open or a cloud run is going.");
            break;
        case State::Usable:
            t.text = RunStatusBar::tr("%n ready", nullptr, s.offered);
            t.tip = RunStatusBar::tr("The cloud agent can call %n terminal tool(s).", nullptr, s.offered);
            t.tone = rv::Tone::Success;
            break;
        case State::Unconfirmed:
            t.text = RunStatusBar::tr("unconfirmed");
            t.tip = RunStatusBar::tr("The tools were offered, but the server cannot say whether the agent sees them.");
            break;
        case State::AgentUnreachable:
            t.text = RunStatusBar::tr("agent offline");
            t.tip = RunStatusBar::tr("The tools are registered, but the cloud agent service is not running right now.");
            t.tone = rv::Tone::Warning;
            break;
        case State::NotRegistered:
            t.text = RunStatusBar::tr("not registered");
            t.tip = RunStatusBar::tr("The tools are not registered with the cloud agent; the terminal retries on its "
                                     "own.");
            if (!s.detail.isEmpty())
                t.tip += QLatin1Char('\n') + s.detail;
            t.tone = rv::Tone::Danger;
            break;
        case State::Unavailable:
            t.text = RunStatusBar::tr("unavailable");
            t.tip = RunStatusBar::tr("The server's tool bridge is temporarily unavailable.");
            if (!s.detail.isEmpty())
                t.tip += QLatin1Char('\n') + s.detail;
            t.tone = rv::Tone::Warning;
            break;
    }
    return t;
}

/// What the run runs on, upper case ("CLOUD · LITE", "CLOUD TASK · DEEP", "LOCAL",
/// "HARNESS · Claude Code · sonnet").
QString rsb_engine_text(const agt::TaskRun& r) {
    if (r.engine == QLatin1String(agt::kEngineLocal))
        return RunStatusBar::tr("LOCAL");
    if (r.engine == QLatin1String(rv::kHarnessEngineId)) {
        const rv::HarnessEngineOption h = rv::harness_engine_option_now();
        return h.enabled ? RunStatusBar::tr("HARNESS · %1").arg(h.label) : RunStatusBar::tr("HARNESS");
    }
    const QString base = r.executor_id() == QLatin1String(agt::kExecutorCloudPlan) ? RunStatusBar::tr("CLOUD TASK")
                                                                                    : RunStatusBar::tr("CLOUD");
    return r.mode.isEmpty() ? base : base + QStringLiteral(" · ") + r.mode.toUpper();
}

} // namespace run_status_bar_detail

using namespace run_status_bar_detail;

struct RunStatusBar::Impl {
    RunStatusBar* q = nullptr;
    SB* bar = nullptr;
    QString run_id;
    QString next_engine;
    qint64 last_response_ms = -1;
    int mcp_connected = -1;
    int mcp_total = -1;
    QTimer* tick = nullptr;
    QString title_full;
    double last_balance = -1;

    void build();
    void wire();
    void retranslate();
    void refresh();
    void refresh_title();
    void refresh_balance(double balance);
    void refresh_tools();
    void refresh_mcp();
    void update_tick();
};

void RunStatusBar::Impl::build() {
    q->setObjectName(QStringLiteral("runStatusBar"));
    q->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* hl = new QHBoxLayout(q);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);
    bar = new SB(q);
    hl->addWidget(bar);

    // Priorities: higher stays visible longer in a narrow window.
    bar->add_item(rsb_id(kRsbEngine), SB::Segment::Left, {}, SB::Tone::Accent, 90);
    bar->add_item(rsb_id(kRsbState), SB::Segment::Left, {}, SB::Tone::Dim, 100);
    bar->set_chip(rsb_id(kRsbState), true);
    bar->add_item(rsb_id(kRsbSteps), SB::Segment::Left, {}, SB::Tone::Normal, 70);
    bar->add_item(rsb_id(kRsbTitle), SB::Segment::Centre, {}, SB::Tone::Normal, 20);
    bar->add_item(rsb_id(kRsbTokens), SB::Segment::Right, {}, SB::Tone::Normal, 40);
    bar->add_item(rsb_id(kRsbCredits), SB::Segment::Right, {}, SB::Tone::Normal, 75);
    bar->add_item(rsb_id(kRsbElapsed), SB::Segment::Right, {}, SB::Tone::Normal, 80);
    bar->add_item(rsb_id(kRsbLast), SB::Segment::Right, {}, SB::Tone::Dim, 35);
    bar->add_item(rsb_id(kRsbTools), SB::Segment::Right, {}, SB::Tone::Normal, 60);
    bar->add_item(rsb_id(kRsbMcp), SB::Segment::Right, {}, SB::Tone::Normal, 30);
    bar->add_item(rsb_id(kRsbBalance), SB::Segment::Right, {}, SB::Tone::Normal, 50);
    bar->add_item(rsb_id(kRsbPaper), SB::Segment::Right, {}, SB::Tone::Info, 95);
    bar->set_chip(rsb_id(kRsbPaper), true);
    bar->set_item_visible(rsb_id(kRsbMcp), false); // until the servers were read

    tick = new QTimer(q);
    tick->setInterval(kRsbTickMs);
}

void RunStatusBar::Impl::wire() {
    auto& svc = rsb_svc();
    QObject::connect(&svc, &agt::TaskRunService::run_updated, q, [this](const QString& id) {
        if (id == run_id)
            refresh();
    });
    QObject::connect(&svc, &agt::TaskRunService::run_finished, q, [this](const QString& id) {
        if (id == run_id)
            refresh();
    });
    QObject::connect(&svc, &agt::TaskRunService::steps_changed, q, [this](const QString& id) {
        if (id == run_id)
            refresh();
    });
    QObject::connect(&svc, &agt::TaskRunService::runs_reset, q, [this]() { refresh(); });
    QObject::connect(tick, &QTimer::timeout, q, [this]() { refresh(); });

    QObject::connect(&ChatModeService::instance(), &ChatModeService::credit_balance_changed, q,
                     [this](double b) { refresh_balance(b); });
    QObject::connect(&auth::AuthManager::instance(), &auth::AuthManager::credit_balance_changed, q,
                     [this](double b) { refresh_balance(b); });
    QObject::connect(&auth::AuthManager::instance(), &auth::AuthManager::auth_state_changed, q, [this]() {
        const auto& s = auth::AuthManager::instance().session();
        refresh_balance(s.authenticated ? s.user_info.credit_balance : -1);
    });
    QObject::connect(&TerminalToolBridge::instance(), &TerminalToolBridge::tools_status_changed, q,
                     [this](const ToolBridgeStatus&) { refresh_tools(); });
    // The title's room follows the font (after every widget took its new style).
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, q,
                     [this]() { refresh_title(); });

    new rv::RvEventHook(q, [this](QEvent::Type t) {
        switch (t) {
            case QEvent::LanguageChange:
                retranslate();
                break;
            case QEvent::Show:
                refresh();
                break;
            case QEvent::Hide:
                tick->stop();
                break;
            case QEvent::Resize:
                refresh_title();
                break;
            default:
                break;
        }
    });
}

void RunStatusBar::Impl::retranslate() {
    bar->set_text(rsb_id(kRsbPaper), RunStatusBar::tr("PAPER"));
    bar->set_tooltip(rsb_id(kRsbPaper),
                     RunStatusBar::tr("Agent runs are paper-only in this version: a run never places a live or broker "
                                      "order. Trades the agent proposes stay approval cards for the Fincept portfolio "
                                      "ledger, and scheduled runs refuse money actions outright."));
    if (QLabel* l = bar->item(rsb_id(kRsbPaper)))
        l->setAccessibleName(RunStatusBar::tr("Trading mode: paper"));
    if (QLabel* l = bar->item(rsb_id(kRsbElapsed)))
        l->setAccessibleName(RunStatusBar::tr("Elapsed time of the run"));
    if (QLabel* l = bar->item(rsb_id(kRsbCredits)))
        l->setAccessibleName(RunStatusBar::tr("Credits the run spent, against its budget"));
    if (QLabel* l = bar->item(rsb_id(kRsbBalance)))
        l->setAccessibleName(RunStatusBar::tr("Account credit balance"));
    if (QLabel* l = bar->item(rsb_id(kRsbTools)))
        l->setAccessibleName(RunStatusBar::tr("Terminal tools offered to the cloud agent"));
    if (QLabel* l = bar->item(rsb_id(kRsbTokens)))
        l->setAccessibleName(RunStatusBar::tr("Tokens the run used"));
    if (QLabel* l = bar->item(rsb_id(kRsbEngine)))
        l->setAccessibleName(RunStatusBar::tr("Engine"));
    const auto& s = auth::AuthManager::instance().session();
    refresh_balance(last_balance >= 0 ? last_balance : (s.authenticated ? s.user_info.credit_balance : -1));
    refresh_tools();
    refresh_mcp();
    refresh();
}

void RunStatusBar::Impl::refresh_balance(double b) {
    last_balance = b;
    bar->set_text(rsb_id(kRsbBalance), RunStatusBar::tr("BAL %1").arg(b >= 0 ? rv::format_credits(b) : QStringLiteral("—")));
    bar->set_tooltip(rsb_id(kRsbBalance), b >= 0 ? RunStatusBar::tr("Your account balance: %1 credits").arg(rv::format_credits(b))
                                                 : RunStatusBar::tr("Sign in to see your credit balance"));
}

void RunStatusBar::Impl::refresh_tools() {
    const RsbTools t = rsb_tools(TerminalToolBridge::instance().status());
    bar->set_item(rsb_id(kRsbTools), RunStatusBar::tr("TOOLS %1").arg(t.text),
                  t.tone == rv::Tone::Neutral ? SB::Tone::Normal : rsb_tone(t.tone));
    bar->set_tooltip(rsb_id(kRsbTools), t.tip);
}

void RunStatusBar::Impl::refresh_mcp() {
    if (mcp_total < 0) {
        bar->set_item_visible(rsb_id(kRsbMcp), false);
        return;
    }
    const SB::Tone tone = mcp_total == 0                ? SB::Tone::Dim
                          : mcp_connected == mcp_total ? SB::Tone::Ok
                                                       : SB::Tone::Warn;
    bar->set_item(rsb_id(kRsbMcp), RunStatusBar::tr("MCP %1/%2").arg(mcp_connected).arg(mcp_total), tone);
    bar->set_tooltip(rsb_id(kRsbMcp), mcp_total == 0
                                          ? RunStatusBar::tr("The cloud agent has no remote MCP servers.")
                                          : RunStatusBar::tr("Remote MCP servers of the cloud agent: %1 of %2 connected")
                                                .arg(mcp_connected)
                                                .arg(mcp_total));
    bar->set_item_visible(rsb_id(kRsbMcp), true);
}

void RunStatusBar::Impl::refresh_title() {
    QLabel* l = bar->item(rsb_id(kRsbTitle));
    if (!l)
        return;
    // The title takes up to a third of the strip; the full text is its tooltip.
    const int room = std::max(QFontMetrics(l->font()).averageCharWidth() * 12, q->width() / 3);
    bar->set_text(rsb_id(kRsbTitle), QFontMetrics(l->font()).elidedText(title_full, Qt::ElideRight, room));
}

void RunStatusBar::Impl::refresh() {
    auto& svc = rsb_svc();
    const agt::TaskRun* r = run_id.isEmpty() ? nullptr : svc.find_run(run_id);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!r) {
        bar->set_text(rsb_id(kRsbEngine), next_engine.isEmpty() ? RunStatusBar::tr("CLOUD") : next_engine.toUpper());
        bar->set_tooltip(rsb_id(kRsbEngine), RunStatusBar::tr("What your next message runs on"));
        bar->set_item(rsb_id(kRsbState), RunStatusBar::tr("NO RUN"), SB::Tone::Dim);
        bar->set_tooltip(rsb_id(kRsbState), QString());
        if (QLabel* l = bar->item(rsb_id(kRsbState)))
            l->setAccessibleName(RunStatusBar::tr("No run"));
        bar->set_item_visible(rsb_id(kRsbSteps), false);
        title_full = run_id.isEmpty() ? RunStatusBar::tr("Send a message or start a task: its progress shows here.")
                                      : RunStatusBar::tr("This run was deleted.");
        bar->set_tooltip(rsb_id(kRsbTitle), QString());
        bar->set_item_visible(rsb_id(kRsbTokens), false);
        bar->set_text(rsb_id(kRsbCredits), RunStatusBar::tr("CREDITS —"));
        bar->set_tooltip(rsb_id(kRsbCredits), QString());
        bar->set_text(rsb_id(kRsbElapsed), RunStatusBar::tr("TIME —"));
        bar->set_tooltip(rsb_id(kRsbElapsed), QString());
    } else {
        bar->set_text(rsb_id(kRsbEngine), rsb_engine_text(*r));
        bar->set_tooltip(rsb_id(kRsbEngine), RunStatusBar::tr("The engine this run runs on"));
        bar->set_item(rsb_id(kRsbState), rv::run_status_pill(r->status), rsb_tone(rv::run_status_tone(r->status)));
        bar->set_tooltip(rsb_id(kRsbState), agt::run_status_label(r->status));
        if (QLabel* l = bar->item(rsb_id(kRsbState)))
            l->setAccessibleName(RunStatusBar::tr("Run state: %1").arg(agt::run_status_label(r->status)));

        const QVector<agt::RunStep> steps = svc.steps(run_id);
        if (rv::is_single_reply(*r, steps) || steps.isEmpty()) {
            bar->set_item_visible(rsb_id(kRsbSteps), false);
        } else {
            const rv::StepProgress p = rv::step_progress(steps);
            bar->set_text(rsb_id(kRsbSteps), RunStatusBar::tr("STEPS %1/%2").arg(p.done).arg(p.total));
            bar->set_tooltip(rsb_id(kRsbSteps),
                             p.failed > 0 ? RunStatusBar::tr("%1 of %2 steps done, %3 failed").arg(p.done).arg(p.total).arg(p.failed)
                                          : RunStatusBar::tr("%1 of %2 steps done").arg(p.done).arg(p.total));
            bar->set_item_visible(rsb_id(kRsbSteps), true);
        }

        QString t = r->title;
        if (r->status == agt::RunStatus::Queued) {
            const int pos = svc.queue_position(run_id);
            if (pos > 0)
                t = RunStatusBar::tr("#%1 in the queue · %2").arg(pos).arg(t);
        }
        const QString act = agt::is_terminal(r->status) ? QString() : r->activity;
        if (!act.isEmpty())
            t += QStringLiteral(" · ") + act;
        title_full = t.simplified();
        bar->set_tooltip(rsb_id(kRsbTitle),
                         r->title + (act.isEmpty() ? QString() : QLatin1Char('\n') + act) +
                             (r->error.isEmpty() || r->status == agt::RunStatus::Completed ? QString()
                                                                                          : QLatin1Char('\n') + r->error));

        if (r->tokens_used > 0) {
            bar->set_text(rsb_id(kRsbTokens), RunStatusBar::tr("TOK %1").arg(QLocale().toString(r->tokens_used)));
            bar->set_tooltip(rsb_id(kRsbTokens), RunStatusBar::tr("Tokens the run used. The input / output split is not "
                                                                  "reported by the service."));
            bar->set_item_visible(rsb_id(kRsbTokens), true);
        } else {
            bar->set_item_visible(rsb_id(kRsbTokens), false);
        }

        QString credits;
        QString credits_tip;
        if (r->engine == QLatin1String(agt::kEngineLocal)) {
            credits = RunStatusBar::tr("local");
            credits_tip = RunStatusBar::tr("Local runs use your own model: no Fincept credits.");
        } else if (r->engine == QLatin1String(rv::kHarnessEngineId)) {
            credits = RunStatusBar::tr("0 cr");
            credits_tip = RunStatusBar::tr("Harness runs (Claude Code, Codex) are billed to your own API key: no Fincept "
                                           "credits. Its tokens are in ACTIVITY.");
        } else if (r->budget.max_credits > 0) {
            credits = QStringLiteral("%1 / %2").arg(rv::format_credits(r->credits_spent),
                                                    rv::format_credits(r->budget.max_credits));
            credits_tip = r->status == agt::RunStatus::BudgetStopped ? RunStatusBar::tr("The run stopped at its credit budget.")
                                                                     : RunStatusBar::tr("Credits spent / the run's budget");
        } else {
            credits = rv::format_credits(r->credits_spent);
            credits_tip = RunStatusBar::tr("Credits this run spent (no budget limit)");
        }
        if (rv::run_uses_credits(*r) && r->credits_unreported > 0) {
            // A turn recovered from the stored conversation: charged, but its bill never arrived.
            credits += QStringLiteral(" +?");
            credits_tip += QLatin1Char('\n') + RunStatusBar::tr("%n turn(s) recovered from the conversation: the server "
                                                                "charged them but did not report the credits.",
                                                                nullptr, r->credits_unreported);
        }
        const bool stopped = r->status == agt::RunStatus::BudgetStopped;
        const bool near = r->budget.max_credits > 0 && r->credits_spent >= 0.8 * r->budget.max_credits;
        bar->set_item(rsb_id(kRsbCredits), RunStatusBar::tr("CREDITS %1").arg(credits),
                      stopped ? SB::Tone::Danger : near ? SB::Tone::Warn : SB::Tone::Normal);
        bar->set_tooltip(rsb_id(kRsbCredits), credits_tip);

        bar->set_text(rsb_id(kRsbElapsed),
                      RunStatusBar::tr("TIME %1")
                          .arg(r->started_at_ms > 0 ? rv::format_duration(rv::run_elapsed_ms(*r, now)) : QStringLiteral("—")));
        bar->set_tooltip(rsb_id(kRsbElapsed),
                         r->started_at_ms > 0
                             ? RunStatusBar::tr("Working time since %1 (pauses and questions to you do not count)")
                                   .arg(QLocale().toString(QDateTime::fromMSecsSinceEpoch(r->started_at_ms),
                                                           QLocale::ShortFormat))
                             : RunStatusBar::tr("Not started yet"));
    }

    // Last response time: the shown run's own when it has ended, else the
    // conversation's last stored answer.
    qint64 last = -1;
    if (r && agt::is_terminal(r->status) && r->started_at_ms > 0 && r->finished_at_ms >= r->started_at_ms)
        last = r->finished_at_ms - r->started_at_ms;
    else if (!r || agt::is_terminal(r->status))
        last = last_response_ms;
    if (last >= 0) {
        bar->set_text(rsb_id(kRsbLast), RunStatusBar::tr("LAST %1").arg(rv::ActivityLog::format_duration_precise(last)));
        bar->set_tooltip(rsb_id(kRsbLast), RunStatusBar::tr("How long the last answer took"));
        bar->set_item_visible(rsb_id(kRsbLast), true);
    } else {
        bar->set_item_visible(rsb_id(kRsbLast), false);
    }

    refresh_title();
    update_tick();
}

void RunStatusBar::Impl::update_tick() {
    const agt::TaskRun* r = run_id.isEmpty() ? nullptr : rsb_svc().find_run(run_id);
    // Only a working run's clock moves (a paused or waiting run's stands still).
    const bool want = q->isVisible() && r && agt::holds_slot(r->status) && r->started_at_ms > 0;
    if (want && !tick->isActive())
        tick->start();
    else if (!want && tick->isActive())
        tick->stop();
}

RunStatusBar::RunStatusBar(QWidget* parent) : QWidget(parent), d_(std::make_unique<Impl>()) {
    d_->q = this;
    d_->build();
    d_->wire();
    d_->retranslate();
}

RunStatusBar::~RunStatusBar() = default;

void RunStatusBar::set_run(const QString& run_id) {
    if (run_id == d_->run_id)
        return;
    d_->run_id = run_id;
    d_->refresh();
}

QString RunStatusBar::run_id() const {
    return d_->run_id;
}

void RunStatusBar::set_next_engine(const QString& engine_label) {
    if (engine_label == d_->next_engine)
        return;
    d_->next_engine = engine_label;
    if (d_->run_id.isEmpty() || !rsb_svc().find_run(d_->run_id))
        d_->refresh();
}

void RunStatusBar::set_last_response_ms(qint64 ms) {
    if (ms == d_->last_response_ms)
        return;
    d_->last_response_ms = ms;
    d_->refresh();
}

void RunStatusBar::set_mcp_state(int connected, int total) {
    d_->mcp_connected = total < 0 ? -1 : std::max(0, connected);
    d_->mcp_total = total;
    d_->refresh_mcp();
}

ui::ScreenStatusBar* RunStatusBar::bar() const {
    return d_->bar;
}

} // namespace fincept::chat_mode
