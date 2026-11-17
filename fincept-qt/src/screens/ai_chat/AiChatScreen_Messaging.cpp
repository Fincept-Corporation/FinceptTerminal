// src/screens/ai_chat/AiChatScreen_Messaging.cpp
//
// Message lifecycle: on_send / start_request, streaming chunks, Stop (cancel → grace →
// detach), finish_turn, Regenerate / Retry / Answer-with, turn actions, the Fincept credits
// and sign-in notices, the busy state and the status / command bar figures.
//
// Part of the partial-class split of AiChatScreen.cpp. Tool activity lives in
// AiChatScreen_Tools.cpp.

#include "core/agent/AgentRiskMarkers.h"
#include "core/config/AppPaths.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/ai_chat/AiChatComposer.h"
#include "screens/ai_chat/AiChatContextRail.h"
#include "screens/ai_chat/AiChatScreen.h"
#include "screens/ai_chat/AiChatSessionRail.h"
#include "screens/ai_chat/AiChatStartPage.h"
#include "screens/ai_chat/AiChatTranscript.h"
#include "services/llm/LlmErrors.h"
#include "services/llm/LlmService.h"
#include "services/llm/ModelCatalog.h"
#include "services/llm/ModelRouter.h"
#include "storage/repositories/ChatRepository.h"
#include "ui/widgets/EnterprisePromo.h"
#include "ui/widgets/ModelBadge.h"
#include "ui/widgets/PlanHint.h"
#include "ui/widgets/Requirements.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLocale>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <memory>

namespace fincept::screens {

namespace {

constexpr const char* kAicMsgTag = "AiChat";

/// The server's own reason in a refused-sign-in error: make_fincept_error() (fed by
/// report_fincept_refusal) puts it between the fixed lead and the fixed "Sign in again …"
/// advice. Empty when the server gave none. Escaped for the notice's Markdown.
QString msg_fincept_auth_reason(const QString& error) {
    QString reason = error.mid(ai_chat::fincept_auth_error_lead().size()).trimmed();
    const qsizetype advice = reason.indexOf(QLatin1String("Sign in again (or check your Fincept API key)"));
    if (advice >= 0)
        reason = reason.left(advice).trimmed();
    for (const QChar c : {QLatin1Char('\\'), QLatin1Char('*'), QLatin1Char('_'), QLatin1Char('`'), QLatin1Char('['),
                          QLatin1Char(']'), QLatin1Char('<'), QLatin1Char('>')})
        reason.replace(c, QString(QLatin1Char('\\')) + c);
    return reason;
}

} // namespace

// ── Sending ──────────────────────────────────────────────────────────────────

void AiChatScreen::on_send() {
    if (streaming_) {
        // While a reply is streaming the Send button is the Stop button.
        on_stop_requested();
        return;
    }
    const QString raw_text = composer_->text().trimmed();
    if (raw_text.isEmpty() && files_.isEmpty())
        return;
    if (!ai_chat::LlmService::instance().is_configured()) {
        show_start_page(false);
        // Notices render Markdown; links open only when safe (http / https).
        const QString ent_url = ui::enterprise::url_with_ref(ui::enterprise::site_url(), QStringLiteral("ai"));
        add_notice(aichat::NoticeKind::Setup,
                   tr("No LLM provider configured. Go to Settings > LLM Configuration to set up a provider.") +
                       QStringLiteral("\n\n") + tr("Or use Fincept Terminal Enterprise — AI credits included.") +
                       QStringLiteral(" [") + tr("See Enterprise ↗") + QStringLiteral("](") + ent_url +
                       QStringLiteral(")"));
        return;
    }
    if (active_session_id_.isEmpty())
        create_new_session();
    if (active_session_id_.isEmpty()) {
        show_start_page(false);
        add_notice(aichat::NoticeKind::Error, tr("Failed to create chat session. Please try again."));
        return;
    }

    // Attached files are read now — only the files the user attached, only on Send.
    QVector<aichat::FilePayload> payloads;
    QVector<aichat::Attachment> file_chips;
    for (const auto& file : std::as_const(files_)) {
        const QFileInfo fi(file.ref);
        QString attach_error;
        aichat::FilePayload p;
        p.name = fi.fileName();
        QFile f(file.ref);
        if (!f.open(QIODevice::ReadOnly)) {
            attach_error = tr("Could not read the attached file \"%1\": %2").arg(fi.fileName(), f.errorString());
        } else {
            // 4 bytes / char worst case for UTF-8, so this always covers the character cap.
            const QByteArray raw = f.read(aichat::kAttachMaxChars * 4);
            p.truncated = !f.atEnd();
            f.close();
            if (raw.contains('\0')) {
                // A binary file (PDF, image, archive…) decoded as text is garbage to the model.
                attach_error = tr("\"%1\" looks like a binary file (PDF, image, archive…) and can't be sent as text. "
                                  "Attach a text-based file such as .txt, .md, .csv, .json or .ipynb.")
                                   .arg(fi.fileName());
            } else {
                p.content = QString::fromUtf8(raw);
                if (p.content.startsWith(QChar(0xFEFF)))
                    p.content.remove(0, 1);
                if (p.content.size() > aichat::kAttachMaxChars) {
                    p.content.truncate(aichat::kAttachMaxChars);
                    p.truncated = true;
                }
            }
        }
        if (!attach_error.isEmpty()) {
            // Nothing was sent — the typed message and the attachments stay.
            show_start_page(false);
            add_notice(aichat::NoticeKind::Error, attach_error);
            return;
        }
        aichat::Attachment chip = file;
        chip.truncated = p.truncated;
        chip.bytes = p.content.size();
        file_chips.append(chip);
        payloads.append(p);
    }

    // Context items (the linked symbol when attached, mentions, references) — exactly what
    // the context rail lists. One-shot: the history carries them from here on.
    const QVector<aichat::Attachment> ctx = outgoing_context();
    const QString text = aichat::build_user_wire(ctx, payloads, raw_text);
    linked_context_pending_ = false;
    context_.clear();
    files_.clear();
    refresh_context_ui();
    ScreenStateManager::instance().notify_changed(this);

    composer_->clear_text();
    show_start_page(false);
    transcript_->set_live_turn(0);
    aichat::Turn u;
    u.role = aichat::TurnRole::User;
    u.text = raw_text;
    u.attachments = ctx;
    u.attachments += file_chips;
    u.when = QDateTime::currentDateTime();
    transcript_->append_turn(u);
    transcript_->scroll_to_bottom();
    total_messages_++;
    last_failed_ = false;
    auto& llm = ai_chat::LlmService::instance();
    ChatRepository::instance().add_message(active_session_id_, "user", text, llm.active_provider(), llm.active_model());
    snippets_[active_session_id_] = aichat::snippet_of(QStringLiteral("user"), text);
    if (sessions_)
        sessions_->set_snippet(active_session_id_, snippets_.value(active_session_id_));
    LOG_INFO(kAicMsgTag, QString("Sending a message (%1 characters typed, %2 file(s), %3 context item(s))")
                             .arg(raw_text.size())
                             .arg(payloads.size())
                             .arg(ctx.size()));
    // Snapshot BEFORE appending the new turn: every request builder appends `text` as the
    // trailing user message itself.
    std::vector<ai_chat::ConversationMessage> hist_copy = history_;
    history_.push_back({"user", text});
    start_request(text, std::move(hist_copy));
}

// The history the model sees. Two cleanups the stored transcript needs:
//  * a failed turn leaves a user message with no answer, so the next turn would send two
//    consecutive user messages — several providers require alternating roles (400);
//  * consecutive assistant messages cannot occur here (regenerate replaces the superseded
//    answer), but are merged defensively for the same reason.
std::vector<ai_chat::ConversationMessage>
AiChatScreen::wire_history(const std::vector<ai_chat::ConversationMessage>& h) {
    std::vector<ai_chat::ConversationMessage> out;
    out.reserve(h.size());
    for (const auto& m : h) {
        if (!out.empty() && out.back().role == m.role) {
            out.back().content += QStringLiteral("\n\n") + m.content;
            continue;
        }
        out.push_back(m);
    }
    return out;
}

// Common tail of on_send / on_regenerate / answer-with: the user turn is already in the
// transcript; `snapshot` is the history BEFORE it.
void AiChatScreen::start_request(const QString& wire_text, std::vector<ai_chat::ConversationMessage> snapshot,
                                 const QString& skip_harness) {
    auto& llm = ai_chat::LlmService::instance();
    const QString req_session = active_session_id_;
    pending_req_session_ = req_session;
    // The route's first choice (ModelRouter); the response says who really answered.
    ai_chat::ChatRoute route;
    route.effort = composer_->effort(); // LOW … MAX, chosen in the composer (default medium)
    route.feature = QString::fromLatin1(ai_chat::kFeatureAiChat);
    route.skip_harness = skip_harness;
    pending_req_label_.clear();
    {
        const QVector<ai_chat::ModelChoice> chain = ai_chat::ModelRouter::instance().resolve_chain(route.feature);
        for (const auto& c : chain) {
            if (c.is_harness() && c.harness_id == skip_harness)
                continue;
            pending_req_provider_ = c.provider_id();
            pending_req_model_ = c.model;
            pending_req_label_ = c.label;
            break;
        }
    }
    route_notice_shown_ = false;
    const quint64 gen = ++request_gen_;
    stop_requested_ = false;
    stop_grace_timer_->stop();
    request_started_ms_ = QDateTime::currentMSecsSinceEpoch();
    tool_steps_.clear();
    tool_job_ids_.clear();

    // The streaming entry point is cancellable (session lane); the blocking chat() fallback
    // is used only for providers that cannot stream.
    stoppable_ = ai_chat::provider_supports_streaming(pending_req_provider_);

    // The answer's block exists from the start: its header names the model and says
    // STREAMING (it replaces the old typing indicator).
    aichat::Turn a;
    a.role = aichat::TurnRole::Assistant;
    a.state = aichat::TurnState::Streaming;
    a.provider = pending_req_provider_;
    a.model = pending_req_model_;
    a.route_label = pending_req_label_;
    a.when = QDateTime::currentDateTime();
    streaming_turn_ = transcript_->append_turn(a);
    transcript_->scroll_to_bottom();
    set_busy(true);

    std::vector<ai_chat::ConversationMessage> wire = wire_history(snapshot);
    // A failed turn leaves its user message unanswered; sending the next one after it would
    // put two user turns back to back. Fold the unanswered message into this one instead.
    QString outgoing = wire_text;
    if (!wire.empty() && wire.back().role == QLatin1String("user")) {
        outgoing = wire.back().content + QStringLiteral("\n\n") + wire_text;
        wire.pop_back();
    }
    const auto policy = !tools_on_ ? ai_chat::LlmService::ToolPolicy::None
                        : nav_on_  ? ai_chat::LlmService::ToolPolicy::All
                                   : ai_chat::LlmService::ToolPolicy::NoNavigation;
    QPointer<AiChatScreen> self = this;
    if (stoppable_) {
        llm.chat_streaming(
            outgoing, wire,
            [self, gen](const QString& chunk, bool done) {
                QMetaObject::invokeMethod(
                    qApp,
                    [self, chunk, done, gen]() {
                        // A stopped-and-detached request keeps streaming until it notices the
                        // cancel; its chunks must not paint anything.
                        if (!self || self->request_gen_ != gen || !self->streaming_)
                            return;
                        self->on_stream_chunk(chunk, done);
                    },
                    Qt::QueuedConnection);
            },
            policy, req_session, route);
    } else {
        const bool use_tools = tools_on_;
        (void)QtConcurrent::run([self, outgoing, wire, req_session, gen, route, use_tools]() {
            auto resp = ai_chat::LlmService::instance().chat(outgoing, wire, use_tools, route);
            // on_streaming_done() only accepts a response that carries this request's
            // session id (the signal path stamps it itself; this direct path must too).
            resp.origin_session_id = req_session;
            QMetaObject::invokeMethod(
                qApp,
                [self, resp, gen]() {
                    // Generation-matched: a detached request's late answer is dropped.
                    if (self && self->request_gen_ == gen && self->streaming_)
                        self->finish_turn(resp);
                },
                Qt::QueuedConnection);
        });
    }
}

void AiChatScreen::on_stream_chunk(const QString& chunk, bool done) {
    Q_UNUSED(done)
    if (streaming_turn_ == 0)
        return;
    // Chain-of-thought → the collapsed REASONING section (never the answer).
    const QString think_prefix = ai_chat::think_stream_prefix();
    if (chunk.startsWith(think_prefix)) {
        transcript_->stream_append_thinking(streaming_turn_, chunk.mid(think_prefix.size()));
        return;
    }
    // Tool activity → the TOOLS rows.
    const QString tool_prefix = ai_chat::tool_stream_prefix();
    if (chunk.startsWith(tool_prefix)) {
        append_tool_chunk(chunk.mid(tool_prefix.size()));
        return;
    }
    // The model switched to tool calls mid-stream: what streamed so far is aborted markup.
    if (chunk.startsWith(QStringLiteral("\x01__TOOL_CALL_CLEAR__"))) {
        transcript_->stream_drop_text(streaming_turn_);
        return;
    }
    if (chunk.isEmpty())
        return;
    // Buffered by the transcript: the tail block repaints at most ~20 times a second.
    transcript_->stream_append(streaming_turn_, chunk);
}

void AiChatScreen::on_streaming_done(ai_chat::LlmResponse response) {
    // A request the user stopped and we stopped waiting for: its answer is the first one to
    // arrive for that session afterwards (the session lane runs one request at a time, in
    // order), so it is swallowed here.
    if (!response.origin_session_id.isEmpty()) {
        auto it = abandoned_responses_.find(response.origin_session_id);
        if (it != abandoned_responses_.end()) {
            if (--it.value() <= 0)
                abandoned_responses_.erase(it);
            LOG_INFO(kAicMsgTag, QString("Discarded the late reply of a stopped request (%1)")
                                     .arg(response.cancelled ? "cancelled" : response.success ? "completed" : "failed"));
            return;
        }
    }
    // finished_streaming() is process-wide and the Quick Chat bubble listens to the same
    // one: only the response to OUR request (same session id, while waiting) is ours.
    if (!streaming_ || response.origin_session_id != pending_req_session_)
        return;
    finish_turn(response);
}

void AiChatScreen::on_route_notice(const QString& origin_session_id, const QString& feature, const QString& notice) {
    // Only our own pending request (the bubble and Quick Ask have their own).
    if (!streaming_ || origin_session_id != pending_req_session_ ||
        feature != QLatin1String(ai_chat::kFeatureAiChat) || notice.isEmpty())
        return;
    route_notice_shown_ = true;
    add_notice(aichat::NoticeKind::Route, notice);
}

quint64 AiChatScreen::add_notice(aichat::NoticeKind kind, const QString& markdown, const QString& error) {
    aichat::Turn t;
    t.role = aichat::TurnRole::Notice;
    t.notice = kind;
    t.text = markdown;
    t.error = error;
    t.when = QDateTime::currentDateTime();
    const quint64 id = transcript_->append_turn(t);
    transcript_->scroll_to_bottom();
    return id;
}

void AiChatScreen::finish_turn(const ai_chat::LlmResponse& response) {
    const quint64 turn_id = streaming_turn_;
    streaming_ = false;
    stoppable_ = false;
    stop_requested_ = false;
    stop_grace_timer_->stop();

    // The tool steps are persisted with the answer so a reloaded conversation still shows
    // which tools produced it.
    finalize_tool_steps();
    const QString trailer = aichat::tool_trailer(tool_steps_);

    const QString session = pending_req_session_;
    // Who really answered (a fallback, or the model a harness reported) — stored with the message.
    const QString provider = response.provider.isEmpty() ? pending_req_provider_ : response.provider;
    const QString model = response.provider.isEmpty() ? pending_req_model_ : response.model;
    pending_req_session_.clear();
    pending_req_provider_.clear();
    pending_req_model_.clear();
    const qint64 duration = request_started_ms_ > 0 ? QDateTime::currentMSecsSinceEpoch() - request_started_ms_ : -1;

    aichat::Turn t;
    if (const aichat::Turn* cur = transcript_->turn(turn_id))
        t = *cur;
    t.provider = provider;
    t.model = model;
    if (!response.route_label.isEmpty())
        t.route_label = response.route_label;
    t.duration_ms = duration;
    t.tools = tool_steps_;
    const QString partial = transcript_->stream_text(turn_id).trimmed();

    // The non-streaming path has no early announcement: say it now.
    if (!response.fallback_notice.isEmpty() && !route_notice_shown_)
        add_notice(aichat::NoticeKind::Route, response.fallback_notice);
    route_notice_shown_ = false;

    auto done = [this, session]() {
        streaming_turn_ = 0;
        set_busy(false);
        if (!isVisible()) {
            unread_ = true;
            if (sessions_)
                sessions_->set_unread(session, true);
        }
        update_stats();
    };

    if (!response.success && !response.cancelled && !response.unavailable_notice.isEmpty()) {
        // The harness was unavailable and the setting is "ask" / "fail": no error block —
        // the notice says what happened and what can answer.
        if (partial.isEmpty() && t.thinking.isEmpty() && t.tools.isEmpty())
            transcript_->remove_turn(turn_id);
        else {
            t.state = aichat::TurnState::Stopped;
            t.text = partial;
            transcript_->update_turn(turn_id, t);
        }
        aichat::Turn n;
        n.role = aichat::TurnRole::Notice;
        n.notice = response.fallback_offer.isEmpty() ? aichat::NoticeKind::Route : aichat::NoticeKind::RouteOffer;
        n.text = response.unavailable_notice;
        n.offer_label = response.fallback_offer;
        n.skip_harness = response.unavailable_harness;
        n.when = QDateTime::currentDateTime();
        const quint64 nid = transcript_->append_turn(n);
        transcript_->set_live_turn(nid);
        transcript_->scroll_to_bottom();
        done();
        return;
    }

    if (!response.success) {
        // Text that streamed before the failure / Stop stays on screen.
        if (response.cancelled) {
            LOG_INFO(kAicMsgTag, QString("LLM request stopped by user (%1 chars kept)").arg(partial.size()));
            t.state = aichat::TurnState::Stopped;
            if (partial.isEmpty()) {
                t.text.clear();
                transcript_->update_turn(turn_id, t);
                transcript_->set_live_turn(turn_id); // RETRY
            } else {
                // Keep the partial answer in the transcript (and tell the model it was cut off
                // on the next turn) so reloading the session shows what the user saw.
                const QString saved = partial + QStringLiteral("\n\n*(stopped)*");
                t.text = saved;
                t.tokens = response.total_tokens;
                transcript_->update_turn(turn_id, t);
                history_.push_back({"assistant", saved});
                total_messages_++;
                total_tokens_ += response.total_tokens;
                ChatRepository::instance().add_message(session, "assistant", saved + trailer, provider, model,
                                                       response.total_tokens);
                last_answer_turn_ = turn_id;
                transcript_->set_live_turn(turn_id); // REGENERATE
            }
        } else {
            LOG_WARN(kAicMsgTag, QString("LLM request failed: %1").arg(response.error));
            last_failed_ = true;
            const auto kind = ai_chat::classify_llm_error(response.error);
            if (kind != ai_chat::LlmErrorKind::Other) {
                // Out of credits / sign-in rejected: an actionable notice, not an error line.
                if (partial.isEmpty() && t.thinking.isEmpty() && t.tools.isEmpty()) {
                    transcript_->remove_turn(turn_id);
                } else {
                    t.state = aichat::TurnState::Stopped;
                    t.text = partial;
                    transcript_->update_turn(turn_id, t);
                }
                show_fincept_notice(response);
            } else {
                t.state = aichat::TurnState::Failed;
                t.text = partial;
                t.error = response.error.isEmpty() ? tr("The request failed with no reason given.") : response.error;
                transcript_->update_turn(turn_id, t);
                transcript_->set_live_turn(turn_id); // RETRY
            }
        }
        transcript_->scroll_to_bottom();
        done();
        return;
    }

    // Success. The service's content is the cleaned answer (think blocks and tool markup
    // removed) — shown AND persisted, so screen and database agree. The streamed text
    // covers a provider that streams but returns no body.
    const QString content = !response.content.trimmed().isEmpty() ? response.content : partial;
    if (content.trimmed().isEmpty()) {
        t.state = aichat::TurnState::Empty;
        t.text.clear();
        transcript_->update_turn(turn_id, t);
        transcript_->set_live_turn(turn_id); // RETRY
        LOG_WARN(kAicMsgTag, "LLM returned success with empty content");
        last_failed_ = true;
        done();
        return;
    }

    note_symbols_from_tools(); // tickers the tools looked up become chips in the answer
    t.state = aichat::TurnState::Done;
    t.text = content;
    t.tokens = response.total_tokens;
    t.prompt_tokens = response.prompt_tokens > 0 ? response.prompt_tokens : -1;
    t.completion_tokens = response.completion_tokens > 0 ? response.completion_tokens : -1;
    t.cost_usd = response.cost_usd;
    if (ai_chat::provider_is_harness(provider))
        t.billing_note = response.billing_note; // a harness answer never touches Fincept credits
    transcript_->update_turn(turn_id, t);
    history_.push_back({"assistant", content});
    total_messages_++;
    total_tokens_ += response.total_tokens;
    if (response.prompt_tokens > 0)
        run_prompt_tokens_ = std::max<qint64>(run_prompt_tokens_, 0) + response.prompt_tokens;
    if (response.completion_tokens > 0)
        run_completion_tokens_ = std::max<qint64>(run_completion_tokens_, 0) + response.completion_tokens;
    if (response.cost_usd >= 0.0)
        run_cost_usd_ = std::max(run_cost_usd_, 0.0) + response.cost_usd;
    last_answer_ms_ = duration;
    last_answer_at_ = QDateTime::currentDateTime();
    conv_provider_ = provider;
    conv_model_ = model;
    // Request-time context: the provider/model that actually produced this answer.
    ChatRepository::instance().add_message(session, "assistant", content + trailer, provider, model,
                                           response.total_tokens);
    snippets_[session] = aichat::snippet_of(QStringLiteral("assistant"), content);
    if (sessions_)
        sessions_->set_snippet(session, snippets_.value(session));
    last_answer_turn_ = turn_id;
    transcript_->set_live_turn(turn_id); // REGENERATE
    LOG_INFO(kAicMsgTag, QString("Answer received (%1 characters, %2 tokens, %3 ms)")
                             .arg(content.size())
                             .arg(response.total_tokens)
                             .arg(duration));
    done();
    if (isVisible())
        composer_->focus_editor();
}

// ── Stop ─────────────────────────────────────────────────────────────────────

void AiChatScreen::on_stop_requested() {
    if (!streaming_ || stop_requested_)
        return;
    stop_requested_ = true;
    if (!stoppable_) {
        // The blocking chat() fallback has no cancel hook — stop waiting now.
        detach_stopped_request();
        return;
    }
    ai_chat::LlmService::instance().cancel_active_request(pending_req_session_);
    composer_->set_stopping();
    update_status();
    // Most turns unwind within ~100 ms. One blocked inside a tool cannot be interrupted —
    // stop waiting for it after the grace period.
    stop_grace_timer_->start();
}

void AiChatScreen::detach_stopped_request() {
    if (!streaming_ || !stop_requested_)
        return;
    LOG_WARN(kAicMsgTag, "Stopped request did not finish within the grace period (blocked in a tool) — detaching");
    // Its chunks are dropped by generation; its finished_streaming() is the next one for
    // this session and is swallowed (non-streaming requests are dropped by generation alone).
    if (stoppable_ && !pending_req_session_.isEmpty())
        abandoned_responses_[pending_req_session_] += 1;
    ++request_gen_;
    ai_chat::LlmResponse stopped;
    stopped.cancelled = true;
    stopped.error = QStringLiteral("Request cancelled");
    stopped.origin_session_id = pending_req_session_;
    finish_turn(stopped);
}

// ── Regenerate / Retry / Answer with ─────────────────────────────────────────

void AiChatScreen::on_regenerate() {
    if (streaming_ || history_.empty() || active_session_id_.isEmpty())
        return;
    if (!ai_chat::LlmService::instance().is_configured())
        return;
    // Regenerate: the newest answer is superseded — dropped from what the model sees, kept
    // (dimmed) on screen and in the database. On reload, the last of consecutive answers wins.
    if (history_.back().role == QLatin1String("assistant")) {
        history_.pop_back();
        if (const aichat::Turn* cur = transcript_->turn(last_answer_turn_)) {
            aichat::Turn t = *cur;
            t.state = aichat::TurnState::Superseded;
            transcript_->update_turn(last_answer_turn_, t);
        }
    }
    // Retry (after a failed / stopped turn) and regenerate both resend the newest user
    // message — already in the transcript and the database.
    if (history_.empty() || history_.back().role != QLatin1String("user"))
        return;
    transcript_->set_live_turn(0);
    const QString text = history_.back().content;
    std::vector<ai_chat::ConversationMessage> snapshot(history_.begin(), history_.end() - 1);
    show_start_page(false);
    start_request(text, std::move(snapshot));
}

void AiChatScreen::on_answer_with(quint64 turn_id) {
    const aichat::Turn* t = transcript_->turn(turn_id);
    if (!t || streaming_ || history_.empty() || history_.back().role != QLatin1String("user") ||
        t->skip_harness.isEmpty())
        return;
    const QString skip = t->skip_harness;
    transcript_->set_live_turn(0);
    const QString msg = history_.back().content;
    std::vector<ai_chat::ConversationMessage> snapshot(history_.begin(), history_.end() - 1);
    start_request(msg, std::move(snapshot), skip);
}

void AiChatScreen::on_turn_action(quint64 turn_id, aichat::TurnAction action) {
    using A = aichat::TurnAction;
    switch (action) {
        case A::Copy:
        case A::AddToReport:
            return; // done by the transcript
        case A::Regenerate:
        case A::Retry:
            on_regenerate();
            return;
        case A::AnswerWith:
            on_answer_with(turn_id);
            return;
        case A::EditResend:
            if (const aichat::Turn* t = transcript_->turn(turn_id)) {
                composer_->set_text(t->text); // fills only — the user presses Send
                composer_->focus_editor();
            }
            return;
        case A::Quote:
            if (const aichat::Turn* t = transcript_->turn(turn_id)) {
                QStringList lines;
                for (const QString& l : aichat::turn_markdown(*t).split(QLatin1Char('\n')))
                    lines << QStringLiteral("> ") + l;
                composer_->insert_text(lines.join(QLatin1Char('\n')) + QStringLiteral("\n\n"));
                composer_->focus_editor();
            }
            return;
        case A::ModelSettings:
            ui::open_llm_settings();
            return;
        case A::LlmSettings:
            EventBus::instance().publish(QStringLiteral("nav.switch_screen"),
                                         QVariantMap{{"screen_id", QStringLiteral("settings")}});
            return;
        case A::ViewPlans:
            ui::billing::open_plans(this);
            return;
        case A::OpenSettings: {
            const ui::RequirementStatus now = ui::check_requirements({ui::Requirement::llm()});
            if (now.link.valid())
                ui::open_setup_link(now.link, this);
            else
                ui::open_llm_settings();
            return;
        }
    }
}

// ── Fincept credits / sign-in notice ─────────────────────────────────────────

void AiChatScreen::show_fincept_notice(const ai_chat::LlmResponse& response) {
    const bool credits = ai_chat::classify_llm_error(response.error) == ai_chat::LlmErrorKind::FinceptOutOfCredits;
    QString text = credits ? tr("**You're out of Fincept credits.**") : tr("**Fincept did not accept your sign-in.**");
    // The service's sentence carries the cost / balance figures when known.
    static const QRegularExpression rx_figures(QStringLiteral("This request needs ([^;]+); your balance is ([^.]+)\\."));
    const auto m = rx_figures.match(response.error);
    if (credits && m.hasMatch())
        text += QStringLiteral("\n\n") + tr("This request needs %1; your balance is %2.").arg(m.captured(1), m.captured(2));
    // A refused sign-in: the server's own reason when it gave one.
    if (!credits) {
        const QString reason = msg_fincept_auth_reason(response.error);
        if (!reason.isEmpty())
            text += QStringLiteral("\n\n") + reason;
    }
    text += QStringLiteral("\n\n") +
            (credits ? tr("Top up your credits, or add your own provider API key in **Settings → LLM Configuration** "
                          "to keep chatting.")
                     : tr("Sign in again (or check your Fincept API key), or add your own provider API key in "
                          "**Settings → LLM Configuration**."));
    // What the plan would change, from the server's plan table (billing::PlanAccess; empty
    // on Pro, without a table, or when Pro prices AI the same).
    if (credits) {
        const QString plan_note = ui::plan::out_of_credits_note(QStringLiteral("ai"));
        if (!plan_note.isEmpty())
            text += QStringLiteral("\n\n") + plan_note;
    }
    const quint64 id = add_notice(credits ? aichat::NoticeKind::Credits : aichat::NoticeKind::SignIn, text);
    transcript_->set_live_turn(id); // RETRY
}

// ── OPEN IN CODE TAB ─────────────────────────────────────────────────────────

void AiChatScreen::open_code_in_code_tab(const QString& code, const QString& lang) {
    Q_UNUSED(lang)
    // A one-cell notebook in the terminal's own files folder, opened in the Code tab
    // (opened only — nothing runs).
    const QString dir = QDir(AppPaths::files()).filePath(QStringLiteral("ai_chat"));
    QDir().mkpath(dir);
    const QString path = QDir(dir).filePath(
        QStringLiteral("snippet_%1.ipynb").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"))));
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(aichat::notebook_json(code)) < 0 || !f.commit()) {
        status_->show_notice(tr("Could not create the notebook: %1").arg(f.errorString()), QString(), {}, 6000,
                             ui::ScreenStatusBar::Tone::Danger);
        return;
    }
    LOG_INFO(kAicMsgTag, QString("Opened a code block in the Code tab (%1 characters)").arg(code.size()));
    EventBus::instance().publish(QStringLiteral("nav.switch_screen"),
                                 QVariantMap{{"screen_id", QStringLiteral("code_editor")},
                                             {"section", QStringLiteral("notebook:") + path}});
}

// ── Busy state, figures ──────────────────────────────────────────────────────

void AiChatScreen::set_busy(bool busy) {
    streaming_ = busy;
    // The composer stays editable while an answer streams, so the next question can be
    // drafted; Enter only sends once the turn is over. SEND ↔ STOP: Send carries "spend",
    // Stop carries no marker (it only cancels this turn — the safe direction).
    composer_->set_busy(busy);
    transcript_->set_actions_enabled(!busy);
    if (sessions_) {
        sessions_->set_interactive(!busy);
        sessions_->set_running(busy ? pending_req_session_ : active_session_id_, busy);
    }
    if (bar_) {
        bar_->set_action_enabled(QStringLiteral("new"), !busy);
        bar_->set_action_enabled(QStringLiteral("more"), !busy);
        if (auto* f = bar_->command_field())
            f->setEnabled(!busy);
    }
    update_model_notice(); // hidden while a turn runs
    update_status();
}

void AiChatScreen::on_provider_changed() {
    update_stats();
}

void AiChatScreen::update_model_hint() {
    if (!model_hint_)
        return;
    const ai_chat::ModelChoice route =
        ai_chat::ModelRouter::instance().resolve(QString::fromLatin1(ai_chat::kFeatureAiChat));
    int cap = 0;
    if (route.is_fincept())
        cap = ai_chat::ModelCatalog::kFinceptMaxOutputTokens;
    else if (!route.is_harness())
        cap = ai_chat::ModelCatalog::output_cap(route.provider.toLower(), route.model);
    QStringList parts;
    if (cap > 0)
        parts << tr("REPLY ≤ %1").arg(aichat::format_count(cap));
    const QString billing = ai_chat::ModelRouter::billing_note(route);
    if (!billing.isEmpty())
        parts << billing;
    model_hint_->setText(parts.join(QStringLiteral(" · ")));
    model_hint_->setToolTip(tr("Largest reply this model accepts (output tokens) and who pays for it. The context "
                               "window is not reported by the model catalogue."));
    model_hint_->setVisible(!parts.isEmpty());
    composer_->set_reply_cap(cap);
}

void AiChatScreen::update_status() {
    if (!status_)
        return;
    using Tone = ui::ScreenStatusBar::Tone;
    auto& llm = ai_chat::LlmService::instance();
    const bool configured = llm.is_configured();
    if (configured) {
        aichat::Turn probe;
        probe.role = aichat::TurnRole::Assistant;
        const ai_chat::ModelChoice route =
            ai_chat::ModelRouter::instance().resolve(QString::fromLatin1(ai_chat::kFeatureAiChat));
        probe.provider = route.provider_id();
        probe.model = route.model;
        probe.route_label = route.label;
        status_->set_text(QStringLiteral("route"), aichat::turn_speaker(probe).toUpper());
        status_->set_tooltip(QStringLiteral("route"), tr("The model that answers AI Chat (change it with the model "
                                                         "badge in the command bar)"));
    } else {
        status_->set_text(QStringLiteral("route"), tr("NO MODEL"));
    }
    if (!configured)
        status_->set_item(QStringLiteral("state"), tr("● NO MODEL"), Tone::Danger);
    else if (streaming_ && stop_requested_)
        status_->set_item(QStringLiteral("state"), tr("● STOPPING"), Tone::Warn);
    else if (streaming_)
        status_->set_item(QStringLiteral("state"), tr("● STREAMING"), Tone::Accent);
    else if (last_failed_)
        status_->set_item(QStringLiteral("state"), tr("● LAST REQUEST FAILED"), Tone::Danger);
    else
        status_->set_item(QStringLiteral("state"), tr("● READY"), Tone::Ok);
    status_->set_text(QStringLiteral("tools"), !tools_on_ ? tr("TOOLS OFF")
                                               : nav_on_  ? tr("TOOLS ON")
                                                          : tr("TOOLS ON · NO NAV"));

    // Tokens: the stored total of the session; in / out only for answers of this run (the
    // database keeps no split). Nothing is shown that the provider did not report.
    if (total_tokens_ > 0) {
        QString text = tr("TOK %1").arg(aichat::format_count(total_tokens_));
        if (run_prompt_tokens_ >= 0 || run_completion_tokens_ >= 0)
            text += QStringLiteral("  ") + tr("IN %1 / OUT %2")
                                               .arg(aichat::format_count(std::max<qint64>(run_prompt_tokens_, 0)),
                                                    aichat::format_count(std::max<qint64>(run_completion_tokens_, 0)));
        status_->set_text(QStringLiteral("tokens"), text);
        status_->set_tooltip(QStringLiteral("tokens"),
                             tr("%1 tokens used in this conversation (prompt + completion, all tool rounds, as reported "
                                "by the provider). In / out covers answers received since the tab opened.")
                                 .arg(QLocale().toString(total_tokens_)));
        status_->set_item_visible(QStringLiteral("tokens"), true);
    } else {
        status_->set_item_visible(QStringLiteral("tokens"), false);
    }
    if (run_cost_usd_ >= 0.0) {
        status_->set_text(QStringLiteral("cost"),
                          QStringLiteral("$%1").arg(run_cost_usd_, 0, 'f', run_cost_usd_ < 0.01 ? 4 : 2));
        status_->set_tooltip(QStringLiteral("cost"), tr("Cost reported by the harness for answers of this run"));
        status_->set_item_visible(QStringLiteral("cost"), true);
    } else {
        status_->set_item_visible(QStringLiteral("cost"), false);
    }
    if (last_answer_ms_ >= 0) {
        status_->set_text(QStringLiteral("last"), tr("LAST %1").arg(aichat::format_ms(last_answer_ms_)));
        status_->set_item_visible(QStringLiteral("last"), true);
    } else {
        status_->set_item_visible(QStringLiteral("last"), false);
    }
    if (last_answer_at_.isValid()) {
        status_->set_text(QStringLiteral("asof"), tr("AS OF %1").arg(last_answer_at_.toString(QStringLiteral("HH:mm:ss"))));
        status_->set_item_visible(QStringLiteral("asof"), true);
    } else {
        status_->set_item_visible(QStringLiteral("asof"), false);
    }
}

void AiChatScreen::update_stats() {
    if (model_badge_)
        model_badge_->refresh();
    update_model_hint();
    const auto& llm = ai_chat::LlmService::instance();
    composer_->set_configured(llm.is_configured(),
                              llm.is_configured() ? QString()
                                                  : tr("No AI model configured — Settings > LLM Configuration"));
    if (start_page_ && start_page_->isVisible())
        start_page_->refresh_setup();
    if (bar_ && bar_->command_field() && !bar_->command_field()->hasFocus())
        bar_->set_command_text(active_session_title_);

    aichat::SessionStats s;
    s.messages = total_messages_;
    s.tokens = total_tokens_;
    s.prompt_tokens = run_prompt_tokens_;
    s.completion_tokens = run_completion_tokens_;
    s.cost_usd = run_cost_usd_;
    s.last_ms = last_answer_ms_;
    s.as_of = last_answer_at_;
    if (!conv_provider_.isEmpty()) {
        aichat::Turn probe;
        probe.role = aichat::TurnRole::Assistant;
        probe.provider = conv_provider_;
        probe.model = conv_model_;
        s.models << aichat::turn_speaker(probe);
    }
    if (context_rail_)
        context_rail_->set_stats(s);
    update_model_notice();
    update_status();
}

} // namespace fincept::screens
