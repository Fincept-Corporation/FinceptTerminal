// src/screens/ai_chat/AiChatScreen_Messaging.cpp
//
// Message lifecycle: typing indicator, on_send, streaming chunk/done,
// provider change, bubble construction, scroll/input helpers, update_stats.
//
// Part of the partial-class split of AiChatScreen.cpp.

#include "screens/ai_chat/AiChatScreen.h"

#include "screens/ai_chat/ChatBubbleFactory.h"
#include "services/llm/LlmService.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "mcp/McpService.h"
#include "storage/repositories/ChatRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPalette>
#include <QPointer>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QtConcurrent/QtConcurrent>

#include <cmath>
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
    static const QStringList states = {"AI is thinking", "AI is thinking·", "AI is thinking··", "AI is thinking···"};
    typing_step_ = (typing_step_ + 1) % states.size();
    typing_dots_lbl_->setText(states[typing_step_]);
}

void AiChatScreen::show_typing(bool show) {
    if (show) {
        typing_step_ = 0;
        typing_dots_lbl_->setText("AI is thinking");
        typing_indicator_->show();
        typing_timer_->start();
    } else {
        typing_timer_->stop();
        typing_indicator_->hide();
    }
}

// ── Session management ────────────────────────────────────────────────────────


void AiChatScreen::on_send() {
    if (streaming_)
        return;
    const QString raw_text = input_box_->toPlainText().trimmed();
    if (raw_text.isEmpty() && attached_file_path_.isEmpty())
        return;
    if (active_session_id_.isEmpty())
        create_new_session();
    if (active_session_id_.isEmpty()) {
        add_message_bubble("system", "Failed to create chat session. Please try again.");
        return;
    }
    if (!ai_chat::LlmService::instance().is_configured()) {
        add_message_bubble("system",
                           "No LLM provider configured. Go to Settings > LLM Configuration to set up a provider.");
        return;
    }

    // Build final text: prepend file contents if attached
    QString text = raw_text;
    if (!attached_file_path_.isEmpty()) {
        QFile f(attached_file_path_);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&f);
            in.setEncoding(QStringConverter::Utf8);
            QString file_content = in.read(32000); // cap at 32K chars
            f.close();
            QFileInfo fi(attached_file_path_);
            text = QString("[Attached file: %1]\n\n%2\n\n---\n\n%3").arg(fi.fileName(), file_content, raw_text);
        }
        // Reset attach state
        attached_file_path_.clear();
        if (attach_badge_)
            attach_badge_->setVisible(false);
        if (attach_btn_)
            attach_btn_->setProperty("active", false);
    }

    // Phase 7: prepend the linked-group symbol context so the LLM has the
    // security in scope. Only emitted on the wire — the visible bubble
    // still shows the user's raw_text. The user's bubble below is
    // unchanged, but `text` (the LLM payload) carries the prefix.
    if (linked_symbol_.is_valid()) {
        text = QString("[Context: %1 (%2)]\n\n%3")
                   .arg(linked_symbol_.symbol,
                        linked_symbol_.asset_class.isEmpty()
                            ? QStringLiteral("equity")
                            : linked_symbol_.asset_class,
                        text);
    }

    input_box_->clear();
    input_box_->setFixedHeight(44);
    set_input_enabled(false);
    streaming_ = true;
    show_welcome(false);
    // Display only raw_text in bubble (not full file dump)
    add_message_bubble("user", raw_text.isEmpty() ? "[File attached — see context]" : raw_text);
    total_messages_++;
    ChatRepository::instance().add_message(active_session_id_, "user", text,
                                           ai_chat::LlmService::instance().active_provider(),
                                           ai_chat::LlmService::instance().active_model());
    history_.push_back({"user", text});

    // Show typing indicator while waiting for first chunk
    show_typing(true);
    scroll_to_bottom();

    std::vector<ai_chat::ConversationMessage> hist_copy = history_;
    const QString provider = ai_chat::LlmService::instance().active_provider();
    if (ai_chat::provider_supports_streaming(provider)) {
        QPointer<AiChatScreen> self = this;
        auto first_chunk = std::make_shared<bool>(true);
        ai_chat::LlmService::instance().chat_streaming(
            text, hist_copy, [self, first_chunk](const QString& chunk, bool done) {
                QMetaObject::invokeMethod(
                    qApp,
                    [self, chunk, done, first_chunk]() {
                        if (!self)
                            return;
                        // On first chunk: hide typing indicator, show streaming bubble
                        if (*first_chunk && !chunk.isEmpty()) {
                            *first_chunk = false;
                            self->show_typing(false);
                            self->streaming_bubble_ = self->add_streaming_bubble();
                            self->scroll_to_bottom();
                        }
                        self->on_stream_chunk(chunk, done);
                    },
                    Qt::QueuedConnection);
            });
    } else {
        QPointer<AiChatScreen> self = this;
        (void)QtConcurrent::run([self, text, hist_copy]() {
            auto resp = ai_chat::LlmService::instance().chat(text, hist_copy);
            QMetaObject::invokeMethod(
                qApp,
                [self, resp]() {
                    if (self)
                        self->on_streaming_done(resp);
                },
                Qt::QueuedConnection);
        });
    }
}

void AiChatScreen::on_stream_chunk(const QString& chunk, bool done) {
    // Snapshot QPointer to local — prevents TOCTOU between null-check and use.
    QLabel* bubble = streaming_bubble_;
    if (!bubble)
        return;

    // Tool-call clear sentinel: reset bubble content (removes partial XML)
    if (chunk.startsWith("\x01__TOOL_CALL_CLEAR__")) {
        fincept::ai_chat::ChatBubbleFactory::replace_streaming_text(bubble, "Calling tool...");
        scroll_to_bottom();
        return;
    }

    if (!chunk.isEmpty()) {
        fincept::ai_chat::ChatBubbleFactory::append_streaming_chunk(bubble, chunk);
        scroll_to_bottom();
    }
    Q_UNUSED(done)
}

void AiChatScreen::on_streaming_done(ai_chat::LlmResponse response) {
    streaming_ = false;
    show_typing(false);

    // Ensure a bubble exists for any terminal state (success + content,
    // failure with an error message, or success-but-empty). Without this,
    // a kimi/openai response that arrives via the non-streaming fallback
    // with no per-chunk emission, or fails before any chunk lands, would
    // be silently dropped — the typing indicator hides and the input is
    // re-enabled but nothing is shown.
    const bool need_bubble = !streaming_bubble_ &&
                             ((response.success && !response.content.isEmpty()) ||
                              !response.success ||
                              response.content.isEmpty());
    if (need_bubble)
        streaming_bubble_ = add_streaming_bubble();

    set_input_enabled(true);

    if (!response.success) {
        if (streaming_bubble_) {
            const QString err = response.error.isEmpty() ? QStringLiteral("Error: request failed")
                                                         : (QStringLiteral("Error: ") + response.error);
            fincept::ai_chat::ChatBubbleFactory::replace_streaming_text(streaming_bubble_, err);
        }
        LOG_WARN("AiChat", QString("LLM request failed: %1").arg(response.error));
        streaming_bubble_ = nullptr;
        return;
    }

    // Success but empty body — surface a hint instead of silently doing nothing.
    if (response.content.isEmpty()) {
        if (streaming_bubble_) {
            const QString hint = QStringLiteral("(empty response — model returned no content)");
            fincept::ai_chat::ChatBubbleFactory::replace_streaming_text(streaming_bubble_, hint);
        }
        LOG_WARN("AiChat", "LLM returned success with empty content");
        streaming_bubble_ = nullptr;
        return;
    }

    const QString content = response.content;
    if (streaming_bubble_) {
        // Use accumulated streamed text if present, else fall back to the
        // response body (non-streaming path). finalize_streaming swaps the
        // label to MarkdownText and reveals the copy button.
        const QString acc = streaming_bubble_->property("acc").toString();
        const QString final_text = acc.isEmpty() ? content : acc;
        fincept::ai_chat::ChatBubbleFactory::finalize_streaming(streaming_bubble_, final_text);
        streaming_bubble_ = nullptr;
    }
    if (!content.isEmpty()) {
        history_.push_back({"assistant", content});
        total_messages_++;
        total_tokens_ += response.total_tokens;
        update_stats();
        ChatRepository::instance().add_message(active_session_id_, "assistant", content,
                                               ai_chat::LlmService::instance().active_provider(),
                                               ai_chat::LlmService::instance().active_model(), response.total_tokens);
    }
    scroll_to_bottom();
    input_box_->setFocus();
}

void AiChatScreen::on_provider_changed() {
    update_stats();
}

// ── Message bubbles ───────────────────────────────────────────────────────────

void AiChatScreen::add_message_bubble(const QString& role, const QString& content, const QString& timestamp) {
    fincept::ai_chat::ChatBubbleFactory::Options opts;
    opts.role               = role;
    opts.content            = content;
    opts.timestamp_iso      = timestamp;
    opts.show_footer        = true;
    opts.user_col_max_width = kUserColMaxWidth;
    opts.ai_col_max_width   = kAiColMaxWidth;
    auto b = fincept::ai_chat::ChatBubbleFactory::build(opts);
    messages_layout_->insertWidget(messages_layout_->count() - 1, b.row);
}

QLabel* AiChatScreen::add_streaming_bubble() {
    fincept::ai_chat::ChatBubbleFactory::Options opts;
    opts.role               = "assistant";
    opts.show_footer        = true;
    opts.user_col_max_width = kUserColMaxWidth;
    opts.ai_col_max_width   = kAiColMaxWidth;
    auto b = fincept::ai_chat::ChatBubbleFactory::build_streaming(opts);
    messages_layout_->insertWidget(messages_layout_->count() - 1, b.row);
    return b.body;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void AiChatScreen::clear_messages() {
    streaming_bubble_.clear();
    while (messages_layout_->count() > 1) {
        QLayoutItem* item = messages_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void AiChatScreen::scroll_to_bottom() {
    QTimer::singleShot(50, this, [this]() {
        if (scroll_area_ && scroll_area_->verticalScrollBar())
            scroll_area_->verticalScrollBar()->setValue(scroll_area_->verticalScrollBar()->maximum());
    });
}

void AiChatScreen::set_input_enabled(bool enabled) {
    input_box_->setEnabled(enabled);
    send_btn_->setEnabled(enabled);
    new_btn_->setEnabled(enabled);
    search_edit_->setEnabled(enabled);
    session_list_->setEnabled(enabled);
    delete_btn_->setEnabled(enabled && !active_session_id_.isEmpty());
    rename_btn_->setEnabled(enabled && !active_session_id_.isEmpty());
    send_btn_->setText(enabled ? "Send" : "···");

    // Status dot + label
    const QString status_color = enabled ? col::POSITIVE() : col::AMBER();
    hdr_status_dot_->setStyleSheet(QString("background:%1;border-radius:0px;").arg(status_color));
    hdr_status_lbl_->setText(enabled ? "Ready" : "Streaming");
    hdr_status_lbl_->setStyleSheet(
        QString("color:%1;font-size:%2px;font-weight:600;").arg(status_color).arg(fnt::SMALL));
}

void AiChatScreen::update_stats() {
    // Header session name
    if (!active_session_title_.isEmpty())
        hdr_session_lbl_->setText(active_session_title_);
    else if (!active_session_id_.isEmpty())
        hdr_session_lbl_->setText(active_session_id_.left(8));
    else
        hdr_session_lbl_->setText("New Conversation");

    // Token count in header
    if (total_tokens_ > 0) {
        hdr_tokens_lbl_->setText(total_tokens_ < 1000 ? QString("%1 tokens").arg(total_tokens_)
                                                      : QString("%1k tokens").arg(total_tokens_ / 1000.0, 0, 'f', 1));
    } else {
        hdr_tokens_lbl_->clear();
    }

    auto& llm = ai_chat::LlmService::instance();
    if (llm.is_configured()) {
        const QString provider_raw = llm.active_provider();
        const bool is_fincept = (provider_raw.toLower() == "fincept");

        // Display names
        const QString prov_display = is_fincept ? "Fincept LLM" : provider_raw.toUpper();
        const QString model_raw = llm.active_model();
        // For fincept, don't expose internal model name
        const QString model_display = is_fincept ? "Fincept LLM" : model_raw;
        QString model_short = model_display;
        if (model_short.length() > 24)
            model_short = model_short.left(22) + "..";

        // Sidebar footer
        provider_lbl_->setText(prov_display);
        provider_lbl_->setStyleSheet(
            QString("color:%1;font-size:%2px;font-weight:600;").arg(col::AMBER()).arg(fnt::SMALL));
        model_lbl_->setText(is_fincept ? "Managed by Fincept" : model_short);
        model_lbl_->setToolTip(is_fincept ? "Fincept LLM — managed AI service" : model_raw);
        model_lbl_->setStyleSheet(QString("color:%1;font-size:%2px;").arg(col::TEXT_SECONDARY()).arg(fnt::TINY));

        // Header model pill — show "Provider / Model" for clarity
        if (is_fincept) {
            hdr_model_lbl_->setText("Fincept LLM");
            hdr_model_lbl_->setToolTip("Fincept managed AI service\n\nChange in Settings > LLM Configuration");
        } else {
            hdr_model_lbl_->setText(provider_raw.left(1).toUpper() + provider_raw.mid(1) + " / " + model_short);
            hdr_model_lbl_->setToolTip("Provider: " + prov_display + "\nModel: " + model_raw +
                                       "\n\nChange in Settings > LLM Configuration");
        }
    } else {
        provider_lbl_->setText("No provider");
        provider_lbl_->setStyleSheet(
            QString("color:%1;font-size:%2px;font-weight:600;").arg(col::NEGATIVE()).arg(fnt::SMALL));
        model_lbl_->setText("Configure in Settings");
        model_lbl_->setStyleSheet(QString("color:%1;font-size:%2px;").arg(col::TEXT_DIM()).arg(fnt::TINY));
        hdr_model_lbl_->setText("No model");
    }
}

} // namespace fincept::screens
