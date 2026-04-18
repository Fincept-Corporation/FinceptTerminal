#include "screens/alpha_arena/AlphaArenaScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "services/alpha_arena/AlphaArenaService.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "ui/theme/Theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QPointer>
#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>
#include <QVBoxLayout>

// ── Stylesheet ──────────────────────────────────────────────────────────────

namespace {
using namespace fincept::ui;

static const QString kStyle =
    QStringLiteral("#aaScreen { background: %1; }"

                   "#aaHeader { background: %2; border-bottom: 2px solid %3; }"
                   "#aaHeaderTitle { color: %4; font-size: 12px; font-weight: 700; background: transparent; }"
                   "#aaHeaderSub { color: %5; font-size: 9px; letter-spacing: 0.5px; background: transparent; }"

                   "#aaStatusBadge { font-size: 8px; font-weight: 700; padding: 2px 6px; }"
                   "#aaCycleLabel { color: %13; font-size: 11px; font-weight: 700; background: transparent; }"
                   "#aaPriceLabel { color: %6; font-size: 11px; font-weight: 700; "
                   "  background: rgba(22,163,74,0.15); padding: 2px 8px; }"

                   "#aaCreatePanel { background: %7; border: 1px solid %8; }"
                   "#aaCreateHeader { background: %2; border-bottom: 1px solid %8; }"
                   "#aaCreateTitle { color: %4; font-size: 11px; font-weight: 700; background: transparent; }"

                   "#aaLabel { color: %5; font-size: 9px; font-weight: 700; "
                   "  letter-spacing: 0.5px; background: transparent; }"
                   "QLineEdit { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: 11px; }"
                   "QComboBox { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: 11px; }"
                   "QComboBox::drop-down { border: none; width: 16px; }"
                   "QComboBox QAbstractItemView { background: %2; color: %4; border: 1px solid %8; "
                   "  selection-background-color: %3; }"
                   "QDoubleSpinBox, QSpinBox { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: 11px; }"
                   "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button, "
                   "QSpinBox::up-button, QSpinBox::down-button { width: 0; }"

                   "#aaCreateBtn { background: %3; color: %1; border: none; padding: 6px 16px; "
                   "  font-size: 10px; font-weight: 700; }"
                   "#aaCreateBtn:hover { background: #FF8800; }"
                   "#aaCreateBtn:disabled { background: %10; color: %11; }"

                   "#aaControlsBar { background: %2; border-bottom: 1px solid %8; }"
                   "#aaRunBtn { background: %3; color: %1; border: none; padding: 5px 14px; "
                   "  font-size: 9px; font-weight: 700; }"
                   "#aaRunBtn:hover { background: #FF8800; }"
                   "#aaRunBtn:disabled { background: %10; color: %11; }"
                   "#aaAutoBtn { background: %7; color: %5; border: 1px solid %8; "
                   "  padding: 5px 14px; font-size: 9px; font-weight: 700; }"
                   "#aaAutoBtn[running=\"true\"] { color: %14; border-color: %14; }"
                   "#aaResetBtn { background: %7; color: %5; border: 1px solid %8; "
                   "  padding: 5px 14px; font-size: 9px; font-weight: 700; }"
                   "#aaIntervalLabel { color: %5; font-size: 9px; background: transparent; }"

                   "#aaLeaderboard { background: %7; border: 1px solid %8; }"
                   "#aaLeaderboardHeader { background: %2; border-bottom: 1px solid %8; }"
                   "#aaLeaderboardTitle { color: %4; font-size: 11px; font-weight: 700; background: transparent; }"

                   "QTableWidget { background: %1; color: %4; border: none; gridline-color: %8; "
                   "  font-size: 11px; }"
                   "QTableWidget::item { padding: 2px 6px; border-bottom: 1px solid %8; }"
                   "QHeaderView::section { background: %2; color: %5; border: none; "
                   "  border-bottom: 1px solid %8; border-right: 1px solid %8; "
                   "  padding: 4px 6px; font-size: 10px; font-weight: 700; }"

                   "#aaRightPanel { background: %7; border-left: 1px solid %8; }"
                   "#aaRightTabBtn { background: transparent; color: %5; border: none; "
                   "  font-size: 9px; font-weight: 700; padding: 6px 10px; "
                   "  border-bottom: 2px solid transparent; }"
                   "#aaRightTabBtn:hover { color: %4; }"
                   "#aaRightTabBtn[active=\"true\"] { color: %4; border-bottom-color: %3; }"

                   "#aaDecisionList { background: %1; border: none; outline: none; font-size: 11px; }"
                   "#aaDecisionList::item { color: %5; padding: 6px 8px; border-bottom: 1px solid %8; }"
                   "#aaDecisionList::item:selected { color: %3; background: rgba(217,119,6,0.1); }"

                   "QTextEdit { background: %1; color: %13; border: none; font-size: 11px; }"

                   "#aaModelList { background: %1; border: none; outline: none; font-size: 11px; }"
                   "#aaModelList::item { color: %5; padding: 4px 8px; border-bottom: 1px solid %8; }"
                   "#aaModelList::item:selected { color: %3; background: rgba(217,119,6,0.1); }"
                   "#aaModelList::item:hover { color: %4; background: %12; }"

                   "#aaPastPanel { background: %7; border: 1px solid %8; }"
                   "#aaPastList { background: %1; border: none; font-size: 11px; }"
                   "#aaPastList::item { color: %5; padding: 6px 8px; border-bottom: 1px solid %8; }"
                   "#aaPastList::item:hover { color: %4; background: %12; }"

                   "#aaStatusBar { background: %2; border-top: 1px solid %8; }"
                   "#aaStatusText { color: %5; font-size: 9px; background: transparent; }"
                   "#aaStatusHighlight { color: %13; font-size: 9px; background: transparent; }"

                   "QSplitter::handle { background: %8; }"
                   "QScrollBar:vertical { background: %1; width: 6px; }"
                   "QScrollBar::handle:vertical { background: %8; min-height: 20px; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(colors::BG_BASE())        // %1
        .arg(colors::BG_RAISED())      // %2
        .arg(colors::AMBER())          // %3
        .arg(colors::TEXT_PRIMARY())   // %4
        .arg(colors::TEXT_SECONDARY()) // %5
        .arg(colors::POSITIVE())       // %6
        .arg(colors::BG_SURFACE())     // %7
        .arg(colors::BORDER_DIM())     // %8
        .arg(colors::BORDER_BRIGHT())  // %9
        .arg(colors::AMBER_DIM())      // %10
        .arg(colors::TEXT_DIM())       // %11
        .arg(colors::BG_HOVER())       // %12
        .arg(colors::CYAN())           // %13
        .arg(colors::NEGATIVE())       // %14
    ;

// ── Trading symbols ─────────────────────────────────────────────────────────

static const QStringList CRYPTO_SYMBOLS = {
    "BTC/USD",  "ETH/USD",   "SOL/USD",  "XRP/USD", "DOGE/USD", "ADA/USD", "AVAX/USD", "DOT/USD",
    "LINK/USD", "MATIC/USD", "ATOM/USD", "UNI/USD", "LTC/USD",  "BCH/USD", "APT/USD",  "ARB/USD",
    "OP/USD",   "FIL/USD",   "NEAR/USD", "INJ/USD", "SUI/USD",  "SEI/USD", "TIA/USD",  "RENDER/USD",
    "FET/USD",  "AAVE/USD",  "MKR/USD",  "CRV/USD", "PEPE/USD", "WIF/USD",
};

static const QStringList COMP_MODES = {"baseline", "monk", "situational", "max_leverage"};

// ── Model colors for leaderboard ────────────────────────────────────────────

static const QStringList MODEL_COLORS = {
    "#FF8800", "#00E5FF", "#9D4EDD", "#00D66F", "#FF3B3B", "#FFD700", "#0088FF", "#E91E63", "#4CAF50", "#FF5722",
};
} // namespace

namespace fincept::screens {

using namespace fincept::ui;

// ── Constructor ─────────────────────────────────────────────────────────────

AlphaArenaScreen::AlphaArenaScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("aaScreen");
    setStyleSheet(kStyle);

    auto_timer_ = new QTimer(this);
    auto_timer_->setInterval(150000); // 150s default
    connect(auto_timer_, &QTimer::timeout, this, &AlphaArenaScreen::on_run_cycle);

    setup_ui();
    populate_model_list();
    LOG_INFO("AlphaArena", "Screen constructed");
}

void AlphaArenaScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    populate_model_list(); // refresh in case user changed LLM config in Settings
    if (is_auto_running_)
        auto_timer_->start();
}

void AlphaArenaScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    auto_timer_->stop();
}

// ── UI Setup ────────────────────────────────────────────────────────────────

void AlphaArenaScreen::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(create_header());

    // Create panel (collapsible)
    create_panel_ = create_create_panel();
    root->addWidget(create_panel_);

    // Controls bar
    root->addWidget(create_controls_bar());

    // Main content
    root->addWidget(create_main_content(), 1);

    // Past competitions (collapsible)
    past_panel_ = create_past_competitions_panel();
    past_panel_->hide();
    root->addWidget(past_panel_);

    root->addWidget(create_status_bar());
}

QWidget* AlphaArenaScreen::create_header() {
    auto* bar = new QWidget(this);
    bar->setObjectName("aaHeader");
    bar->setFixedHeight(42);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(10);

    auto* title = new QLabel("ALPHA ARENA");
    title->setObjectName("aaHeaderTitle");
    hl->addWidget(title);

    status_badge_ = new QLabel("READY");
    status_badge_->setObjectName("aaStatusBadge");
    status_badge_->setStyleSheet(QString("color: %1; background: rgba(217,119,6,0.15); "
                                         "font-size: 8px; font-weight: 700; padding: 2px 6px;")
                                     .arg(colors::AMBER()));
    hl->addWidget(status_badge_);

    cycle_label_ = new QLabel("CYCLE 0");
    cycle_label_->setObjectName("aaCycleLabel");
    hl->addWidget(cycle_label_);

    price_label_ = new QLabel;
    price_label_->setObjectName("aaPriceLabel");
    price_label_->hide();
    hl->addWidget(price_label_);

    hl->addStretch(1);

    auto* past_btn = new QPushButton("HISTORY");
    past_btn->setObjectName("aaResetBtn");
    past_btn->setCursor(Qt::PointingHandCursor);
    connect(past_btn, &QPushButton::clicked, this, &AlphaArenaScreen::on_past_competitions_toggle);
    hl->addWidget(past_btn);

    auto* refresh_btn = new QPushButton("REFRESH");
    refresh_btn->setObjectName("aaResetBtn");
    refresh_btn->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn, &QPushButton::clicked, this, &AlphaArenaScreen::on_refresh_leaderboard);
    hl->addWidget(refresh_btn);

    return bar;
}

QWidget* AlphaArenaScreen::create_create_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("aaCreatePanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* hdr = new QWidget(this);
    hdr->setObjectName("aaCreateHeader");
    hdr->setFixedHeight(34);
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(12, 0, 12, 0);
    auto* ht = new QLabel("CREATE COMPETITION");
    ht->setObjectName("aaCreateTitle");
    hhl->addWidget(ht);
    hhl->addStretch(1);
    vl->addWidget(hdr);

    // Body
    auto* body = new QWidget(this);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 12, 12, 12);
    bl->setSpacing(10);

    // Row 1: Name + Type
    auto* r1 = new QHBoxLayout;
    r1->setSpacing(8);

    auto* name_col = new QVBoxLayout;
    auto* name_lbl = new QLabel("COMPETITION NAME");
    name_lbl->setObjectName("aaLabel");
    comp_name_ = new QLineEdit("Alpha Arena Competition");
    name_col->addWidget(name_lbl);
    name_col->addWidget(comp_name_);
    r1->addLayout(name_col, 2);

    auto* type_col = new QVBoxLayout;
    auto* type_lbl = new QLabel("TYPE");
    type_lbl->setObjectName("aaLabel");
    comp_type_ = new QComboBox;
    comp_type_->addItems({"Crypto Trading", "Prediction Markets"});
    connect(comp_type_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &AlphaArenaScreen::on_competition_type_changed);
    type_col->addWidget(type_lbl);
    type_col->addWidget(comp_type_);
    r1->addLayout(type_col, 1);

    bl->addLayout(r1);

    // Row 2: Symbol + Mode + Capital + Interval
    auto* r2 = new QHBoxLayout;
    r2->setSpacing(8);

    auto* sym_col = new QVBoxLayout;
    auto* sym_lbl = new QLabel("SYMBOL");
    sym_lbl->setObjectName("aaLabel");
    comp_symbol_ = new QComboBox;
    comp_symbol_->addItems(CRYPTO_SYMBOLS);
    sym_col->addWidget(sym_lbl);
    sym_col->addWidget(comp_symbol_);
    r2->addLayout(sym_col, 1);

    auto* mode_col = new QVBoxLayout;
    auto* mode_lbl = new QLabel("MODE");
    mode_lbl->setObjectName("aaLabel");
    comp_mode_ = new QComboBox;
    comp_mode_->addItems(COMP_MODES);
    mode_col->addWidget(mode_lbl);
    mode_col->addWidget(comp_mode_);
    r2->addLayout(mode_col, 1);

    auto* cap_col = new QVBoxLayout;
    auto* cap_lbl = new QLabel("INITIAL CAPITAL ($)");
    cap_lbl->setObjectName("aaLabel");
    comp_capital_ = new QDoubleSpinBox;
    comp_capital_->setRange(100, 10000000);
    comp_capital_->setValue(10000);
    comp_capital_->setDecimals(0);
    comp_capital_->setPrefix("$");
    comp_capital_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    cap_col->addWidget(cap_lbl);
    cap_col->addWidget(comp_capital_);
    r2->addLayout(cap_col, 1);

    auto* int_col = new QVBoxLayout;
    auto* int_lbl = new QLabel("INTERVAL (sec)");
    int_lbl->setObjectName("aaLabel");
    comp_interval_ = new QSpinBox;
    comp_interval_->setRange(10, 600);
    comp_interval_->setValue(150);
    comp_interval_->setSuffix("s");
    comp_interval_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    int_col->addWidget(int_lbl);
    int_col->addWidget(comp_interval_);
    r2->addLayout(int_col, 1);

    bl->addLayout(r2);

    // AI Models
    auto* models_lbl = new QLabel("AI MODELS (select 2+)");
    models_lbl->setObjectName("aaLabel");
    bl->addWidget(models_lbl);

    model_list_ = new QListWidget;
    model_list_->setObjectName("aaModelList");
    model_list_->setSelectionMode(QAbstractItemView::MultiSelection);
    model_list_->setFixedHeight(80);
    bl->addWidget(model_list_);

    // Create button
    create_btn_ = new QPushButton("CREATE COMPETITION");
    create_btn_->setObjectName("aaCreateBtn");
    create_btn_->setCursor(Qt::PointingHandCursor);
    create_btn_->setFixedHeight(34);
    connect(create_btn_, &QPushButton::clicked, this, &AlphaArenaScreen::on_create_competition);
    bl->addWidget(create_btn_);

    vl->addWidget(body);
    return panel;
}

QWidget* AlphaArenaScreen::create_controls_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("aaControlsBar");
    bar->setFixedHeight(36);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);

    run_btn_ = new QPushButton("RUN CYCLE");
    run_btn_->setObjectName("aaRunBtn");
    run_btn_->setCursor(Qt::PointingHandCursor);
    run_btn_->setEnabled(false);
    connect(run_btn_, &QPushButton::clicked, this, &AlphaArenaScreen::on_run_cycle);
    hl->addWidget(run_btn_);

    auto_btn_ = new QPushButton("AUTO RUN");
    auto_btn_->setObjectName("aaAutoBtn");
    auto_btn_->setCursor(Qt::PointingHandCursor);
    auto_btn_->setEnabled(false);
    auto_btn_->setProperty("running", false);
    connect(auto_btn_, &QPushButton::clicked, this, &AlphaArenaScreen::on_toggle_auto_run);
    hl->addWidget(auto_btn_);

    reset_btn_ = new QPushButton("RESET");
    reset_btn_->setObjectName("aaResetBtn");
    reset_btn_->setCursor(Qt::PointingHandCursor);
    connect(reset_btn_, &QPushButton::clicked, this, &AlphaArenaScreen::on_reset);
    hl->addWidget(reset_btn_);

    hl->addStretch(1);

    interval_label_ = new QLabel("INTERVAL: 150s");
    interval_label_->setObjectName("aaIntervalLabel");
    hl->addWidget(interval_label_);

    return bar;
}

QWidget* AlphaArenaScreen::create_main_content() {
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);

    // Left: leaderboard
    splitter->addWidget(create_leaderboard_panel());

    // Right: 7-tab panel
    splitter->addWidget(create_right_panel());

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setSizes({700, 400});

    return splitter;
}

QWidget* AlphaArenaScreen::create_leaderboard_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("aaLeaderboard");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* hdr = new QWidget(this);
    hdr->setObjectName("aaLeaderboardHeader");
    hdr->setFixedHeight(34);
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(12, 0, 12, 0);
    auto* ht = new QLabel("LEADERBOARD");
    ht->setObjectName("aaLeaderboardTitle");
    hhl->addWidget(ht);
    hhl->addStretch(1);
    leaderboard_cycle_ = new QLabel("Cycle 0");
    leaderboard_cycle_->setObjectName("aaStatusText");
    hhl->addWidget(leaderboard_cycle_);
    vl->addWidget(hdr);

    leaderboard_table_ = new QTableWidget;
    leaderboard_table_->setColumnCount(7);
    leaderboard_table_->setHorizontalHeaderLabels(
        {"RANK", "MODEL", "PORTFOLIO", "PnL", "RETURN %", "TRADES", "WIN RATE"});
    leaderboard_table_->horizontalHeader()->setStretchLastSection(true);
    leaderboard_table_->verticalHeader()->setVisible(false);
    leaderboard_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    leaderboard_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    vl->addWidget(leaderboard_table_, 1);

    return panel;
}

QWidget* AlphaArenaScreen::create_right_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("aaRightPanel");
    panel->setMinimumWidth(280);
    panel->setMaximumWidth(400);

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Tab bar
    auto* tab_bar = new QWidget(this);
    tab_bar->setFixedHeight(32);
    auto* tbl = new QHBoxLayout(tab_bar);
    tbl->setContentsMargins(4, 0, 4, 0);
    tbl->setSpacing(0);

    const QStringList tabs = {"DECISIONS", "HITL", "SENTIMENT", "METRICS", "GRID", "RESEARCH", "BROKER"};
    for (int i = 0; i < tabs.size(); ++i) {
        auto* btn = new QPushButton(tabs[i]);
        btn->setObjectName("aaRightTabBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", i == 0);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_right_tab_changed(i); });
        tbl->addWidget(btn);
        right_tab_btns_.append(btn);
    }
    vl->addWidget(tab_bar);

    // Stack
    right_stack_ = new QStackedWidget;

    // 0: Decisions
    decisions_list_ = new QListWidget;
    decisions_list_->setObjectName("aaDecisionList");
    right_stack_->addWidget(decisions_list_);

    // 1: HITL
    hitl_content_ = new QTextEdit;
    hitl_content_->setReadOnly(true);
    hitl_content_->setPlaceholderText("Human-in-the-loop approvals will appear here.\n"
                                      "High-risk trades require manual approval before execution.");
    right_stack_->addWidget(hitl_content_);

    // 2: Sentiment
    sentiment_content_ = new QTextEdit;
    sentiment_content_->setReadOnly(true);
    sentiment_content_->setPlaceholderText("Market sentiment analysis will appear here.\n"
                                           "Mood: RISK_ON / RISK_OFF / MIXED");
    right_stack_->addWidget(sentiment_content_);

    // 3: Metrics
    metrics_table_ = new QTableWidget;
    metrics_table_->setColumnCount(2);
    metrics_table_->setHorizontalHeaderLabels({"METRIC", "VALUE"});
    metrics_table_->horizontalHeader()->setStretchLastSection(true);
    metrics_table_->verticalHeader()->setVisible(false);
    metrics_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    right_stack_->addWidget(metrics_table_);

    // 4: Grid
    grid_content_ = new QTextEdit;
    grid_content_->setReadOnly(true);
    grid_content_->setPlaceholderText("Grid trading strategy configuration.\n"
                                      "Place buy/sell orders at regular price intervals.");
    right_stack_->addWidget(grid_content_);

    // 5: Research
    research_content_ = new QTextEdit;
    research_content_->setReadOnly(true);
    research_content_->setPlaceholderText("SEC filings and company research.\n"
                                          "Search by ticker to load 10-K, 10-Q, 8-K filings.");
    right_stack_->addWidget(research_content_);

    // 6: Broker
    broker_content_ = new QTextEdit;
    broker_content_->setReadOnly(true);
    broker_content_->setPlaceholderText("Broker selection and configuration.\n"
                                        "Supported: Kraken, Binance, Coinbase, and more.");
    right_stack_->addWidget(broker_content_);

    vl->addWidget(right_stack_, 1);
    return panel;
}

QWidget* AlphaArenaScreen::create_past_competitions_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("aaPastPanel");
    panel->setFixedHeight(200);

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* hdr = new QWidget(this);
    hdr->setFixedHeight(30);
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(12, 0, 12, 0);
    auto* ht = new QLabel("PAST COMPETITIONS");
    ht->setObjectName("aaLeaderboardTitle");
    hhl->addWidget(ht);
    hhl->addStretch(1);
    vl->addWidget(hdr);

    past_list_ = new QListWidget;
    past_list_->setObjectName("aaPastList");
    connect(past_list_, &QListWidget::itemClicked, this, &AlphaArenaScreen::on_past_competition_clicked);
    vl->addWidget(past_list_, 1);

    return panel;
}

QWidget* AlphaArenaScreen::create_status_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("aaStatusBar");
    bar->setFixedHeight(26);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* left = new QLabel("ALPHA ARENA");
    left->setObjectName("aaStatusText");
    hl->addWidget(left);
    hl->addStretch(1);

    status_comp_ = new QLabel("NO COMPETITION");
    status_comp_->setObjectName("aaStatusText");
    hl->addWidget(status_comp_);

    hl->addSpacing(12);
    status_models_ = new QLabel;
    status_models_->setObjectName("aaStatusText");
    hl->addWidget(status_models_);

    hl->addSpacing(12);
    status_info_ = new QLabel;
    status_info_->setObjectName("aaStatusHighlight");
    hl->addWidget(status_info_);

    return bar;
}

// ── Slots ───────────────────────────────────────────────────────────────────

void AlphaArenaScreen::on_create_competition() {
    auto selected = model_list_->selectedItems();
    if (selected.size() < 2) {
        status_info_->setText("Select at least 2 models");
        return;
    }

    set_loading(true);

    QJsonArray models;
    QJsonObject api_keys; // collected per-provider for top-level payload field
    for (auto* item : selected) {
        int row = model_list_->row(item);
        if (row >= 0 && row < model_entries_.size()) {
            const auto& entry = model_entries_[row];
            QJsonObject m;
            m["name"] = entry.display_name; // Python reads "name"
            m["provider"] = entry.provider;
            m["model_id"] = entry.model_id;
            m["api_key"] = entry.api_key;
            m["base_url"] = entry.base_url;
            if (!entry.profile_id.isEmpty())
                m["profile_id"] = entry.profile_id;
            models.append(m);
            // Populate top-level api_keys map expected by Python
            if (!entry.provider.isEmpty() && !entry.api_key.isEmpty())
                api_keys[entry.provider] = entry.api_key;
        }
    }

    // Python reads params fields directly — inline config into params
    QJsonObject params;
    params["competition_name"] = comp_name_->text();
    params["symbols"] = QJsonArray{comp_symbol_->currentText()};
    params["mode"] = comp_mode_->currentText();
    params["initial_capital"] = comp_capital_->value();
    params["cycle_interval_seconds"] = comp_interval_->value();
    params["models"] = models;

    QJsonObject payload;
    payload["action"] = "create_competition";
    payload["params"] = params;
    payload["api_keys"] = api_keys;

    QPointer<AlphaArenaScreen> self = this;
    services::alpha_arena::AlphaArenaService::instance().run_action(
        payload, [self](const services::alpha_arena::ActionResult& r) {
        if (!self)
            return;
        self->set_loading(false);
        if (!r.success) {
            self->status_info_->setText("Error: " + r.error.left(60));
            return;
        }
        const QJsonObject obj = r.data;
        // Python returns competition_id at top level
        self->competition_id_ = obj["competition_id"].toString();
        self->competition_status_ = "created";
        self->create_panel_->hide();
        self->run_btn_->setEnabled(true);
        self->auto_btn_->setEnabled(true);
        self->status_badge_->setText("CREATED");
        self->status_badge_->setStyleSheet(
            QString("color: %1; background: rgba(22,163,74,0.15); font-size: 8px; font-weight: 700; padding: 2px 6px;")
                .arg(colors::POSITIVE()));
        self->status_comp_->setText("COMP: " + self->competition_id_.left(8) + "...");
        self->status_models_->setText(QString::number(self->model_list_->selectedItems().size()) + " models");
        self->interval_label_->setText("INTERVAL: " + QString::number(self->comp_interval_->value()) + "s");
        LOG_INFO("AlphaArena", "Competition created: " + self->competition_id_);
    });
}

void AlphaArenaScreen::on_run_cycle() {
    if (loading_ || competition_id_.isEmpty())
        return;

    set_loading(true);
    QJsonObject params;
    params["competition_id"] = competition_id_;
    run_python_action("run_cycle", params);
}

void AlphaArenaScreen::on_toggle_auto_run() {
    is_auto_running_ = !is_auto_running_;

    auto_btn_->setProperty("running", is_auto_running_);
    auto_btn_->style()->unpolish(auto_btn_);
    auto_btn_->style()->polish(auto_btn_);
    auto_btn_->setText(is_auto_running_ ? "STOP AUTO" : "AUTO RUN");

    if (is_auto_running_) {
        auto_timer_->setInterval(comp_interval_->value() * 1000);
        auto_timer_->start();
        on_run_cycle(); // run immediately
        LOG_INFO("AlphaArena", "Auto-run started");
    } else {
        auto_timer_->stop();
        LOG_INFO("AlphaArena", "Auto-run stopped");
    }
}

void AlphaArenaScreen::on_reset() {
    competition_id_.clear();
    competition_status_.clear();
    cycle_count_ = 0;
    is_auto_running_ = false;
    auto_timer_->stop();

    status_badge_->setText("READY");
    status_badge_->setStyleSheet(QString("color: %1; background: rgba(217,119,6,0.15); "
                                         "font-size: 8px; font-weight: 700; padding: 2px 6px;")
                                     .arg(colors::AMBER()));
    cycle_label_->setText("CYCLE 0");
    price_label_->hide();
    leaderboard_table_->setRowCount(0);
    leaderboard_cycle_->setText("Cycle 0");
    decisions_list_->clear();
    metrics_table_->setRowCount(0);

    run_btn_->setEnabled(false);
    auto_btn_->setEnabled(false);
    auto_btn_->setText("AUTO RUN");
    auto_btn_->setProperty("running", false);
    auto_btn_->style()->unpolish(auto_btn_);
    auto_btn_->style()->polish(auto_btn_);

    status_comp_->setText("NO COMPETITION");
    status_models_->clear();
    status_info_->clear();

    create_panel_->show();
    LOG_INFO("AlphaArena", "Reset");
}

void AlphaArenaScreen::on_right_tab_changed(int tab) {
    right_stack_->setCurrentIndex(tab);
    for (int i = 0; i < right_tab_btns_.size(); ++i) {
        right_tab_btns_[i]->setProperty("active", i == tab);
        right_tab_btns_[i]->style()->unpolish(right_tab_btns_[i]);
        right_tab_btns_[i]->style()->polish(right_tab_btns_[i]);
    }
    ScreenStateManager::instance().notify_changed(this);
}

void AlphaArenaScreen::on_competition_type_changed(int index) {
    // Show/hide crypto-specific fields for prediction markets
    comp_symbol_->setVisible(index == 0);
    comp_mode_->setVisible(index == 0);
}

void AlphaArenaScreen::on_refresh_leaderboard() {
    if (competition_id_.isEmpty())
        return;
    QJsonObject params;
    params["competition_id"] = competition_id_;
    run_python_action("get_leaderboard", params);
}

void AlphaArenaScreen::on_past_competitions_toggle() {
    past_visible_ = !past_visible_;
    past_panel_->setVisible(past_visible_);
    if (past_visible_)
        load_past_competitions();
}

void AlphaArenaScreen::on_past_competition_clicked(QListWidgetItem* item) {
    if (!item)
        return;
    QString id = item->data(Qt::UserRole).toString();
    if (id.isEmpty())
        return;

    competition_id_ = id;
    past_panel_->hide();
    past_visible_ = false;
    create_panel_->hide();

    run_btn_->setEnabled(true);
    auto_btn_->setEnabled(true);
    status_comp_->setText("COMP: " + id.left(8) + "...");

    on_refresh_leaderboard();
    LOG_INFO("AlphaArena", "Resumed competition: " + id);
}

// ── Python integration ──────────────────────────────────────────────────────

void AlphaArenaScreen::run_python_action(const QString& action, const QJsonObject& params) {
    QJsonObject payload;
    payload["action"] = action;
    payload["params"] = params;

    QPointer<AlphaArenaScreen> self = this;

    services::alpha_arena::AlphaArenaService::instance().run_action(
        payload, [self, action](const services::alpha_arena::ActionResult& r) {
            if (!self)
                return;
            self->set_loading(false);

            if (!r.success) {
                self->status_info_->setText("Error: " + r.error.left(60));
                return;
            }

            const QJsonObject obj = r.data;

            // Python returns fields at top level, not nested under "data"
            if (action == "run_cycle") {
                int cycle_number = obj["cycle_number"].toInt(self->cycle_count_ + 1);
                self->cycle_count_ = cycle_number;
                self->cycle_label_->setText("CYCLE " + QString::number(self->cycle_count_));
                self->leaderboard_cycle_->setText("Cycle " + QString::number(self->cycle_count_));

                self->status_badge_->setText("RUNNING");
                self->status_badge_->setStyleSheet(QString("color: %1; background: rgba(22,163,74,0.15); "
                                                           "font-size: 8px; font-weight: 700; padding: 2px 6px;")
                                                       .arg(colors::POSITIVE()));

                if (obj.contains("leaderboard"))
                    self->update_leaderboard(obj["leaderboard"].toArray());

                // Decisions extracted from events (DECISION_COMPLETED events)
                if (obj.contains("events")) {
                    QJsonArray decisions;
                    for (const auto& ev : obj["events"].toArray()) {
                        auto eo = ev.toObject();
                        if (eo["event"].toString().contains("decision")) {
                            auto meta = eo["metadata"].toObject();
                            if (!meta.isEmpty())
                                decisions.append(meta);
                        }
                    }
                    if (!decisions.isEmpty())
                        self->update_decisions(decisions);
                }

                self->status_info_->setText("Cycle " + QString::number(self->cycle_count_) + " complete");
                LOG_INFO("AlphaArena", "Cycle " + QString::number(self->cycle_count_) + " complete");

            } else if (action == "get_leaderboard") {
                if (obj.contains("leaderboard"))
                    self->update_leaderboard(obj["leaderboard"].toArray());

            } else if (action == "list_competitions") {
                auto comps = obj["competitions"].toArray();
                self->past_list_->clear();
                for (const auto& c : comps) {
                    auto co = c.toObject();
                    // config is a nested object containing competition_name
                    auto cfg = co["config"].toObject();
                    QString name = cfg["competition_name"].toString(co["competition_id"].toString().left(8));
                    QString label = name + " — " + co["status"].toString() + " (" +
                                    QString::number(co["cycle_count"].toInt()) + " cycles)";
                    auto* item = new QListWidgetItem(label);
                    item->setData(Qt::UserRole, co["competition_id"].toString());
                    self->past_list_->addItem(item);
                }
            }
        });
}

// ── Data display ────────────────────────────────────────────────────────────

void AlphaArenaScreen::update_leaderboard(const QJsonArray& entries) {
    leaderboard_table_->setSortingEnabled(false);
    leaderboard_table_->setRowCount(entries.size());

    for (int i = 0; i < entries.size(); ++i) {
        auto e = entries[i].toObject();
        int rank = e["rank"].toInt(i + 1);
        QString model = e["model_name"].toString();
        double portfolio = e["portfolio_value"].toDouble();
        double pnl = e["total_pnl"].toDouble();
        double ret = e["return_pct"].toDouble();
        int trades = e["trades_count"].toInt();
        double win_rate = e["win_rate"].toDouble(-1);

        // Rank with medal
        QString rank_str = (rank == 1) ? "1st" : (rank == 2) ? "2nd" : (rank == 3) ? "3rd" : QString::number(rank);
        auto* rank_item = new QTableWidgetItem(rank_str);
        rank_item->setTextAlignment(Qt::AlignCenter);
        if (rank <= 3) {
            QString medal_color = (rank == 1) ? "#FFD700" : (rank == 2) ? "#C0C0C0" : "#CD7F32";
            rank_item->setForeground(QColor(medal_color));
        }
        leaderboard_table_->setItem(i, 0, rank_item);

        // Model name (colored)
        auto* model_item = new QTableWidgetItem(model);
        QString color = (i < MODEL_COLORS.size()) ? MODEL_COLORS[i] : colors::TEXT_SECONDARY;
        model_item->setForeground(QColor(color));
        leaderboard_table_->setItem(i, 1, model_item);

        // Portfolio value
        auto* port_item = new QTableWidgetItem(QString("$%1").arg(portfolio, 0, 'f', 2));
        port_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        port_item->setForeground(QColor(colors::CYAN()));
        leaderboard_table_->setItem(i, 2, port_item);

        // PnL
        auto* pnl_item = new QTableWidgetItem(QString("%1$%2").arg(pnl >= 0 ? "+" : "").arg(pnl, 0, 'f', 2));
        pnl_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        pnl_item->setForeground(QColor(pnl >= 0 ? colors::POSITIVE() : colors::NEGATIVE()));
        leaderboard_table_->setItem(i, 3, pnl_item);

        // Return %
        auto* ret_item = new QTableWidgetItem(QString("%1%2%").arg(ret >= 0 ? "+" : "").arg(ret, 0, 'f', 2));
        ret_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        ret_item->setForeground(QColor(ret >= 0 ? colors::POSITIVE() : colors::NEGATIVE()));
        leaderboard_table_->setItem(i, 4, ret_item);

        // Trades
        auto* trades_item = new QTableWidgetItem(QString::number(trades));
        trades_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        leaderboard_table_->setItem(i, 5, trades_item);

        // Win rate
        QString wr_str = win_rate >= 0 ? QString("%1%").arg(win_rate * 100, 0, 'f', 1) : "--";
        auto* wr_item = new QTableWidgetItem(wr_str);
        wr_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        leaderboard_table_->setItem(i, 6, wr_item);
    }

    leaderboard_table_->resizeColumnsToContents();
    leaderboard_table_->setSortingEnabled(true);
}

void AlphaArenaScreen::update_decisions(const QJsonArray& decisions) {
    decisions_list_->clear();

    int limit = qMin(decisions.size(), 50);
    for (int i = 0; i < limit; ++i) {
        auto d = decisions[i].toObject();
        QString model = d["model_name"].toString();
        QString action = d["action"].toString().toUpper();
        QString symbol = d["symbol"].toString();
        double confidence = d["confidence"].toDouble();
        QString reasoning = d["reasoning"].toString();

        QString action_color = (action == "BUY")     ? colors::POSITIVE
                               : (action == "SELL")  ? colors::NEGATIVE
                               : (action == "SHORT") ? "#9D4EDD"
                                                     : colors::TEXT_SECONDARY;

        QString text = QString("%1 | %2 %3 | Conf: %4%\n%5")
                           .arg(model, action, symbol)
                           .arg(qRound(confidence * 100))
                           .arg(reasoning.left(80));

        auto* item = new QListWidgetItem(text);
        item->setForeground(QColor(action_color));
        decisions_list_->addItem(item);
    }
}

void AlphaArenaScreen::load_past_competitions() {
    QJsonObject params;
    params["limit"] = 20;
    run_python_action("list_competitions", params);
}

void AlphaArenaScreen::set_loading(bool loading) {
    loading_ = loading;
    create_btn_->setEnabled(!loading);
    run_btn_->setEnabled(!loading && !competition_id_.isEmpty());
    run_btn_->setText(loading ? "RUNNING..." : "RUN CYCLE");
}

// ── Model list population ────────────────────────────────────────────────────

void AlphaArenaScreen::populate_model_list() {
    // Preserve previously selected display names so we can restore selection
    QStringList previously_selected;
    for (auto* item : model_list_->selectedItems())
        previously_selected << item->text();

    model_list_->clear();
    model_entries_.clear();

    // 1. Named profiles (highest fidelity — user configured them explicitly)
    auto profiles_res = LlmProfileRepository::instance().list_profiles();
    if (profiles_res.is_ok()) {
        for (const auto& p : profiles_res.value()) {
            if (p.provider.isEmpty() || p.model_id.isEmpty())
                continue;
            ArenaModelEntry entry;
            entry.display_name = p.name + " (" + p.provider + ")";
            entry.provider = p.provider;
            entry.model_id = p.model_id;
            entry.api_key = p.api_key;
            entry.base_url = p.base_url;
            entry.profile_id = p.id;
            model_entries_.append(entry);
            model_list_->addItem(entry.display_name);
        }
    }

    // 2. Configured providers (one entry per provider using its active model)
    auto providers_res = LlmConfigRepository::instance().list_providers();
    if (providers_res.is_ok()) {
        for (const auto& c : providers_res.value()) {
            if (c.provider.isEmpty() || c.model.isEmpty())
                continue;
            // Avoid duplicating a provider already covered by a profile
            bool already_added = false;
            for (const auto& e : std::as_const(model_entries_)) {
                if (e.provider == c.provider && e.model_id == c.model) {
                    already_added = true;
                    break;
                }
            }
            if (already_added)
                continue;

            ArenaModelEntry entry;
            entry.display_name = c.model + " (" + c.provider + ")";
            entry.provider = c.provider;
            entry.model_id = c.model;
            entry.api_key = c.api_key;
            entry.base_url = c.base_url;
            model_entries_.append(entry);
            model_list_->addItem(entry.display_name);
        }
    }

    // 3. Fallback: show a hint if nothing is configured
    if (model_entries_.isEmpty()) {
        auto* placeholder = new QListWidgetItem("No LLM providers configured — go to Settings → LLM Config");
        placeholder->setFlags(Qt::NoItemFlags); // not selectable
        placeholder->setForeground(QColor(colors::TEXT_DIM()));
        model_list_->addItem(placeholder);
        LOG_WARN("AlphaArena", "No LLM providers or profiles configured");
        return;
    }

    // Restore previous selections by display name
    for (int i = 0; i < model_list_->count(); ++i) {
        auto* item = model_list_->item(i);
        if (previously_selected.contains(item->text()))
            item->setSelected(true);
    }

    LOG_DEBUG("AlphaArena", QString("Model list populated: %1 entries").arg(model_entries_.size()));
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap AlphaArenaScreen::save_state() const {
    return {
        {"right_tab", right_stack_ ? right_stack_->currentIndex() : 0},
    };
}

void AlphaArenaScreen::restore_state(const QVariantMap& state) {
    const int tab = state.value("right_tab", 0).toInt();
    if (tab > 0)
        on_right_tab_changed(tab);
}

} // namespace fincept::screens
