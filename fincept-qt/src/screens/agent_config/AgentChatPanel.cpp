// src/screens/agent_config/AgentChatPanel.cpp
#include "screens/agent_config/AgentChatPanel.h"

#include "core/logging/Logger.h"
#include "services/agents/AgentService.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QTimer>

namespace fincept::screens {

// ── Bubble builder ───────────────────────────────────────────────────────────

static QWidget* make_bubble(const QString& text, const QString& role,
                             const QString& timestamp, const QString& agent_name = {}) {
    auto* bubble = new QWidget;
    auto* vl = new QVBoxLayout(bubble);
    vl->setContentsMargins(0, 4, 0, 4);
    vl->setSpacing(2);

    bool is_user = (role == "user");
    bool is_system = (role == "system");

    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    if (is_user) row->addStretch();

    auto* card = new QFrame;
    card->setMaximumWidth(600);

    QString bg, tc, bd;
    if (is_user) {
        bg = ui::colors::AMBER_DIM; tc = ui::colors::TEXT_PRIMARY; bd = ui::colors::AMBER;
    } else if (is_system) {
        bg = ui::colors::BG_RAISED; tc = ui::colors::CYAN; bd = ui::colors::BORDER_MED;
    } else {
        bg = ui::colors::BG_SURFACE; tc = ui::colors::TEXT_PRIMARY; bd = ui::colors::BORDER_DIM;
    }
    card->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;padding:10px;}").arg(bg, bd));

    auto* cl = new QVBoxLayout(card);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(4);

    // Role + agent name header
    auto* hdr_row = new QHBoxLayout;
    auto* role_lbl = new QLabel(is_user ? "YOU" : is_system ? "SYSTEM" : "AGENT");
    role_lbl->setStyleSheet(
        QString("color:%1;font-size:9px;font-weight:700;letter-spacing:1px;")
            .arg(is_user ? ui::colors::AMBER : is_system ? ui::colors::CYAN : ui::colors::POSITIVE));
    hdr_row->addWidget(role_lbl);

    if (!agent_name.isEmpty() && !is_user && !is_system) {
        auto* name_badge = new QLabel(agent_name);
        name_badge->setStyleSheet(
            QString("color:%1;font-size:8px;font-weight:600;background:%2;padding:1px 4px;border-radius:2px;")
                .arg(ui::colors::CYAN, ui::colors::BG_RAISED));
        hdr_row->addWidget(name_badge);
    }
    hdr_row->addStretch();

    // Timestamp
    auto* time_lbl = new QLabel(timestamp);
    time_lbl->setStyleSheet(QString("color:%1;font-size:9px;").arg(ui::colors::TEXT_DIM));
    hdr_row->addWidget(time_lbl);
    cl->addLayout(hdr_row);

    // Content
    auto* content = new QTextEdit;
    content->setReadOnly(true);
    content->setPlainText(text);
    content->setFrameShape(QFrame::NoFrame);
    content->setStyleSheet(
        QString("QTextEdit{background:transparent;color:%1;font-size:12px;border:none;padding:0;}").arg(tc));
    content->document()->setDocumentMargin(0);
    content->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    content->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QObject::connect(content->document(), &QTextDocument::contentsChanged, content, [content]() {
        content->setFixedHeight(static_cast<int>(content->document()->size().height()) + 4);
    });
    cl->addWidget(content);

    row->addWidget(card);
    if (!is_user) row->addStretch();
    vl->addLayout(row);
    return bubble;
}

// ── Constructor ──────────────────────────────────────────────────────────────

AgentChatPanel::AgentChatPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("AgentChatPanel");
    build_ui();
    setup_connections();
}

void AgentChatPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header
    auto* header = new QWidget;
    header->setFixedHeight(40);
    header->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(12, 0, 12, 0);

    auto* title = new QLabel("AGENT CHAT");
    title->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER));
    hl->addWidget(title);
    hl->addStretch();

    route_toggle_ = new QPushButton("AUTO-ROUTE: OFF");
    route_toggle_->setCheckable(true);
    route_toggle_->setCursor(Qt::PointingHandCursor);
    route_toggle_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;padding:4px 10px;"
                "font-size:9px;font-weight:600;}QPushButton:checked{color:%3;border-color:%3;}"
                "QPushButton:hover{background:%4;}")
            .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED, ui::colors::POSITIVE, ui::colors::BG_HOVER));
    hl->addWidget(route_toggle_);

    clear_btn_ = new QPushButton("CLEAR");
    clear_btn_->setCursor(Qt::PointingHandCursor);
    clear_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;padding:4px 10px;"
                "font-size:9px;font-weight:600;}QPushButton:hover{background:%2;}")
            .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED));
    hl->addWidget(clear_btn_);
    root->addWidget(header);

    // Messages area
    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area_->setStyleSheet(
        QString("QScrollArea{background:%1;border:none;}"
                "QScrollBar:vertical{background:%1;width:6px;}"
                "QScrollBar::handle:vertical{background:%2;min-height:20px;}")
            .arg(ui::colors::BG_BASE, ui::colors::BORDER_BRIGHT));

    messages_container_ = new QWidget;
    messages_layout_ = new QVBoxLayout(messages_container_);
    messages_layout_->setContentsMargins(16, 12, 16, 12);
    messages_layout_->setSpacing(4);
    messages_layout_->addStretch();
    scroll_area_->setWidget(messages_container_);
    root->addWidget(scroll_area_, 1);

    // Portfolio context bar
    build_portfolio_bar(root);

    // Input bar
    auto* ib = new QWidget;
    ib->setFixedHeight(52);
    ib->setStyleSheet(QString("background:%1;border-top:1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
    auto* il = new QHBoxLayout(ib);
    il->setContentsMargins(12, 8, 12, 8);
    il->setSpacing(8);

    input_edit_ = new QLineEdit;
    input_edit_->setPlaceholderText("Ask an agent...");
    input_edit_->setStyleSheet(
        QString("QLineEdit{background:%1;color:%2;border:1px solid %3;padding:8px 12px;font-size:13px;}")
            .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED));
    il->addWidget(input_edit_, 1);

    send_btn_ = new QPushButton("SEND");
    send_btn_->setCursor(Qt::PointingHandCursor);
    send_btn_->setFixedWidth(80);
    send_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:none;padding:8px;font-size:11px;"
                "font-weight:700;letter-spacing:1px;}QPushButton:hover{background:%3;}"
                "QPushButton:disabled{background:%4;color:%5;}")
            .arg(ui::colors::AMBER, ui::colors::BG_BASE, ui::colors::ORANGE,
                 ui::colors::BG_RAISED, ui::colors::TEXT_TERTIARY));
    il->addWidget(send_btn_);
    root->addWidget(ib);

    // Status
    status_label_ = new QLabel;
    status_label_->setFixedHeight(20);
    status_label_->setStyleSheet(QString("background:%1;color:%2;font-size:9px;padding:2px 12px;")
                                     .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_TERTIARY));
    root->addWidget(status_label_);

    add_system_bubble("Welcome to Agent Chat. Toggle AUTO-ROUTE to let the system pick the best agent.");
}

void AgentChatPanel::build_portfolio_bar(QVBoxLayout* root) {
    portfolio_bar_ = new QWidget;
    portfolio_bar_->setFixedHeight(36);
    portfolio_bar_->setStyleSheet(
        QString("background:%1;border-top:1px solid %2;border-bottom:1px solid %2;")
            .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* pl = new QHBoxLayout(portfolio_bar_);
    pl->setContentsMargins(12, 4, 12, 4);
    pl->setSpacing(8);

    auto* lbl = new QLabel("PORTFOLIO CTX:");
    lbl->setStyleSheet(QString("color:%1;font-size:9px;font-weight:600;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY));
    pl->addWidget(lbl);

    portfolio_combo_ = new QComboBox;
    portfolio_combo_->addItem("None");
    portfolio_combo_->setStyleSheet(
        QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:2px 8px;font-size:10px;}"
                "QComboBox::drop-down{border:none;}")
            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED));
    pl->addWidget(portfolio_combo_);

    auto quick_btn_style =
        QString("QPushButton{background:transparent;color:%1;border:none;font-size:9px;font-weight:700;"
                "padding:2px 6px;}QPushButton:hover{color:%2;}")
            .arg(ui::colors::CYAN, ui::colors::AMBER);

    analyze_btn_ = new QPushButton("ANALYZE");
    analyze_btn_->setCursor(Qt::PointingHandCursor);
    analyze_btn_->setStyleSheet(quick_btn_style);
    pl->addWidget(analyze_btn_);

    rebalance_btn_ = new QPushButton("REBALANCE");
    rebalance_btn_->setCursor(Qt::PointingHandCursor);
    rebalance_btn_->setStyleSheet(quick_btn_style);
    pl->addWidget(rebalance_btn_);

    risk_btn_ = new QPushButton("RISK");
    risk_btn_->setCursor(Qt::PointingHandCursor);
    risk_btn_->setStyleSheet(quick_btn_style);
    pl->addWidget(risk_btn_);

    pl->addStretch();
    root->addWidget(portfolio_bar_);
}

void AgentChatPanel::setup_connections() {
    connect(send_btn_, &QPushButton::clicked, this, &AgentChatPanel::send_message);
    connect(clear_btn_, &QPushButton::clicked, this, &AgentChatPanel::clear_chat);
    connect(input_edit_, &QLineEdit::returnPressed, this, &AgentChatPanel::send_message);
    connect(route_toggle_, &QPushButton::toggled, this, [this](bool c) {
        auto_routing_ = c;
        route_toggle_->setText(c ? "AUTO-ROUTE: ON" : "AUTO-ROUTE: OFF");
    });

    // Portfolio quick actions
    connect(analyze_btn_, &QPushButton::clicked, this, [this]() {
        QString pf = portfolio_combo_->currentText();
        if (pf != "None") {
            input_edit_->setText(QString("Analyze my portfolio '%1' — give key metrics and recommendations.").arg(pf));
            send_message();
        }
    });
    connect(rebalance_btn_, &QPushButton::clicked, this, [this]() {
        QString pf = portfolio_combo_->currentText();
        if (pf != "None") {
            input_edit_->setText(QString("Suggest rebalancing for my portfolio '%1' to optimize risk-return.").arg(pf));
            send_message();
        }
    });
    connect(risk_btn_, &QPushButton::clicked, this, [this]() {
        QString pf = portfolio_combo_->currentText();
        if (pf != "None") {
            input_edit_->setText(QString("Perform risk analysis on my portfolio '%1' — VaR, drawdown, stress test.").arg(pf));
            send_message();
        }
    });

    auto& svc = services::AgentService::instance();

    connect(&svc, &services::AgentService::routing_result, this, [this](services::RoutingResult r) {
        if (r.success) {
            add_system_bubble(
                QString("Routed to: %1 (intent: %2, confidence: %3%)")
                    .arg(r.agent_id, r.intent).arg(int(r.confidence * 100)));
            services::AgentService::instance().run_agent(last_query_, r.config);
        } else {
            add_system_bubble("Routing failed, using default agent.");
            services::AgentService::instance().run_agent(last_query_, {});
        }
    });

    connect(&svc, &services::AgentService::agent_result, this, [this](services::AgentExecutionResult r) {
        executing_ = false;
        send_btn_->setEnabled(true);
        send_btn_->setText("SEND");
        if (r.success) {
            add_assistant_bubble(r.response);
            status_label_->setText(QString("Response received (%1ms)").arg(r.execution_time_ms));
        } else {
            add_system_bubble("Error: " + r.error);
            status_label_->setText("Agent execution failed");
        }
    });
}

void AgentChatPanel::send_message() {
    QString text = input_edit_->text().trimmed();
    if (text.isEmpty() || executing_) return;

    executing_ = true;
    send_btn_->setEnabled(false);
    send_btn_->setText("...");
    status_label_->setText("Processing...");

    add_user_bubble(text);
    last_query_ = text;
    input_edit_->clear();

    if (auto_routing_)
        services::AgentService::instance().route_query(last_query_);
    else
        services::AgentService::instance().run_agent(last_query_, {});
}

void AgentChatPanel::add_user_bubble(const QString& text) {
    auto* b = make_bubble(text, "user", QDateTime::currentDateTime().toString("hh:mm"));
    messages_layout_->insertWidget(messages_layout_->count() - 1, b);
    scroll_to_bottom();
}

void AgentChatPanel::add_assistant_bubble(const QString& text, const QString& agent_name) {
    auto* b = make_bubble(text, "assistant", QDateTime::currentDateTime().toString("hh:mm"), agent_name);
    messages_layout_->insertWidget(messages_layout_->count() - 1, b);
    scroll_to_bottom();
}

void AgentChatPanel::add_system_bubble(const QString& text) {
    auto* b = make_bubble(text, "system", QDateTime::currentDateTime().toString("hh:mm"));
    messages_layout_->insertWidget(messages_layout_->count() - 1, b);
    scroll_to_bottom();
}

void AgentChatPanel::scroll_to_bottom() {
    QTimer::singleShot(50, this, [this]() {
        scroll_area_->verticalScrollBar()->setValue(scroll_area_->verticalScrollBar()->maximum());
    });
}

void AgentChatPanel::clear_chat() {
    while (messages_layout_->count() > 1) {
        auto* item = messages_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    status_label_->clear();
    add_system_bubble("Chat cleared. Start a new conversation.");
}

} // namespace fincept::screens
