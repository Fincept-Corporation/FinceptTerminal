#include "ai_chat/AiChatBubble.h"

#include "services/stt/WhisperService.h"
#include "storage/repositories/ChatRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QAudioOutput>
#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <memory>

namespace fincept {

namespace col = fincept::ui::colors;

// Override Qt's default blue palette on QTextEdit to match Obsidian theme.
// Without this, links render blue and selection highlights are blue.
static void apply_obsidian_palette(QTextEdit* edit) {
    QPalette p = edit->palette();
    p.setColor(QPalette::Link, QColor(col::AMBER()));
    p.setColor(QPalette::LinkVisited, QColor(col::AMBER()));
    p.setColor(QPalette::Highlight, QColor(col::AMBER_DIM()));
    p.setColor(QPalette::HighlightedText, QColor(col::TEXT_PRIMARY()));
    p.setColor(QPalette::Base, Qt::transparent);
    p.setColor(QPalette::Text, QColor(col::TEXT_PRIMARY()));
    edit->setPalette(p);
}
namespace fnt = fincept::ui::fonts;

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr int PANEL_W  = 320;
static constexpr int PANEL_H  = 440;
static constexpr int BTN_SIZE = 42;
static constexpr int MARGIN   = 16;

// ── Constructor ───────────────────────────────────────────────────────────────
AiChatBubble::AiChatBubble(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);

    tts_player_ = new QMediaPlayer(this);
    auto* ao = new QAudioOutput(this);
    tts_player_->setAudioOutput(ao);
    connect(tts_player_, &QMediaPlayer::mediaStatusChanged,
            this, &AiChatBubble::on_tts_media_status);

    pulse_timer_ = new QTimer(this);
    pulse_timer_->setInterval(400);
    connect(pulse_timer_, &QTimer::timeout, this, &AiChatBubble::animate_pulse);

    build_bubble_button();
    build_chat_panel();
    chat_panel_->hide();

    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::finished_streaming,
            this, &AiChatBubble::on_streaming_done, Qt::UniqueConnection);

    // ── WhisperService wiring ─────────────────────────────────────────────────
    auto& stt = services::WhisperService::instance();
    connect(&stt, &services::WhisperService::transcription_ready,
            this, &AiChatBubble::on_transcription);
    connect(&stt, &services::WhisperService::listening_changed,
            this, &AiChatBubble::on_stt_listening_changed);
    connect(&stt, &services::WhisperService::error_occurred,
            this, &AiChatBubble::on_stt_error);
    connect(&stt, &services::WhisperService::model_download_progress,
            this, &AiChatBubble::on_model_download_progress);
    connect(&stt, &services::WhisperService::model_ready,
            this, [this]() {
                update_voice_status();
                if (voice_mode_) start_listening();
            });

    if (parent) parent->installEventFilter(this);
    reposition();
    raise();

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed,
            this, [this](const ui::ThemeTokens&) { refresh_theme(); });
}

// ── Theme refresh ─────────────────────────────────────────────────────────────
void AiChatBubble::refresh_theme() {
    if (bubble_btn_)
        bubble_btn_->setStyleSheet(
            QString("background:%1;border:2px solid %2;border-radius:0px;")
            .arg(col::BG_SURFACE(), col::BORDER_BRIGHT()));
    if (chat_panel_)
        chat_panel_->setStyleSheet(
            QString("background:%1;border:1px solid %2;border-radius:0px;")
            .arg(col::BG_SURFACE(), col::BORDER_MED()));
    if (msg_container_)
        msg_container_->setStyleSheet(QString("background:%1;").arg(col::BG_SURFACE()));
    if (scroll_area_)
        scroll_area_->setStyleSheet(
            QString("QScrollArea{background:%1;border:none;}"
                    "QScrollBar:vertical{background:transparent;width:4px;margin:2px;}"
                    "QScrollBar::handle:vertical{background:%2;border-radius:0px;min-height:20px;}"
                    "QScrollBar::handle:vertical:hover{background:%3;}"
                    "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
            .arg(col::BG_SURFACE(), col::BORDER_MED(), col::AMBER()));
    if (input_box_)
        input_box_->setStyleSheet(
            QString("QPlainTextEdit{background:%1;color:%2;border:1px solid %3;"
                    "border-radius:0px;padding:6px 10px;font-size:13px;}"
                    "QPlainTextEdit:focus{border-color:%4;}")
            .arg(col::BG_RAISED(), col::TEXT_PRIMARY(), col::BORDER_MED(), col::AMBER()));
    if (send_btn_)
        send_btn_->setStyleSheet(
            QString("QPushButton{background:%1;color:%2;border:none;"
                    "border-radius:0px;font-size:20px;font-weight:700;}"
                    "QPushButton:hover:enabled{background:%3;}"
                    "QPushButton:disabled{background:%4;color:%5;}")
            .arg(col::AMBER(), col::BG_BASE(), col::AMBER_DIM(),
                 col::BG_RAISED(), col::TEXT_TERTIARY()));
    if (mic_btn_)
        mic_btn_->setStyleSheet(
            QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                    "border-radius:0px;font-size:18px;}"
                    "QPushButton:hover{border-color:%3;}"
                    "QPushButton:checked{background:%4;border-color:%4;}")
            .arg(col::TEXT_PRIMARY(), col::BORDER_MED(), col::AMBER(), col::AMBER_DIM()));
}

// ── Reposition ────────────────────────────────────────────────────────────────
void AiChatBubble::reposition() {
    if (!parentWidget()) return;

    const QRect pr = parentWidget()->rect();
    const int bx = pr.width()  - BTN_SIZE - MARGIN;
    const int by = pr.height() - BTN_SIZE - MARGIN;
    setGeometry(bx, by, BTN_SIZE, BTN_SIZE);
    bubble_btn_->setGeometry(0, 0, BTN_SIZE, BTN_SIZE);

    const int px = pr.width()  - PANEL_W  - MARGIN;
    const int py = pr.height() - PANEL_H  - BTN_SIZE - MARGIN - 10;
    if (chat_panel_->parentWidget() != parentWidget())
        chat_panel_->setParent(parentWidget());
    chat_panel_->setGeometry(px, (py < 4 ? 4 : py), PANEL_W, PANEL_H);
    if (is_open_) { chat_panel_->show(); chat_panel_->raise(); }
    raise();
}

// ── Bubble button ─────────────────────────────────────────────────────────────
void AiChatBubble::build_bubble_button() {
    bubble_btn_ = new QWidget(this);
    bubble_btn_->setFixedSize(BTN_SIZE, BTN_SIZE);
    bubble_btn_->setCursor(Qt::PointingHandCursor);
    bubble_btn_->setStyleSheet(
        QString("background:%1;border:2px solid %2;border-radius:0px;")
            .arg(col::BG_SURFACE(), col::BORDER_BRIGHT()));

    auto* lbl = new QLabel("⬡", bubble_btn_);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setGeometry(0, 0, BTN_SIZE, BTN_SIZE);
    lbl->setStyleSheet(
        QString("color:%1;font-size:24px;background:transparent;").arg(col::TEXT_PRIMARY()));

    unread_badge_ = new QLabel(bubble_btn_);
    unread_badge_->setFixedSize(18, 18);
    unread_badge_->setAlignment(Qt::AlignCenter);
    unread_badge_->move(BTN_SIZE - 16, -2);
    unread_badge_->setStyleSheet(
        QString("background:%1;color:white;border-radius:0px;"
                "font-size:9px;font-weight:bold;").arg(col::NEGATIVE()));
    unread_badge_->hide();

    bubble_btn_->installEventFilter(this);
}

// ── Chat panel ────────────────────────────────────────────────────────────────
void AiChatBubble::build_chat_panel() {
    chat_panel_ = new QWidget(nullptr);
    chat_panel_->setFixedSize(PANEL_W, PANEL_H);
    chat_panel_->setStyleSheet(
        QString("QWidget#chatPanel{background:%1;border:1px solid %2;border-radius:0px;}")
            .arg(col::BG_SURFACE(), col::BORDER_MED()));
    chat_panel_->setObjectName("chatPanel");

    auto* vl = new QVBoxLayout(chat_panel_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_panel_header());

    // Message scroll area
    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area_->setFrameShape(QFrame::NoFrame);
    scroll_area_->setStyleSheet(
        QString("QScrollArea{background:%1;border:none;}"
                "QScrollBar:vertical{background:transparent;width:4px;margin:2px;}"
                "QScrollBar::handle:vertical{background:%2;border-radius:0px;min-height:20px;}"
                "QScrollBar::handle:vertical:hover{background:%3;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
            .arg(col::BG_SURFACE(), col::BORDER_MED(), col::AMBER()));

    msg_container_ = new QWidget;
    msg_container_->setStyleSheet(
        QString("background:%1;").arg(col::BG_SURFACE()));
    msg_layout_ = new QVBoxLayout(msg_container_);
    msg_layout_->setContentsMargins(14, 14, 14, 10);
    msg_layout_->setSpacing(10);
    msg_layout_->addStretch();

    scroll_area_->setWidget(msg_container_);
    vl->addWidget(scroll_area_, 1);

    // Typing indicator
    auto* typing_row = new QWidget;
    typing_row->setStyleSheet("background:transparent;");
    typing_row->setFixedHeight(22);
    auto* typing_hl = new QHBoxLayout(typing_row);
    typing_hl->setContentsMargins(16, 0, 0, 0);
    typing_hl->setSpacing(0);
    // We use voice_status_lbl_ to also show typing state (reuse slot)
    voice_status_lbl_ = new QLabel;
    voice_status_lbl_->setStyleSheet(
        QString("color:%1;font-size:11px;font-style:italic;background:transparent;")
            .arg(col::TEXT_TERTIARY()));
    typing_hl->addWidget(voice_status_lbl_);
    typing_hl->addStretch();
    vl->addWidget(typing_row);

    // Divider
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(col::BORDER_DIM()));
    vl->addWidget(sep);

    vl->addWidget(build_input_row());
    vl->addWidget(build_voice_status_bar());
}

QWidget* AiChatBubble::build_panel_header() {
    auto* hdr = new QWidget;
    hdr->setFixedHeight(46);
    hdr->setStyleSheet(
        QString("background:%1;border-radius:0px;border-bottom:1px solid %2;")
            .arg(col::BG_RAISED(), col::BORDER_MED()));

    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(14, 0, 10, 0);
    hl->setSpacing(8);

    auto* icon = new QLabel("⬡");
    icon->setStyleSheet(
        QString("color:%1;font-size:20px;background:transparent;").arg(col::TEXT_PRIMARY()));
    hl->addWidget(icon);

    auto* title = new QLabel("Fincept AI");
    title->setStyleSheet(
        QString("color:%1;font-size:13px;font-weight:700;background:transparent;")
            .arg(col::TEXT_PRIMARY()));
    hl->addWidget(title, 1);

    // Voice mode toggle
    voice_mode_btn_ = new QPushButton("Voice");
    voice_mode_btn_->setFixedHeight(24);
    voice_mode_btn_->setCheckable(true);
    voice_mode_btn_->setCursor(Qt::PointingHandCursor);
    voice_mode_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                "font-size:11px;font-weight:600;border-radius:0px;padding:0 8px;}"
                "QPushButton:checked{background:%3;color:%4;border-color:%3;}"
                "QPushButton:hover:!checked{color:%4;border-color:%5;}")
            .arg(col::TEXT_SECONDARY(), col::BORDER_MED(),
                 col::AMBER(), col::BG_BASE(), col::AMBER()));
    connect(voice_mode_btn_, &QPushButton::clicked, this, &AiChatBubble::on_toggle_voice_mode);
    hl->addWidget(voice_mode_btn_);

    // New chat button
    new_btn_ = new QPushButton("＋");
    new_btn_->setFixedSize(28, 26);
    new_btn_->setCursor(Qt::PointingHandCursor);
    new_btn_->setToolTip("New conversation");
    new_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                "font-size:18px;font-weight:700;border-radius:0px;}"
                "QPushButton:hover{background:%3;color:%1;}")
            .arg(col::TEXT_PRIMARY(), col::BORDER_MED(), col::BG_HOVER()));
    connect(new_btn_, &QPushButton::clicked, this, &AiChatBubble::on_new_session);
    hl->addWidget(new_btn_);

    // Close button
    close_btn_ = new QPushButton("✕");
    close_btn_->setFixedSize(26, 26);
    close_btn_->setCursor(Qt::PointingHandCursor);
    close_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:none;font-size:16px;}"
                "QPushButton:hover{color:%2;}")
            .arg(col::TEXT_PRIMARY(), col::NEGATIVE()));
    connect(close_btn_, &QPushButton::clicked, this, &AiChatBubble::close_panel);
    hl->addWidget(close_btn_);

    return hdr;
}

QWidget* AiChatBubble::build_input_row() {
    auto* row = new QWidget;
    row->setFixedHeight(58);
    row->setStyleSheet(QString("background:%1;").arg(col::BG_SURFACE()));

    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(10, 10, 10, 10);
    hl->setSpacing(6);

    input_box_ = new QPlainTextEdit;
    input_box_->setFixedHeight(38);
    input_box_->setPlaceholderText("Ask anything…");
    input_box_->setStyleSheet(
        QString("QPlainTextEdit{background:%1;color:%2;border:1px solid %3;"
                "border-radius:0px;padding:6px 10px;font-size:13px;}"
                "QPlainTextEdit:focus{border-color:%4;}")
            .arg(col::BG_RAISED(), col::TEXT_PRIMARY(), col::BORDER_MED(), col::AMBER()));
    input_box_->installEventFilter(this);
    hl->addWidget(input_box_, 1);

    mic_btn_ = new QPushButton("🎤");
    mic_btn_->setFixedSize(36, 36);
    mic_btn_->setCheckable(true);
    mic_btn_->setCursor(Qt::PointingHandCursor);
    mic_btn_->setToolTip("Voice input");
    mic_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                "border-radius:0px;font-size:18px;}"
                "QPushButton:hover{border-color:%3;}"
                "QPushButton:checked{background:%4;border-color:%4;}")
            .arg(col::TEXT_PRIMARY(), col::BORDER_MED(), col::AMBER(), col::AMBER_DIM()));
    connect(mic_btn_, &QPushButton::clicked, this, &AiChatBubble::on_toggle_mic);
    hl->addWidget(mic_btn_);

    send_btn_ = new QPushButton("↑");
    send_btn_->setFixedSize(36, 36);
    send_btn_->setCursor(Qt::PointingHandCursor);
    send_btn_->setToolTip("Send  (Enter)");
    send_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:none;"
                "border-radius:0px;font-size:20px;font-weight:700;}"
                "QPushButton:hover:enabled{background:%3;}"
                "QPushButton:disabled{background:%4;color:%5;}")
            .arg(col::AMBER(), col::BG_BASE(), col::AMBER_DIM(),
                 col::BG_RAISED(), col::TEXT_TERTIARY()));
    connect(send_btn_, &QPushButton::clicked, this, &AiChatBubble::on_send);
    hl->addWidget(send_btn_);

    return row;
}

QWidget* AiChatBubble::build_voice_status_bar() {
    voice_status_bar_ = new QWidget;
    voice_status_bar_->setFixedHeight(28);
    voice_status_bar_->setStyleSheet(
        QString("background:%1;border-top:1px solid %2;border-radius:0px;")
            .arg(col::BG_RAISED(), col::BORDER_DIM()));

    auto* hl = new QHBoxLayout(voice_status_bar_);
    hl->setContentsMargins(12, 0, 10, 0);
    hl->setSpacing(6);

    // Note: voice_status_lbl_ is used for typing indicator above.
    // This bar holds stop button only.
    auto* status_lbl = new QLabel("Voice mode active");
    status_lbl->setStyleSheet(
        QString("color:%1;font-size:11px;background:transparent;").arg(col::TEXT_TERTIARY()));
    hl->addWidget(status_lbl, 1);

    stop_speech_btn_ = new QPushButton("■ Stop");
    stop_speech_btn_->setFixedHeight(20);
    stop_speech_btn_->setCursor(Qt::PointingHandCursor);
    stop_speech_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:white;border:none;"
                "font-size:10px;font-weight:700;border-radius:0px;padding:0 8px;}"
                "QPushButton:hover{background:%2;}")
            .arg(col::NEGATIVE(), col::WARNING()));
    stop_speech_btn_->hide();
    connect(stop_speech_btn_, &QPushButton::clicked, this, &AiChatBubble::on_stop_speech);
    hl->addWidget(stop_speech_btn_);

    voice_status_bar_->hide();
    return voice_status_bar_;
}

// ── Open / Close ──────────────────────────────────────────────────────────────
void AiChatBubble::on_toggle_open() {
    if (is_open_) close_panel(); else open_panel();
}

void AiChatBubble::open_panel() {
    if (is_open_) return;
    is_open_ = true;
    unread_count_ = 0;
    unread_badge_->hide();
    ensure_session();
    reposition();
    chat_panel_->show();
    chat_panel_->raise();
    raise();
    input_box_->setFocus();
}

void AiChatBubble::close_panel() {
    is_open_ = false;
    chat_panel_->hide();
}

// ── Session — always start fresh on first open ────────────────────────────────
void AiChatBubble::ensure_session() {
    // Always create a new session if none active — never load existing chat history
    if (!active_session_id_.isEmpty()) return;
    on_new_session();
}

void AiChatBubble::on_new_session() {
    // Clear message UI
    while (msg_layout_->count() > 1) {
        auto* item = msg_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    history_.clear();
    streaming_bubble_.clear();
    active_session_id_.clear();

    // Show a brief welcome hint
    const QString title = QString("Quick Chat %1")
        .arg(QDateTime::currentDateTime().toString("hh:mm"));
    auto res = ChatRepository::instance().create_session(
        title,
        ai_chat::LlmService::instance().active_provider(),
        ai_chat::LlmService::instance().active_model());

    if (res.is_ok()) {
        active_session_id_ = res.value().id;
        // Show a placeholder hint
        auto* hint = new QLabel("How can I help you?");
        hint->setAlignment(Qt::AlignCenter);
        hint->setStyleSheet(
            QString("color:%1;font-size:13px;font-style:italic;background:transparent;")
                .arg(col::TEXT_TERTIARY()));
        msg_layout_->insertWidget(msg_layout_->count() - 1, hint);
    } else {
        // Session creation failed — show error and block input so the user
        // isn't silently sending messages with no persistence.
        auto* err = new QLabel("Session error — please restart.");
        err->setAlignment(Qt::AlignCenter);
        err->setStyleSheet("color:#ef4444;font-size:12px;background:transparent;");
        msg_layout_->insertWidget(msg_layout_->count() - 1, err);
        if (input_box_) input_box_->setEnabled(false);
        LOG_ERROR("AiChatBubble", "Failed to create chat session: " + QString::fromStdString(res.error()));
    }

    if (voice_status_lbl_) voice_status_lbl_->clear();
}

// ── Send ──────────────────────────────────────────────────────────────────────
void AiChatBubble::on_send() {
    const QString text = input_box_->toPlainText().trimmed();
    if (text.isEmpty() || streaming_) return;

    // Fail fast if LLM is not configured — show inline error instead of a
    // confusing delayed "No provider" message after the network round-trip.
    if (!ai_chat::LlmService::instance().is_configured()) {
        add_bubble("assistant",
            "AI chat is not configured. Please go to **Settings → LLM Config** "
            "and add an API key or select the Fincept provider.");
        return;
    }

    input_box_->clear();
    ensure_session();

    // Remove the welcome hint if present (first message)
    if (msg_layout_->count() == 2) { // stretch + hint
        auto* item = msg_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    add_bubble("user", text);
    history_.push_back({"user", text});
    ChatRepository::instance().add_message(active_session_id_, "user", text);

    streaming_ = true;
    set_ui_enabled(false);

    // Show typing indicator
    if (voice_status_lbl_) voice_status_lbl_->setText("AI is thinking…");

    // Defer streaming bubble creation to first non-empty chunk.
    // Use shared_ptr<bool> so the single flag is shared between outer (bg thread)
    // and inner (UI thread) lambdas — no mutable bool copies across thread boundary.
    QPointer<AiChatBubble> self = this;
    auto first_chunk = std::make_shared<bool>(true);
    // Pass use_tools=false — the floating bubble should never trigger
    // navigation or other tool side-effects that would hijack the user's
    // current screen.
    ai_chat::LlmService::instance().chat_streaming(
        text, history_,
        [self, first_chunk](const QString& chunk, bool done) {
            QMetaObject::invokeMethod(qApp, [self, chunk, done, first_chunk]() {
                if (!self) return;
                if (*first_chunk && !chunk.isEmpty()) {
                    *first_chunk = false;
                    if (self->voice_status_lbl_) self->voice_status_lbl_->clear();
                    self->streaming_bubble_ = self->add_streaming_bubble();
                }
                self->on_stream_chunk(chunk, done);
            }, Qt::QueuedConnection);
        },
        /*use_tools=*/ false);
}

void AiChatBubble::on_stream_chunk(const QString& chunk, bool done) {
    // Snapshot QPointer to local — prevents TOCTOU between null-check and use.
    QTextEdit* bubble = streaming_bubble_;
    if (!bubble) return;
    if (!chunk.isEmpty()) {
        bubble->moveCursor(QTextCursor::End);
        bubble->insertPlainText(chunk);
        scroll_to_bottom();
    }
    Q_UNUSED(done)
}

// ── Bubble-panel markdown CSS (compact version for the mini chat) ─────────────
static QString bubble_panel_md_css(const QString& text_color) {
    return QString(
        "body { color: %1; line-height: 1.4; }"
        "p { margin-top: 4px; margin-bottom: 4px; }"
        "ul, ol { margin-top: 3px; margin-bottom: 3px; padding-left: 16px; }"
        "li { margin-top: 2px; margin-bottom: 2px; }"
        "h1, h2, h3, h4 { margin-top: 8px; margin-bottom: 3px; color: %2; font-weight: 700; }"
        "hr { margin-top: 6px; margin-bottom: 6px; border: none; "
        "     border-top: 1px solid %3; }"
        "a { color: %2; text-decoration: underline; }"
        "code { background: %4; color: %2; padding: 1px 3px; "
        "       font-family: 'Consolas', 'Courier New', monospace; font-size: 12px; }"
        "pre { background: %4; border: 1px solid %3; padding: 8px 10px; "
        "      margin-top: 4px; margin-bottom: 4px; "
        "      font-family: 'Consolas', 'Courier New', monospace; font-size: 12px; }"
        "blockquote { border-left: 2px solid %2; padding-left: 8px; "
        "             margin-top: 4px; margin-bottom: 4px; color: %5; }"
        "table { border-collapse: collapse; margin-top: 4px; margin-bottom: 4px; }"
        "th { background: %4; border: 1px solid %3; padding: 3px 6px; "
        "     font-weight: 600; color: %2; }"
        "td { border: 1px solid %3; padding: 3px 6px; }"
        "strong { color: %1; font-weight: 700; }"
    ).arg(text_color, col::AMBER(), col::BORDER_MED(), col::BG_BASE(), col::TEXT_SECONDARY());
}

void AiChatBubble::on_streaming_done(ai_chat::LlmResponse response) {
    streaming_ = false;
    if (voice_status_lbl_) voice_status_lbl_->clear();
    set_ui_enabled(true);

    const QString content = response.success
        ? response.content
        : QString("Error: %1").arg(response.error);

    // Non-streaming path: create bubble now
    if (!streaming_bubble_ && !content.isEmpty()) {
        streaming_bubble_ = add_streaming_bubble();
        streaming_bubble_->setPlainText(content);
    }

    if (streaming_bubble_) {
        // Convert streamed plain text to rendered markdown
        const QString final_text = streaming_bubble_->toPlainText();
        if (!final_text.isEmpty()) {
            streaming_bubble_->document()->setDefaultStyleSheet(
                bubble_panel_md_css(col::TEXT_PRIMARY()));
            streaming_bubble_->setMarkdown(final_text);
        }
        streaming_bubble_->setReadOnly(true);
        streaming_bubble_ = nullptr;
    }

    history_.push_back({"assistant", content});
    ChatRepository::instance().add_message(
        active_session_id_, "assistant", content,
        ai_chat::LlmService::instance().active_provider(),
        ai_chat::LlmService::instance().active_model(),
        response.total_tokens);

    if (!is_open_) update_unread(1);

    if (voice_mode_ && response.success)
        speak_text(content);
    else if (voice_mode_)
        start_listening();
}

// ── Message bubbles ───────────────────────────────────────────────────────────
void AiChatBubble::add_bubble(const QString& role, const QString& text) {
    const bool is_user = (role == "user");

    auto* row = new QWidget;
    row->setStyleSheet("background:transparent;");
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(0);

    if (is_user) rl->addStretch();

    auto* col_w = new QWidget;
    col_w->setStyleSheet("background:transparent;");
    col_w->setMaximumWidth(static_cast<int>(PANEL_W * 0.82));
    auto* cvl = new QVBoxLayout(col_w);
    cvl->setContentsMargins(0, 0, 0, 0);
    cvl->setSpacing(3);

    // Role micro-label
    auto* role_lbl = new QLabel(is_user ? "You" : "AI");
    role_lbl->setAlignment(is_user ? Qt::AlignRight : Qt::AlignLeft);
    role_lbl->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;background:transparent;")
            .arg(is_user ? col::AMBER() : col::AMBER()));
    cvl->addWidget(role_lbl);

    // Bubble — use QTextEdit for consistent markdown rendering
    auto* bubble_frame = new QWidget;
    bubble_frame->setStyleSheet(
        QString("background:%1;border:1px solid %2;border-radius:0px;padding:0px;")
            .arg(is_user ? "rgba(120,53,15,0.45)" : col::BG_SURFACE(),
                 is_user ? "rgba(217,119,6,0.28)" : col::BORDER_DIM()));

    auto* bvl = new QVBoxLayout(bubble_frame);
    bvl->setContentsMargins(8, 6, 8, 6);
    bvl->setSpacing(0);

    auto* body = new QTextEdit;
    body->setReadOnly(true);
    body->setFrameShape(QFrame::NoFrame);
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    body->document()->setDocumentMargin(2);
    body->document()->setDefaultStyleSheet(
        bubble_panel_md_css(is_user ? "#fff7ed" : col::TEXT_PRIMARY()));
    apply_obsidian_palette(body);
    body->setMarkdown(text);
    body->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->setStyleSheet(
        QString("QTextEdit{background:transparent;color:%1;border:none;font-size:13px;}")
            .arg(is_user ? "#fff7ed" : col::TEXT_PRIMARY()));
    // Size to content
    body->document()->setTextWidth(static_cast<int>(PANEL_W * 0.82) - 20);
    const int doc_h = static_cast<int>(body->document()->size().height());
    body->setFixedHeight(qMax(doc_h + 8, 24));
    bvl->addWidget(body);
    cvl->addWidget(bubble_frame);

    rl->addWidget(col_w);
    if (!is_user) rl->addStretch();

    msg_layout_->insertWidget(msg_layout_->count() - 1, row);
    scroll_to_bottom();
}

QTextEdit* AiChatBubble::add_streaming_bubble() {
    auto* row = new QWidget;
    row->setStyleSheet("background:transparent;");
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);

    auto* col_w = new QWidget;
    col_w->setStyleSheet("background:transparent;");
    col_w->setMaximumWidth(static_cast<int>(PANEL_W * 0.82));
    auto* cvl = new QVBoxLayout(col_w);
    cvl->setContentsMargins(0, 0, 0, 0);
    cvl->setSpacing(3);

    auto* role_lbl = new QLabel("AI");
    role_lbl->setAlignment(Qt::AlignLeft);
    role_lbl->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;background:transparent;")
            .arg(col::AMBER()));
    cvl->addWidget(role_lbl);

    auto* body = new QTextEdit;
    body->setReadOnly(false);
    body->setFrameShape(QFrame::NoFrame);
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    body->document()->setDocumentMargin(4);
    body->document()->setDefaultStyleSheet(
        bubble_panel_md_css(col::TEXT_PRIMARY()));
    apply_obsidian_palette(body);
    body->setFixedHeight(32);
    body->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    body->setStyleSheet(
        QString("QTextEdit{background:%1;color:%2;border:1px solid %3;"
                "border-radius:0px;padding:8px 12px;font-size:13px;}")
            .arg(col::BG_SURFACE(), col::TEXT_PRIMARY(), col::BORDER_DIM()));

    // Dynamic height as content streams in
    connect(body->document(), &QTextDocument::contentsChanged, body,
            [body, col_w, row]() {
                body->document()->setTextWidth(
                    body->viewport()->width() > 0 ? body->viewport()->width() : 300);
                const int h = qMax(static_cast<int>(body->document()->size().height()) + 8, 32);
                body->setFixedHeight(h);
                col_w->adjustSize();
                row->adjustSize();
            });

    cvl->addWidget(body);
    rl->addWidget(col_w);
    rl->addStretch();

    msg_layout_->insertWidget(msg_layout_->count() - 1, row);
    scroll_to_bottom();
    return body;
}

void AiChatBubble::scroll_to_bottom() {
    QTimer::singleShot(30, this, [this]() {
        if (scroll_area_ && scroll_area_->verticalScrollBar())
            scroll_area_->verticalScrollBar()->setValue(
                scroll_area_->verticalScrollBar()->maximum());
    });
}

void AiChatBubble::set_ui_enabled(bool enabled) {
    send_btn_->setEnabled(enabled);
    mic_btn_->setEnabled(enabled);
    input_box_->setEnabled(enabled);
    send_btn_->setText(enabled ? "↑" : "…");
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

// ── Voice input (STT — whisper.cpp offline, cross-platform) ──────────────────

void AiChatBubble::on_toggle_mic() {
    if (is_listening_) stop_listening();
    else               start_listening();
}

void AiChatBubble::start_listening() {
    if (is_listening_ || is_speaking_) return;
    services::WhisperService::instance().start_listening();
    // is_listening_ state is updated via on_stt_listening_changed signal.
}

void AiChatBubble::stop_listening() {
    services::WhisperService::instance().stop_listening();
    // is_listening_ state is updated via on_stt_listening_changed signal.
}

void AiChatBubble::on_stt_listening_changed(bool active) {
    is_listening_ = active;
    mic_btn_->setChecked(active);
    if (active) {
        pulse_timer_->start();
    } else {
        pulse_timer_->stop();
        mic_btn_->setChecked(false);
    }
    update_voice_status();
}

void AiChatBubble::on_transcription(const QString& text) {
    if (text.isEmpty()) return;

    input_box_->setPlainText(text);
    if (voice_mode_) on_send();
    update_voice_status();
}

void AiChatBubble::on_stt_error(const QString& message) {
    if (voice_status_lbl_)
        voice_status_lbl_->setText(QStringLiteral("⚠ ") + message);
}

void AiChatBubble::on_model_download_progress(int percent) {
    if (voice_status_lbl_)
        voice_status_lbl_->setText(
            QStringLiteral("Downloading voice model… %1%").arg(percent));
}

// ── Voice mode ────────────────────────────────────────────────────────────────
void AiChatBubble::on_toggle_voice_mode() {
    voice_mode_ = voice_mode_btn_->isChecked();
    voice_status_bar_->setVisible(voice_mode_);
    if (voice_mode_) {
        if (!is_open_) open_panel();

        // Warn once if TTS is unavailable — voice input still works,
        // but AI responses will not be spoken aloud.
#ifndef HAS_QT_TTS
        if (voice_status_lbl_)
            voice_status_lbl_->setText(
                "⚠ Voice responses unavailable — Qt TextToSpeech not installed. "
                "Input-only mode active.");
#else
        // Check at runtime whether any TTS engine is available
        if (tts_engine_ && tts_engine_->availableEngines().isEmpty()) {
            if (voice_status_lbl_)
                voice_status_lbl_->setText(
                    "⚠ No TTS engine found (install speech-dispatcher on Linux). "
                    "Input-only mode active.");
        }
#endif
        QTimer::singleShot(300, this, &AiChatBubble::start_listening);
    } else {
        stop_listening();
        stop_tts();
    }
    update_voice_status();
}

// ── TTS ───────────────────────────────────────────────────────────────────────
void AiChatBubble::speak_text(const QString& text) {
    if (text.isEmpty()) return;

    // Strip markdown formatting — TTS engines speak raw text.
    QString clean = text;
    clean.remove(QRegularExpression(R"(\*\*|__|~~|```[^`]*```|`[^`]*`)"));
    clean.remove(QRegularExpression(R"(#{1,6} )"));
    clean = clean.trimmed();

    if (clean.isEmpty()) {
        if (voice_mode_) start_listening();
        return;
    }

    is_speaking_ = true;
    update_voice_status();
    stop_speech_btn_->show();

    // TTS via Qt's cross-platform QTextToSpeech (Qt 6.4+).
    // Instantiated lazily — avoids cost when voice mode is never used.
    // QTextToSpeech uses platform engines: SAPI on Windows, AVSpeech on macOS,
    // speech-dispatcher on Linux.  No subprocess, no PowerShell.
#ifdef HAS_QT_TTS
    if (!tts_engine_) {
        tts_engine_ = new QTextToSpeech(this);
        tts_engine_->setRate(0.15);
        connect(tts_engine_, &QTextToSpeech::stateChanged,
                this, [this](QTextToSpeech::State state) {
                    if (state == QTextToSpeech::Ready ||
                        state == QTextToSpeech::Error) {
                        is_speaking_ = false;
                        stop_speech_btn_->hide();
                        update_voice_status();
                        if (voice_mode_ && !is_listening_ && !streaming_)
                            QTimer::singleShot(400, this, &AiChatBubble::start_listening);
                    }
                });
    }
    tts_engine_->say(clean.left(1200));
#else
    // Qt TextToSpeech module not built — skip TTS silently, resume listening.
    // User was already warned when they enabled voice mode.
    is_speaking_ = false;
    stop_speech_btn_->hide();
    if (voice_status_lbl_)
        voice_status_lbl_->setText("Voice response skipped (TTS unavailable)");
    update_voice_status();
    if (voice_mode_ && !is_listening_ && !streaming_)
        QTimer::singleShot(400, this, &AiChatBubble::start_listening);
#endif
}

void AiChatBubble::stop_tts() {
    is_speaking_ = false;
    stop_speech_btn_->hide();
    tts_player_->stop();
#ifdef HAS_QT_TTS
    if (tts_engine_) tts_engine_->stop();
#endif
    update_voice_status();
}

void AiChatBubble::on_stop_speech() {
    stop_tts();
    if (voice_mode_ && !is_listening_ && !streaming_)
        QTimer::singleShot(200, this, &AiChatBubble::start_listening);
}

void AiChatBubble::on_tts_media_status(QMediaPlayer::MediaStatus status) {
    if (status == QMediaPlayer::EndOfMedia || status == QMediaPlayer::InvalidMedia) {
        is_speaking_ = false;
        stop_speech_btn_->hide();
        update_voice_status();
        if (voice_mode_)
            QTimer::singleShot(400, this, &AiChatBubble::start_listening);
    }
}

// ── Voice status ──────────────────────────────────────────────────────────────
void AiChatBubble::update_voice_status() {
    if (!voice_mode_) return;
    if (!voice_status_lbl_) return;

    if (is_speaking_) {
        voice_status_lbl_->setText("▶ AI speaking…");
        voice_status_lbl_->setStyleSheet(
            QString("color:%1;font-size:11px;font-style:italic;background:transparent;")
                .arg(col::WARNING()));
    } else if (is_listening_) {
        voice_status_lbl_->setText("● Listening…");
        voice_status_lbl_->setStyleSheet(
            QString("color:%1;font-size:11px;font-style:italic;background:transparent;")
                .arg(col::POSITIVE()));
    } else if (streaming_) {
        voice_status_lbl_->setText("AI is thinking…");
        voice_status_lbl_->setStyleSheet(
            QString("color:%1;font-size:11px;font-style:italic;background:transparent;")
                .arg(col::TEXT_DIM()));
    }
}

// ── Pulse animation ───────────────────────────────────────────────────────────
void AiChatBubble::animate_pulse() {
    pulse_step_ = (pulse_step_ + 1) % 2;
    mic_btn_->setChecked(pulse_step_ != 0);
}

// ── Event filter ──────────────────────────────────────────────────────────────
bool AiChatBubble::eventFilter(QObject* obj, QEvent* e) {
    if (obj == bubble_btn_ && e->type() == QEvent::MouseButtonRelease) {
        on_toggle_open();
        return true;
    }
    if (obj == input_box_ && e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Return && !(ke->modifiers() & Qt::ShiftModifier)) {
            on_send();
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
