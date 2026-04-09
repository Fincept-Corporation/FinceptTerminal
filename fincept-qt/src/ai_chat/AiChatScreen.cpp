// AiChatScreen.cpp — Fincept AI Chat, Obsidian design system

#include "ai_chat/AiChatScreen.h"

#include "ai_chat/LlmService.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "mcp/McpService.h"
#include "storage/repositories/ChatRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <memory>

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDialog>
#include <QFile>
#include <QPalette>
#include <QFileDialog>
#include <QFileInfo>
#include <QTextStream>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTimer>
#include <QToolButton>
#include <QtConcurrent/QtConcurrent>

namespace fincept::screens {

static constexpr const char* TAG = "AiChatScreen";

namespace fnt = fincept::ui::fonts;
namespace col = fincept::ui::colors;

// ── Style helpers ─────────────────────────────────────────────────────────────

static QString bubble_style(const QString& role) {
    if (role == "user")
        return "background:rgba(120,53,15,0.45);border:1px solid rgba(217,119,6,0.28);"
               "border-radius:0px;padding:10px 14px;";
    if (role == "system")
        return "background:rgba(50,12,12,0.85);border:1px solid rgba(220,38,38,0.22);"
               "border-radius:0px;padding:10px 14px;";
    return QString("background:%1;border:1px solid %2;border-radius:0px;padding:10px 14px;")
        .arg(col::BG_SURFACE(), col::BORDER_DIM());
}

static QString body_color(const QString& role) {
    if (role == "user")   return "#fff7ed";
    if (role == "system") return "#fee2e2";
    return col::TEXT_PRIMARY();
}

// Default stylesheet for rendered markdown inside QTextEdit / QLabel bubbles.
// Comprehensive styling for LLM responses: paragraphs, lists, headings,
// code blocks, blockquotes, tables, and links.
static QString markdown_css(const QString& text_color) {
    return QString(
        "body { color: %1; line-height: 1.5; }"
        "p { margin-top: 6px; margin-bottom: 6px; }"
        "ul, ol { margin-top: 4px; margin-bottom: 4px; padding-left: 20px; }"
        "li { margin-top: 3px; margin-bottom: 3px; }"
        "h1 { margin-top: 14px; margin-bottom: 6px; color: %2; font-size: 18px; font-weight: 700; }"
        "h2 { margin-top: 12px; margin-bottom: 5px; color: %2; font-size: 16px; font-weight: 700; }"
        "h3 { margin-top: 10px; margin-bottom: 4px; color: %2; font-size: 15px; font-weight: 600; }"
        "h4 { margin-top: 8px; margin-bottom: 4px; color: %2; font-size: 14px; font-weight: 600; }"
        "hr { margin-top: 10px; margin-bottom: 10px; border: none; "
        "     border-top: 1px solid %3; }"
        "a { color: %2; text-decoration: underline; }"
        "code { background: %4; color: %2; padding: 1px 4px; "
        "       font-family: 'Consolas', 'Courier New', monospace; font-size: 13px; }"
        "pre { background: %4; border: 1px solid %3; padding: 10px 12px; "
        "      margin-top: 6px; margin-bottom: 6px; "
        "      font-family: 'Consolas', 'Courier New', monospace; font-size: 13px; }"
        "blockquote { border-left: 3px solid %2; padding-left: 12px; "
        "             margin-top: 6px; margin-bottom: 6px; color: %5; }"
        "table { border-collapse: collapse; margin-top: 6px; margin-bottom: 6px; }"
        "th { background: %4; border: 1px solid %3; padding: 4px 8px; "
        "     font-weight: 600; color: %2; }"
        "td { border: 1px solid %3; padding: 4px 8px; }"
        "strong { color: %1; font-weight: 700; }"
        "em { font-style: italic; }"
    ).arg(text_color, col::AMBER(), col::BORDER_MED(), col::BG_RAISED(), col::TEXT_SECONDARY());
}

// Override Qt's default blue palette on QTextEdit to match Obsidian theme.
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

static QString generate_session_title() {
    static const QStringList prefixes = {"Amber","Apex","Atlas","Echo","Flux","Nova","Slate","Vector"};
    static const QStringList nouns    = {"Brief","Drift","Focus","Ledger","Macro","Pulse","Signal","Tape"};
    const int pi = QRandomGenerator::global()->bounded(prefixes.size());
    const int ni = QRandomGenerator::global()->bounded(nouns.size());
    const QString sfx = QString::number(QRandomGenerator::global()->bounded(0x10000), 16)
                            .rightJustified(4,'0').toUpper();
    return QString("%1 %2 %3").arg(prefixes[pi], nouns[ni], sfx);
}

static QString display_session_title(const ChatSession& s) {
    if (!s.title.trimmed().isEmpty() && s.title.trimmed().toLower() != "chat")
        return s.title;
    return QString("Session %1").arg(s.id.left(4).toUpper());
}

static QString display_session_meta(const ChatSession& s) {
    QDateTime dt = QDateTime::fromString(s.updated_at, Qt::ISODate);
    if (!dt.isValid())
        dt = QDateTime::fromString(s.updated_at, "yyyy-MM-dd HH:mm:ss");
    const QString stamp = dt.isValid() ? dt.toString("MMM d · hh:mm") : "";
    return s.message_count > 0
        ? QString("%1 msg%2%3").arg(s.message_count)
              .arg(stamp.isEmpty() ? "" : "  ·  ")
              .arg(stamp)
        : stamp;
}

// ── Constructor ───────────────────────────────────────────────────────────────

AiChatScreen::AiChatScreen(QWidget* parent) : QWidget(parent) {
    typing_timer_ = new QTimer(this);
    typing_timer_->setInterval(400);
    connect(typing_timer_, &QTimer::timeout, this, &AiChatScreen::on_typing_indicator_tick);

    build_ui();
    load_sessions();

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed,
            this, [this](const ui::ThemeTokens&) { refresh_theme(); });
}

void AiChatScreen::refresh_theme() {
    // Root
    setStyleSheet(QString("background:%1;").arg(col::BG_BASE()));

    // Sidebar
    if (sidebar_)
        sidebar_->setStyleSheet(
            QString("background:%1;border-right:1px solid %2;").arg(col::BG_SURFACE(), col::BORDER_DIM()));

    // Chat area
    if (chat_widget_)
        chat_widget_->setStyleSheet(QString("background:%1;").arg(col::BG_BASE()));
    if (messages_container_)
        messages_container_->setStyleSheet(QString("background:%1;").arg(col::BG_BASE()));

    // Scroll area
    if (scroll_area_)
        scroll_area_->setStyleSheet(
            QString("QScrollArea{background:%1;border:none;}"
                    "QScrollBar:vertical{background:transparent;width:5px;margin:2px;}"
                    "QScrollBar::handle:vertical{background:%2;border-radius:0px;min-height:30px;}"
                    "QScrollBar::handle:vertical:hover{background:%3;}"
                    "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
            .arg(col::BG_BASE(), col::BORDER_MED(), col::BORDER_BRIGHT()));

    // Input box
    if (input_box_)
        input_box_->setStyleSheet(
            QString("QPlainTextEdit{background:%1;color:%2;border:1px solid %3;"
                    "border-radius:0px;padding:8px 14px;font-size:%4px;}"
                    "QPlainTextEdit:focus{border-color:%5;}")
            .arg(col::BG_BASE(), col::TEXT_PRIMARY(), col::BORDER_MED())
            .arg(fnt::BODY).arg(col::AMBER()));

    // Send button
    if (send_btn_)
        send_btn_->setStyleSheet(
            QString("QPushButton{background:%1;color:%2;border:none;border-radius:0px;"
                    "font-size:%3px;font-weight:700;}"
                    "QPushButton:hover:enabled{background:%4;}"
                    "QPushButton:disabled{background:%5;color:%6;}")
            .arg(col::AMBER(), col::BG_BASE()).arg(fnt::SMALL)
            .arg(col::ORANGE(), col::BG_RAISED(), col::TEXT_DIM()));

    // Attach button
    if (attach_btn_)
        attach_btn_->setStyleSheet(
            QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                    "border-radius:0px;font-size:20px;font-weight:700;}"
                    "QPushButton:hover{background:rgba(217,119,6,0.15);border-color:%3;}")
            .arg(col::TEXT_PRIMARY(), col::BORDER_MED(), col::AMBER()));

    // Header bar elements
    if (hdr_session_lbl_)
        hdr_session_lbl_->setStyleSheet(
            QString("color:%1;font-size:%2px;font-weight:600;")
            .arg(col::TEXT_PRIMARY()).arg(fnt::BODY));
    if (hdr_tokens_lbl_)
        hdr_tokens_lbl_->setStyleSheet(
            QString("color:%1;font-size:%2px;").arg(col::TEXT_DIM()).arg(fnt::TINY));

    // Update provider/model stats to pick up new colors
    update_stats();
}

void AiChatScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::finished_streaming,
            this, &AiChatScreen::on_streaming_done, Qt::UniqueConnection);
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::config_changed,
            this, &AiChatScreen::on_provider_changed, Qt::UniqueConnection);
    update_stats();
}

void AiChatScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
}

void AiChatScreen::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
}

// ── Build UI ──────────────────────────────────────────────────────────────────

void AiChatScreen::build_ui() {
    setStyleSheet(QString("background:%1;").arg(col::BG_BASE()));
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);
    build_sidebar();
    build_chat_area();
    root->addWidget(sidebar_);
    root->addWidget(chat_widget_, 1);
}

// ── Sidebar ───────────────────────────────────────────────────────────────────

void AiChatScreen::build_sidebar() {
    sidebar_ = new QWidget;
    sidebar_->setFixedWidth(280);
    sidebar_->setStyleSheet(
        QString("background:%1;border-right:1px solid %2;").arg(col::BG_SURFACE(), col::BORDER_DIM()));

    auto* vl = new QVBoxLayout(sidebar_);
    vl->setContentsMargins(0,0,0,0);
    vl->setSpacing(0);

    // ── Header ───────────────────────────────────────────────────────────
    auto* hdr = new QWidget;
    hdr->setFixedHeight(60);
    hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(col::BG_BASE(), col::BORDER_DIM()));
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(14, 0, 10, 0);
    hhl->setSpacing(6);

    auto* icon = new QLabel("⬡");
    icon->setStyleSheet(QString("color:%1;font-size:22px;").arg(col::TEXT_PRIMARY()));
    hhl->addWidget(icon);

    auto* title = new QLabel("Fincept AI");
    title->setStyleSheet(
        QString("color:%1;font-size:%2px;font-weight:700;").arg(col::TEXT_PRIMARY()).arg(fnt::BODY));
    hhl->addWidget(title, 1);

    new_btn_ = new QPushButton("＋");
    new_btn_->setFixedSize(34, 34);
    new_btn_->setCursor(Qt::PointingHandCursor);
    new_btn_->setToolTip("New Chat  (Ctrl+N)");
    new_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                "border-radius:0px;font-size:20px;font-weight:700;}"
                "QPushButton:hover{background:%3;color:%1;}"
                "QPushButton:disabled{color:%4;border-color:%5;}")
            .arg(col::TEXT_PRIMARY(), col::BORDER_MED(), col::BG_HOVER(), col::TEXT_DIM(), col::BORDER_DIM()));
    connect(new_btn_, &QPushButton::clicked, this, &AiChatScreen::on_new_session);
    hhl->addWidget(new_btn_);
    vl->addWidget(hdr);

    // ── Search ───────────────────────────────────────────────────────────
    auto* search_wrap = new QWidget;
    search_wrap->setStyleSheet(QString("background:%1;").arg(col::BG_SURFACE()));
    auto* swl = new QHBoxLayout(search_wrap);
    swl->setContentsMargins(10, 8, 10, 8);

    search_edit_ = new QLineEdit;
    search_edit_->setPlaceholderText("Search sessions...");
    search_edit_->setFixedHeight(30);
    search_edit_->setStyleSheet(
        QString("QLineEdit{background:%1;color:%2;border:1px solid %3;"
                "border-radius:0px;padding:2px 10px;font-size:%4px;}"
                "QLineEdit:focus{border-color:%5;}")
            .arg(col::BG_BASE(), col::TEXT_PRIMARY(), col::BORDER_MED())
            .arg(fnt::SMALL).arg(col::AMBER()));
    connect(search_edit_, &QLineEdit::textChanged, this, &AiChatScreen::on_search_changed);
    swl->addWidget(search_edit_);
    vl->addWidget(search_wrap);

    // ── Session list ─────────────────────────────────────────────────────
    session_list_ = new QListWidget;
    session_list_->setWordWrap(false);
    session_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    session_list_->setStyleSheet(
        QString("QListWidget{background:%1;border:none;outline:none;}"
                "QListWidget::item{padding:10px 14px;border-bottom:1px solid %2;"
                "color:%3;font-size:%4px;}"
                "QListWidget::item:selected{background:rgba(217,119,6,0.10);"
                "border-left:3px solid %5;color:%6;}"
                "QListWidget::item:hover:!selected{background:rgba(255,255,255,0.03);}"
                "QScrollBar:vertical{background:transparent;width:4px;}"
                "QScrollBar::handle:vertical{background:%7;border-radius:0px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
            .arg(col::BG_SURFACE(), col::BORDER_DIM(), col::TEXT_SECONDARY())
            .arg(fnt::SMALL)
            .arg(col::AMBER(), col::TEXT_PRIMARY(), col::BORDER_MED()));
    connect(session_list_, &QListWidget::currentRowChanged,
            this, &AiChatScreen::on_session_selected);
    vl->addWidget(session_list_, 1);

    // ── Session actions ──────────────────────────────────────────────────
    auto* actions = new QWidget;
    actions->setFixedHeight(42);
    actions->setStyleSheet(
        QString("background:%1;border-top:1px solid %2;").arg(col::BG_BASE(), col::BORDER_DIM()));
    auto* al = new QHBoxLayout(actions);
    al->setContentsMargins(10, 0, 10, 0);
    al->setSpacing(6);

    rename_btn_ = new QPushButton("Rename");
    rename_btn_->setEnabled(false);
    rename_btn_->setFixedHeight(26);
    rename_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                "border-radius:0px;font-size:%3px;padding:0 8px;}"
                "QPushButton:hover:enabled{background:%2;color:%4;}"
                "QPushButton:disabled{color:%5;border-color:%6;}")
            .arg(col::TEXT_SECONDARY(), col::BORDER_MED()).arg(fnt::SMALL)
            .arg(col::BG_BASE(), col::TEXT_DIM(), col::BORDER_DIM()));
    connect(rename_btn_, &QPushButton::clicked, this, &AiChatScreen::on_rename_session);
    al->addWidget(rename_btn_);

    al->addStretch();

    delete_btn_ = new QPushButton("Delete");
    delete_btn_->setEnabled(false);
    delete_btn_->setFixedHeight(26);
    delete_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid rgba(220,38,38,0.28);"
                "border-radius:0px;font-size:%2px;padding:0 8px;}"
                "QPushButton:hover:enabled{background:%1;color:%3;border-color:%1;}"
                "QPushButton:disabled{color:%4;border-color:%5;}")
            .arg(col::NEGATIVE()).arg(fnt::SMALL)
            .arg(col::BG_BASE(), col::TEXT_DIM(), col::BORDER_DIM()));
    connect(delete_btn_, &QPushButton::clicked, this, &AiChatScreen::on_delete_session);
    al->addWidget(delete_btn_);
    vl->addWidget(actions);

    // ── Provider/Model footer ────────────────────────────────────────────
    auto* footer = new QWidget;
    footer->setFixedHeight(50);
    footer->setStyleSheet(
        QString("background:%1;border-top:1px solid %2;").arg(col::BG_BASE(), col::BORDER_DIM()));
    auto* fl = new QVBoxLayout(footer);
    fl->setContentsMargins(14, 7, 14, 7);
    fl->setSpacing(2);

    provider_lbl_ = new QLabel("No provider");
    provider_lbl_->setStyleSheet(
        QString("color:%1;font-size:%2px;font-weight:600;")
            .arg(col::AMBER()).arg(fnt::SMALL));
    provider_lbl_->setToolTip("Active LLM Provider");

    model_lbl_ = new QLabel("No model");
    model_lbl_->setStyleSheet(
        QString("color:%1;font-size:%2px;").arg(col::TEXT_SECONDARY()).arg(fnt::TINY));
    model_lbl_->setToolTip("Active Model — change in Settings > LLM Configuration");

    fl->addWidget(provider_lbl_);
    fl->addWidget(model_lbl_);
    vl->addWidget(footer);
}

// ── Chat area ─────────────────────────────────────────────────────────────────

void AiChatScreen::build_chat_area() {
    chat_widget_ = new QWidget;
    chat_widget_->setStyleSheet(QString("background:%1;").arg(col::BG_BASE()));
    auto* vl = new QVBoxLayout(chat_widget_);
    vl->setContentsMargins(0,0,0,0);
    vl->setSpacing(0);

    vl->addWidget(build_header_bar());

    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setFrameShape(QFrame::NoFrame);
    scroll_area_->setStyleSheet(
        QString("QScrollArea{background:%1;border:none;}"
                "QScrollBar:vertical{background:transparent;width:5px;margin:2px;}"
                "QScrollBar::handle:vertical{background:%2;border-radius:0px;min-height:30px;}"
                "QScrollBar::handle:vertical:hover{background:%3;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
            .arg(col::BG_BASE(), col::BORDER_MED(), col::BORDER_BRIGHT()));

    messages_container_ = new QWidget;
    messages_container_->setStyleSheet(QString("background:%1;").arg(col::BG_BASE()));
    messages_layout_ = new QVBoxLayout(messages_container_);
    messages_layout_->setContentsMargins(32, 24, 32, 16);
    messages_layout_->setSpacing(18);
    messages_layout_->addStretch();
    scroll_area_->setWidget(messages_container_);
    vl->addWidget(scroll_area_, 1);

    // Typing indicator sits above input, inside chat area
    vl->addWidget(build_typing_indicator());
    vl->addWidget(build_input_area());
}

QWidget* AiChatScreen::build_header_bar() {
    auto* bar = new QWidget;
    bar->setFixedHeight(52);
    bar->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;")
            .arg(col::BG_RAISED(), col::BORDER_DIM()));
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(20, 0, 16, 0);
    hl->setSpacing(10);

    // Status dot
    hdr_status_dot_ = new QLabel;
    hdr_status_dot_->setFixedSize(8, 8);
    hdr_status_dot_->setStyleSheet(
        QString("background:%1;border-radius:0px;").arg(col::POSITIVE()));
    hl->addWidget(hdr_status_dot_);

    // Session name
    hdr_session_lbl_ = new QLabel("New Conversation");
    hdr_session_lbl_->setStyleSheet(
        QString("color:%1;font-size:%2px;font-weight:600;")
            .arg(col::TEXT_PRIMARY()).arg(fnt::BODY));
    hl->addWidget(hdr_session_lbl_);

    hl->addStretch();

    // Token count
    hdr_tokens_lbl_ = new QLabel;
    hdr_tokens_lbl_->setStyleSheet(
        QString("color:%1;font-size:%2px;").arg(col::TEXT_DIM()).arg(fnt::TINY));
    hl->addWidget(hdr_tokens_lbl_);

    // Divider
    auto* div = new QFrame;
    div->setFrameShape(QFrame::VLine);
    div->setFixedWidth(1);
    div->setStyleSheet(QString("background:%1;").arg(col::BORDER_DIM()));
    hl->addWidget(div);

    // Active model pill
    hdr_model_lbl_ = new QLabel("No model");
    hdr_model_lbl_->setStyleSheet(
        QString("color:%1;font-size:%2px;background:%3;border:1px solid %4;"
                "border-radius:0px;padding:2px 8px;")
            .arg(col::TEXT_SECONDARY()).arg(fnt::TINY)
            .arg(col::BG_BASE(), col::BORDER_MED()));
    hdr_model_lbl_->setToolTip("Active model — change in Settings > LLM Configuration");
    hl->addWidget(hdr_model_lbl_);

    // Status text
    hdr_status_lbl_ = new QLabel("Ready");
    hdr_status_lbl_->setFixedWidth(64);
    hdr_status_lbl_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hdr_status_lbl_->setStyleSheet(
        QString("color:%1;font-size:%2px;font-weight:600;")
            .arg(col::POSITIVE()).arg(fnt::SMALL));
    hl->addWidget(hdr_status_lbl_);

    return bar;
}

// ── Typing indicator ──────────────────────────────────────────────────────────

QWidget* AiChatScreen::build_typing_indicator() {
    typing_indicator_ = new QWidget;
    typing_indicator_->setFixedHeight(28);
    typing_indicator_->setStyleSheet("background:transparent;");
    auto* hl = new QHBoxLayout(typing_indicator_);
    hl->setContentsMargins(36, 0, 0, 0);

    typing_dots_lbl_ = new QLabel("AI is thinking");
    typing_dots_lbl_->setStyleSheet(
        QString("color:%1;font-size:%2px;font-style:italic;")
            .arg(col::TEXT_SECONDARY()).arg(fnt::SMALL));
    hl->addWidget(typing_dots_lbl_);
    hl->addStretch();

    typing_indicator_->hide();
    return typing_indicator_;
}

// ── Welcome panel ─────────────────────────────────────────────────────────────

QWidget* AiChatScreen::build_welcome() {
    auto* panel = new QWidget;
    panel->setStyleSheet(QString("background:%1;").arg(col::BG_BASE()));
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(48, 36, 48, 28);
    vl->setSpacing(20);

    auto* heading = new QLabel("How can I help you?");
    heading->setAlignment(Qt::AlignCenter);
    heading->setStyleSheet(
        QString("color:%1;font-size:%2px;font-weight:700;")
            .arg(col::TEXT_PRIMARY()).arg(fnt::TITLE));
    vl->addWidget(heading);

    auto* sub = new QLabel(
        "Ask about markets, portfolios, macro data, or any financial topic.\n"
        "Conversations are saved automatically.");
    sub->setAlignment(Qt::AlignCenter);
    sub->setWordWrap(true);
    sub->setStyleSheet(
        QString("color:%1;font-size:%2px;").arg(col::TEXT_SECONDARY()).arg(fnt::SMALL));
    vl->addWidget(sub);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);

    struct Sug { const char* cat; const char* cat_color; const char* text; };
    const Sug suggestions[] = {
        {"Markets",   col::CYAN(),     "Show me today's top market movers"},
        {"News",      col::AMBER(),    "Summarize the latest financial news"},
        {"Portfolio", col::POSITIVE(), "Analyze my portfolio performance"},
        {"Analytics", col::AMBER(),    "Calculate valuation for AAPL"},
        {"Economics", col::CYAN(),     "Current GDP and inflation data"},
        {"Research",  col::POSITIVE(), "Tech sector market trends"},
    };

    for (int i = 0; i < 6; ++i) {
        auto* btn = new QPushButton;
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            QString("QPushButton{background:%1;border:1px solid %2;border-radius:0px;"
                    "padding:12px 14px;text-align:left;}"
                    "QPushButton:hover{background:%3;border-color:%4;}")
                .arg(col::BG_RAISED(), col::BORDER_DIM(), col::BG_HOVER(), col::BORDER_BRIGHT()));

        auto* bl = new QVBoxLayout(btn);
        bl->setContentsMargins(0,0,0,0);
        bl->setSpacing(4);

        auto* cat = new QLabel(suggestions[i].cat);
        cat->setStyleSheet(
            QString("color:%1;font-size:%2px;font-weight:700;background:transparent;")
                .arg(suggestions[i].cat_color).arg(fnt::TINY));
        cat->setAttribute(Qt::WA_TransparentForMouseEvents);
        bl->addWidget(cat);

        auto* desc = new QLabel(suggestions[i].text);
        desc->setStyleSheet(
            QString("color:%1;font-size:%2px;background:transparent;")
                .arg(col::TEXT_PRIMARY()).arg(fnt::SMALL));
        desc->setWordWrap(true);
        desc->setAttribute(Qt::WA_TransparentForMouseEvents);
        bl->addWidget(desc);

        const QString prompt = suggestions[i].text;
        connect(btn, &QPushButton::clicked, this, [this, prompt]() {
            input_box_->setPlainText(prompt);
            on_send();
        });
        grid->addWidget(btn, i / 3, i % 3);
    }
    vl->addLayout(grid);
    return panel;
}

// ── Input area ────────────────────────────────────────────────────────────────

QWidget* AiChatScreen::build_input_area() {
    auto* bar = new QWidget;
    bar->setStyleSheet(
        QString("background:%1;border-top:1px solid %2;")
            .arg(col::BG_RAISED(), col::BORDER_DIM()));
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 12, 16, 12);
    hl->setSpacing(10);

    input_box_ = new QPlainTextEdit;
    input_box_->setPlaceholderText("Message Fincept AI...");
    input_box_->setFixedHeight(44);
    input_box_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    input_box_->setStyleSheet(
        QString("QPlainTextEdit{background:%1;color:%2;border:1px solid %3;"
                "border-radius:0px;padding:8px 14px;font-size:%4px;}"
                "QPlainTextEdit:focus{border-color:%5;}")
            .arg(col::BG_BASE(), col::TEXT_PRIMARY(), col::BORDER_MED())
            .arg(fnt::BODY).arg(col::AMBER()));
    input_box_->installEventFilter(this);
    connect(input_box_, &QPlainTextEdit::textChanged, this, [this]() {
        const int lines = input_box_->document()->blockCount();
        input_box_->setFixedHeight(qMin(qMax(lines, 1), 6) * 24 + 20);
    });
    hl->addWidget(input_box_, 1);

    // Attach file button
    attach_btn_ = new QPushButton("⊕");
    attach_btn_->setFixedSize(44, 44);
    attach_btn_->setCursor(Qt::PointingHandCursor);
    attach_btn_->setToolTip("Attach a file to this message");
    attach_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                "border-radius:0px;font-size:20px;font-weight:700;}"
                "QPushButton:hover{background:rgba(217,119,6,0.15);border-color:%1;}"
                "QPushButton[active=\"true\"]{background:rgba(217,119,6,0.2);"
                "border-color:%1;color:%1;}")
            .arg(col::TEXT_PRIMARY(), col::BORDER_MED()));
    connect(attach_btn_, &QPushButton::clicked, this, &AiChatScreen::on_attach_file);
    hl->addWidget(attach_btn_);

    // Badge showing attached file name (hidden by default)
    attach_badge_ = new QLabel;
    attach_badge_->setVisible(false);
    attach_badge_->setMaximumWidth(160);
    attach_badge_->setStyleSheet(
        QString("color:%1;font-size:10px;background:rgba(217,119,6,0.12);"
                "border:1px solid #78350f;border-radius:0px;padding:2px 6px;%2")
            .arg(col::AMBER(), "font-family:'Consolas',monospace;"));
    hl->addWidget(attach_badge_);

    send_btn_ = new QPushButton("Send  ↑");
    send_btn_->setFixedSize(82, 44);
    send_btn_->setCursor(Qt::PointingHandCursor);
    send_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:none;border-radius:0px;"
                "font-size:%3px;font-weight:700;}"
                "QPushButton:hover:enabled{background:%4;}"
                "QPushButton:disabled{background:%5;color:%6;}")
            .arg(col::AMBER(), col::BG_BASE()).arg(fnt::SMALL)
            .arg(col::ORANGE(), col::BG_RAISED(), col::TEXT_DIM()));
    connect(send_btn_, &QPushButton::clicked, this, &AiChatScreen::on_send);
    hl->addWidget(send_btn_);

    return bar;
}

// ── Events ────────────────────────────────────────────────────────────────────

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

void AiChatScreen::on_search_changed(const QString& text) {
    const QString filter = text.trimmed().toLower();
    for (int i = 0; i < session_list_->count(); ++i) {
        auto* item = session_list_->item(i);
        item->setHidden(!filter.isEmpty() && !item->text().toLower().contains(filter));
    }
}

// ── Typing indicator slot ─────────────────────────────────────────────────────

void AiChatScreen::on_typing_indicator_tick() {
    static const QStringList states = {
        "AI is thinking", "AI is thinking·", "AI is thinking··", "AI is thinking···"
    };
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

void AiChatScreen::load_sessions() {
    session_list_->blockSignals(true);
    session_list_->clear();
    auto result = ChatRepository::instance().list_sessions();
    if (result.is_ok()) {
        for (const auto& s : result.value()) {
            const QString meta  = display_session_meta(s);
            const QString title = display_session_title(s);
            const QString label = title + (meta.isEmpty() ? "" : "\n" + meta);
            auto* item = new QListWidgetItem(label);
            item->setData(Qt::UserRole, s.id);
            item->setToolTip(title + (meta.isEmpty() ? "" : "\n" + meta));
            item->setSizeHint(QSize(0, 52));
            session_list_->addItem(item);
        }
    }
    session_list_->blockSignals(false);

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
    if (streaming_) return;
    const QString title = generate_session_title();
    auto result = ChatRepository::instance().create_session(
        title,
        ai_chat::LlmService::instance().active_provider(),
        ai_chat::LlmService::instance().active_model());
    if (result.is_err()) {
        LOG_ERROR(TAG, "create_new_session failed: " + QString::fromStdString(result.error()));
        return;
    }
    active_session_id_    = result.value().id;
    active_session_title_ = result.value().title;
    history_.clear();
    total_tokens_   = 0;
    total_messages_ = 0;
    clear_messages();
    load_sessions();
    hdr_session_lbl_->setText(active_session_title_);
    delete_btn_->setEnabled(true);
    rename_btn_->setEnabled(true);
    update_stats();
    show_welcome(true);
}

void AiChatScreen::load_messages(const QString& session_id) {
    clear_messages();
    history_.clear();
    auto result = ChatRepository::instance().get_messages(session_id);
    if (result.is_err()) return;
    const auto& msgs = result.value();
    total_messages_ = static_cast<int>(msgs.size());
    total_tokens_   = 0;
    for (const auto& msg : msgs) {
        add_message_bubble(msg.role, msg.content, msg.timestamp);
        if (msg.role != "system")
            history_.push_back({msg.role, msg.content});
        total_tokens_ += msg.tokens_used;
    }
    show_welcome(msgs.empty());
    scroll_to_bottom();
    update_stats();
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void AiChatScreen::on_new_session() { create_new_session(); }

void AiChatScreen::on_session_selected(int row) {
    if (streaming_ || row < 0 || row >= session_list_->count()) return;
    auto* item = session_list_->item(row);
    active_session_id_    = item->data(Qt::UserRole).toString();
    active_session_title_ = item->text().split('\n').first();
    delete_btn_->setEnabled(true);
    rename_btn_->setEnabled(true);
    hdr_session_lbl_->setText(active_session_title_);
    load_messages(active_session_id_);
    ScreenStateManager::instance().notify_changed(this);
}

void AiChatScreen::on_rename_session() {
    if (active_session_id_.isEmpty()) return;
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, "Rename Session", "Session name:",
        QLineEdit::Normal, active_session_title_, &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    ChatRepository::instance().update_session_title(active_session_id_, name.trimmed());
    active_session_title_ = name.trimmed();
    hdr_session_lbl_->setText(active_session_title_);
    load_sessions();
}

void AiChatScreen::on_delete_session() {
    if (streaming_ || active_session_id_.isEmpty()) return;
    ChatRepository::instance().delete_session(active_session_id_);
    active_session_id_.clear();
    active_session_title_.clear();
    history_.clear();
    clear_messages();
    delete_btn_->setEnabled(false);
    rename_btn_->setEnabled(false);
    load_sessions();
}

void AiChatScreen::on_attach_file() {
    // Let user pick from File Manager index or browse disk
    QStringList paths = QFileDialog::getOpenFileNames(
        this, "Attach File to Message", QString(),
        "All Files (*);;Text Files (*.txt *.md *.csv *.json);;Notebooks (*.ipynb);;PDF (*.pdf)");
    if (paths.isEmpty()) return;

    attached_file_path_ = paths.first(); // single attach for now
    QFileInfo fi(attached_file_path_);
    if (attach_badge_) {
        attach_badge_->setText("⊕ " + fi.fileName());
        attach_badge_->setVisible(true);
    }
    if (attach_btn_)
        attach_btn_->setProperty("active", true);
}

void AiChatScreen::on_send() {
    if (streaming_) return;
    const QString raw_text = input_box_->toPlainText().trimmed();
    if (raw_text.isEmpty() && attached_file_path_.isEmpty()) return;
    if (active_session_id_.isEmpty()) create_new_session();
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
            text = QString("[Attached file: %1]\n\n%2\n\n---\n\n%3")
                       .arg(fi.fileName(), file_content, raw_text);
        }
        // Reset attach state
        attached_file_path_.clear();
        if (attach_badge_) attach_badge_->setVisible(false);
        if (attach_btn_)   attach_btn_->setProperty("active", false);
    }

    input_box_->clear();
    input_box_->setFixedHeight(44);
    set_input_enabled(false);
    streaming_ = true;
    show_welcome(false);
    // Display only raw_text in bubble (not full file dump)
    add_message_bubble("user", raw_text.isEmpty()
        ? "[File attached — see context]" : raw_text);
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
        ai_chat::LlmService::instance().chat_streaming(text, hist_copy,
            [self, first_chunk](const QString& chunk, bool done) {
                QMetaObject::invokeMethod(qApp, [self, chunk, done, first_chunk]() {
                    if (!self) return;
                    // On first chunk: hide typing indicator, show streaming bubble
                    if (*first_chunk && !chunk.isEmpty()) {
                        *first_chunk = false;
                        self->show_typing(false);
                        self->streaming_bubble_ = self->add_streaming_bubble();
                        self->scroll_to_bottom();
                    }
                    self->on_stream_chunk(chunk, done);
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
    // Snapshot QPointer to local — prevents TOCTOU between null-check and use.
    QTextEdit* bubble = streaming_bubble_;
    if (!bubble) return;

    // Tool-call clear sentinel: reset bubble content (removes partial XML)
    if (chunk.startsWith("\x01__TOOL_CALL_CLEAR__")) {
        bubble->clear();
        bubble->setPlainText("Calling tool...");
        scroll_to_bottom();
        return;
    }

    if (!chunk.isEmpty()) {
        // If bubble shows the "Calling tool..." placeholder, replace it
        if (bubble->toPlainText() == "Calling tool...") {
            bubble->setPlainText(chunk);
        } else {
            bubble->moveCursor(QTextCursor::End);
            bubble->insertPlainText(chunk);
        }
        scroll_to_bottom();
    }
    Q_UNUSED(done)
}

void AiChatScreen::on_streaming_done(ai_chat::LlmResponse response) {
    streaming_ = false;
    show_typing(false);

    // If non-streaming path: create bubble now with full content
    if (!streaming_bubble_ && response.success && !response.content.isEmpty()) {
        streaming_bubble_ = add_streaming_bubble();
    }

    set_input_enabled(true);

    if (!response.success) {
        if (streaming_bubble_)
            streaming_bubble_->setPlainText("Error: " + response.error);
        streaming_bubble_ = nullptr;
        return;
    }

    const QString content = response.content;
    if (streaming_bubble_) {
        // Get final text (either from response or what was streamed)
        QString final_text = streaming_bubble_->toPlainText();
        if (!content.isEmpty() && final_text.isEmpty())
            final_text = content;
        // Re-render as markdown so **bold**, - lists, code blocks, etc. display properly
        if (!final_text.isEmpty()) {
            streaming_bubble_->document()->setDefaultStyleSheet(
                markdown_css(col::TEXT_PRIMARY()));
            streaming_bubble_->setMarkdown(final_text);
        }
        streaming_bubble_->setReadOnly(true);

        // Show the copy button now that streaming is done
        auto* copy_obj = streaming_bubble_->property("copy_btn").value<QObject*>();
        if (auto* copy_btn = qobject_cast<QPushButton*>(copy_obj))
            copy_btn->show();

        streaming_bubble_ = nullptr;
    }
    if (!content.isEmpty()) {
        history_.push_back({"assistant", content});
        total_messages_++;
        total_tokens_ += response.total_tokens;
        update_stats();
        ChatRepository::instance().add_message(active_session_id_, "assistant", content,
            ai_chat::LlmService::instance().active_provider(),
            ai_chat::LlmService::instance().active_model(),
            response.total_tokens);
    }
    scroll_to_bottom();
    input_box_->setFocus();
}

void AiChatScreen::on_provider_changed() { update_stats(); }

// ── Message bubbles ───────────────────────────────────────────────────────────

void AiChatScreen::add_message_bubble(const QString& role, const QString& content,
                                      const QString& timestamp) {
    const bool is_user   = (role == "user");
    const bool is_system = (role == "system");

    const QString ts = [&]() -> QString {
        QDateTime dt = QDateTime::fromString(timestamp, Qt::ISODate);
        if (!dt.isValid()) dt = QDateTime::fromString(timestamp, "yyyy-MM-dd HH:mm:ss");
        if (!dt.isValid() && !timestamp.isEmpty()) return timestamp;
        return (dt.isValid() ? dt : QDateTime::currentDateTime()).toString("HH:mm");
    }();

    auto* row = new QWidget;
    row->setStyleSheet("background:transparent;");
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0,0,0,0);
    rl->setSpacing(0);

    if (is_user) rl->addStretch();

    auto* col_widget = new QWidget;
    col_widget->setStyleSheet("background:transparent;");
    col_widget->setMaximumWidth(is_user ? 560 : 680);
    auto* cvl = new QVBoxLayout(col_widget);
    cvl->setContentsMargins(0,0,0,0);
    cvl->setSpacing(4);

    // Role label
    auto* role_lbl = new QLabel(is_user ? "You" : (is_system ? "System" : "AI"));
    role_lbl->setAlignment(is_user ? Qt::AlignRight : Qt::AlignLeft);
    role_lbl->setStyleSheet(
        QString("color:%1;font-size:%2px;font-weight:600;background:transparent;")
            .arg(is_user ? col::AMBER() : (is_system ? col::NEGATIVE() : col::AMBER()))
            .arg(fnt::TINY));
    cvl->addWidget(role_lbl);

    // Bubble
    auto* bubble = new QFrame;
    bubble->setStyleSheet(bubble_style(role));
    auto* bvl = new QVBoxLayout(bubble);
    bvl->setContentsMargins(0,0,0,0);

    auto* body = new QTextEdit;
    body->setReadOnly(true);
    body->setFrameShape(QFrame::NoFrame);
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    body->document()->setDocumentMargin(4);
    body->document()->setDefaultStyleSheet(markdown_css(body_color(role)));
    apply_obsidian_palette(body);
    body->setMarkdown(content);
    body->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->setStyleSheet(
        QString("QTextEdit{background:transparent;color:%1;border:none;font-size:%2px;}")
            .arg(body_color(role)).arg(fnt::BODY));
    // Size to content
    body->document()->setTextWidth(is_user ? 520 : 640);
    const int doc_h = static_cast<int>(body->document()->size().height());
    body->setFixedHeight(qMax(doc_h + 16, 32));
    bvl->addWidget(body);
    cvl->addWidget(bubble);

    // Footer row: timestamp + copy button
    auto* footer = new QWidget;
    footer->setStyleSheet("background:transparent;");
    auto* fhl = new QHBoxLayout(footer);
    fhl->setContentsMargins(0, 0, 0, 0);
    fhl->setSpacing(6);

    auto* time_lbl = new QLabel(ts);
    time_lbl->setStyleSheet(
        QString("color:%1;font-size:%2px;background:transparent;")
            .arg(col::TEXT_DIM()).arg(fnt::TINY));
    fhl->addWidget(time_lbl);

    if (!is_user && !is_system) {
        fhl->addStretch();
        auto* copy_btn = new QPushButton("Copy");
        copy_btn->setFixedHeight(20);
        copy_btn->setCursor(Qt::PointingHandCursor);
        copy_btn->setStyleSheet(
            QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                    "border-radius:0px;padding:0 8px;font-size:%3px;}"
                    "QPushButton:hover{background:%2;color:%4;}")
                .arg(col::TEXT_DIM(), col::BORDER_MED()).arg(fnt::TINY).arg(col::TEXT_PRIMARY()));
        QString plain = content;
        connect(copy_btn, &QPushButton::clicked, this, [plain, copy_btn]() {
            QApplication::clipboard()->setText(plain);
            copy_btn->setText("Copied!");
            QTimer::singleShot(1500, copy_btn, [copy_btn]() { copy_btn->setText("Copy"); });
        });
        fhl->addWidget(copy_btn);
    }

    if (is_user) fhl->insertStretch(0);
    cvl->addWidget(footer);

    rl->addWidget(col_widget);
    if (!is_user) rl->addStretch();

    messages_layout_->insertWidget(messages_layout_->count() - 1, row);
}

QTextEdit* AiChatScreen::add_streaming_bubble() {
    auto* row = new QWidget;
    row->setStyleSheet("background:transparent;");
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0,0,0,0);

    auto* col_widget = new QWidget;
    col_widget->setStyleSheet("background:transparent;");
    col_widget->setMaximumWidth(680);
    auto* cvl = new QVBoxLayout(col_widget);
    cvl->setContentsMargins(0,0,0,0);
    cvl->setSpacing(4);

    auto* role_lbl = new QLabel("AI");
    role_lbl->setAlignment(Qt::AlignLeft);
    role_lbl->setStyleSheet(
        QString("color:%1;font-size:%2px;font-weight:600;background:transparent;")
            .arg(col::AMBER()).arg(fnt::TINY));
    cvl->addWidget(role_lbl);

    auto* bubble = new QFrame;
    bubble->setStyleSheet(bubble_style("assistant"));
    auto* bvl = new QVBoxLayout(bubble);
    bvl->setContentsMargins(0,0,0,0);

    auto* body = new QTextEdit;
    body->setReadOnly(false);
    body->setFrameShape(QFrame::NoFrame);
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    body->document()->setDocumentMargin(4);
    body->document()->setDefaultStyleSheet(markdown_css(col::TEXT_PRIMARY()));
    apply_obsidian_palette(body);
    body->setMinimumHeight(32);
    body->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    body->setStyleSheet(
        QString("QTextEdit{background:transparent;color:%1;border:none;font-size:%2px;}")
            .arg(col::TEXT_PRIMARY()).arg(fnt::BODY));
    // Grow height to fit content as chunks stream in
    connect(body->document(), &QTextDocument::contentsChanged, body, [body, bubble, row]() {
        body->document()->setTextWidth(body->viewport()->width() > 0
                                       ? body->viewport()->width()
                                       : 640);
        const int doc_h = static_cast<int>(body->document()->size().height());
        const int new_h = qMax(doc_h + 16, 32);
        body->setFixedHeight(new_h);
        bubble->adjustSize();
        row->adjustSize();
    });
    bvl->addWidget(body);
    cvl->addWidget(bubble);

    // Copy button — hidden during streaming, shown when done
    auto* copy_btn = new QPushButton("Copy");
    copy_btn->setFixedHeight(20);
    copy_btn->setCursor(Qt::PointingHandCursor);
    copy_btn->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                "border-radius:0px;padding:0 8px;font-size:%3px;}"
                "QPushButton:hover{background:%2;color:%4;}")
            .arg(col::TEXT_DIM(), col::BORDER_MED()).arg(fnt::TINY).arg(col::TEXT_PRIMARY()));
    copy_btn->hide();
    connect(copy_btn, &QPushButton::clicked, this, [body, copy_btn]() {
        QApplication::clipboard()->setText(body->toPlainText());
        copy_btn->setText("Copied!");
        QTimer::singleShot(1500, copy_btn, [copy_btn]() { copy_btn->setText("Copy"); });
    });
    auto* footer = new QWidget;
    footer->setStyleSheet("background:transparent;");
    auto* fhl = new QHBoxLayout(footer);
    fhl->setContentsMargins(0, 0, 0, 0);
    fhl->addStretch();
    fhl->addWidget(copy_btn);
    cvl->addWidget(footer);

    // Store copy_btn so on_streaming_done can show it
    body->setProperty("copy_btn", QVariant::fromValue(static_cast<QObject*>(copy_btn)));

    rl->addWidget(col_widget);
    rl->addStretch();

    messages_layout_->insertWidget(messages_layout_->count() - 1, row);
    return body;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void AiChatScreen::clear_messages() {
    streaming_bubble_.clear();
    while (messages_layout_->count() > 1) {
        QLayoutItem* item = messages_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}

void AiChatScreen::scroll_to_bottom() {
    QTimer::singleShot(50, this, [this]() {
        if (scroll_area_ && scroll_area_->verticalScrollBar())
            scroll_area_->verticalScrollBar()->setValue(
                scroll_area_->verticalScrollBar()->maximum());
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
    hdr_status_dot_->setStyleSheet(
        QString("background:%1;border-radius:0px;").arg(status_color));
    hdr_status_lbl_->setText(enabled ? "Ready" : "Streaming");
    hdr_status_lbl_->setStyleSheet(
        QString("color:%1;font-size:%2px;font-weight:600;")
            .arg(status_color).arg(fnt::SMALL));
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
        hdr_tokens_lbl_->setText(
            total_tokens_ < 1000
                ? QString("%1 tokens").arg(total_tokens_)
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
        const QString model_raw    = llm.active_model();
        // For fincept, don't expose internal model name
        const QString model_display = is_fincept ? "Fincept LLM" : model_raw;
        QString model_short = model_display;
        if (model_short.length() > 24) model_short = model_short.left(22) + "..";

        // Sidebar footer
        provider_lbl_->setText(prov_display);
        provider_lbl_->setStyleSheet(
            QString("color:%1;font-size:%2px;font-weight:600;").arg(col::AMBER()).arg(fnt::SMALL));
        model_lbl_->setText(is_fincept ? "Managed by Fincept" : model_short);
        model_lbl_->setToolTip(is_fincept ? "Fincept LLM — managed AI service"
                                          : model_raw);
        model_lbl_->setStyleSheet(
            QString("color:%1;font-size:%2px;").arg(col::TEXT_SECONDARY()).arg(fnt::TINY));

        // Header model pill — show "Provider / Model" for clarity
        if (is_fincept) {
            hdr_model_lbl_->setText("Fincept LLM");
            hdr_model_lbl_->setToolTip("Fincept managed AI service\n\nChange in Settings > LLM Configuration");
        } else {
            hdr_model_lbl_->setText(provider_raw.left(1).toUpper() + provider_raw.mid(1) + " / " + model_short);
            hdr_model_lbl_->setToolTip("Provider: " + prov_display + "\nModel: " + model_raw
                                       + "\n\nChange in Settings > LLM Configuration");
        }
    } else {
        provider_lbl_->setText("No provider");
        provider_lbl_->setStyleSheet(
            QString("color:%1;font-size:%2px;font-weight:600;").arg(col::NEGATIVE()).arg(fnt::SMALL));
        model_lbl_->setText("Configure in Settings");
        model_lbl_->setStyleSheet(
            QString("color:%1;font-size:%2px;").arg(col::TEXT_DIM()).arg(fnt::TINY));
        hdr_model_lbl_->setText("No model");
    }
}

void AiChatScreen::show_welcome(bool show) {
    if (welcome_panel_) welcome_panel_->setVisible(show);
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap AiChatScreen::save_state() const {
    return {
        {"session_id", active_session_id_},
    };
}

void AiChatScreen::restore_state(const QVariantMap& state) {
    const QString sid = state.value("session_id").toString();
    if (sid.isEmpty()) return;

    // Find and select the matching row in the session list
    for (int i = 0; i < session_list_->count(); ++i) {
        auto* item = session_list_->item(i);
        if (item && item->data(Qt::UserRole).toString() == sid) {
            session_list_->setCurrentRow(i);
            on_session_selected(i);
            return;
        }
    }
    // Session not found (may have been deleted) — leave default selection
}

} // namespace fincept::screens
