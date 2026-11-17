// ChatMessagePanel.cpp — Chat Mode's centre: start page / transcript document + composer.
//
// 2026-11-17 revamp. The run logic (attach, replay, answer, proposals, recovery) is
// the WP2 panel's, ported onto transcript items: every message, activity row, card
// and note is a transcript::Item in TranscriptView, which only builds widgets for
// what is on screen. Streaming text is parsed incrementally: the complete blocks of
// the reply (up to the last blank line outside a code fence) are parsed once and
// appended; only the open tail is parsed again, at most 25 times a second.

#include "screens/chat_mode/ChatMessagePanel.h"

#include "auth/AuthTypes.h"
#include "core/logging/Logger.h"
#include "screens/chat_mode/ChatComposer.h"
#include "screens/chat_mode/ChatModeService.h"
#include "screens/chat_mode/ChatStartPage.h"
#include "screens/chat_mode/ComposerContextBar.h"
#include "screens/chat_mode/RunViewLogic.h"
#include "screens/chat_mode/RunViewWidgets.h"
#include "screens/chat_mode/TranscriptView.h"
#include "services/agent_tasks/ArtefactService.h"
#include "services/agent_tasks/CloudSseMapping.h"
#include "services/agent_tasks/RunPlanPrompts.h"
#include "services/agent_tasks/TaskRunService.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EnterprisePromo.h"
#include "ui/widgets/ScreenCommandBar.h"

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QThreadPool>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::chat_mode {

namespace agt = fincept::agent_tasks;
namespace tr_ = fincept::chat_mode::transcript;

namespace chat_message_panel_detail {

QString cmp_tr(const char* text, const char* disambiguation = nullptr, int n = -1) {
    return QCoreApplication::translate("fincept::chat_mode::ChatMessagePanel", text, disambiguation, n);
}

/// A server timestamp (ISO 8601, UTC when it has no offset) as a QDateTime.
QDateTime cmp_parse_time(const QString& iso) {
    const QString s = iso.trimmed();
    if (s.isEmpty())
        return {};
    QDateTime dt = QDateTime::fromString(s, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(s, Qt::ISODate);
    if (!dt.isValid()) {
        static const QRegularExpression kMicros(QStringLiteral("(\\.\\d{3})\\d+"));
        QString trimmed = s;
        trimmed.replace(kMicros, QStringLiteral("\\1"));
        dt = QDateTime::fromString(trimmed, Qt::ISODateWithMs);
    }
    if (dt.isValid() && dt.timeSpec() == Qt::LocalTime)
        dt.setTimeZone(QTimeZone::utc());
    return dt;
}

QString cmp_shorten(const QString& text, int max_chars) {
    const QString t = text.simplified();
    return t.size() > max_chars ? t.left(max_chars - 1) + QChar(0x2026) : t;
}

/// "Sent with: …" text from a composed prompt's context block (the attachment's name, not its text).
QString cmp_context_line(const QString& context_block) {
    QString context = context_block;
    const int body = static_cast<int>(context.indexOf(QStringLiteral("\n<<<\n")));
    if (body >= 0)
        context = context.left(body);
    context = context.simplified();
    if (context.size() > 200)
        context = context.left(199) + QChar(0x2026);
    return context;
}

tr_::ItemPtr cmp_item(tr_::ItemKind kind, int turn) {
    auto it = std::make_shared<tr_::Item>();
    it->kind = kind;
    it->turn = turn;
    return it;
}

void cmp_append_blocks(const md::Document& doc, int turn, QVector<tr_::ItemPtr>* out) {
    for (const md::Block& b : doc.blocks) {
        auto it = cmp_item(tr_::ItemKind::Markdown, turn);
        it->block = b;
        out->append(it);
    }
}

tr_::ItemPtr cmp_sources_item(const QVector<md::Link>& links, int turn) {
    QJsonArray arr;
    for (const md::Link& l : links)
        arr.append(QJsonObject{{QStringLiteral("url"), l.url}, {QStringLiteral("text"), l.text}});
    auto it = cmp_item(tr_::ItemKind::Sources, turn);
    it->data.insert(QStringLiteral("links"), arr);
    return it;
}

// ── Trade proposals (the decision path of the WP2 panel, unchanged) ─────────

/// Whether this server has POST /chat/agent/proposals/{id} (process-wide). Learned
/// from the first decision's answer; "missing" is tested again after an hour, as
/// the server can be upgraded while the terminal runs.
enum class CmpProposalRoute { Unknown, Available, Missing };
CmpProposalRoute g_cmp_proposal_route = CmpProposalRoute::Unknown;
qint64 g_cmp_proposal_route_missing_since_ms = 0;

CmpProposalRoute cmp_proposal_route() {
    if (g_cmp_proposal_route == CmpProposalRoute::Missing &&
        QDateTime::currentMSecsSinceEpoch() - g_cmp_proposal_route_missing_since_ms > 3600LL * 1000)
        g_cmp_proposal_route = CmpProposalRoute::Unknown;
    return g_cmp_proposal_route;
}

QString cmp_proposal_status_label(const QString& status) {
    if (status == QLatin1String("executed"))
        return cmp_tr("EXECUTED");
    if (status == QLatin1String("rejected"))
        return cmp_tr("REJECTED");
    if (status == QLatin1String("failed"))
        return cmp_tr("NOT EXECUTED (FAILED)");
    if (status == QLatin1String("expired"))
        return cmp_tr("EXPIRED — NOT DECIDED");
    if (status == QLatin1String("unavailable"))
        return cmp_tr("NO LONGER AVAILABLE");
    return status.toUpper();
}

QString cmp_proposal_status_text(const QString& status) {
    if (status == QLatin1String("expired"))
        return cmp_tr("This proposal expired before it was decided. Ask the agent again if you still want this trade.");
    if (status == QLatin1String("unavailable"))
        return cmp_tr("This proposal is no longer available (the agent service no longer knows it). Ask the agent again "
                      "if you still want this trade.");
    return cmp_tr("Decision: %1").arg(cmp_proposal_status_label(status));
}

QString cmp_proposal_cost_hint(CmpProposalRoute route) {
    switch (route) {
        case CmpProposalRoute::Available:
            return cmp_tr("Your decision is applied directly: no agent turn, no credits. Proposals expire after an hour.");
        case CmpProposalRoute::Missing:
            return cmp_tr("Your decision is sent to the agent as your next message (one agent turn). Proposals expire "
                          "after an hour.");
        case CmpProposalRoute::Unknown:
            break;
    }
    return cmp_tr("Your decision is applied directly (free). Where the server cannot do that yet, it is sent to the agent "
                  "as your next message instead (one agent turn). Proposals expire after an hour.");
}

/// The protocol text of the fallback (servers without the proposals route) — matches
/// the agent's prompt, so it is not translated.
QString cmp_proposal_fallback(const QJsonObject& proposal, bool approved) {
    const QString proposal_id = proposal.value(QStringLiteral("proposal_id")).toString().trimmed();
    const QString action = proposal.value(QStringLiteral("action")).toString().trimmed().toUpper();
    const QString symbol = proposal.value(QStringLiteral("symbol")).toString().trimmed();
    const double quantity = proposal.value(QStringLiteral("quantity")).toDouble();
    const double price = proposal.value(QStringLiteral("price")).toDouble();
    const QString ccy = proposal.value(QStringLiteral("currency")).toString().trimmed();
    const QString price_text = QString::number(price, 'f', 2) + (ccy.isEmpty() ? QString() : QLatin1Char(' ') + ccy);
    const QString order = QStringLiteral("the %1 order for %2 shares of %3 at %4")
                              .arg(action, QString::number(quantity, 'g', 12), symbol, price_text);
    return approved ? QStringLiteral("Approved: Execute %1. Proposal ID: %2").arg(order, proposal_id)
                    : QStringLiteral("Rejected: Do not execute %1. Proposal ID: %2").arg(order, proposal_id);
}

/// An Approval item for `proposal` with `status` ("pending" / "" = awaiting a decision).
tr_::ItemPtr cmp_proposal_item(const QJsonObject& proposal, const QString& status, int turn) {
    auto it = cmp_item(tr_::ItemKind::Approval, turn);
    it->data = proposal;
    it->ref = proposal.value(QStringLiteral("proposal_id")).toString().trimmed();
    const bool pending = status.isEmpty() || status == QLatin1String("pending");
    if (!pending) {
        it->state = static_cast<int>(tr_::ApprovalState::Decided);
        it->status = cmp_proposal_status_text(status);
        return it;
    }
    if (it->ref.isEmpty() || proposal.value(QStringLiteral("action")).toString().trimmed().isEmpty() ||
        proposal.value(QStringLiteral("symbol")).toString().trimmed().isEmpty()) {
        it->state = static_cast<int>(tr_::ApprovalState::Invalid);
        it->status = cmp_tr("This proposal is incomplete and cannot be approved here. Ask the agent to propose the "
                            "trade again.");
        return it;
    }
    it->state = static_cast<int>(tr_::ApprovalState::Pending);
    QString hint = cmp_proposal_cost_hint(cmp_proposal_route());
    const QDateTime expires = QDateTime::fromString(proposal.value(QStringLiteral("expires_at")).toString(), Qt::ISODateWithMs);
    if (expires.isValid())
        hint += QLatin1Char(' ') +
                cmp_tr("This one expires at %1.").arg(QLocale().toString(expires.toLocalTime().time(), QLocale::ShortFormat));
    it->status = hint;
    return it;
}

// ── History (worker thread) ─────────────────────────────────────────────────

struct CmpHistory {
    QVector<tr_::Turn> turns;
    QVector<tr_::ItemPtr> items;
    QString latest_question;
    qint64 last_response_ms = -1;
};

/// The stored conversation as transcript turns and items. Pure: runs on a worker.
/// `prompt_runs`: a run's prompt -> its id (this conversation's runs), so replies
/// of known runs offer NOTES.
CmpHistory cmp_build_history(const QVector<ChatMessage>& messages, const md::ParseOptions& opts,
                             const QHash<QString, QString>& prompt_runs) {
    namespace rpp = agt::run_plan_prompts;
    CmpHistory h;
    QString current_run;
    const int n = static_cast<int>(messages.size());
    for (int i = 0; i < n; ++i) {
        const ChatMessage& m = messages.at(i);
        const ChatMessage* previous = i > 0 ? &messages.at(i - 1) : nullptr;
        const bool latest = i == n - 1;
        const auto new_turn = [&h, &m](const QString& role, const QString& text) {
            tr_::Turn t;
            t.role = role;
            t.when = cmp_parse_time(m.created_at);
            t.text = text;
            if (role != QLatin1String("user")) {
                t.tokens = m.tokens_used > 0 ? m.tokens_used : -1;
                t.duration_ms = m.response_time_ms > 0 ? m.response_time_ms : -1;
            }
            h.turns.append(t);
            return static_cast<int>(h.turns.size()) - 1;
        };
        // A planned task's plan reply is shown as the plan — never as raw JSON.
        if (m.role == QLatin1String("assistant") && previous && previous->role == QLatin1String("user") &&
            m.interrupt_question().isEmpty()) {
            const rpp::RequestInfo prev = rpp::classify_request(previous->content);
            if (prev.kind == rpp::RequestKind::Plan) {
                QString why;
                int proposed = 0;
                const QVector<agt::RunStep> steps =
                    rpp::parse_plan_reply(m.content, prev.goal, rpp::kPlanHardMaxSteps, &why, &proposed);
                if (why.isEmpty() && !steps.isEmpty()) {
                    const int t = new_turn(QStringLiteral("assistant"), QString());
                    h.turns[t].label = cmp_tr("PLAN");
                    h.turns[t].run_id = current_run;
                    QJsonArray arr;
                    QStringList plain;
                    for (const agt::RunStep& s : steps) {
                        arr.append(QJsonObject{{QStringLiteral("title"), s.title},
                                               {QStringLiteral("status"), QStringLiteral("pending")},
                                               {QStringLiteral("deliverable"), s.deliverable.trimmed()}});
                        plain << QStringLiteral("%1. %2").arg(s.idx + 1).arg(s.title);
                    }
                    h.turns[t].text = plain.join(QLatin1Char('\n'));
                    auto plan = cmp_item(tr_::ItemKind::Plan, t);
                    plan->data.insert(QStringLiteral("title"), cmp_tr("Plan"));
                    plan->data.insert(QStringLiteral("steps"), arr);
                    plan->expanded = true;
                    h.items.append(plan);
                    continue;
                }
            }
        }
        if (m.role == QLatin1String("user")) {
            const rpp::RequestInfo info = rpp::classify_request(m.content);
            const QString key = info.kind == rpp::RequestKind::Plan ? info.goal.trimmed() : m.content.trimmed();
            if (info.kind == rpp::RequestKind::Plan || info.kind == rpp::RequestKind::None)
                current_run = prompt_runs.value(key);
            if (info.kind == rpp::RequestKind::Step) {
                auto note = cmp_item(tr_::ItemKind::Note, -1);
                note->text = info.title.isEmpty() ? cmp_tr("Step %1 of %2").arg(info.step).arg(info.steps)
                                                  : cmp_tr("Step %1 of %2: %3").arg(info.step).arg(info.steps).arg(info.title);
                h.items.append(note);
                continue;
            }
            if (info.kind == rpp::RequestKind::Synthesis) {
                auto note = cmp_item(tr_::ItemKind::Note, -1);
                note->text = cmp_tr("Final report");
                h.items.append(note);
                continue;
            }
            const QString stored = info.kind == rpp::RequestKind::Plan ? info.goal : m.content;
            const run_view::SplitPrompt split = run_view::split_composed_prompt(stored);
            const int t = new_turn(QStringLiteral("user"), split.text);
            auto it = cmp_item(tr_::ItemKind::UserText, t);
            it->text = split.text;
            it->text2 = cmp_context_line(split.context);
            h.items.append(it);
            if (info.kind == rpp::RequestKind::Plan) {
                auto note = cmp_item(tr_::ItemKind::Note, -1);
                note->text = cmp_tr("Planned task: the agent writes a plan, carries out each step and then writes the "
                                    "final report.");
                h.items.append(note);
            }
            continue;
        }
        // An agent reply: its text, its trade proposals, its question.
        const QString question = m.interrupt_question();
        const QJsonArray proposals = m.proposals();
        QString content = m.content;
        if (!question.isEmpty()) {
            // The server also appends the question to `content`; the question card shows it.
            const QString trimmed = content.trimmed();
            if (trimmed == question)
                content.clear();
            else if (trimmed.endsWith(question))
                content = trimmed.left(trimmed.size() - question.size()).trimmed();
        }
        const int t = new_turn(QStringLiteral("assistant"), content);
        h.turns[t].run_id = current_run;
        if (m.response_time_ms > 0)
            h.last_response_ms = m.response_time_ms;
        if (!content.trimmed().isEmpty()) {
            const md::Document doc = md::parse(content, opts);
            cmp_append_blocks(doc, t, &h.items);
            if (!doc.links.isEmpty())
                h.items.append(cmp_sources_item(doc.links, t));
        } else if (question.isEmpty() && proposals.isEmpty()) {
            auto note = cmp_item(tr_::ItemKind::Note, t);
            note->text = cmp_tr("(empty reply)");
            h.items.append(note);
        }
        for (const QJsonValue& p : proposals)
            h.items.append(cmp_proposal_item(p.toObject(), p.toObject().value(QStringLiteral("status")).toString(), t));
        if (!question.isEmpty()) {
            auto q = cmp_item(tr_::ItemKind::Question, t);
            q->text = question;
            q->text2 = latest ? cmp_tr("Your next message answers this question.")
                              : cmp_tr("The agent asked this earlier in the conversation.");
            h.items.append(q);
            if (latest)
                h.latest_question = question;
        }
    }
    return h;
}

QString cmp_banner_style() {
    return QStringLiteral(
               "QFrame#chatServiceBanner{background:%1;border:none;border-bottom:1px solid %2;}"
               "QFrame#chatServiceBanner QLabel{background:transparent;color:%3;font-size:%4px;font-family:%5;}"
               "QFrame#chatServiceBanner QPushButton{background:%6;color:%3;border:1px solid %2;padding:2px 10px;"
               "font-weight:700;font-size:%7px;font-family:%5;}"
               "QFrame#chatServiceBanner QPushButton:hover,QFrame#chatServiceBanner QPushButton:focus{border-color:%8;}"
               "QFrame#chatServiceBanner QPushButton#chatServiceDismiss{background:transparent;border:none;color:%9;}")
        .arg(ui::colors::tint(QColor(ui::colors::DANGER()), 0.12), ui::colors::DANGER(), ui::colors::TEXT_PRIMARY(),
             ui::fonts::px(-2), QString(ui::fonts::DATA_FAMILY), ui::colors::BG_RAISED(), ui::fonts::px(-3),
             ui::colors::AMBER(), ui::colors::TEXT_SECONDARY());
}

} // namespace chat_message_panel_detail

using namespace chat_message_panel_detail;

// ── Construction ─────────────────────────────────────────────────────────────

ChatMessagePanel::ChatMessagePanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    wire();
    update_page();
}

ChatMessagePanel::~ChatMessagePanel() = default;

void ChatMessagePanel::build_ui() {
    setObjectName(QStringLiteral("chatMessagePanel"));
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // One banner for a failure of the chat service itself, updated in place.
    service_banner_ = new QFrame(this);
    service_banner_->setObjectName(QStringLiteral("chatServiceBanner"));
    service_banner_->setStyleSheet(cmp_banner_style());
    auto* sb = new QHBoxLayout(service_banner_);
    sb->setContentsMargins(14, 5, 10, 5);
    sb->setSpacing(8);
    service_banner_text_ = new QLabel(service_banner_);
    service_banner_text_->setWordWrap(true);
    service_banner_text_->setTextFormat(Qt::PlainText);
    service_retry_btn_ = new QPushButton(tr("RETRY"), service_banner_);
    service_retry_btn_->setProperty("fincept_agent_risk", "spend"); // agents: re-sends a billed model request
    service_retry_btn_->setCursor(Qt::PointingHandCursor);
    service_dismiss_btn_ = new QPushButton(QStringLiteral("×"), service_banner_);
    service_dismiss_btn_->setObjectName(QStringLiteral("chatServiceDismiss"));
    service_dismiss_btn_->setCursor(Qt::PointingHandCursor);
    service_dismiss_btn_->setToolTip(tr("Hide this message"));
    service_dismiss_btn_->setAccessibleName(tr("Hide this message"));
    sb->addWidget(service_banner_text_, 1);
    sb->addWidget(service_retry_btn_);
    sb->addWidget(service_dismiss_btn_);
    service_banner_->setVisible(false);
    vl->addWidget(service_banner_);

    stack_ = new QStackedWidget(this);
    start_ = new ChatStartPage(stack_);
    view_ = new TranscriptView(stack_);
    stack_->addWidget(start_);
    stack_->addWidget(view_);
    vl->addWidget(stack_, 1);

    composer_ = new ChatComposer(this);
    vl->addWidget(composer_);

    stream_flush_timer_ = new QTimer(this);
    stream_flush_timer_->setSingleShot(true);
    stream_flush_timer_->setInterval(kStreamFlushMs);
}

void ChatMessagePanel::wire() {
    connect(service_retry_btn_, &QPushButton::clicked, this, [this]() {
        service_retry_btn_->setEnabled(false);
        service_banner_text_->setText(tr("Trying again…"));
        emit service_retry_requested();
    });
    connect(service_dismiss_btn_, &QPushButton::clicked, this, [this]() { service_banner_->setVisible(false); });
    connect(stream_flush_timer_, &QTimer::timeout, this, &ChatMessagePanel::flush_stream);

    // Composer.
    connect(composer_, &ChatComposer::submit_requested, this, &ChatMessagePanel::on_submit);
    connect(composer_, &ChatComposer::stop_requested, this, [this]() {
        if (!rv_.run_id.isEmpty())
            emit stop_requested(rv_.run_id);
    });
    connect(composer_, &ChatComposer::draft_changed, this, &ChatMessagePanel::draft_changed);
    connect(composer_, &ChatComposer::mode_toggled, this, &ChatMessagePanel::mode_toggled);
    connect(composer_, &ChatComposer::profile_changed, this, &ChatMessagePanel::profile_changed);
    connect(composer_, &ChatComposer::answer_cancelled, this, [this]() { set_awaiting_answer(false); });
    connect(composer_, &ChatComposer::template_requested, this, &ChatMessagePanel::template_requested);
    connect(composer_, &ChatComposer::templates_requested, this, &ChatMessagePanel::templates_requested);
    connect(composer_, &ChatComposer::escape_pressed, this, &ChatMessagePanel::escape_pressed);
    // The composer's inline "start this costly task?" step was confirmed.
    connect(composer_->context_bar(), &ComposerContextBar::confirmed, this, [this]() {
        send_confirmed_ = true;
        on_submit(composer_->text().trimmed());
        send_confirmed_ = false;
    });

    // Start page.
    connect(start_, &ChatStartPage::example_chosen, this, [this](const QString& prompt) {
        composer_->set_text(prompt);
        composer_->focus_input();
    });
    connect(start_, &ChatStartPage::run_open_requested, this, &ChatMessagePanel::run_open_requested);
    connect(start_, &ChatStartPage::templates_requested, this, &ChatMessagePanel::templates_requested);
    connect(start_, &ChatStartPage::leave_requested, this, &ChatMessagePanel::leave_requested);

    // Transcript.
    connect(view_, &TranscriptView::action_requested, this, &ChatMessagePanel::on_transcript_action);
    connect(view_, &TranscriptView::scroll_changed, this, &ChatMessagePanel::scroll_changed);

    // A planned run's steps changed: the plan strip follows.
    connect(&agt::TaskRunService::instance(), &agt::TaskRunService::steps_changed, this, [this](const QString& id) {
        if (id == rv_.run_id && !history_pending_)
            refresh_plan_item();
    });
}

void ChatMessagePanel::update_page() {
    const bool start = view_->is_empty() && !history_pending_ && !pending_send_ && rv_.run_id.isEmpty();
    QWidget* want = start ? static_cast<QWidget*>(start_) : static_cast<QWidget*>(view_);
    if (stack_->currentWidget() != want)
        stack_->setCurrentWidget(want);
}

md::ParseOptions ChatMessagePanel::parse_options() const {
    md::ParseOptions o;
    for (const QString& s : composer_->context_bar()->symbols())
        o.symbols.insert(s);
    if (const agt::TaskRun* r = rv_.run_id.isEmpty() ? nullptr : agt::TaskRunService::instance().find_run(rv_.run_id))
        for (const QJsonValue& v : r->context.value(QStringLiteral("symbols")).toArray())
            if (!v.toString().isEmpty())
                o.symbols.insert(v.toString());
    return o;
}

// ── Public accessors ─────────────────────────────────────────────────────────

ComposerContextBar* ChatMessagePanel::context_bar() const {
    return composer_->context_bar();
}

bool ChatMessagePanel::is_awaiting_answer() const {
    return composer_->is_awaiting_answer();
}

void ChatMessagePanel::focus_composer() {
    composer_->focus_input();
}

void ChatMessagePanel::focus_transcript() {
    if (stack_->currentWidget() == start_)
        start_->focus_first();
    else
        view_->focus_document();
}

bool ChatMessagePanel::start_page_shown() const {
    return stack_->currentWidget() == start_;
}

void ChatMessagePanel::show_start_page() {
    clear_messages();
    set_session_title({});
    start_->refresh();
}

void ChatMessagePanel::set_stream_mode(StreamMode mode) {
    composer_->set_stream_mode(mode);
}

StreamMode ChatMessagePanel::current_mode() const {
    return composer_->stream_mode();
}

QString ChatMessagePanel::draft_text() const {
    return composer_->text();
}

void ChatMessagePanel::set_draft_text(const QString& text) {
    composer_->set_text(text);
}

int ChatMessagePanel::scroll_position() const {
    return view_->scroll_value();
}

void ChatMessagePanel::set_scroll_position(int pos) {
    view_->set_scroll_value(pos);
}

void ChatMessagePanel::set_profiles(const QVector<AgentProfile>& profiles) {
    composer_->set_profiles(profiles);
}

int ChatMessagePanel::selected_profile_id() const {
    return composer_->selected_profile_id();
}

int ChatMessagePanel::preferred_profile_id() const {
    return composer_->preferred_profile_id();
}

void ChatMessagePanel::set_preferred_profile_id(int id) {
    composer_->set_preferred_profile_id(id);
}

void ChatMessagePanel::set_session_title(const QString& title) {
    // The default title is stored untranslated (see kChatDefaultSessionTitle).
    session_title_ = title.isEmpty() ? QString() : chat_display_session_title(title);
    emit session_title_changed(session_title_);
}

void ChatMessagePanel::set_credits(double credits) {
    last_credits_ = credits;
    composer_->set_credits(credits);
}

void ChatMessagePanel::set_turn_cost(double credits) {
    if (rv_.turn < 0)
        return;
    if (tr_::Turn* t = view_->turn(rv_.turn)) {
        t->credits = credits;
        view_->update_turn(rv_.turn);
    }
}

// ── Transcript helpers ───────────────────────────────────────────────────────

int ChatMessagePanel::add_user_turn(const QString& text, const QString& context_block, const QDateTime& when) {
    tr_::Turn t;
    t.role = QStringLiteral("user");
    t.when = when;
    t.text = text;
    const int index = view_->add_turn(t);
    auto it = cmp_item(tr_::ItemKind::UserText, index);
    it->text = text;
    it->text2 = cmp_context_line(context_block);
    view_->append(it);
    update_page();
    return index;
}

void ChatMessagePanel::ensure_turn() {
    if (rv_.turn >= 0)
        return;
    tr_::Turn t;
    t.role = QStringLiteral("assistant");
    t.when = QDateTime::currentDateTime();
    t.run_id = rv_.run_id;
    t.streaming = true;
    if (const agt::TaskRun* r = rv_.run_id.isEmpty() ? nullptr : agt::TaskRunService::instance().find_run(rv_.run_id)) {
        const QVector<agt::RunStep> steps = agt::TaskRunService::instance().steps(rv_.run_id);
        if (!run_view::is_single_reply(*r, steps) && steps.size() > 1 && r->current_step >= 0 &&
            r->current_step < steps.size()) {
            t.step = r->current_step;
            t.label = tr("STEP %1 OF %2").arg(r->current_step + 1).arg(steps.size());
        }
    }
    rv_.turn = view_->add_turn(t);
    rv_.turn_started_ms = QDateTime::currentMSecsSinceEpoch();
    update_page();
}

quint64 ChatMessagePanel::add_live_item(const tr_::ItemPtr& item) {
    close_text_segment();
    const quint64 id = view_->append(item);
    update_page();
    return id;
}

quint64 ChatMessagePanel::add_note(const QString& text) {
    if (history_pending_) {
        deferred_notices_ << text;
        return 0;
    }
    auto it = cmp_item(tr_::ItemKind::Note, -1);
    it->text = text;
    return add_live_item(it);
}

quint64 ChatMessagePanel::add_error(const QString& text, const QString& action, const QString& action_text,
                                    const QString& ref) {
    auto it = cmp_item(tr_::ItemKind::Error, rv_.turn);
    it->text = text;
    it->action = action;
    it->text2 = action_text;
    it->ref = ref;
    return add_live_item(it);
}

void ChatMessagePanel::add_credits_card() {
    // One per streak of 402s: never two cards in a row.
    if (last_item_is_credits_)
        return;
    last_item_is_credits_ = true;
    add_live_item(cmp_item(tr_::ItemKind::CreditsCta, rv_.turn));
}

void ChatMessagePanel::add_question(const QString& question, const QString& hint) {
    ensure_turn();
    auto it = cmp_item(tr_::ItemKind::Question, rv_.turn);
    it->text = question;
    it->text2 = hint;
    add_live_item(it);
}

void ChatMessagePanel::add_proposal(const QJsonObject& proposal, const QString& status) {
    ensure_turn();
    const tr_::ItemPtr it = cmp_proposal_item(proposal, status, rv_.turn);
    const quint64 id = add_live_item(it);
    if (!it->ref.isEmpty())
        proposal_items_.insert(it->ref, id);
}

void ChatMessagePanel::settle_proposal(const QString& proposal_id, const QString& text) {
    const quint64 id = proposal_items_.value(proposal_id, 0);
    tr_::Item* it = id ? view_->item(id) : nullptr;
    if (!it)
        return;
    it->state = static_cast<int>(tr_::ApprovalState::Decided);
    it->status = text;
    view_->changed(id);
}

void ChatMessagePanel::set_awaiting_answer(bool on, const QString& question) {
    awaiting_question_ = on ? question : QString();
    composer_->set_awaiting_answer(on, question);
}

void ChatMessagePanel::set_status(const QString& text) {
    status_text_ = text;
    if (!busy_)
        return;
    const agt::TaskRun* run = rv_.run_id.isEmpty() ? nullptr : agt::TaskRunService::instance().find_run(rv_.run_id);
    const qint64 since = run ? (run->started_at_ms > 0 ? run->started_at_ms : run->created_at_ms)
                             : QDateTime::currentMSecsSinceEpoch();
    composer_->set_activity(status_text_, since);
}

// ── Loading ──────────────────────────────────────────────────────────────────

void ChatMessagePanel::load_messages(const QVector<ChatMessage>& messages) {
    clear_messages();
    if (messages.isEmpty()) {
        update_page();
        return;
    }
    const int gen = ++load_generation_;
    history_pending_ = true;
    auto loading = cmp_item(tr_::ItemKind::Note, -1);
    loading->text = tr("Loading the conversation (%n message(s))…", nullptr, static_cast<int>(messages.size()));
    loading_item_ = view_->append(loading);
    update_page();

    // This conversation's runs by prompt (NOTES on their replies).
    QHash<QString, QString> prompt_runs;
    auto& svc = agt::TaskRunService::instance();
    for (const QString& id : svc.run_ids()) {
        const agt::TaskRun* r = svc.find_run(id);
        if (r && !session_id_.isEmpty() && r->session_uuid == session_id_)
            prompt_runs.insert(r->prompt.trimmed(), r->id);
    }
    const md::ParseOptions opts = parse_options();
    QPointer<ChatMessagePanel> self(this);
    LOG_DEBUG("ChatMessagePanel", QStringLiteral("Building a conversation messages=%1").arg(messages.size()));
    QThreadPool::globalInstance()->start([self, gen, messages, opts, prompt_runs]() {
        const CmpHistory h = cmp_build_history(messages, opts, prompt_runs);
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, gen, h]() {
                if (self)
                    self->history_ready(gen, h.turns, h.items, h.latest_question, h.last_response_ms);
            },
            Qt::QueuedConnection);
    });
}

void ChatMessagePanel::history_ready(int generation, const QVector<tr_::Turn>& turns, const QVector<tr_::ItemPtr>& items,
                                     const QString& latest_question, qint64 last_response_ms) {
    if (generation != load_generation_ || !history_pending_)
        return; // another conversation was opened meanwhile
    history_pending_ = false;
    if (loading_item_)
        view_->remove(loading_item_);
    loading_item_ = 0;
    view_->append_history(turns, items);
    for (const tr_::ItemPtr& it : items)
        if (it->kind == tr_::ItemKind::Approval && !it->ref.isEmpty())
            proposal_items_.insert(it->ref, it->id);
    if (!latest_question.isEmpty())
        set_awaiting_answer(true, latest_question);
    view_->set_approvals_enabled(!busy_);
    view_->scroll_to_bottom();
    emit last_response_changed(last_response_ms);
    if (deferred_attach_) {
        deferred_attach_ = false;
        const QString run = rv_.run_id;
        rv_ = RunView{};
        do_attach(run, deferred_attach_shown_);
    }
    const QStringList notices = deferred_notices_;
    deferred_notices_.clear();
    for (const QString& n : notices)
        add_note(n);
    update_page();
}

void ChatMessagePanel::clear_messages() {
    // The transcript only: a reply in progress belongs to TaskRunService and keeps
    // going (the screen re-attaches it when its conversation is shown again).
    ++load_generation_;
    history_pending_ = false;
    deferred_attach_ = false;
    deferred_notices_.clear();
    loading_item_ = 0;
    stream_flush_timer_->stop();
    rv_ = RunView{};
    pending_send_ = false;
    last_item_is_credits_ = false;
    turn_answers_question_.clear();
    turn_answer_text_.clear();
    set_awaiting_answer(false);
    proposal_items_.clear();
    view_->clear();
    update_busy_state();
    update_page();
}

void ChatMessagePanel::show_load_error(const QString& message) {
    clear_messages();
    add_error(tr("Could not load this conversation: %1").arg(message), QStringLiteral("retry_load"), tr("RETRY"));
    update_page();
}

// ── The attached run ─────────────────────────────────────────────────────────

void ChatMessagePanel::attach_run(const QString& run_id, bool user_bubble_shown) {
    const agt::TaskRun* run = agt::TaskRunService::instance().find_run(run_id);
    if (!run)
        return;
    if (rv_.run_id == run_id) {
        pending_send_ = false;
        update_busy_state();
        return;
    }
    if (history_pending_) {
        // The stored conversation is still being built: render the run after it, in order.
        rv_ = RunView{};
        rv_.run_id = run_id;
        pending_send_ = false;
        deferred_attach_ = true;
        deferred_attach_shown_ = user_bubble_shown;
        update_busy_state();
        return;
    }
    do_attach(run_id, user_bubble_shown);
}

void ChatMessagePanel::do_attach(const QString& run_id, bool user_bubble_shown) {
    auto& svc = agt::TaskRunService::instance();
    const agt::TaskRun* run = svc.find_run(run_id);
    if (!run)
        return;
    flush_stream(); // whatever the previous view still held
    if (!rv_.turn_finalized && rv_.turn >= 0)
        finalize_turn({});
    rv_ = RunView{};
    rv_.run_id = run_id;
    pending_send_ = false;
    if (!user_bubble_shown) {
        const run_view::SplitPrompt split = run_view::split_composed_prompt(run->prompt);
        add_user_turn(split.text, split.context,
                      QDateTime::fromMSecsSinceEpoch(run->created_at_ms > 0 ? run->created_at_ms
                                                                            : QDateTime::currentMSecsSinceEpoch()));
    }
    update_page();
    refresh_plan_item();

    // Replay what this turn has done so far (earlier turns of the run are in the
    // stored messages), then the text streamed so far.
    const bool waiting = run->status == agt::RunStatus::WaitingInput;
    QPointer<ChatMessagePanel> self(this);
    const auto replay = [self, run_id, waiting](const QVector<agt::RunEvent>& events) {
        if (!self || self->rv_.run_id != run_id)
            return; // another conversation was opened meanwhile
        RunView& v = self->rv_;
        // The current turn starts after the user's last answer or — in a planned run —
        // at its current step: the earlier steps and their replies are stored messages.
        int start = 0;
        for (int i = static_cast<int>(events.size()) - 1; i >= 0; --i) {
            const agt::RunEvent& e = events.at(i);
            if (e.kind == agt::RunEventKind::Answer) {
                start = i + 1;
                break;
            }
            if (e.kind == agt::RunEventKind::Status && e.data.contains(QStringLiteral("turn"))) {
                start = i + 1; // the step's note is its stored request
                v.steps_shown = true;
                v.last_seq = std::max(v.last_seq, e.seq);
                break;
            }
        }
        for (int i = start; i < events.size(); ++i) {
            const agt::RunEvent& e = events.at(i);
            if (e.seq <= v.last_seq)
                continue;
            // A paused turn's question and proposals are stored with its message.
            if (waiting && (e.kind == agt::RunEventKind::Question || e.kind == agt::RunEventKind::Approval)) {
                v.last_seq = std::max(v.last_seq, e.seq);
                continue;
            }
            self->render_event(e, /*replay=*/true);
        }
        auto& s = agt::TaskRunService::instance();
        const agt::TaskRun* r = s.find_run(run_id);
        if (!r)
            return;
        if (agt::is_terminal(r->status)) {
            self->on_run_finished(run_id);
        } else if (r->status != agt::RunStatus::WaitingInput) {
            const QString live = s.live_text(run_id);
            if (!live.isEmpty()) {
                self->ensure_turn();
                v.pending_text = live;
                self->flush_stream();
            }
        }
        self->update_busy_state();
        self->view_->scroll_to_bottom();
    };
    if (!svc.cached_events(run_id).isEmpty() || !agt::is_terminal(run->status))
        replay(svc.cached_events(run_id));
    else
        svc.fetch_events(run_id, this, replay);
    update_busy_state();
    on_run_updated(run_id);
}

void ChatMessagePanel::bind_pending_run(const QString& run_id) {
    pending_send_ = false;
    if (rv_.run_id == run_id)
        update_busy_state();
    else
        attach_run(run_id, /*user_bubble_shown=*/true);
}

void ChatMessagePanel::fail_pending_send(const QString& message) {
    pending_send_ = false;
    add_error(message);
    if (!turn_answers_question_.isEmpty() && !composer_->is_awaiting_answer())
        set_awaiting_answer(true, turn_answers_question_); // the answer did not go out: still waiting
    turn_answers_question_.clear();
    turn_answer_text_.clear();
    update_busy_state();
}

void ChatMessagePanel::show_error(const QString& message) {
    add_error(message);
}

void ChatMessagePanel::add_notice(const QString& text) {
    add_note(text);
}

void ChatMessagePanel::show_service_problem(const QString& text) {
    service_banner_text_->setText(text);
    service_retry_btn_->setEnabled(true);
    service_banner_->setVisible(true);
}

void ChatMessagePanel::clear_service_problem() {
    service_banner_->setVisible(false);
    service_retry_btn_->setEnabled(true);
}

void ChatMessagePanel::on_run_event(const QString& run_id, const agt::RunEvent& ev) {
    if (run_id != rv_.run_id || ev.seq <= rv_.last_seq || history_pending_)
        return; // a deferred attach replays it from the cached events
    render_event(ev, /*replay=*/false);
}

void ChatMessagePanel::start_new_turn_view() {
    flush_stream();
    if (!rv_.turn_finalized && rv_.turn >= 0)
        finalize_turn({});
    const QString run_id = rv_.run_id;
    const int last_seq = rv_.last_seq;
    const bool steps_shown = rv_.steps_shown;
    const quint64 plan = rv_.plan_item;
    rv_ = RunView{};
    rv_.run_id = run_id;
    rv_.last_seq = last_seq;
    rv_.steps_shown = steps_shown;
    rv_.plan_item = plan;
}

void ChatMessagePanel::upsert_activity(const agt::RunEvent& ev) {
    ensure_turn();
    const QString tool = ev.data.value(QStringLiteral("tool")).toString(ev.title);
    quint64 id = ev.ref_id.isEmpty() ? 0 : rv_.activity.value(ev.ref_id, 0);
    tr_::Item* it = id ? view_->item(id) : nullptr;
    if (!it) {
        auto item = cmp_item(tr_::ItemKind::Activity, rv_.turn);
        item->text = tool.isEmpty() ? tr("Tool") : tool;
        item->text2 = ev.kind == agt::RunEventKind::ToolCall
                          ? agent_tasks::artefact_logic::summarize_arguments(ev.body, 400)
                          : QString();
        item->ref = ev.ref_id;
        item->data.insert(QStringLiteral("kind"), QStringLiteral("tool"));
        item->data.insert(QStringLiteral("args"), ev.kind == agt::RunEventKind::ToolCall ? ev.body : QString());
        item->data.insert(QStringLiteral("run_id"), rv_.run_id);
        item->data.insert(QStringLiteral("step"), ev.step);
        item->data.insert(QStringLiteral("call_ts"), static_cast<double>(ev.ts_ms));
        id = add_live_item(item);
        it = view_->item(id);
        if (!ev.ref_id.isEmpty())
            rv_.activity.insert(ev.ref_id, id);
        rv_.activity_order.append(id);
    }
    if (ev.kind == agt::RunEventKind::ToolResult && it) {
        const bool ok = !ev.is_error && ev.data.value(QStringLiteral("success")).toBool(true);
        const QJsonValue d = ev.data.value(QStringLiteral("durationMs"));
        qint64 ms = d.isDouble() ? static_cast<qint64>(d.toDouble()) : -1;
        const qint64 call_ts = static_cast<qint64>(it->data.value(QStringLiteral("call_ts")).toDouble());
        if (ms < 0 && call_ts > 0 && ev.ts_ms > call_ts)
            ms = ev.ts_ms - call_ts; // terminal tools: from their call to their result
        it->state = ok ? 1 : 2;
        it->data.insert(QStringLiteral("success"), ok);
        it->data.insert(QStringLiteral("duration_ms"), static_cast<double>(ms));
        it->data.insert(QStringLiteral("preview"), ev.body);
        // Terminal tools carry their whole result; the cloud only a shortened preview.
        if (ev.data.contains(QStringLiteral("result"))) {
            it->data.insert(QStringLiteral("result"), ev.data.value(QStringLiteral("result")));
            it->data.remove(QStringLiteral("preview_only"));
        } else {
            it->data.insert(QStringLiteral("preview_only"), true);
        }
        it->result.reset(); // classified again when expanded
        view_->changed(id);
    }
}

void ChatMessagePanel::refresh_plan_item() {
    if (rv_.run_id.isEmpty())
        return;
    auto& svc = agt::TaskRunService::instance();
    const agt::TaskRun* r = svc.find_run(rv_.run_id);
    const QVector<agt::RunStep> steps = svc.steps(rv_.run_id);
    if (!r || steps.size() <= 1 || run_view::is_single_reply(*r, steps))
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QJsonArray arr;
    for (const agt::RunStep& s : steps) {
        QJsonObject o{{QStringLiteral("title"), s.title},
                      {QStringLiteral("status"), agt::step_status_id(s.status)},
                      {QStringLiteral("deliverable"), s.deliverable.trimmed()}};
        const qint64 ms = run_view::step_elapsed_ms(s, now);
        if (ms >= 0)
            o.insert(QStringLiteral("ms"), static_cast<double>(ms));
        arr.append(o);
    }
    tr_::Item* it = rv_.plan_item ? view_->item(rv_.plan_item) : nullptr;
    if (it) {
        it->data.insert(QStringLiteral("steps"), arr);
        view_->changed(rv_.plan_item);
        return;
    }
    auto item = cmp_item(tr_::ItemKind::Plan, -1);
    item->data.insert(QStringLiteral("title"), tr("Plan"));
    item->data.insert(QStringLiteral("steps"), arr);
    rv_.plan_item = add_live_item(item);
}

void ChatMessagePanel::render_event(const agt::RunEvent& ev, bool replay) {
    using K = agt::RunEventKind;
    rv_.last_seq = std::max(rv_.last_seq, ev.seq);
    if (agt::is_verbose_event(ev))
        return; // detail rows live in the ACTIVITY page
    if (rv_.turn_finalized && ev.kind != K::Answer && ev.kind != K::Note && ev.kind != K::Usage && ev.kind != K::Status &&
        ev.kind != K::Error && ev.kind != K::Final && ev.kind != K::ApprovalDecided && !rv_.finished_rendered) {
        // New activity after a paused turn's reply was folded: it belongs to the next turn.
        start_new_turn_view();
    }
    last_item_is_credits_ = false;

    switch (ev.kind) {
        case K::Plan: {
            refresh_plan_item();
            if (!rv_.plan_item) {
                ensure_turn();
                auto item = cmp_item(tr_::ItemKind::Activity, rv_.turn);
                item->text = tr("Plan");
                item->text2 = cmp_shorten(ev.body.isEmpty() ? ev.title : ev.body, 300);
                item->data.insert(QStringLiteral("kind"), QStringLiteral("plan"));
                item->data.insert(QStringLiteral("body"), ev.body.isEmpty() ? ev.title : ev.body);
                item->state = 1;
                rv_.activity_order.append(add_live_item(item));
            }
            set_status(tr("planning…"));
            break;
        }
        case K::ToolCall:
            ++rv_.tools;
            upsert_activity(ev);
            set_status(tr("running %1").arg(ev.data.value(QStringLiteral("tool")).toString(ev.title)));
            break;
        case K::ToolResult:
            if (!rv_.activity.contains(ev.ref_id) && !ev.ref_id.startsWith(QLatin1String("terminal-")))
                ++rv_.tools; // a result without its call (older server)
            upsert_activity(ev);
            break;
        case K::Delegation: {
            ensure_turn();
            const bool start = ev.data.value(QStringLiteral("phase")).toString() != QLatin1String("end");
            const quint64 known = ev.ref_id.isEmpty() ? 0 : rv_.activity.value(ev.ref_id, 0);
            tr_::Item* it = known ? view_->item(known) : nullptr;
            if (start || !it) {
                if (start)
                    ++rv_.delegations;
                auto item = cmp_item(tr_::ItemKind::Activity, rv_.turn);
                item->text = tr("Specialists");
                item->text2 = ev.title;
                item->ref = ev.ref_id;
                item->state = start ? 0 : 1;
                item->data.insert(QStringLiteral("kind"), QStringLiteral("delegation"));
                item->data.insert(QStringLiteral("body"), cmp_shorten(ev.body, 2000));
                const quint64 id = add_live_item(item);
                rv_.activity_order.append(id);
                if (!ev.ref_id.isEmpty())
                    rv_.activity.insert(ev.ref_id, id);
                if (start)
                    set_status(tr("specialists working…"));
            } else {
                it->state = 1;
                it->text2 = ev.title;
                it->data.insert(QStringLiteral("body"), cmp_shorten(ev.body, 2000));
                view_->changed(known);
            }
            break;
        }
        case K::Thinking: {
            ensure_turn();
            if (rv_.thinking_text.size() < kMaxThinkingChars) {
                if (!rv_.thinking_text.isEmpty())
                    rv_.thinking_text += QStringLiteral("\n\n");
                rv_.thinking_text += ev.body.trimmed();
                if (rv_.thinking_text.size() > kMaxThinkingChars)
                    rv_.thinking_text = rv_.thinking_text.left(kMaxThinkingChars) + QStringLiteral("\n") +
                                        tr("(longer reasoning is kept in the run's activity log)");
            }
            tr_::Item* it = rv_.reasoning_item ? view_->item(rv_.reasoning_item) : nullptr;
            if (it) {
                it->text = rv_.thinking_text;
                view_->changed(rv_.reasoning_item);
            } else {
                auto item = cmp_item(tr_::ItemKind::Reasoning, rv_.turn);
                item->text = rv_.thinking_text;
                rv_.reasoning_item = add_live_item(item);
            }
            set_status(tr("thinking…"));
            break;
        }
        case K::Question: {
            const bool resumable = ev.data.value(QStringLiteral("resumable")).toBool();
            add_question(ev.body, resumable ? tr("Your next message answers this question, and the agent continues where "
                                                 "it stopped.")
                                            : tr("Reply in the message box below: your reply is sent to the agent as the "
                                                 "next message in this conversation."));
            // A live pause arms the composer when the run reports WaitingInput; an
            // answer recovered from the conversation is armed now.
            if (resumable && ev.data.value(QStringLiteral("stored")).toBool())
                set_awaiting_answer(true, ev.body);
            break;
        }
        case K::Approval:
            add_proposal(ev.data, QStringLiteral("pending"));
            break;
        case K::ApprovalDecided: {
            const QString status = ev.data.value(QStringLiteral("status")).toString().trimmed();
            const QString message = ev.data.value(QStringLiteral("message")).toString().trimmed();
            QString text = status.toUpper();
            if (status == QLatin1String("executed"))
                text = tr("EXECUTED");
            else if (status == QLatin1String("rejected"))
                text = tr("REJECTED");
            else if (status == QLatin1String("failed"))
                text = tr("NOT EXECUTED (FAILED)");
            if (!message.isEmpty())
                text += QStringLiteral(" — ") + message;
            settle_proposal(ev.ref_id, text);
            break;
        }
        case K::Note: {
            const QJsonArray follow = ev.data.value(QStringLiteral("follow_ups")).toArray();
            if (!follow.isEmpty()) {
                rv_.follow_ups.clear();
                for (const QJsonValue& v : follow)
                    rv_.follow_ups << v.toString();
            } else if (ev.data.value(QStringLiteral("critique")).toBool()) {
                ensure_turn();
                auto item = cmp_item(tr_::ItemKind::Activity, rv_.turn);
                item->text = tr("Check");
                item->text2 = ev.title;
                item->state = 1;
                item->data.insert(QStringLiteral("kind"), QStringLiteral("check"));
                item->data.insert(QStringLiteral("body"), ev.body);
                rv_.activity_order.append(add_live_item(item));
            } else {
                add_note(ev.body.isEmpty() ? ev.title : ev.title + QStringLiteral(": ") + ev.body);
            }
            break;
        }
        case K::Status:
            if (ev.data.contains(QStringLiteral("turn"))) {
                // A planned run's next step (or its final report): a turn of its own.
                if (!rv_.turn_finalized && rv_.turn >= 0) {
                    if (!replay && !rv_.text_items.isEmpty())
                        rv_.steps_shown = true;
                    start_new_turn_view();
                }
                if (!ev.title.isEmpty())
                    add_note(ev.title);
                refresh_plan_item();
            }
            if (!ev.title.isEmpty())
                set_status(ev.title);
            if (ev.data.value(QStringLiteral("recovering")).toBool() || ev.data.value(QStringLiteral("detached")).toBool())
                add_note(ev.body.isEmpty() ? ev.title : ev.title + QStringLiteral("\n") + ev.body);
            break;
        case K::Usage:
            if (ev.data.contains(QStringLiteral("credits_charged")))
                set_turn_cost(ev.data.value(QStringLiteral("credits_charged")).toDouble());
            break;
        case K::Error:
            rv_.error_code = ev.data.value(QStringLiteral("code")).toString();
            break;
        case K::Answer:
            if (!replay)
                start_new_turn_view();
            break;
        case K::StepEnd: {
            refresh_plan_item();
            // A step's reply that was detached (or recovered after a lost stream) shows
            // only what streamed before; the step's result is the whole stored answer.
            if (replay || ev.data.value(QStringLiteral("status")).toString() != agt::step_status_id(agt::StepStatus::Done))
                break;
            const QVector<agt::RunStep> steps = agt::TaskRunService::instance().steps(rv_.run_id);
            if (ev.step < 0 || ev.step >= steps.size())
                break;
            const QString summary = steps.at(ev.step).summary;
            if (summary.trimmed().isEmpty() || rv_.turn < 0)
                break;
            const QString shown = (rv_.text + rv_.pending_text).trimmed();
            if (summary.trimmed().size() > shown.size() + 16 || !summary.contains(shown.left(200)))
                finalize_turn(summary);
            break;
        }
        case K::StepStart:
            refresh_plan_item();
            break;
        case K::Artefact:
        case K::Final:
            break; // the FILES page and the final render show these
    }
}

// ── Streaming text ───────────────────────────────────────────────────────────

void ChatMessagePanel::on_run_text_delta(const QString& run_id, int, const QString& chunk) {
    if (run_id != rv_.run_id || chunk.isEmpty() || history_pending_)
        return;
    if (rv_.turn_finalized)
        start_new_turn_view();
    ensure_turn();
    rv_.pending_text += chunk;
    if (!stream_flush_timer_->isActive())
        stream_flush_timer_->start();
}

void ChatMessagePanel::close_text_segment() {
    flush_stream();
    if (rv_.tail.isEmpty() && rv_.seg_start == rv_.text.size())
        return;
    // What streamed so far stays as it is; later text starts a new segment below
    // the item about to be added (the document keeps the order things happened in).
    rv_.tail.clear();
    rv_.seg_items.clear();
    rv_.seg_start = static_cast<int>(rv_.text.size());
    rv_.seg_parsed = 0;
    rv_.cursor.reset();
}

void ChatMessagePanel::flush_stream() {
    stream_flush_timer_->stop();
    if (rv_.pending_text.isEmpty() || rv_.turn < 0)
        return;
    rv_.text += rv_.pending_text;
    rv_.pending_text.clear();
    const QString seg = rv_.text.mid(rv_.seg_start);
    const md::ParseOptions opts = parse_options();
    const int stable = rv_.cursor.advance(seg);
    const auto add_links = [this](const QVector<md::Link>& links) {
        for (const md::Link& l : links) {
            const bool known = std::any_of(rv_.links.cbegin(), rv_.links.cend(),
                                           [&l](const md::Link& k) { return k.url == l.url; });
            if (!known)
                rv_.links.append(l);
        }
    };
    if (stable > rv_.seg_parsed) {
        // Complete blocks: parsed once, never again.
        const md::Document settled = md::parse(seg.mid(rv_.seg_parsed, stable - rv_.seg_parsed), opts);
        for (quint64 id : std::as_const(rv_.tail)) {
            view_->remove(id);
            rv_.text_items.removeAll(id);
            rv_.seg_items.removeAll(id);
        }
        rv_.tail.clear();
        for (const md::Block& b : settled.blocks) {
            auto it = cmp_item(tr_::ItemKind::Markdown, rv_.turn);
            it->block = b;
            const quint64 id = view_->append(it);
            rv_.text_items.append(id);
            rv_.seg_items.append(id);
        }
        add_links(settled.links);
        rv_.seg_parsed = stable;
    }
    // The open tail: parsed again, items updated in place.
    const md::Document tail = md::parse(seg.mid(rv_.seg_parsed), opts);
    const int keep = static_cast<int>(std::min(rv_.tail.size(), tail.blocks.size()));
    for (int k = 0; k < keep; ++k) {
        tr_::Item* it = view_->item(rv_.tail.at(k));
        if (it && it->block != tail.blocks.at(k)) {
            it->block = tail.blocks.at(k);
            view_->changed(rv_.tail.at(k));
        }
    }
    while (rv_.tail.size() > tail.blocks.size()) {
        const quint64 id = rv_.tail.takeLast();
        view_->remove(id);
        rv_.text_items.removeAll(id);
        rv_.seg_items.removeAll(id);
    }
    for (int k = keep; k < tail.blocks.size(); ++k) {
        auto it = cmp_item(tr_::ItemKind::Markdown, rv_.turn);
        it->block = tail.blocks.at(k);
        const quint64 id = view_->append(it);
        rv_.tail.append(id);
        rv_.text_items.append(id);
        rv_.seg_items.append(id);
    }
    if (tr_::Turn* t = view_->turn(rv_.turn)) {
        const bool first = t->text.isEmpty();
        t->text = rv_.text;
        if (first)
            view_->update_turn(rv_.turn);
    }
    set_status(tr("writing…"));
    update_page();
}

void ChatMessagePanel::finalize_turn(const QString& final_text) {
    flush_stream();
    if (rv_.turn_finalized)
        return;
    rv_.turn_finalized = true;
    if (rv_.turn < 0)
        return;
    const QString streamed = rv_.text;
    QString md_text = streamed;
    if (!final_text.trimmed().isEmpty() && final_text.trimmed() != streamed.trimmed()) {
        // The stored / final answer differs from what streamed: render it instead.
        md_text = final_text;
        for (quint64 id : std::as_const(rv_.text_items))
            view_->remove(id);
        rv_.text_items.clear();
        rv_.links.clear();
        const md::Document doc = md::parse(final_text, parse_options());
        for (const md::Block& b : doc.blocks) {
            auto it = cmp_item(tr_::ItemKind::Markdown, rv_.turn);
            it->block = b;
            rv_.text_items.append(view_->append(it));
        }
        rv_.links = doc.links;
    } else if (!rv_.seg_items.isEmpty()) {
        // Streaming split the open segment at blank lines (a loose list became two
        // lists): parse it once, whole, and replace its items only when that differs.
        const QString seg = rv_.text.mid(rv_.seg_start);
        const md::Document doc = md::parse(seg, parse_options());
        bool same = doc.blocks.size() == rv_.seg_items.size();
        for (int k = 0; same && k < doc.blocks.size(); ++k) {
            const tr_::Item* it = view_->item(rv_.seg_items.at(k));
            same = it && it->block == doc.blocks.at(k);
        }
        if (!same) {
            for (quint64 id : std::as_const(rv_.seg_items)) {
                view_->remove(id);
                rv_.text_items.removeAll(id);
            }
            rv_.seg_items.clear();
            for (const md::Block& b : doc.blocks) {
                auto it = cmp_item(tr_::ItemKind::Markdown, rv_.turn);
                it->block = b;
                const quint64 id = view_->append(it);
                rv_.text_items.append(id);
                rv_.seg_items.append(id);
            }
        }
    }
    rv_.tail.clear();
    if (!rv_.links.isEmpty())
        view_->append(cmp_sources_item(rv_.links, rv_.turn));
    // Fold a long activity list into its one-line summary (click shows it again).
    if (rv_.activity_order.size() > 3) {
        QStringList parts;
        if (rv_.tools > 0)
            parts << tr("%n TOOL CALL(S)", nullptr, rv_.tools);
        if (rv_.delegations > 0)
            parts << tr("%n DELEGATION(S)", nullptr, rv_.delegations);
        const int others = static_cast<int>(rv_.activity_order.size()) - rv_.tools - rv_.delegations;
        if (others > 0)
            parts << tr("%n MORE", nullptr, others);
        auto group = cmp_item(tr_::ItemKind::ActivityGroup, rv_.turn);
        group->text = tr("ACTIVITY") + QStringLiteral("  ·  ") + parts.join(QStringLiteral("  ·  "));
        group->children = rv_.activity_order;
        for (quint64 id : std::as_const(rv_.activity_order)) {
            if (tr_::Item* it = view_->item(id)) {
                it->hidden = true;
                view_->changed(id);
            }
        }
        view_->insert_before(rv_.activity_order.first(), group);
    }
    if (tr_::Turn* t = view_->turn(rv_.turn)) {
        t->streaming = false;
        t->text = md_text;
        if (!t->when.isValid())
            t->when = QDateTime::currentDateTime();
        if (rv_.turn_started_ms > 0)
            t->duration_ms = QDateTime::currentMSecsSinceEpoch() - rv_.turn_started_ms;
        if (const agt::TaskRun* r = agt::TaskRunService::instance().find_run(rv_.run_id)) {
            if (r->executor_id() == QLatin1String(agt::kExecutorCloudTurn) && r->tokens_used > 0)
                t->tokens = r->tokens_used;
        }
        view_->update_turn(rv_.turn);
    }
    if (md_text.trimmed().isEmpty() && rv_.text_items.isEmpty() && rv_.activity_order.isEmpty() && !rv_.reasoning_item) {
        // A turn that produced nothing (stopped before the first token) leaves no empty header.
        if (tr_::Turn* t = view_->turn(rv_.turn))
            t->label = tr("NO REPLY");
        view_->update_turn(rv_.turn);
    }
}

void ChatMessagePanel::on_run_updated(const QString& run_id) {
    if (run_id != rv_.run_id)
        return;
    const agt::TaskRun* run = agt::TaskRunService::instance().find_run(run_id);
    if (!run) { // deleted
        rv_ = RunView{};
        update_busy_state();
        return;
    }
    if (history_pending_) {
        update_busy_state();
        return;
    }
    if (run->status == agt::RunStatus::WaitingInput && !rv_.turn_finalized) {
        finalize_turn({});
        const QString q = run->waiting_payload.value(QStringLiteral("question")).toString();
        if (!composer_->is_awaiting_answer())
            set_awaiting_answer(true, q.isEmpty() ? tr("The agent needs more information to continue.") : q);
        turn_answers_question_.clear();
        turn_answer_text_.clear();
    }
    // Paused between two steps: the finished step's reply is complete — render it.
    if (run->status == agt::RunStatus::Paused && !rv_.turn_finalized && rv_.turn >= 0)
        finalize_turn({});
    update_busy_state();
    if (!busy_)
        return;
    if (run->status == agt::RunStatus::Queued) {
        const int pos = agt::TaskRunService::instance().queue_position(run_id);
        set_status(pos > 0 ? tr("queued — %1 run(s) ahead; it starts when a slot frees up").arg(pos - 1) : tr("queued"));
    } else if (!run->activity.isEmpty()) {
        set_status(run->activity);
    }
}

void ChatMessagePanel::on_run_finished(const QString& run_id) {
    if (run_id != rv_.run_id || rv_.finished_rendered || history_pending_)
        return;
    const agt::TaskRun* run = agt::TaskRunService::instance().find_run(run_id);
    if (!run)
        return;
    rv_.finished_rendered = true;
    // A planned run that did not complete ends with its steps' results joined: when
    // each step already has its own reply, the last one keeps its own text.
    const bool keep_step_text = rv_.steps_shown && run->status != agt::RunStatus::Completed;
    if (rv_.turn < 0 && !keep_step_text && !run->final_text.trimmed().isEmpty())
        ensure_turn();
    finalize_turn(keep_step_text ? QString() : run->final_text);

    using S = agt::RunStatus;
    const bool was_answer = !turn_answers_question_.isEmpty() || !turn_answer_text_.isEmpty();
    if (run->status == S::Failed || run->status == S::BudgetStopped) {
        if (rv_.turn >= 0)
            if (tr_::Turn* t = view_->turn(rv_.turn)) {
                t->failed = true;
                view_->update_turn(rv_.turn);
            }
        const run_view::RunActions acts = run_view::run_actions(*run, run_view::executor_can_pause(*run));
        add_error(run->error.isEmpty() ? tr("The reply failed.") : run->error,
                  acts.retry.enabled ? QStringLiteral("retry_run") : QString(), tr("RETRY"), run_id);
        if (rv_.error_code == QLatin1String("insufficient_credits"))
            add_credits_card();
        if (was_answer && rv_.error_code == QLatin1String("not_waiting")) {
            // The server stored the reply as a message but did not answer it: offer it back.
            if (composer_->text().trimmed().isEmpty() && !turn_answer_text_.isEmpty())
                composer_->set_text(turn_answer_text_);
        } else if (was_answer && rv_.error_code != QLatin1String("resume_in_progress") &&
                   !composer_->is_awaiting_answer() && !turn_answers_question_.isEmpty()) {
            // The answer did not reach the paused run: keep the question armed.
            set_awaiting_answer(true, turn_answers_question_);
        }
    } else if (run->status == S::Cancelled || run->status == S::Interrupted) {
        if (!run->error.isEmpty())
            add_note(run->error);
    } else if (run->status == S::Completed && !rv_.follow_ups.isEmpty()) {
        auto f = cmp_item(tr_::ItemKind::FollowUps, rv_.turn);
        f->list = rv_.follow_ups;
        add_live_item(f);
    }
    turn_answers_question_.clear();
    turn_answer_text_.clear();
    if (run->started_at_ms > 0 && run->finished_at_ms > run->started_at_ms)
        emit last_response_changed(run->finished_at_ms - run->started_at_ms);
    refresh_plan_item();
    update_busy_state();
}

void ChatMessagePanel::update_busy_state() {
    const agt::TaskRun* run = rv_.run_id.isEmpty() ? nullptr : agt::TaskRunService::instance().find_run(rv_.run_id);
    const bool run_busy = run && agt::is_active(run->status);
    busy_ = run_busy || pending_send_;
    QString stop_text = tr("STOP");
    QString stop_tip;
    if (run_busy) {
        const bool waiting_for_store = run->recovering || run->detached;
        const bool cloud_turn = run->executor_id() == QLatin1String(agt::kExecutorCloudTurn);
        stop_text = waiting_for_store ? tr("STOP WAITING") : tr("STOP");
        stop_tip = waiting_for_store
                       ? tr("Stop waiting for the answer the server stores (Esc). It may still appear in the "
                            "conversation later.")
                   : cloud_turn ? tr("Stop following this reply (Esc). The server finishes it anyway — it is still "
                                     "charged — and its answer is picked up from the conversation.")
                                : tr("Stop this run (Esc). Finished steps are kept; you can retry it from any step.");
    }
    composer_->set_run_state(busy_, pending_send_, run_busy, stop_text, stop_tip);
    view_->set_approvals_enabled(!busy_);
    if (busy_) {
        const qint64 since = run ? (run->started_at_ms > 0 ? run->started_at_ms : run->created_at_ms)
                                 : QDateTime::currentMSecsSinceEpoch();
        composer_->set_activity(status_text_.isEmpty() ? tr("working…") : status_text_, since);
    } else {
        status_text_.clear();
        composer_->set_activity(QString(), 0);
    }
    update_page();
}

// ── Sending ──────────────────────────────────────────────────────────────────

void ChatMessagePanel::on_submit(const QString& text_in) {
    const QString text = text_in.trimmed();
    if (text.isEmpty())
        return;
    if (history_pending_) {
        composer_->show_note(tr("The conversation is still loading — send again in a moment."));
        return;
    }
    if (busy_) {
        composer_->show_note(tr("A reply is still going: wait for it, or STOP it first."));
        return;
    }
    if (text.size() > agt::kChatMessageMaxChars) {
        composer_->show_note(tr("This message is %1 characters long; at most %2 can be sent. Shorten it, or attach long "
                                "text as context.")
                                 .arg(QLocale().toString(text.size()), QLocale().toString(agt::kChatMessageMaxChars)),
                             true);
        return;
    }
    // A task estimated above kConfirmAboveCredits asks first, inline in the composer
    // (its START comes back through ComposerContextBar::confirmed()).
    ComposerContextBar* bar = composer_->context_bar();
    if (!composer_->is_awaiting_answer() && !send_confirmed_ && bar->needs_confirmation()) {
        bar->ask_confirmation(last_credits_ > 0 ? last_credits_ : -1);
        return;
    }
    composer_->clear_text();
    // While the agent's run is paused on a question, the composer answers it.
    send_text(text, composer_->is_awaiting_answer());
}

void ChatMessagePanel::send_text(const QString& text, bool as_answer) {
    if (text.isEmpty() || busy_)
        return;
    // An answer continues the paused run; any other message leaves the question behind.
    turn_answers_question_ = as_answer ? awaiting_question_ : QString();
    turn_answer_text_ = as_answer ? text : QString();
    if (composer_->is_awaiting_answer())
        set_awaiting_answer(false);
    last_item_is_credits_ = false;
    // The context the composer appends goes with this message: say so under it too.
    const QString context =
        as_answer ? QString() : run_view::split_composed_prompt(composer_->context_bar()->compose_prompt(text)).context;
    add_user_turn(text, context, QDateTime::currentDateTime());
    pending_send_ = true;
    status_text_ = tr("sending…");
    update_busy_state();
    view_->scroll_to_bottom();
    if (as_answer)
        emit answer_requested(text, composer_->stream_mode());
    else
        emit send_requested(text, composer_->stream_mode());
}

// ── Transcript actions ───────────────────────────────────────────────────────

void ChatMessagePanel::on_transcript_action(const tr_::Action& a) {
    const tr_::Turn* turn = view_->turn(a.turn);
    if (a.kind == QLatin1String("quote")) {
        if (turn)
            composer_->insert_quote(turn->text);
    } else if (a.kind == QLatin1String("use_again")) {
        if (turn) {
            composer_->set_text(turn->text);
            composer_->focus_input();
        }
    } else if (a.kind == QLatin1String("follow_up")) {
        // Fills the composer: the user decides whether to send (one more agent turn).
        composer_->set_text(a.value);
        composer_->focus_input();
    } else if (a.kind == QLatin1String("notes")) {
        send_turn_to_notes(a.turn);
    } else if (a.kind == QLatin1String("retry_step")) {
        if (!turn || turn->run_id.isEmpty() || turn->step < 0)
            return;
        QString err;
        const QString child = agt::TaskRunService::instance().retry_from_step(turn->run_id, turn->step, &err);
        if (child.isEmpty())
            composer_->show_note(err.isEmpty() ? tr("The step could not be retried.") : err, true);
        else
            emit run_selection_requested(child);
    } else if (a.kind == QLatin1String("approve") || a.kind == QLatin1String("deny")) {
        tr_::Item* it = view_->item(a.item);
        if (!it || it->state != static_cast<int>(tr_::ApprovalState::Pending))
            return;
        const bool approved = a.kind == QLatin1String("approve");
        decide_proposal(a.item, approved, cmp_proposal_fallback(it->data, approved), 0);
    } else if (a.kind == QLatin1String("symbol")) {
        show_symbol_menu(a.value);
    } else if (a.kind == QLatin1String("error_action")) {
        tr_::Item* it = view_->item(a.item);
        if (it) {
            it->state = 1; // pressed: one retry per error
            view_->changed(a.item);
        }
        if (a.value == QLatin1String("retry_load")) {
            emit retry_load_requested();
        } else if (a.value == QLatin1String("service_retry")) {
            emit service_retry_requested();
        } else if (a.value == QLatin1String("retry_run") && it) {
            QString err;
            const QString child = agt::TaskRunService::instance().retry_from_step(it->ref, 0, &err);
            if (child.isEmpty()) {
                composer_->show_note(err.isEmpty() ? tr("The run could not be retried.") : err, true);
                it->state = 0;
                view_->changed(a.item);
            } else {
                emit run_selection_requested(child);
            }
        }
    } else if (a.kind == QLatin1String("get_pro")) {
        ui::billing::open_plans(this);
    } else if (a.kind == QLatin1String("save_artefact")) {
        save_activity_artefact(a.item);
    } else if (a.kind == QLatin1String("open_code")) {
        if (!a.value.trimmed().isEmpty())
            emit code_open_requested(a.value);
    }
}

void ChatMessagePanel::send_turn_to_notes(int turn_index) {
    const tr_::Turn* turn = view_->turn(turn_index);
    if (!turn || turn->run_id.isEmpty() || turn->text.trimmed().isEmpty())
        return;
    QString title = turn->text.section(QLatin1Char('\n'), 0, 0).trimmed();
    while (title.startsWith(QLatin1Char('#')))
        title = title.mid(1).trimmed();
    title = cmp_shorten(title.isEmpty() ? tr("Agent reply") : title, 80);
    QJsonObject source{{QStringLiteral("origin"), QStringLiteral("reply")}};
    if (turn->step >= 0)
        source.insert(QStringLiteral("step"), turn->step);
    composer_->show_note(tr("Sending to Notes…"));
    QPointer<ChatMessagePanel> self(this);
    auto& svc = agt::ArtefactService::instance();
    svc.save_text(turn->run_id, turn->step, QString::fromLatin1(agt::kArtefactReport), title, turn->text, source, this,
                  [self](bool ok, const QString& artefact_id, const QString& err) {
                      if (!self)
                          return;
                      if (!ok) {
                          self->composer_->show_note(tr("Could not send to Notes: %1").arg(err), true);
                          return;
                      }
                      agt::ArtefactService::instance().send_to_notes(
                          artefact_id, self, [self](bool sent, int, const QString& error) {
                              if (self)
                                  self->composer_->show_note(sent ? tr("Sent to Notes (also kept in the run's FILES).")
                                                                  : tr("Could not send to Notes: %1").arg(error),
                                                             !sent);
                          });
                  });
}

void ChatMessagePanel::save_activity_artefact(quint64 item_id) {
    tr_::Item* it = view_->item(item_id);
    if (!it || it->status == QLatin1String("saving") || it->status == QLatin1String("saved"))
        return;
    const QString run_id = it->data.value(QStringLiteral("run_id")).toString();
    if (run_id.isEmpty())
        return;
    it->status = QStringLiteral("saving");
    view_->changed(item_id);
    QJsonObject source;
    if (!it->ref.isEmpty())
        source.insert(QStringLiteral("ref_id"), it->ref);
    const QString args = it->data.value(QStringLiteral("args")).toString();
    if (!args.isEmpty())
        source.insert(QStringLiteral("arguments"), args.left(500));
    QPointer<ChatMessagePanel> self(this);
    agt::ArtefactService::instance().save_tool_result(
        run_id, it->data.value(QStringLiteral("step")).toInt(-1), it->text, it->data.value(QStringLiteral("result")),
        it->data.value(QStringLiteral("preview")).toString(), source, this,
        [self, item_id](bool ok, const QString&, const QString& err) {
            if (!self)
                return;
            if (tr_::Item* item = self->view_->item(item_id)) {
                item->status = ok ? QStringLiteral("saved") : QString();
                self->view_->changed(item_id);
            }
            if (!ok)
                self->composer_->show_note(tr("Could not save the result: %1").arg(err), true);
        });
}

void ChatMessagePanel::show_symbol_menu(const QString& symbol) {
    if (symbol.isEmpty())
        return;
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setStyleSheet(ui::ScreenCommandBar::menu_css());
    QString label = symbol;
    label.replace(QLatin1Char('&'), QStringLiteral("&&"));
    QAction* open = menu->addAction(tr("Open %1 in Equity Research").arg(label));
    QObject::connect(open, &QAction::triggered, this, [this, symbol]() { emit symbol_open_requested(symbol); });
    QAction* add = menu->addAction(tr("Add %1 to the message context").arg(label));
    add->setEnabled(!composer_->context_bar()->symbols().contains(symbol));
    QObject::connect(add, &QAction::triggered, this, [this, symbol]() { composer_->context_bar()->add_symbol(symbol); });
    QAction* copy = menu->addAction(tr("Copy %1").arg(label));
    QObject::connect(copy, &QAction::triggered, this, [symbol]() {
        if (QClipboard* cb = QApplication::clipboard())
            cb->setText(symbol);
    });
    menu->popup(QCursor::pos()); // non-blocking
}

// ── Trade proposals ──────────────────────────────────────────────────────────

void ChatMessagePanel::decide_proposal(quint64 item_id, bool approved, const QString& fallback_text, int attempt) {
    tr_::Item* it = view_->item(item_id);
    if (!it)
        return; // the card is gone (another conversation was opened)
    const QString proposal_id = it->ref;
    const auto set = [this, item_id](tr_::ApprovalState state, const QString& text) {
        if (tr_::Item* item = view_->item(item_id)) {
            item->state = static_cast<int>(state);
            if (!text.isNull())
                item->status = text;
            view_->changed(item_id);
        }
    };

    if (cmp_proposal_route() == CmpProposalRoute::Missing) {
        // Older server: the decision is the next message (one agent turn, as the card says).
        if (busy_)
            return; // a turn is running; the buttons come back when it ends
        set(tr_::ApprovalState::Decided, approved ? tr("Approved — sent to the agent.") : tr("Rejected — sent to the agent."));
        send_text(fallback_text, false);
        return;
    }

    set(tr_::ApprovalState::Busy,
        attempt == 0 ? (approved ? tr("Executing the approved trade...") : tr("Rejecting the trade...")) : QString());

    QPointer<ChatMessagePanel> self(this);
    ChatModeService::instance().decide_proposal(
        proposal_id, approved, session_id_,
        [self, item_id, proposal_id, approved, fallback_text, attempt](ProposalOutcome outcome, ProposalDecision d,
                                                                       QString message, int retry_after_s) {
            if (!self)
                return;
            const auto set_state = [self, item_id](tr_::ApprovalState state, const QString& text) {
                if (tr_::Item* item = self->view_->item(item_id)) {
                    item->state = static_cast<int>(state);
                    if (!text.isNull())
                        item->status = text;
                    self->view_->changed(item_id);
                }
            };
            switch (outcome) {
                case ProposalOutcome::Decided: {
                    g_cmp_proposal_route = CmpProposalRoute::Available;
                    QString text = cmp_proposal_status_label(d.status);
                    if (!d.message.isEmpty())
                        text += QStringLiteral(" — ") + d.message;
                    if (d.replayed)
                        text += QLatin1Char('\n') + tr("This proposal had already been decided; that decision stands.");
                    set_state(tr_::ApprovalState::Decided, text);
                    emit self->proposal_decided(proposal_id, d.status);
                    return;
                }
                case ProposalOutcome::Gone:
                    g_cmp_proposal_route = CmpProposalRoute::Available;
                    set_state(tr_::ApprovalState::Decided,
                              tr("This proposal is no longer available: it was already decided, or the agent service "
                                 "restarted. Ask the agent again if you still want this trade."));
                    return;
                case ProposalOutcome::Expired:
                    g_cmp_proposal_route = CmpProposalRoute::Available;
                    set_state(tr_::ApprovalState::Decided,
                              message.isEmpty() ? tr("This proposal has expired (proposals are valid for an hour). Ask "
                                                     "the agent to propose the trade again.")
                                                : message);
                    return;
                case ProposalOutcome::InProgress:
                    g_cmp_proposal_route = CmpProposalRoute::Available;
                    // Another decision of it is still running: look again twice, then hand it back.
                    if (attempt < 2) {
                        set_state(tr_::ApprovalState::Busy, tr("This decision is being processed — checking again..."));
                        const int delay_ms = std::max(retry_after_s * 1000, 2000 * (attempt + 1));
                        QTimer::singleShot(delay_ms, self, [self, item_id, approved, fallback_text, attempt]() {
                            if (self)
                                self->decide_proposal(item_id, approved, fallback_text, attempt + 1);
                        });
                        return;
                    }
                    set_state(tr_::ApprovalState::Pending,
                              tr("This decision is still being processed. Press again in a moment to see the outcome — "
                                 "a trade is never executed twice."));
                    return;
                case ProposalOutcome::RouteMissing:
                    g_cmp_proposal_route = CmpProposalRoute::Missing;
                    g_cmp_proposal_route_missing_since_ms = QDateTime::currentMSecsSinceEpoch();
                    if (!self->view_->item(item_id))
                        return; // the card's conversation is no longer shown: send nothing into another
                    if (self->busy_) {
                        set_state(tr_::ApprovalState::Pending, cmp_proposal_cost_hint(CmpProposalRoute::Missing));
                        return;
                    }
                    set_state(tr_::ApprovalState::Decided,
                              approved ? tr("Approved — sent to the agent as your next message (one agent turn).")
                                       : tr("Rejected — sent to the agent as your next message (one agent turn)."));
                    self->send_text(fallback_text, false);
                    return;
                case ProposalOutcome::Failed:
                    // Includes a timeout, after which the decision may already have been
                    // applied: pressing again is safe — the server answers with that decision.
                    set_state(tr_::ApprovalState::Pending,
                              tr("%1 Press again to retry — a trade is never executed twice.")
                                  .arg(message.isEmpty() ? tr("Your decision could not be delivered.") : message));
                    return;
            }
        });
}

void ChatMessagePanel::on_insufficient_credits(const QString& message) {
    // `message` is the server's own explanation (required vs available credits, an
    // expired pass) plus where to recharge.
    add_error(message.isEmpty() ? tr("Insufficient credits — this request was not run.") : message);
    add_credits_card();
}

} // namespace fincept::chat_mode
