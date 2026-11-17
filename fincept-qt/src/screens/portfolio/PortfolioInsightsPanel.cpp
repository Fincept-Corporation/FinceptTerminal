// src/screens/portfolio/PortfolioInsightsPanel.cpp
#include "screens/portfolio/PortfolioInsightsPanel.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "screens/portfolio/PortfolioFormat.h"
#include "services/agents/AgentService.h"
#include "services/billing/PlanAccess.h"
#include "services/llm/LlmService.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/markdown/MarkdownRenderer.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"
#include "ui/widgets/StatTile.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::screens {

namespace {

// SettingsRepository category under which AI/agent results are persisted.
constexpr const char* kInsightsCategory = "portfolio_insights";
// A remembered result older than this gets the "may be out of date · REFRESH" notice.
constexpr int kInsightsMaxAgeS = 24 * 3600;
// An LLM / agent run that has not answered after this long is given up (RETRY).
constexpr int kInsightsTimeoutS = 10 * 60;
// Streamed agent text is pushed into the pane at most this often.
constexpr int kInsightsPartialMs = 150;

// Side of the "×" close button: 28px at the default font size, growing with it.
int insights_close_button_side() {
    return std::max(28, ui::fonts::font_px(6) + 8);
}

/// Cost line of the AI analysis preview. Fincept's own models are billed in credits
/// (the server's plan table, price key "ai": included with the plan, or its price);
/// another provider bills the user's own key — and without a table: the old wording.
QString insights_ai_cost_note() {
    const char* ctx = "fincept::screens::PortfolioInsightsPanel";
    const QString fallback = QCoreApplication::translate(ctx, "Uses your LLM provider — may use credits");
    if (ai_chat::LlmService::instance().active_provider().compare(QLatin1String("fincept"), Qt::CaseInsensitive) != 0)
        return fallback;
    const auto& access = billing::PlanAccess::instance();
    if (!access.has_table())
        return fallback;
    if (access.included(QStringLiteral("ai")))
        return QCoreApplication::translate(ctx, "Fincept AI — included with your plan, no credits used");
    if (const billing::PlanPrice* row = access.price(QStringLiteral("ai"))) {
        const QString price = access.is_paid() ? row->pro_price : row->free_price;
        if (!price.isEmpty())
            return QCoreApplication::translate(ctx, "Fincept AI — %1 on your plan").arg(price);
    }
    return fallback;
}

QString insights_type_label(const QString& type) {
    if (type == QLatin1String("opportunities"))
        return QStringLiteral("OPPS");
    return type.toUpper();
}

/// Plain one-paragraph message (no traceback) for an error page.
QString insights_plain_error(const QString& raw) {
    QString s = raw.trimmed();
    const qsizetype tb = s.indexOf(QLatin1String("Traceback"));
    if (tb > 0)
        s = s.left(tb).trimmed();
    if (s.size() > 400)
        s = s.left(400) + QStringLiteral("…");
    return s;
}

/// ISO with offset (what persist_result writes) or "2026-11-17 14:02:11" (SQLite
/// datetime('now'), which is UTC) -> UTC QDateTime; invalid when neither parses.
QDateTime insights_parse_time(const QString& s) {
    if (s.isEmpty())
        return {};
    if (s.contains(QLatin1Char('T'))) {
        const QDateTime t = QDateTime::fromString(s, Qt::ISODate);
        return t.isValid() ? t.toUTC() : QDateTime();
    }
    QDateTime t = QDateTime::fromString(s, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!t.isValid())
        return {};
    t.setTimeZone(QTimeZone(QTimeZone::UTC));
    return t;
}

// Markdown rendering is delegated to ui::MarkdownRenderer — the same path used
// by AgentChatPanel and AI Chat so portfolio responses look consistent.

} // namespace

PortfolioInsightsPanel::PortfolioInsightsPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("PortfolioInsightsPanel");
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(560);
    build_ui();
    hide();

    // Wire AgentService results. Both run_portfolio_analysis (AI tab) and
    // run_agent_streaming (Agent tab) come back through this service — we
    // demultiplex by the request id we stored when we kicked off the run.
    auto& svc = services::AgentService::instance();

    connect(&svc, &services::AgentService::agent_result, this, [this](services::AgentExecutionResult r) {
        // Only the AI tab uses the non-streaming run_portfolio_analysis path. agent_result is
        // emitted by every agent path in the app (Agent Config, AI chat, workflows), so match
        // on the request id of OUR run, not merely "something is pending".
        if (ai_pending_req_id_.isEmpty() || r.request_id != ai_pending_req_id_)
            return;
        const quint64 id = std::exchange(ai_pending_pane_id_, 0);
        const QString type = ai_pending_type_;
        const QString run_pid = ai_pending_portfolio_id_;
        Stored s;
        s.snapshot = ai_pending_snapshot_;
        ai_pending_type_.clear();
        ai_pending_req_id_.clear();
        ai_pending_portfolio_id_.clear();
        ai_pending_snapshot_ = {};
        header_status_->clear();
        if (r.success && !r.response.trimmed().isEmpty()) {
            s.markdown = r.response;
            s.as_of = QDateTime::currentDateTimeUtc();
            s.ms = r.execution_time_ms;
            // Kept under the book the run was started for — the user may have switched since.
            persist_result(QStringLiteral("ai"), run_pid, type, s);
            if (run_pid == summary_.portfolio.id)
                ai_store_.insert(run_pid + QLatin1Char('/') + type, s);
            ai_pane_->deliver(id, stored_payload(s), s.as_of);
        } else {
            const QString msg = r.error.trimmed().isEmpty() ? tr("The model returned no answer.")
                                                            : insights_plain_error(r.error);
            ai_pane_->fail(id, msg, tr("Analysis failed"));
        }
    });

    connect(&svc, &services::AgentService::agent_stream_thinking, this,
            [this](const QString& request_id, const QString& status) {
                if (agent_pending_req_id_.isEmpty() || request_id != agent_pending_req_id_)
                    return;
                header_status_->setText("● " + status);
            });

    // Live token streaming — accumulate and push the text so far into the pane
    // (deliver_partial, throttled); the final markdown lands on agent_stream_done.
    connect(&svc, &services::AgentService::agent_stream_token, this,
            [this](const QString& request_id, const QString& token) {
                if (agent_pending_req_id_.isEmpty() || request_id != agent_pending_req_id_)
                    return;
                agent_streaming_text_ += token;
                if (!agent_partial_timer_->isActive())
                    agent_partial_timer_->start();
            });

    connect(&svc, &services::AgentService::agent_stream_done, this, [this](services::AgentExecutionResult r) {
        if (agent_pending_req_id_.isEmpty() || r.request_id != agent_pending_req_id_)
            return;
        agent_partial_timer_->stop();
        const quint64 id = std::exchange(agent_pending_pane_id_, 0);
        const QString agent_id = agent_pending_id_;
        const QString run_pid = agent_pending_portfolio_id_;
        Stored s;
        s.snapshot = agent_pending_snapshot_;
        agent_pending_req_id_.clear();
        agent_pending_id_.clear();
        agent_pending_portfolio_id_.clear();
        agent_pending_snapshot_ = {};
        header_status_->clear();

        // finagent_core emits some agents via the final JSON line
        // (r.response populated) and others via streamed tokens (where
        // r.response may come back empty). Mirror AgentChatPanel's
        // logic: prefer r.response, fall back to accumulated tokens.
        QString final_text = r.response.trimmed();
        if (final_text.isEmpty())
            final_text = agent_streaming_text_.trimmed();
        agent_streaming_text_.clear();

        if (r.success && !final_text.isEmpty()) {
            s.markdown = final_text;
            s.as_of = QDateTime::currentDateTimeUtc();
            s.ms = r.execution_time_ms;
            persist_result(QStringLiteral("agent"), run_pid, agent_id, s);
            if (run_pid == summary_.portfolio.id)
                agent_store_.insert(run_pid + QLatin1Char('/') + agent_id, s);
            agent_pane_->deliver(id, stored_payload(s), s.as_of);
        } else if (r.success) {
            // Agent reported success but produced no text — likely a config issue
            // (no LLM key in the agent's profile, etc.).
            agent_pane_->fail(id,
                              tr("The agent finished but returned no text. Check the agent's LLM profile in Agent "
                                 "Config, and that a provider is set in Settings > LLM Config."),
                              tr("No answer"));
        } else {
            const QString msg = r.error.trimmed().isEmpty() ? tr("The agent returned no answer.")
                                                            : insights_plain_error(r.error);
            agent_pane_->fail(id, msg, tr("Agent run failed"));
        }
    });

    // Refresh the agent dropdown whenever finagent_core discovers agents.
    // This matches the AgentChatPanel wiring so both lists stay in sync.
    connect(&svc, &services::AgentService::agents_discovered, this,
            [this](QVector<services::AgentInfo> agents, QVector<services::AgentCategory>) {
                discovered_ = std::move(agents);
                discovering_ = false;
                discovery_done_ = true;
                if (!discovered_.isEmpty())
                    discovery_failed_ = false;
                if (current_tab_ == Tab::Agent || agent_stack_->currentIndex() == 0)
                    reload_agents();
            });

    connect(&svc, &services::AgentService::error_occurred, this,
            [this](const QString& context, const QString& message) {
                if (context == QLatin1String("run_portfolio_analysis")) {
                    if (ai_pending_pane_id_ == 0)
                        return;
                    // Close the pending gate too, or a later unrelated result could be taken as ours.
                    const quint64 id = std::exchange(ai_pending_pane_id_, 0);
                    ai_pending_type_.clear();
                    ai_pending_req_id_.clear();
                    ai_pending_portfolio_id_.clear();
                    header_status_->clear();
                    ai_pane_->fail(id, insights_plain_error(message), tr("Analysis failed"));
                } else if (context == QLatin1String("run_agent_streaming")) {
                    if (agent_pending_pane_id_ == 0)
                        return;
                    agent_partial_timer_->stop();
                    const quint64 id = std::exchange(agent_pending_pane_id_, 0);
                    agent_pending_req_id_.clear();
                    agent_pending_id_.clear();
                    agent_pending_portfolio_id_.clear();
                    agent_streaming_text_.clear();
                    header_status_->clear();
                    agent_pane_->fail(id, insights_plain_error(message), tr("Agent run failed"));
                } else if (context == QLatin1String("discover_agents")) {
                    discovering_ = false;
                    discovery_done_ = true;
                    discovery_failed_ = true;
                    discovery_error_ = insights_plain_error(message);
                    if (current_tab_ == Tab::Agent || agent_stack_->currentIndex() == 0)
                        reload_agents();
                }
            });

    // A provider configured (or removed) while the panel is open: the panes re-check
    // their LLM requirement on the next show; re-check now when visible.
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::config_changed, this, [this]() {
        LOG_DEBUG("InsightsPanel", "LLM configuration changed; re-checking the requirement");
        configure_ai_preview(ai_type_); // the cost line names the provider's billing
        if (isVisible()) {
            ai_pane_->recheck_requirements();
            agent_pane_->recheck_requirements();
        }
    });
    // The server's plan table arrived / changed: the AI preview's cost line follows it.
    connect(&billing::PlanAccess::instance(), &billing::PlanAccess::changed, this,
            [this]() { configure_ai_preview(ai_type_); });

    // The close button's size follows the font and the result HTML carries theme
    // colours: both are redone once the new appearance has settled (never inside a
    // theme_changed handler).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this,
            &PortfolioInsightsPanel::refresh_theme);
    // Currency / number format changed: re-format the results on screen (no new run).
    ui::formatting::on_format_changed(this, [this]() {
        if (ai_pane_->state() == ui::AutoLoadPane::State::Content && !ai_pane_->is_running())
            render_payload(ai_kpis_, ai_content_, ai_pane_->payload());
        if (agent_pane_->state() == ui::AutoLoadPane::State::Content && agent_pending_pane_id_ == 0)
            render_payload(agent_kpis_, agent_content_, agent_pane_->payload());
    });
}

PortfolioInsightsPanel::~PortfolioInsightsPanel() {
    // The panes are deleted with the children, after this object's members: their
    // destructors must not call back into them.
    if (ai_pane_)
        ai_pane_->set_cancel({});
    if (agent_pane_)
        agent_pane_->set_cancel({});
}

void PortfolioInsightsPanel::refresh_theme() {
    const int side = insights_close_button_side();
    if (header_close_btn_->width() != side)
        header_close_btn_->setFixedSize(side, side);
    // The result views hold HTML coloured when it was rendered: render it again.
    if (ai_pane_->state() == ui::AutoLoadPane::State::Content && !ai_pane_->is_running())
        render_payload(ai_kpis_, ai_content_, ai_pane_->payload());
    if (agent_pane_->state() == ui::AutoLoadPane::State::Content && agent_pending_pane_id_ == 0)
        render_payload(agent_kpis_, agent_content_, agent_pane_->payload());
}

void PortfolioInsightsPanel::build_ui() {
    const QString bg = ui::colors::BG_BASE();
    const QString surface = ui::colors::BG_SURFACE();
    const QString hover = ui::colors::BG_HOVER();
    const QString border = ui::colors::BORDER_MED();
    const QString border_dim = ui::colors::BORDER_DIM();
    const QString amber = ui::colors::AMBER();
    const QString text1 = ui::colors::TEXT_PRIMARY();
    const QString text2 = ui::colors::TEXT_SECONDARY();

    // Opaque background — this is the whole fix for the previous overlap bug.
    setStyleSheet(QString("#PortfolioInsightsPanel { background:%1; border-left:1px solid %2; }").arg(bg, border));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setMinimumHeight(44);
    header->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;").arg(surface, border));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(16, 0, 8, 0);
    hl->setSpacing(8);

    header_title_ = new QLabel(tr("PORTFOLIO INSIGHTS"));
    header_title_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:800; letter-spacing:2px;").arg(amber));
    hl->addWidget(header_title_);

    header_status_ = new QLabel;
    header_status_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(text2));
    hl->addWidget(header_status_);
    hl->addStretch();

    header_close_btn_ = new QPushButton("×");
    header_close_btn_->setFixedSize(insights_close_button_side(), insights_close_button_side());
    header_close_btn_->setCursor(Qt::PointingHandCursor);
    header_close_btn_->setToolTip(tr("Close  (Esc)"));
    header_close_btn_->setStyleSheet(QString("QPushButton { background:transparent; border:none; color:%1;"
                                             "  font-size:" + fincept::ui::fonts::px(6) + "px; font-weight:300; }"
                                             "QPushButton:hover { color:%2; background:%3; }")
                                         .arg(text2, text1, hover));
    connect(header_close_btn_, &QPushButton::clicked, this, [this]() {
        hide();
        emit close_requested();
    });
    hl->addWidget(header_close_btn_);
    root->addWidget(header);

    // ── Tab bar ───────────────────────────────────────────────────────────────
    auto* tab_bar = new QWidget(this);
    tab_bar->setMinimumHeight(36);
    tab_bar->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;").arg(bg, border_dim));
    auto* tl = new QHBoxLayout(tab_bar);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(0);

    auto make_tab = [&](const QString& txt) {
        auto* b = new QPushButton(txt);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:none;"
                                 "  border-bottom:2px solid transparent; padding:8px 0;"
                                 "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; letter-spacing:1.5px; }"
                                 "QPushButton:checked { color:%2; border-bottom:2px solid %2;"
                                 "  background:" + ui::colors::alpha(ui::colors::AMBER, 0.06) + "; }"
                                 "QPushButton:hover:!checked { color:%3; }")
                             .arg(text2, amber, text1));
        return b;
    };
    tab_ai_btn_ = make_tab(tr("AI ANALYSIS"));
    tab_agent_btn_ = make_tab(tr("AGENT RUNNER"));
    tab_ai_btn_->setChecked(true);
    connect(tab_ai_btn_, &QPushButton::clicked, this, [this]() { switch_tab(Tab::AI); });
    connect(tab_agent_btn_, &QPushButton::clicked, this, [this]() { switch_tab(Tab::Agent); });
    tl->addWidget(tab_ai_btn_, 1);
    tl->addWidget(tab_agent_btn_, 1);
    root->addWidget(tab_bar);

    // ── Pages ─────────────────────────────────────────────────────────────────
    pages_ = new QStackedWidget(this);
    pages_->addWidget(build_ai_page());
    pages_->addWidget(build_agent_page());
    root->addWidget(pages_, 1);
}

// The result area shown by both panes: the book at run time (KPI strip) above the
// model's markdown answer.
QWidget* PortfolioInsightsPanel::build_result_view(QWidget* parent, ui::KpiStrip** kpis, QTextBrowser** text) {
    auto* w = new QWidget(parent);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(12, 10, 12, 0);
    l->setSpacing(8);
    auto* strip = new ui::KpiStrip(w);
    strip->add_tile(QStringLiteral("nav"), tr("NAV"));
    strip->add_tile(QStringLiteral("pnl"), tr("UNREALISED P&L"));
    strip->add_tile(QStringLiteral("positions"), tr("POSITIONS"));
    for (int i = 0; i < strip->count(); ++i)
        strip->tile(i)->set_compact(true);
    l->addWidget(strip);
    auto* tb = new QTextBrowser(w);
    tb->setOpenExternalLinks(true);
    tb->setStyleSheet(QString("QTextBrowser { background:%1; color:%2; border:none; padding:4px 4px 16px 4px;"
                              "  font-size:" + fincept::ui::fonts::px(-2) + "px; }")
                          .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY()));
    l->addWidget(tb, 1);
    *kpis = strip;
    *text = tb;
    return w;
}

QWidget* PortfolioInsightsPanel::build_ai_page() {
    const QString bg = ui::colors::BG_BASE();
    const QString surface = ui::colors::BG_SURFACE();
    const QString border_dim = ui::colors::BORDER_DIM();
    const QString amber = ui::colors::AMBER();
    const QString text1 = ui::colors::TEXT_PRIMARY();
    const QString text2 = ui::colors::TEXT_SECONDARY();
    const QString text3 = ui::colors::TEXT_TERTIARY();

    auto* page = new QWidget;
    page->setStyleSheet(QString("background:%1;").arg(bg));
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Config block
    auto* cfg = new QWidget(page);
    cfg->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;").arg(surface, border_dim));
    auto* cl = new QVBoxLayout(cfg);
    cl->setContentsMargins(16, 12, 16, 12);
    cl->setSpacing(10);

    ai_type_label_ = new QLabel(tr("ANALYSIS TYPE"));
    ai_type_label_->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; letter-spacing:1.5px;").arg(text3));
    cl->addWidget(ai_type_label_);

    auto* pill_row = new QHBoxLayout;
    pill_row->setSpacing(6);
    auto make_pill = [&](const QString& txt, const QString& id) {
        auto* p = new QPushButton(txt);
        p->setCheckable(true);
        p->setCursor(Qt::PointingHandCursor);
        p->setMinimumHeight(28);
        p->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:1px solid %3;"
                                 "  padding:0 14px; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; letter-spacing:1px; }"
                                 "QPushButton:checked { background:" + ui::colors::alpha(ui::colors::AMBER, 0.15) + "; color:%4;"
                                 "  border:1px solid %4; }"
                                 "QPushButton:hover:!checked { color:%5; border:1px solid %5; }")
                             .arg(bg, text2, border_dim, amber, text1));
        connect(p, &QPushButton::clicked, this, [this, id]() { set_ai_type(id); });
        pill_row->addWidget(p);
        return p;
    };
    ai_full_ = make_pill(tr("FULL"), "full");
    ai_risk_ = make_pill(tr("RISK"), "risk");
    ai_rebal_ = make_pill(tr("REBALANCE"), "rebalance");
    ai_opport_ = make_pill(tr("OPPS"), "opportunities");
    ai_full_->setChecked(true);
    pill_row->addStretch();
    cl->addLayout(pill_row);

    ai_run_ = new QPushButton(tr("RUN FULL ANALYSIS"));
    ai_run_->setProperty("fincept_agent_risk", "spend"); // LLM call
    ai_run_->setCursor(Qt::PointingHandCursor);
    ai_run_->setMinimumHeight(36);
    ai_run_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                   "  font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700; letter-spacing:2px; }"
                                   "QPushButton:hover { background:%3; }"
                                   "QPushButton:disabled { background:%4; color:%5; }")
                               .arg(amber, ui::colors::TEXT_ON_ACCENT(), ui::colors::AMBER_DIM(), border_dim, text3));
    cl->addWidget(ai_run_);

    ai_meta_ = new QLabel;
    ai_meta_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;").arg(text3));
    cl->addWidget(ai_meta_);

    lay->addWidget(cfg);

    // Results: Manual pane (class C — an LLM call). Each analysis type of each
    // portfolio is one view with its own remembered result.
    ai_pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::Manual, page);
    // No list price to give set_cost_credits (the provider bills tokens): its RUN / RETRY /
    // RUN AGAIN buttons call the LLM — "spend" for agents operating the UI.
    ai_pane_->set_agent_risk(QStringLiteral("spend"));
    ai_pane_->set_content(build_result_view(ai_pane_->content_parent(), &ai_kpis_, &ai_content_));
    ai_pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Text);
    ai_pane_->set_requirements({ui::Requirement::llm()});
    ai_pane_->set_max_age(kInsightsMaxAgeS);
    ai_pane_->set_timeout(kInsightsTimeoutS);
    ai_pane_->set_status_label(ai_meta_);
    ai_pane_->set_remembered_source([this](const QString& view, QJsonValue* payload, QDateTime* as_of) {
        const auto it = ai_store_.constFind(view);
        if (it == ai_store_.constEnd() || !it->as_of.isValid())
            return false;
        *payload = stored_payload(*it);
        *as_of = it->as_of;
        return true;
    });
    ai_pane_->set_start([this](quint64 id) { start_ai(id); });
    ai_pane_->set_cancel([this](quint64 id) {
        // Superseded / given up: the late answer is ignored (AgentService cannot stop it).
        if (ai_pending_pane_id_ != id)
            return;
        ai_pending_pane_id_ = 0;
        ai_pending_req_id_.clear();
        ai_pending_type_.clear();
        ai_pending_portfolio_id_.clear();
        header_status_->clear();
    });
    ai_pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) {
        render_payload(ai_kpis_, ai_content_, p);
    });
    connect(ai_pane_, &ui::AutoLoadPane::result_shown, this, [this]() { update_run_buttons(); });
    connect(ai_pane_, &ui::AutoLoadPane::running_changed, this, [this]() { update_run_buttons(); });
    connect(ai_run_, &QPushButton::clicked, this, [this]() {
        if (!ui::check_requirements({ui::Requirement::llm()}).ok) {
            ai_pane_->recheck_requirements(); // shows what is missing and where to set it
            return;
        }
        ai_pane_->run();
    });
    lay->addWidget(ai_pane_, 1);
    configure_ai_preview(ai_type_);
    return page;
}

QWidget* PortfolioInsightsPanel::build_agent_page() {
    const QString bg = ui::colors::BG_BASE();
    const QString surface = ui::colors::BG_SURFACE();
    const QString border_dim = ui::colors::BORDER_DIM();
    const QString amber = ui::colors::AMBER();
    const QString text1 = ui::colors::TEXT_PRIMARY();
    const QString text2 = ui::colors::TEXT_SECONDARY();
    const QString text3 = ui::colors::TEXT_TERTIARY();

    auto* page = new QWidget;
    page->setStyleSheet(QString("background:%1;").arg(bg));
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    agent_stack_ = new QStackedWidget(page);
    outer->addWidget(agent_stack_);

    // 0 — discovering the agents (finagent_core, Python)
    agent_discovering_ = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Text, agent_stack_);
    agent_discovering_->set_message(tr("Discovering agents…"));
    agent_stack_->addWidget(agent_discovering_);

    // 1 — none found / discovery failed
    agent_none_ = new ui::EmptyState(agent_stack_);
    auto rediscover = [this]() {
        discovery_done_ = false;
        discovery_failed_ = false;
        discovered_.clear();
        reload_agents();
    };
    auto open_agent_config = []() {
        EventBus::instance().publish("nav.switch_screen", {{"screen_id", QStringLiteral("agent_config")}});
    };
    // Failed: RETRY (primary) / OPEN AGENT CONFIG. None found: OPEN AGENT CONFIG (primary) / CHECK AGAIN.
    connect(agent_none_, &ui::EmptyState::primary_clicked, this, [this, rediscover, open_agent_config]() {
        if (discovery_failed_)
            rediscover();
        else
            open_agent_config();
    });
    connect(agent_none_, &ui::EmptyState::secondary_clicked, this, [this, rediscover, open_agent_config]() {
        if (discovery_failed_)
            open_agent_config();
        else
            rediscover();
    });
    agent_stack_->addWidget(agent_none_);

    // 2 — ready: agent picker + result pane
    auto* ready = new QWidget(agent_stack_);
    auto* lay = new QVBoxLayout(ready);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto* cfg = new QWidget(ready);
    cfg->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;").arg(surface, border_dim));
    auto* cl = new QVBoxLayout(cfg);
    cl->setContentsMargins(16, 12, 16, 12);
    cl->setSpacing(8);

    agent_select_label_ = new QLabel(tr("SELECT AGENT"));
    agent_select_label_->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; letter-spacing:1.5px;").arg(text3));
    cl->addWidget(agent_select_label_);

    agent_cb_ = new QComboBox;
    agent_cb_->setMinimumHeight(30);
    agent_cb_->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                     "  padding:0 10px; font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                                     "QComboBox::drop-down { border:none; width:20px; }"
                                     "QComboBox QAbstractItemView { background:%1; color:%2; border:1px solid %3;"
                                     "  selection-background-color:" + ui::colors::alpha(ui::colors::AMBER, 0.15) +
                                     "; selection-color:%2; }")
                                 .arg(bg, text1, border_dim));
    connect(agent_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (agent_desc_)
            agent_desc_->setText(agent_cb_->currentData(Qt::UserRole + 1).toString());
        select_agent_view();
    });
    cl->addWidget(agent_cb_);

    agent_desc_ = new QLabel;
    agent_desc_->setWordWrap(true);
    agent_desc_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; line-height:1.5;").arg(text2));
    cl->addWidget(agent_desc_);

    agent_run_ = new QPushButton(tr("RUN AGENT"));
    agent_run_->setProperty("fincept_agent_risk", "spend"); // agent run (model quota)
    agent_run_->setCursor(Qt::PointingHandCursor);
    agent_run_->setMinimumHeight(36);
    agent_run_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                      "  font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700; letter-spacing:2px; }"
                                      "QPushButton:hover { background:%3; }"
                                      "QPushButton:disabled { background:%4; color:%5; }")
                                  .arg(amber, ui::colors::TEXT_ON_ACCENT(), ui::colors::AMBER_DIM(), border_dim, text3));
    cl->addWidget(agent_run_);

    agent_meta_ = new QLabel;
    agent_meta_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;").arg(text3));
    cl->addWidget(agent_meta_);

    lay->addWidget(cfg);

    agent_pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::Manual, ready);
    agent_pane_->set_agent_risk(QStringLiteral("spend")); // its re-run buttons run the agent's LLM
    agent_pane_->set_content(build_result_view(agent_pane_->content_parent(), &agent_kpis_, &agent_content_));
    agent_pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Text);
    agent_pane_->set_requirements({ui::Requirement::llm()});
    agent_pane_->set_max_age(kInsightsMaxAgeS);
    agent_pane_->set_timeout(kInsightsTimeoutS);
    agent_pane_->set_status_label(agent_meta_);
    agent_pane_->set_remembered_source([this](const QString& view, QJsonValue* payload, QDateTime* as_of) {
        const auto it = agent_store_.constFind(view);
        if (it == agent_store_.constEnd() || !it->as_of.isValid())
            return false;
        *payload = stored_payload(*it);
        *as_of = it->as_of;
        return true;
    });
    agent_pane_->set_start([this](quint64 id) { start_agent(id); });
    agent_pane_->set_cancel([this](quint64 id) {
        if (agent_pending_pane_id_ != id)
            return;
        agent_partial_timer_->stop();
        agent_pending_pane_id_ = 0;
        agent_pending_req_id_.clear();
        agent_pending_id_.clear();
        agent_pending_portfolio_id_.clear();
        agent_streaming_text_.clear();
        header_status_->clear();
    });
    agent_pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) {
        render_payload(agent_kpis_, agent_content_, p);
    });
    connect(agent_pane_, &ui::AutoLoadPane::result_shown, this, [this]() { update_run_buttons(); });
    connect(agent_pane_, &ui::AutoLoadPane::running_changed, this, [this]() { update_run_buttons(); });
    connect(agent_run_, &QPushButton::clicked, this, [this]() {
        if (!ui::check_requirements({ui::Requirement::llm()}).ok) {
            agent_pane_->recheck_requirements();
            return;
        }
        agent_pane_->run();
    });
    lay->addWidget(agent_pane_, 1);
    agent_stack_->addWidget(ready);

    agent_partial_timer_ = new QTimer(this);
    agent_partial_timer_->setSingleShot(true);
    agent_partial_timer_->setInterval(kInsightsPartialMs);
    connect(agent_partial_timer_, &QTimer::timeout, this, [this]() {
        if (agent_pending_pane_id_ == 0 || agent_streaming_text_.isEmpty())
            return;
        QJsonObject p;
        p.insert(QStringLiteral("markdown"), agent_streaming_text_);
        p.insert(QStringLiteral("partial"), true);
        p.insert(QStringLiteral("snapshot"), agent_pending_snapshot_);
        agent_pane_->deliver_partial(agent_pending_pane_id_, p);
    });

    agent_stack_->setCurrentIndex(0);
    return page;
}

// ── Views, previews ──────────────────────────────────────────────────────────

QString PortfolioInsightsPanel::ai_view_key(const QString& type) const {
    return summary_.portfolio.id + QLatin1Char('/') + type;
}

QString PortfolioInsightsPanel::agent_view_key(const QString& agent_id) const {
    return summary_.portfolio.id + QLatin1Char('/') + agent_id;
}

void PortfolioInsightsPanel::configure_ai_preview(const QString& type) {
    QString title, what;
    if (type == QLatin1String("risk")) {
        title = tr("Risk analysis");
        what = tr("Concentration, correlation and downside scenarios for this book, in plain language.");
    } else if (type == QLatin1String("rebalance")) {
        title = tr("Rebalancing plan");
        what = tr("Target weights for each holding, with the reasoning behind every change.");
    } else if (type == QLatin1String("opportunities")) {
        title = tr("Opportunities");
        what = tr("Undervalued positions and gaps in the allocation worth a closer look.");
    } else {
        title = tr("Full portfolio review");
        what = tr("A broad review of holdings, allocation and performance with concrete next steps.");
    }
    auto spec = ui::EmptyState::preview_spec(title, what + QLatin1Char(' ') + tr("Sends your holdings to the configured LLM."),
                                             tr("RUN %1 ANALYSIS").arg(insights_type_label(type)),
                                             insights_ai_cost_note(), ui::EmptyState::Tone::Warning,
                                             ui::LoadingSkeleton::Shape::Text);
    const bool empty_book = summary_.holdings.isEmpty();
    if (empty_book) {
        spec.note = tr("Add a position first — there is nothing to analyse yet.");
        spec.note_tone = ui::EmptyState::Tone::Info;
    }
    const QString ccy = pfmt::base_ccy(summary_);
    spec.details = {{tr("Portfolio"), summary_.portfolio.name.isEmpty() ? QStringLiteral("—") : summary_.portfolio.name},
                    {tr("Positions"), QString::number(summary_.total_positions)},
                    {tr("NAV"), ui::formatting::money(summary_.total_market_value, ccy)}};
    spec.details_heading = tr("WHAT IT SENDS");
    ai_pane_->preview()->apply(spec);
    ai_pane_->preview()->set_primary_enabled(!empty_book);
}

void PortfolioInsightsPanel::configure_agent_preview() {
    const QString name = agent_cb_ && agent_cb_->count() > 0 ? agent_cb_->currentText() : tr("Agent");
    const QString desc = agent_cb_ ? agent_cb_->currentData(Qt::UserRole + 1).toString() : QString();
    QString what = desc.trimmed().isEmpty() ? tr("Runs this agent on the portfolio and returns its analysis.")
                                            : desc.trimmed();
    if (!what.endsWith(QLatin1Char('.')))
        what += QLatin1Char('.');
    auto spec = ui::EmptyState::preview_spec(name, what + QLatin1Char(' ') + tr("Sends your holdings to the configured LLM."),
                                             tr("RUN AGENT"), tr("Uses your LLM provider — may use credits"),
                                             ui::EmptyState::Tone::Warning, ui::LoadingSkeleton::Shape::Text);
    const bool empty_book = summary_.holdings.isEmpty();
    if (empty_book) {
        spec.note = tr("Add a position first — there is nothing to analyse yet.");
        spec.note_tone = ui::EmptyState::Tone::Info;
    }
    const QString ccy = pfmt::base_ccy(summary_);
    spec.details = {{tr("Portfolio"), summary_.portfolio.name.isEmpty() ? QStringLiteral("—") : summary_.portfolio.name},
                    {tr("Positions"), QString::number(summary_.total_positions)},
                    {tr("NAV"), ui::formatting::money(summary_.total_market_value, ccy)}};
    spec.details_heading = tr("WHAT IT SENDS");
    agent_pane_->preview()->apply(spec);
    agent_pane_->preview()->set_primary_enabled(!empty_book);
}

void PortfolioInsightsPanel::set_ai_type(const QString& type) {
    ai_type_ = type;
    ai_full_->setChecked(type == "full");
    ai_risk_->setChecked(type == "risk");
    ai_rebal_->setChecked(type == "rebalance");
    ai_opport_->setChecked(type == "opportunities");
    configure_ai_preview(type); // before switching: the preview belongs to the view shown next
    if (!summary_.portfolio.id.isEmpty())
        ai_pane_->set_view(ai_view_key(type), ui::AutoLoadPane::Policy::Manual);
    update_run_buttons();
}

void PortfolioInsightsPanel::select_agent_view() {
    if (!agent_pane_ || !agent_cb_)
        return;
    const QString aid = agent_cb_->currentData().toString();
    if (aid.isEmpty() || summary_.portfolio.id.isEmpty())
        return;
    configure_agent_preview();
    agent_pane_->set_view(agent_view_key(aid), ui::AutoLoadPane::Policy::Manual);
    update_run_buttons();
}

void PortfolioInsightsPanel::update_run_buttons() {
    const bool has_holdings = !summary_.holdings.isEmpty();
    const QString why = has_holdings ? QString() : tr("Add a position first");
    if (ai_run_ && ai_pane_) {
        const QString label = insights_type_label(ai_type_);
        const QString text = ai_pane_->has_result() ? tr("RE-RUN %1 ANALYSIS").arg(label)
                                                    : tr("RUN %1 ANALYSIS").arg(label);
        if (ai_run_->text() != text)
            ai_run_->setText(text);
        ai_run_->setEnabled(has_holdings && !ai_pane_->is_running());
        ai_run_->setToolTip(why);
    }
    if (agent_run_ && agent_pane_ && agent_cb_) {
        const QString text = agent_pane_->has_result() ? tr("RE-RUN AGENT") : tr("RUN AGENT");
        if (agent_run_->text() != text)
            agent_run_->setText(text);
        agent_run_->setEnabled(has_holdings && !agent_pane_->is_running() &&
                               !agent_cb_->currentData().toString().isEmpty());
        agent_run_->setToolTip(why);
    }
}

void PortfolioInsightsPanel::set_summary(const portfolio::PortfolioSummary& summary) {
    summary_ = summary;
    // Reload only when the portfolio actually changes (or on first show, when
    // last_portfolio_id_ is empty — e.g. a freshly-recreated panel after the
    // user navigated away and back). Same-portfolio re-entry keeps the live
    // in-memory stores untouched.
    if (summary.portfolio.id != last_portfolio_id_) {
        last_portfolio_id_ = summary.portfolio.id;
        load_persisted_results(summary.portfolio.id); // clears + repopulates the stores from disk
    }
    // Previews name the inputs (positions, NAV) — keep them current; the views
    // follow the portfolio (each keeps its own remembered result).
    set_ai_type(ai_type_);
    select_agent_view();
    update_run_buttons();
}

// ── Persistence ──────────────────────────────────────────────────────────────

QJsonObject PortfolioInsightsPanel::stored_payload(const Stored& s) {
    QJsonObject p;
    p.insert(QStringLiteral("markdown"), s.markdown);
    if (s.ms >= 0)
        p.insert(QStringLiteral("ms"), static_cast<double>(s.ms));
    if (!s.snapshot.isEmpty())
        p.insert(QStringLiteral("snapshot"), s.snapshot);
    return p;
}

void PortfolioInsightsPanel::persist_result(const QString& kind, const QString& portfolio_id, const QString& id,
                                            const Stored& s) {
    if (portfolio_id.isEmpty() || id.isEmpty())
        return;
    QJsonObject o;
    o["r"] = s.markdown;
    o["t"] = s.as_of.toString(Qt::ISODate); // real timestamp: the "as of" of the remembered result
    if (s.ms >= 0)
        o["ms"] = static_cast<double>(s.ms);
    if (!s.snapshot.isEmpty())
        o["s"] = s.snapshot;
    const QString key = QStringLiteral("pi.%1.%2.%3").arg(kind, portfolio_id, id);
    auto r = SettingsRepository::instance().set(key, QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)),
                                                kInsightsCategory);
    if (r.is_err())
        LOG_WARN("InsightsPanel", logf("Insight result could not be saved",
                                       {{"kind", kind}, {"portfolio", portfolio_id}, {"error", QString::fromStdString(r.error())}}));
}

void PortfolioInsightsPanel::load_persisted_results(const QString& portfolio_id) {
    ai_store_.clear();
    agent_store_.clear();
    if (portfolio_id.isEmpty())
        return;

    auto r = SettingsRepository::instance().get_by_category(kInsightsCategory);
    if (r.is_err()) {
        LOG_WARN("InsightsPanel", logf("Saved insight results could not be read",
                                       {{"portfolio", portfolio_id}, {"error", QString::fromStdString(r.error())}}));
        return;
    }

    const QString ai_prefix = QString("pi.ai.%1.").arg(portfolio_id);
    const QString agent_prefix = QString("pi.agent.%1.").arg(portfolio_id);
    for (const auto& row : r.value()) {
        const bool is_ai = row.key.startsWith(ai_prefix);
        const bool is_agent = !is_ai && row.key.startsWith(agent_prefix);
        if (!is_ai && !is_agent)
            continue;
        Stored s;
        QJsonParseError err;
        const auto doc = QJsonDocument::fromJson(row.value.toUtf8(), &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject o = doc.object();
            s.markdown = o.value("r").toString();
            s.as_of = insights_parse_time(o.value("t").toString());
            s.ms = o.value("ms").isDouble() ? static_cast<qint64>(o.value("ms").toDouble()) : -1;
            s.snapshot = o.value("s").toObject();
        } else {
            s.markdown = row.value; // legacy/plain value
        }
        // Results saved before the timestamp was stored: the row's own write time.
        if (!s.as_of.isValid())
            s.as_of = insights_parse_time(row.updated_at);
        if (s.markdown.trimmed().isEmpty() || !s.as_of.isValid())
            continue;
        if (is_ai)
            ai_store_.insert(portfolio_id + QLatin1Char('/') + row.key.mid(ai_prefix.size()), s);
        else
            agent_store_.insert(portfolio_id + QLatin1Char('/') + row.key.mid(agent_prefix.size()), s);
    }
}

// ── Runs ─────────────────────────────────────────────────────────────────────

QJsonObject PortfolioInsightsPanel::snapshot() const {
    QJsonObject o;
    o.insert(QStringLiteral("nav"), summary_.total_market_value);
    o.insert(QStringLiteral("pnl"), summary_.total_unrealized_pnl);
    o.insert(QStringLiteral("pnl_pct"), summary_.total_unrealized_pnl_percent);
    o.insert(QStringLiteral("positions"), summary_.total_positions);
    o.insert(QStringLiteral("ccy"), pfmt::base_ccy(summary_));
    return o;
}

QString PortfolioInsightsPanel::build_portfolio_context() const {
    // Model input, not UI text: machine-readable numbers (C locale, no grouping) with
    // the ISO code beside every amount; a price is in its own quote currency ("GBp").
    const auto num = [](double v, int dp) {
        const QString s = ui::formatting::export_number(v, dp);
        return s.isEmpty() ? QStringLiteral("n/a") : s;
    };
    const QString base = pfmt::base_ccy(summary_);
    QStringList lines;
    lines << QString("Portfolio: %1  |  NAV: %2 %3  |  Base currency: %3")
                 .arg(summary_.portfolio.name, num(summary_.total_market_value, 2), base);
    lines << QString("Total P&L: %1%  |  Day Change: %2%  |  Positions: %3")
                 .arg(num(summary_.total_unrealized_pnl_percent, 2), num(summary_.total_day_change_percent, 2))
                 .arg(summary_.total_positions);
    lines << "";
    lines << "Holdings:";
    for (const auto& h : summary_.holdings) {
        const QString qccy = h.price_currency.isEmpty() ? h.currency : h.price_currency;
        lines << QString("  %1 [%2]  qty %3 @ %4 %5  |  P&L %6%  |  wt %7%")
                     .arg(h.symbol, h.sector.isEmpty() ? QStringLiteral("Unclassified") : h.sector, num(h.quantity, 8),
                          num(h.current_price, 8), qccy, num(h.unrealized_pnl_percent, 2), num(h.weight, 1));
    }
    return lines.join("\n");
}

void PortfolioInsightsPanel::start_ai(quint64 request_id) {
    if (summary_.holdings.isEmpty()) {
        ai_pane_->fail(request_id, tr("Add a position first — there is nothing to analyse yet."), tr("No positions"));
        return;
    }
    if (!ai_chat::LlmService::instance().is_configured()) {
        ai_pane_->fail(request_id,
                       tr("No AI model is configured. Choose a provider and a model in Settings > LLM Config."),
                       tr("No AI model configured"));
        return;
    }

    // Serialize the portfolio summary so the Python side can format it however
    // the analysis_type requires. Keys mirror what finagent_core expects.
    QJsonArray holdings;
    for (const auto& h : summary_.holdings) {
        QJsonObject o;
        o["symbol"] = h.symbol;
        o["sector"] = h.sector.isEmpty() ? QStringLiteral("Unclassified") : h.sector;
        o["quantity"] = h.quantity;
        o["avg_buy_price"] = h.avg_buy_price;
        o["current_price"] = h.current_price;
        // The two prices above are in the quote currency (minor units kept: "GBp"); the
        // amounts below are in the portfolio's base currency (summary "currency").
        o["price_currency"] = h.price_currency.isEmpty() ? h.currency : h.price_currency;
        o["market_value"] = h.market_value;
        o["cost_basis"] = h.cost_basis;
        o["unrealized_pnl"] = h.unrealized_pnl;
        o["unrealized_pnl_percent"] = h.unrealized_pnl_percent;
        o["weight"] = h.weight;
        holdings.append(o);
    }
    QJsonObject summary_json;
    summary_json["name"] = summary_.portfolio.name;
    summary_json["currency"] = summary_.portfolio.currency;
    summary_json["total_market_value"] = summary_.total_market_value;
    summary_json["total_cost_basis"] = summary_.total_cost_basis;
    summary_json["total_unrealized_pnl"] = summary_.total_unrealized_pnl;
    summary_json["total_unrealized_pnl_percent"] = summary_.total_unrealized_pnl_percent;
    summary_json["total_day_change_percent"] = summary_.total_day_change_percent;
    summary_json["total_positions"] = summary_.total_positions;
    summary_json["holdings"] = holdings;
    // Include a plain-text fallback for Python paths that ignore the structured
    // summary and just forward the prompt verbatim to the LLM.
    summary_json["context_text"] = build_portfolio_context();

    ai_pending_pane_id_ = request_id;
    ai_pending_type_ = ai_type_;
    ai_pending_portfolio_id_ = summary_.portfolio.id;
    ai_pending_snapshot_ = snapshot();
    ai_pane_->set_loading_text(tr("Running the %1 analysis through your LLM…").arg(insights_type_label(ai_type_)));
    header_status_->setText(tr("● running %1…").arg(insights_type_label(ai_type_)));

    // The result arrives asynchronously, so storing the id after the call cannot miss it
    // (unless the service failed at once and error_occurred already closed this run).
    const QString req = services::AgentService::instance().run_portfolio_analysis(ai_type_, summary_json);
    if (ai_pending_pane_id_ == request_id)
        ai_pending_req_id_ = req;
}

void PortfolioInsightsPanel::start_agent(quint64 request_id) {
    if (summary_.holdings.isEmpty()) {
        agent_pane_->fail(request_id, tr("Add a position first — there is nothing to analyse yet."),
                          tr("No positions"));
        return;
    }
    const QString agent_id = agent_cb_->currentData().toString();
    if (agent_id.isEmpty()) {
        agent_pane_->fail(request_id, tr("No agent is selected. Create one in Agent Config, then return here."),
                          tr("No agent"));
        return;
    }
    if (!ai_chat::LlmService::instance().is_configured()) {
        agent_pane_->fail(request_id,
                          tr("No AI model is configured. Choose a provider and a model in Settings > LLM Config."),
                          tr("No AI model configured"));
        return;
    }

    // Resolve the friendly agent name from the cached finagent list (the
    // canonical source). Fall back to the dropdown label if the cache was
    // cleared between populate and click.
    QString agent_name;
    for (const auto& a : services::AgentService::instance().cached_agents()) {
        if (a.id == agent_id) {
            agent_name = a.name;
            break;
        }
    }
    if (agent_name.isEmpty())
        agent_name = agent_cb_->currentText();

    // Compose the query that finagent_core will run through the configured
    // agent: portfolio context first, then a task prompt. The agent's own
    // system prompt (configured in Agent Config) shapes the style.
    const QString query = build_portfolio_context() + "\n\nTask: Analyze this portfolio as " + agent_name +
                          ". Provide clear sections, concrete recommendations, and quantify exposures "
                          "where data allows.";

    QJsonObject config;
    config["agent_id"] = agent_id;

    agent_partial_timer_->stop();
    agent_streaming_text_.clear();
    agent_pending_pane_id_ = request_id;
    agent_pending_id_ = agent_id;
    agent_pending_portfolio_id_ = summary_.portfolio.id;
    agent_pending_snapshot_ = snapshot();
    agent_pane_->set_loading_text(tr("Running %1 on this portfolio…").arg(agent_name));
    header_status_->setText(tr("● running %1…").arg(agent_name));
    const QString req = services::AgentService::instance().run_agent_streaming(query, config);
    if (agent_pending_pane_id_ == request_id)
        agent_pending_req_id_ = req;
}

// ── Rendering ────────────────────────────────────────────────────────────────

void PortfolioInsightsPanel::render_payload(ui::KpiStrip* kpis, QTextBrowser* text, const QJsonValue& payload) {
    const QJsonObject p = payload.toObject();
    const bool partial = p.value(QStringLiteral("partial")).toBool(false);
    // The book the answer is about: the snapshot taken at run time; for results saved
    // before snapshots existed, today's book (labelled "now").
    QJsonObject snap = p.value(QStringLiteral("snapshot")).toObject();
    const bool at_run = !snap.isEmpty();
    if (!at_run)
        snap = snapshot();
    const QString ccy = snap.value(QStringLiteral("ccy")).toString(pfmt::base_ccy(summary_));
    const QString when = at_run ? tr("at run time") : tr("now");
    if (auto* t = kpis->tile(QStringLiteral("nav"))) {
        t->set_value(ui::formatting::money(snap.value(QStringLiteral("nav")).toDouble(), ccy,
                                           ui::formatting::Compact::FromSettings));
        t->set_sub_label(when);
    }
    if (auto* t = kpis->tile(QStringLiteral("pnl"))) {
        const double pnl = snap.value(QStringLiteral("pnl")).toDouble();
        const double pct = snap.value(QStringLiteral("pnl_pct")).toDouble();
        t->set_value(ui::formatting::signed_money(pnl, ccy, ui::formatting::Compact::FromSettings));
        if (std::isfinite(pct))
            t->set_delta(pct); // signed "+1.23%" with ▲/▼ — never colour alone
        else
            t->clear_delta();
        t->set_sub_label(when);
    }
    if (auto* t = kpis->tile(QStringLiteral("positions"))) {
        t->set_value(QString::number(snap.value(QStringLiteral("positions")).toInt()));
        t->set_sub_label(when);
    }

    const QString md = p.value(QStringLiteral("markdown")).toString();
    if (partial) {
        // Streaming: plain text, followed at the bottom.
        text->setPlainText(md);
        auto* sb = text->verticalScrollBar();
        sb->setValue(sb->maximum());
    } else {
        render_markdown(text, md);
    }
    const double ms = p.value(QStringLiteral("ms")).toDouble(-1);
    QLabel* meta = text == ai_content_ ? ai_meta_ : agent_meta_;
    if (meta)
        meta->setToolTip(ms >= 0 ? tr("The model took %1 s").arg(ui::formatting::number(ms / 1000.0, 1)) : QString());
}

void PortfolioInsightsPanel::render_markdown(QTextBrowser* target, const QString& markdown) {
    if (!target)
        return;
    target->setHtml(ui::MarkdownRenderer::render(markdown.trimmed().isEmpty() ? tr("(empty response)") : markdown));
    target->verticalScrollBar()->setValue(0);
}

// ── Tabs, agents ─────────────────────────────────────────────────────────────

void PortfolioInsightsPanel::open_tab(Tab tab) {
    switch_tab(tab);
    show();
    raise();
    setFocus();
}

void PortfolioInsightsPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (current_tab_ == Tab::Agent)
        reload_agents();
}

void PortfolioInsightsPanel::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) {
        hide();
        emit close_requested();
        return;
    }
    QWidget::keyPressEvent(e);
}

void PortfolioInsightsPanel::switch_tab(Tab tab) {
    current_tab_ = tab;
    tab_ai_btn_->setChecked(tab == Tab::AI);
    tab_agent_btn_->setChecked(tab == Tab::Agent);
    pages_->setCurrentIndex(tab == Tab::AI ? 0 : 1);
    if (tab == Tab::Agent)
        reload_agents();
}

void PortfolioInsightsPanel::show_agent_page(int page) {
    if (agent_stack_ && agent_stack_->currentIndex() != page)
        agent_stack_->setCurrentIndex(page);
}

void PortfolioInsightsPanel::reload_agents() {
    if (!agent_cb_ || in_reload_)
        return;
    in_reload_ = true;

    // Single source of truth: finagent_core agents from AgentService (its cache,
    // else the last list it delivered). We do NOT fall back to
    // AgentConfigRepository — that table only contains user-saved overrides
    // (often 0 or 2 rows) which would mislead the user into thinking only those
    // agents are available.
    QVector<services::AgentInfo> cached = services::AgentService::instance().cached_agents();
    if (cached.isEmpty())
        cached = discovered_;
    if (cached.isEmpty()) {
        if (discovering_) {
            show_agent_page(0);
        } else if (discovery_done_) {
            // Answered: an error, or genuinely no agent. Never re-discovered by itself.
            if (discovery_failed_) {
                agent_none_->apply(ui::EmptyState::error_spec(
                    discovery_error_.isEmpty() ? tr("The agents could not be listed. Python may still be setting up.")
                                               : discovery_error_,
                    tr("Agents could not be loaded"), tr("RETRY")));
                agent_none_->set_secondary_action(tr("OPEN AGENT CONFIG"));
            } else {
                auto spec = ui::EmptyState::no_data_spec(
                    tr("No agents yet"), tr("Create an agent in Agent Config, then come back to run it on this portfolio."),
                    tr("OPEN AGENT CONFIG"));
                spec.secondary = tr("CHECK AGAIN");
                agent_none_->apply(spec);
            }
            show_agent_page(1);
        } else {
            // Cold — show the skeleton and kick off discovery; agents_discovered (or
            // error_occurred "discover_agents") brings us back here.
            discovering_ = true;
            show_agent_page(0);
            services::AgentService::instance().discover_agents();
            in_reload_ = false;
            if (!discovering_) // answered synchronously: show what it said (discovery_done_ is set)
                reload_agents();
            return;
        }
        in_reload_ = false;
        return;
    }

    const QString prev = agent_cb_->currentData().toString();
    agent_cb_->blockSignals(true);
    agent_cb_->clear();
    for (const auto& a : cached) {
        const QString label = a.category.isEmpty() ? a.name : QString("[%1] %2").arg(a.category, a.name);
        agent_cb_->addItem(label, a.id);
        agent_cb_->setItemData(agent_cb_->count() - 1, a.description, Qt::UserRole + 1);
    }
    const int idx = prev.isEmpty() ? 0 : agent_cb_->findData(prev);
    agent_cb_->setCurrentIndex(idx < 0 ? 0 : idx);
    agent_cb_->blockSignals(false);
    if (agent_desc_)
        agent_desc_->setText(agent_cb_->currentData(Qt::UserRole + 1).toString());
    show_agent_page(2);
    select_agent_view();
    in_reload_ = false;
}

void PortfolioInsightsPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PortfolioInsightsPanel::retranslateUi() {
    if (header_title_)
        header_title_->setText(tr("PORTFOLIO INSIGHTS"));
    if (header_close_btn_)
        header_close_btn_->setToolTip(tr("Close  (Esc)"));
    if (tab_ai_btn_)
        tab_ai_btn_->setText(tr("AI ANALYSIS"));
    if (tab_agent_btn_)
        tab_agent_btn_->setText(tr("AGENT RUNNER"));
    if (ai_type_label_)
        ai_type_label_->setText(tr("ANALYSIS TYPE"));
    if (agent_select_label_)
        agent_select_label_->setText(tr("SELECT AGENT"));
    if (ai_full_)
        ai_full_->setText(tr("FULL"));
    if (ai_risk_)
        ai_risk_->setText(tr("RISK"));
    if (ai_rebal_)
        ai_rebal_->setText(tr("REBALANCE"));
    if (ai_opport_)
        ai_opport_->setText(tr("OPPS"));
    if (agent_discovering_)
        agent_discovering_->set_message(tr("Discovering agents…"));
    for (ui::KpiStrip* strip : {ai_kpis_, agent_kpis_}) {
        if (!strip)
            continue;
        if (auto* t = strip->tile(QStringLiteral("nav")))
            t->set_label(tr("NAV"));
        if (auto* t = strip->tile(QStringLiteral("pnl")))
            t->set_label(tr("UNREALISED P&L"));
        if (auto* t = strip->tile(QStringLiteral("positions")))
            t->set_label(tr("POSITIONS"));
    }
    configure_ai_preview(ai_type_);
    if (agent_cb_ && agent_cb_->count() > 0)
        configure_agent_preview();
    update_run_buttons();
}

} // namespace fincept::screens
