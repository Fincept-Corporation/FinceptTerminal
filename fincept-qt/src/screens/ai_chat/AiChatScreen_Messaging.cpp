// src/screens/ai_chat/AiChatScreen_Messaging.cpp
//
// Message lifecycle: typing indicator, on_send / start_request, streaming
// chunks, Stop (cancel → grace → detach), finish_turn, Regenerate / Retry, the
// Fincept credits notice, bubble construction, scroll/input helpers,
// update_stats.
//
// Part of the partial-class split of AiChatScreen.cpp. The Thinking / Tools
// cards live in AiChatScreen_Tools.cpp.

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "mcp/McpService.h"
#include "screens/ai_chat/AiChatScreen.h"
#include "screens/ai_chat/ChatBubbleFactory.h"
#include "services/llm/LlmErrors.h"
#include "services/llm/LlmService.h"
#include "storage/repositories/ChatRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EnterprisePromo.h"

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPointer>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

#include <memory>

namespace fincept::screens {

namespace fnt = fincept::ui::fonts;
namespace col = fincept::ui::colors;

// Bubble visuals live in ChatBubbleFactory — shared with AiChatBubble.
// Only the column max widths are local config: the tab gets more horizontal
// room than the floating bubble.
static constexpr int kUserColMaxWidth = 560;
static constexpr int kAiColMaxWidth = 680;

void AiChatScreen::on_typing_indicator_tick() {
    const QStringList states = {tr("AI is thinking"), tr("AI is thinking·"), tr("AI is thinking··"),
                                tr("AI is thinking···")};
    typing_step_ = (typing_step_ + 1) % states.size();
    typing_dots_lbl_->setText(states[typing_step_]);
}

void AiChatScreen::show_typing(bool show) {
    if (show) {
        typing_step_ = 0;
        typing_dots_lbl_->setText(tr("AI is thinking"));
        typing_indicator_->show();
        typing_timer_->start();
    } else {
        typing_timer_->stop();
        typing_indicator_->hide();
    }
}

// ── Sending ──────────────────────────────────────────────────────────────────

void AiChatScreen::on_send() {
    if (streaming_) {
        // While a reply is streaming the Send button is the Stop button.
        on_stop_requested();
        return;
    }
    const QString raw_text = input_box_->toPlainText().trimmed();
    if (raw_text.isEmpty() && attached_file_path_.isEmpty())
        return;
    if (active_session_id_.isEmpty())
        create_new_session();
    if (active_session_id_.isEmpty()) {
        add_message_bubble("system", tr("Failed to create chat session. Please try again."));
        return;
    }
    if (!ai_chat::LlmService::instance().is_configured()) {
        // System bubbles render Markdown and follow https links (ChatBubbleFactory).
        const QString ent_url =
            ui::enterprise::url_with_ref(ui::enterprise::site_url(), QStringLiteral("ai"));
        add_message_bubble("system",
                           tr("No LLM provider configured. Go to Settings > LLM Configuration to set up a provider.") +
                               QStringLiteral("\n\n") +
                               tr("Or use Fincept Terminal Enterprise — AI credits included.") +
                               QStringLiteral(" [%1](%2)").arg(tr("See Enterprise ↗"), ent_url));
        return;
    }

    // Build final text: prepend file contents if attached
    QString text = raw_text;
    if (!attached_file_path_.isEmpty()) {
        const QFileInfo fi(attached_file_path_);
        QString attach_error;
        QString file_content;
        bool truncated = false;
        constexpr qsizetype kAttachMaxChars = 32000;
        QFile f(attached_file_path_);
        if (!f.open(QIODevice::ReadOnly)) {
            attach_error = tr("Could not read the attached file \"%1\": %2").arg(fi.fileName(), f.errorString());
        } else {
            // 4 bytes/char worst case for UTF-8, so this always covers the character cap.
            const QByteArray raw = f.read(kAttachMaxChars * 4);
            truncated = !f.atEnd();
            f.close();
            if (raw.contains('\0')) {
                // The picker offers PDF / notebooks / "All files": a binary one used to be
                // decoded as text and sent to the model as garbage.
                attach_error = tr("\"%1\" looks like a binary file (PDF, image, archive…) and can't be sent as text. "
                                  "Attach a text-based file such as .txt, .md, .csv, .json or .ipynb.")
                                   .arg(fi.fileName());
            } else {
                file_content = QString::fromUtf8(raw);
                if (file_content.startsWith(QChar(0xFEFF)))
                    file_content.remove(0, 1);
                if (file_content.size() > kAttachMaxChars) {
                    file_content.truncate(kAttachMaxChars);
                    truncated = true;
                }
            }
        }
        // Reset attach state either way: the file is consumed or unusable.
        attached_file_path_.clear();
        if (attach_badge_)
            attach_badge_->setVisible(false);
        if (attach_btn_)
            attach_btn_->setProperty("active", false);
        ScreenStateManager::instance().notify_changed(this);
        if (!attach_error.isEmpty()) {
            // Nothing was sent — the typed message stays in the box.
            add_message_bubble("system", attach_error);
            return;
        }
        // A silently clipped file reads as the whole file, so the model is told when it
        // is one. This is prompt text (stored in history), not UI — deliberately not tr().
        const QString header =
            truncated ? QStringLiteral("[Attached file: %1 — truncated to the first %2 characters]")
                            .arg(fi.fileName())
                            .arg(kAttachMaxChars)
                      : QStringLiteral("[Attached file: %1]").arg(fi.fileName());
        text = QString("%1\n\n%2\n\n---\n\n%3").arg(header, file_content, raw_text);
    }

    // Phase 7: prepend the linked-group symbol context so the LLM has the
    // security in scope. Only emitted on the wire — the visible bubble
    // still shows the user's raw_text. The user's bubble below is
    // unchanged, but `text` (the LLM payload) carries the prefix.
    if (linked_symbol_.is_valid() && linked_context_pending_) {
        text = QString("[Context: %1 (%2)]\n\n%3")
                   .arg(linked_symbol_.symbol,
                        linked_symbol_.asset_class.isEmpty() ? QStringLiteral("equity") : linked_symbol_.asset_class,
                        text);
        linked_context_pending_ = false; // one-shot: the history now carries the context
    }

    input_box_->clear();
    input_box_->setFixedHeight(44);
    show_welcome(false);
    retire_answer_action();
    // Display only raw_text in bubble (not full file dump)
    add_message_bubble("user", raw_text.isEmpty() ? tr("[File attached — see context]") : raw_text);
    total_messages_++;
    auto& llm = ai_chat::LlmService::instance();
    ChatRepository::instance().add_message(active_session_id_, "user", text, llm.active_provider(),
                                           llm.active_model());
    // Snapshot BEFORE appending the new turn: every request builder appends
    // `text` as the trailing user message itself, so including it in the history
    // too sent the same turn twice (and, on the Fincept flat-prompt path, printed
    // "User: …" twice in a row).
    std::vector<ai_chat::ConversationMessage> hist_copy = history_;
    history_.push_back({"user", text});
    start_request(text, std::move(hist_copy));
}

// The history the model sees. Two cleanups the stored transcript needs:
//  * a failed turn leaves a user message with no answer, so the next turn would
//    send two consecutive user messages — several providers require strictly
//    alternating roles and reject that with a 400;
//  * consecutive assistant messages cannot occur here (regenerate replaces the
//    superseded answer), but are merged defensively for the same reason.
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

// Common tail of on_send / on_regenerate: the user turn is already in the
// transcript; `snapshot` is the history BEFORE it.
void AiChatScreen::start_request(const QString& wire_text, std::vector<ai_chat::ConversationMessage> snapshot) {
    auto& llm = ai_chat::LlmService::instance();
    const QString req_session = active_session_id_;
    pending_req_session_ = req_session;
    pending_req_provider_ = llm.active_provider();
    pending_req_model_ = llm.active_model();
    const quint64 gen = ++request_gen_;
    stop_requested_ = false;
    stop_grace_timer_->stop();

    // The streaming entry point is cancellable (session lane); the blocking
    // chat() fallback is used only for providers that cannot stream.
    stoppable_ = ai_chat::provider_supports_streaming(pending_req_provider_);
    streaming_ = true;
    set_input_enabled(false);
    update_model_notice(); // hidden while a turn runs

    // Show typing indicator while waiting for first chunk
    show_typing(true);
    scroll_to_bottom();

    // Fresh Thinking / Tools sections for this message (both cards are created
    // lazily by on_stream_chunk when their channel first produces a chunk).
    reset_thinking_state();
    reset_tools_state();
    streaming_bubble_ = nullptr;
    streaming_row_ = nullptr;
    streaming_role_lbl_ = nullptr;

    std::vector<ai_chat::ConversationMessage> wire = wire_history(snapshot);
    // A turn that failed (credits, network, Stop before any answer) leaves its
    // user message unanswered. Sending the next one after it would put two user
    // turns back to back, which providers that require alternating roles reject
    // with a 400. Fold the unanswered message into this one instead — it is
    // usually the context the new message refers to ("try again").
    QString outgoing = wire_text;
    if (!wire.empty() && wire.back().role == QLatin1String("user")) {
        outgoing = wire.back().content + QStringLiteral("\n\n") + wire_text;
        wire.pop_back();
    }
    QPointer<AiChatScreen> self = this;
    if (stoppable_) {
        auto first_chunk = std::make_shared<bool>(true);
        llm.chat_streaming(
            outgoing, wire,
            [self, first_chunk, gen](const QString& chunk, bool done) {
                QMetaObject::invokeMethod(
                    qApp,
                    [self, chunk, done, first_chunk, gen]() {
                        // A stopped-and-detached request keeps streaming until it
                        // notices the cancel; its chunks must not paint anything.
                        if (!self || self->request_gen_ != gen || !self->streaming_)
                            return;
                        // On first chunk: hide the typing indicator. The answer
                        // bubble and Thinking card are created lazily inside
                        // on_stream_chunk so a leading reasoning chunk doesn't
                        // spawn an empty answer bubble above the card.
                        if (*first_chunk && !chunk.isEmpty()) {
                            *first_chunk = false;
                            self->show_typing(false);
                        }
                        self->on_stream_chunk(chunk, done);
                    },
                    Qt::QueuedConnection);
            },
            ai_chat::LlmService::ToolPolicy::All, req_session);
    } else {
        (void)QtConcurrent::run([self, outgoing, wire, req_session, gen]() {
            auto resp = ai_chat::LlmService::instance().chat(outgoing, wire);
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

    // Chain-of-thought chunk → separate, collapsible Thinking section (kept out
    // of the answer bubble). Routed in-band via the think_stream_prefix sentinel.
    const QString think_prefix = ai_chat::think_stream_prefix();
    if (chunk.startsWith(think_prefix)) {
        append_thinking_chunk(chunk.mid(think_prefix.size()));
        return;
    }

    // Tool activity → its own collapsible Tools card, never the answer bubble.
    const QString tool_prefix = ai_chat::tool_stream_prefix();
    if (chunk.startsWith(tool_prefix)) {
        append_tool_chunk(chunk.mid(tool_prefix.size()));
        scroll_to_bottom();
        return;
    }

    // Tool-call clear sentinel: the model switched to tool calls mid-stream, so
    // what streamed so far is aborted markup. Drop that bubble entirely — the
    // answer is re-created after the Tools card, in reading order — and show
    // the typing indicator until the first tool line arrives.
    if (chunk.startsWith("\x01__TOOL_CALL_CLEAR__")) {
        if (streaming_row_ && streaming_bubble_)
            streaming_row_->deleteLater();
        streaming_bubble_ = nullptr;
        streaming_row_ = nullptr;
        streaming_role_lbl_ = nullptr;
        if (!tools_card_)
            show_typing(true);
        return;
    }

    if (chunk.isEmpty())
        return;

    // Lazily create the answer bubble on the first real answer chunk so it lands
    // below any Thinking card that arrived first.
    if (!streaming_bubble_)
        streaming_bubble_ = add_streaming_bubble();
    // Buffered: the factory paints at most ~15 times a second.
    fincept::ai_chat::ChatBubbleFactory::append_streaming_chunk(streaming_bubble_, chunk);
    scroll_to_bottom();
}

void AiChatScreen::on_streaming_done(ai_chat::LlmResponse response) {
    // A request the user stopped and we stopped waiting for: its answer is the
    // first one to arrive for that session afterwards (the session lane runs one
    // request at a time, in order), so it is swallowed here — even if a newer
    // request for the same session is already waiting behind it.
    if (!response.origin_session_id.isEmpty()) {
        auto it = abandoned_responses_.find(response.origin_session_id);
        if (it != abandoned_responses_.end()) {
            if (--it.value() <= 0)
                abandoned_responses_.erase(it);
            LOG_INFO("AiChat", QString("Discarded the late reply of a stopped request (%1)")
                                   .arg(response.cancelled ? "cancelled" : response.success ? "completed" : "failed"));
            return;
        }
    }
    // finished_streaming() is a process-wide signal and the floating Quick Chat bubble
    // listens to the same one. Without this check every Quick Chat answer landed here
    // too — rendered into this tab's transcript, appended to its history and written
    // to the database under an empty session id — and, symmetrically, this tab's own
    // answers leaked into the bubble. Only the response to OUR request (same session
    // id, while we are waiting for one) is ours.
    if (!streaming_ || response.origin_session_id != pending_req_session_)
        return;
    finish_turn(response);
}

QString AiChatScreen::answer_footer_meta(const QString& provider, const QString& model, int tokens) const {
    QStringList parts;
    if (provider.compare(QLatin1String("fincept"), Qt::CaseInsensitive) == 0)
        parts << tr("Fincept LLM");
    else if (!provider.isEmpty())
        parts << (model.isEmpty() ? provider : provider + QStringLiteral(" / ") + model);
    if (tokens > 0)
        parts << tr("%1 tok").arg(QLocale().toString(tokens));
    return parts.join(QStringLiteral("  ·  "));
}

void AiChatScreen::finish_turn(const ai_chat::LlmResponse& response) {
    streaming_ = false;
    stoppable_ = false;
    stop_requested_ = false;
    stop_grace_timer_->stop();
    show_typing(false);

    // Lock in the Thinking and Tools cards (swap their live headers for final
    // labels). The tool steps are captured first: they are persisted with the
    // answer so a reloaded conversation still shows which tools produced it.
    finalize_thinking_card();
    const QString trailer = tool_trailer();
    finalize_tools_card();

    set_input_enabled(true);
    const QString session = pending_req_session_;
    const QString provider = pending_req_provider_;
    const QString model = pending_req_model_;
    pending_req_session_.clear();
    pending_req_provider_.clear();
    pending_req_model_.clear();

    namespace cbf = fincept::ai_chat;
    if (!response.success) {
        // Text that streamed before the failure / Stop stays on screen. Replacing the
        // bubble with the error used to discard it — up to minutes of generated answer.
        const QString partial =
            streaming_bubble_ ? cbf::ChatBubbleFactory::streaming_text(streaming_bubble_).trimmed() : QString();

        if (response.cancelled) {
            LOG_INFO("AiChat", QString("LLM request stopped by user (%1 chars kept)").arg(partial.size()));
            if (partial.isEmpty()) {
                if (!streaming_bubble_)
                    streaming_bubble_ = add_streaming_bubble();
                cbf::ChatBubbleFactory::replace_streaming_text(streaming_bubble_, tr("Stopped."));
                cbf::ChatBubbleFactory::finalize_streaming(streaming_bubble_, tr("*Stopped.*"));
                add_answer_action(streaming_bubble_, /*failed=*/true);
            } else {
                // Keep the partial answer in the transcript (and tell the model it was cut off
                // on the next turn) so reloading the session shows what the user saw.
                const QString saved = partial + QStringLiteral("\n\n*(stopped)*");
                cbf::ChatBubbleFactory::finalize_streaming(
                    streaming_bubble_, partial + QStringLiteral("\n\n*") + tr("(stopped)") + QStringLiteral("*"),
                    answer_footer_meta(provider, model, response.total_tokens));
                history_.push_back({"assistant", saved});
                total_messages_++;
                total_tokens_ += response.total_tokens;
                update_stats();
                ChatRepository::instance().add_message(session, "assistant", saved + trailer, provider, model,
                                                       response.total_tokens);
                add_answer_action(streaming_bubble_, /*failed=*/false);
            }
        } else {
            LOG_WARN("AiChat", QString("LLM request failed: %1").arg(response.error));
            const auto kind = ai_chat::classify_llm_error(response.error);
            if (kind != ai_chat::LlmErrorKind::Other) {
                // Out of credits / sign-in rejected: an actionable notice, not a red error line.
                if (streaming_bubble_ && !partial.isEmpty())
                    cbf::ChatBubbleFactory::finalize_streaming(streaming_bubble_, partial);
                else if (streaming_row_)
                    streaming_row_->deleteLater();
                streaming_bubble_ = nullptr;
                show_fincept_notice(response);
            } else {
                const QString err =
                    response.error.isEmpty() ? tr("Error: request failed") : tr("Error: %1").arg(response.error);
                if (!streaming_bubble_)
                    streaming_bubble_ = add_streaming_bubble();
                if (partial.isEmpty()) {
                    cbf::ChatBubbleFactory::replace_streaming_text(streaming_bubble_, err);
                    cbf::ChatBubbleFactory::finalize_streaming(streaming_bubble_, QStringLiteral("**") + err +
                                                                                        QStringLiteral("**"));
                } else {
                    cbf::ChatBubbleFactory::finalize_streaming(
                        streaming_bubble_, partial + QStringLiteral("\n\n**") + err + QStringLiteral("**"));
                }
                add_answer_action(streaming_bubble_, /*failed=*/true);
            }
        }
        streaming_bubble_ = nullptr;
        scroll_to_bottom();
        return;
    }

    // Success. The service's content is the cleaned answer (think blocks and
    // tool markup removed) — shown AND persisted, so screen and database agree.
    // The streamed text covers a provider that streams but returns no body.
    const QString acc = streaming_bubble_ ? cbf::ChatBubbleFactory::streaming_text(streaming_bubble_) : QString();
    const QString content = !response.content.trimmed().isEmpty() ? response.content : acc;
    if (content.trimmed().isEmpty()) {
        if (!streaming_bubble_)
            streaming_bubble_ = add_streaming_bubble();
        cbf::ChatBubbleFactory::replace_streaming_text(streaming_bubble_,
                                                       tr("(empty response — model returned no content)"));
        cbf::ChatBubbleFactory::finalize_streaming(streaming_bubble_);
        add_answer_action(streaming_bubble_, /*failed=*/true);
        LOG_WARN("AiChat", "LLM returned success with empty content");
        streaming_bubble_ = nullptr;
        scroll_to_bottom();
        return;
    }

    if (!streaming_bubble_)
        streaming_bubble_ = add_streaming_bubble();
    // finalize_streaming swaps the label to MarkdownText, fills the footer
    // (time · provider / model · tokens) and reveals Copy / Copy code.
    cbf::ChatBubbleFactory::finalize_streaming(streaming_bubble_, content,
                                               answer_footer_meta(provider, model, response.total_tokens));
    history_.push_back({"assistant", content});
    total_messages_++;
    total_tokens_ += response.total_tokens;
    conv_provider_ = provider;
    conv_model_ = model;
    update_stats();
    // Request-time context: the provider/model that actually produced this answer.
    ChatRepository::instance().add_message(session, "assistant", content + trailer, provider, model,
                                           response.total_tokens);
    add_answer_action(streaming_bubble_, /*failed=*/false);
    streaming_bubble_ = nullptr;
    trim_rendered_rows();
    scroll_to_bottom();
    if (input_box_->isEnabled())
        input_box_->setFocus();
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
    send_btn_->setText(tr("Stopping…"));
    send_btn_->setEnabled(false);
    hdr_status_lbl_->setText(tr("Stopping"));
    // Most turns unwind within ~100 ms. One blocked inside a tool cannot be
    // interrupted — stop waiting for it after the grace period.
    stop_grace_timer_->start();
}

void AiChatScreen::detach_stopped_request() {
    if (!streaming_ || !stop_requested_)
        return;
    LOG_WARN("AiChat", "Stopped request did not finish within the grace period (blocked in a tool) — detaching");
    // Its chunks are dropped by generation; its finished_streaming() is the next
    // one for this session and is swallowed (non-streaming requests are dropped
    // by generation alone).
    if (stoppable_ && !pending_req_session_.isEmpty())
        abandoned_responses_[pending_req_session_] += 1;
    ++request_gen_;
    ai_chat::LlmResponse stopped;
    stopped.cancelled = true;
    stopped.error = QStringLiteral("Request cancelled");
    stopped.origin_session_id = pending_req_session_;
    finish_turn(stopped);
}

// ── Regenerate / Retry ───────────────────────────────────────────────────────

void AiChatScreen::on_regenerate() {
    if (streaming_ || history_.empty() || active_session_id_.isEmpty())
        return;
    if (!ai_chat::LlmService::instance().is_configured())
        return;
    // Regenerate: the newest answer is superseded — dropped from what the model
    // sees, kept (dimmed) on screen and in the database. On reload, the last of
    // consecutive answers wins (see load_messages).
    if (history_.back().role == QLatin1String("assistant")) {
        history_.pop_back();
        if (last_answer_row_) {
            // Dimmed, not disabled: its Copy buttons must keep working.
            auto* fx = new QGraphicsOpacityEffect(last_answer_row_);
            fx->setOpacity(0.5);
            last_answer_row_->setGraphicsEffect(fx);
        }
        if (last_answer_role_lbl_)
            last_answer_role_lbl_->setText(tr("AI · superseded — regenerated below"));
    }
    // Retry (after a failed / stopped turn) and regenerate both resend the
    // newest user message — already in the transcript and the database.
    if (history_.empty() || history_.back().role != QLatin1String("user"))
        return;
    retire_answer_action();
    const QString text = history_.back().content;
    std::vector<ai_chat::ConversationMessage> snapshot(history_.begin(), history_.end() - 1);
    show_welcome(false);
    start_request(text, std::move(snapshot));
}

void AiChatScreen::add_answer_action(QLabel* body, bool failed) {
    retire_answer_action();
    QWidget* footer = fincept::ai_chat::ChatBubbleFactory::footer_of(body);
    auto* fhl = footer ? qobject_cast<QHBoxLayout*>(footer->layout()) : nullptr;
    if (!fhl)
        return;
    auto* btn = fincept::ai_chat::ChatBubbleFactory::make_footer_button(
        failed ? tr("↻ Retry") : tr("↻ Regenerate"),
        failed ? tr("Send the last message again (Ctrl+R)") : tr("Generate a new answer to the last message (Ctrl+R)"));
    connect(btn, &QPushButton::clicked, this, &AiChatScreen::on_regenerate);
    fhl->addWidget(btn);
    answer_action_btn_ = btn;
    if (!failed) {
        // Row + role label of the newest real answer: regenerate dims them.
        last_answer_row_ = streaming_row_;
        last_answer_role_lbl_ = streaming_role_lbl_;
    } else {
        last_answer_row_ = nullptr;
        last_answer_role_lbl_ = nullptr;
    }
}

void AiChatScreen::retire_answer_action() {
    if (answer_action_btn_)
        answer_action_btn_->deleteLater();
    answer_action_btn_ = nullptr;
}

// ── Fincept credits / sign-in notice ─────────────────────────────────────────

void AiChatScreen::show_fincept_notice(const ai_chat::LlmResponse& response) {
    const bool credits = ai_chat::classify_llm_error(response.error) == ai_chat::LlmErrorKind::FinceptOutOfCredits;
    QString text = credits ? tr("**You're out of Fincept credits.**") : tr("**Fincept did not accept your sign-in.**");
    // The service's sentence carries the cost/balance figures when known.
    static const QRegularExpression rx_figures(QStringLiteral("This request needs ([^;]+); your balance is ([^.]+)\\."));
    const auto m = rx_figures.match(response.error);
    if (credits && m.hasMatch())
        text += QStringLiteral("\n\n") + tr("This request needs %1; your balance is %2.").arg(m.captured(1), m.captured(2));
    text += QStringLiteral("\n\n") +
            (credits ? tr("Top up your credits, or add your own provider API key in **Settings → LLM Configuration** "
                          "to keep chatting.")
                     : tr("Sign in again (or check your Fincept API key), or add your own provider API key in "
                          "**Settings → LLM Configuration**."));

    fincept::ai_chat::ChatBubbleFactory::Options opts;
    opts.role = "system";
    opts.content = text;
    opts.show_footer = true;
    opts.user_col_max_width = kUserColMaxWidth;
    opts.ai_col_max_width = kAiColMaxWidth;
    auto b = fincept::ai_chat::ChatBubbleFactory::build(opts);
    if (b.footer_layout) {
        if (credits) {
            auto* plans = fincept::ai_chat::ChatBubbleFactory::make_footer_button(tr("View plans"),
                                                                                  tr("Open Plans & Pricing"));
            connect(plans, &QPushButton::clicked, this, [this]() { ui::billing::open_plans(this); });
            b.footer_layout->addWidget(plans);
        }
        auto* settings = fincept::ai_chat::ChatBubbleFactory::make_footer_button(
            tr("LLM settings"), tr("Open Settings to add your own provider API key"));
        connect(settings, &QPushButton::clicked, this, []() {
            EventBus::instance().publish(QStringLiteral("nav.switch_screen"),
                                         QVariantMap{{"screen_id", QStringLiteral("settings")}});
        });
        b.footer_layout->addWidget(settings);
        auto* retry = fincept::ai_chat::ChatBubbleFactory::make_footer_button(
            tr("↻ Retry"), tr("Send the last message again (Ctrl+R)"));
        connect(retry, &QPushButton::clicked, this, &AiChatScreen::on_regenerate);
        b.footer_layout->addWidget(retry);
        retire_answer_action();
        answer_action_btn_ = retry;
        b.footer_layout->addStretch();
    }
    insert_row(b.row);
}

// ── Message bubbles ───────────────────────────────────────────────────────────

void AiChatScreen::insert_row(QWidget* row, Qt::Alignment align) {
    // Before the trailing stretch.
    messages_layout_->insertWidget(messages_layout_->count() - 1, row, 0, align);
    rendered_rows_.append(row);
}

// Bounded memory for long live sessions: past kMaxLiveRows rendered rows the
// oldest are destroyed (they remain in history_ and the database) and the
// "show earlier" bar offers to rebuild them.
void AiChatScreen::trim_rendered_rows() {
    rendered_rows_.removeIf([](const QPointer<QWidget>& p) { return p.isNull(); });
    if (rendered_rows_.size() <= kMaxLiveRows)
        return;
    const int drop = rendered_rows_.size() - kMaxLiveRows * 2 / 3;
    for (int i = 0; i < drop; ++i) {
        QWidget* w = rendered_rows_.takeFirst();
        if (!w)
            continue;
        messages_layout_->removeWidget(w);
        w->deleteLater();
    }
    // Rows ≠ messages (tool / thinking cards), so offer a full reload rather
    // than a count we cannot state exactly.
    hidden_message_count_ = std::max(hidden_message_count_, 1);
    load_earlier_btn_->setText(tr("Show earlier messages"));
    load_earlier_btn_->show();
}

QLabel* AiChatScreen::add_message_bubble(const QString& role, const QString& content, const QString& timestamp,
                                         const QString& footer_meta) {
    fincept::ai_chat::ChatBubbleFactory::Options opts;
    opts.role = role;
    opts.content = content;
    opts.timestamp_iso = timestamp;
    opts.footer_meta = footer_meta;
    opts.show_footer = true;
    opts.user_col_max_width = kUserColMaxWidth;
    opts.ai_col_max_width = kAiColMaxWidth;
    auto b = fincept::ai_chat::ChatBubbleFactory::build(opts);
    insert_row(b.row);
    if (role == QLatin1String("assistant")) {
        last_answer_row_ = b.row;
        last_answer_role_lbl_ = b.role_lbl;
        streaming_row_ = b.row; // add_answer_action() reads these for the newest answer
        streaming_role_lbl_ = b.role_lbl;
    }
    return b.body;
}

QLabel* AiChatScreen::add_streaming_bubble() {
    fincept::ai_chat::ChatBubbleFactory::Options opts;
    opts.role = "assistant";
    opts.show_footer = true;
    opts.user_col_max_width = kUserColMaxWidth;
    opts.ai_col_max_width = kAiColMaxWidth;
    auto b = fincept::ai_chat::ChatBubbleFactory::build_streaming(opts);
    insert_row(b.row);
    streaming_row_ = b.row;
    streaming_role_lbl_ = b.role_lbl;
    return b.body;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void AiChatScreen::clear_messages() {
    streaming_bubble_.clear();
    streaming_row_.clear();
    streaming_role_lbl_.clear();
    last_answer_row_.clear();
    last_answer_role_lbl_.clear();
    answer_action_btn_.clear();
    reset_thinking_state(); // card widgets are deleted by the loop below
    reset_tools_state();
    rendered_rows_.clear();
    // Skip the welcome panel and the "earlier messages" bar — permanent fixtures
    // reused across sessions (only shown/hidden). Everything else is a message row.
    for (int i = messages_layout_->count() - 1; i >= 0; --i) {
        QLayoutItem* item = messages_layout_->itemAt(i);
        QWidget* w = item ? item->widget() : nullptr;
        if (!w || w == welcome_panel_ || w == load_earlier_btn_)
            continue; // trailing stretch has no widget
        QLayoutItem* taken = messages_layout_->takeAt(i);
        w->deleteLater();
        delete taken;
    }
    hidden_message_count_ = 0;
    if (load_earlier_btn_)
        load_earlier_btn_->hide();
}

void AiChatScreen::scroll_to_bottom() {
    // Re-arm following. The rangeChanged handler installed in build_chat_area()
    // does the real work — it fires once the layout has settled and therefore
    // knows the true maximum. This deferred nudge covers the case where content
    // changed without altering the scroll range.
    stick_to_bottom_ = true;

    // Debounce: a single scroll lands at the end of the current event-loop batch.
    // Without this, a 100-chunk stream queued 100 independent 50ms timers, each
    // racing to setValue() and starving paint events.
    if (scroll_pending_)
        return;
    scroll_pending_ = true;
    QTimer::singleShot(0, this, [this]() {
        scroll_pending_ = false;
        if (!scroll_area_ || !scroll_area_->verticalScrollBar())
            return;
        auto* sb = scroll_area_->verticalScrollBar();
        programmatic_scroll_ = true;
        sb->setValue(sb->maximum());
        programmatic_scroll_ = false;
    });
}

void AiChatScreen::set_input_enabled(bool enabled) {
    // The composer stays editable while an answer streams, so the next question
    // can be drafted; Enter only sends once the turn is over (eventFilter).
    input_box_->setEnabled(true);
    // While a reply is running the send button is "Stop" (on_send() routes the
    // click to on_stop_requested()). Every request can be stopped: streamed ones
    // are cancelled, the blocking fallback is detached.
    send_btn_->setEnabled(true);
    new_btn_->setEnabled(enabled);
    search_edit_->setEnabled(enabled);
    session_list_->setEnabled(enabled);
    delete_btn_->setEnabled(enabled && !active_session_id_.isEmpty());
    rename_btn_->setEnabled(enabled && !active_session_id_.isEmpty());
    if (export_btn_)
        export_btn_->setEnabled(enabled && !active_session_id_.isEmpty());
    send_btn_->setText(enabled ? tr("Send") : tr("Stop"));
    send_btn_->setToolTip(enabled ? tr("Send (Enter)") : tr("Stop the answer (Esc)"));

    // Status dot + label
    const QString status_color = enabled ? col::POSITIVE() : col::AMBER();
    hdr_status_dot_->setStyleSheet(QString("background:%1;border-radius:0px;").arg(status_color));
    hdr_status_lbl_->setText(enabled ? tr("Ready") : tr("Streaming"));
    hdr_status_lbl_->setStyleSheet(
        QString("color:%1;font-size:%2px;font-weight:600;").arg(status_color).arg(fnt::SMALL));
}

void AiChatScreen::on_provider_changed() {
    update_stats();
}

void AiChatScreen::update_stats() {
    // Header session name
    if (!active_session_title_.isEmpty())
        hdr_session_lbl_->setText(active_session_title_);
    else if (!active_session_id_.isEmpty())
        hdr_session_lbl_->setText(active_session_id_.left(8));
    else
        hdr_session_lbl_->setText(tr("New Conversation"));

    // Token count in header — every round of every turn in this conversation,
    // as reported by the provider (tool loops included).
    if (total_tokens_ > 0) {
        hdr_tokens_lbl_->setText(total_tokens_ < 1000 ? tr("%1 tokens").arg(total_tokens_)
                                                      : tr("%1k tokens").arg(total_tokens_ / 1000.0, 0, 'f', 1));
        hdr_tokens_lbl_->setToolTip(tr("%1 tokens used in this conversation (prompt + completion, all tool rounds, "
                                       "as reported by the provider)")
                                        .arg(QLocale().toString(total_tokens_)));
    } else {
        hdr_tokens_lbl_->clear();
        hdr_tokens_lbl_->setToolTip(QString());
    }

    auto& llm = ai_chat::LlmService::instance();
    if (llm.is_configured()) {
        const QString provider_raw = llm.active_provider();
        const bool is_fincept = (provider_raw.toLower() == "fincept");

        // Display names
        const QString prov_display = is_fincept ? tr("Fincept LLM") : provider_raw.toUpper();
        const QString model_raw = llm.active_model();
        // For fincept, don't expose internal model name
        const QString model_display = is_fincept ? tr("Fincept LLM") : model_raw;
        QString model_short = model_display;
        if (model_short.length() > 24)
            model_short = model_short.left(22) + "..";

        // Sidebar footer
        provider_lbl_->setText(prov_display);
        provider_lbl_->setStyleSheet(
            QString("color:%1;font-size:%2px;font-weight:600;").arg(col::AMBER()).arg(fnt::SMALL));
        model_lbl_->setText(is_fincept ? tr("Managed by Fincept") : model_short);
        model_lbl_->setToolTip(is_fincept ? tr("Fincept LLM — managed AI service") : model_raw);
        model_lbl_->setStyleSheet(QString("color:%1;font-size:%2px;").arg(col::TEXT_SECONDARY()).arg(fnt::TINY));

        // Header model pill — show "Provider / Model" for clarity
        if (is_fincept) {
            hdr_model_lbl_->setText(tr("Fincept LLM"));
            hdr_model_lbl_->setToolTip(tr("Fincept managed AI service\n\nChange in Settings > LLM Configuration"));
        } else {
            hdr_model_lbl_->setText(provider_raw.left(1).toUpper() + provider_raw.mid(1) + " / " + model_short);
            hdr_model_lbl_->setToolTip(
                tr("Provider: %1\nModel: %2\n\nChange in Settings > LLM Configuration").arg(prov_display, model_raw));
        }
    } else {
        provider_lbl_->setText(tr("No provider"));
        provider_lbl_->setStyleSheet(
            QString("color:%1;font-size:%2px;font-weight:600;").arg(col::NEGATIVE()).arg(fnt::SMALL));
        model_lbl_->setText(tr("Configure in Settings"));
        model_lbl_->setStyleSheet(QString("color:%1;font-size:%2px;").arg(col::TEXT_DIM()).arg(fnt::TINY));
        hdr_model_lbl_->setText(tr("No model"));
    }
    update_model_notice();
}

} // namespace fincept::screens
