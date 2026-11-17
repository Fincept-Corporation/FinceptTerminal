// src/screens/agent_config/AgentChatPanel.cpp
#include "screens/agent_config/AgentChatPanel.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "services/agents/AgentService.h"
#include "services/llm/LlmService.h"
#include "services/llm/ProviderCatalog.h"
#include "storage/repositories/AgentConfigRepository.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "storage/repositories/PortfolioRepository.h"
#include "ui/markdown/MarkdownRenderer.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QAbstractTextDocumentLayout>
#include <QCompleter>
#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSizePolicy>
#include <QTextDocument>
#include <QTextOption>
#include <QtMath>

namespace fincept::screens {

namespace col = fincept::ui::colors;
namespace fnt = fincept::ui::fonts;

// ── Style helpers ─────────────────────────────────────────────────────────────

static QString bubble_style(const QString& role) {
    if (role == "user")
        return QString("background:%1;border:1px solid %2;border-radius:6px;padding:10px 14px;")
            .arg(col::ACCENT_BG(), col::alpha(col::AMBER, 0.35));
    if (role == "system")
        return QString("background:%1;border:1px solid %2;"
                       "border-radius:6px;padding:10px 14px;")
            .arg(col::BG_SURFACE(), col::BORDER_DIM());
    return QString("background:%1;border:1px solid %2;border-radius:6px;padding:10px 14px;")
        .arg(col::BG_RAISED(), col::BORDER_MED());
}

static QString body_color(const QString& role) {
    if (role == "system")
        return col::TEXT_SECONDARY;
    return col::TEXT_PRIMARY;
}

static QString role_color(const QString& role) {
    if (role == "user")
        return col::AMBER;
    if (role == "system")
        return col::TEXT_TERTIARY;
    return col::CYAN;
}

static QString role_label(const QString& role) {
    if (role == "user")
        return QCoreApplication::translate("AgentChatPanel", "You");
    if (role == "system")
        return QCoreApplication::translate("AgentChatPanel", "System");
    return QCoreApplication::translate("AgentChatPanel", "Agent");
}

// Keeps a read-only answer bubble exactly as tall as its text wrapped at the
// width the QTextEdit really has. The old code measured at a fixed 600 px, so in
// a narrower panel the bottom lines were cut off (both scroll bars are off) and
// in a wider one the bubble had empty space below the text. QTextEdit re-wraps
// its document to the viewport width on every resize, and the document layout
// reports each new size — following it covers streaming, the final markdown
// re-render and panel resizes alike.
static void agent_chat_fit_height(QTextEdit* body) {
    auto apply = [body](const QSizeF& size) {
        const int h = qMax(qCeil(size.height()) + 8, 28);
        body->setMinimumHeight(h);
        body->setMaximumHeight(h);
    };
    QObject::connect(body->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, body,
                     apply);
    apply(body->document()->size());
}

// Property on an answer bubble: the markdown it renders, kept so a theme change
// can re-render the HTML (its colours are inline).
static constexpr const char* kAgentChatMarkdownProp = "agent_md";

// Composer height: the text's height, between one line of the input font and
// about five lines.
static void agent_chat_fit_input(QTextEdit* edit) {
    const int line_h = QFontMetrics(fnt::font(-1)).lineSpacing();
    const int min_h = qMax(44, line_h + 18);
    const int max_h = qMax(120, line_h * 5 + 18);
    const int doc_h = static_cast<int>(edit->document()->size().height());
    edit->setFixedHeight(qBound(min_h, doc_h + 18, max_h));
}

// ── Constructor ───────────────────────────────────────────────────────────────

AgentChatPanel::AgentChatPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("AgentChatPanel");

    typing_timer_ = new QTimer(this);
    typing_timer_->setInterval(400);
    connect(typing_timer_, &QTimer::timeout, this, [this]() {
        const QStringList states = {tr("Agent is thinking"), tr("Agent is thinking."), tr("Agent is thinking.."),
                                    tr("Agent is thinking...")};
        typing_step_ = (typing_step_ + 1) % states.size();
        typing_dots_lbl_->setText(states[typing_step_]);
    });

    build_ui();
    setup_connections();

    // Seed agent selector from cache immediately
    const auto cached = services::AgentService::instance().cached_agents();
    if (!cached.isEmpty()) {
        agent_selector_->blockSignals(true);
        agent_selector_->clear();
        agent_selector_->addItem(tr("Default (global LLM)"), QString{});
        for (const auto& a : cached)
            agent_selector_->addItem(QString("[%1] %2").arg(a.category, a.name), a.id);
        agent_selector_->blockSignals(false);
    }

    update_llm_status();

    // Answer bubbles are rendered HTML with inline colours: re-render them.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        for (QTextEdit* body : messages_container_->findChildren<QTextEdit*>()) {
            const QVariant md = body->property(kAgentChatMarkdownProp);
            if (md.isValid())
                body->setHtml(ui::MarkdownRenderer::render(md.toString()));
        }
        agent_chat_fit_input(input_edit_);
    });
}

// ── Build UI ──────────────────────────────────────────────────────────────────

void AgentChatPanel::build_ui() {
    setStyleSheet(QString("background:%1;").arg(col::BG_BASE()));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setMinimumHeight(52);
    header->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(col::BG_RAISED(), col::BORDER_DIM()));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 12, 0);
    hl->setSpacing(10);

    header_title_ = new QLabel(tr("AGENT CHAT"));
    header_title_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-1) + "px;font-weight:700;letter-spacing:1.5px;").arg(col::AMBER()));
    hl->addWidget(header_title_);

    // Thin divider
    auto* div1 = new QFrame;
    div1->setFrameShape(QFrame::VLine);
    div1->setFixedWidth(1);
    div1->setStyleSheet(QString("background:%1;").arg(col::BORDER_DIM()));
    hl->addWidget(div1);

    // Agent selector
    agent_caption_ = new QLabel(tr("AGENT:"));
    agent_caption_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:600;letter-spacing:0.5px;").arg(col::TEXT_SECONDARY()));
    hl->addWidget(agent_caption_);

    agent_selector_ = new QComboBox;
    agent_selector_->addItem(tr("Default (global LLM)"), QString{});
    agent_selector_->setMinimumWidth(200);
    agent_selector_->setMaximumWidth(420);
    agent_selector_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    agent_selector_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    agent_selector_->setCursor(Qt::PointingHandCursor);
    agent_selector_->setToolTip(tr("Select a configured agent, or Default to use the global LLM."));

    // Editable + completer gives us an inline search bar inside the dropdown.
    // PopupCompletion shows filtered matches as a popup list while typing.
    agent_selector_->setEditable(true);
    agent_selector_->setInsertPolicy(QComboBox::NoInsert);
    agent_selector_->lineEdit()->setPlaceholderText(tr("Search agent..."));
    agent_selector_->lineEdit()->setClearButtonEnabled(true);
    {
        auto* completer = new QCompleter(agent_selector_->model(), agent_selector_);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        completer->setFilterMode(Qt::MatchContains); // match anywhere in name
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setMaxVisibleItems(12);
        completer->popup()->setStyleSheet(
            QString("QAbstractItemView{"
                    "background:%1;color:%2;border:1px solid %3;"
                    "selection-background-color:%4;selection-color:%5;"
                    "font-size:" + fincept::ui::fonts::px(-3) + "px;padding:2px;outline:none;}"
                    "QAbstractItemView::item{padding:4px 10px;min-height:22px;}")
                .arg(col::BG_RAISED(), col::TEXT_PRIMARY(), col::AMBER(), col::AMBER_DIM(), col::TEXT_ON_ACCENT()));
        agent_selector_->setCompleter(completer);
    }

    agent_selector_->setStyleSheet(
        QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:3px 10px;"
                "font-size:" + fincept::ui::fonts::px(-4) + "px;border-radius:3px;}"
                "QComboBox::drop-down{border:none;width:18px;}"
                "QComboBox:hover{border-color:%4;}"
                "QComboBox QAbstractItemView{background:%1;color:%2;"
                "selection-background-color:%5;selection-color:%6;border:1px solid %3;"
                "font-size:" + fincept::ui::fonts::px(-3) + "px;padding:2px;outline:none;}"
                "QComboBox QAbstractItemView::item{padding:4px 10px;min-height:22px;}"
                "QComboBox QLineEdit{background:%1;color:%2;border:none;"
                "selection-background-color:%5;selection-color:%6;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:0 4px;}")
            .arg(col::BG_BASE(), col::TEXT_PRIMARY(), col::BORDER_MED(), col::AMBER(), col::AMBER_DIM(),
                 col::TEXT_ON_ACCENT()));

    // When user picks an item from popup, clear search text and show the selected name
    connect(agent_selector_, QOverload<int>::of(&QComboBox::activated), agent_selector_, [this](int idx) {
        // Show full item text so truncation never occurs after selection
        agent_selector_->lineEdit()->setText(agent_selector_->itemText(idx));
        agent_selector_->lineEdit()->setCursorPosition(0);
    });

    hl->addWidget(agent_selector_);

    hl->addStretch();

    // Active model pill
    hdr_model_lbl_ = new QLabel(tr("No model"));
    set_model_pill_style(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) +
                                 "px;background:%2;border:1px solid %3;border-radius:3px;padding:2px 8px;")
                             .arg(col::TEXT_SECONDARY(), col::BG_BASE(), col::BORDER_MED()));
    hdr_model_lbl_->setToolTip(tr("Active LLM — configure in Settings > LLM Configuration"));
    hl->addWidget(hdr_model_lbl_);

    // Status chip
    hdr_status_lbl_ = new QLabel(tr("Ready"));
    hdr_status_lbl_->setMinimumWidth(72);
    hdr_status_lbl_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hdr_status_lbl_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:700;").arg(col::SUCCESS()));
    hl->addWidget(hdr_status_lbl_);

    auto* div2 = new QFrame;
    div2->setFrameShape(QFrame::VLine);
    div2->setFixedWidth(1);
    div2->setStyleSheet(QString("background:%1;").arg(col::BORDER_DIM()));
    hl->addWidget(div2);

    // Auto-route toggle
    route_toggle_ = new QPushButton(tr("AUTO-ROUTE"));
    route_toggle_->setCheckable(true);
    route_toggle_->setCursor(Qt::PointingHandCursor);
    route_toggle_->setMinimumHeight(28);
    route_toggle_->setToolTip(tr("When ON, the system picks the best agent for each query."));
    route_toggle_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                "padding:3px 10px;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:600;border-radius:3px;}"
                "QPushButton:checked{color:%3;border-color:%3;background:%5;}"
                "QPushButton:hover{background:%4;}")
            .arg(col::TEXT_SECONDARY(), col::BORDER_MED(), col::SUCCESS(), col::BG_HOVER(),
                 col::tint(col::SUCCESS(), 0.1)));
    hl->addWidget(route_toggle_);

    // Run-as-task toggle — visible only when Agentic Mode is enabled.
    // When ON, send_message() dispatches via AgentService::start_task and the
    // query becomes a durable background task (visible in the AGENTIC tab).
    run_as_task_toggle_ = new QPushButton(tr("RUN AS TASK"));
    run_as_task_toggle_->setCheckable(true);
    run_as_task_toggle_->setCursor(Qt::PointingHandCursor);
    run_as_task_toggle_->setMinimumHeight(28);
    run_as_task_toggle_->setToolTip(
        tr("When ON, this query runs as a durable background task with per-step progress."));
    run_as_task_toggle_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                "padding:3px 10px;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:600;border-radius:3px;}"
                "QPushButton:checked{color:%3;border-color:%3;background:%5;}"
                "QPushButton:hover{background:%4;}")
            .arg(col::TEXT_SECONDARY(), col::BORDER_MED(), col::AMBER(), col::BG_HOVER(), col::ACCENT_BG()));
    run_as_task_toggle_->setVisible(false); // gated by agentic_mode_enabled
    hl->addWidget(run_as_task_toggle_);

    // Clear button
    clear_btn_ = new QPushButton(tr("CLEAR"));
    clear_btn_->setCursor(Qt::PointingHandCursor);
    clear_btn_->setMinimumHeight(28);
    clear_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                "padding:3px 10px;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:600;border-radius:3px;}"
                "QPushButton:hover{background:%3;color:%4;border-color:%4;}")
            .arg(col::TEXT_SECONDARY(), col::BORDER_DIM(), col::BG_HOVER(), col::TEXT_PRIMARY()));
    hl->addWidget(clear_btn_);
    root->addWidget(header);

    // ── Portfolio context bar ─────────────────────────────────────────────────
    auto* pbar = new QWidget(this);
    pbar->setMinimumHeight(38);
    pbar->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;").arg(col::BG_SURFACE(), col::BORDER_DIM()));
    auto* pl = new QHBoxLayout(pbar);
    pl->setContentsMargins(14, 0, 14, 0);
    pl->setSpacing(8);

    portfolio_caption_ = new QLabel(tr("PORTFOLIO:"));
    portfolio_caption_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:600;letter-spacing:0.5px;").arg(col::TEXT_TERTIARY()));
    pl->addWidget(portfolio_caption_);

    portfolio_combo_ = new QComboBox;
    portfolio_combo_->addItem(tr("None"));
    portfolio_combo_->setFixedWidth(160);
    portfolio_combo_->setStyleSheet(QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:2px 8px;"
                                            "font-size:" + fincept::ui::fonts::px(-4) + "px;border-radius:3px;}"
                                            "QComboBox::drop-down{border:none;}"
                                            "QComboBox QAbstractItemView{background:%1;color:%2;"
                                            "selection-background-color:%4;selection-color:%5;border:1px solid %3;}")
                                        .arg(col::BG_BASE(), col::TEXT_PRIMARY(), col::BORDER_DIM(), col::AMBER_DIM(),
                                             col::TEXT_ON_ACCENT()));
    pl->addWidget(portfolio_combo_);

    auto qbtn = [&](const QString& label, const QString& clr) -> QPushButton* {
        auto* b = new QPushButton(label);
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumHeight(24);
        b->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                                 "font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:600;padding:2px 8px;border-radius:3px;}"
                                 "QPushButton:hover{background:%3;border-color:%1;}")
                             .arg(clr, col::BORDER_DIM(), col::overlay(0.05)));
        return b;
    };
    analyze_btn_ = qbtn(tr("ANALYZE"), col::CYAN);
    analyze_btn_->setProperty("fincept_agent_risk", "spend"); // agents: a billed model analysis
    rebalance_btn_ = qbtn(tr("REBALANCE"), col::AMBER);
    risk_btn_ = qbtn(tr("RISK"), col::DANGER);
    pl->addWidget(analyze_btn_);
    pl->addWidget(rebalance_btn_);
    pl->addWidget(risk_btn_);
    pl->addStretch();
    root->addWidget(pbar);

    // ── Messages scroll area ──────────────────────────────────────────────────
    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area_->setStyleSheet(QString("QScrollArea{background:%1;border:none;}"
                                        "QScrollBar:vertical{background:%1;width:5px;border:none;}"
                                        "QScrollBar::handle:vertical{background:%2;border-radius:2px;min-height:20px;}"
                                        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                                    .arg(col::BG_BASE(), col::BORDER_MED()));

    messages_container_ = new QWidget(this);
    messages_container_->setStyleSheet(QString("background:%1;").arg(col::BG_BASE()));
    messages_layout_ = new QVBoxLayout(messages_container_);
    messages_layout_->setContentsMargins(24, 20, 24, 20);
    messages_layout_->setSpacing(12);
    messages_layout_->addStretch();
    scroll_area_->setWidget(messages_container_);

    // Welcome panel (overlaid inside the scroll container indirectly via layout slot)
    welcome_panel_ = new QWidget(this);
    welcome_panel_->setStyleSheet(QString("background:%1;").arg(col::BG_BASE()));
    auto* wvl = new QVBoxLayout(welcome_panel_);
    wvl->setContentsMargins(48, 40, 48, 32);
    wvl->setSpacing(16);
    wvl->addStretch();

    welcome_title_ = new QLabel(tr("How can I help you?"));
    welcome_title_->setAlignment(Qt::AlignCenter);
    welcome_title_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(8) + "px;font-weight:700;background:transparent;").arg(col::TEXT_PRIMARY()));
    wvl->addWidget(welcome_title_);

    welcome_subtitle_ = new QLabel(tr("Ask about markets, portfolios, or any financial topic.\n"
                                      "Select an agent above, or use Auto-Route to let the system decide."));
    welcome_subtitle_->setAlignment(Qt::AlignCenter);
    welcome_subtitle_->setWordWrap(true);
    welcome_subtitle_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-2) + "px;background:transparent;").arg(col::TEXT_SECONDARY()));
    wvl->addWidget(welcome_subtitle_);

    // Suggestion chips
    auto* chip_row = new QHBoxLayout;
    chip_row->setSpacing(10);
    chip_row->addStretch();
    struct Chip {
        const char* label;
        const char* color;
        const char* query;
    };
    const Chip chips[] = {
        {"Markets", col::CYAN, "Show today's top market movers"},
        {"Portfolio", col::SUCCESS, "Analyze my portfolio performance"},
        {"Analytics", col::AMBER, "Calculate valuation for AAPL"},
        {"Risk", col::DANGER, "Run risk analysis on my portfolio"},
    };
    for (const auto& c : chips) {
        auto* btn = new QPushButton(tr(c.label));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:1px solid %3;"
                                   "padding:6px 14px;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:600;border-radius:4px;}"
                                   "QPushButton:hover{background:%4;}")
                               .arg(col::BG_RAISED(), c.color, col::BORDER_MED(), col::BG_HOVER()));
        const QString q = tr(c.query);
        connect(btn, &QPushButton::clicked, this, [this, q]() {
            input_edit_->setPlainText(q);
            send_message();
        });
        chip_row->addWidget(btn);
    }
    chip_row->addStretch();
    wvl->addLayout(chip_row);
    wvl->addStretch();

    messages_layout_->insertWidget(0, welcome_panel_);

    // Typing indicator
    typing_indicator_ = new QWidget(this);
    typing_indicator_->setMinimumHeight(28);
    typing_indicator_->setStyleSheet("background:transparent;");
    auto* til = new QHBoxLayout(typing_indicator_);
    til->setContentsMargins(4, 0, 0, 0);
    typing_dots_lbl_ = new QLabel(tr("Agent is thinking"));
    typing_dots_lbl_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;font-style:italic;background:transparent;").arg(col::TEXT_DIM()));
    til->addWidget(typing_dots_lbl_);
    til->addStretch();
    typing_indicator_->hide();
    messages_layout_->insertWidget(messages_layout_->count() - 1, typing_indicator_);

    root->addWidget(scroll_area_, 1);

    // ── Input bar ─────────────────────────────────────────────────────────────
    auto* ib = new QWidget(this);
    ib->setStyleSheet(QString("background:%1;border-top:1px solid %2;").arg(col::BG_RAISED(), col::BORDER_DIM()));
    auto* il = new QHBoxLayout(ib);
    il->setContentsMargins(14, 10, 14, 10);
    il->setSpacing(10);

    input_edit_ = new QTextEdit;
    input_edit_->setProperty("fincept_agent_risk", "spend"); // agents: Enter sends a billed model request
    input_edit_->setPlaceholderText(tr("Message agent... (Shift+Enter for new line, Enter to send)"));
    input_edit_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    input_edit_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    input_edit_->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    input_edit_->setFrameShape(QFrame::NoFrame);
    input_edit_->setStyleSheet(QString("QTextEdit{background:%1;color:%2;border:1px solid %3;"
                                       "padding:8px 12px;font-size:" + fincept::ui::fonts::px(-1) + "px;border-radius:4px;}"
                                       "QTextEdit:focus{border-color:%4;}")
                                   .arg(col::BG_BASE(), col::TEXT_PRIMARY(), col::BORDER_MED(), col::AMBER()));
    input_edit_->installEventFilter(this);
    input_edit_->setAccessibleName(tr("Message the selected agent"));
    input_edit_->setAccessibleDescription(tr("Enter sends, Shift+Enter inserts a new line."));
    // Grow height as user types (up to ~5 lines)
    agent_chat_fit_input(input_edit_);
    connect(input_edit_->document(), &QTextDocument::contentsChanged, input_edit_,
            [this]() { agent_chat_fit_input(input_edit_); });
    il->addWidget(input_edit_, 1);

    send_btn_ = new QPushButton(tr("Send"));
    send_btn_->setProperty("fincept_agent_risk", "spend"); // agents: sends a billed model request
    send_btn_->setFixedSize(76, 44);
    send_btn_->setCursor(Qt::PointingHandCursor);
    send_btn_->setAccessibleName(tr("Send message to agent"));
    send_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:none;border-radius:6px;"
                "font-size:" + fincept::ui::fonts::px(-2) + "px;font-weight:700;}"
                "QPushButton:hover:enabled{background:%3;}"
                "QPushButton:disabled{background:%4;color:%5;}")
            .arg(col::AMBER(), col::TEXT_ON_ACCENT(), col::AMBER_DIM(), col::BG_RAISED(), col::TEXT_TERTIARY()));
    il->addWidget(send_btn_);
    root->addWidget(ib);

    // Explicit tab order across the panel's interactive controls — creation
    // order otherwise walks header widgets before the composer.
    setTabOrder(agent_selector_, route_toggle_);
    setTabOrder(route_toggle_, run_as_task_toggle_);
    setTabOrder(run_as_task_toggle_, clear_btn_);
    setTabOrder(clear_btn_, portfolio_combo_);
    setTabOrder(portfolio_combo_, analyze_btn_);
    setTabOrder(analyze_btn_, rebalance_btn_);
    setTabOrder(rebalance_btn_, risk_btn_);
    setTabOrder(risk_btn_, input_edit_);
    setTabOrder(input_edit_, send_btn_);

    // ── Status bar ────────────────────────────────────────────────────────────
    status_label_ = new QLabel;
    status_label_->setMinimumHeight(18);
    status_label_->setStyleSheet(
        QString("background:%1;color:%2;font-size:" + fincept::ui::fonts::px(-5) + "px;padding:0 14px;").arg(col::BG_SURFACE(), col::TEXT_TERTIARY()));
    root->addWidget(status_label_);
}

// ── Event filter (Enter to send) ─────────────────────────────────────────────

bool AgentChatPanel::eventFilter(QObject* obj, QEvent* event) {
    if (obj == input_edit_ && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && !(ke->modifiers() & Qt::ShiftModifier)) {
            ke->accept();
            send_message();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ── Connections ───────────────────────────────────────────────────────────────

void AgentChatPanel::setup_connections() {
    // While a reply is streaming the button is STOP (see set_executing); Enter in
    // the composer still only sends.
    connect(send_btn_, &QPushButton::clicked, this, [this]() {
        if (!executing_) {
            send_message();
            return;
        }
        // A streaming run is killed here; its agent_stream_done(kCancelledError)
        // closes the bubble.
        if (services::AgentService::instance().cancel_run(pending_request_id_))
            return;
        // Still in the AUTO-ROUTE step: route_query is not a cancellable run, so
        // STOP did nothing and the agent started anyway. Drop the request here —
        // routing_result ignores an id that is no longer pending, so no run starts.
        show_typing(false);
        set_executing(false);
        if (streaming_bubble_widget_) {
            streaming_bubble_widget_->setPlainText(tr("[stopped]"));
            streaming_bubble_widget_->setReadOnly(true);
            streaming_bubble_widget_ = nullptr;
        }
        streaming_text_.clear();
        status_label_->setText(tr("Stopped by user"));
        hdr_status_lbl_->setText(tr("Stopped"));
        hdr_status_lbl_->setStyleSheet(
            QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:700;").arg(col::DANGER()));
    });
    connect(clear_btn_, &QPushButton::clicked, this, &AgentChatPanel::clear_chat);

    connect(route_toggle_, &QPushButton::toggled, this, [this](bool on) {
        auto_routing_ = on;
        route_toggle_->setText(on ? tr("AUTO-ROUTE: ON") : tr("AUTO-ROUTE"));
        agent_selector_->setEnabled(!on);
        update_llm_status(); // the header names what answers
    });
    // Another agent may answer with another model: keep the header pill true.
    connect(agent_selector_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { update_llm_status(); });

    connect(run_as_task_toggle_, &QPushButton::toggled, this, [this](bool on) {
        run_as_task_ = on;
        run_as_task_toggle_->setText(on ? tr("RUN AS TASK: ON") : tr("RUN AS TASK"));
    });

    // Visibility of the "RUN AS TASK" toggle follows the Agentic Mode setting.
    auto apply_agentic_visibility = [this]() {
        const bool on = services::AgentService::agentic_mode_enabled();
        run_as_task_toggle_->setVisible(on);
        if (!on) {
            run_as_task_toggle_->setChecked(false);
            run_as_task_ = false;
        }
    };
    apply_agentic_visibility();
    connect(&fincept::EventBus::instance(), &fincept::EventBus::eventPublished, this,
            [apply_agentic_visibility](const QString& event, const QVariantMap&) {
                if (event == QStringLiteral("settings.agentic_mode_changed"))
                    apply_agentic_visibility();
            });

    // Agent selector repopulation on discovery
    auto& svc = services::AgentService::instance();
    connect(&svc, &services::AgentService::agents_discovered, this,
            [this](QVector<services::AgentInfo> agents, QVector<services::AgentCategory>) {
                const QString prev_id = agent_selector_->currentData().toString();
                agent_selector_->blockSignals(true);
                agent_selector_->clear();
                agent_selector_->addItem(tr("Default (global LLM)"), QString{});
                for (const auto& a : agents)
                    agent_selector_->addItem(QString("[%1] %2").arg(a.category, a.name), a.id);
                // Restore previous selection
                int restore = 0;
                if (!prev_id.isEmpty()) {
                    for (int i = 1; i < agent_selector_->count(); ++i) {
                        if (agent_selector_->itemData(i).toString() == prev_id) {
                            restore = i;
                            break;
                        }
                    }
                }
                agent_selector_->blockSignals(false);
                agent_selector_->setCurrentIndex(restore);
                // Keep completer model in sync after repopulation
                if (agent_selector_->completer())
                    agent_selector_->completer()->setModel(agent_selector_->model());
            });

    // LLM config changes
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::config_changed, this,
            &AgentChatPanel::update_llm_status, Qt::UniqueConnection);

    // Streaming signals
    connect(&svc, &services::AgentService::agent_stream_thinking, this,
            [this](const QString& req_id, const QString& status) {
                if (req_id != pending_request_id_)
                    return;
                status_label_->setText(status);
            });

    connect(&svc, &services::AgentService::agent_stream_token, this,
            [this](const QString& req_id, const QString& token) {
                if (req_id != pending_request_id_)
                    return;
                streaming_text_ += token;
                if (streaming_bubble_widget_) {
                    if (typing_indicator_->isVisible())
                        show_typing(false);
                    // Replace placeholder sentinel on first real token
                    if (streaming_bubble_widget_->toPlainText() == "...") {
                        streaming_bubble_widget_->setPlainText(token);
                    } else {
                        streaming_bubble_widget_->moveCursor(QTextCursor::End);
                        streaming_bubble_widget_->insertPlainText(token);
                    }
                    scroll_to_bottom();
                }
                status_label_->setText(tr("Streaming..."));
            });

    connect(&svc, &services::AgentService::agent_stream_done, this, [this](services::AgentExecutionResult r) {
        if (pending_request_id_.isEmpty() || r.request_id != pending_request_id_)
            return;
        show_typing(false);
        set_executing(false);

        const bool stopped = !r.success && r.error == QLatin1String(services::AgentService::kCancelledError);
        if (streaming_bubble_widget_) {
            if (stopped) {
                // STOP pressed: keep the partial answer, mark where it was cut off.
                const QString partial = streaming_text_.trimmed();
                streaming_bubble_widget_->setPlainText(partial.isEmpty() ? tr("[stopped]")
                                                                         : partial + "\n\n" + tr("[stopped]"));
            } else if (!r.success) {
                streaming_bubble_widget_->setPlainText(tr("Error: %1").arg(r.error));
            } else {
                // Replace streamed plain-text tokens with fully rendered markdown HTML.
                const QString final_text = r.response.isEmpty() ? streaming_text_ : r.response;
                // Height follows the re-rendered text (agent_chat_fit_height).
                if (!final_text.trimmed().isEmpty()) {
                    streaming_bubble_widget_->setProperty(kAgentChatMarkdownProp, final_text);
                    streaming_bubble_widget_->setHtml(ui::MarkdownRenderer::render(final_text));
                }
            }
            streaming_bubble_widget_->setReadOnly(true);
            streaming_bubble_widget_ = nullptr;
        } else if (r.success && !r.response.isEmpty()) {
            add_assistant_bubble(r.response);
        } else if (!r.success) {
            add_system_bubble(stopped ? tr("Stopped.") : tr("Error: %1").arg(r.error));
        }

        streaming_text_.clear();
        if (r.success) {
            status_label_->setText(tr("Response received (%1ms)").arg(r.execution_time_ms));
            hdr_status_lbl_->setText(tr("Ready"));
            hdr_status_lbl_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:700;").arg(col::SUCCESS()));
        } else {
            status_label_->setText(stopped ? tr("Stopped by user") : tr("Agent execution failed"));
            hdr_status_lbl_->setText(stopped ? tr("Stopped") : tr("Error"));
            hdr_status_lbl_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:700;").arg(col::DANGER()));
        }
        scroll_to_bottom();
    });

    // No AgentService::error_occurred handler: route_query always answers with
    // routing_result and run_agent_streaming always ends in agent_stream_done —
    // crash, spawn failure and missing Python included. error_occurred reports
    // unrelated discovery / task / schedule / plan / config calls; reacting to
    // those ended a live reply as "Error" and then dropped the real answer.

    // Routing result
    connect(&svc, &services::AgentService::routing_result, this, [this](services::RoutingResult r) {
        // An emptied id (STOP / CLEAR during routing) means: do not start the run.
        if (pending_request_id_.isEmpty() || r.request_id != pending_request_id_)
            return;
        if (r.success) {
            add_system_bubble(tr("Routed to: %1 (intent: %2, confidence: %3%)")
                                  .arg(r.agent_id, r.intent)
                                  .arg(static_cast<int>(r.confidence * 100)));
            // route_query's `config` carries only tools/reasoning hints — not the
            // routed agent's id — so without it the run used a generic assistant
            // instead of the agent the bubble above just announced.
            QJsonObject run_cfg = r.config;
            if (!r.agent_id.isEmpty() && !run_cfg.contains("agent_id"))
                run_cfg["agent_id"] = r.agent_id;
            pending_request_id_ = services::AgentService::instance().run_agent_streaming(last_query_, run_cfg);
        } else {
            add_system_bubble(tr("Auto-routing failed — using default agent."));
            pending_request_id_ = services::AgentService::instance().run_agent_streaming(last_query_, {});
        }
    });

    // Portfolio quick actions. The "None" entry has no data role, so an empty
    // currentData() means "no portfolio selected" (survives translation).
    connect(analyze_btn_, &QPushButton::clicked, this, [this]() {
        const QString pf = portfolio_combo_->currentText();
        if (portfolio_combo_->currentData().toString().isEmpty())
            return;
        input_edit_->setPlainText(tr("Analyze my portfolio '%1' — give key metrics and recommendations.").arg(pf));
        send_message();
    });
    connect(rebalance_btn_, &QPushButton::clicked, this, [this]() {
        const QString pf = portfolio_combo_->currentText();
        if (portfolio_combo_->currentData().toString().isEmpty())
            return;
        input_edit_->setPlainText(tr("Suggest rebalancing for portfolio '%1' to optimize risk-return.").arg(pf));
        send_message();
    });
    connect(risk_btn_, &QPushButton::clicked, this, [this]() {
        const QString pf = portfolio_combo_->currentText();
        if (portfolio_combo_->currentData().toString().isEmpty())
            return;
        input_edit_->setPlainText(tr("Perform risk analysis on portfolio '%1' — VaR, drawdown, stress test.").arg(pf));
        send_message();
    });
}

// ── showEvent ─────────────────────────────────────────────────────────────────

void AgentChatPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    services::AgentService::instance().discover_agents();
    update_llm_status();
    refresh_portfolios();
}

// ── LLM status display ────────────────────────────────────────────────────────

AgentChatPanel::AnsweringModel AgentChatPanel::answering_model() const {
    AnsweringModel m;
    auto& llm = ai_chat::LlmService::instance();
    const auto use_global = [&m, &llm]() {
        if (llm.is_configured()) {
            m.provider = llm.active_provider();
            m.model = llm.active_model();
            m.ok = true;
        }
    };
    if (auto_routing_) {
        use_global();
        m.source = tr("Auto-route picks the agent for each message: a saved agent answers with its own LLM profile, "
                      "a built-in agent with the global LLM shown here.");
        return m;
    }
    const QString agent_id = agent_selector_ ? agent_selector_->currentData().toString() : QString();
    if (agent_id.isEmpty()) {
        use_global();
        m.source = tr("Default: the global LLM (Settings > LLM Configuration).");
        return m;
    }
    // Only agents saved in the CREATE tab carry an LLM profile assignment; the
    // built-in (Python card) agents run on the global LLM (AgentService).
    if (AgentConfigRepository::instance().get(agent_id).is_err()) {
        use_global();
        m.source = tr("This built-in agent answers with the global LLM (Settings > LLM Configuration).");
        return m;
    }
    auto& repo = LlmProfileRepository::instance();
    const ResolvedLlmProfile r = repo.resolve_for_context(QStringLiteral("agent"), agent_id);
    if (r.provider.isEmpty()) {
        use_global();
        m.source = tr("No LLM profile applies to this agent: it answers with the global LLM.");
        return m;
    }
    m.provider = r.provider;
    m.model = r.model_id;
    m.ok = true;
    const QString own_id = repo.get_assignment(QStringLiteral("agent"), agent_id);
    QString own_harness; // the agent's own assignment is a harness profile the runtime passes over
    if (!own_id.isEmpty() && own_id != r.profile_id) {
        if (const auto own = repo.get_profile(own_id); own.is_ok() && own.value().is_harness())
            own_harness = own.value().name;
    }
    if (!own_id.isEmpty() && own_id == r.profile_id)
        m.source = tr("This agent's LLM profile “%1”.").arg(r.profile_name);
    else if (!r.profile_id.isEmpty() && r.profile_id == repo.get_assignment(QStringLiteral("agent_default"), {}))
        m.source = tr("The default LLM profile for agents, “%1”.").arg(r.profile_name);
    else if (!r.profile_id.isEmpty())
        m.source = tr("The global default LLM profile, “%1”.").arg(r.profile_name);
    else
        m.source = tr("The active provider (Settings > LLM Configuration).");
    if (!own_harness.isEmpty())
        m.source += QLatin1Char(' ') + tr("Its assigned profile “%1” is an agent harness, which tool-using agents "
                                          "cannot use yet.")
                                           .arg(own_harness);
    return m;
}

void AgentChatPanel::set_model_pill_style(const QString& css) {
    if (css == model_pill_css_)
        return;
    model_pill_css_ = css;
    hdr_model_lbl_->setStyleSheet(css);
}

void AgentChatPanel::update_llm_status() {
    // The pill names the model that will actually answer the next message for
    // the current agent selection (it used to show the global provider even when
    // the selected agent resolves to its own profile).
    const AnsweringModel m = answering_model();
    if (m.ok) {
        const QString provider = ai_chat::ProviderCatalog::display_name(m.provider);
        hdr_model_lbl_->setText(m.model.isEmpty() ? provider : QStringLiteral("%1 / %2").arg(provider, m.model));
        hdr_model_lbl_->setToolTip(tr("Answers the next message: %1 / %2\n%3\nConfigure in Settings > LLM Configuration "
                                      "or the agent's LLM profile (AGENTS view).")
                                       .arg(m.provider, m.model, m.source));
        set_model_pill_style(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) +
                                     "px;background:%2;border:1px solid %3;border-radius:3px;padding:2px 8px;")
                                 .arg(col::TEXT_SECONDARY(), col::BG_BASE(), col::BORDER_MED()));
        if (!executing_) {
            hdr_status_lbl_->setText(tr("Ready"));
            hdr_status_lbl_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:700;").arg(col::SUCCESS()));
        }
    } else {
        hdr_model_lbl_->setText(tr("No LLM configured"));
        hdr_model_lbl_->setToolTip(tr("No LLM answers this selection — configure one in Settings > LLM Configuration."));
        set_model_pill_style(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) +
                                     "px;background:%2;border:1px solid %3;border-radius:3px;padding:2px 8px;")
                                 .arg(col::DANGER(), col::BG_BASE(), col::DANGER()));
        hdr_status_lbl_->setText(tr("Unconfigured"));
        hdr_status_lbl_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:700;").arg(col::DANGER()));
        status_label_->setText(tr("No LLM provider configured — go to Settings > LLM Configuration"));
    }
}

// ── send_message ─────────────────────────────────────────────────────────────

void AgentChatPanel::send_message() {
    const QString text = input_edit_->toPlainText().trimmed();
    if (text.isEmpty() || executing_)
        return;

    // Guard: something must answer. A saved agent with its own LLM profile can
    // answer even when no global provider is set (AgentService embeds its model);
    // a background task resolves on its own side, so it keeps the global check.
    auto& llm = ai_chat::LlmService::instance();
    if (run_as_task_ ? !llm.is_configured() : !answering_model().ok) {
        add_system_bubble(tr("No LLM provider configured. Go to Settings > LLM Configuration to set one up."));
        return;
    }

    set_executing(true);
    show_welcome(false);

    // Show the raw user text in the bubble (without the injected context blob)
    add_user_bubble(text);
    input_edit_->clear();
    agent_chat_fit_input(input_edit_);

    // Build enriched query: prepend portfolio context if one is selected
    const QString pf_ctx = build_portfolio_context();
    last_query_ = pf_ctx.isEmpty() ? text : QString("%1\n\nUser question: %2").arg(pf_ctx, text);

    // Show which portfolio is active in the status bar
    if (!pf_ctx.isEmpty()) {
        const QString pf_name = portfolio_combo_->currentText();
        status_label_->setText(tr("Portfolio context: %1").arg(pf_name));
    }

    // Create streaming bubble — seed with "..." so height bootstraps correctly
    auto* te = add_streaming_bubble();
    te->setPlainText("...");
    streaming_bubble_widget_ = te;
    show_typing(true);
    scroll_to_bottom();

    if (run_as_task_) {
        // Agentic Mode: dispatch the query as a durable background task. The
        // chat bubble closes immediately with a confirmation; live progress is
        // shown in the AGENTIC tab (and on the task:event:* DataHub topic).
        const QString agent_id = agent_selector_->currentData().toString();
        QJsonObject config;
        if (!agent_id.isEmpty())
            config["agent_id"] = agent_id;
        pending_request_id_ = services::AgentService::instance().start_task(last_query_, config);
        if (streaming_bubble_widget_) {
            streaming_bubble_widget_->setPlainText(tr("Task started. Open the AGENTIC tab to watch progress."));
            streaming_bubble_widget_->setReadOnly(true);
            streaming_bubble_widget_ = nullptr;
        }
        show_typing(false);
        set_executing(false);
    } else if (auto_routing_) {
        pending_request_id_ = services::AgentService::instance().route_query(last_query_);
    } else {
        const QString agent_id = agent_selector_->currentData().toString();
        QJsonObject config;
        if (!agent_id.isEmpty())
            config["agent_id"] = agent_id;
        pending_request_id_ = services::AgentService::instance().run_agent_streaming(last_query_, config);
    }
}

// ── Bubble builders ───────────────────────────────────────────────────────────

// ── resizeEvent — keep bubbles at most 72% of panel width ────────────────────

void AgentChatPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (!messages_container_)
        return;
    const int max_user = static_cast<int>(width() * 0.72);
    const int max_ai = static_cast<int>(width() * 0.82);
    // Update all col_w widgets — they are direct children of row widgets in the layout
    for (int i = 0; i < messages_layout_->count(); ++i) {
        auto* item = messages_layout_->itemAt(i);
        if (!item->widget())
            continue;
        auto* row = item->widget();
        // Each row has one QHBoxLayout with a col_w inside
        auto* rl = qobject_cast<QHBoxLayout*>(row->layout());
        if (!rl)
            continue;
        for (int j = 0; j < rl->count(); ++j) {
            auto* wi = rl->itemAt(j);
            if (!wi->widget())
                continue;
            auto* col_w = wi->widget();
            if (col_w->objectName() == "bubble_user")
                col_w->setMaximumWidth(max_user);
            else if (col_w->objectName() == "bubble_ai")
                col_w->setMaximumWidth(max_ai);
        }
    }
}

// ── Portfolio context builder ─────────────────────────────────────────────────

QString AgentChatPanel::build_portfolio_context() const {
    const QString pf_name = portfolio_combo_->currentText();
    // The first combo item ("None") carries no data role — treat any empty-data
    // selection as "no portfolio" so the check survives translation.
    if (portfolio_combo_->currentData().toString().isEmpty() || pf_name.isEmpty())
        return {};

    const QString pf_id = portfolio_combo_->currentData().toString();
    QString ctx = QString("[Portfolio context: %1]\n").arg(pf_name);

    // Fetch assets
    const auto assets_result = PortfolioRepository::instance().get_assets(pf_id);
    if (assets_result.is_ok() && !assets_result.value().isEmpty()) {
        ctx += "Holdings:\n";
        for (const auto& a : assets_result.value()) {
            ctx += QString("  %1: qty=%2, avg_cost=%3\n")
                       .arg(a.symbol)
                       .arg(a.quantity, 0, 'f', 4)
                       .arg(a.avg_buy_price, 0, 'f', 2);
        }
    }
    return ctx;
}

void AgentChatPanel::refresh_portfolios() {
    portfolio_combo_->blockSignals(true);
    const QString prev = portfolio_combo_->currentText();
    portfolio_combo_->clear();
    portfolio_combo_->addItem(tr("None"));
    const auto result = PortfolioRepository::instance().list_portfolios();
    if (result.is_ok()) {
        for (const auto& p : result.value())
            portfolio_combo_->addItem(p.name, p.id);
    }
    const int idx = portfolio_combo_->findText(prev);
    portfolio_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
    portfolio_combo_->blockSignals(false);
}

// ── Bubble builders ───────────────────────────────────────────────────────────

void AgentChatPanel::add_user_bubble(const QString& text) {
    const QString ts = QDateTime::currentDateTime().toString("HH:mm");
    const QString role = "user";

    auto* row = new QWidget(this);
    row->setStyleSheet("background:transparent;");
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(0);
    rl->addStretch();

    auto* col_w = new QWidget(this);
    col_w->setObjectName("bubble_user");
    col_w->setStyleSheet("background:transparent;");
    col_w->setMaximumWidth(static_cast<int>(width() * 0.72));
    auto* cvl = new QVBoxLayout(col_w);
    cvl->setContentsMargins(0, 0, 0, 0);
    cvl->setSpacing(4);

    auto* role_lbl = new QLabel(role_label(role));
    role_lbl->setAlignment(Qt::AlignRight);
    role_lbl->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:600;background:transparent;").arg(role_color(role)));
    cvl->addWidget(role_lbl);

    auto* bubble = new QFrame;
    bubble->setStyleSheet(bubble_style(role));
    auto* bvl = new QVBoxLayout(bubble);
    bvl->setContentsMargins(0, 0, 0, 0);
    auto* body = new QLabel(text);
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->setTextFormat(Qt::PlainText);
    body->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-2) + "px;background:transparent;").arg(body_color(role)));
    bvl->addWidget(body);
    cvl->addWidget(bubble);

    auto* ts_lbl = new QLabel(ts);
    ts_lbl->setAlignment(Qt::AlignRight);
    ts_lbl->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;background:transparent;").arg(col::TEXT_DIM()));
    cvl->addWidget(ts_lbl);

    rl->addWidget(col_w);
    messages_layout_->insertWidget(messages_layout_->count() - 1, row);
    scroll_to_bottom();
}

void AgentChatPanel::add_assistant_bubble(const QString& text, const QString& agent_name) {
    const QString ts = QDateTime::currentDateTime().toString("HH:mm");
    const QString role = "assistant";

    auto* row = new QWidget(this);
    row->setStyleSheet("background:transparent;");
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(0);

    auto* col_w = new QWidget(this);
    col_w->setObjectName("bubble_ai");
    col_w->setStyleSheet("background:transparent;");
    col_w->setMaximumWidth(static_cast<int>(width() * 0.82));
    auto* cvl = new QVBoxLayout(col_w);
    cvl->setContentsMargins(0, 0, 0, 0);
    cvl->setSpacing(4);

    auto* hdr_row = new QHBoxLayout;
    auto* role_lbl = new QLabel(agent_name.isEmpty() ? tr("Agent") : agent_name);
    role_lbl->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:600;background:transparent;").arg(role_color(role)));
    hdr_row->addWidget(role_lbl);
    hdr_row->addStretch();
    cvl->addLayout(hdr_row);

    auto* bubble = new QFrame;
    bubble->setStyleSheet(bubble_style(role));
    auto* bvl = new QVBoxLayout(bubble);
    bvl->setContentsMargins(0, 0, 0, 0);

    // Use QTextEdit so markdown tables/bold/lists render correctly.
    // QLabel with Qt::MarkdownText does not support tables.
    auto* body = new QTextEdit;
    body->setReadOnly(true);
    body->setFrameShape(QFrame::NoFrame);
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    body->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    body->document()->setDocumentMargin(4);
    body->setStyleSheet(
        QString("QTextEdit{background:transparent;color:%1;border:none;font-size:" + fincept::ui::fonts::px(-2) + "px;}").arg(body_color(role)));
    body->setProperty(kAgentChatMarkdownProp, text);
    body->setHtml(ui::MarkdownRenderer::render(text));
    // Height follows the text wrapped at the bubble's real width.
    agent_chat_fit_height(body);

    bvl->addWidget(body);
    cvl->addWidget(bubble);

    auto* ts_lbl = new QLabel(ts);
    ts_lbl->setAlignment(Qt::AlignLeft);
    ts_lbl->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;background:transparent;").arg(col::TEXT_DIM()));
    cvl->addWidget(ts_lbl);

    rl->addWidget(col_w);
    rl->addStretch();
    messages_layout_->insertWidget(messages_layout_->count() - 1, row);
    scroll_to_bottom();
}

void AgentChatPanel::add_system_bubble(const QString& text) {
    const QString role = "system";

    auto* row = new QWidget(this);
    row->setStyleSheet("background:transparent;");
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->addStretch();

    auto* bubble = new QFrame;
    bubble->setStyleSheet(bubble_style(role));
    bubble->setMaximumWidth(static_cast<int>(width() * 0.60));
    auto* bvl = new QHBoxLayout(bubble);
    bvl->setContentsMargins(0, 0, 0, 0);
    auto* body = new QLabel(text);
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;background:transparent;").arg(body_color(role)));
    bvl->addWidget(body);

    rl->addWidget(bubble);
    rl->addStretch();
    messages_layout_->insertWidget(messages_layout_->count() - 1, row);
    scroll_to_bottom();
}

QTextEdit* AgentChatPanel::add_streaming_bubble(const QString& agent_name) {
    auto* row = new QWidget(this);
    row->setStyleSheet("background:transparent;");
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(0);

    auto* col_w = new QWidget(this);
    col_w->setObjectName("bubble_ai");
    col_w->setStyleSheet("background:transparent;");
    col_w->setMaximumWidth(static_cast<int>(width() * 0.82));
    auto* cvl = new QVBoxLayout(col_w);
    cvl->setContentsMargins(0, 0, 0, 0);
    cvl->setSpacing(4);

    auto* role_lbl = new QLabel(agent_name.isEmpty() ? tr("Agent") : agent_name);
    role_lbl->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:600;background:transparent;").arg(col::CYAN()));
    cvl->addWidget(role_lbl);

    auto* bubble = new QFrame;
    bubble->setStyleSheet(bubble_style("assistant"));
    auto* bvl = new QVBoxLayout(bubble);
    bvl->setContentsMargins(0, 0, 0, 0);

    auto* body = new QTextEdit;
    // Read-only from the start — programmatic setPlainText/insertPlainText still
    // work, and the user can no longer type into the agent's answer mid-stream.
    body->setReadOnly(true);
    body->setFrameShape(QFrame::NoFrame);
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    body->document()->setDocumentMargin(4);
    body->setMinimumHeight(28);
    body->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    body->setStyleSheet(
        QString("QTextEdit{background:transparent;color:%1;border:none;font-size:" + fincept::ui::fonts::px(-2) + "px;}").arg(col::TEXT_PRIMARY()));
    // Auto-grow as tokens arrive, at the width the bubble actually has.
    agent_chat_fit_height(body);
    bvl->addWidget(body);
    cvl->addWidget(bubble);

    rl->addWidget(col_w);
    rl->addStretch();
    messages_layout_->insertWidget(messages_layout_->count() - 1, row);
    return body;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void AgentChatPanel::scroll_to_bottom() {
    QTimer::singleShot(50, this, [this]() {
        scroll_area_->verticalScrollBar()->setValue(scroll_area_->verticalScrollBar()->maximum());
    });
}

void AgentChatPanel::set_executing(bool on) {
    executing_ = on;
    // Stays enabled while a run is in flight: it becomes the STOP button.
    send_btn_->setText(on ? tr("Stop") : tr("Send"));
    if (on) {
        hdr_status_lbl_->setText(tr("Streaming"));
        hdr_status_lbl_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:700;").arg(col::AMBER()));
        status_label_->setText(tr("Processing..."));
    }
    if (!on) {
        pending_request_id_.clear();
    }
}

void AgentChatPanel::show_welcome(bool on) {
    if (welcome_panel_)
        welcome_panel_->setVisible(on);
}

void AgentChatPanel::show_typing(bool on) {
    if (!typing_indicator_)
        return;
    if (on) {
        // Keep the indicator immediately above the trailing stretch, i.e. below
        // the newest bubble. Bubbles are inserted at count()-1 too, so without
        // this re-anchor the indicator stays wherever it was first inserted and
        // "Agent is thinking" renders at the TOP of the transcript.
        const int last = messages_layout_->count() - 1; // the stretch
        if (messages_layout_->indexOf(typing_indicator_) != last - 1) {
            messages_layout_->removeWidget(typing_indicator_);
            messages_layout_->insertWidget(messages_layout_->count() - 1, typing_indicator_);
        }
        typing_step_ = 0;
        typing_dots_lbl_->setText(tr("Agent is thinking"));
        typing_indicator_->show();
        typing_timer_->start();
    } else {
        typing_timer_->stop();
        typing_indicator_->hide();
    }
}

void AgentChatPanel::clear_chat() {
    // CLEAR during a run used to leave the Python agent running (and spending)
    // with its answer discarded. Release the request id first so this panel's
    // agent_stream_done handler ignores the cancellation (the transcript is being
    // wiped), then stop the run. During the AUTO-ROUTE step there is no run yet:
    // the released id makes routing_result drop it, so none is started.
    if (executing_ && !pending_request_id_.isEmpty()) {
        const QString running = pending_request_id_;
        pending_request_id_.clear();
        services::AgentService::instance().cancel_run(running);
    }
    // Null streaming refs before deleting widgets
    streaming_bubble_widget_ = nullptr;
    streaming_text_.clear();
    show_typing(false);
    set_executing(false);

    // Remove every message row, keeping the two fixtures and the trailing
    // stretch. The old version assumed the typing indicator sat at count-2 and
    // blindly took index 1 — but new bubbles are inserted *after* the typing
    // indicator, so index 1 WAS the typing indicator. Clearing therefore
    // deleteLater()'d it (leaving `typing_indicator_` dangling — the next send
    // called show()/setText() on freed memory) and always left the last message
    // row behind. Identify the fixtures by pointer instead of by index.
    for (int i = messages_layout_->count() - 1; i >= 0; --i) {
        QLayoutItem* item = messages_layout_->itemAt(i);
        QWidget* w = item ? item->widget() : nullptr;
        if (!w || w == welcome_panel_ || w == typing_indicator_)
            continue; // stretch (no widget) + the two permanent fixtures
        QLayoutItem* taken = messages_layout_->takeAt(i);
        w->deleteLater();
        delete taken;
    }

    show_welcome(true);
    hdr_status_lbl_->setText(tr("Ready"));
    hdr_status_lbl_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:700;").arg(col::SUCCESS()));
    status_label_->clear();
    update_llm_status();
}

// ── Re-translation ───────────────────────────────────────────────────────────

void AgentChatPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void AgentChatPanel::retranslateUi() {
    // Header.
    if (header_title_)
        header_title_->setText(tr("AGENT CHAT"));
    if (agent_caption_)
        agent_caption_->setText(tr("AGENT:"));
    if (agent_selector_) {
        agent_selector_->setToolTip(tr("Select a configured agent, or Default to use the global LLM."));
        if (agent_selector_->lineEdit())
            agent_selector_->lineEdit()->setPlaceholderText(tr("Search agent..."));
        // Item 0 is the fixed "Default (global LLM)" entry (empty data role).
        if (agent_selector_->count() > 0 && agent_selector_->itemData(0).toString().isEmpty())
            agent_selector_->setItemText(0, tr("Default (global LLM)"));
    }
    // Toggle buttons reflect on/off state — re-apply the matching label.
    if (route_toggle_)
        route_toggle_->setText(route_toggle_->isChecked() ? tr("AUTO-ROUTE: ON") : tr("AUTO-ROUTE"));
    if (route_toggle_)
        route_toggle_->setToolTip(tr("When ON, the system picks the best agent for each query."));
    if (run_as_task_toggle_)
        run_as_task_toggle_->setText(run_as_task_toggle_->isChecked() ? tr("RUN AS TASK: ON") : tr("RUN AS TASK"));
    if (run_as_task_toggle_)
        run_as_task_toggle_->setToolTip(
            tr("When ON, this query runs as a durable background task with per-step progress."));
    if (clear_btn_)
        clear_btn_->setText(tr("CLEAR"));
    if (hdr_model_lbl_)
        hdr_model_lbl_->setToolTip(tr("Active LLM — configure in Settings > LLM Configuration"));

    // Portfolio context bar.
    if (portfolio_caption_)
        portfolio_caption_->setText(tr("PORTFOLIO:"));
    // Item 0 is the fixed "None" entry (no data role).
    if (portfolio_combo_ && portfolio_combo_->count() > 0 && portfolio_combo_->itemData(0).toString().isEmpty())
        portfolio_combo_->setItemText(0, tr("None"));
    if (analyze_btn_)
        analyze_btn_->setText(tr("ANALYZE"));
    if (rebalance_btn_)
        rebalance_btn_->setText(tr("REBALANCE"));
    if (risk_btn_)
        risk_btn_->setText(tr("RISK"));

    // Welcome panel.
    if (welcome_title_)
        welcome_title_->setText(tr("How can I help you?"));
    if (welcome_subtitle_)
        welcome_subtitle_->setText(tr("Ask about markets, portfolios, or any financial topic.\n"
                                      "Select an agent above, or use Auto-Route to let the system decide."));

    // Input bar (send_btn_ flips to "Stop" while executing — leave that state).
    if (input_edit_)
        input_edit_->setPlaceholderText(tr("Message agent... (Shift+Enter for new line, Enter to send)"));
    if (send_btn_ && !executing_)
        send_btn_->setText(tr("Send"));

    // Header status pill + live status bar hold runtime state. Refresh the LLM
    // status (it re-derives Ready / model / Unconfigured text from current config).
    update_llm_status();
}

} // namespace fincept::screens
