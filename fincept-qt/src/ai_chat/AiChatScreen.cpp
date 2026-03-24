// AiChatScreen.cpp — Fincept AI Terminal chat, Obsidian design system

#include "ai_chat/AiChatScreen.h"

#include "ai_chat/LlmService.h"
#include "core/logging/Logger.h"
#include "mcp/McpService.h"
#include "storage/repositories/ChatRepository.h"

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

// ── Obsidian palette ─────────────────────────────────────────────────────────
namespace obs {
constexpr const char* BG_BASE = "#080808";
constexpr const char* BG_SURFACE = "#0a0a0a";
constexpr const char* BG_RAISED = "#111111";
constexpr const char* BG_HOVER = "#161616";
constexpr const char* BORDER_DIM = "#1a1a1a";
constexpr const char* BORDER_MED = "#222222";
constexpr const char* BORDER_BRT = "#333333";
constexpr const char* TEXT_PRI = "#e5e5e5";
constexpr const char* TEXT_SEC = "#808080";
constexpr const char* TEXT_TER = "#525252";
constexpr const char* TEXT_DIM = "#404040";
constexpr const char* AMBER = "#d97706";
constexpr const char* AMBER_DIM = "#78350f";
constexpr const char* POSITIVE = "#16a34a";
constexpr const char* NEGATIVE = "#dc2626";
constexpr const char* CYAN = "#0891b2";
constexpr const char* INFO = "#2563eb";
constexpr const char* ROW_ALT = "#0c0c0c";
} // namespace obs

static const QString MF = "font-family:'Consolas','Courier New',monospace;";

// ── Shared styles ────────────────────────────────────────────────────────────

static QFrame* sep() {
    auto* s = new QFrame;
    s->setFixedHeight(1);
    s->setStyleSheet(QString("background:%1;border:none;").arg(obs::BORDER_DIM));
    return s;
}

// ============================================================================
// Construction
// ============================================================================

AiChatScreen::AiChatScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    load_sessions();
}

void AiChatScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::finished_streaming, this,
            &AiChatScreen::on_streaming_done, Qt::UniqueConnection);
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::config_changed, this,
            &AiChatScreen::on_provider_changed, Qt::UniqueConnection);
    update_stats();
    if (clock_timer_)
        clock_timer_->start(1000);
}

void AiChatScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    if (clock_timer_)
        clock_timer_->stop();
}

// ============================================================================
// Build UI
// ============================================================================

void AiChatScreen::build_ui() {
    setStyleSheet(QString("background:%1;").arg(obs::BG_BASE));
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    build_sidebar();
    build_chat_area();

    root->addWidget(sidebar_);
    root->addWidget(chat_widget_, 1);
}

// ── Left sidebar ─────────────────────────────────────────────────────────────

void AiChatScreen::build_sidebar() {
    sidebar_ = new QWidget;
    sidebar_->setFixedWidth(260);
    sidebar_->setStyleSheet(QString("background:%1;border-right:1px solid %2;").arg(obs::BG_SURFACE, obs::BORDER_DIM));

    auto* vl = new QVBoxLayout(sidebar_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Header ───────────────────────────────────────────────────────────
    auto* hdr = new QWidget;
    hdr->setFixedHeight(34);
    hdr->setStyleSheet(QString("background:%1;").arg(obs::BG_RAISED));
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(12, 0, 12, 0);

    auto* title = new QLabel("CONVERSATIONS");
    title->setStyleSheet(
        QString("color:%1;font-size:12px;font-weight:700;letter-spacing:0.5px;%2").arg(obs::AMBER, MF));
    hhl->addWidget(title);
    hhl->addStretch();

    session_count_lbl_ = new QLabel("0");
    session_count_lbl_->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;%2").arg(obs::TEXT_SEC, MF));
    hhl->addWidget(session_count_lbl_);
    vl->addWidget(hdr);

    // ── New session button ───────────────────────────────────────────────
    auto* new_bar = new QWidget;
    new_bar->setStyleSheet(QString("background:%1;").arg(obs::BG_SURFACE));
    auto* nbl = new QHBoxLayout(new_bar);
    nbl->setContentsMargins(10, 8, 10, 8);

    new_btn_ = new QPushButton("+ NEW CONVERSATION");
    new_btn_->setFixedHeight(28);
    new_btn_->setStyleSheet(QString("QPushButton{background:rgba(217,119,6,0.1);color:%1;border:1px solid %2;"
                                    "font-size:12px;font-weight:700;%3}"
                                    "QPushButton:hover{background:%1;color:%4;}")
                                .arg(obs::AMBER, obs::AMBER_DIM, MF, obs::BG_BASE));
    connect(new_btn_, &QPushButton::clicked, this, &AiChatScreen::on_new_session);
    nbl->addWidget(new_btn_);
    vl->addWidget(new_bar);

    // ── Search ───────────────────────────────────────────────────────────
    auto* search_bar = new QWidget;
    search_bar->setStyleSheet(QString("background:%1;").arg(obs::BG_SURFACE));
    auto* shl = new QHBoxLayout(search_bar);
    shl->setContentsMargins(10, 0, 10, 8);

    search_edit_ = new QLineEdit;
    search_edit_->setPlaceholderText("Search conversations...");
    search_edit_->setFixedHeight(26);
    search_edit_->setStyleSheet(
        QString("QLineEdit{background:%1;color:%2;border:1px solid %3;"
                "padding:3px 8px;font-size:12px;%4}"
                "QLineEdit:focus{border-color:%5;}"
                "QLineEdit::placeholder{color:%6;}")
            .arg(obs::BG_BASE, obs::TEXT_PRI, obs::BORDER_DIM, MF, obs::BORDER_BRT, obs::TEXT_DIM));
    connect(search_edit_, &QLineEdit::textChanged, this, &AiChatScreen::on_search_changed);
    shl->addWidget(search_edit_);
    vl->addWidget(search_bar);

    // ── Session list ─────────────────────────────────────────────────────
    session_list_ = new QListWidget;
    session_list_->setStyleSheet(
        QString("QListWidget{background:transparent;border:none;color:%1;font-size:13px;%2}"
                "QListWidget::item{padding:10px 12px;border-left:2px solid transparent;}"
                "QListWidget::item:selected{background:rgba(217,119,6,0.06);border-left:2px solid %3;color:%4;}"
                "QListWidget::item:hover{background:%5;}")
            .arg(obs::TEXT_SEC, MF, obs::AMBER, obs::TEXT_PRI, obs::BG_HOVER));
    connect(session_list_, &QListWidget::currentRowChanged, this, &AiChatScreen::on_session_selected);
    vl->addWidget(session_list_, 1);

    // ── Delete ───────────────────────────────────────────────────────────
    auto* del_bar = new QWidget;
    del_bar->setStyleSheet(QString("background:%1;").arg(obs::BG_SURFACE));
    auto* dbl = new QHBoxLayout(del_bar);
    dbl->setContentsMargins(10, 4, 10, 4);

    delete_btn_ = new QPushButton("DELETE");
    delete_btn_->setEnabled(false);
    delete_btn_->setFixedHeight(24);
    delete_btn_->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                                       "font-size:11px;font-weight:700;%3}"
                                       "QPushButton:hover{background:rgba(220,38,38,0.1);border-color:%1;}"
                                       "QPushButton:disabled{color:%4;border-color:%5;}")
                                   .arg(obs::NEGATIVE, obs::BORDER_DIM, MF, obs::TEXT_DIM, obs::BORDER_DIM));
    connect(delete_btn_, &QPushButton::clicked, this, &AiChatScreen::on_delete_session);
    dbl->addWidget(delete_btn_);
    vl->addWidget(del_bar);

    // ── Stats footer ─────────────────────────────────────────────────────
    auto* stats = new QWidget;
    stats->setStyleSheet(QString("background:%1;border-top:1px solid %2;").arg(obs::BG_RAISED, obs::BORDER_DIM));
    auto* svl = new QVBoxLayout(stats);
    svl->setContentsMargins(12, 8, 12, 8);
    svl->setSpacing(4);

    // Provider info
    provider_lbl_ = new QLabel("NO PROVIDER");
    provider_lbl_->setStyleSheet(
        QString("color:%1;font-size:11px;font-weight:700;letter-spacing:0.5px;%2").arg(obs::AMBER, MF));
    svl->addWidget(provider_lbl_);

    svl->addWidget(sep());

    auto make_stat = [&](const QString& label) -> QLabel* {
        auto* row = new QWidget;
        row->setStyleSheet("background:transparent;");
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        auto* l = new QLabel(label);
        l->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;%2").arg(obs::TEXT_SEC, MF));
        rl->addWidget(l);
        auto* v = new QLabel("0");
        v->setAlignment(Qt::AlignRight);
        v->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;%2").arg(obs::CYAN, MF));
        rl->addWidget(v);
        svl->addWidget(row);
        return v;
    };
    stats_msgs_lbl_ = make_stat("MESSAGES");
    stats_tokens_lbl_ = make_stat("TOKENS");
    vl->addWidget(stats);
}

// ── Chat area ────────────────────────────────────────────────────────────────

void AiChatScreen::build_chat_area() {
    chat_widget_ = new QWidget;
    chat_widget_->setStyleSheet(QString("background:%1;").arg(obs::BG_BASE));
    auto* vl = new QVBoxLayout(chat_widget_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_header_bar());

    // Messages scroll area
    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setFrameShape(QFrame::NoFrame);
    scroll_area_->setStyleSheet(QString("QScrollArea{background:%1;border:none;}"
                                        "QScrollBar:vertical{background:transparent;width:6px;}"
                                        "QScrollBar::handle:vertical{background:%2;}"
                                        "QScrollBar::handle:vertical:hover{background:%3;}"
                                        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                                    .arg(obs::BG_BASE, obs::BORDER_MED, obs::BORDER_BRT));

    messages_container_ = new QWidget;
    messages_container_->setStyleSheet(QString("background:%1;").arg(obs::BG_BASE));
    messages_layout_ = new QVBoxLayout(messages_container_);
    messages_layout_->setContentsMargins(20, 16, 20, 16);
    messages_layout_->setSpacing(0);
    messages_layout_->addStretch();
    scroll_area_->setWidget(messages_container_);
    vl->addWidget(scroll_area_, 1);

    welcome_panel_ = build_welcome();
    vl->addWidget(welcome_panel_);
    vl->addWidget(build_input_area());
}

QWidget* AiChatScreen::build_header_bar() {
    auto* bar = new QWidget;
    bar->setFixedHeight(34);
    bar->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;").arg(obs::BG_RAISED, obs::BORDER_DIM));
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(10);

    auto* title = new QLabel("AI CHAT");
    title->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;letter-spacing:1px;%2").arg(obs::AMBER, MF));
    hl->addWidget(title);

    auto* div1 = new QLabel("|");
    div1->setStyleSheet(QString("color:%1;font-size:13px;%2").arg(obs::BORDER_BRT, MF));
    hl->addWidget(div1);

    hdr_status_lbl_ = new QLabel("READY");
    hdr_status_lbl_->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;%2").arg(obs::POSITIVE, MF));
    hl->addWidget(hdr_status_lbl_);

    hl->addStretch();

    hdr_session_lbl_ = new QLabel("");
    hdr_session_lbl_->setStyleSheet(QString("color:%1;font-size:12px;%2").arg(obs::TEXT_DIM, MF));
    hl->addWidget(hdr_session_lbl_);

    auto* div2 = new QLabel("|");
    div2->setStyleSheet(QString("color:%1;font-size:13px;%2").arg(obs::BORDER_BRT, MF));
    hl->addWidget(div2);

    hdr_clock_lbl_ = new QLabel("00:00:00");
    hdr_clock_lbl_->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;%2").arg(obs::TEXT_SEC, MF));
    hl->addWidget(hdr_clock_lbl_);

    clock_timer_ = new QTimer(this);
    connect(clock_timer_, &QTimer::timeout, this, &AiChatScreen::on_clock_tick);
    return bar;
}

// ── Welcome / suggestions panel ──────────────────────────────────────────────

QWidget* AiChatScreen::build_welcome() {
    auto* panel = new QWidget;
    panel->setStyleSheet(QString("background:%1;border-top:1px solid %2;").arg(obs::BG_SURFACE, obs::BORDER_DIM));
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(20, 16, 20, 16);
    vl->setSpacing(12);

    // Welcome header
    auto* welcome_title = new QLabel("FINCEPT AI ASSISTANT");
    welcome_title->setAlignment(Qt::AlignCenter);
    welcome_title->setStyleSheet(
        QString("color:%1;font-size:16px;font-weight:700;letter-spacing:1px;%2").arg(obs::AMBER, MF));
    vl->addWidget(welcome_title);

    auto* welcome_sub = new QLabel("Ask about markets, analyze portfolios, run analytics, or explore financial data");
    welcome_sub->setAlignment(Qt::AlignCenter);
    welcome_sub->setWordWrap(true);
    welcome_sub->setStyleSheet(QString("color:%1;font-size:13px;%2").arg(obs::TEXT_SEC, MF));
    vl->addWidget(welcome_sub);

    vl->addSpacing(4);

    // Suggestion grid — 2 rows x 3 cols
    auto* grid = new QGridLayout;
    grid->setSpacing(8);

    struct Sug {
        const char* cat;
        const char* text;
    };
    const Sug suggestions[] = {
        {"MARKETS", "Show me today's top market movers"},  {"NEWS", "Summarize the latest financial news"},
        {"PORTFOLIO", "Analyze my portfolio performance"}, {"ANALYTICS", "Calculate valuation for AAPL"},
        {"ECONOMICS", "Current GDP and inflation data"},   {"RESEARCH", "Tech sector market trends"},
    };

    for (int i = 0; i < 6; ++i) {
        auto* btn = new QPushButton;
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton{background:%1;border:1px solid %2;padding:12px;text-align:left;%3}"
                                   "QPushButton:hover{background:%4;border-color:%5;}")
                               .arg(obs::BG_RAISED, obs::BORDER_DIM, MF, obs::BG_HOVER, obs::AMBER_DIM));

        auto* bl = new QVBoxLayout(btn);
        bl->setContentsMargins(0, 0, 0, 0);
        bl->setSpacing(4);

        auto* cat = new QLabel(suggestions[i].cat);
        cat->setStyleSheet(
            QString("color:%1;font-size:11px;font-weight:700;letter-spacing:0.5px;%2").arg(obs::AMBER, MF));
        cat->setAttribute(Qt::WA_TransparentForMouseEvents);
        bl->addWidget(cat);

        auto* desc = new QLabel(suggestions[i].text);
        desc->setStyleSheet(QString("color:%1;font-size:12px;%2").arg(obs::TEXT_PRI, MF));
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

    // Hint
    auto* hint =
        new QLabel("Or type your own question below. The AI can access market data, run Python analytics, and more.");
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    hint->setStyleSheet(QString("color:%1;font-size:11px;%2").arg(obs::TEXT_DIM, MF));
    vl->addWidget(hint);

    return panel;
}

// ── Input area ───────────────────────────────────────────────────────────────

QWidget* AiChatScreen::build_input_area() {
    auto* bar = new QWidget;
    bar->setStyleSheet(QString("background:%1;border-top:1px solid %2;").arg(obs::BG_RAISED, obs::BORDER_DIM));
    auto* vl = new QVBoxLayout(bar);
    vl->setContentsMargins(16, 10, 16, 10);
    vl->setSpacing(6);

    // Input container
    auto* input_row = new QWidget;
    input_row->setStyleSheet(QString("background:%1;border:1px solid %2;").arg(obs::BG_BASE, obs::BORDER_DIM));
    auto* irl = new QHBoxLayout(input_row);
    irl->setContentsMargins(10, 8, 10, 8);
    irl->setSpacing(10);

    auto* prompt_sym = new QLabel(">");
    prompt_sym->setStyleSheet(QString("color:%1;font-size:14px;font-weight:700;%2").arg(obs::AMBER, MF));
    prompt_sym->setFixedWidth(14);
    irl->addWidget(prompt_sym, 0, Qt::AlignTop);

    input_box_ = new QPlainTextEdit;
    input_box_->setPlaceholderText("Ask anything about markets, data, or analytics...");
    input_box_->setFixedHeight(24);
    input_box_->setStyleSheet(QString("QPlainTextEdit{background:transparent;color:%1;border:none;font-size:14px;%2}"
                                      "QPlainTextEdit[readOnly=\"true\"]{color:%3;}")
                                  .arg(obs::TEXT_PRI, MF, obs::TEXT_DIM));
    input_box_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    input_box_->installEventFilter(this);
    connect(input_box_, &QPlainTextEdit::textChanged, this, [this]() {
        int lines = input_box_->document()->blockCount();
        input_box_->setFixedHeight(qMin(qMax(lines, 1), 5) * 22 + 4);
    });
    irl->addWidget(input_box_, 1);

    send_btn_ = new QPushButton("SEND");
    send_btn_->setFixedSize(70, 30);
    send_btn_->setCursor(Qt::PointingHandCursor);
    send_btn_->setStyleSheet(
        QString("QPushButton{background:rgba(217,119,6,0.1);color:%1;border:1px solid %2;"
                "font-size:12px;font-weight:700;letter-spacing:0.5px;%3}"
                "QPushButton:hover{background:%1;color:%4;}"
                "QPushButton:disabled{background:%5;color:%6;border-color:%7;}")
            .arg(obs::AMBER, obs::AMBER_DIM, MF, obs::BG_BASE, obs::BG_RAISED, obs::TEXT_DIM, obs::BORDER_DIM));
    connect(send_btn_, &QPushButton::clicked, this, &AiChatScreen::on_send);
    irl->addWidget(send_btn_, 0, Qt::AlignBottom);
    vl->addWidget(input_row);

    // Help text
    auto* help = new QHBoxLayout;
    help->setContentsMargins(0, 0, 0, 0);
    auto* lh = new QLabel("ENTER to send  |  SHIFT+ENTER new line  |  CTRL+N new chat");
    lh->setStyleSheet(QString("color:%1;font-size:11px;%2").arg(obs::TEXT_DIM, MF));
    help->addWidget(lh);
    help->addStretch();
    auto* rh = new QLabel("FINCEPT AI");
    rh->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;%2").arg(obs::TEXT_DIM, MF));
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
            if (ke->modifiers() & Qt::ShiftModifier)
                return false;
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
    session_count_lbl_->setText(QString::number(session_list_->count()));

    if (!active_session_id_.isEmpty()) {
        for (int i = 0; i < session_list_->count(); ++i) {
            if (session_list_->item(i)->data(Qt::UserRole).toString() == active_session_id_) {
                session_list_->setCurrentRow(i);
                return;
            }
        }
    }
    if (session_list_->count() > 0)
        session_list_->setCurrentRow(0);
    else
        create_new_session();
}

void AiChatScreen::create_new_session() {
    auto result = ChatRepository::instance().create_session("Chat", ai_chat::LlmService::instance().active_provider(),
                                                            ai_chat::LlmService::instance().active_model());
    if (result.is_err())
        return;
    active_session_id_ = result.value().id;
    history_.clear();
    total_tokens_ = 0;
    total_messages_ = 0;
    load_sessions();
    show_welcome(true);
}

void AiChatScreen::load_messages(const QString& session_id) {
    while (messages_layout_->count() > 1) {
        QLayoutItem* item = messages_layout_->takeAt(1);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    history_.clear();
    auto result = ChatRepository::instance().get_messages(session_id);
    if (result.is_err())
        return;
    const auto& msgs = result.value();
    total_messages_ = static_cast<int>(msgs.size());
    for (const auto& msg : msgs) {
        add_message_bubble(msg.role, msg.content, msg.timestamp);
        if (msg.role != "system")
            history_.push_back({msg.role, msg.content});
    }
    show_welcome(msgs.empty());
    scroll_to_bottom();
    update_stats();
}

// ============================================================================
// Slots
// ============================================================================

void AiChatScreen::on_new_session() {
    create_new_session();
}

void AiChatScreen::on_session_selected(int row) {
    if (row < 0 || row >= session_list_->count())
        return;
    QString id = session_list_->item(row)->data(Qt::UserRole).toString();
    if (id == active_session_id_)
        return;
    active_session_id_ = id;
    delete_btn_->setEnabled(true);
    hdr_session_lbl_->setText(id.left(8));
    load_messages(id);
}

void AiChatScreen::on_delete_session() {
    if (active_session_id_.isEmpty())
        return;
    ChatRepository::instance().delete_session(active_session_id_);
    active_session_id_.clear();
    history_.clear();
    load_sessions();
}

void AiChatScreen::on_send() {
    if (streaming_)
        return;
    QString text = input_box_->toPlainText().trimmed();
    if (text.isEmpty())
        return;
    if (active_session_id_.isEmpty())
        create_new_session();
    if (!ai_chat::LlmService::instance().is_configured()) {
        add_message_bubble("system",
                           "No LLM provider configured. Go to Settings > LLM Configuration to set up a provider.");
        return;
    }
    input_box_->clear();
    input_box_->setFixedHeight(24);
    set_input_enabled(false);
    streaming_ = true;
    show_welcome(false);
    add_message_bubble("user", text);
    total_messages_++;
    ChatRepository::instance().add_message(active_session_id_, "user", text,
                                           ai_chat::LlmService::instance().active_provider(),
                                           ai_chat::LlmService::instance().active_model());
    history_.push_back({"user", text});
    streaming_bubble_ = add_streaming_bubble();
    scroll_to_bottom();

    std::vector<ai_chat::ConversationMessage> hist_copy = history_;
    QString provider = ai_chat::LlmService::instance().active_provider();
    if (ai_chat::provider_supports_streaming(provider)) {
        QPointer<AiChatScreen> self = this;
        ai_chat::LlmService::instance().chat_streaming(text, hist_copy, [self](const QString& chunk, bool done) {
            QMetaObject::invokeMethod(
                qApp,
                [self, chunk, done]() {
                    if (self)
                        self->on_stream_chunk(chunk, done);
                },
                Qt::QueuedConnection);
        });
    } else {
        QPointer<AiChatScreen> self = this;
        QtConcurrent::run([self, text, hist_copy]() {
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
    if (!streaming_bubble_)
        return;
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
            streaming_bubble_->setPlainText("Error: " + response.error);
            streaming_bubble_->setStyleSheet(
                QString("QTextEdit{background:transparent;color:%1;border:none;font-size:13px;%2}")
                    .arg(obs::NEGATIVE, MF));
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

void AiChatScreen::on_provider_changed() {
    update_stats();
}

// ============================================================================
// Message bubbles — Obsidian design
// ============================================================================

void AiChatScreen::add_message_bubble(const QString& role, const QString& content, const QString& timestamp) {
    bool is_user = (role == "user");
    bool is_system = (role == "system");

    QString ts = timestamp.isEmpty() ? QDateTime::currentDateTime().toString("HH:mm:ss")
                                     : QDateTime::fromString(timestamp, Qt::ISODate).toString("HH:mm:ss");

    // Outer alignment wrapper
    auto* align = new QWidget;
    align->setStyleSheet("background:transparent;");
    auto* al = new QHBoxLayout(align);
    al->setContentsMargins(0, 0, 0, 0);

    if (is_user)
        al->addStretch();

    // Bubble container
    auto* bubble = new QWidget;
    bubble->setMaximumWidth(680);
    bubble->setMinimumWidth(120);

    if (is_system) {
        bubble->setStyleSheet(QString("background:rgba(220,38,38,0.05);border:1px solid %1;").arg(obs::BORDER_DIM));
    } else if (is_user) {
        bubble->setStyleSheet(QString("background:%1;border:1px solid %2;").arg(obs::BG_SURFACE, obs::BORDER_DIM));
    } else {
        bubble->setStyleSheet(QString("background:%1;border-left:2px solid %2;border-top:1px solid %3;"
                                      "border-right:1px solid %3;border-bottom:1px solid %3;")
                                  .arg(obs::BG_SURFACE, obs::AMBER, obs::BORDER_DIM));
    }

    auto* bvl = new QVBoxLayout(bubble);
    bvl->setContentsMargins(12, 8, 12, 8);
    bvl->setSpacing(6);

    // Header row: role + timestamp
    auto* hdr = new QWidget;
    hdr->setStyleSheet("background:transparent;");
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(0, 0, 0, 0);
    hhl->setSpacing(8);

    if (is_system) {
        auto* nm = new QLabel("SYSTEM");
        nm->setStyleSheet(
            QString("color:%1;font-size:11px;font-weight:700;letter-spacing:0.5px;%2").arg(obs::NEGATIVE, MF));
        hhl->addWidget(nm);
    } else if (is_user) {
        auto* nm = new QLabel("YOU");
        nm->setStyleSheet(
            QString("color:%1;font-size:11px;font-weight:700;letter-spacing:0.5px;%2").arg(obs::TEXT_SEC, MF));
        hhl->addWidget(nm);
    } else {
        auto* nm = new QLabel("FINCEPT AI");
        nm->setStyleSheet(
            QString("color:%1;font-size:11px;font-weight:700;letter-spacing:0.5px;%2").arg(obs::AMBER, MF));
        hhl->addWidget(nm);
    }

    hhl->addStretch();

    auto* tl = new QLabel(ts);
    tl->setStyleSheet(QString("color:%1;font-size:11px;%2").arg(obs::TEXT_DIM, MF));
    hhl->addWidget(tl);
    bvl->addWidget(hdr);

    // Body — use QLabel for correct auto-sizing with word wrap
    auto* body = new QLabel(content);
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->setStyleSheet(QString("color:%1;font-size:13px;background:transparent;%2").arg(obs::TEXT_PRI, MF));
    bvl->addWidget(body);

    al->addWidget(bubble);
    if (!is_user)
        al->addStretch();
    messages_layout_->addWidget(align);

    // Spacer
    auto* spacer = new QWidget;
    spacer->setFixedHeight(8);
    spacer->setStyleSheet("background:transparent;");
    messages_layout_->addWidget(spacer);
}

QTextEdit* AiChatScreen::add_streaming_bubble() {
    auto* align = new QWidget;
    align->setStyleSheet("background:transparent;");
    auto* al = new QHBoxLayout(align);
    al->setContentsMargins(0, 0, 0, 0);

    auto* bubble = new QWidget;
    bubble->setMaximumWidth(680);
    bubble->setMinimumWidth(120);
    bubble->setStyleSheet(QString("background:%1;border-left:2px solid %2;border-top:1px solid %3;"
                                  "border-right:1px solid %3;border-bottom:1px solid %3;")
                              .arg(obs::BG_SURFACE, obs::AMBER, obs::BORDER_DIM));

    auto* bvl = new QVBoxLayout(bubble);
    bvl->setContentsMargins(12, 8, 12, 8);
    bvl->setSpacing(6);

    // Header
    auto* hdr = new QWidget;
    hdr->setStyleSheet("background:transparent;");
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(0, 0, 0, 0);
    hhl->setSpacing(8);

    auto* nm = new QLabel("FINCEPT AI");
    nm->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;letter-spacing:0.5px;%2").arg(obs::AMBER, MF));
    hhl->addWidget(nm);
    hhl->addStretch();

    auto* st = new QLabel("THINKING...");
    st->setStyleSheet(
        QString("color:%1;font-size:11px;font-weight:700;letter-spacing:0.5px;%2").arg(obs::TEXT_DIM, MF));
    hhl->addWidget(st);
    bvl->addWidget(hdr);

    // Body — streaming, needs QTextEdit for incremental appends
    auto* body = new QTextEdit;
    body->setReadOnly(false);
    body->setFrameShape(QFrame::NoFrame);
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    body->document()->setDocumentMargin(0);
    body->setMinimumHeight(24);
    body->setStyleSheet(
        QString("QTextEdit{background:transparent;color:%1;border:none;font-size:13px;%2}").arg(obs::TEXT_PRI, MF));
    body->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(body->document(), &QTextDocument::contentsChanged, body, [body]() {
        int h = static_cast<int>(body->document()->size().height()) + 10;
        body->setMinimumHeight(qMax(h, 24));
        body->setMaximumHeight(qMax(h, 24));
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
    hdr_status_lbl_->setText(enabled ? "READY" : "PROCESSING");
    hdr_status_lbl_->setStyleSheet(
        QString("color:%1;font-size:12px;font-weight:700;%2").arg(enabled ? obs::POSITIVE : obs::TEXT_DIM, MF));
}

void AiChatScreen::update_stats() {
    stats_msgs_lbl_->setText(QString::number(total_messages_));
    stats_tokens_lbl_->setText(QString::number(total_tokens_));
    if (!active_session_id_.isEmpty())
        hdr_session_lbl_->setText(active_session_id_.left(8));

    // Update provider label
    auto& llm = ai_chat::LlmService::instance();
    if (llm.is_configured()) {
        QString prov = llm.active_provider().toUpper();
        QString model = llm.active_model();
        if (model.length() > 20)
            model = model.left(18) + "..";
        provider_lbl_->setText(prov + " | " + model);
        provider_lbl_->setStyleSheet(
            QString("color:%1;font-size:11px;font-weight:700;letter-spacing:0.5px;%2").arg(obs::AMBER, MF));
    } else {
        provider_lbl_->setText("NO PROVIDER CONFIGURED");
        provider_lbl_->setStyleSheet(
            QString("color:%1;font-size:11px;font-weight:700;letter-spacing:0.5px;%2").arg(obs::NEGATIVE, MF));
    }
}

void AiChatScreen::show_welcome(bool show) {
    if (welcome_panel_)
        welcome_panel_->setVisible(show);
}

} // namespace fincept::screens
