// AiChatScreen.cpp — Fincept AI Terminal chat (Qt port of fincept-web)

#include "ai_chat/AiChatScreen.h"
#include "ai_chat/LlmService.h"
#include "mcp/McpService.h"
#include "storage/repositories/ChatRepository.h"
#include "core/logging/Logger.h"

#include <QApplication>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPointer>
#include <QScrollBar>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

namespace fincept::screens {

static constexpr const char* TAG = "AiChatScreen";

// ── Fincept terminal palette (matches fincept-web FINCEPT object) ────────────
namespace fc {
    constexpr const char* ORANGE    = "#FF8800";
    constexpr const char* WHITE     = "#FFFFFF";
    constexpr const char* RED       = "#FF3B3B";
    constexpr const char* GREEN     = "#00D66F";
    constexpr const char* GRAY      = "#787878";
    constexpr const char* DARK_BG   = "#000000";
    constexpr const char* PANEL_BG  = "#0F0F0F";
    constexpr const char* HEADER_BG = "#1A1A1A";
    constexpr const char* BORDER    = "#2A2A2A";
    constexpr const char* HOVER     = "#1F1F1F";
    constexpr const char* MUTED     = "#4A4A4A";
    constexpr const char* CYAN      = "#00E5FF";
    constexpr const char* YELLOW    = "#FFD700";
}
static const QString MF = "font-family:'IBM Plex Mono','Consolas','Courier New',monospace;";

// ============================================================================
// Construction
// ============================================================================

AiChatScreen::AiChatScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    load_sessions();
}

void AiChatScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::finished_streaming,
            this, &AiChatScreen::on_streaming_done, Qt::UniqueConnection);
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::config_changed,
            this, &AiChatScreen::on_provider_changed, Qt::UniqueConnection);
    update_stats();
    if (clock_timer_) clock_timer_->start(1000);
}

// ============================================================================
// Build UI
// ============================================================================

void AiChatScreen::build_ui() {
    setStyleSheet(QString("background:%1;").arg(fc::DARK_BG));
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    build_sidebar();
    build_chat_area();

    root->addWidget(sidebar_);
    root->addWidget(chat_widget_, 1);
}

// ── Left sidebar (280px) ─────────────────────────────────────────────────────

void AiChatScreen::build_sidebar() {
    sidebar_ = new QWidget;
    sidebar_->setFixedWidth(280);
    sidebar_->setStyleSheet(
        QString("background:%1;border-right:1px solid %2;").arg(fc::PANEL_BG, fc::BORDER));

    auto* vl = new QVBoxLayout(sidebar_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Session header
    auto* hdr = new QWidget;
    hdr->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;").arg(fc::HEADER_BG, fc::BORDER));
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(12, 8, 12, 8);

    session_count_lbl_ = new QLabel("SESSIONS (0)");
    session_count_lbl_->setStyleSheet(
        QString("color:%1;font-size:9px;font-weight:700;letter-spacing:0.5px;%2").arg(fc::GRAY, MF));
    hhl->addWidget(session_count_lbl_);
    hhl->addStretch();

    new_btn_ = new QPushButton("+ NEW");
    new_btn_->setFixedHeight(22);
    new_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:none;padding:4px 8px;"
                "font-size:8px;font-weight:700;%3}QPushButton:hover{background:#cc6e00;}")
            .arg(fc::ORANGE, fc::DARK_BG, MF));
    connect(new_btn_, &QPushButton::clicked, this, &AiChatScreen::on_new_session);
    hhl->addWidget(new_btn_);
    vl->addWidget(hdr);

    // Search
    auto* search_bar = new QWidget;
    search_bar->setStyleSheet(QString("background:%1;").arg(fc::PANEL_BG));
    auto* shl = new QHBoxLayout(search_bar);
    shl->setContentsMargins(8, 8, 8, 8);
    search_edit_ = new QLineEdit;
    search_edit_->setPlaceholderText("SEARCH...");
    search_edit_->setStyleSheet(
        QString("QLineEdit{background:%1;color:%2;border:1px solid %3;padding:6px 8px;font-size:9px;%4}"
                "QLineEdit:focus{border-color:%5;}").arg(fc::DARK_BG, fc::WHITE, fc::BORDER, MF, fc::ORANGE));
    connect(search_edit_, &QLineEdit::textChanged, this, &AiChatScreen::on_search_changed);
    shl->addWidget(search_edit_);
    vl->addWidget(search_bar);

    // Session list
    session_list_ = new QListWidget;
    session_list_->setStyleSheet(
        QString("QListWidget{background:transparent;border:none;color:%1;font-size:10px;%2}"
                "QListWidget::item{padding:10px 12px;margin:0 8px 4px 8px;border-left:2px solid transparent;}"
                "QListWidget::item:selected{background:rgba(255,136,0,0.08);border-left:2px solid %3;}"
                "QListWidget::item:hover{background:%4;}").arg(fc::WHITE, MF, fc::ORANGE, fc::HOVER));
    connect(session_list_, &QListWidget::currentRowChanged, this, &AiChatScreen::on_session_selected);
    vl->addWidget(session_list_, 1);

    // Delete button
    auto* del_bar = new QWidget;
    del_bar->setStyleSheet(QString("background:%1;").arg(fc::PANEL_BG));
    auto* dbl = new QHBoxLayout(del_bar);
    dbl->setContentsMargins(8, 4, 8, 4);
    delete_btn_ = new QPushButton("DELETE SESSION");
    delete_btn_->setEnabled(false);
    delete_btn_->setFixedHeight(22);
    delete_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %1;"
                "font-size:8px;font-weight:700;%2}"
                "QPushButton:hover{background:%1;color:%3;}"
                "QPushButton:disabled{color:%4;border-color:%5;}")
            .arg(fc::RED, MF, fc::WHITE, fc::MUTED, fc::BORDER));
    connect(delete_btn_, &QPushButton::clicked, this, &AiChatScreen::on_delete_session);
    dbl->addWidget(delete_btn_);
    vl->addWidget(del_bar);

    // Stats footer
    auto* stats = new QWidget;
    stats->setStyleSheet(QString("background:%1;border-top:1px solid %2;").arg(fc::HEADER_BG, fc::BORDER));
    auto* svl = new QVBoxLayout(stats);
    svl->setContentsMargins(12, 8, 12, 8);
    svl->setSpacing(2);

    auto* stats_title = new QLabel("USAGE STATS");
    stats_title->setStyleSheet(
        QString("color:%1;font-size:9px;font-weight:700;letter-spacing:0.5px;%2").arg(fc::GRAY, MF));
    svl->addWidget(stats_title);

    auto make_stat = [&](const QString& label) -> QLabel* {
        auto* row = new QWidget;
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        auto* l = new QLabel(label);
        l->setStyleSheet(QString("color:%1;font-size:9px;%2").arg(fc::GRAY, MF));
        rl->addWidget(l);
        auto* v = new QLabel("0");
        v->setAlignment(Qt::AlignRight);
        v->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;%2").arg(fc::CYAN, MF));
        rl->addWidget(v);
        svl->addWidget(row);
        return v;
    };
    stats_msgs_lbl_   = make_stat("TOTAL MSGS");
    stats_tokens_lbl_ = make_stat("TOTAL TOKENS");
    vl->addWidget(stats);
}

// ── Chat area ────────────────────────────────────────────────────────────────

void AiChatScreen::build_chat_area() {
    chat_widget_ = new QWidget;
    chat_widget_->setStyleSheet(QString("background:%1;").arg(fc::DARK_BG));
    auto* vl = new QVBoxLayout(chat_widget_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_header_bar());

    // Messages
    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setFrameShape(QFrame::NoFrame);
    scroll_area_->setStyleSheet(
        QString("QScrollArea{background:%1;border:none;}"
                "QScrollBar:vertical{background:%1;width:6px;}"
                "QScrollBar::handle:vertical{background:%2;border-radius:3px;}"
                "QScrollBar::handle:vertical:hover{background:%3;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
            .arg(fc::DARK_BG, fc::BORDER, fc::MUTED));
    messages_container_ = new QWidget;
    messages_container_->setStyleSheet(QString("background:%1;").arg(fc::DARK_BG));
    messages_layout_ = new QVBoxLayout(messages_container_);
    messages_layout_->setContentsMargins(16, 16, 16, 16);
    messages_layout_->setSpacing(0);
    messages_layout_->addStretch();
    scroll_area_->setWidget(messages_container_);
    vl->addWidget(scroll_area_, 1);

    suggestions_panel_ = build_suggestions();
    vl->addWidget(suggestions_panel_);
    vl->addWidget(build_input_area());
}

QWidget* AiChatScreen::build_header_bar() {
    auto* bar = new QWidget;
    bar->setStyleSheet(QString("background:%1;border-bottom:2px solid %2;").arg(fc::HEADER_BG, fc::ORANGE));
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 8, 16, 8);
    hl->setSpacing(8);

    auto* icon = new QLabel(">");
    icon->setStyleSheet(QString("color:%1;font-size:16px;font-weight:700;%2").arg(fc::ORANGE, MF));
    hl->addWidget(icon);
    auto* title = new QLabel("FINCEPT AI TERMINAL");
    title->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;letter-spacing:0.5px;%2").arg(fc::ORANGE, MF));
    hl->addWidget(title);
    auto* sep1 = new QLabel("|");
    sep1->setStyleSheet(QString("color:%1;font-size:11px;%2").arg(fc::GRAY, MF));
    hl->addWidget(sep1);

    hdr_status_online_ = new QLabel("ONLINE");
    hdr_status_online_->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;%2").arg(fc::GREEN, MF));
    hl->addWidget(hdr_status_online_);
    hdr_status_ai_ = new QLabel("AI READY");
    hdr_status_ai_->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;%2").arg(fc::YELLOW, MF));
    hl->addWidget(hdr_status_ai_);
    hl->addStretch();

    auto make_info = [&](const QString& label) -> QLabel* {
        auto* l = new QLabel(label);
        l->setStyleSheet(QString("color:%1;font-size:9px;%2").arg(fc::GRAY, MF));
        hl->addWidget(l);
        auto* v = new QLabel("-");
        v->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;%2").arg(fc::CYAN, MF));
        hl->addWidget(v);
        return v;
    };
    hdr_session_lbl_ = make_info("SESSION:");
    hdr_msgs_lbl_    = make_info("MSGS:");

    auto* sep2 = new QLabel("|");
    sep2->setStyleSheet(QString("color:%1;font-size:9px;%2").arg(fc::GRAY, MF));
    hl->addWidget(sep2);
    hdr_clock_lbl_ = new QLabel("00:00:00");
    hdr_clock_lbl_->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;%2").arg(fc::WHITE, MF));
    hl->addWidget(hdr_clock_lbl_);

    clock_timer_ = new QTimer(this);
    connect(clock_timer_, &QTimer::timeout, this, &AiChatScreen::on_clock_tick);
    return bar;
}

QWidget* AiChatScreen::build_suggestions() {
    auto* panel = new QWidget;
    panel->setStyleSheet(QString("background:%1;border-top:1px solid %2;").arg(fc::HEADER_BG, fc::BORDER));
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(16, 12, 16, 16);
    vl->setSpacing(8);

    auto* lbl = new QLabel("TRY ASKING:");
    lbl->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;letter-spacing:0.5px;%2").arg(fc::GRAY, MF));
    vl->addWidget(lbl);

    auto* grid = new QGridLayout;
    grid->setSpacing(8);

    struct Sug { QString cat; QString text; };
    const Sug suggestions[] = {
        {"MARKETS",    "Show me top market movers today"},
        {"NEWS",       "Get latest financial news summary"},
        {"PORTFOLIO",  "Analyze my portfolio performance"},
        {"ANALYTICS",  "Calculate stock valuation for AAPL"},
        {"ECONOMICS",  "Current GDP and inflation data"},
        {"RESEARCH",   "Show market trends for tech sector"},
    };

    for (int i = 0; i < 6; ++i) {
        auto* btn = new QPushButton;
        btn->setStyleSheet(
            QString("QPushButton{background:transparent;border:1px solid %1;padding:10px 12px;text-align:left;%2}"
                    "QPushButton:hover{background:%3;border-color:%4;}").arg(fc::BORDER, MF, fc::HOVER, fc::ORANGE));
        auto* bl = new QVBoxLayout(btn);
        bl->setContentsMargins(0, 0, 0, 0);
        bl->setSpacing(2);
        auto* cat = new QLabel(suggestions[i].cat);
        cat->setStyleSheet(QString("color:%1;font-size:8px;font-weight:700;%2").arg(fc::GRAY, MF));
        cat->setAttribute(Qt::WA_TransparentForMouseEvents);
        bl->addWidget(cat);
        auto* desc = new QLabel(suggestions[i].text);
        desc->setStyleSheet(QString("color:%1;font-size:10px;%2").arg(fc::WHITE, MF));
        desc->setWordWrap(true);
        desc->setAttribute(Qt::WA_TransparentForMouseEvents);
        bl->addWidget(desc);
        QString prompt = suggestions[i].text;
        connect(btn, &QPushButton::clicked, this, [this, prompt]() {
            input_box_->setPlainText(prompt);
            on_send();
        });
        grid->addWidget(btn, i / 3, i % 3);
    }
    vl->addLayout(grid);
    return panel;
}

QWidget* AiChatScreen::build_input_area() {
    auto* bar = new QWidget;
    bar->setStyleSheet(QString("background:%1;border-top:1px solid %2;").arg(fc::HEADER_BG, fc::BORDER));
    auto* vl = new QVBoxLayout(bar);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(6);

    auto* input_row = new QWidget;
    input_row->setStyleSheet(QString("background:%1;border:1px solid %2;").arg(fc::DARK_BG, fc::BORDER));
    auto* irl = new QHBoxLayout(input_row);
    irl->setContentsMargins(8, 8, 8, 8);
    irl->setSpacing(8);

    auto* prompt_sym = new QLabel(">");
    prompt_sym->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;%2").arg(fc::ORANGE, MF));
    prompt_sym->setFixedWidth(14);
    irl->addWidget(prompt_sym, 0, Qt::AlignTop);

    input_box_ = new QPlainTextEdit;
    input_box_->setPlaceholderText("ENTER QUERY...");
    input_box_->setFixedHeight(24);
    input_box_->setStyleSheet(
        QString("QPlainTextEdit{background:transparent;color:%1;border:none;font-size:11px;%2}").arg(fc::WHITE, MF));
    input_box_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    input_box_->installEventFilter(this);
    connect(input_box_, &QPlainTextEdit::textChanged, this, [this]() {
        int lines = input_box_->document()->blockCount();
        input_box_->setFixedHeight(qMin(qMax(lines, 1), 5) * 20 + 4);
    });
    irl->addWidget(input_box_, 1);

    send_btn_ = new QPushButton("SEND");
    send_btn_->setFixedHeight(28);
    send_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:none;padding:8px 16px;"
                "font-size:9px;font-weight:700;letter-spacing:0.5px;%3}"
                "QPushButton:hover{background:#cc6e00;}"
                "QPushButton:disabled{background:%4;color:%5;}")
            .arg(fc::ORANGE, fc::DARK_BG, MF, fc::MUTED, fc::GRAY));
    connect(send_btn_, &QPushButton::clicked, this, &AiChatScreen::on_send);
    irl->addWidget(send_btn_, 0, Qt::AlignBottom);
    vl->addWidget(input_row);

    auto* help = new QHBoxLayout;
    auto* lh = new QLabel("ENTER TO SEND | SHIFT+ENTER FOR NEW LINE");
    lh->setStyleSheet(QString("color:%1;font-size:9px;%2").arg(fc::GRAY, MF));
    help->addWidget(lh);
    help->addStretch();
    auto* rh = new QLabel("POWERED BY FINCEPT AI ENGINE");
    rh->setStyleSheet(QString("color:%1;font-size:9px;%2").arg(fc::GRAY, MF));
    help->addWidget(rh);
    vl->addLayout(help);
    return bar;
}

// ============================================================================
// Events
// ============================================================================

bool AiChatScreen::eventFilter(QObject* obj, QEvent* event) {
    if (obj == input_box_ && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (ke->modifiers() & Qt::ShiftModifier) return false;
            ke->accept();
            on_send();
            return true;
        }
        if (ke->key() == Qt::Key_N && (ke->modifiers() & Qt::ControlModifier)) {
            on_new_session();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void AiChatScreen::on_clock_tick() {
    hdr_clock_lbl_->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
}

void AiChatScreen::on_search_changed(const QString& text) {
    QString filter = text.trimmed().toLower();
    for (int i = 0; i < session_list_->count(); ++i) {
        auto* item = session_list_->item(i);
        item->setHidden(!filter.isEmpty() && !item->text().toLower().contains(filter));
    }
}

// ============================================================================
// Session management
// ============================================================================

void AiChatScreen::load_sessions() {
    session_list_->blockSignals(true);
    session_list_->clear();
    auto result = ChatRepository::instance().list_sessions();
    if (result.is_err()) {
        session_list_->blockSignals(false);
        return;
    }
    for (const auto& s : result.value()) {
        QString label = s.title.isEmpty() ? "Chat " + s.id.left(8) : s.title;
        auto* item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, s.id);
        session_list_->addItem(item);
    }
    session_list_->blockSignals(false);
    session_count_lbl_->setText(QString("SESSIONS (%1)").arg(session_list_->count()));

    if (!active_session_id_.isEmpty()) {
        for (int i = 0; i < session_list_->count(); ++i) {
            if (session_list_->item(i)->data(Qt::UserRole).toString() == active_session_id_) {
                session_list_->setCurrentRow(i);
                return;
            }
        }
    }
    if (session_list_->count() > 0) session_list_->setCurrentRow(0);
    else create_new_session();
}

void AiChatScreen::create_new_session() {
    auto result = ChatRepository::instance().create_session(
        "Chat", ai_chat::LlmService::instance().active_provider(),
        ai_chat::LlmService::instance().active_model());
    if (result.is_err()) return;
    active_session_id_ = result.value().id;
    history_.clear();
    total_tokens_ = 0;
    total_messages_ = 0;
    load_sessions();
    show_suggestions(true);
}

void AiChatScreen::load_messages(const QString& session_id) {
    while (messages_layout_->count() > 1) {
        QLayoutItem* item = messages_layout_->takeAt(1);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    history_.clear();
    auto result = ChatRepository::instance().get_messages(session_id);
    if (result.is_err()) return;
    const auto& msgs = result.value();
    total_messages_ = static_cast<int>(msgs.size());
    for (const auto& msg : msgs) {
        add_message_bubble(msg.role, msg.content, msg.timestamp);
        if (msg.role != "system") history_.push_back({msg.role, msg.content});
    }
    show_suggestions(msgs.empty());
    scroll_to_bottom();
    update_stats();
}

// ============================================================================
// Slots
// ============================================================================

void AiChatScreen::on_new_session() { create_new_session(); }

void AiChatScreen::on_session_selected(int row) {
    if (row < 0 || row >= session_list_->count()) return;
    QString id = session_list_->item(row)->data(Qt::UserRole).toString();
    if (id == active_session_id_) return;
    active_session_id_ = id;
    delete_btn_->setEnabled(true);
    hdr_session_lbl_->setText(id.left(8));
    load_messages(id);
}

void AiChatScreen::on_delete_session() {
    if (active_session_id_.isEmpty()) return;
    ChatRepository::instance().delete_session(active_session_id_);
    active_session_id_.clear();
    history_.clear();
    load_sessions();
}

void AiChatScreen::on_send() {
    if (streaming_) return;
    QString text = input_box_->toPlainText().trimmed();
    if (text.isEmpty()) return;
    if (active_session_id_.isEmpty()) create_new_session();
    if (!ai_chat::LlmService::instance().is_configured()) {
        add_message_bubble("system", "No LLM provider configured. Go to Settings > LLM Configuration.");
        return;
    }
    input_box_->clear();
    input_box_->setFixedHeight(24);
    set_input_enabled(false);
    streaming_ = true;
    show_suggestions(false);
    add_message_bubble("user", text);
    total_messages_++;
    ChatRepository::instance().add_message(active_session_id_, "user", text,
        ai_chat::LlmService::instance().active_provider(), ai_chat::LlmService::instance().active_model());
    history_.push_back({"user", text});
    streaming_bubble_ = add_streaming_bubble();
    scroll_to_bottom();

    std::vector<ai_chat::ConversationMessage> hist_copy = history_;
    QString provider = ai_chat::LlmService::instance().active_provider();
    if (ai_chat::provider_supports_streaming(provider)) {
        QPointer<AiChatScreen> self = this;
        ai_chat::LlmService::instance().chat_streaming(text, hist_copy,
            [self](const QString& chunk, bool done) {
                QMetaObject::invokeMethod(qApp, [self, chunk, done]() {
                    if (self) self->on_stream_chunk(chunk, done);
                }, Qt::QueuedConnection);
            });
    } else {
        QPointer<AiChatScreen> self = this;
        QtConcurrent::run([self, text, hist_copy]() {
            auto resp = ai_chat::LlmService::instance().chat(text, hist_copy);
            QMetaObject::invokeMethod(qApp, [self, resp]() {
                if (self) self->on_streaming_done(resp);
            }, Qt::QueuedConnection);
        });
    }
}

void AiChatScreen::on_stream_chunk(const QString& chunk, bool done) {
    if (!streaming_bubble_) return;
    if (!chunk.isEmpty()) {
        streaming_bubble_->moveCursor(QTextCursor::End);
        streaming_bubble_->insertPlainText(chunk);
        scroll_to_bottom();
    }
    Q_UNUSED(done)
}

void AiChatScreen::on_streaming_done(ai_chat::LlmResponse response) {
    streaming_ = false;
    set_input_enabled(true);
    if (!response.success) {
        if (streaming_bubble_) {
            streaming_bubble_->setPlainText("ERROR: " + response.error);
            streaming_bubble_->setStyleSheet(
                QString("QTextEdit{background:%1;color:%2;border:1px solid %2;padding:10px;font-size:11px;%3}")
                    .arg(fc::PANEL_BG, fc::RED, MF));
        }
        streaming_bubble_ = nullptr;
        return;
    }
    QString content = response.content;
    if (streaming_bubble_) {
        if (!content.isEmpty() && streaming_bubble_->toPlainText().isEmpty())
            streaming_bubble_->setPlainText(content);
        streaming_bubble_->setReadOnly(true);
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

void AiChatScreen::on_provider_changed() { update_stats(); }

// ============================================================================
// Message bubbles — matches fincept-web ChatMessageBubble
// ============================================================================

void AiChatScreen::add_message_bubble(const QString& role, const QString& content,
                                       const QString& timestamp) {
    bool is_user = (role == "user");
    QString border_color = is_user ? fc::YELLOW : fc::ORANGE;
    QString icon_text    = is_user ? "U" : ">";
    QString label_text   = is_user ? "YOU" : "FINCEPT AI";
    QString label_color  = is_user ? fc::YELLOW : fc::ORANGE;
    QString ts = timestamp.isEmpty()
        ? QDateTime::currentDateTime().toString("HH:mm:ss")
        : QDateTime::fromString(timestamp, Qt::ISODate).toString("HH:mm:ss");

    auto* align = new QWidget;
    auto* al = new QHBoxLayout(align);
    al->setContentsMargins(0, 0, 0, 0);
    if (is_user) al->addStretch();

    auto* bubble = new QWidget;
    bubble->setMaximumWidth(700);
    bubble->setMinimumWidth(120);
    bubble->setStyleSheet(QString("background:%1;border:1px solid %2;").arg(fc::PANEL_BG, border_color));

    auto* bvl = new QVBoxLayout(bubble);
    bvl->setContentsMargins(10, 10, 10, 10);
    bvl->setSpacing(0);

    // Header
    auto* hdr = new QWidget;
    hdr->setStyleSheet(QString("border-bottom:1px solid %1;padding-bottom:6px;margin-bottom:8px;").arg(fc::BORDER));
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(0, 0, 0, 6);
    hhl->setSpacing(6);

    auto* ic = new QLabel(icon_text);
    ic->setFixedSize(16, 16);
    ic->setAlignment(Qt::AlignCenter);
    ic->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;border:1px solid %1;%2").arg(label_color, MF));
    hhl->addWidget(ic);
    auto* nm = new QLabel(label_text);
    nm->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;letter-spacing:0.5px;border:none;%2").arg(label_color, MF));
    hhl->addWidget(nm);
    hhl->addStretch();
    auto* tl = new QLabel(ts);
    tl->setStyleSheet(QString("color:%1;font-size:9px;border:none;%2").arg(fc::GRAY, MF));
    hhl->addWidget(tl);
    bvl->addWidget(hdr);

    // Body
    auto* body = new QTextEdit;
    body->setReadOnly(true);
    body->setPlainText(content);
    body->setFrameShape(QFrame::NoFrame);
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    body->document()->setDocumentMargin(0);
    body->setStyleSheet(
        QString("QTextEdit{background:transparent;color:%1;border:none;font-size:11px;%2}").arg(fc::WHITE, MF));
    auto resize_fn = [body]() {
        QSize sz = body->document()->size().toSize();
        body->setFixedHeight(qMin(qMax(sz.height() + 4, 20), 800));
    };
    connect(body->document(), &QTextDocument::contentsChanged, body, resize_fn);
    QTimer::singleShot(0, body, resize_fn);
    bvl->addWidget(body);

    al->addWidget(bubble);
    if (!is_user) al->addStretch();
    messages_layout_->addWidget(align);

    auto* spacer = new QWidget;
    spacer->setFixedHeight(12);
    messages_layout_->addWidget(spacer);
}

QTextEdit* AiChatScreen::add_streaming_bubble() {
    auto* align = new QWidget;
    auto* al = new QHBoxLayout(align);
    al->setContentsMargins(0, 0, 0, 0);

    auto* bubble = new QWidget;
    bubble->setMaximumWidth(700);
    bubble->setMinimumWidth(120);
    bubble->setStyleSheet(QString("background:%1;border:1px solid %2;").arg(fc::PANEL_BG, fc::ORANGE));

    auto* bvl = new QVBoxLayout(bubble);
    bvl->setContentsMargins(10, 10, 10, 10);
    bvl->setSpacing(0);

    auto* hdr = new QWidget;
    hdr->setStyleSheet(QString("border-bottom:1px solid %1;padding-bottom:6px;margin-bottom:8px;").arg(fc::BORDER));
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(0, 0, 0, 6);
    hhl->setSpacing(6);
    auto* ic = new QLabel(">");
    ic->setFixedSize(16, 16);
    ic->setAlignment(Qt::AlignCenter);
    ic->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;border:1px solid %1;%2").arg(fc::ORANGE, MF));
    hhl->addWidget(ic);
    auto* nm = new QLabel("FINCEPT AI");
    nm->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;letter-spacing:0.5px;border:none;%2").arg(fc::ORANGE, MF));
    hhl->addWidget(nm);
    hhl->addStretch();
    auto* st = new QLabel("PROCESSING...");
    st->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;letter-spacing:0.5px;border:none;%2").arg(fc::GRAY, MF));
    hhl->addWidget(st);
    bvl->addWidget(hdr);

    auto* body = new QTextEdit;
    body->setReadOnly(false);
    body->setFrameShape(QFrame::NoFrame);
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    body->document()->setDocumentMargin(0);
    body->setFixedHeight(24);
    body->setStyleSheet(QString("QTextEdit{background:transparent;color:%1;border:none;font-size:11px;%2}").arg(fc::WHITE, MF));
    connect(body->document(), &QTextDocument::contentsChanged, body, [body]() {
        QSize sz = body->document()->size().toSize();
        body->setFixedHeight(qMin(qMax(sz.height() + 4, 24), 800));
    });
    bvl->addWidget(body);

    al->addWidget(bubble);
    al->addStretch();
    messages_layout_->addWidget(align);
    return body;
}

// ============================================================================
// Helpers
// ============================================================================

void AiChatScreen::scroll_to_bottom() {
    QTimer::singleShot(50, this, [this]() {
        if (scroll_area_ && scroll_area_->verticalScrollBar())
            scroll_area_->verticalScrollBar()->setValue(scroll_area_->verticalScrollBar()->maximum());
    });
}

void AiChatScreen::set_input_enabled(bool enabled) {
    input_box_->setEnabled(enabled);
    send_btn_->setEnabled(enabled);
    send_btn_->setText(enabled ? "SEND" : "...");
    hdr_status_ai_->setText(enabled ? "AI READY" : "PROCESSING...");
    hdr_status_ai_->setStyleSheet(
        QString("color:%1;font-size:9px;font-weight:700;%2").arg(enabled ? fc::YELLOW : fc::GRAY, MF));
}

void AiChatScreen::update_stats() {
    hdr_msgs_lbl_->setText(QString::number(total_messages_));
    stats_msgs_lbl_->setText(QString::number(total_messages_));
    stats_tokens_lbl_->setText(QString::number(total_tokens_));
    if (!active_session_id_.isEmpty())
        hdr_session_lbl_->setText(active_session_id_.left(8));
}

void AiChatScreen::show_suggestions(bool show) {
    if (suggestions_panel_) suggestions_panel_->setVisible(show);
}

} // namespace fincept::screens
