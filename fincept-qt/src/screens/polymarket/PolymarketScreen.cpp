#include "screens/polymarket/PolymarketScreen.h"

#include "core/logging/Logger.h"
#include "services/polymarket/PolymarketService.h"
#include "ui/charts/ChartFactory.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

// ── Constants ───────────────────────────────────────────────────────────────

namespace {
using namespace fincept::ui;
using namespace fincept::services::polymarket;

static const QStringList OUTCOME_COLORS = {"#00D66F", "#FF3B3B", "#FF8800", "#4F8EF7", "#A855F7"};
static const QStringList VIEW_NAMES = {"MARKETS", "EVENTS", "RESOLVED"};
static const QStringList DETAIL_TABS = {"OVERVIEW", "ORDER BOOK", "PRICE CHART", "TRADES"};
static const QStringList INTERVAL_LABELS = {"1H", "6H", "1D", "1W", "1M", "ALL"};
static const QStringList INTERVAL_VALUES = {"1h", "6h", "1d", "1w", "1m", "max"};

static QString format_volume(double vol) {
    if (vol >= 1e9) return QString("$%1B").arg(vol / 1e9, 0, 'f', 1);
    if (vol >= 1e6) return QString("$%1M").arg(vol / 1e6, 0, 'f', 1);
    if (vol >= 1e3) return QString("$%1K").arg(vol / 1e3, 0, 'f', 1);
    return QString("$%1").arg(vol, 0, 'f', 0);
}

static QString format_price(double p) {
    return QString("%1c").arg(qRound(p * 100));
}

static QString format_timestamp(int64_t ts) {
    return QDateTime::fromSecsSinceEpoch(ts, Qt::UTC).toString("yyyy-MM-dd HH:mm");
}

// ── Stylesheet ──────────────────────────────────────────────────────────────

static const QString kStyle = QStringLiteral(
    "#polyScreen { background: %1; }"

    "#polyHeader { background: %2; border-bottom: 2px solid %3; }"
    "#polyHeaderTitle { color: %4; font-size: 12px; font-weight: 700; background: transparent; }"
    "#polyHeaderSub { color: %5; font-size: 9px; letter-spacing: 0.5px; background: transparent; }"

    "#polyViewBtn { background: transparent; color: %5; border: 1px solid %8; "
    "  font-size: 9px; font-weight: 700; padding: 4px 12px; }"
    "#polyViewBtn:hover { color: %4; }"
    "#polyViewBtn[active=\"true\"] { background: %3; color: %1; border-color: %3; }"

    "#polySearchInput { background: %1; color: %4; border: 1px solid %8; "
    "  padding: 4px 8px; font-size: 11px; }"
    "#polySearchInput:focus { border-color: %9; }"

    "QComboBox { background: %1; color: %4; border: 1px solid %8; "
    "  padding: 3px 8px; font-size: 11px; }"
    "QComboBox::drop-down { border: none; width: 16px; }"
    "QComboBox QAbstractItemView { background: %2; color: %4; border: 1px solid %8; "
    "  selection-background-color: %3; }"

    "#polyRefreshBtn { background: %7; color: %5; border: 1px solid %8; "
    "  padding: 4px 10px; font-size: 9px; font-weight: 700; }"
    "#polyRefreshBtn:hover { color: %4; }"

    "#polyListPanel { background: %7; border-right: 1px solid %8; }"
    "#polyListHeader { color: %5; font-size: 10px; font-weight: 700; "
    "  letter-spacing: 0.5px; background: transparent; padding: 6px 8px; "
    "  border-bottom: 1px solid %8; }"

    "#polyMarketList { background: %1; border: none; outline: none; font-size: 11px; }"
    "#polyMarketList::item { color: %5; padding: 6px 10px; border-bottom: 1px solid %8; }"
    "#polyMarketList::item:hover { color: %4; background: %12; }"
    "#polyMarketList::item:selected { color: %3; background: rgba(217,119,6,0.1); "
    "  border-left: 2px solid %3; }"

    "#polyPageBar { background: %7; border-top: 1px solid %8; }"
    "#polyPageBtn { background: %7; color: %5; border: 1px solid %8; "
    "  padding: 3px 10px; font-size: 9px; font-weight: 700; }"
    "#polyPageBtn:hover { color: %4; }"
    "#polyPageBtn:disabled { color: %11; }"
    "#polyPageLabel { color: %5; font-size: 9px; background: transparent; }"

    "#polyDetailPanel { background: %1; }"
    "#polyDetailTabBtn { background: transparent; color: %5; border: none; "
    "  font-size: 9px; font-weight: 700; padding: 6px 14px; "
    "  border-bottom: 2px solid transparent; }"
    "#polyDetailTabBtn:hover { color: %4; }"
    "#polyDetailTabBtn[active=\"true\"] { color: %4; border-bottom-color: %3; }"

    "#polyDetailQuestion { color: %4; font-size: 14px; font-weight: 700; "
    "  background: transparent; }"
    "#polyDetailLabel { color: %5; font-size: 9px; font-weight: 700; "
    "  letter-spacing: 0.5px; background: transparent; }"
    "#polyDetailValue { color: %13; font-size: 13px; font-weight: 700; "
    "  background: transparent; }"
    "#polyDetailStatus { font-size: 9px; font-weight: 700; padding: 2px 6px; }"

    "#polyOutcomeBar { padding: 4px 8px; margin: 2px 0; }"
    "#polyOutcomeName { color: %4; font-size: 11px; font-weight: 700; background: transparent; }"
    "#polyOutcomePrice { font-size: 13px; font-weight: 700; background: transparent; }"

    "QTableWidget { background: %1; color: %4; border: none; gridline-color: %8; "
    "  font-size: 11px; }"
    "QTableWidget::item { padding: 2px 6px; border-bottom: 1px solid %8; }"
    "QHeaderView::section { background: %2; color: %5; border: none; "
    "  border-bottom: 1px solid %8; border-right: 1px solid %8; "
    "  padding: 4px 6px; font-size: 10px; font-weight: 700; }"

    "#polyEmptyState { color: %11; font-size: 13px; background: transparent; }"
    "#polyChartContainer { background: %1; }"

    "#polyStatusBar { background: %2; border-top: 1px solid %8; }"
    "#polyStatusText { color: %5; font-size: 9px; background: transparent; }"
    "#polyStatusHighlight { color: %13; font-size: 9px; background: transparent; }"

    "QSplitter::handle { background: %8; }"
    "QScrollBar:vertical { background: %1; width: 6px; }"
    "QScrollBar::handle:vertical { background: %8; min-height: 20px; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
)
    .arg(colors::BG_BASE)         // %1
    .arg(colors::BG_RAISED)       // %2
    .arg(colors::AMBER)           // %3
    .arg(colors::TEXT_PRIMARY)    // %4
    .arg(colors::TEXT_SECONDARY)  // %5
    .arg(colors::POSITIVE)        // %6  (reserved)
    .arg(colors::BG_SURFACE)      // %7
    .arg(colors::BORDER_DIM)      // %8
    .arg(colors::BORDER_BRIGHT)   // %9
    .arg(colors::AMBER_DIM)       // %10 (reserved)
    .arg(colors::TEXT_DIM)         // %11
    .arg(colors::BG_HOVER)        // %12
    .arg(colors::CYAN)             // %13
    .arg(colors::NEGATIVE)        // %14 (reserved)
    ;
} // anonymous namespace

namespace fincept::screens {

using namespace fincept::ui;
using namespace fincept::services::polymarket;

// ── Constructor ─────────────────────────────────────────────────────────────

PolymarketScreen::PolymarketScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("polyScreen");
    setStyleSheet(kStyle);
    setup_ui();
    connect_service();

    // Auto-refresh timer — interval set, but NOT started (P3).
    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(60000); // 60s
    connect(refresh_timer_, &QTimer::timeout, this, &PolymarketScreen::on_refresh);

    LOG_INFO("Polymarket", "Screen constructed");
}

// ── Visibility (P3) ─────────────────────────────────────────────────────────

void PolymarketScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh_timer_->start();
    if (first_show_) {
        first_show_ = false;
        load_current_view();
    }
}

void PolymarketScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    refresh_timer_->stop();
}

// ── Service wiring ──────────────────────────────────────────────────────────

void PolymarketScreen::connect_service() {
    auto& svc = PolymarketService::instance();
    connect(&svc, &PolymarketService::markets_ready,
            this, &PolymarketScreen::on_markets_received);
    connect(&svc, &PolymarketService::events_ready,
            this, &PolymarketScreen::on_events_received);
    connect(&svc, &PolymarketService::order_book_ready,
            this, &PolymarketScreen::on_order_book_received);
    connect(&svc, &PolymarketService::price_history_ready,
            this, &PolymarketScreen::on_price_history_received);
    connect(&svc, &PolymarketService::trades_ready,
            this, &PolymarketScreen::on_trades_received);
    connect(&svc, &PolymarketService::price_summary_ready,
            this, &PolymarketScreen::on_price_summary_received);
    connect(&svc, &PolymarketService::request_error,
            this, &PolymarketScreen::on_service_error);
}

// ── UI Setup ────────────────────────────────────────────────────────────────

void PolymarketScreen::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(create_header());

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);

    auto* left = create_list_panel();
    left->setMinimumWidth(320);
    left->setMaximumWidth(420);

    auto* right = create_detail_panel();

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({360, 700});

    root->addWidget(splitter, 1);
    root->addWidget(create_status_bar());
}

QWidget* PolymarketScreen::create_header() {
    auto* bar = new QWidget;
    bar->setObjectName("polyHeader");
    bar->setFixedHeight(76);

    auto* vl = new QVBoxLayout(bar);
    vl->setContentsMargins(16, 6, 16, 6);
    vl->setSpacing(6);

    // Title row
    auto* title_row = new QHBoxLayout;
    title_row->setSpacing(8);
    auto* title = new QLabel("POLYMARKET PREDICTION MARKETS");
    title->setObjectName("polyHeaderTitle");
    title_row->addWidget(title);
    title_row->addStretch(1);

    refresh_btn_ = new QPushButton("REFRESH");
    refresh_btn_->setObjectName("polyRefreshBtn");
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn_, &QPushButton::clicked, this, &PolymarketScreen::on_refresh);
    title_row->addWidget(refresh_btn_);

    auto* live_badge = new QLabel("LIVE");
    live_badge->setStyleSheet(QString("color: %1; font-size: 8px; font-weight: 700; "
                                       "background: rgba(22,163,74,0.2); padding: 2px 6px;")
                                  .arg(colors::POSITIVE));
    title_row->addWidget(live_badge);

    vl->addLayout(title_row);

    // Navigation row: view tabs + search + sort
    auto* nav_row = new QHBoxLayout;
    nav_row->setSpacing(4);

    for (int i = 0; i < VIEW_NAMES.size(); ++i) {
        auto* btn = new QPushButton(VIEW_NAMES[i]);
        btn->setObjectName("polyViewBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", i == 0);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_view_changed(i); });
        nav_row->addWidget(btn);
        view_btns_.append(btn);
    }

    nav_row->addSpacing(12);

    search_input_ = new QLineEdit;
    search_input_->setObjectName("polySearchInput");
    search_input_->setPlaceholderText("SEARCH MARKETS...");
    search_input_->setFixedWidth(200);
    connect(search_input_, &QLineEdit::returnPressed, this, &PolymarketScreen::on_search);
    nav_row->addWidget(search_input_);

    sort_combo_ = new QComboBox;
    sort_combo_->addItems({"VOLUME", "LIQUIDITY", "RECENT"});
    sort_combo_->setFixedWidth(90);
    connect(sort_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PolymarketScreen::on_sort_changed);
    nav_row->addWidget(sort_combo_);

    nav_row->addStretch(1);
    vl->addLayout(nav_row);

    return bar;
}

QWidget* PolymarketScreen::create_list_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("polyListPanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    list_header_ = new QLabel("MARKETS");
    list_header_->setObjectName("polyListHeader");
    vl->addWidget(list_header_);

    market_list_ = new QListWidget;
    market_list_->setObjectName("polyMarketList");
    connect(market_list_, &QListWidget::itemClicked, this, &PolymarketScreen::on_market_clicked);
    vl->addWidget(market_list_, 1);

    // Pagination bar
    auto* page_bar = new QWidget;
    page_bar->setObjectName("polyPageBar");
    page_bar->setFixedHeight(30);
    auto* phl = new QHBoxLayout(page_bar);
    phl->setContentsMargins(8, 0, 8, 0);
    phl->setSpacing(6);

    prev_btn_ = new QPushButton("PREV");
    prev_btn_->setObjectName("polyPageBtn");
    prev_btn_->setCursor(Qt::PointingHandCursor);
    connect(prev_btn_, &QPushButton::clicked, this, &PolymarketScreen::on_prev_page);

    page_label_ = new QLabel("Page 1");
    page_label_->setObjectName("polyPageLabel");
    page_label_->setAlignment(Qt::AlignCenter);

    next_btn_ = new QPushButton("NEXT");
    next_btn_->setObjectName("polyPageBtn");
    next_btn_->setCursor(Qt::PointingHandCursor);
    connect(next_btn_, &QPushButton::clicked, this, &PolymarketScreen::on_next_page);

    phl->addWidget(prev_btn_);
    phl->addStretch(1);
    phl->addWidget(page_label_);
    phl->addStretch(1);
    phl->addWidget(next_btn_);
    vl->addWidget(page_bar);

    return panel;
}

QWidget* PolymarketScreen::create_detail_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("polyDetailPanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Tab bar
    auto* tab_bar = new QWidget;
    tab_bar->setFixedHeight(32);
    auto* tbl = new QHBoxLayout(tab_bar);
    tbl->setContentsMargins(12, 0, 12, 0);
    tbl->setSpacing(0);

    for (int i = 0; i < DETAIL_TABS.size(); ++i) {
        auto* btn = new QPushButton(DETAIL_TABS[i]);
        btn->setObjectName("polyDetailTabBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", i == 0);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_detail_tab_changed(i); });
        tbl->addWidget(btn);
        detail_tab_btns_.append(btn);
    }
    tbl->addStretch(1);
    vl->addWidget(tab_bar);

    // Stacked widget for detail pages
    detail_stack_ = new QStackedWidget;
    detail_stack_->addWidget(create_overview_page());   // 0
    detail_stack_->addWidget(create_orderbook_page());  // 1
    detail_stack_->addWidget(create_chart_page());      // 2
    detail_stack_->addWidget(create_trades_page());     // 3

    vl->addWidget(detail_stack_, 1);
    return panel;
}

QWidget* PolymarketScreen::create_overview_page() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* page = new QWidget;
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    detail_question_ = new QLabel("Select a market to view details");
    detail_question_->setObjectName("polyDetailQuestion");
    detail_question_->setWordWrap(true);
    vl->addWidget(detail_question_);

    // Stats grid
    auto* stats = new QWidget;
    auto* sgl = new QHBoxLayout(stats);
    sgl->setContentsMargins(0, 0, 0, 0);
    sgl->setSpacing(16);

    auto make_stat = [&](const QString& label, QLabel*& val_lbl) {
        auto* box = new QWidget;
        auto* bvl = new QVBoxLayout(box);
        bvl->setContentsMargins(0, 0, 0, 0);
        bvl->setSpacing(1);
        auto* lbl = new QLabel(label);
        lbl->setObjectName("polyDetailLabel");
        val_lbl = new QLabel("--");
        val_lbl->setObjectName("polyDetailValue");
        bvl->addWidget(lbl);
        bvl->addWidget(val_lbl);
        sgl->addWidget(box);
    };

    make_stat("VOLUME", detail_volume_);
    make_stat("LIQUIDITY", detail_liquidity_);
    make_stat("END DATE", detail_end_date_);
    make_stat("MIDPOINT", detail_midpoint_);
    make_stat("SPREAD", detail_spread_);
    make_stat("LAST TRADE", detail_last_trade_);

    detail_status_ = new QLabel;
    detail_status_->setObjectName("polyDetailStatus");
    sgl->addWidget(detail_status_);
    sgl->addStretch(1);
    vl->addWidget(stats);

    // Outcomes container
    outcome_container_ = new QWidget;
    auto* ocl = new QVBoxLayout(outcome_container_);
    ocl->setContentsMargins(0, 0, 0, 0);
    ocl->setSpacing(2);
    vl->addWidget(outcome_container_);

    vl->addStretch(1);
    scroll->setWidget(page);
    return scroll;
}

QWidget* PolymarketScreen::create_orderbook_page() {
    orderbook_table_ = new QTableWidget;
    orderbook_table_->setColumnCount(4);
    orderbook_table_->setHorizontalHeaderLabels({"SIDE", "PRICE", "SIZE", "DEPTH"});
    orderbook_table_->horizontalHeader()->setStretchLastSection(true);
    orderbook_table_->verticalHeader()->setVisible(false);
    orderbook_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    orderbook_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    return orderbook_table_;
}

QWidget* PolymarketScreen::create_chart_page() {
    auto* container = new QWidget;
    container->setObjectName("polyChartContainer");
    auto* vl = new QVBoxLayout(container);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(4);

    // Interval selector
    auto* toolbar = new QHBoxLayout;
    auto* lbl = new QLabel("INTERVAL");
    lbl->setObjectName("polyDetailLabel");
    toolbar->addWidget(lbl);

    interval_combo_ = new QComboBox;
    interval_combo_->addItems(INTERVAL_LABELS);
    interval_combo_->setCurrentIndex(2); // default 1D
    interval_combo_->setFixedWidth(70);
    connect(interval_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PolymarketScreen::on_history_interval_changed);
    toolbar->addWidget(interval_combo_);
    toolbar->addStretch(1);
    vl->addLayout(toolbar);

    // Placeholder for chart (replaced dynamically)
    chart_widget_ = new QWidget;
    auto* placeholder_layout = new QVBoxLayout(chart_widget_);
    auto* empty = new QLabel("Select a market to view price chart");
    empty->setObjectName("polyEmptyState");
    empty->setAlignment(Qt::AlignCenter);
    placeholder_layout->addWidget(empty);
    vl->addWidget(chart_widget_, 1);

    return container;
}

QWidget* PolymarketScreen::create_trades_page() {
    trades_table_ = new QTableWidget;
    trades_table_->setColumnCount(4);
    trades_table_->setHorizontalHeaderLabels({"TIME", "SIDE", "PRICE", "SIZE"});
    trades_table_->horizontalHeader()->setStretchLastSection(true);
    trades_table_->verticalHeader()->setVisible(false);
    trades_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    trades_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    return trades_table_;
}

QWidget* PolymarketScreen::create_status_bar() {
    auto* bar = new QWidget;
    bar->setObjectName("polyStatusBar");
    bar->setFixedHeight(26);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* left = new QLabel("POLYMARKET");
    left->setObjectName("polyStatusText");
    hl->addWidget(left);
    hl->addStretch(1);

    status_view_ = new QLabel("MARKETS");
    status_view_->setObjectName("polyStatusText");
    hl->addWidget(status_view_);

    hl->addSpacing(12);
    status_count_ = new QLabel;
    status_count_->setObjectName("polyStatusText");
    hl->addWidget(status_count_);

    hl->addSpacing(12);
    status_market_ = new QLabel;
    status_market_->setObjectName("polyStatusHighlight");
    hl->addWidget(status_market_);

    return bar;
}

// ── View slots ──────────────────────────────────────────────────────────────

void PolymarketScreen::on_view_changed(int view) {
    active_view_ = static_cast<View>(view);
    current_page_ = 0;

    for (int i = 0; i < view_btns_.size(); ++i) {
        view_btns_[i]->setProperty("active", i == view);
        view_btns_[i]->style()->unpolish(view_btns_[i]);
        view_btns_[i]->style()->polish(view_btns_[i]);
    }

    status_view_->setText(VIEW_NAMES[view]);
    list_header_->setText(VIEW_NAMES[view]);
    load_current_view();

    LOG_INFO("Polymarket", "View: " + VIEW_NAMES[view]);
}

void PolymarketScreen::on_search() {
    QString query = search_input_->text().trimmed();
    if (query.isEmpty()) {
        load_current_view();
        return;
    }

    set_loading(true);
    PolymarketService::instance().search_markets(query, 50);
}

void PolymarketScreen::on_sort_changed(int /*index*/) {
    if (!first_show_) load_current_view();
}

void PolymarketScreen::on_refresh() {
    load_current_view();
}

void PolymarketScreen::on_prev_page() {
    if (current_page_ > 0) {
        --current_page_;
        if (active_view_ == EVENTS || active_view_ == RESOLVED)
            display_event_list();
        else
            display_market_list();
    }
}

void PolymarketScreen::on_next_page() {
    int total = (active_view_ == EVENTS || active_view_ == RESOLVED)
                    ? events_.size()
                    : markets_.size();
    int total_pages = qMax(1, (total + PAGE_SIZE - 1) / PAGE_SIZE);
    if (current_page_ + 1 < total_pages) {
        ++current_page_;
        if (active_view_ == EVENTS || active_view_ == RESOLVED)
            display_event_list();
        else
            display_market_list();
    }
}

void PolymarketScreen::on_market_clicked(QListWidgetItem* item) {
    if (!item) return;
    int idx = item->data(Qt::UserRole).toInt();

    if (active_view_ == EVENTS || active_view_ == RESOLVED) {
        // Events view: user clicks an event, select its first market
        if (idx >= 0 && idx < events_.size()) {
            const auto& evt = events_[idx];
            if (!evt.markets.isEmpty()) {
                select_market(evt.markets[0]);
            }
        }
    } else {
        if (idx >= 0 && idx < markets_.size()) {
            select_market(markets_[idx]);
        }
    }
}

void PolymarketScreen::on_detail_tab_changed(int tab) {
    detail_tab_ = tab;
    detail_stack_->setCurrentIndex(tab);

    for (int i = 0; i < detail_tab_btns_.size(); ++i) {
        detail_tab_btns_[i]->setProperty("active", i == tab);
        detail_tab_btns_[i]->style()->unpolish(detail_tab_btns_[i]);
        detail_tab_btns_[i]->style()->polish(detail_tab_btns_[i]);
    }
}

void PolymarketScreen::on_history_interval_changed(int index) {
    if (!has_selection_ || selected_market_.clob_token_ids.isEmpty()) return;
    int fidelity = 5;
    if (index <= 1) fidelity = 1;      // 1H, 6H → 1 min
    else if (index == 2) fidelity = 5;  // 1D → 5 min
    else if (index == 3) fidelity = 30; // 1W → 30 min
    else fidelity = 60;                  // 1M, ALL → 60 min

    PolymarketService::instance().fetch_price_history(
        selected_market_.clob_token_ids[0],
        INTERVAL_VALUES[index], fidelity);
}

// ── Data loading ────────────────────────────────────────────────────────────

void PolymarketScreen::load_current_view() {
    set_loading(true);

    QString sort_key = "volume";
    if (sort_combo_->currentIndex() == 1) sort_key = "liquidity";
    else if (sort_combo_->currentIndex() == 2) sort_key = "startDate";

    auto& svc = PolymarketService::instance();

    switch (active_view_) {
    case MARKETS:
        svc.fetch_markets(sort_key, 100, 0, false);
        break;
    case EVENTS:
        svc.fetch_events(sort_key, 100, 0, false);
        break;
    case RESOLVED:
        svc.fetch_events("endDate", 100, 0, true);
        break;
    }
}

void PolymarketScreen::select_market(const Market& market) {
    selected_market_ = market;
    has_selection_ = true;

    // Update overview
    detail_question_->setText(market.question);
    detail_volume_->setText(format_volume(market.volume));
    detail_liquidity_->setText(format_volume(market.liquidity));
    detail_end_date_->setText(market.end_date.left(10));

    // Status badge
    if (market.closed) {
        detail_status_->setText("RESOLVED");
        detail_status_->setStyleSheet(QString("color: %1; background: rgba(22,163,74,0.15); "
                                              "font-size: 9px; font-weight: 700; padding: 2px 6px;")
                                          .arg(colors::POSITIVE));
    } else if (market.active) {
        detail_status_->setText("ACTIVE");
        detail_status_->setStyleSheet(QString("color: %1; background: rgba(217,119,6,0.15); "
                                              "font-size: 9px; font-weight: 700; padding: 2px 6px;")
                                          .arg(colors::AMBER));
    } else {
        detail_status_->setText("INACTIVE");
        detail_status_->setStyleSheet(QString("color: %1; background: transparent; "
                                              "font-size: 9px; font-weight: 700; padding: 2px 6px;")
                                          .arg(colors::TEXT_DIM));
    }

    // Outcomes
    auto* layout = outcome_container_->layout();
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    for (int i = 0; i < market.outcomes.size(); ++i) {
        const auto& outcome = market.outcomes[i];
        auto* row = new QWidget;
        row->setObjectName("polyOutcomeBar");
        QString color = (i < OUTCOME_COLORS.size()) ? OUTCOME_COLORS[i] : colors::TEXT_SECONDARY;
        row->setStyleSheet(QString("background: %1; border-left: 3px solid %2;")
                               .arg(colors::BG_RAISED, color));

        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(8, 4, 8, 4);
        rl->setSpacing(8);

        auto* name = new QLabel(outcome.name);
        name->setObjectName("polyOutcomeName");

        auto* price = new QLabel(format_price(outcome.price));
        price->setObjectName("polyOutcomePrice");
        price->setStyleSheet(QString("color: %1;").arg(color));

        rl->addWidget(name);
        rl->addStretch(1);
        rl->addWidget(price);
        layout->addWidget(row);
    }

    // Reset price summary labels
    detail_midpoint_->setText("--");
    detail_spread_->setText("--");
    detail_last_trade_->setText("--");

    // Status bar
    QString q = market.question;
    status_market_->setText(q.left(50) + (q.size() > 50 ? "..." : ""));

    // Fetch detail data from service
    if (!market.clob_token_ids.isEmpty()) {
        auto& svc = PolymarketService::instance();
        QString token = market.clob_token_ids[0];

        svc.fetch_order_book(token);
        svc.fetch_price_summary(token);
        svc.fetch_price_history(token, INTERVAL_VALUES[interval_combo_->currentIndex()], 5);
    }
    if (!market.condition_id.isEmpty()) {
        PolymarketService::instance().fetch_trades(market.condition_id, 100);
    }

    on_detail_tab_changed(0); // reset to overview
}

// ── Service signal handlers ─────────────────────────────────────────────────

void PolymarketScreen::on_markets_received(const QVector<Market>& markets) {
    set_loading(false);
    markets_ = markets;
    current_page_ = 0;
    display_market_list();
    status_count_->setText(QString::number(markets.size()) + " markets");
}

void PolymarketScreen::on_events_received(const QVector<Event>& events) {
    set_loading(false);
    events_ = events;
    current_page_ = 0;
    display_event_list();
    status_count_->setText(QString::number(events.size()) + " events");
}

void PolymarketScreen::on_order_book_received(const OrderBook& book) {
    orderbook_table_->setSortingEnabled(false);
    orderbook_table_->setRowCount(0);

    int total = book.asks.size() + book.bids.size();
    orderbook_table_->setRowCount(total);

    int row = 0;
    double ask_depth = 0;

    // Asks (highest to lowest price)
    for (int i = book.asks.size() - 1; i >= 0; --i) {
        const auto& a = book.asks[i];
        ask_depth += a.size;

        auto* side_item = new QTableWidgetItem("ASK");
        side_item->setForeground(QColor(colors::NEGATIVE));
        orderbook_table_->setItem(row, 0, side_item);

        auto* price_item = new QTableWidgetItem(QString::number(a.price, 'f', 4));
        price_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        price_item->setForeground(QColor(colors::NEGATIVE));
        orderbook_table_->setItem(row, 1, price_item);

        auto* size_item = new QTableWidgetItem(QString::number(a.size, 'f', 2));
        size_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        orderbook_table_->setItem(row, 2, size_item);

        auto* depth_item = new QTableWidgetItem(QString::number(ask_depth, 'f', 2));
        depth_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        orderbook_table_->setItem(row, 3, depth_item);
        ++row;
    }

    double bid_depth = 0;

    // Bids (highest to lowest price)
    for (const auto& b : book.bids) {
        bid_depth += b.size;

        auto* side_item = new QTableWidgetItem("BID");
        side_item->setForeground(QColor(colors::POSITIVE));
        orderbook_table_->setItem(row, 0, side_item);

        auto* price_item = new QTableWidgetItem(QString::number(b.price, 'f', 4));
        price_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        price_item->setForeground(QColor(colors::POSITIVE));
        orderbook_table_->setItem(row, 1, price_item);

        auto* size_item = new QTableWidgetItem(QString::number(b.size, 'f', 2));
        size_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        orderbook_table_->setItem(row, 2, size_item);

        auto* depth_item = new QTableWidgetItem(QString::number(bid_depth, 'f', 2));
        depth_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        orderbook_table_->setItem(row, 3, depth_item);
        ++row;
    }

    orderbook_table_->resizeColumnsToContents();
    orderbook_table_->setSortingEnabled(true);
}

void PolymarketScreen::on_price_history_received(const PriceHistory& history) {
    // Build data points for ChartFactory
    QVector<ChartFactory::DataPoint> data;
    data.reserve(history.points.size());
    for (const auto& pt : history.points) {
        data.append({static_cast<double>(pt.timestamp), pt.price});
    }

    // Replace chart widget
    auto* chart_container = detail_stack_->widget(2);
    auto* vl = chart_container->layout();

    // Remove old chart (index 1 is the chart, index 0 is the toolbar)
    if (vl->count() > 1) {
        auto* old = vl->takeAt(1);
        if (old->widget()) old->widget()->deleteLater();
        delete old;
    }

    if (data.isEmpty()) {
        auto* empty = new QLabel("No price history available");
        empty->setObjectName("polyEmptyState");
        empty->setAlignment(Qt::AlignCenter);
        vl->addWidget(empty);
    } else {
        auto* chart_view = ChartFactory::line_chart("PRICE HISTORY", data, colors::AMBER);
        vl->addWidget(chart_view);
    }
}

void PolymarketScreen::on_trades_received(const QVector<Trade>& trades) {
    trades_table_->setSortingEnabled(false);
    trades_table_->setRowCount(0);

    int n = qMin(trades.size(), 200);
    trades_table_->setRowCount(n);

    for (int i = 0; i < n; ++i) {
        const auto& t = trades[i];
        QString time_str = format_timestamp(t.timestamp);

        trades_table_->setItem(i, 0, new QTableWidgetItem(time_str));

        auto* side_item = new QTableWidgetItem(t.side);
        side_item->setForeground(QColor(t.side == "BUY" ? colors::POSITIVE : colors::NEGATIVE));
        trades_table_->setItem(i, 1, side_item);

        auto* price_item = new QTableWidgetItem(QString::number(t.price, 'f', 4));
        price_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        price_item->setForeground(QColor(colors::CYAN));
        trades_table_->setItem(i, 2, price_item);

        auto* size_item = new QTableWidgetItem(QString::number(t.size, 'f', 2));
        size_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        trades_table_->setItem(i, 3, size_item);
    }

    trades_table_->resizeColumnsToContents();
    trades_table_->setSortingEnabled(true);
}

void PolymarketScreen::on_price_summary_received(const PriceSummary& summary) {
    detail_midpoint_->setText(format_price(summary.midpoint));
    detail_spread_->setText(format_price(summary.spread));
    detail_last_trade_->setText(format_price(summary.last_trade_price));
}

void PolymarketScreen::on_service_error(const QString& ctx, const QString& msg) {
    set_loading(false);
    list_header_->setText("ERROR");
    LOG_ERROR("Polymarket", ctx + ": " + msg);
}

// ── Display helpers ─────────────────────────────────────────────────────────

void PolymarketScreen::display_market_list() {
    market_list_->clear();

    int start = current_page_ * PAGE_SIZE;
    int end = qMin(start + PAGE_SIZE, markets_.size());

    for (int i = start; i < end; ++i) {
        const auto& m = markets_[i];
        QString price_str;
        if (!m.outcomes.isEmpty()) {
            price_str = QString(" | YES %1").arg(format_price(m.outcomes[0].price));
        }

        auto* item = new QListWidgetItem(
            m.question.left(60) + "\n" + format_volume(m.volume) + price_str);
        item->setData(Qt::UserRole, i);
        market_list_->addItem(item);
    }

    update_pagination();
    list_header_->setText(QString("MARKETS (%1)").arg(markets_.size()));
}

void PolymarketScreen::display_event_list() {
    market_list_->clear();

    int start = current_page_ * PAGE_SIZE;
    int end = qMin(start + PAGE_SIZE, events_.size());

    for (int i = start; i < end; ++i) {
        const auto& e = events_[i];
        QString sub = format_volume(e.volume);
        if (!e.markets.isEmpty()) {
            sub += QString(" | %1 markets").arg(e.markets.size());
        }

        auto* item = new QListWidgetItem(e.title.left(60) + "\n" + sub);
        item->setData(Qt::UserRole, i);
        market_list_->addItem(item);
    }

    update_pagination();
    QString label = (active_view_ == RESOLVED) ? "RESOLVED" : "EVENTS";
    list_header_->setText(QString("%1 (%2)").arg(label).arg(events_.size()));
}

void PolymarketScreen::update_pagination() {
    int total = (active_view_ == EVENTS || active_view_ == RESOLVED)
                    ? events_.size()
                    : markets_.size();
    int total_pages = qMax(1, (total + PAGE_SIZE - 1) / PAGE_SIZE);
    page_label_->setText(QString("Page %1 / %2").arg(current_page_ + 1).arg(total_pages));
    prev_btn_->setEnabled(current_page_ > 0);
    next_btn_->setEnabled(current_page_ + 1 < total_pages);
}

void PolymarketScreen::set_loading(bool loading) {
    loading_ = loading;
    refresh_btn_->setEnabled(!loading);
    refresh_btn_->setText(loading ? "LOADING..." : "REFRESH");
}

} // namespace fincept::screens
