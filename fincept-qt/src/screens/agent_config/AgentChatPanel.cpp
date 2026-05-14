// src/screens/agent_config/AgentChatPanel.cpp
#include "screens/agent_config/AgentChatPanel.h"

#include "services/llm/LlmService.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "services/agents/AgentService.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/PortfolioRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/markdown/MarkdownRenderer.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QCompleter>
#include <QDateTime>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSizePolicy>
#include <QTextOption>

namespace fincept::screens {

namespace col = fincept::ui::colors;
namespace fnt = fincept::ui::fonts;

// ── Style helpers ─────────────────────────────────────────────────────────────

static QString bubble_style(const QString& role) {
    if (role == "user")
        return "background:rgba(120,53,15,0.45);border:1px solid rgba(217,119,6,0.28);"
               "border-radius:6px;padding:10px 14px;";
    if (role == "system")
        return QString("background:%1;border:1px solid %2;"
                       "border-radius:6px;padding:10px 14px;")
            .arg(col::BG_SURFACE(), col::BORDER_DIM());
    return QString("background:%1;border:1px solid %2;border-radius:6px;padding:10px 14px;")
        .arg(col::BG_RAISED(), col::BORDER_MED());
}

static QString body_color(const QString& role) {
    if (role == "user")
        return "#fff7ed";
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
        return "You";
    if (role == "system")
        return "System";
    return "Agent";
}

// ── Constructor ───────────────────────────────────────────────────────────────

AgentChatPanel::AgentChatPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("AgentChatPanel");

    typing_timer_ = new QTimer(this);
    typing_timer_->setInterval(400);
    connect(typing_timer_, &QTimer::timeout, this, [this]() {
        static const QStringList states = {"Agent is thinking", "Agent is thinking.", "Agent is thinking..",
                                           "Agent is thinking..."};
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
        agent_selector_->addItem("Default (global LLM)", QString{});
        for (const auto& a : cached)
            agent_selector_->addItem(QString("[%1] %2").arg(a.category, a.name), a.id);
        agent_selector_->blockSignals(false);
    }

    update_llm_status();

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { update(); });
}

// ── Build UI ──────────────────────────────────────────────────────────────────

void AgentChatPanel::build_ui() {
    setStyleSheet(QString("background:%1;").arg(col::BG_BASE()));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setFixedHeight(52);
    header->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;").arg(col::BG_RAISED(), col::BORDER_DIM()));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 12, 0);
    hl->setSpacing(10);

    auto* title = new QLabel("AGENT CHAT");
    title->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;letter-spacing:1.5px;").arg(col::AMBER()));
    hl->addWidget(title);

    // Thin divider
    auto* div1 = new QFrame;
    div1->setFrameShape(QFrame::VLine);
    div1->setFixedWidth(1);
    div1->setStyleSheet(QString("background:%1;").arg(col::BORDER_DIM()));
    hl->addWidget(div1);

    // Agent selector
    auto* agent_lbl = new QLabel("AGENT:");
    agent_lbl->setStyleSheet(
        QString("color:%1;font-size:9px;font-weight:600;letter-spacing:0.5px;").arg(col::TEXT_TERTIARY()));
    hl->addWidget(agent_lbl);

    agent_selector_ = new QComboBox;
    agent_selector_->addItem("Default (global LLM)", QString{});
    agent_selector_->setMinimumWidth(200);
    agent_selector_->setMaximumWidth(420);
    agent_selector_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    agent_selector_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    agent_selector_->setCursor(Qt::PointingHandCursor);
    agent_selector_->setToolTip("Select a configured agent, or Default to use the global LLM.");

    // Editable + completer gives us an inline search bar inside the dropdown.
    // PopupCompletion shows filtered matches as a popup list while typing.
    agent_selector_->setEditable(true);
    agent_selector_->setInsertPolicy(QComboBox::NoInsert);
    agent_selector_->lineEdit()->setPlaceholderText("Search agent...");
    agent_selector_->lineEdit()->setClearButtonEnabled(true);
    {
        auto* completer = new QCompleter(agent_selector_->model(), agent_selector_);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        completer->setFilterMode(Qt::MatchContains); // match anywhere in name
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setMaxVisibleItems(12);
        completer->popup()->setStyleSheet(QString("QAbstractItemView{"
                                                  "background:%1;color:%2;border:1px solid %3;"
                                                  "selection-background-color:%4;"
                                                  "font-size:11px;padding:2px;outline:none;}"
                                                  "QAbstractItemView::item{padding:4px 10px;min-height:22px;}")
                                              .arg(col::BG_RAISED(), col::TEXT_PRIMARY(), col::AMBER(), col::AMBER_DIM()));
        agent_selector_->setCompleter(completer);
    }

    agent_selector_->setStyleSheet(
        QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:3px 10px;"
                "font-size:10px;border-radius:3px;}"
                "QComboBox::drop-down{border:none;width:18px;}"
                "QComboBox:hover{border-color:%4;}"
                "QComboBox QAbstractItemView{background:%1;color:%2;"
                "selection-background-color:%5;border:1px solid %3;"
                "font-size:11px;padding:2px;outline:none;}"
                "QComboBox QAbstractItemView::item{padding:4px 10px;min-height:22px;}"
                "QComboBox QLineEdit{background:%1;color:%2;border:none;"
                "selection-background-color:%5;font-size:10px;padding:0 4px;}")
            .arg(col::BG_BASE(), col::TEXT_PRIMARY(), col::BORDER_MED(), col::AMBER(), col::AMBER_DIM()));

    // When user picks an item from popup, clear search text and show the selected name
    connect(agent_selector_, QOverload<int>::of(&QComboBox::activated), agent_selector_, [this](int idx) {
        // Show full item text so truncation never occurs after selection
        agent_selector_->lineEdit()->setText(agent_selector_->itemText(idx));
        agent_selector_->lineEdit()->setCursorPosition(0);
    });

    hl->addWidget(agent_selector_);

    hl->addStretch();

    // Active model pill
    hdr_model_lbl_ = new QLabel("No model");
    hdr_model_lbl_->setStyleSheet(QString("color:%1;font-size:9px;background:%2;border:1px solid %3;"
                                          "border-radius:3px;padding:2px 8px;")
                                      .arg(col::TEXT_SECONDARY(), col::BG_BASE(), col::BORDER_MED()));
    hdr_model_lbl_->setToolTip("Active LLM — configure in Settings > LLM Configuration");
    hl->addWidget(hdr_model_lbl_);

    // Status chip
    hdr_status_lbl_ = new QLabel("Ready");
    hdr_status_lbl_->setFixedWidth(72);
    hdr_status_lbl_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hdr_status_lbl_->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;").arg(col::POSITIVE()));
    hl->addWidget(hdr_status_lbl_);

    auto* div2 = new QFrame;
    div2->setFrameShape(QFrame::VLine);
    div2->setFixedWidth(1);
    div2->setStyleSheet(QString("background:%1;").arg(col::BORDER_DIM()));
    hl->addWidget(div2);

    // Auto-route toggle
    route_toggle_ = new QPushButton("AUTO-ROUTE");
    route_toggle_->setCheckable(true);
    route_toggle_->setCursor(Qt::PointingHandCursor);
    route_toggle_->setFixedHeight(28);
    route_toggle_->setToolTip("When ON, the system picks the best agent for each query.");
    route_toggle_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                "padding:3px 10px;font-size:9px;font-weight:600;border-radius:3px;}"
                "QPushButton:checked{color:%3;border-color:%3;background:rgba(34,197,94,0.08);}"
                "QPushButton:hover{background:%4;}")
            .arg(col::TEXT_SECONDARY(), col::BORDER_MED(), col::POSITIVE(), col::BG_HOVER()));
    hl->addWidget(route_toggle_);

    // Run-as-task toggle — visible only when Agentic Mode is enabled.
    // When ON, send_message() dispatches via AgentService::start_task and the
    // query becomes a durable background task (visible in the AGENTIC tab).
    run_as_task_toggle_ = new QPushButton("RUN AS TASK");
    run_as_task_toggle_->setCheckable(true);
    run_as_task_toggle_->setCursor(Qt::PointingHandCursor);
    run_as_task_toggle_->setFixedHeight(28);
    run_as_task_toggle_->setToolTip("When ON, this query runs as a durable background task with per-step progress.");
    run_as_task_toggle_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                "padding:3px 10px;font-size:9px;font-weight:600;border-radius:3px;}"
                "QPushButton:checked{color:%3;border-color:%3;background:rgba(245,158,11,0.08);}"
                "QPushButton:hover{background:%4;}")
            .arg(col::TEXT_SECONDARY(), col::BORDER_MED(), col::AMBER(), col::BG_HOVER()));
    run_as_task_toggle_->setVisible(false); // gated by agentic_mode_enabled
    hl->addWidget(run_as_task_toggle_);

    // Clear button
    clear_btn_ = new QPushButton("CLEAR");
    clear_btn_->setCursor(Qt::PointingHandCursor);
    clear_btn_->setFixedHeight(28);
    clear_btn_->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                                      "padding:3px 10px;font-size:9px;font-weight:600;border-radius:3px;}"
                                      "QPushButton:hover{background:%3;color:%4;border-color:%4;}")
                                  .arg(col::TEXT_TERTIARY(), col::BORDER_DIM(), col::BG_HOVER(), col::TEXT_SECONDARY()));
    hl->addWidget(clear_btn_);
    root->addWidget(header);

    // ── Portfolio context bar ─────────────────────────────────────────────────
    auto* pbar = new QWidget(this);
    pbar->setFixedHeight(38);
    pbar->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;").arg(col::BG_SURFACE(), col::BORDER_DIM()));
    auto* pl = new QHBoxLayout(pbar);
    pl->setContentsMargins(14, 0, 14, 0);
    pl->setSpacing(8);

    auto* plbl = new QLabel("PORTFOLIO:");
    plbl->setStyleSheet(
        QString("color:%1;font-size:9px;font-weight:600;letter-spacing:0.5px;").arg(col::TEXT_TERTIARY()));
    pl->addWidget(plbl);

    portfolio_combo_ = new QComboBox;
    portfolio_combo_->addItem("None");
    portfolio_combo_->setFixedWidth(160);
    portfolio_combo_->setStyleSheet(QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:2px 8px;"
                                            "font-size:10px;border-radius:3px;}"
                                            "QComboBox::drop-down{border:none;}"
                                            "QComboBox QAbstractItemView{background:%1;color:%2;"
                                            "selection-background-color:%4;border:1px solid %3;}")
                                        .arg(col::BG_BASE(), col::TEXT_PRIMARY(), col::BORDER_DIM(), col::AMBER_DIM()));
    pl->addWidget(portfolio_combo_);

    auto qbtn = [&](const QString& label, const QString& clr) -> QPushButton* {
        auto* b = new QPushButton(label);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(24);
        b->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                                 "font-size:9px;font-weight:600;padding:2px 8px;border-radius:3px;}"
                                 "QPushButton:hover{background:rgba(255,255,255,0.05);border-color:%1;}")
                             .arg(clr, col::BORDER_DIM()));
        return b;
    };
    analyze_btn_ = qbtn("ANALYZE", col::CYAN);
    rebalance_btn_ = qbtn("REBALANCE", col::AMBER);
    risk_btn_ = qbtn("RISK", col::NEGATIVE);
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

    auto* w_title = new QLabel("How can I help you?");
    w_title->setAlignment(Qt::AlignCenter);
    w_title->setStyleSheet(
        QString("color:%1;font-size:22px;font-weight:700;background:transparent;").arg(col::TEXT_PRIMARY()));
    wvl->addWidget(w_title);

    auto* w_sub = new QLabel("Ask about markets, portfolios, or any financial topic.\n"
                             "Select an agent above, or use Auto-Route to let the system decide.");
    w_sub->setAlignment(Qt::AlignCenter);
    w_sub->setWordWrap(true);
    w_sub->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;").arg(col::TEXT_SECONDARY()));
    wvl->addWidget(w_sub);

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
        {"Portfolio", col::POSITIVE, "Analyze my portfolio performance"},
        {"Analytics", col::AMBER, "Calculate valuation for AAPL"},
        {"Risk", col::NEGATIVE, "Run risk analysis on my portfolio"},
    };
    for (const auto& c : chips) {
        auto* btn = new QPushButton(c.label);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:1px solid %3;"
                                   "padding:6px 14px;font-size:10px;font-weight:600;border-radius:4px;}"
                                   "QPushButton:hover{background:%4;}")
                               .arg(col::BG_RAISED(), c.color, col::BORDER_MED(), col::BG_HOVER()));
        const QString q = c.query;
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
    typing_indicator_->setFixedHeight(28);
    typing_indicator_->setStyleSheet("background:transparent;");
    auto* til = new QHBoxLayout(typing_indicator_);
    til->setContentsMargins(4, 0, 0, 0);
    typing_dots_lbl_ = new QLabel("Agent is thinking");
    typing_dots_lbl_->setStyleSheet(
        QString("color:%1;font-size:11px;font-style:italic;background:transparent;").arg(col::TEXT_DIM()));
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
    input_edit_->setPlaceholderText("Message agent... (Shift+Enter for new line, Enter to send)");
    input_edit_->setFixedHeight(44);
    input_edit_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    input_edit_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    input_edit_->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    input_edit_->setFrameShape(QFrame::NoFrame);
    input_edit_->setStyleSheet(QString("QTextEdit{background:%1;color:%2;border:1px solid %3;"
                                       "padding:8px 12px;font-size:13px;border-radius:4px;}"
                                       "QTextEdit:focus{border-color:%4;}")
                                   .arg(col::BG_BASE(), col::TEXT_PRIMARY(), col::BORDER_MED(), col::AMBER()));
    input_edit_->installEventFilter(this);
    // Grow height as user types (max ~120px / ~5 lines)
    connect(input_edit_->document(), &QTextDocument::contentsChanged, input_edit_, [this]() {
        int doc_h = static_cast<int>(input_edit_->document()->size().height());
        int new_h = qBound(44, doc_h + 18, 120);
        input_edit_->setFixedHeight(new_h);
    });
    il->addWidget(input_edit_, 1);

    send_btn_ = new QPushButton("Send");
    send_btn_->setFixedSize(76, 44);
    send_btn_->setCursor(Qt::PointingHandCursor);
    send_btn_->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:none;border-radius:6px;"
                                     "font-size:12px;font-weight:700;}"
                                     "QPushButton:hover:enabled{background:%3;}"
                                     "QPushButton:disabled{background:%4;color:%5;}")
                                 .arg(col::AMBER(), col::BG_BASE(), col::ORANGE(), col::BG_RAISED(), col::TEXT_TERTIARY()));
    il->addWidget(send_btn_);
    root->addWidget(ib);

    // ── Status bar ────────────────────────────────────────────────────────────
    status_label_ = new QLabel;
    status_label_->setFixedHeight(18);
    status_label_->setStyleSheet(
        QString("background:%1;color:%2;font-size:9px;padding:0 14px;").arg(col::BG_SURFACE(), col::TEXT_TERTIARY()));
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
    connect(send_btn_, &QPushButton::clicked, this, &AgentChatPanel::send_message);
    connect(clear_btn_, &QPushButton::clicked, this, &AgentChatPanel::clear_chat);

    connect(route_toggle_, &QPushButton::toggled, this, [this](bool on) {
        auto_routing_ = on;
        route_toggle_->setText(on ? "AUTO-ROUTE: ON" : "AUTO-ROUTE");
        agent_selector_->setEnabled(!on);
    });

    connect(run_as_task_toggle_, &QPushButton::toggled, this, [this](bool on) {
        run_as_task_ = on;
        run_as_task_toggle_->setText(on ? "RUN AS TASK: ON" : "RUN AS TASK");
    });

    // Visibility of the "RUN AS TASK" toggle follows the Agentic Mode setting.
    auto apply_agentic_visibility = [this]() {
        auto r = fincept::SettingsRepository::instance().get(
            QStringLiteral("agentic_mode_enabled"), QStringLiteral("false"));
        const bool on = r.is_ok() && r.value() == QStringLiteral("true");
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
                agent_selector_->addItem("Default (global LLM)", QString{});
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
                status_label_->setText("Streaming...");
            });

    connect(&svc, &services::AgentService::agent_stream_done, this, [this](services::AgentExecutionResult r) {
        if (r.request_id != pending_request_id_)
            return;
        show_typing(false);
        set_executing(false);

        if (streaming_bubble_widget_) {
            if (!r.success) {
                streaming_bubble_widget_->setPlainText("Error: " + r.error);
            } else {
                // Replace streamed plain-text tokens with fully rendered markdown HTML.
                const QString final_text = r.response.isEmpty() ? streaming_text_ : r.response;
                if (!final_text.trimmed().isEmpty()) {
                    streaming_bubble_widget_->setHtml(ui::MarkdownRenderer::render(final_text));
                    streaming_bubble_widget_->document()->setTextWidth(600);
                    const int h = static_cast<int>(streaming_bubble_widget_->document()->size().height());
                    streaming_bubble_widget_->setMinimumHeight(qMax(h + 8, 28));
                    streaming_bubble_widget_->setMaximumHeight(qMax(h + 8, 28));
                }
            }
            streaming_bubble_widget_->setReadOnly(true);
            streaming_bubble_widget_ = nullptr;
        } else if (r.success && !r.response.isEmpty()) {
            add_assistant_bubble(r.response);
        } else if (!r.success) {
            add_system_bubble("Error: " + r.error);
        }

        streaming_text_.clear();
        if (r.success) {
            status_label_->setText(QString("Response received (%1ms)").arg(r.execution_time_ms));
            hdr_status_lbl_->setText("Ready");
            hdr_status_lbl_->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;").arg(col::POSITIVE()));
        } else {
            status_label_->setText("Agent execution failed");
            hdr_status_lbl_->setText("Error");
            hdr_status_lbl_->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;").arg(col::NEGATIVE()));
        }
        scroll_to_bottom();
    });

    // Routing result
    connect(&svc, &services::AgentService::routing_result, this, [this](services::RoutingResult r) {
        if (r.request_id != pending_request_id_)
            return;
        if (r.success) {
            add_system_bubble(QString("Routed to: %1 (intent: %2, confidence: %3%)")
                                  .arg(r.agent_id, r.intent)
                                  .arg(static_cast<int>(r.confidence * 100)));
            pending_request_id_ = services::AgentService::instance().run_agent_streaming(last_query_, r.config);
        } else {
            add_system_bubble("Auto-routing failed — using default agent.");
            pending_request_id_ = services::AgentService::instance().run_agent_streaming(last_query_, {});
        }
    });

    // Portfolio quick actions
    connect(analyze_btn_, &QPushButton::clicked, this, [this]() {
        const QString pf = portfolio_combo_->currentText();
        if (pf == "None")
            return;
        input_edit_->setPlainText(QString("Analyze my portfolio '%1' — give key metrics and recommendations.").arg(pf));
        send_message();
    });
    connect(rebalance_btn_, &QPushButton::clicked, this, [this]() {
        const QString pf = portfolio_combo_->currentText();
        if (pf == "None")
            return;
        input_edit_->setPlainText(QString("Suggest rebalancing for portfolio '%1' to optimize risk-return.").arg(pf));
        send_message();
    });
    connect(risk_btn_, &QPushButton::clicked, this, [this]() {
        const QString pf = portfolio_combo_->currentText();
        if (pf == "None")
            return;
        input_edit_->setPlainText(
            QString("Perform risk analysis on portfolio '%1' — VaR, drawdown, stress test.").arg(pf));
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

void AgentChatPanel::update_llm_status() {
    auto& llm = ai_chat::LlmService::instance();
    if (llm.is_configured()) {
        const QString label = QString("%1 / %2").arg(llm.active_provider(), llm.active_model());
        hdr_model_lbl_->setText(label);
        hdr_model_lbl_->setStyleSheet(QString("color:%1;font-size:9px;background:%2;border:1px solid %3;"
                                              "border-radius:3px;padding:2px 8px;")
                                          .arg(col::TEXT_SECONDARY(), col::BG_BASE(), col::BORDER_MED()));
        hdr_status_lbl_->setText("Ready");
        hdr_status_lbl_->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;").arg(col::POSITIVE()));
    } else {
        hdr_model_lbl_->setText("No LLM configured");
        hdr_model_lbl_->setStyleSheet(QString("color:%1;font-size:9px;background:%2;border:1px solid %3;"
                                              "border-radius:3px;padding:2px 8px;")
                                          .arg(col::NEGATIVE(), col::BG_BASE(), col::NEGATIVE()));
        hdr_status_lbl_->setText("Unconfigured");
        hdr_status_lbl_->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;").arg(col::NEGATIVE()));
        status_label_->setText("No LLM provider configured — go to Settings > LLM Configuration");
    }
}

// ── send_message ─────────────────────────────────────────────────────────────

void AgentChatPanel::send_message() {
    const QString text = input_edit_->toPlainText().trimmed();
    if (text.isEmpty() || executing_)
        return;

    // Guard: require LLM config
    auto& llm = ai_chat::LlmService::instance();
    if (!llm.is_configured()) {
        add_system_bubble("No LLM provider configured. Go to Settings > LLM Configuration to set one up.");
        return;
    }

    set_executing(true);
    show_welcome(false);

    // Show the raw user text in the bubble (without the injected context blob)
    add_user_bubble(text);
    input_edit_->clear();
    input_edit_->setFixedHeight(44);

    // Build enriched query: prepend portfolio context if one is selected
    const QString pf_ctx = build_portfolio_context();
    last_query_ = pf_ctx.isEmpty() ? text : QString("%1\n\nUser question: %2").arg(pf_ctx, text);

    // Show which portfolio is active in the status bar
    if (!pf_ctx.isEmpty()) {
        const QString pf_name = portfolio_combo_->currentText();
        status_label_->setText(QString("Portfolio context: %1").arg(pf_name));
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
            streaming_bubble_widget_->setPlainText(
                QStringLiteral("Task started. Open the AGENTIC tab to watch progress."));
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
    if (pf_name == "None" || pf_name.isEmpty())
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
    portfolio_combo_->addItem("None");
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
        QString("color:%1;font-size:9px;font-weight:600;background:transparent;").arg(role_color(role)));
    cvl->addWidget(role_lbl);

    auto* bubble = new QFrame;
    bubble->setStyleSheet(bubble_style(role));
    auto* bvl = new QVBoxLayout(bubble);
    bvl->setContentsMargins(0, 0, 0, 0);
    auto* body = new QLabel(text);
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->setTextFormat(Qt::PlainText);
    body->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;").arg(body_color(role)));
    bvl->addWidget(body);
    cvl->addWidget(bubble);

    auto* ts_lbl = new QLabel(ts);
    ts_lbl->setAlignment(Qt::AlignRight);
    ts_lbl->setStyleSheet(QString("color:%1;font-size:9px;background:transparent;").arg(col::TEXT_DIM()));
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
    auto* role_lbl = new QLabel(agent_name.isEmpty() ? "Agent" : agent_name);
    role_lbl->setStyleSheet(
        QString("color:%1;font-size:9px;font-weight:600;background:transparent;").arg(role_color(role)));
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
        QString("QTextEdit{background:transparent;color:%1;border:none;font-size:12px;}").arg(body_color(role)));
    body->setHtml(ui::MarkdownRenderer::render(text));
    // Size to content using same fixed-width approach as streaming bubble
    body->document()->setTextWidth(600);
    const int doc_h = static_cast<int>(body->document()->size().height());
    body->setMinimumHeight(qMax(doc_h + 8, 28));
    body->setMaximumHeight(qMax(doc_h + 8, 28));

    bvl->addWidget(body);
    cvl->addWidget(bubble);

    auto* ts_lbl = new QLabel(ts);
    ts_lbl->setAlignment(Qt::AlignLeft);
    ts_lbl->setStyleSheet(QString("color:%1;font-size:9px;background:transparent;").arg(col::TEXT_DIM()));
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
    body->setStyleSheet(QString("color:%1;font-size:11px;background:transparent;").arg(body_color(role)));
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

    auto* role_lbl = new QLabel(agent_name.isEmpty() ? "Agent" : agent_name);
    role_lbl->setStyleSheet(QString("color:%1;font-size:9px;font-weight:600;background:transparent;").arg(col::CYAN()));
    cvl->addWidget(role_lbl);

    auto* bubble = new QFrame;
    bubble->setStyleSheet(bubble_style("assistant"));
    auto* bvl = new QVBoxLayout(bubble);
    bvl->setContentsMargins(0, 0, 0, 0);

    auto* body = new QTextEdit;
    body->setReadOnly(false);
    body->setFrameShape(QFrame::NoFrame);
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    body->document()->setDocumentMargin(4);
    body->setMinimumHeight(28);
    body->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    body->setStyleSheet(
        QString("QTextEdit{background:transparent;color:%1;border:none;font-size:12px;}").arg(col::TEXT_PRIMARY()));
    // Reliable auto-grow: recompute at a fixed known width (600px matches col_w maxWidth 680
    // minus bubble padding). No dependency on viewport()->width() which can be 0 pre-layout.
    auto recompute_height = [body]() {
        body->document()->setTextWidth(600);
        const int doc_h = static_cast<int>(body->document()->size().height());
        body->setMinimumHeight(qMax(doc_h + 8, 28));
        body->setMaximumHeight(qMax(doc_h + 8, 28));
    };
    connect(body->document(), &QTextDocument::contentsChanged, body, recompute_height);
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
    send_btn_->setEnabled(!on);
    send_btn_->setText(on ? "..." : "Send");
    if (on) {
        hdr_status_lbl_->setText("Streaming");
        hdr_status_lbl_->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;").arg(col::AMBER()));
        status_label_->setText("Processing...");
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
        typing_step_ = 0;
        typing_dots_lbl_->setText("Agent is thinking");
        typing_indicator_->show();
        typing_timer_->start();
    } else {
        typing_timer_->stop();
        typing_indicator_->hide();
    }
}

void AgentChatPanel::clear_chat() {
    // Null streaming refs before deleting widgets
    streaming_bubble_widget_ = nullptr;
    streaming_text_.clear();
    show_typing(false);
    set_executing(false);

    // Remove all message rows. Layout structure:
    //   [0]        = welcome_panel_
    //   [1..N]     = message rows
    //   [count-2]  = typing_indicator_
    //   [count-1]  = stretch
    // Stop when only welcome + typing_indicator_ + stretch remain (count == 3).
    // Always remove at index 1 so we never touch welcome(0), typing(count-2) or stretch(count-1).
    while (messages_layout_->count() > 3) {
        QLayoutItem* item = messages_layout_->takeAt(1);
        if (item && item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    show_welcome(true);
    hdr_status_lbl_->setText("Ready");
    hdr_status_lbl_->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;").arg(col::POSITIVE()));
    status_label_->clear();
    update_llm_status();
}

} // namespace fincept::screens
