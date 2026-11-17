#include "services/report_builder/ReportAiSession.h"

#include "core/logging/Logger.h"

#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <atomic>

namespace fincept::services {

namespace report_ai_session_detail {

constexpr const char* kRasTag = "Research"; // the fixed log tag of services/report_builder (logging policy)

/// Request ids are unique in the process, so two panels never share a backend session id.
std::atomic<quint64>& ras_next_id() {
    static std::atomic<quint64> n{0};
    return n;
}

// LlmService's in-band sentinels (services/llm/LlmService.h): "\x01__TOOL_CALL_CLEAR__" drops
// what streamed so far; chunks starting with \x02 (thinking) or \x03 (tool progress) are
// not answer text. Mirrored here so the session (and its tests) need no LlmService.
bool ras_is_clear(const QString& chunk) {
    return chunk.startsWith(QStringLiteral("\x01__TOOL_CALL_CLEAR__"));
}

bool ras_is_sentinel(const QString& chunk) {
    if (chunk.isEmpty())
        return false;
    const ushort c = chunk.at(0).unicode();
    return c == 0x01 || c == 0x02 || c == 0x03;
}

} // namespace report_ai_session_detail

using namespace report_ai_session_detail;
namespace ai = report::ai;

ReportAiSession::ReportAiSession(ReportAiBackend* backend, QObject* parent) : QObject(parent), backend_(backend) {
    timeout_timer_ = new QTimer(this);
    timeout_timer_->setSingleShot(true);
    connect(timeout_timer_, &QTimer::timeout, this, &ReportAiSession::on_timeout);
}

ReportAiSession::~ReportAiSession() {
    if (state_ == State::Streaming && backend_ && active_id_ != 0)
        backend_->cancel(active_id_);
}

// ── Flow ─────────────────────────────────────────────────────────────────────

void ReportAiSession::prepare(const AiRequest& request) {
    stop_in_flight();
    request_ = request;
    stream_.clear();
    reply_.clear();
    proposal_.clear();
    hunks_.clear();
    flags_.clear();
    titles_.clear();
    error_.clear();
    settings_hint_ = false;
    route_label_.clear();
    fallback_notice_.clear();

    const bool report_action = ai::is_report_action(request_.action);
    const bool selection = !report_action && has_selection();
    if (report_action)
        sent_text_.clear();
    else if (selection)
        sent_text_ = request_.block_text.mid(request_.sel_start, request_.sel_end - request_.sel_start);
    else
        sent_text_ = request_.block_text;
    const int target = report_action ? 0 : request_.block_id;
    facts_ = ai::collect_facts(request_.doc, target);
    prompt_ = ai::build_prompt(request_.action, request_.tone, sent_text_, facts_, request_.doc, target, selection);
    original_ = report_action ? QString() : request_.block_text;
    // The number check accepts what the model was given: the facts and this text.
    source_text_ = report_action ? ai::report_excerpt(request_.doc) : request_.block_text;
    if (!prompt_.heading.isEmpty())
        source_text_ += QLatin1Char('\n') + prompt_.heading;

    if (!report_action && sent_text_.size() > kMaxTextChars) {
        error_ = tr("This block is too long to send (%1 characters; the limit is %2). Select a passage and try "
                    "again — nothing was sent.")
                     .arg(sent_text_.size())
                     .arg(kMaxTextChars);
        prompt_ = ai::Prompt(); // nothing can be sent
        set_state(State::Failed);
        return;
    }
    set_state(State::Ready);
}

bool ReportAiSession::send() {
    if (!backend_ || prompt_.user.isEmpty())
        return false;
    if (state_ == State::Idle || state_ == State::Streaming)
        return false;
    if (backend_->model_info().needs_setup)
        return false;
    return start_request();
}

bool ReportAiSession::retry() {
    return send();
}

void ReportAiSession::cancel() {
    if (state_ != State::Streaming)
        return;
    stop_in_flight();
    error_ = tr("Stopped — nothing was changed.");
    set_state(State::Cancelled);
}

void ReportAiSession::discard() {
    stop_in_flight();
    active_id_ = 0;
    request_ = AiRequest();
    facts_.clear();
    prompt_ = ai::Prompt();
    sent_text_.clear();
    source_text_.clear();
    stream_.clear();
    reply_.clear();
    original_.clear();
    proposal_.clear();
    hunks_.clear();
    flags_.clear();
    titles_.clear();
    error_.clear();
    settings_hint_ = false;
    route_label_.clear();
    fallback_notice_.clear();
    set_state(State::Idle);
}

bool ReportAiSession::accept_all() {
    if (state_ != State::Proposal)
        return false;
    const ai::Action a = request_.action;
    if (a == ai::Action::SuggestTitle)
        return !titles_.isEmpty() && use_title(titles_.first());
    ApplyResult r;
    if (a == ai::Action::SummariseReport) {
        r.kind = ApplyResult::Kind::InsertBlocks;
        // After the front matter (cover, table of contents); at the start without one.
        for (const report::ReportComponent& c : request_.doc.components) {
            const bool front = c.type == QLatin1String("cover") || c.type == QLatin1String("toc") ||
                               (r.after_block_id != 0 && c.type == QLatin1String("page_break"));
            if (!front)
                break;
            r.after_block_id = c.id;
        }
        report::ReportComponent heading;
        heading.type = QStringLiteral("heading");
        heading.content = tr("Executive summary");
        heading.config.insert(QStringLiteral("level"), QStringLiteral("1"));
        report::ReportComponent text;
        text.type = QStringLiteral("text");
        text.content = proposal_;
        text.config.insert(QStringLiteral("format"), QStringLiteral("markdown"));
        r.blocks = {heading, text};
        r.undo_text = tr("AI: executive summary");
        emit_apply(r);
        return true;
    }
    if (no_change())
        return false;
    r.kind = ApplyResult::Kind::ReplaceText;
    r.block_id = request_.block_id;
    r.new_content = proposal_;
    r.old_content = original_;
    switch (a) {
        case ai::Action::DraftFromData:
            r.undo_text = tr("AI: draft from data");
            break;
        case ai::Action::Rewrite:
            r.undo_text = tr("AI: rewrite");
            break;
        case ai::Action::Shorten:
            r.undo_text = tr("AI: shorten");
            break;
        case ai::Action::Expand:
            r.undo_text = tr("AI: expand");
            break;
        case ai::Action::FixGrammar:
            r.undo_text = tr("AI: fix grammar");
            break;
        case ai::Action::ChangeTone:
            r.undo_text = tr("AI: change tone");
            break;
        case ai::Action::SummariseReport:
        case ai::Action::SuggestTitle:
            break;
    }
    emit_apply(r);
    return true;
}

bool ReportAiSession::accept_hunks(const QSet<int>& hunks) {
    if (state_ != State::Proposal)
        return false;
    const ai::Action a = request_.action;
    if (a == ai::Action::SuggestTitle)
        return false;
    if (a == ai::Action::SummariseReport)
        return !hunks.isEmpty() && accept_all();
    const QVector<int> changes = ai::change_hunks(hunks_);
    QSet<int> valid;
    for (int i : changes)
        if (hunks.contains(i))
            valid.insert(i);
    if (valid.isEmpty())
        return false;
    if (valid.size() == changes.size())
        return accept_all(); // every change: the proposal exactly as written
    const QString content = ai::apply_hunks(hunks_, valid);
    if (content == ai::apply_hunks(hunks_, {}))
        return false;
    ApplyResult r;
    r.kind = ApplyResult::Kind::ReplaceText;
    r.block_id = request_.block_id;
    r.new_content = content;
    r.old_content = original_;
    r.undo_text = tr("AI: accept %n change(s)", nullptr, static_cast<int>(valid.size()));
    emit_apply(r);
    return true;
}

bool ReportAiSession::use_title(const QString& title) {
    if (state_ != State::Proposal || request_.action != ai::Action::SuggestTitle)
        return false;
    const QString t = title.simplified();
    if (t.isEmpty())
        return false;
    ApplyResult r;
    r.kind = ApplyResult::Kind::SetTitle;
    r.title = t;
    r.undo_text = tr("AI: report title");
    emit_apply(r);
    return true;
}

void ReportAiSession::set_timeout_ms(int ms) {
    timeout_ms_ = std::max(0, ms);
}

QString ReportAiSession::timeout_message(int ms) {
    if (ms >= 60000 && ms % 60000 == 0) {
        const int minutes = ms / 60000;
        return minutes == 1 ? tr("No answer within 1 minute — nothing was changed.")
                            : tr("No answer within %1 minutes — nothing was changed.").arg(minutes);
    }
    const int seconds = std::max(1, (ms + 999) / 1000);
    return seconds == 1 ? tr("No answer within 1 second — nothing was changed.")
                        : tr("No answer within %1 seconds — nothing was changed.").arg(seconds);
}

// ── State ────────────────────────────────────────────────────────────────────

bool ReportAiSession::has_selection() const {
    return request_.sel_start >= 0 && request_.sel_end > request_.sel_start &&
           request_.sel_end <= request_.block_text.size();
}

AiModelInfo ReportAiSession::model_info() const {
    return backend_ ? backend_->model_info() : AiModelInfo();
}

bool ReportAiSession::no_change() const {
    if (request_.action == ai::Action::SuggestTitle)
        return titles_.isEmpty();
    if (request_.action == ai::Action::SummariseReport)
        return proposal_.trimmed().isEmpty();
    return proposal_ == original_ || ai::change_hunks(hunks_).isEmpty();
}

qint64 ReportAiSession::elapsed_ms() const {
    return clock_.isValid() ? clock_.elapsed() : 0;
}

// ── Internals ────────────────────────────────────────────────────────────────

void ReportAiSession::set_state(State s) {
    state_ = s;
    emit state_changed();
}

bool ReportAiSession::start_request() {
    const quint64 id = ++ras_next_id();
    active_id_ = id;
    stream_.clear();
    reply_.clear();
    proposal_.clear();
    hunks_.clear();
    flags_.clear();
    titles_.clear();
    error_.clear();
    settings_hint_ = false;
    route_label_.clear();
    fallback_notice_.clear();
    start_model_ = backend_->model_info().label;
    clock_.start();
    LOG_INFO(kRasTag, logf("AI draft request sent", {{"action", ai::action_id(request_.action)},
                                                     {"request", id},
                                                     {"chars", prompt_.text_chars},
                                                     {"facts", prompt_.fact_count},
                                                     {"model", start_model_}}));
    set_state(State::Streaming);
    if (timeout_ms_ > 0)
        timeout_timer_->start(timeout_ms_);
    QPointer<ReportAiSession> self(this);
    backend_->start(
        id, prompt_.system, prompt_.user,
        [self](quint64 rid, const QString& chunk) {
            if (self)
                self->on_chunk(rid, chunk);
        },
        [self](quint64 rid, const AiBackendResult& result) {
            if (self)
                self->on_done(rid, result);
        });
    return true;
}

void ReportAiSession::on_chunk(quint64 id, const QString& chunk) {
    if (id == 0 || id != active_id_ || state_ != State::Streaming)
        return; // an older request, or one that was stopped
    if (timeout_ms_ > 0)
        timeout_timer_->start(timeout_ms_); // any sign of life (thinking included) restarts the idle time-out
    if (chunk.isEmpty())
        return;
    if (ras_is_clear(chunk)) {
        stream_.clear();
        emit stream_updated(stream_);
        return;
    }
    if (ras_is_sentinel(chunk))
        return;
    stream_ += chunk;
    emit stream_updated(stream_);
}

void ReportAiSession::on_done(quint64 id, const AiBackendResult& result) {
    if (id == 0 || id != active_id_ || state_ != State::Streaming)
        return;
    timeout_timer_->stop();
    route_label_ = result.route_label;
    fallback_notice_ = result.fallback_notice;
    if (result.cancelled) {
        error_ = tr("Stopped — nothing was changed.");
        log_end("cancelled");
        set_state(State::Cancelled);
        return;
    }
    if (!result.ok) {
        error_ = result.error.trimmed().isEmpty() ? tr("The model did not answer — nothing was changed.")
                                                  : result.error.trimmed();
        settings_hint_ = result.settings_hint;
        log_end("failed");
        set_state(State::Failed);
        return;
    }
    reply_ = ai::clean_reply(stream_.trimmed().isEmpty() ? result.text : stream_);
    if (reply_.isEmpty()) {
        error_ = tr("The model returned an empty answer — nothing was changed.");
        log_end("empty");
        set_state(State::Failed);
        return;
    }
    build_proposal();
    if (request_.action == ai::Action::SuggestTitle && titles_.isEmpty()) {
        error_ = tr("The model's answer held no usable title — nothing was changed.");
        log_end("empty");
        set_state(State::Failed);
        return;
    }
    log_end("ok");
    set_state(State::Proposal);
    emit proposal_ready();
}

void ReportAiSession::on_timeout() {
    if (state_ != State::Streaming)
        return;
    if (backend_ && active_id_ != 0)
        backend_->cancel(active_id_);
    error_ = timeout_message(timeout_ms_);
    log_end("timeout");
    set_state(State::Failed);
}

void ReportAiSession::stop_in_flight() {
    if (state_ != State::Streaming)
        return;
    timeout_timer_->stop();
    if (backend_ && active_id_ != 0)
        backend_->cancel(active_id_);
    log_end("cancelled");
    // The state moves on with the caller (Cancelled / Idle / Ready); late chunks and results
    // of this id are dropped because the state is no longer Streaming.
    state_ = State::Cancelled;
}

void ReportAiSession::build_proposal() {
    const ai::Action a = request_.action;
    if (a == ai::Action::SuggestTitle) {
        titles_ = ai::parse_titles(reply_);
        proposal_.clear();
        hunks_.clear();
        flags_ = ai::check_numbers(titles_.join(QLatin1Char('\n')), facts_, source_text_);
        return;
    }
    if (a == ai::Action::SummariseReport) {
        proposal_ = reply_;
        hunks_ = ai::diff_paragraphs(QString(), proposal_);
        flags_ = ai::check_numbers(proposal_, facts_, source_text_);
        return;
    }
    if (has_selection()) {
        // The reply replaces only the selection; the selection's own surrounding whitespace stays.
        const QString& t = request_.block_text;
        const int s = request_.sel_start;
        const int e = request_.sel_end;
        const QString sel = t.mid(s, e - s);
        int lead = 0;
        while (lead < sel.size() && sel.at(lead).isSpace())
            ++lead;
        int trail = 0;
        while (trail < sel.size() - lead && sel.at(sel.size() - 1 - trail).isSpace())
            ++trail;
        proposal_ = t.left(s) + sel.left(lead) + reply_ + sel.right(trail) + t.mid(e);
    } else {
        proposal_ = reply_;
    }
    hunks_ = ai::diff_paragraphs(original_, proposal_);
    flags_ = ai::check_numbers(proposal_, facts_, source_text_);
}

void ReportAiSession::emit_apply(const ApplyResult& r) {
    // Idle first: a slot that asks again (accept twice) finds nothing to accept.
    set_state(State::Idle);
    emit apply_requested(r);
}

void ReportAiSession::log_end(const char* outcome) {
    LOG_INFO(kRasTag, logf("AI draft request finished",
                           {{"action", ai::action_id(request_.action)},
                            {"request", active_id_},
                            {"outcome", outcome},
                            {"duration_ms", elapsed_ms()},
                            {"chars", prompt_.text_chars},
                            {"facts", prompt_.fact_count},
                            {"reply_chars", static_cast<int>(reply_.size())},
                            {"flagged", static_cast<int>(flags_.size())},
                            {"model", route_label_.isEmpty() ? start_model_ : route_label_}}));
}

} // namespace fincept::services
