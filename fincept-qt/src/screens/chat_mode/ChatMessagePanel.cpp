#include "screens/chat_mode/ChatMessagePanel.h"

#include "screens/chat_mode/ChatModeService.h"
#include "ui/markdown/MarkdownRenderer.h"
#include "ui/theme/Theme.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLocale>
#include <QPalette>
#include <QScrollBar>
#include <QSizePolicy>
#include <QTextCursor>

#include <algorithm>

namespace fincept::chat_mode {

static constexpr const char* FONT = "'Consolas','Courier New',monospace";

static QString user_bubble_ss() {
    return QString("background:rgba(120,53,15,0.35);color:%1;border:1px solid rgba(217,119,6,0.2);"
                   "border-radius:0px;padding:8px 12px;font-size:14px;"
                   "font-family:'Consolas','Courier New',monospace;")
        .arg(ui::colors::TEXT_PRIMARY());
}

static QString ai_bubble_ss() {
    return QString("background:%1;color:%2;border:1px solid %3;"
                   "border-radius:0px;padding:8px 12px;font-size:14px;"
                   "font-family:'Consolas','Courier New',monospace;")
        .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM());
}

static QString error_bubble_ss() {
    return QString("background:rgba(50,12,12,0.7);color:%1;border:1px solid rgba(220,38,38,0.2);"
                   "border-radius:0px;padding:8px 12px;font-size:13px;"
                   "font-family:'Consolas','Courier New',monospace;")
        .arg(ui::colors::NEGATIVE());
}

static void apply_obsidian_palette(QTextEdit* edit) {
    QPalette p = edit->palette();
    p.setColor(QPalette::Link, QColor(ui::colors::AMBER()));
    p.setColor(QPalette::LinkVisited, QColor(ui::colors::AMBER()));
    p.setColor(QPalette::Highlight, QColor(ui::colors::AMBER_DIM()));
    p.setColor(QPalette::HighlightedText, QColor(ui::colors::TEXT_PRIMARY()));
    p.setColor(QPalette::Base, Qt::transparent);
    p.setColor(QPalette::Text, QColor(ui::colors::TEXT_PRIMARY()));
    edit->setPalette(p);
}

ChatMessagePanel::ChatMessagePanel(QWidget* parent) : QWidget(parent) {
    build_ui();

    render_timer_ = new QTimer(this);
    render_timer_->setSingleShot(true);
    render_timer_->setInterval(120);
    connect(render_timer_, &QTimer::timeout, this, [this]() {
        if (!streaming_bubble_ || streaming_buffer_.isEmpty())
            return;
        streaming_bubble_->setHtml(ui::MarkdownRenderer::render(streaming_buffer_));
        resize_bubble(streaming_bubble_);
        render_dirty_ = false;
        if (!scroll_locked_)
            scroll_to_bottom();
    });
}

void ChatMessagePanel::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
    vl->addWidget(build_header());
    vl->addWidget(build_messages_area(), 1);
    vl->addWidget(build_typing_indicator());
    vl->addWidget(build_input_area());
}

QWidget* ChatMessagePanel::build_header() {
    auto* hdr = new QWidget(this);
    hdr->setFixedHeight(34);
    hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(14, 0, 14, 0);
    hl->setSpacing(10);

    hdr_title_lbl_ = new QLabel("New Conversation");
    hdr_title_lbl_->setStyleSheet(QString("color:%1;font-size:14px;font-weight:600;"
                                          "font-family:%2;background:transparent;")
                                      .arg(ui::colors::TEXT_PRIMARY(), FONT));
    hl->addWidget(hdr_title_lbl_);
    hl->addStretch();

    hdr_credits_lbl_ = new QLabel;
    hdr_credits_lbl_->setStyleSheet(QString("color:%1;font-size:12px;font-weight:600;"
                                            "font-family:%2;background:transparent;")
                                        .arg(ui::colors::CYAN(), FONT));
    hl->addWidget(hdr_credits_lbl_);

    hdr_tools_lbl_ = new QLabel;
    hdr_tools_lbl_->setStyleSheet(
        QString("color:%1;font-size:12px;font-family:%2;background:transparent;").arg(ui::colors::TEXT_TERTIARY(), FONT));
    hl->addWidget(hdr_tools_lbl_);

    mode_btn_ = new QPushButton("LITE");
    mode_btn_->setFixedHeight(22);
    mode_btn_->setToolTip("Toggle Lite / Deep mode");
    mode_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;"
                "border-radius:0px;font-size:11px;font-weight:600;padding:0 10px;"
                "font-family:%4;letter-spacing:0.5px;}"
                "QPushButton:hover{background:%5;border-color:%2;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::AMBER(), ui::colors::BORDER_MED(), FONT, ui::colors::BG_HOVER()));
    connect(mode_btn_, &QPushButton::clicked, this, [this]() {
        current_mode_ = (current_mode_ == StreamMode::Lite) ? StreamMode::Deep : StreamMode::Lite;
        mode_btn_->setText(current_mode_ == StreamMode::Lite ? "LITE" : "DEEP");
        emit mode_toggled(current_mode_);
    });
    hl->addWidget(mode_btn_);

    hdr_tokens_lbl_ = new QLabel("0 tokens");
    hdr_tokens_lbl_->setStyleSheet(
        QString("color:%1;font-size:12px;font-family:%2;background:transparent;").arg(ui::colors::TEXT_DIM(), FONT));
    hl->addWidget(hdr_tokens_lbl_);

    return hdr;
}

QWidget* ChatMessagePanel::build_messages_area() {
    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area_->setStyleSheet(QString("QScrollArea{background:%1;border:none;}"
                                        "QScrollBar:vertical{background:transparent;width:4px;margin:2px;}"
                                        "QScrollBar::handle:vertical{background:%2;border-radius:0px;min-height:30px;}"
                                        "QScrollBar::handle:vertical:hover{background:%3;}"
                                        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                                    .arg(ui::colors::BG_BASE(), ui::colors::BORDER_MED(), ui::colors::BORDER_BRIGHT()));

    messages_container_ = new QWidget(this);
    messages_container_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    messages_layout_ = new QVBoxLayout(messages_container_);
    messages_layout_->setContentsMargins(14, 10, 14, 10);
    messages_layout_->setSpacing(10);

    welcome_panel_ = build_welcome();
    messages_layout_->addWidget(welcome_panel_);
    messages_layout_->addStretch(1);

    scroll_area_->setWidget(messages_container_);

    // Scroll persistence signal — emitted when the user scrolls within the
    // message history; ChatModeScreen listens and debounces a save.
    if (auto* vbar = scroll_area_->verticalScrollBar())
        connect(vbar, &QScrollBar::valueChanged, this, [this](int) { emit scroll_changed(); });

    return scroll_area_;
}

QWidget* ChatMessagePanel::build_welcome() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(14, 40, 14, 40);
    vl->setSpacing(14);
    vl->setAlignment(Qt::AlignCenter);

    auto* logo = new QLabel("FINCEPT AGENT");
    logo->setAlignment(Qt::AlignCenter);
    logo->setStyleSheet(QString("color:%1;font-size:20px;font-weight:700;letter-spacing:1px;"
                                "font-family:%2;background:transparent;")
                            .arg(ui::colors::AMBER(), FONT));
    vl->addWidget(logo);

    auto* sub = new QLabel("AI-powered financial intelligence.\n"
                           "Markets, equities, portfolio, macro insights.");
    sub->setAlignment(Qt::AlignCenter);
    sub->setWordWrap(true);
    sub->setStyleSheet(
        QString("color:%1;font-size:13px;font-family:%2;background:transparent;").arg(ui::colors::TEXT_TERTIARY(), FONT));
    vl->addWidget(sub);

    const QStringList chips = {"Outlook for AAPL?", "Today's market news", "Portfolio risk analysis",
                               "Key indicators this week"};
    auto* row = new QWidget(this);
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 8, 0, 0);
    rl->setSpacing(6);
    rl->setAlignment(Qt::AlignCenter);
    for (const auto& text : chips) {
        auto* btn = new QPushButton(text);
        btn->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:1px solid %3;"
                                   "border-radius:0px;padding:6px 12px;font-size:12px;"
                                   "font-family:%4;}"
                                   "QPushButton:hover{background:%5;color:%6;border-color:%7;}")
                               .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), FONT,
                                    ui::colors::BG_HOVER(), ui::colors::TEXT_PRIMARY(), ui::colors::AMBER()));
        connect(btn, &QPushButton::clicked, this, [this, text]() {
            input_box_->setPlainText(text);
            on_send_clicked();
        });
        rl->addWidget(btn);
    }
    vl->addWidget(row);
    return w;
}

QWidget* ChatMessagePanel::build_typing_indicator() {
    typing_indicator_ = new QWidget(this);
    typing_indicator_->setFixedHeight(26);
    typing_indicator_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    typing_indicator_->setVisible(false);

    auto* hl = new QHBoxLayout(typing_indicator_);
    hl->setContentsMargins(14, 4, 14, 4);
    hl->setSpacing(6);

    auto* lbl = new QLabel("Agent");
    lbl->setStyleSheet(QString("color:%1;font-size:12px;font-weight:600;"
                               "font-family:%2;background:transparent;")
                           .arg(ui::colors::AMBER(), FONT));
    hl->addWidget(lbl);

    typing_dots_lbl_ = new QLabel(".");
    typing_dots_lbl_->setStyleSheet(
        QString("color:%1;font-size:12px;background:transparent;font-family:%2;").arg(ui::colors::AMBER(), FONT));
    hl->addWidget(typing_dots_lbl_);

    typing_status_lbl_ = new QLabel;
    typing_status_lbl_->setStyleSheet(
        QString("color:%1;font-size:11px;background:transparent;font-family:%2;").arg(ui::colors::TEXT_DIM(), FONT));
    hl->addWidget(typing_status_lbl_);
    hl->addStretch();

    typing_timer_ = new QTimer(this);
    typing_timer_->setInterval(400);
    connect(typing_timer_, &QTimer::timeout, this, &ChatMessagePanel::on_typing_tick);
    return typing_indicator_;
}

QWidget* ChatMessagePanel::build_input_area() {
    auto* container = new QWidget(this);
    container->setStyleSheet(
        QString("background:%1;border-top:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(container);
    vl->setContentsMargins(14, 8, 14, 10);
    vl->setSpacing(6);

    input_box_ = new QPlainTextEdit;
    input_box_->setPlaceholderText("Ask anything... (Enter to send, Shift+Enter for new line)");
    input_box_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    input_box_->setFixedHeight(36);
    input_box_->setStyleSheet(QString("QPlainTextEdit{background:%1;color:%2;border:1px solid %3;"
                                      "border-radius:0px;padding:6px 12px;font-size:14px;font-family:%4;}"
                                      "QPlainTextEdit:focus{border:1px solid %5;}"
                                      "QScrollBar:vertical{background:%1;width:4px;}"
                                      "QScrollBar::handle:vertical{background:%6;}"
                                      "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                                  .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), FONT,
                                       ui::colors::BORDER_BRIGHT(), ui::colors::BORDER_MED()));
    input_box_->installEventFilter(this);
    vl->addWidget(input_box_);

    auto* bottom = new QHBoxLayout;
    bottom->setSpacing(6);

    char_lbl_ = new QLabel("0 / 4000");
    char_lbl_->setStyleSheet(
        QString("color:%1;font-size:11px;font-family:%2;background:transparent;").arg(ui::colors::TEXT_DIM(), FONT));
    bottom->addWidget(char_lbl_);
    bottom->addStretch();

    connect(input_box_, &QPlainTextEdit::textChanged, this, [this]() {
        const QString text = input_box_->toPlainText();
        char_lbl_->setText(QString("%1 / 4000").arg(text.length()));
        if (text.isEmpty()) {
            input_box_->setFixedHeight(36);
            emit draft_changed();
            return;
        }
        auto* doc = input_box_->document();
        doc->setTextWidth(input_box_->viewport()->width());
        const int need = static_cast<int>(doc->size().height()) + 16;
        const int h = (need < 36) ? 36 : (need > 160) ? 160 : need;
        if (input_box_->height() != h)
            input_box_->setFixedHeight(h);
        emit draft_changed();
    });

    optimize_btn_ = new QPushButton("Optimize");
    optimize_btn_->setFixedHeight(26);
    optimize_btn_->setToolTip("Optimize prompt with AI");
    optimize_btn_->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:1px solid %3;"
                                         "border-radius:0px;font-size:12px;padding:0 10px;font-family:%4;}"
                                         "QPushButton:hover{background:%5;color:%6;border-color:%6;}"
                                         "QPushButton:disabled{color:%7;border-color:%1;}")
                                     .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(),
                                          FONT, ui::colors::BG_HOVER(), ui::colors::AMBER(), ui::colors::BORDER_BRIGHT()));
    connect(optimize_btn_, &QPushButton::clicked, this, &ChatMessagePanel::on_optimize_clicked);
    bottom->addWidget(optimize_btn_);

    stop_btn_ = new QPushButton("Stop");
    stop_btn_->setFixedHeight(26);
    stop_btn_->setVisible(false);
    stop_btn_->setStyleSheet(QString("QPushButton{background:rgba(50,12,12,0.7);color:%1;"
                                     "border:1px solid rgba(220,38,38,0.2);border-radius:0px;"
                                     "font-size:12px;padding:0 12px;font-family:%2;}"
                                     "QPushButton:hover{background:rgba(60,15,15,0.8);}")
                                 .arg(ui::colors::NEGATIVE(), FONT));
    connect(stop_btn_, &QPushButton::clicked, this, []() { ChatModeService::instance().abort_stream(); });
    bottom->addWidget(stop_btn_);

    send_btn_ = new QPushButton("Send");
    send_btn_->setFixedHeight(26);
    send_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:none;"
                "border-radius:0px;font-size:12px;font-weight:600;padding:0 16px;"
                "font-family:%3;}"
                "QPushButton:hover{background:#b45309;}"
                "QPushButton:disabled{background:%4;color:%5;}")
            .arg(ui::colors::AMBER(), ui::colors::BG_BASE(), FONT, ui::colors::BG_RAISED(), ui::colors::TEXT_DIM()));
    connect(send_btn_, &QPushButton::clicked, this, &ChatMessagePanel::on_send_clicked);
    bottom->addWidget(send_btn_);

    vl->addLayout(bottom);
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
    while (messages_layout_->count() > 0) {
        auto* item = messages_layout_->takeAt(0);
        if (item->widget() && item->widget() != welcome_panel_)
            item->widget()->deleteLater();
        delete item;
    }
    messages_layout_->addWidget(welcome_panel_);
    messages_layout_->addStretch(1);
    show_welcome(true);
    total_tokens_ = 0;
    hdr_tokens_lbl_->setText("0 tokens");
    thinking_card_ = nullptr;
}

void ChatMessagePanel::set_session_title(const QString& title) {
    hdr_title_lbl_->setText(title.isEmpty() ? "New Conversation" : title);
}

void ChatMessagePanel::set_stream_mode(StreamMode mode) {
    current_mode_ = mode;
    mode_btn_->setText(mode == StreamMode::Lite ? "LITE" : "DEEP");
}

// ── Bubble creation ──────────────────────────────────────────────────────────

void ChatMessagePanel::resize_bubble(QTextEdit* bubble) {
    auto* doc = bubble->document();
    const int vw = bubble->viewport()->width();
    doc->setTextWidth(vw > 0 ? vw : 600);
    bubble->setFixedHeight(static_cast<int>(doc->size().height()) + 20);
}

void ChatMessagePanel::add_message_bubble(const QString& role, const QString& content, const QString& timestamp) {
    show_welcome(false);

    auto* bubble = new QTextEdit;
    bubble->setReadOnly(true);
    apply_obsidian_palette(bubble);
    bubble->setStyleSheet(role == "user" ? user_bubble_ss() : ai_bubble_ss());
    bubble->setFrameShape(QFrame::NoFrame);
    bubble->document()->setDocumentMargin(4);
    bubble->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    bubble->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    bubble->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    if (role == "user")
        bubble->setPlainText(content);
    else
        bubble->setHtml(ui::MarkdownRenderer::render(content));

    resize_bubble(bubble);
    QTimer::singleShot(0, bubble, [this, bubble]() { resize_bubble(bubble); });

    auto* row = new QWidget(this);
    auto* row_vl = new QVBoxLayout(row);
    row_vl->setContentsMargins(0, 0, 0, 0);
    row_vl->setSpacing(2);

    auto* meta = new QHBoxLayout;
    meta->setSpacing(6);
    auto* role_lbl = new QLabel(role == "user" ? "You" : "Agent");
    role_lbl->setStyleSheet(
        QString("color:%1;font-size:11px;font-weight:600;font-family:%2;"
                "background:transparent;letter-spacing:0.5px;")
            .arg(role == "user" ? QString(ui::colors::TEXT_SECONDARY()) : QString(ui::colors::AMBER()), FONT));
    meta->addWidget(role_lbl);
    if (!timestamp.isEmpty()) {
        auto* ts = new QLabel(timestamp.left(16));
        ts->setStyleSheet(QString("color:%1;font-size:11px;font-family:%2;"
                                  "background:transparent;")
                              .arg(ui::colors::TEXT_DIM(), FONT));
        meta->addWidget(ts);
    }
    meta->addStretch();
    row_vl->addLayout(meta);
    row_vl->addWidget(bubble);

    const int pos = messages_layout_->count() - 1; // before stretch
    messages_layout_->insertWidget(pos, row);
}

QTextEdit* ChatMessagePanel::add_streaming_bubble() {
    show_welcome(false);

    auto* bubble = new QTextEdit;
    bubble->setReadOnly(true);
    apply_obsidian_palette(bubble);
    bubble->setStyleSheet(ai_bubble_ss());
    bubble->setFrameShape(QFrame::NoFrame);
    bubble->document()->setDocumentMargin(4);
    bubble->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    bubble->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    bubble->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    bubble->setFixedHeight(40);

    auto* row = new QWidget(this);
    auto* row_vl = new QVBoxLayout(row);
    row_vl->setContentsMargins(0, 0, 0, 0);
    row_vl->setSpacing(2);
    auto* lbl = new QLabel("Agent");
    lbl->setStyleSheet(QString("color:%1;font-size:11px;font-weight:600;font-family:%2;"
                               "background:transparent;letter-spacing:0.5px;")
                           .arg(ui::colors::AMBER(), FONT));
    row_vl->addWidget(lbl);
    row_vl->addWidget(bubble);

    const int pos = messages_layout_->count() - 1;
    messages_layout_->insertWidget(pos, row);
    return bubble;
}

void ChatMessagePanel::insert_collapsed_thinking_card(int before_index) {
    if (pending_thinking_.isEmpty() && pending_tools_.isEmpty())
        return;

    auto* card = new QWidget(this);
    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(8, 4, 8, 4);
    vl->setSpacing(2);
    card->setStyleSheet(
        QString("background:%1;border:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    QString summary;
    if (!pending_tools_.isEmpty()) {
        QStringList tool_names;
        for (const auto& [name, ms] : pending_tools_)
            tool_names.append(QString("%1 (%2ms)").arg(name).arg(ms));
        summary = QString("> %1 thinking steps | tools: %2").arg(pending_thinking_.size()).arg(tool_names.join(", "));
    } else {
        summary = QString("> %1 thinking steps").arg(pending_thinking_.size());
    }

    auto* header = new QPushButton(summary);
    header->setCursor(Qt::PointingHandCursor);
    header->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:none;"
                                  "font-size:11px;font-family:%2;text-align:left;padding:2px 0;}"
                                  "QPushButton:hover{color:%3;}")
                              .arg(ui::colors::TEXT_TERTIARY(), FONT, ui::colors::TEXT_SECONDARY()));
    vl->addWidget(header);

    auto* detail = new QWidget(this);
    detail->setVisible(false);
    auto* dvl = new QVBoxLayout(detail);
    dvl->setContentsMargins(4, 4, 0, 0);
    dvl->setSpacing(1);

    // Merge thinking chunks into one block
    const QString merged = pending_thinking_.join(" ").left(1000);
    auto* thought = new QLabel(merged);
    thought->setWordWrap(true);
    thought->setStyleSheet(QString("color:%1;font-size:11px;font-style:italic;"
                                   "font-family:%2;background:transparent;")
                               .arg(ui::colors::TEXT_DIM(), FONT));
    dvl->addWidget(thought);

    for (const auto& [name, ms] : pending_tools_) {
        auto* line = new QLabel(QString(":: %1 %2ms").arg(name).arg(ms));
        line->setStyleSheet(QString("color:%1;font-size:11px;font-family:%2;"
                                    "background:transparent;")
                                .arg(ui::colors::AMBER(), FONT));
        dvl->addWidget(line);
    }
    vl->addWidget(detail);

    connect(header, &QPushButton::clicked, this, [detail]() { detail->setVisible(!detail->isVisible()); });

    // Insert before the agent bubble (or at end if index unknown)
    const int pos = (before_index >= 0) ? before_index : (messages_layout_->count() - 1);
    messages_layout_->insertWidget(pos, card);

    pending_thinking_.clear();
    pending_tools_.clear();
    thinking_card_ = card;
}

// ── SSE slots ─────────────────────────────────────────────────────────────────

void ChatMessagePanel::on_stream_session_meta(const QString&, const QString& new_title) {
    if (!new_title.isEmpty())
        set_session_title(new_title);
}

void ChatMessagePanel::on_stream_text_delta(const QString& text) {
    if (!streaming_bubble_)
        return;

    streaming_buffer_ += text;
    render_dirty_ = true;

    if (!render_timer_->isActive())
        render_timer_->start();
}

void ChatMessagePanel::on_stream_tool_end(const QString& tool_name, int duration_ms) {
    pending_tools_.append({tool_name, duration_ms});
    typing_status_lbl_->setText(QString("used %1").arg(tool_name));
}

void ChatMessagePanel::on_stream_step_start(int step_number) {
    typing_status_lbl_->setText(QString("step %1").arg(step_number));
}

void ChatMessagePanel::on_stream_step_finish(int tokens_used) {
    total_tokens_ += tokens_used;
    hdr_tokens_lbl_->setText(QString("%1 tokens").arg(total_tokens_));
}

void ChatMessagePanel::on_stream_thinking(const QString& content) {
    if (!content.isEmpty()) {
        pending_thinking_.append(content);
        typing_status_lbl_->setText("thinking...");
    }
}

void ChatMessagePanel::on_stream_finish(int total_tokens) {
    total_tokens_ += total_tokens;
    hdr_tokens_lbl_->setText(QString("%1 tokens").arg(total_tokens_));

    render_timer_->stop();
    if (streaming_bubble_ && !streaming_buffer_.isEmpty()) {
        streaming_bubble_->setHtml(ui::MarkdownRenderer::render(streaming_buffer_));
        resize_bubble(streaming_bubble_);
    }

    // Insert collapsed thinking card between user query and agent response
    if (!pending_thinking_.isEmpty() || !pending_tools_.isEmpty()) {
        // Find the streaming bubble's parent row widget index
        QWidget* bubble_row = streaming_bubble_ ? streaming_bubble_->parentWidget() : nullptr;
        int bubble_idx = -1;
        if (bubble_row) {
            bubble_idx = messages_layout_->indexOf(bubble_row);
        }
        insert_collapsed_thinking_card(bubble_idx);
    }

    streaming_ = false;
    streaming_bubble_ = nullptr;
    streaming_buffer_.clear();
    scroll_locked_ = false;
    show_typing(false);
    set_input_enabled(true);
    send_btn_->setVisible(true);
    optimize_btn_->setVisible(true);
    stop_btn_->setVisible(false);
    scroll_to_bottom();
}

void ChatMessagePanel::on_stream_error(const QString& message) {
    render_timer_->stop();
    streaming_ = false;
    streaming_bubble_ = nullptr;
    streaming_buffer_.clear();
    scroll_locked_ = false;
    pending_thinking_.clear();
    pending_tools_.clear();
    show_typing(false);
    set_input_enabled(true);
    send_btn_->setVisible(true);
    optimize_btn_->setVisible(true);
    stop_btn_->setVisible(false);

    auto* err = new QLabel("! " + message);
    err->setWordWrap(true);
    err->setStyleSheet(error_bubble_ss());
    const int pos = messages_layout_->count() - 1;
    messages_layout_->insertWidget(pos, err);
    scroll_to_bottom();
}

void ChatMessagePanel::on_stream_heartbeat() {}

void ChatMessagePanel::on_insufficient_credits() {
    on_stream_error("Insufficient credits. Top up to continue.");
}

void ChatMessagePanel::on_tools_registered(int count) {
    hdr_tools_lbl_->setText(count > 0 ? QString("%1 tools").arg(count) : QString());
}

void ChatMessagePanel::set_credits(int credits) {
    if (credits > 0)
        hdr_credits_lbl_->setText(QString("%1 credits").arg(QLocale(QLocale::English).toString(credits)));
    else
        hdr_credits_lbl_->setText("0 credits");
}

// ── Send / Optimize ──────────────────────────────────────────────────────────

void ChatMessagePanel::on_send_clicked() {
    const QString text = input_box_->toPlainText().trimmed();
    if (text.isEmpty() || streaming_)
        return;

    input_box_->clear();
    streaming_ = true;
    streaming_buffer_.clear();
    scroll_locked_ = false;
    pending_thinking_.clear();
    pending_tools_.clear();
    thinking_card_ = nullptr;

    add_message_bubble("user", text);
    show_typing(true);
    set_input_enabled(false);
    send_btn_->setVisible(false);
    optimize_btn_->setVisible(false);
    stop_btn_->setVisible(true);
    streaming_bubble_ = add_streaming_bubble();
    scroll_to_bottom();

    emit send_requested(text, current_mode_);
}

void ChatMessagePanel::on_optimize_clicked() {
    const QString text = input_box_->toPlainText().trimmed();
    if (text.isEmpty() || streaming_)
        return;

    optimize_btn_->setEnabled(false);
    optimize_btn_->setText("...");

    QPointer<ChatMessagePanel> self = this;
    ChatModeService::instance().optimize_prompt(text, current_mode_ == StreamMode::Deep ? "deep" : "lite",
                                                [self](bool ok, OptimizedPrompt result, QString err) {
                                                    if (!self)
                                                        return;
                                                    self->optimize_btn_->setEnabled(true);
                                                    self->optimize_btn_->setText("Optimize");
                                                    if (!ok) {
                                                        self->on_stream_error("Optimize failed: " + err);
                                                        return;
                                                    }
                                                    if (!result.optimized.isEmpty())
                                                        self->input_box_->setPlainText(result.optimized);
                                                });
}

// ── Event filter ─────────────────────────────────────────────────────────────

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

// ── Helpers ──────────────────────────────────────────────────────────────────

void ChatMessagePanel::scroll_to_bottom() {
    QTimer::singleShot(10, this, [this]() {
        auto* bar = scroll_area_->verticalScrollBar();
        if (bar)
            bar->setValue(bar->maximum());
    });
}

void ChatMessagePanel::show_welcome(bool show) {
    if (welcome_panel_)
        welcome_panel_->setVisible(show);
}

void ChatMessagePanel::show_typing(bool show) {
    typing_indicator_->setVisible(show);
    if (show) {
        typing_status_lbl_->clear();
        typing_step_ = 0;
        typing_timer_->start();
    } else {
        typing_timer_->stop();
    }
}

void ChatMessagePanel::set_input_enabled(bool enabled) {
    input_box_->setEnabled(enabled);
}

void ChatMessagePanel::on_typing_tick() {
    static const char* dots[] = {".", ". .", ". . ."};
    typing_dots_lbl_->setText(QLatin1String(dots[typing_step_ % 3]));
    ++typing_step_;
}

// ── Draft + scroll accessors (consumed by ChatModeScreen persistence) ───────

QString ChatMessagePanel::draft_text() const {
    return input_box_ ? input_box_->toPlainText() : QString();
}

void ChatMessagePanel::set_draft_text(const QString& text) {
    if (!input_box_ || input_box_->toPlainText() == text)
        return;
    input_box_->setPlainText(text);
    // Move cursor to end so the user can continue typing where they left off.
    auto c = input_box_->textCursor();
    c.movePosition(QTextCursor::End);
    input_box_->setTextCursor(c);
}

int ChatMessagePanel::scroll_position() const {
    if (!scroll_area_)
        return 0;
    auto* bar = scroll_area_->verticalScrollBar();
    return bar ? bar->value() : 0;
}

void ChatMessagePanel::set_scroll_position(int pos) {
    if (!scroll_area_)
        return;
    auto* bar = scroll_area_->verticalScrollBar();
    if (bar)
        bar->setValue(pos);
}

} // namespace fincept::chat_mode
