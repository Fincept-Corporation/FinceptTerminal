#include "screens/chat_mode/ChatMessagePanel.h"
#include "screens/chat_mode/ChatModeService.h"
#include "ui/markdown/MarkdownRenderer.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QScrollBar>
#include <QSizePolicy>
#include <QTimer>

namespace fincept::chat_mode {

// ── Stylesheet constants ──────────────────────────────────────────────────────

static const char* USER_BUBBLE_SS =
    "background:#1a1a0a;color:#e5e5e5;border:1px solid #2a2a1a;"
    "border-radius:8px;padding:10px 12px;font-size:13px;"
    "font-family:'Consolas',monospace;";

static const char* AI_BUBBLE_SS =
    "background:#0a0a14;color:#e5e5e5;border:1px solid #1a1a2a;"
    "border-radius:8px;padding:10px 12px;font-size:13px;"
    "font-family:'Consolas',monospace;";

static const char* ERROR_BUBBLE_SS =
    "background:#140a0a;color:#ff6b6b;border:1px solid #2a1a1a;"
    "border-radius:8px;padding:10px 12px;font-size:13px;"
    "font-family:'Consolas',monospace;";

static const char* TOOL_BADGE_SS =
    "background:#111120;color:#6b6bff;border:1px solid #1a1a30;"
    "border-radius:4px;padding:3px 8px;font-size:11px;"
    "font-family:'Consolas',monospace;";

// ── Constructor ───────────────────────────────────────────────────────────────

ChatMessagePanel::ChatMessagePanel(QWidget* parent) : QWidget(parent) {
    build_ui();
}

// ── Build UI ──────────────────────────────────────────────────────────────────

void ChatMessagePanel::build_ui() {
    setStyleSheet("background:#080808;");
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_header());
    vl->addWidget(build_messages_area(), 1);
    vl->addWidget(build_typing_indicator());
    vl->addWidget(build_input_area());
}

QWidget* ChatMessagePanel::build_header() {
    auto* hdr = new QWidget;
    hdr->setFixedHeight(40);
    hdr->setStyleSheet("background:#0a0a0a;border-bottom:1px solid #1a1a1a;");
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(8);

    hdr_title_lbl_ = new QLabel("New Conversation");
    hdr_title_lbl_->setStyleSheet(
        "color:#e5e5e5;font-size:13px;font-weight:700;"
        "font-family:'Consolas',monospace;background:transparent;");
    hl->addWidget(hdr_title_lbl_);

    hl->addStretch();

    // Mode toggle button
    mode_btn_ = new QPushButton("LITE");
    mode_btn_->setFixedHeight(22);
    mode_btn_->setToolTip("Toggle between Lite (fast) and Deep (multi-step) mode");
    mode_btn_->setStyleSheet(
        "QPushButton{background:#111111;color:#d97706;border:1px solid #2a2a1a;"
        "border-radius:3px;font-size:10px;font-weight:700;padding:0 8px;"
        "font-family:'Consolas',monospace;}"
        "QPushButton:hover{background:#1a1a0a;border-color:#d97706;}");
    connect(mode_btn_, &QPushButton::clicked, this, [this]() {
        current_mode_ = (current_mode_ == StreamMode::Lite) ? StreamMode::Deep : StreamMode::Lite;
        mode_btn_->setText(current_mode_ == StreamMode::Lite ? "LITE" : "DEEP");
        emit mode_toggled(current_mode_);
    });
    hl->addWidget(mode_btn_);

    hdr_tokens_lbl_ = new QLabel("0 tokens");
    hdr_tokens_lbl_->setStyleSheet(
        "color:#404040;font-size:11px;font-family:'Consolas',monospace;"
        "background:transparent;");
    hl->addWidget(hdr_tokens_lbl_);

    return hdr;
}

QWidget* ChatMessagePanel::build_messages_area() {
    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area_->setStyleSheet(
        "QScrollArea{background:#080808;border:none;}"
        "QScrollBar:vertical{background:#080808;width:4px;}"
        "QScrollBar::handle:vertical{background:#2a2a2a;border-radius:2px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    messages_container_ = new QWidget;
    messages_container_->setStyleSheet("background:#080808;");
    messages_layout_ = new QVBoxLayout(messages_container_);
    messages_layout_->setContentsMargins(16, 12, 16, 12);
    messages_layout_->setSpacing(12);
    messages_layout_->addStretch(1);

    welcome_panel_ = build_welcome();
    messages_layout_->insertWidget(0, welcome_panel_);

    scroll_area_->setWidget(messages_container_);
    return scroll_area_;
}

QWidget* ChatMessagePanel::build_welcome() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(32, 40, 32, 40);
    vl->setSpacing(16);
    vl->setAlignment(Qt::AlignCenter);

    auto* logo = new QLabel("◈ FINCEPT AGENT");
    logo->setAlignment(Qt::AlignCenter);
    logo->setStyleSheet(
        "color:#d97706;font-size:22px;font-weight:700;"
        "font-family:'Consolas',monospace;background:transparent;");
    vl->addWidget(logo);

    auto* sub = new QLabel(
        "Your AI-powered financial intelligence assistant.\n"
        "Ask about markets, analyze equities, run portfolio analysis,\n"
        "or get macro economic insights.");
    sub->setAlignment(Qt::AlignCenter);
    sub->setWordWrap(true);
    sub->setStyleSheet(
        "color:#606060;font-size:13px;font-family:'Consolas',monospace;"
        "background:transparent;line-height:1.6;");
    vl->addWidget(sub);

    // Quick suggestion chips
    const QStringList suggestions = {
        "What's the outlook for AAPL?",
        "Summarize today's market news",
        "Analyze my portfolio risk",
        "What are the key economic indicators this week?"
    };

    auto* chips_row = new QWidget;
    auto* chips_layout = new QHBoxLayout(chips_row);
    chips_layout->setContentsMargins(0, 8, 0, 0);
    chips_layout->setSpacing(8);
    chips_layout->setAlignment(Qt::AlignCenter);

    for (const auto& s : suggestions) {
        auto* chip = new QPushButton(s);
        chip->setStyleSheet(
            "QPushButton{background:#111111;color:#808080;border:1px solid #2a2a2a;"
            "border-radius:14px;padding:6px 14px;font-size:11px;"
            "font-family:'Consolas',monospace;}"
            "QPushButton:hover{background:#1a1a1a;color:#e5e5e5;border-color:#d97706;}");
        connect(chip, &QPushButton::clicked, this, [this, s]() {
            input_box_->setPlainText(s);
            on_send_clicked();
        });
        chips_layout->addWidget(chip);
    }
    vl->addWidget(chips_row);

    return w;
}

QWidget* ChatMessagePanel::build_typing_indicator() {
    typing_indicator_ = new QWidget;
    typing_indicator_->setFixedHeight(28);
    typing_indicator_->setStyleSheet("background:#080808;");
    typing_indicator_->setVisible(false);

    auto* hl = new QHBoxLayout(typing_indicator_);
    hl->setContentsMargins(16, 4, 16, 4);
    hl->setSpacing(6);

    auto* agent_lbl = new QLabel("Agent");
    agent_lbl->setStyleSheet(
        "color:#d97706;font-size:11px;font-weight:700;"
        "font-family:'Consolas',monospace;background:transparent;");
    hl->addWidget(agent_lbl);

    typing_dots_lbl_ = new QLabel("●");
    typing_dots_lbl_->setStyleSheet(
        "color:#d97706;font-size:11px;background:transparent;");
    hl->addWidget(typing_dots_lbl_);
    hl->addStretch();

    typing_timer_ = new QTimer(this);
    typing_timer_->setInterval(400);
    connect(typing_timer_, &QTimer::timeout, this, &ChatMessagePanel::on_typing_tick);

    return typing_indicator_;
}

QWidget* ChatMessagePanel::build_input_area() {
    auto* container = new QWidget;
    container->setStyleSheet("background:#0a0a0a;border-top:1px solid #1a1a1a;");
    auto* vl = new QVBoxLayout(container);
    vl->setContentsMargins(16, 8, 16, 12);
    vl->setSpacing(6);

    // Input box
    input_box_ = new QPlainTextEdit;
    input_box_->setPlaceholderText("Ask Fincept Agent anything… (Enter to send, Shift+Enter for new line)");
    input_box_->setMaximumHeight(120);
    input_box_->setMinimumHeight(44);
    input_box_->setStyleSheet(
        "QPlainTextEdit{background:#111111;color:#e5e5e5;border:1px solid #2a2a2a;"
        "border-radius:6px;padding:8px 12px;font-size:13px;"
        "font-family:'Consolas',monospace;}"
        "QPlainTextEdit:focus{border:1px solid #d97706;}"
        "QScrollBar:vertical{background:#111111;width:4px;}"
        "QScrollBar::handle:vertical{background:#2a2a2a;border-radius:2px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");
    input_box_->installEventFilter(this);
    vl->addWidget(input_box_);

    // Bottom row: char count + buttons
    auto* bottom_row = new QHBoxLayout;
    bottom_row->setSpacing(6);

    char_lbl_ = new QLabel("0 / 4000");
    char_lbl_->setStyleSheet(
        "color:#404040;font-size:11px;font-family:'Consolas',monospace;"
        "background:transparent;");
    bottom_row->addWidget(char_lbl_);
    bottom_row->addStretch();

    connect(input_box_, &QPlainTextEdit::textChanged, this, [this]() {
        const int n = input_box_->toPlainText().length();
        char_lbl_->setText(QString("%1 / 4000").arg(n));
    });

    stop_btn_ = new QPushButton("■ Stop");
    stop_btn_->setFixedHeight(30);
    stop_btn_->setVisible(false);
    stop_btn_->setStyleSheet(
        "QPushButton{background:#2a0a0a;color:#ff6b6b;border:1px solid #3a1a1a;"
        "border-radius:4px;font-size:12px;padding:0 14px;"
        "font-family:'Consolas',monospace;}"
        "QPushButton:hover{background:#3a1a1a;}");
    connect(stop_btn_, &QPushButton::clicked, this, []() {
        ChatModeService::instance().abort_stream(); // forward declaration needed — see note
    });
    bottom_row->addWidget(stop_btn_);

    send_btn_ = new QPushButton("Send  ↵");
    send_btn_->setFixedHeight(30);
    send_btn_->setStyleSheet(
        "QPushButton{background:#d97706;color:#000000;border:none;"
        "border-radius:4px;font-size:12px;font-weight:700;padding:0 18px;"
        "font-family:'Consolas',monospace;}"
        "QPushButton:hover{background:#f59e0b;}"
        "QPushButton:disabled{background:#2a2a1a;color:#606060;}");
    connect(send_btn_, &QPushButton::clicked, this, &ChatMessagePanel::on_send_clicked);
    bottom_row->addWidget(send_btn_);

    vl->addLayout(bottom_row);
    return container;
}

// ── Public API ────────────────────────────────────────────────────────────────

void ChatMessagePanel::load_messages(const QVector<ChatMessage>& messages) {
    clear_messages();
    if (messages.isEmpty()) {
        show_welcome(true);
        return;
    }
    show_welcome(false);
    for (const auto& m : messages)
        add_message_bubble(m.role, m.content, m.created_at);
    scroll_to_bottom();
}

void ChatMessagePanel::clear_messages() {
    // Remove all bubble widgets (leave stretch + welcome panel)
    QLayoutItem* item = nullptr;
    while ((item = messages_layout_->takeAt(messages_layout_->count() - 1))) {
        if (item->widget() && item->widget() != welcome_panel_)
            item->widget()->deleteLater();
        delete item;
    }
    // Re-add stretch + welcome
    messages_layout_->addStretch(1);
    show_welcome(true);
    total_tokens_ = 0;
    update_tokens_label();
}

void ChatMessagePanel::set_session_title(const QString& title) {
    hdr_title_lbl_->setText(title.isEmpty() ? "New Conversation" : title);
}

void ChatMessagePanel::set_stream_mode(StreamMode mode) {
    current_mode_ = mode;
    mode_btn_->setText(mode == StreamMode::Lite ? "LITE" : "DEEP");
}

// ── Message bubbles ───────────────────────────────────────────────────────────

void ChatMessagePanel::add_message_bubble(const QString& role, const QString& content,
                                          const QString& timestamp)
{
    show_welcome(false);

    auto* bubble = new QTextEdit;
    bubble->setReadOnly(true);
    if (role == "user") {
        bubble->setPlainText(content);
    } else {
        bubble->setHtml(ui::MarkdownRenderer::render(content));
    }
    bubble->setStyleSheet(role == "user" ? USER_BUBBLE_SS : AI_BUBBLE_SS);
    bubble->setFrameShape(QFrame::NoFrame);
    bubble->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    bubble->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    bubble->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Auto-resize to content
    auto resize_to_content = [bubble]() {
        QTextDocument* doc = bubble->document();
        doc->setTextWidth(bubble->viewport()->width());
        const int h = static_cast<int>(doc->size().height()) + 4;
        bubble->setFixedHeight(qBound(40, h, 600));
    };
    resize_to_content();
    connect(bubble, &QTextEdit::textChanged, this, resize_to_content);

    // Role label row
    auto* row = new QWidget;
    auto* row_vl = new QVBoxLayout(row);
    row_vl->setContentsMargins(0, 0, 0, 0);
    row_vl->setSpacing(3);

    // Role + timestamp header
    auto* meta_row = new QHBoxLayout;
    meta_row->setSpacing(6);
    auto* role_lbl = new QLabel(role == "user" ? "You" : "Agent");
    role_lbl->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;font-family:'Consolas',monospace;"
                "background:transparent;")
            .arg(role == "user" ? "#d97706" : "#6b6bff"));
    meta_row->addWidget(role_lbl);
    if (!timestamp.isEmpty()) {
        auto* ts_lbl = new QLabel(timestamp.left(16));
        ts_lbl->setStyleSheet(
            "color:#404040;font-size:10px;font-family:'Consolas',monospace;"
            "background:transparent;");
        meta_row->addWidget(ts_lbl);
    }
    meta_row->addStretch();
    row_vl->addLayout(meta_row);
    row_vl->addWidget(bubble);

    // Insert before the stretch (last item)
    const int insert_pos = messages_layout_->count() - 1;
    messages_layout_->insertWidget(insert_pos, row);

    QTimer::singleShot(0, this, [this]() { scroll_to_bottom(); });
}

QTextEdit* ChatMessagePanel::add_streaming_bubble() {
    show_welcome(false);

    auto* bubble = new QTextEdit;
    bubble->setReadOnly(true);
    bubble->setStyleSheet(AI_BUBBLE_SS);
    bubble->setFrameShape(QFrame::NoFrame);
    bubble->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    bubble->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    bubble->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    bubble->setFixedHeight(44);

    // Role label row
    auto* row = new QWidget;
    auto* row_vl = new QVBoxLayout(row);
    row_vl->setContentsMargins(0, 0, 0, 0);
    row_vl->setSpacing(3);

    auto* role_lbl = new QLabel("Agent");
    role_lbl->setStyleSheet(
        "color:#6b6bff;font-size:10px;font-weight:700;font-family:'Consolas',monospace;"
        "background:transparent;");
    row_vl->addWidget(role_lbl);
    row_vl->addWidget(bubble);

    const int insert_pos = messages_layout_->count() - 1;
    messages_layout_->insertWidget(insert_pos, row);

    return bubble;
}

void ChatMessagePanel::append_tool_badge(const QString& tool_name, int duration_ms) {
    auto* badge = new QLabel(QString("⚙ %1  %2ms").arg(tool_name).arg(duration_ms));
    badge->setStyleSheet(TOOL_BADGE_SS);
    badge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    const int insert_pos = messages_layout_->count() - 1;
    messages_layout_->insertWidget(insert_pos, badge, 0, Qt::AlignLeft);
}

// ── SSE slots ─────────────────────────────────────────────────────────────────

void ChatMessagePanel::on_stream_session_meta(const QString& /*session_id*/,
                                               const QString& new_title)
{
    if (!new_title.isEmpty())
        set_session_title(new_title);
}

void ChatMessagePanel::on_stream_text_delta(const QString& text) {
    if (!streaming_bubble_) return;

    // Accumulate raw markdown in buffer
    streaming_buffer_ += text;

    // Show plain text while streaming (fast, no HTML parse overhead per chunk)
    streaming_bubble_->setPlainText(streaming_buffer_);

    // Resize bubble
    QTextDocument* doc = streaming_bubble_->document();
    doc->setTextWidth(streaming_bubble_->viewport()->width());
    const int h = static_cast<int>(doc->size().height()) + 4;
    streaming_bubble_->setFixedHeight(qBound(40, h, 600));

    scroll_to_bottom();
}

void ChatMessagePanel::on_stream_tool_end(const QString& tool_name, int duration_ms) {
    append_tool_badge(tool_name, duration_ms);
}

void ChatMessagePanel::on_stream_finish(int total_tokens) {
    total_tokens_ += total_tokens;
    update_tokens_label();

    // Re-render the complete streamed text as proper markdown HTML
    if (streaming_bubble_ && !streaming_buffer_.isEmpty()) {
        streaming_bubble_->setHtml(ui::MarkdownRenderer::render(streaming_buffer_));
        // Resize to final HTML height
        QTextDocument* doc = streaming_bubble_->document();
        doc->setTextWidth(streaming_bubble_->viewport()->width());
        const int h = static_cast<int>(doc->size().height()) + 4;
        streaming_bubble_->setFixedHeight(qBound(40, h, 600));
    }

    streaming_        = false;
    streaming_bubble_ = nullptr;
    streaming_buffer_.clear();
    show_typing(false);
    set_input_enabled(true);
    send_btn_->setVisible(true);
    stop_btn_->setVisible(false);
    scroll_to_bottom();
}

void ChatMessagePanel::on_stream_error(const QString& message) {
    streaming_        = false;
    streaming_bubble_ = nullptr;
    streaming_buffer_.clear();
    show_typing(false);
    set_input_enabled(true);
    send_btn_->setVisible(true);
    stop_btn_->setVisible(false);

    // Show error bubble
    auto* err = new QLabel("⚠ " + message);
    err->setWordWrap(true);
    err->setStyleSheet(ERROR_BUBBLE_SS);
    const int insert_pos = messages_layout_->count() - 1;
    messages_layout_->insertWidget(insert_pos, err);
    scroll_to_bottom();
}

void ChatMessagePanel::on_stream_heartbeat() {
    // Keep-alive — nothing to do
}

void ChatMessagePanel::on_insufficient_credits() {
    on_stream_error("Insufficient credits. Please top up your account to continue.");
}

// ── Send ──────────────────────────────────────────────────────────────────────

void ChatMessagePanel::on_send_clicked() {
    const QString text = input_box_->toPlainText().trimmed();
    if (text.isEmpty() || streaming_) return;

    input_box_->clear();
    streaming_        = true;
    streaming_buffer_.clear();

    // Show user bubble immediately
    add_message_bubble("user", text);

    // Prepare streaming bubble + UI state
    show_typing(true);
    set_input_enabled(false);
    send_btn_->setVisible(false);
    stop_btn_->setVisible(true);

    streaming_bubble_ = add_streaming_bubble();

    emit send_requested(text, current_mode_);
}

// ── Input event filter (Enter to send) ───────────────────────────────────────

bool ChatMessagePanel::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == input_box_ && ev->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(ev);
        if (ke->key() == Qt::Key_Return && !(ke->modifiers() & Qt::ShiftModifier)) {
            on_send_clicked();
            return true;
        }
    }
    return QWidget::eventFilter(obj, ev);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void ChatMessagePanel::scroll_to_bottom() {
    QScrollBar* sb = scroll_area_->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void ChatMessagePanel::show_welcome(bool show) {
    if (welcome_panel_)
        welcome_panel_->setVisible(show);
}

void ChatMessagePanel::show_typing(bool show) {
    typing_indicator_->setVisible(show);
    if (show)
        typing_timer_->start();
    else
        typing_timer_->stop();
}

void ChatMessagePanel::set_input_enabled(bool enabled) {
    input_box_->setEnabled(enabled);
}

void ChatMessagePanel::update_tokens_label() {
    hdr_tokens_lbl_->setText(QString("%1 tokens").arg(total_tokens_));
}

void ChatMessagePanel::on_typing_tick() {
    static const QVector<QString> DOTS = {"●", "● ●", "● ● ●"};
    typing_dots_lbl_->setText(DOTS[typing_step_ % 3]);
    ++typing_step_;
}

} // namespace fincept::chat_mode
