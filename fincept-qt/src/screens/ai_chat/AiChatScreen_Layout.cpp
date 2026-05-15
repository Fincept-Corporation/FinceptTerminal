// src/screens/ai_chat/AiChatScreen_Layout.cpp
//
// UI construction: build_ui, build_sidebar, build_chat_area, build_header_bar,
// build_typing_indicator, build_welcome, build_input_area.
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

void AiChatScreen::build_ui() {
    setStyleSheet(QString("background:%1;").arg(col::BG_BASE()));
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    build_sidebar();
    build_chat_area();
    root->addWidget(sidebar_);
    root->addWidget(chat_widget_, 1);
}

// ── Sidebar ───────────────────────────────────────────────────────────────────

void AiChatScreen::build_sidebar() {
    sidebar_ = new QWidget;
    // Use min/max width pair instead of setFixedWidth so the collapse
    // animation can drive maximumWidth between 0 and kSidebarExpandedWidth.
    sidebar_->setMinimumWidth(0);
    sidebar_->setMaximumWidth(kSidebarExpandedWidth);
    sidebar_->setStyleSheet(
        QString("background:%1;border-right:1px solid %2;").arg(col::BG_SURFACE(), col::BORDER_DIM()));

    auto* vl = new QVBoxLayout(sidebar_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Header ───────────────────────────────────────────────────────────
    auto* hdr = new QWidget;
    hdr->setFixedHeight(60);
    hdr->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;").arg(col::BG_BASE(), col::BORDER_DIM()));
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(14, 0, 10, 0);
    hhl->setSpacing(6);

    auto* icon = new QLabel("⬡");
    icon->setStyleSheet(QString("color:%1;font-size:22px;").arg(col::TEXT_PRIMARY()));
    hhl->addWidget(icon);

    auto* title = new QLabel("Fincept AI");
    title->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:700;").arg(col::TEXT_PRIMARY()).arg(fnt::BODY));
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
    search_edit_->setStyleSheet(QString("QLineEdit{background:%1;color:%2;border:1px solid %3;"
                                        "border-radius:0px;padding:2px 10px;font-size:%4px;}"
                                        "QLineEdit:focus{border-color:%5;}")
                                    .arg(col::BG_BASE(), col::TEXT_PRIMARY(), col::BORDER_MED())
                                    .arg(fnt::SMALL)
                                    .arg(col::AMBER()));
    connect(search_edit_, &QLineEdit::textChanged, this, &AiChatScreen::on_search_changed);
    swl->addWidget(search_edit_);
    vl->addWidget(search_wrap);

    // ── Session list ─────────────────────────────────────────────────────
    session_list_ = new QListWidget;
    session_list_->setWordWrap(false);
    session_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    session_list_->setStyleSheet(QString("QListWidget{background:%1;border:none;outline:none;}"
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
    connect(session_list_, &QListWidget::currentRowChanged, this, &AiChatScreen::on_session_selected);
    vl->addWidget(session_list_, 1);

    // ── Session actions ──────────────────────────────────────────────────
    auto* actions = new QWidget;
    actions->setFixedHeight(42);
    actions->setStyleSheet(QString("background:%1;border-top:1px solid %2;").arg(col::BG_BASE(), col::BORDER_DIM()));
    auto* al = new QHBoxLayout(actions);
    al->setContentsMargins(10, 0, 10, 0);
    al->setSpacing(6);

    rename_btn_ = new QPushButton("Rename");
    rename_btn_->setEnabled(false);
    rename_btn_->setFixedHeight(26);
    rename_btn_->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                                       "border-radius:0px;font-size:%3px;padding:0 8px;}"
                                       "QPushButton:hover:enabled{background:%2;color:%4;}"
                                       "QPushButton:disabled{color:%5;border-color:%6;}")
                                   .arg(col::TEXT_SECONDARY(), col::BORDER_MED())
                                   .arg(fnt::SMALL)
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
            .arg(col::NEGATIVE())
            .arg(fnt::SMALL)
            .arg(col::BG_BASE(), col::TEXT_DIM(), col::BORDER_DIM()));
    connect(delete_btn_, &QPushButton::clicked, this, &AiChatScreen::on_delete_session);
    al->addWidget(delete_btn_);
    vl->addWidget(actions);

    // ── Provider/Model footer ────────────────────────────────────────────
    auto* footer = new QWidget;
    footer->setFixedHeight(50);
    footer->setStyleSheet(QString("background:%1;border-top:1px solid %2;").arg(col::BG_BASE(), col::BORDER_DIM()));
    auto* fl = new QVBoxLayout(footer);
    fl->setContentsMargins(14, 7, 14, 7);
    fl->setSpacing(2);

    provider_lbl_ = new QLabel("No provider");
    provider_lbl_->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:600;").arg(col::AMBER()).arg(fnt::SMALL));
    provider_lbl_->setToolTip("Active LLM Provider");

    model_lbl_ = new QLabel("No model");
    model_lbl_->setStyleSheet(QString("color:%1;font-size:%2px;").arg(col::TEXT_SECONDARY()).arg(fnt::TINY));
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
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_header_bar());

    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setFrameShape(QFrame::NoFrame);
    scroll_area_->setStyleSheet(QString("QScrollArea{background:%1;border:none;}"
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

    // Debounced persistence when the user scrolls through chat history.
    if (auto* vbar = scroll_area_->verticalScrollBar())
        connect(vbar, &QScrollBar::valueChanged, this,
                [this](int) { ScreenStateManager::instance().notify_changed(this); });

    // Typing indicator sits above input, inside chat area
    vl->addWidget(build_typing_indicator());
    vl->addWidget(build_input_area());
}

QWidget* AiChatScreen::build_header_bar() {
    auto* bar = new QWidget;
    bar->setFixedHeight(52);
    bar->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;").arg(col::BG_RAISED(), col::BORDER_DIM()));
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(8, 0, 16, 0);
    hl->setSpacing(10);

    // Sidebar collapse toggle. Lives at the left edge of the chat header so
    // it remains visible (and can re-expand the sidebar) even when the
    // sidebar is collapsed to width 0.
    sidebar_toggle_btn_ = new QPushButton("‹");
    sidebar_toggle_btn_->setFixedSize(28, 28);
    sidebar_toggle_btn_->setCursor(Qt::PointingHandCursor);
    sidebar_toggle_btn_->setToolTip("Collapse sidebar  (Ctrl+B)");
    sidebar_toggle_btn_->setShortcut(QKeySequence("Ctrl+B"));
    sidebar_toggle_btn_->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                                               "border-radius:0px;font-size:18px;font-weight:700;padding:0;}"
                                               "QPushButton:hover{background:%3;color:%4;border-color:%4;}")
                                           .arg(col::TEXT_SECONDARY(), col::BORDER_DIM(), col::BG_HOVER(), col::AMBER()));
    connect(sidebar_toggle_btn_, &QPushButton::clicked, this, &AiChatScreen::on_toggle_sidebar);
    hl->addWidget(sidebar_toggle_btn_);

    // Status dot
    hdr_status_dot_ = new QLabel;
    hdr_status_dot_->setFixedSize(8, 8);
    hdr_status_dot_->setStyleSheet(QString("background:%1;border-radius:0px;").arg(col::POSITIVE()));
    hl->addWidget(hdr_status_dot_);

    // Session name
    hdr_session_lbl_ = new QLabel("New Conversation");
    hdr_session_lbl_->setStyleSheet(
        QString("color:%1;font-size:%2px;font-weight:600;").arg(col::TEXT_PRIMARY()).arg(fnt::BODY));
    hl->addWidget(hdr_session_lbl_);

    hl->addStretch();

    // Token count
    hdr_tokens_lbl_ = new QLabel;
    hdr_tokens_lbl_->setStyleSheet(QString("color:%1;font-size:%2px;").arg(col::TEXT_DIM()).arg(fnt::TINY));
    hl->addWidget(hdr_tokens_lbl_);

    // Divider
    auto* div = new QFrame;
    div->setFrameShape(QFrame::VLine);
    div->setFixedWidth(1);
    div->setStyleSheet(QString("background:%1;").arg(col::BORDER_DIM()));
    hl->addWidget(div);

    // Active model pill
    hdr_model_lbl_ = new QLabel("No model");
    hdr_model_lbl_->setStyleSheet(QString("color:%1;font-size:%2px;background:%3;border:1px solid %4;"
                                          "border-radius:0px;padding:2px 8px;")
                                      .arg(col::TEXT_SECONDARY())
                                      .arg(fnt::TINY)
                                      .arg(col::BG_BASE(), col::BORDER_MED()));
    hdr_model_lbl_->setToolTip("Active model — change in Settings > LLM Configuration");
    hl->addWidget(hdr_model_lbl_);

    // Status text
    hdr_status_lbl_ = new QLabel("Ready");
    hdr_status_lbl_->setFixedWidth(64);
    hdr_status_lbl_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hdr_status_lbl_->setStyleSheet(
        QString("color:%1;font-size:%2px;font-weight:600;").arg(col::POSITIVE()).arg(fnt::SMALL));
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
        QString("color:%1;font-size:%2px;font-style:italic;").arg(col::TEXT_SECONDARY()).arg(fnt::SMALL));
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
        QString("color:%1;font-size:%2px;font-weight:700;").arg(col::TEXT_PRIMARY()).arg(fnt::TITLE));
    vl->addWidget(heading);

    auto* sub = new QLabel("Ask about markets, portfolios, macro data, or any financial topic.\n"
                           "Conversations are saved automatically.");
    sub->setAlignment(Qt::AlignCenter);
    sub->setWordWrap(true);
    sub->setStyleSheet(QString("color:%1;font-size:%2px;").arg(col::TEXT_SECONDARY()).arg(fnt::SMALL));
    vl->addWidget(sub);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);

    struct Sug {
        const char* cat;
        const char* cat_color;
        const char* text;
    };
    const Sug suggestions[] = {
        {"Markets", col::CYAN(), "Show me today's top market movers"},
        {"News", col::AMBER(), "Summarize the latest financial news"},
        {"Portfolio", col::POSITIVE(), "Analyze my portfolio performance"},
        {"Analytics", col::AMBER(), "Calculate valuation for AAPL"},
        {"Economics", col::CYAN(), "Current GDP and inflation data"},
        {"Research", col::POSITIVE(), "Tech sector market trends"},
    };

    for (int i = 0; i < 6; ++i) {
        auto* btn = new QPushButton;
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton{background:%1;border:1px solid %2;border-radius:0px;"
                                   "padding:12px 14px;text-align:left;}"
                                   "QPushButton:hover{background:%3;border-color:%4;}")
                               .arg(col::BG_RAISED(), col::BORDER_DIM(), col::BG_HOVER(), col::BORDER_BRIGHT()));

        auto* bl = new QVBoxLayout(btn);
        bl->setContentsMargins(0, 0, 0, 0);
        bl->setSpacing(4);

        auto* cat = new QLabel(suggestions[i].cat);
        cat->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:700;background:transparent;")
                               .arg(suggestions[i].cat_color)
                               .arg(fnt::TINY));
        cat->setAttribute(Qt::WA_TransparentForMouseEvents);
        bl->addWidget(cat);

        auto* desc = new QLabel(suggestions[i].text);
        desc->setStyleSheet(
            QString("color:%1;font-size:%2px;background:transparent;").arg(col::TEXT_PRIMARY()).arg(fnt::SMALL));
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
    bar->setStyleSheet(QString("background:%1;border-top:1px solid %2;").arg(col::BG_RAISED(), col::BORDER_DIM()));
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 12, 16, 12);
    hl->setSpacing(10);

    input_box_ = new QPlainTextEdit;
    input_box_->setPlaceholderText("Message Fincept AI...");
    input_box_->setFixedHeight(44);
    input_box_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    input_box_->setStyleSheet(QString("QPlainTextEdit{background:%1;color:%2;border:1px solid %3;"
                                      "border-radius:0px;padding:8px 14px;font-size:%4px;}"
                                      "QPlainTextEdit:focus{border-color:%5;}")
                                  .arg(col::BG_BASE(), col::TEXT_PRIMARY(), col::BORDER_MED())
                                  .arg(fnt::BODY)
                                  .arg(col::AMBER()));
    input_box_->installEventFilter(this);
    connect(input_box_, &QPlainTextEdit::textChanged, this, [this]() {
        const int lines = input_box_->document()->blockCount();
        input_box_->setFixedHeight(qMin(qMax(lines, 1), 6) * 24 + 20);
        ScreenStateManager::instance().notify_changed(this);
    });
    hl->addWidget(input_box_, 1);

    // Attach file button
    attach_btn_ = new QPushButton("⊕");
    attach_btn_->setFixedSize(44, 44);
    attach_btn_->setCursor(Qt::PointingHandCursor);
    attach_btn_->setToolTip("Attach a file to this message");
    attach_btn_->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
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
    attach_badge_->setStyleSheet(QString("color:%1;font-size:10px;background:rgba(217,119,6,0.12);"
                                         "border:1px solid #78350f;border-radius:0px;padding:2px 6px;%2")
                                     .arg(col::AMBER(), "font-family:'Consolas',monospace;"));
    hl->addWidget(attach_badge_);

    send_btn_ = new QPushButton("Send  ↑");
    send_btn_->setFixedSize(82, 44);
    send_btn_->setCursor(Qt::PointingHandCursor);
    send_btn_->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:none;border-radius:0px;"
                                     "font-size:%3px;font-weight:700;}"
                                     "QPushButton:hover:enabled{background:%4;}"
                                     "QPushButton:disabled{background:%5;color:%6;}")
                                 .arg(col::AMBER(), col::BG_BASE())
                                 .arg(fnt::SMALL)
                                 .arg(col::ORANGE(), col::BG_RAISED(), col::TEXT_DIM()));
    connect(send_btn_, &QPushButton::clicked, this, &AiChatScreen::on_send);
    hl->addWidget(send_btn_);

    return bar;
}

// ── Events ────────────────────────────────────────────────────────────────────

} // namespace fincept::screens
