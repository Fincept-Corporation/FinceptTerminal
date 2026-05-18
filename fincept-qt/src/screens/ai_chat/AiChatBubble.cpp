// AiChatBubble.cpp — "Quick Chat" floating bubble.
//
// Stays explicitly separate from the AI Chat tab:
//   • No ChatRepository writes — chat history is in-memory only.
//   • Conversations never appear in the AI Chat tab's session list.
//
// UI is intentionally minimal:
//   ┌─ Header (title + subtitle "not saved" + voice / clear / close) ─┐
//   │  ...messages, scrollable                                        │
//   │  ────────────────────────────────────────────────────────────── │
//   │  [ input box .................. ] 🎤  [Send]                    │
//   │  status strip (one line, hidden when idle)                      │
//   └──────────────────────────────────────────────────────────────────┘

#include "screens/ai_chat/AiChatBubble.h"

#include "screens/ai_chat/ChatBubbleFactory.h"
#include "services/stt/SpeechService.h"
#include "services/tts/TtsService.h"
#include "services/voice_trigger/ClapDetectorService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QDateTime>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QUrl>

#include <algorithm>
#include <memory>

namespace fincept {

namespace col = fincept::ui::colors;

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr int PANEL_W   = 380;
static constexpr int PANEL_H   = 540;
static constexpr int BTN_SIZE  = 42;
static constexpr int MARGIN    = 16;
static constexpr int HEADER_H  = 56;
static constexpr int INPUT_H   = 60;
static constexpr int STATUS_H  = 26;

// Bubble visuals are owned by ChatBubbleFactory — shared with AiChatScreen.
// The bubble passes a smaller column max width than the full-width tab.
static constexpr int kBubbleColMaxWidth = static_cast<int>(PANEL_W * 0.84);

// ── Constructor ───────────────────────────────────────────────────────────────
AiChatBubble::AiChatBubble(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);

    build_bubble_button();
    build_chat_panel();
    chat_panel_->hide();

    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::finished_streaming, this,
            &AiChatBubble::on_streaming_done, Qt::UniqueConnection);

    // STT wiring
    auto& stt = services::SpeechService::instance();
    connect(&stt, &services::SpeechService::transcription_ready, this, &AiChatBubble::on_transcription);
    connect(&stt, &services::SpeechService::listening_changed,    this, &AiChatBubble::on_stt_listening_changed);
    connect(&stt, &services::SpeechService::error_occurred,       this, &AiChatBubble::on_stt_error);

    // TTS wiring — drives the "Speaking" status + auto-resume in voice mode
    auto& tts = services::TtsService::instance();
    connect(&tts, &services::TtsService::speaking_started, this, [this]() {
        is_speaking_ = true;
        error_msg_.clear();
        render_status();
    });
    connect(&tts, &services::TtsService::speaking_finished, this, [this]() {
        is_speaking_ = false;
        render_status();
        if (voice_mode_ && is_open_ && !is_listening_ && !streaming_)
            QTimer::singleShot(400, this, &AiChatBubble::start_listening);
    });
    connect(&tts, &services::TtsService::error_occurred, this, [this](const QString& msg) {
        LOG_WARN("AiChatBubble", QString("TTS error: %1").arg(msg));
        is_speaking_ = false;
        error_msg_   = msg;
        render_status();
        if (voice_mode_ && is_open_ && !is_listening_)
            QTimer::singleShot(600, this, &AiChatBubble::start_listening);
    });

    // Clap-to-start trigger. The service is idle by default — only started
    // when AppConfig "voice/clap_to_start/enabled" is true. When the service
    // hears a clap it fires `clap_detected()`; we open the panel and start
    // the mic. The detector pauses while STT is active and resumes on stop.
    auto& clap = services::ClapDetectorService::instance();
    connect(&clap, &services::ClapDetectorService::clap_detected, this, [this]() {
        LOG_INFO("AiChatBubble", "Clap detected — opening panel + starting mic");
        if (!is_open_)
            open_panel();
        if (!is_listening_ && !is_speaking_)
            start_listening();
    });
    if (services::ClapDetectorService::is_enabled_in_config())
        clap.start();

    if (parent)
        parent->installEventFilter(this);

    reposition();
    raise();

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
}

// ── Theme refresh ─────────────────────────────────────────────────────────────
void AiChatBubble::refresh_theme() {
    if (bubble_btn_)
        bubble_btn_->setStyleSheet(QString("background:%1;border:2px solid %2;border-radius:0px;")
                                       .arg(col::BG_SURFACE(), col::BORDER_BRIGHT()));
    if (chat_panel_)
        chat_panel_->setStyleSheet(
            QString("QWidget#chatPanel{background:%1;border:1px solid %2;border-radius:0px;}")
                .arg(col::BG_SURFACE(), col::BORDER_MED()));
}

// ── Reposition ────────────────────────────────────────────────────────────────
void AiChatBubble::reposition() {
    if (!parentWidget())
        return;

    const QRect pr = parentWidget()->rect();
    const int bx = pr.width()  - BTN_SIZE - MARGIN;
    const int by = pr.height() - BTN_SIZE - MARGIN;
    setGeometry(bx, by, BTN_SIZE, BTN_SIZE);
    bubble_btn_->setGeometry(0, 0, BTN_SIZE, BTN_SIZE);

    const int px = pr.width()  - PANEL_W - MARGIN;
    const int py = pr.height() - PANEL_H - BTN_SIZE - MARGIN - 10;
    if (chat_panel_->parentWidget() != parentWidget())
        chat_panel_->setParent(parentWidget());
    chat_panel_->setGeometry(px, (py < 4 ? 4 : py), PANEL_W, PANEL_H);

    if (is_open_) {
        chat_panel_->show();
        chat_panel_->raise();
    }
    raise();
}

// ── Bubble (collapsed) button ────────────────────────────────────────────────
void AiChatBubble::build_bubble_button() {
    bubble_btn_ = new QWidget(this);
    bubble_btn_->setFixedSize(BTN_SIZE, BTN_SIZE);
    bubble_btn_->setCursor(Qt::PointingHandCursor);
    bubble_btn_->setToolTip("Quick Chat (separate from AI Chat tab)");
    bubble_btn_->setStyleSheet(QString("background:%1;border:2px solid %2;border-radius:0px;")
                                   .arg(col::BG_SURFACE(), col::BORDER_BRIGHT()));

    auto* lbl = new QLabel("⬡", bubble_btn_);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setGeometry(0, 0, BTN_SIZE, BTN_SIZE);
    lbl->setStyleSheet(QString("color:%1;font-size:24px;background:transparent;")
                           .arg(col::TEXT_PRIMARY()));

    unread_badge_ = new QLabel(bubble_btn_);
    unread_badge_->setFixedSize(18, 18);
    unread_badge_->setAlignment(Qt::AlignCenter);
    unread_badge_->move(BTN_SIZE - 16, -2);
    unread_badge_->setStyleSheet(QString("background:%1;color:white;border-radius:0px;"
                                         "font-size:9px;font-weight:bold;")
                                     .arg(col::NEGATIVE()));
    unread_badge_->hide();

    bubble_btn_->installEventFilter(this);
}

// ── Chat panel ────────────────────────────────────────────────────────────────
void AiChatBubble::build_chat_panel() {
    chat_panel_ = new QWidget(nullptr);
    chat_panel_->setObjectName("chatPanel");
    chat_panel_->setFixedSize(PANEL_W, PANEL_H);
    chat_panel_->setStyleSheet(
        QString("QWidget#chatPanel{background:%1;border:1px solid %2;border-radius:0px;}")
            .arg(col::BG_SURFACE(), col::BORDER_MED()));

    auto* vl = new QVBoxLayout(chat_panel_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_panel_header());

    // ── Messages scroll area ─────────────────────────────────────────────────
    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area_->setFrameShape(QFrame::NoFrame);
    scroll_area_->setStyleSheet(QString("QScrollArea{background:%1;border:none;}"
                                        "QScrollBar:vertical{background:transparent;width:5px;margin:2px;}"
                                        "QScrollBar::handle:vertical{background:%2;border-radius:0px;min-height:24px;}"
                                        "QScrollBar::handle:vertical:hover{background:%3;}"
                                        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                                    .arg(col::BG_SURFACE(), col::BORDER_MED(), col::AMBER()));

    msg_container_ = new QWidget;
    msg_container_->setStyleSheet(QString("background:%1;").arg(col::BG_SURFACE()));
    msg_layout_ = new QVBoxLayout(msg_container_);
    msg_layout_->setContentsMargins(14, 14, 14, 10);
    msg_layout_->setSpacing(10);
    msg_layout_->addStretch();

    scroll_area_->setWidget(msg_container_);
    vl->addWidget(scroll_area_, 1);

    build_welcome_widget();
    show_welcome_if_empty();

    // ── Divider ──────────────────────────────────────────────────────────────
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(col::BORDER_DIM()));
    vl->addWidget(sep);

    vl->addWidget(build_input_row());
    vl->addWidget(build_status_strip());
}

QWidget* AiChatBubble::build_panel_header() {
    auto* hdr = new QWidget;
    hdr->setFixedHeight(HEADER_H);
    hdr->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                           .arg(col::BG_RAISED(), col::BORDER_MED()));

    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(14, 8, 8, 8);
    hl->setSpacing(8);

    // Title block — title + subtitle stacked
    auto* title_col = new QWidget;
    title_col->setStyleSheet("background:transparent;");
    auto* tl = new QVBoxLayout(title_col);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(0);

    auto* title = new QLabel("Quick Chat");
    title->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;background:transparent;")
                             .arg(col::TEXT_PRIMARY()));
    tl->addWidget(title);

    auto* subtitle = new QLabel("Not saved · separate from AI Chat tab");
    subtitle->setStyleSheet(QString("color:%1;font-size:10px;background:transparent;")
                                .arg(col::TEXT_TERTIARY()));
    tl->addWidget(subtitle);

    hl->addWidget(title_col, 1);

    // Voice mode toggle (continuous mic + speak)
    voice_mode_btn_ = new QPushButton("Voice");
    voice_mode_btn_->setFixedHeight(26);
    voice_mode_btn_->setCheckable(true);
    voice_mode_btn_->setCursor(Qt::PointingHandCursor);
    voice_mode_btn_->setToolTip("Toggle continuous voice conversation (auto-listen + auto-speak)");
    voice_mode_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                "font-size:11px;font-weight:600;border-radius:0px;padding:0 10px;}"
                "QPushButton:checked{background:%3;color:%4;border-color:%3;}"
                "QPushButton:hover:!checked{color:%5;border-color:%5;}")
            .arg(col::TEXT_SECONDARY(), col::BORDER_MED(),
                 col::AMBER(), col::BG_BASE(), col::AMBER()));
    connect(voice_mode_btn_, &QPushButton::clicked, this, &AiChatBubble::on_toggle_voice_mode);
    hl->addWidget(voice_mode_btn_);

    // Clear chat (does NOT touch ChatRepository — wipes in-memory only)
    new_btn_ = new QPushButton("Clear");
    new_btn_->setFixedHeight(26);
    new_btn_->setCursor(Qt::PointingHandCursor);
    new_btn_->setToolTip("Clear this chat (Ctrl+L)");
    new_btn_->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                                    "font-size:11px;font-weight:600;border-radius:0px;padding:0 10px;}"
                                    "QPushButton:hover{color:%3;border-color:%3;}")
                                .arg(col::TEXT_SECONDARY(), col::BORDER_MED(), col::AMBER()));
    connect(new_btn_, &QPushButton::clicked, this, &AiChatBubble::on_clear_chat);
    hl->addWidget(new_btn_);

    // Close (Esc)
    close_btn_ = new QPushButton("X");
    close_btn_->setFixedSize(26, 26);
    close_btn_->setCursor(Qt::PointingHandCursor);
    close_btn_->setToolTip("Close (Esc)");
    close_btn_->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:none;font-size:14px;}"
                                      "QPushButton:hover{color:%2;}")
                                  .arg(col::TEXT_PRIMARY(), col::NEGATIVE()));
    connect(close_btn_, &QPushButton::clicked, this, &AiChatBubble::close_panel);
    hl->addWidget(close_btn_);

    return hdr;
}

QWidget* AiChatBubble::build_input_row() {
    auto* row = new QWidget;
    row->setFixedHeight(INPUT_H);
    row->setStyleSheet(QString("background:%1;").arg(col::BG_SURFACE()));

    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(10, 10, 10, 10);
    hl->setSpacing(6);

    input_box_ = new QPlainTextEdit;
    input_box_->setFixedHeight(40);
    input_box_->setPlaceholderText("Ask anything…  (Enter to send, Shift+Enter for newline)");
    input_box_->setStyleSheet(QString("QPlainTextEdit{background:%1;color:%2;border:1px solid %3;"
                                      "border-radius:0px;padding:6px 10px;font-size:13px;}"
                                      "QPlainTextEdit:focus{border-color:%4;}")
                                  .arg(col::BG_RAISED(), col::TEXT_PRIMARY(),
                                       col::BORDER_MED(), col::AMBER()));
    input_box_->installEventFilter(this);
    hl->addWidget(input_box_, 1);

    mic_btn_ = new QPushButton("🎤");
    mic_btn_->setFixedSize(38, 38);
    mic_btn_->setCursor(Qt::PointingHandCursor);
    mic_btn_->setToolTip("Push-to-talk: click to dictate one message");
    // No setCheckable — listening state is rendered via property + opacity.
    mic_btn_->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                                    "border-radius:0px;font-size:18px;}"
                                    "QPushButton:hover{border-color:%3;}"
                                    "QPushButton[listening=\"true\"]{background:%4;border-color:%3;color:%5;}")
                                .arg(col::TEXT_PRIMARY(), col::BORDER_MED(), col::AMBER(),
                                     col::AMBER_DIM(), col::BG_BASE()));
    mic_btn_->setProperty("listening", false);
    connect(mic_btn_, &QPushButton::clicked, this, &AiChatBubble::on_toggle_mic);
    hl->addWidget(mic_btn_);

    send_btn_ = new QPushButton("Send");
    send_btn_->setFixedSize(56, 38);
    send_btn_->setCursor(Qt::PointingHandCursor);
    send_btn_->setToolTip("Send (Enter)");
    send_btn_->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:none;"
                                     "border-radius:0px;font-size:12px;font-weight:700;}"
                                     "QPushButton:hover:enabled{background:%3;}"
                                     "QPushButton:disabled{background:%4;color:%5;}")
                                 .arg(col::AMBER(), col::BG_BASE(), col::AMBER_DIM(),
                                      col::BG_RAISED(), col::TEXT_TERTIARY()));
    connect(send_btn_, &QPushButton::clicked, this, &AiChatBubble::on_send);
    hl->addWidget(send_btn_);

    return row;
}

QWidget* AiChatBubble::build_status_strip() {
    status_strip_ = new QWidget;
    status_strip_->setFixedHeight(STATUS_H);
    status_strip_->setStyleSheet(QString("background:%1;border-top:1px solid %2;")
                                     .arg(col::BG_RAISED(), col::BORDER_DIM()));

    auto* hl = new QHBoxLayout(status_strip_);
    hl->setContentsMargins(12, 0, 8, 0);
    hl->setSpacing(8);

    status_dot_ = new QLabel("●");
    status_dot_->setStyleSheet(QString("color:%1;font-size:11px;background:transparent;")
                                   .arg(col::TEXT_TERTIARY()));
    hl->addWidget(status_dot_);

    status_lbl_ = new QLabel;
    status_lbl_->setStyleSheet(QString("color:%1;font-size:11px;background:transparent;")
                                   .arg(col::TEXT_TERTIARY()));
    status_lbl_->setWordWrap(false);
    status_lbl_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    hl->addWidget(status_lbl_, 1);

    stop_speech_btn_ = new QPushButton("Stop");
    stop_speech_btn_->setFixedHeight(20);
    stop_speech_btn_->setCursor(Qt::PointingHandCursor);
    stop_speech_btn_->setToolTip("Stop speaking");
    stop_speech_btn_->setStyleSheet(QString("QPushButton{background:%1;color:white;border:none;"
                                            "font-size:10px;font-weight:700;border-radius:0px;padding:0 10px;}"
                                            "QPushButton:hover{background:%2;}")
                                        .arg(col::NEGATIVE(), col::WARNING()));
    stop_speech_btn_->hide();
    connect(stop_speech_btn_, &QPushButton::clicked, this, &AiChatBubble::on_stop_speech);
    hl->addWidget(stop_speech_btn_);

    status_strip_->hide();   // hidden when status is Idle
    return status_strip_;
}

// ── Welcome card (replaces the fragile "remove on first message" trick) ─────
void AiChatBubble::build_welcome_widget() {
    welcome_widget_ = new QWidget;
    welcome_widget_->setStyleSheet("background:transparent;");
    auto* wvl = new QVBoxLayout(welcome_widget_);
    wvl->setContentsMargins(8, 32, 8, 8);
    wvl->setSpacing(8);

    auto* glyph = new QLabel("⬡");
    glyph->setAlignment(Qt::AlignCenter);
    glyph->setStyleSheet(QString("color:%1;font-size:36px;background:transparent;")
                             .arg(col::AMBER()));
    wvl->addWidget(glyph);

    auto* h = new QLabel("How can I help?");
    h->setAlignment(Qt::AlignCenter);
    h->setStyleSheet(QString("color:%1;font-size:14px;font-weight:700;background:transparent;")
                         .arg(col::TEXT_PRIMARY()));
    wvl->addWidget(h);

    auto* sub = new QLabel("Ask a quick question. Nothing here is saved.\n"
                           "For long-form chats use the AI Chat tab.");
    sub->setAlignment(Qt::AlignCenter);
    sub->setWordWrap(true);
    sub->setStyleSheet(QString("color:%1;font-size:11px;background:transparent;")
                           .arg(col::TEXT_TERTIARY()));
    wvl->addWidget(sub);

    wvl->addStretch();
}

void AiChatBubble::show_welcome_if_empty() {
    if (!welcome_widget_ || !msg_layout_)
        return;
    if (chat_history_.empty() && welcome_widget_->parent() != msg_container_) {
        msg_layout_->insertWidget(msg_layout_->count() - 1, welcome_widget_);
        welcome_widget_->show();
    }
}

void AiChatBubble::hide_welcome() {
    if (welcome_widget_ && welcome_widget_->parent() == msg_container_) {
        msg_layout_->removeWidget(welcome_widget_);
        welcome_widget_->setParent(nullptr);
        welcome_widget_->hide();
    }
}

// ── Open / Close ──────────────────────────────────────────────────────────────
void AiChatBubble::open_panel() {
    if (is_open_)
        return;
    is_open_ = true;
    unread_count_ = 0;
    unread_badge_->hide();
    reposition();
    chat_panel_->show();
    chat_panel_->raise();
    raise();
    input_box_->setFocus();
    render_status();
}

void AiChatBubble::close_panel() {
    is_open_ = false;
    // Voice mode running while the panel is hidden is confusing — turn it off.
    if (voice_mode_) {
        voice_mode_ = false;
        if (voice_mode_btn_)
            voice_mode_btn_->setChecked(false);
    }
    if (is_listening_)
        stop_listening();
    if (is_speaking_)
        stop_tts();
    chat_panel_->hide();
    // Closing the panel doesn't disable clap-to-start — that is a global
    // wake-up trigger, not panel state. The clap detector keeps running so
    // the user can summon the bubble back with a clap.
}

// ── Clear chat (in-memory only — never touches ChatRepository) ────────────────
void AiChatBubble::on_clear_chat() {
    LOG_INFO("AiChatBubble", "Clearing chat (in-memory only)");
    // Stop any ongoing voice activity
    if (is_listening_) stop_listening();
    if (is_speaking_)  stop_tts();

    // Tear down message widgets but keep the trailing stretch.
    while (msg_layout_->count() > 1) {
        auto* item = msg_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    chat_history_.clear();
    streaming_bubble_.clear();
    streaming_ = false;
    error_msg_.clear();

    // Rebuild welcome card and re-show it.
    build_welcome_widget();
    show_welcome_if_empty();
    set_input_enabled(true);
    render_status();
}

// ── Send ──────────────────────────────────────────────────────────────────────
void AiChatBubble::on_send() {
    const QString text = input_box_->toPlainText().trimmed();
    if (text.isEmpty() || streaming_)
        return;

    // Fail fast if LLM is not configured — inline error beats a confusing
    // delayed network failure.
    if (!ai_chat::LlmService::instance().is_configured()) {
        add_bubble("assistant",
                   "AI chat is not configured. Open **Settings → LLM Config** "
                   "and add an API key or pick the Fincept provider.");
        return;
    }

    input_box_->clear();
    hide_welcome();

    add_bubble("user", text);
    chat_history_.push_back({"user", text});

    streaming_ = true;
    error_msg_.clear();
    set_input_enabled(false);
    render_status();

    // Defer streaming bubble creation to the first non-empty chunk.
    QPointer<AiChatBubble> self = this;
    auto first_chunk = std::make_shared<bool>(true);
    // ToolPolicy::NoNavigation — the floating bubble lets the model use tools
    // (so "add SPGI to my watchlist" works), but hides the `navigation`
    // category so the model can't yank the user out of their current screen
    // by calling navigate_to_tab / list_tabs / get_current_tab.
    ai_chat::LlmService::instance().chat_streaming(
        text, chat_history_,
        [self, first_chunk](const QString& chunk, bool done) {
            QMetaObject::invokeMethod(
                qApp,
                [self, chunk, done, first_chunk]() {
                    if (!self)
                        return;
                    if (*first_chunk && !chunk.isEmpty()) {
                        *first_chunk = false;
                        self->streaming_bubble_ = self->add_streaming_bubble();
                    }
                    self->on_stream_chunk(chunk, done);
                },
                Qt::QueuedConnection);
        },
        ai_chat::LlmService::ToolPolicy::NoNavigation);
}

void AiChatBubble::on_stream_chunk(const QString& chunk, bool done) {
    QLabel* bubble = streaming_bubble_;
    if (!bubble)
        return;
    if (!chunk.isEmpty()) {
        fincept::ai_chat::ChatBubbleFactory::append_streaming_chunk(bubble, chunk);
        scroll_to_bottom();
    }
    Q_UNUSED(done)
}

void AiChatBubble::on_streaming_done(ai_chat::LlmResponse response) {
    streaming_ = false;
    set_input_enabled(true);

    const QString content = response.success ? response.content
                                             : QString("Error: %1").arg(response.error);

    if (!streaming_bubble_ && !content.isEmpty())
        streaming_bubble_ = add_streaming_bubble();

    if (streaming_bubble_) {
        const QString acc = streaming_bubble_->property("acc").toString();
        const QString final_text = acc.isEmpty() ? content : acc;
        fincept::ai_chat::ChatBubbleFactory::finalize_streaming(streaming_bubble_, final_text);
        streaming_bubble_ = nullptr;
    }

    chat_history_.push_back({"assistant", content});

    if (!is_open_)
        update_unread(1);

    if (voice_mode_ && response.success) {
        speak_text(content);
    } else if (voice_mode_) {
        start_listening();
    }
    render_status();
}

// ── Message bubbles ───────────────────────────────────────────────────────────
void AiChatBubble::add_bubble(const QString& role, const QString& text) {
    fincept::ai_chat::ChatBubbleFactory::Options opts;
    opts.role               = role;
    opts.content            = text;
    opts.show_footer        = true;
    opts.user_col_max_width = kBubbleColMaxWidth;
    opts.ai_col_max_width   = kBubbleColMaxWidth;
    auto b = fincept::ai_chat::ChatBubbleFactory::build(opts);
    msg_layout_->insertWidget(msg_layout_->count() - 1, b.row);
    scroll_to_bottom();
}

QLabel* AiChatBubble::add_streaming_bubble() {
    fincept::ai_chat::ChatBubbleFactory::Options opts;
    opts.role               = "assistant";
    opts.show_footer        = true;
    opts.user_col_max_width = kBubbleColMaxWidth;
    opts.ai_col_max_width   = kBubbleColMaxWidth;
    auto b = fincept::ai_chat::ChatBubbleFactory::build_streaming(opts);
    msg_layout_->insertWidget(msg_layout_->count() - 1, b.row);
    scroll_to_bottom();
    return b.body;
}

void AiChatBubble::scroll_to_bottom() {
    QTimer::singleShot(30, this, [this]() {
        if (scroll_area_ && scroll_area_->verticalScrollBar())
            scroll_area_->verticalScrollBar()->setValue(scroll_area_->verticalScrollBar()->maximum());
    });
}

void AiChatBubble::set_input_enabled(bool enabled) {
    send_btn_->setEnabled(enabled);
    input_box_->setEnabled(enabled);
    // Mic stays clickable so the user can cancel a stuck listen, but it's
    // greyed visually while streaming.
    mic_btn_->setEnabled(enabled || is_listening_);
    send_btn_->setText(enabled ? "Send" : "…");
}

void AiChatBubble::update_unread(int delta) {
    unread_count_ += delta;
    if (unread_count_ > 0) {
        unread_badge_->setText(unread_count_ > 9 ? "9+" : QString::number(unread_count_));
        unread_badge_->show();
    } else {
        unread_badge_->hide();
    }
}

// ── Voice input (STT) ────────────────────────────────────────────────────────
void AiChatBubble::on_toggle_mic() {
    LOG_INFO("AiChatBubble", QString("Mic toggled — listening=%1 speaking=%2 voice_mode=%3")
                                 .arg(is_listening_).arg(is_speaking_).arg(voice_mode_));
    if (is_listening_)
        stop_listening();
    else
        start_listening();
}

void AiChatBubble::start_listening() {
    LOG_INFO("AiChatBubble", QString("start_listening — listening=%1 speaking=%2")
                                 .arg(is_listening_).arg(is_speaking_));
    if (is_listening_ || is_speaking_)
        return;
    // Pause clap detector while STT owns the mic — avoids the user's own
    // speech (or the start-of-utterance plosive) being heard as a clap.
    if (services::ClapDetectorService::instance().is_active())
        services::ClapDetectorService::instance().stop();
    services::SpeechService::instance().start_listening();
}

void AiChatBubble::stop_listening() {
    LOG_INFO("AiChatBubble", "stop_listening — forwarding to SpeechService");
    services::SpeechService::instance().stop_listening();
}

void AiChatBubble::on_stt_listening_changed(bool active) {
    is_listening_ = active;
    set_mic_listening_visual(active);
    if (active)
        error_msg_.clear();
    render_status();

    // STT released the mic — bring the clap detector back if still enabled.
    if (!active && !is_speaking_ && !streaming_ &&
        services::ClapDetectorService::is_enabled_in_config() &&
        !services::ClapDetectorService::instance().is_active()) {
        services::ClapDetectorService::instance().start();
    }
}

void AiChatBubble::on_transcription(const QString& text) {
    LOG_INFO("AiChatBubble", QString("Transcription: len=%1 voice_mode=%2")
                                 .arg(text.length()).arg(voice_mode_));
    if (text.isEmpty())
        return;

    input_box_->setPlainText(text);

    if (!voice_mode_) {
        stop_listening();
    } else {
        on_send();
    }
    render_status();
}

void AiChatBubble::on_stt_error(const QString& message) {
    LOG_WARN("AiChatBubble", QString("STT error: %1").arg(message));
    error_msg_ = message;
    render_status();
}

// ── Voice mode (continuous loop) ─────────────────────────────────────────────
void AiChatBubble::on_toggle_voice_mode() {
    voice_mode_ = voice_mode_btn_->isChecked();
    LOG_INFO("AiChatBubble", QString("Voice mode -> %1").arg(voice_mode_));
    if (voice_mode_) {
        if (!is_open_)
            open_panel();
        QTimer::singleShot(300, this, &AiChatBubble::start_listening);
    } else {
        stop_listening();
        stop_tts();
    }
    render_status();
}

// ── TTS ───────────────────────────────────────────────────────────────────────
void AiChatBubble::speak_text(const QString& text) {
    LOG_INFO("AiChatBubble", QString("speak_text len=%1").arg(text.length()));
    if (text.isEmpty())
        return;

    // Strip markdown for TTS — engines speak raw text.
    QString clean = text;
    clean.remove(QRegularExpression(R"(\*\*|__|~~|```[^`]*```|`[^`]*`)"));
    clean.remove(QRegularExpression(R"(#{1,6} )"));
    clean = clean.trimmed();

    if (clean.isEmpty()) {
        if (voice_mode_)
            start_listening();
        return;
    }
    services::TtsService::instance().speak(clean.left(1200));
}

void AiChatBubble::stop_tts() {
    LOG_INFO("AiChatBubble", "stop_tts");
    services::TtsService::instance().stop();
    is_speaking_ = false;
    render_status();
}

void AiChatBubble::on_stop_speech() {
    stop_tts();
    if (voice_mode_ && !is_listening_ && !streaming_)
        QTimer::singleShot(200, this, &AiChatBubble::start_listening);
}

// ── Status strip rendering (single source of truth) ──────────────────────────
void AiChatBubble::render_status() {
    if (!status_strip_)
        return;

    Status s = Status::Idle;
    if (!error_msg_.isEmpty())   s = Status::Error;
    else if (is_speaking_)       s = Status::Speaking;
    else if (is_listening_)      s = Status::Listening;
    else if (streaming_)         s = Status::Thinking;

    QString color, text;
    bool show_stop = false;
    switch (s) {
        case Status::Idle:
            status_strip_->setVisible(false);
            return;
        case Status::Listening:
            color = col::POSITIVE();
            text  = "Listening — speak now";
            break;
        case Status::Thinking:
            color = col::TEXT_DIM();
            text  = "AI is thinking…";
            break;
        case Status::Speaking:
            color = col::WARNING();
            text  = "AI speaking…";
            show_stop = true;
            break;
        case Status::Error:
            color = col::NEGATIVE();
            text  = error_msg_;
            break;
    }

    status_strip_->setVisible(true);
    status_dot_->setStyleSheet(QString("color:%1;font-size:11px;background:transparent;").arg(color));
    status_lbl_->setText(text);
    status_lbl_->setStyleSheet(QString("color:%1;font-size:11px;background:transparent;").arg(color));
    status_lbl_->setToolTip(text);
    stop_speech_btn_->setVisible(show_stop);
}

void AiChatBubble::set_mic_listening_visual(bool on) {
    if (!mic_btn_)
        return;
    mic_btn_->setProperty("listening", on);
    // Re-polish so [listening="true"] selector takes effect.
    mic_btn_->style()->unpolish(mic_btn_);
    mic_btn_->style()->polish(mic_btn_);
    mic_btn_->update();
}

// ── Event filter (bubble click, Enter/Shift+Enter, Esc, Ctrl+L) ──────────────
bool AiChatBubble::eventFilter(QObject* obj, QEvent* e) {
    if (obj == bubble_btn_ && e->type() == QEvent::MouseButtonRelease) {
        if (is_open_)
            close_panel();
        else
            open_panel();
        return true;
    }
    if (obj == input_box_ && e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Return && !(ke->modifiers() & Qt::ShiftModifier)) {
            on_send();
            return true;
        }
        if (ke->key() == Qt::Key_Escape) {
            close_panel();
            return true;
        }
        if (ke->key() == Qt::Key_L && (ke->modifiers() & Qt::ControlModifier)) {
            on_clear_chat();
            return true;
        }
    }
    if (obj == parentWidget() && e->type() == QEvent::Resize) {
        reposition();
        return false;
    }
    return QWidget::eventFilter(obj, e);
}

} // namespace fincept
