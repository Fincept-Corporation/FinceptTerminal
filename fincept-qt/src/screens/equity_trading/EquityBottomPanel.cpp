// EquityBottomPanel.cpp — tabbed portfolio display
#include "screens/equity_trading/EquityBottomPanel.h"

#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"

#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace fincept::screens::equity {

namespace {

// Funds card slots (order must match setup_funds_tab + retranslateUi + set_funds_view).
enum FundCard {
    FC_Available = 0,
    FC_UsedMargin,
    FC_TotalEquity,
    FC_RealizedPnl,
    FC_UnrealizedPnl,
    FC_HoldingsValue,
    FC_OpeningBalance,
    FC_MarginUtil,
    FC_Collateral,
};

// Stats card slots (order must match setup_stats_tab + retranslateUi + set_stats_view).
enum StatCard {
    SC_NetPnl = 0,
    SC_TodayPnl,
    SC_ReturnPct,
    SC_WinRate,
    SC_ProfitFactor,
    SC_TotalTrades,
    SC_AvgWin,
    SC_AvgLoss,
    SC_Turnover,
    SC_LargestWin,
    SC_LargestLoss,
    SC_TotalFees,
};

// Money with the connected broker's currency symbol and locale-aware grouping
// (Indian lakh grouping for ₹). `signed_pfx` prepends an explicit +/- for P&L.
QString fmt_money(double v, const QString& sym, bool signed_pfx = false) {
    const QLocale loc = (sym == QString::fromUtf8("₹")) ? QLocale(QLocale::English, QLocale::India)
                                                        : QLocale(QLocale::English, QLocale::UnitedStates);
    const QString mag = loc.toString(qAbs(v), 'f', 2);
    const QString prefix = v < 0 ? QStringLiteral("-") : (signed_pfx ? QStringLiteral("+") : QString());
    return prefix + sym + mag;
}

QColor order_status_color(const QString& status) {
    const QString s = status.toLower();
    if (s == "filled" || s == "complete" || s == "completed")
        return {fincept::ui::colors::POSITIVE()};
    if (s == "rejected" || s == "cancelled" || s == "canceled")
        return {fincept::ui::colors::NEGATIVE()};
    return {fincept::ui::colors::AMBER()}; // pending / partial / open / accepted
}

} // namespace

EquityBottomPanel::EquityBottomPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("eqBottomPanel");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    tabs_ = new QTabWidget;
    tabs_->setObjectName("eqBottomTabs");
    tabs_->setDocumentMode(true);
    // Show full tab labels; scroll (don't truncate) when they overflow the width.
    tabs_->setElideMode(Qt::ElideNone);
    tabs_->setUsesScrollButtons(true);

    setup_positions_tab();
    setup_holdings_tab();
    setup_orders_tab();
    setup_funds_tab();
    setup_stats_tab();
    setup_time_sales_tab();
    setup_auctions_tab();
    setup_calendar_tab();

    // Collapse toggle pinned to the tab-bar corner. Collapsing leaves only the
    // tab-bar header and returns the vertical space to the chart above.
    collapse_btn_ = new QToolButton;
    collapse_btn_->setObjectName("eqBottomCollapseBtn");
    collapse_btn_->setCursor(Qt::PointingHandCursor);
    collapse_btn_->setAutoRaise(true);
    collapse_btn_->setText(QStringLiteral("▾")); // ▾ (click to collapse)
    collapse_btn_->setToolTip(tr("Collapse panel"));
    collapse_btn_->setStyleSheet(
        "QToolButton{background:transparent;border:none;color:#808080;font-size:12px;padding:0 10px;}"
        "QToolButton:hover{color:#e5e5e5;}");
    connect(collapse_btn_, &QToolButton::clicked, this, &EquityBottomPanel::toggle_collapsed);
    tabs_->setCornerWidget(collapse_btn_, Qt::TopRightCorner);

    // Clicking any tab while collapsed expands the sheet to that tab.
    connect(tabs_, &QTabWidget::tabBarClicked, this, [this](int) {
        if (collapsed_)
            toggle_collapsed();
    });

    // QTabWidget's page stack — hidden while collapsed so the panel shrinks to
    // the tab-bar height without fighting the pages' minimum heights.
    tab_content_ = tabs_->findChild<QStackedWidget*>();

    layout->addWidget(tabs_);
}

void EquityBottomPanel::toggle_collapsed() {
    collapsed_ = !collapsed_;
    apply_collapsed();
}

void EquityBottomPanel::apply_collapsed() {
    auto* splitter = qobject_cast<QSplitter*>(parentWidget());
    const int header_h = tabs_->tabBar()->sizeHint().height();
    if (collapsed_) {
        if (splitter)
            saved_split_sizes_ = splitter->sizes();
        if (tab_content_)
            tab_content_->hide();
        setMaximumHeight(header_h);
        collapse_btn_->setText(QStringLiteral("▴")); // ▴ (click to expand)
        collapse_btn_->setToolTip(tr("Expand panel"));
    } else {
        setMaximumHeight(QWIDGETSIZE_MAX);
        if (tab_content_)
            tab_content_->show();
        collapse_btn_->setText(QStringLiteral("▾")); // ▾
        collapse_btn_->setToolTip(tr("Collapse panel"));
        if (splitter && !saved_split_sizes_.isEmpty())
            splitter->setSizes(saved_split_sizes_);
    }
}

void EquityBottomPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EquityBottomPanel::retranslateUi() {
    // Tab labels
    if (tabs_) {
        if (positions_tab_idx_ >= 0)  tabs_->setTabText(positions_tab_idx_, tr("POSITIONS"));
        if (holdings_tab_idx_ >= 0)   tabs_->setTabText(holdings_tab_idx_, tr("HOLDINGS"));
        if (orders_tab_idx_ >= 0)     tabs_->setTabText(orders_tab_idx_, tr("ORDERS"));
        if (funds_tab_idx_ >= 0)      tabs_->setTabText(funds_tab_idx_, tr("FUNDS"));
        if (stats_tab_idx_ >= 0)      tabs_->setTabText(stats_tab_idx_, tr("STATS"));
        if (time_sales_tab_idx_ >= 0) tabs_->setTabText(time_sales_tab_idx_, tr("TIME & SALES"));
        if (auctions_tab_idx_ >= 0)   tabs_->setTabText(auctions_tab_idx_, tr("AUCTIONS"));
        if (calendar_tab_idx_ >= 0)   tabs_->setTabText(calendar_tab_idx_, tr("CALENDAR"));
    }

    // Table headers
    if (positions_table_)
        positions_table_->setHorizontalHeaderLabels({tr("Symbol"), tr("Product"), tr("Opened"), tr("Side"), tr("Qty"),
                                                     tr("Avg Price"), tr("LTP"), tr("P&L"), tr("P&L %"), tr("Action")});
    if (holdings_table_)
        holdings_table_->setHorizontalHeaderLabels(
            {tr("Symbol"), tr("Qty"), tr("Avg Price"), tr("LTP"), tr("Invested"), tr("Current"), tr("P&L"),
             tr("P&L %"), tr("Day P&L"), tr("Action")});
    if (orders_table_)
        orders_table_->setHorizontalHeaderLabels({tr("Order ID"), tr("Symbol"), tr("Product"), tr("Side"), tr("Type"),
                                                  tr("Qty"), tr("Price"), tr("Status"), tr("Time"), tr("Action")});
    if (auctions_table_)
        auctions_table_->setHorizontalHeaderLabels(
            {tr("Date"), tr("Type"), tr("Time"), tr("Price"), tr("Size"), tr("Exchange")});
    if (time_sales_table_)
        time_sales_table_->setHorizontalHeaderLabels(
            {tr("Time"), tr("Price"), tr("Size"), tr("Exchange"), tr("Conditions"), tr("Tape")});
    if (calendar_table_)
        calendar_table_->setHorizontalHeaderLabels(
            {tr("Date"), tr("Open (ET)"), tr("Close (ET)"), tr("Pre-Market"), tr("After-Hours")});

    // Action buttons
    if (close_all_btn_)      close_all_btn_->setText(tr("SQUARE OFF ALL"));
    if (cancel_all_btn_)     cancel_all_btn_->setText(tr("CANCEL ALL ORDERS"));
    if (holdings_import_btn_) holdings_import_btn_->setText(tr("IMPORT TO PORTFOLIO"));
    if (holdings_square_off_btn_) holdings_square_off_btn_->setText(tr("SQUARE OFF ALL"));

    // Holdings summary-strip captions
    if (holdings_count_caption_)    holdings_count_caption_->setText(tr("HOLDINGS"));
    if (holdings_invested_caption_) holdings_invested_caption_->setText(tr("INVESTED"));
    if (holdings_current_caption_)  holdings_current_caption_->setText(tr("CURRENT"));
    if (holdings_pnl_caption_)      holdings_pnl_caption_->setText(tr("TOTAL P&L"));
    if (holdings_day_pnl_caption_)  holdings_day_pnl_caption_->setText(tr("TODAY'S P&L"));
    if (holdings_pnl_pct_caption_)  holdings_pnl_pct_caption_->setText(tr("RETURN %"));

    // Funds card captions
    const QString fund_caps[kFundCards] = {tr("AVAILABLE BALANCE"), tr("USED MARGIN"),   tr("TOTAL EQUITY"),
                                           tr("REALIZED P&L"),      tr("UNREALIZED P&L"), tr("HOLDINGS VALUE"),
                                           tr("OPENING BALANCE"),   tr("MARGIN USED %"),  tr("COLLATERAL")};
    for (int i = 0; i < kFundCards; ++i)
        if (fund_card_cap_[i]) fund_card_cap_[i]->setText(fund_caps[i]);

    // Stats card captions
    const QString stat_caps[kStatCards] = {tr("NET P&L"),     tr("TODAY'S P&L"),   tr("RETURN %"),
                                           tr("WIN RATE"),    tr("PROFIT FACTOR"), tr("TOTAL TRADES"),
                                           tr("AVG WIN"),     tr("AVG LOSS"),      tr("TURNOVER"),
                                           tr("LARGEST WIN"), tr("LARGEST LOSS"),  tr("TOTAL CHARGES")};
    for (int i = 0; i < kStatCards; ++i)
        if (stat_card_cap_[i]) stat_card_cap_[i]->setText(stat_caps[i]);

    // The calendar clock banner ("● MARKET OPEN/CLOSED/--") is live data driven
    // by set_clock(); it re-renders in the new language on the next clock tick,
    // so it is intentionally not reapplied here.
}

QTableWidgetItem* EquityBottomPanel::ensure_item(QTableWidget* table, int row, int col) {
    auto* item = table->item(row, col);
    if (!item) {
        item = new QTableWidgetItem;
        item->setForeground(QColor(fincept::ui::colors::TEXT_PRIMARY()));
        table->setItem(row, col, item);
    }
    return item;
}

// ── Positions Tab ──────────────────────────────────────────────────────────

void EquityBottomPanel::setup_positions_tab() {
    auto* container = new QWidget;
    auto* vlay = new QVBoxLayout(container);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);

    // Action bar with Square Off All button
    auto* action_bar = new QWidget;
    action_bar->setObjectName("eqPositionsActionBar");
    action_bar->setStyleSheet(QString("#eqPositionsActionBar{background:%1;border-bottom:1px solid %2;}")
                                  .arg(fincept::ui::colors::PANEL(), fincept::ui::colors::BORDER()));
    auto* bar_layout = new QHBoxLayout(action_bar);
    bar_layout->setContentsMargins(12, 4, 12, 4);
    bar_layout->setSpacing(10);

    // Net total P&L across the open (intraday) positions.
    positions_total_pnl_label_ = new QLabel(tr("Total P&L  --"));
    positions_total_pnl_label_->setStyleSheet(
        QString("font-size:11px;font-weight:700;color:%1;").arg(fincept::ui::colors::TEXT_PRIMARY()));
    bar_layout->addWidget(positions_total_pnl_label_);

    // Small square-off button for a P&L group (winners / losers).
    auto make_group_btn = [](const QString& color, const QString& tip) {
        auto* b = new QPushButton(QString::fromUtf8("✕"));
        b->setCursor(Qt::PointingHandCursor);
        b->setEnabled(false);
        b->setFixedHeight(18);
        b->setToolTip(tip);
        b->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %1;"
                                 "padding:0 6px;font-size:10px;font-weight:700;border-radius:2px;}"
                                 "QPushButton:hover:enabled{background:%1;color:#000;}"
                                 "QPushButton:disabled{color:%2;border-color:%2;}")
                             .arg(color, fincept::ui::colors::TEXT_SECONDARY()));
        return b;
    };

    // Winners group: count + sum, with a square-off-profits button.
    positions_win_label_ = new QLabel(QString::fromUtf8("▲ 0"));
    positions_win_label_->setStyleSheet(
        QString("font-size:11px;font-weight:600;color:%1;").arg(fincept::ui::colors::POSITIVE()));
    bar_layout->addWidget(positions_win_label_);
    win_squareoff_btn_ = make_group_btn(fincept::ui::colors::POSITIVE(), tr("Square off all winning positions"));
    connect(win_squareoff_btn_, &QPushButton::clicked, this, [this]() {
        if (account_id_.isEmpty())
            return;
        auto answer = QMessageBox::warning(this, tr("Square Off Winning Positions"),
                                           tr("This will close ALL positions currently in profit.\n\nAre you sure?"),
                                           QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes)
            emit square_off_group_requested(account_id_, 1);
    });
    bar_layout->addWidget(win_squareoff_btn_);

    // Losers group: count + sum, with a cut-losses button.
    positions_loss_label_ = new QLabel(QString::fromUtf8("▼ 0"));
    positions_loss_label_->setStyleSheet(
        QString("font-size:11px;font-weight:600;color:%1;").arg(fincept::ui::colors::NEGATIVE()));
    bar_layout->addWidget(positions_loss_label_);
    loss_squareoff_btn_ = make_group_btn(fincept::ui::colors::NEGATIVE(), tr("Square off all losing positions"));
    connect(loss_squareoff_btn_, &QPushButton::clicked, this, [this]() {
        if (account_id_.isEmpty())
            return;
        auto answer = QMessageBox::warning(this, tr("Square Off Losing Positions"),
                                           tr("This will close ALL positions currently in loss.\n\nAre you sure?"),
                                           QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes)
            emit square_off_group_requested(account_id_, -1);
    });
    bar_layout->addWidget(loss_squareoff_btn_);

    bar_layout->addStretch(1);

    close_all_btn_ = new QPushButton(tr("SQUARE OFF ALL"));
    close_all_btn_->setCursor(Qt::PointingHandCursor);
    close_all_btn_->setStyleSheet(
        QString("QPushButton{background:rgba(220,38,38,0.12);color:%1;border:1px solid %2;"
                "padding:4px 14px;font-size:11px;font-weight:700;letter-spacing:0.5px;border-radius:2px;}"
                "QPushButton:hover{background:rgba(220,38,38,0.25);}")
            .arg(fincept::ui::colors::NEGATIVE(), fincept::ui::colors::NEGATIVE_DIM()));
    connect(close_all_btn_, &QPushButton::clicked, this, [this]() {
        if (account_id_.isEmpty())
            return;
        auto answer = QMessageBox::warning(this, tr("Square Off All Positions"),
                                           tr("This will close ALL open positions.\n\nAre you sure?"),
                                           QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
        emit close_all_positions_requested(account_id_);
    });
    bar_layout->addWidget(close_all_btn_);
    vlay->addWidget(action_bar);

    positions_table_ = new QTableWidget;
    positions_table_->setObjectName("eqTable");
    positions_table_->setColumnCount(10);
    positions_table_->setHorizontalHeaderLabels({tr("Symbol"), tr("Product"), tr("Opened"), tr("Side"), tr("Qty"),
                                                 tr("Avg Price"), tr("LTP"), tr("P&L"), tr("P&L %"), tr("Action")});
    positions_table_->verticalHeader()->setVisible(false);
    positions_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    positions_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    positions_table_->setShowGrid(false);
    positions_table_->horizontalHeader()->setStretchLastSection(true);
    positions_table_->verticalHeader()->setDefaultSectionSize(22);

    // Right-click a row → Buy (add) / Sell (reduce) for that symbol. Symbol +
    // product are read from the row items so it stays correct regardless of order.
    positions_table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(positions_table_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& p) {
        const int row = positions_table_->rowAt(p.y());
        if (row < 0)
            return;
        auto* sym_item = positions_table_->item(row, 0);
        if (!sym_item || sym_item->text().isEmpty())
            return;
        const QString symbol = sym_item->text();
        auto* prod_item = positions_table_->item(row, 1);
        const QString product = prod_item && !prod_item->text().isEmpty() ? prod_item->text() : QStringLiteral("MIS");
        QMenu menu(this);
        QAction* buy = menu.addAction(tr("Buy / Add  %1").arg(symbol));
        QAction* sell = menu.addAction(tr("Sell / Reduce  %1").arg(symbol));
        auto* qty_item = positions_table_->item(row, 4); // Qty column
        const double held = qty_item ? qAbs(qty_item->text().toDouble()) : 0.0;
        QAction* chosen = menu.exec(positions_table_->viewport()->mapToGlobal(p));
        if (chosen == buy)
            emit trade_symbol_requested(symbol, product, true, 0.0); // add → ticket defaults to 1
        else if (chosen == sell)
            emit trade_symbol_requested(symbol, product, false, held); // reduce → pre-fill held qty
    });

    // Left-click a position row → load that symbol's chart (parity with the
    // watchlist). Clicks on the Action cell-widget (col 9) go to its buttons, not
    // here, so the SELL button keeps working.
    connect(positions_table_, &QTableWidget::cellClicked, this, [this](int row, int col) {
        auto* sym_item = positions_table_->item(row, 0);
        LOG_INFO("posdbg", QString("positions cellClicked row=%1 col=%2 sym='%3'")
                               .arg(row).arg(col).arg(sym_item ? sym_item->text() : QStringLiteral("<null>")));
        if (sym_item && !sym_item->text().isEmpty())
            emit chart_symbol_requested(sym_item->text());
    });

    vlay->addWidget(positions_table_, 1);

    positions_tab_idx_ = tabs_->addTab(container, tr("POSITIONS"));
}

// ── Holdings Tab ───────────────────────────────────────────────────────────

void EquityBottomPanel::setup_holdings_tab() {
    auto* tab = new QWidget;
    auto* v = new QVBoxLayout(tab);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // Summary strip across the top of the Holdings tab.
    auto* strip = new QWidget;
    strip->setObjectName("eqHoldingsStrip");
    strip->setStyleSheet(QString("#eqHoldingsStrip{background:%1;border-bottom:1px solid %2;}")
                             .arg(fincept::ui::colors::PANEL(), fincept::ui::colors::BORDER()));
    auto* strip_layout = new QHBoxLayout(strip);
    strip_layout->setContentsMargins(12, 6, 12, 6);
    strip_layout->setSpacing(24);

    auto make_stat = [&](const QString& caption, QLabel*& caption_out) -> QLabel* {
        auto* cell = new QWidget;
        auto* cv = new QVBoxLayout(cell);
        cv->setContentsMargins(0, 0, 0, 0);
        cv->setSpacing(1);
        auto* cap = new QLabel(caption);
        cap->setStyleSheet(QString("color:%1;font-size:10px;letter-spacing:0.5px;")
                               .arg(fincept::ui::colors::TEXT_SECONDARY()));
        caption_out = cap;
        auto* val = new QLabel("--");
        val->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;")
                               .arg(fincept::ui::colors::TEXT_PRIMARY()));
        cv->addWidget(cap);
        cv->addWidget(val);
        strip_layout->addWidget(cell);
        return val;
    };
    holdings_count_label_     = make_stat(tr("HOLDINGS"),  holdings_count_caption_);
    holdings_invested_label_  = make_stat(tr("INVESTED"),  holdings_invested_caption_);
    holdings_current_label_   = make_stat(tr("CURRENT"),   holdings_current_caption_);
    holdings_pnl_label_       = make_stat(tr("TOTAL P&L"), holdings_pnl_caption_);
    holdings_day_pnl_label_   = make_stat(tr("TODAY'S P&L"), holdings_day_pnl_caption_);
    holdings_pnl_pct_label_   = make_stat(tr("RETURN %"),  holdings_pnl_pct_caption_);
    strip_layout->addStretch(1);

    holdings_import_btn_ = new QPushButton(tr("IMPORT TO PORTFOLIO"));
    holdings_import_btn_->setCursor(Qt::PointingHandCursor);
    holdings_import_btn_->setEnabled(false);
    holdings_import_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:4px 12px;"
                "font-size:11px;font-weight:700;letter-spacing:0.5px;}"
                "QPushButton:hover{background:%4;color:#000;}"
                "QPushButton:disabled{color:%5;border-color:%3;}")
            .arg(fincept::ui::colors::PANEL(), fincept::ui::colors::ORANGE(),
                 fincept::ui::colors::BORDER(), fincept::ui::colors::ORANGE(),
                 fincept::ui::colors::TEXT_SECONDARY()));
    connect(holdings_import_btn_, &QPushButton::clicked, this, [this]() {
        if (!last_holdings_.isEmpty())
            emit import_holdings_requested(last_holdings_);
    });
    strip_layout->addWidget(holdings_import_btn_);

    holdings_replicate_btn_ = new QPushButton(tr("REPLICATE → PAPER"));
    holdings_replicate_btn_->setCursor(Qt::PointingHandCursor);
    holdings_replicate_btn_->setStyleSheet(holdings_import_btn_->styleSheet());
    connect(holdings_replicate_btn_, &QPushButton::clicked, this,
            [this]() { emit replicate_portfolio_requested(); });
    strip_layout->addWidget(holdings_replicate_btn_);

    // SQUARE OFF ALL — sells every holding (delivery/CNC) at market. Mirrors the
    // Positions tab button, but acts ONLY on holdings; positions are untouched.
    holdings_square_off_btn_ = new QPushButton(tr("SQUARE OFF ALL"));
    holdings_square_off_btn_->setCursor(Qt::PointingHandCursor);
    holdings_square_off_btn_->setEnabled(false);
    holdings_square_off_btn_->setStyleSheet(
        QString("QPushButton{background:rgba(220,38,38,0.12);color:%1;border:1px solid %2;"
                "padding:4px 14px;font-size:11px;font-weight:700;letter-spacing:0.5px;border-radius:2px;}"
                "QPushButton:hover{background:rgba(220,38,38,0.25);}"
                "QPushButton:disabled{color:%3;border-color:%2;}")
            .arg(fincept::ui::colors::NEGATIVE(), fincept::ui::colors::NEGATIVE_DIM(),
                 fincept::ui::colors::TEXT_SECONDARY()));
    connect(holdings_square_off_btn_, &QPushButton::clicked, this, [this]() {
        LOG_INFO("sqoff", QString("[panel] SQUARE OFF ALL clicked: account_id='%1' holdings=%2")
                              .arg(account_id_)
                              .arg(last_holdings_.size()));
        if (account_id_.isEmpty() || last_holdings_.isEmpty())
            return;
        auto answer = QMessageBox::warning(
            this, tr("Square Off All Holdings"),
            tr("This will place MARKET SELL orders to exit ALL %1 holding(s).\n\n"
               "Positions are NOT affected. Are you sure?")
                .arg(last_holdings_.size()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
        emit square_off_all_holdings_requested(last_holdings_);
    });
    strip_layout->addWidget(holdings_square_off_btn_);

    v->addWidget(strip);

    holdings_table_ = new QTableWidget;
    holdings_table_->setObjectName("eqTable");
    holdings_table_->setColumnCount(10);
    holdings_table_->setHorizontalHeaderLabels({tr("Symbol"), tr("Qty"), tr("Avg Price"), tr("LTP"),
                                                tr("Invested"), tr("Current"), tr("P&L"), tr("P&L %"),
                                                tr("Day P&L"), tr("Action")});
    holdings_table_->verticalHeader()->setVisible(false);
    holdings_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    holdings_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    holdings_table_->setShowGrid(false);
    holdings_table_->horizontalHeader()->setStretchLastSection(true);
    holdings_table_->verticalHeader()->setDefaultSectionSize(22);
    holdings_table_->setAlternatingRowColors(true);
    holdings_table_->setSortingEnabled(true);
    // Smooth pixel-based scrolling (default is per-item, which feels janky).
    holdings_table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    holdings_table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    holdings_table_->verticalScrollBar()->setSingleStep(8);
    holdings_table_->horizontalScrollBar()->setSingleStep(16);

    // Right-click a holding → Buy (add) / Sell (reduce). Holdings are delivery, so
    // the resulting order uses the CNC product.
    holdings_table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(holdings_table_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& p) {
        const int row = holdings_table_->rowAt(p.y());
        if (row < 0)
            return;
        auto* sym_item = holdings_table_->item(row, 0);
        if (!sym_item || sym_item->text().isEmpty())
            return;
        const QString symbol = sym_item->text();
        QMenu menu(this);
        QAction* buy = menu.addAction(tr("Buy / Add  %1").arg(symbol));
        QAction* sell = menu.addAction(tr("Sell / Reduce  %1").arg(symbol));
        auto* qty_item = holdings_table_->item(row, 1); // Qty column
        const double held = qty_item ? qAbs(qty_item->text().toDouble()) : 0.0;
        QAction* chosen = menu.exec(holdings_table_->viewport()->mapToGlobal(p));
        if (chosen == buy)
            emit trade_symbol_requested(symbol, QStringLiteral("CNC"), true, 0.0);
        else if (chosen == sell)
            emit trade_symbol_requested(symbol, QStringLiteral("CNC"), false, held);
    });

    v->addWidget(holdings_table_, 1);
    holdings_tab_idx_ = tabs_->addTab(tab, tr("HOLDINGS"));
}

// ── Orders Tab ─────────────────────────────────────────────────────────────

void EquityBottomPanel::setup_orders_tab() {
    auto* container = new QWidget;
    auto* vlay = new QVBoxLayout(container);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);

    // Action bar with Cancel All button
    auto* action_bar = new QWidget;
    action_bar->setObjectName("eqOrdersActionBar");
    action_bar->setStyleSheet(QString("#eqOrdersActionBar{background:%1;border-bottom:1px solid %2;}")
                                  .arg(fincept::ui::colors::PANEL(), fincept::ui::colors::BORDER()));
    auto* bar_layout = new QHBoxLayout(action_bar);
    bar_layout->setContentsMargins(12, 4, 12, 4);
    bar_layout->setSpacing(8);

    // Per-day order book: pick a date to view that IST day's orders (default today,
    // so each session starts empty instead of accumulating every past order).
    auto* date_lbl = new QLabel(tr("DATE"));
    date_lbl->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:600;letter-spacing:0.5px;").arg(fincept::ui::colors::TEXT_SECONDARY()));
    orders_date_edit_ = new QDateEdit(QDate::currentDate());
    orders_date_edit_->setObjectName("eqOrdersDate");
    orders_date_edit_->setCalendarPopup(true);
    orders_date_edit_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    orders_date_edit_->setMaximumDate(QDate::currentDate());
    orders_date_edit_->setStyleSheet(
        QString("QDateEdit{background:%1;color:%2;border:1px solid %3;padding:2px 6px;font-size:11px;border-radius:2px;}")
            .arg(fincept::ui::colors::BG_BASE(), fincept::ui::colors::TEXT_PRIMARY(), fincept::ui::colors::BORDER()));
    connect(orders_date_edit_, &QDateEdit::dateChanged, this, [this](const QDate& d) {
        if (!suppress_orders_date_signal_)
            emit orders_day_changed(d);
    });
    bar_layout->addWidget(date_lbl);
    bar_layout->addWidget(orders_date_edit_);
    bar_layout->addStretch(1);

    cancel_all_btn_ = new QPushButton(tr("CANCEL ALL ORDERS"));
    cancel_all_btn_->setCursor(Qt::PointingHandCursor);
    cancel_all_btn_->setStyleSheet(
        QString("QPushButton{background:rgba(217,119,6,0.12);color:%1;border:1px solid %2;"
                "padding:4px 14px;font-size:11px;font-weight:700;letter-spacing:0.5px;border-radius:2px;}"
                "QPushButton:hover{background:rgba(217,119,6,0.25);}")
            .arg(fincept::ui::colors::AMBER(), fincept::ui::colors::AMBER_DIM()));
    connect(cancel_all_btn_, &QPushButton::clicked, this, [this]() {
        if (account_id_.isEmpty())
            return;
        auto answer = QMessageBox::warning(this, tr("Cancel All Orders"),
                                           tr("This will cancel ALL pending orders.\n\nAre you sure?"),
                                           QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
        emit cancel_all_orders_requested(account_id_);
    });
    bar_layout->addWidget(cancel_all_btn_);
    vlay->addWidget(action_bar);

    orders_table_ = new QTableWidget;
    orders_table_->setObjectName("eqTable");
    orders_table_->setColumnCount(10);
    orders_table_->setHorizontalHeaderLabels({tr("Order ID"), tr("Symbol"), tr("Product"), tr("Side"), tr("Type"),
                                              tr("Qty"), tr("Price"), tr("Status"), tr("Time"), tr("Action")});
    orders_table_->verticalHeader()->setVisible(false);
    orders_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    orders_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    orders_table_->setShowGrid(false);
    orders_table_->horizontalHeader()->setStretchLastSection(true);
    orders_table_->verticalHeader()->setDefaultSectionSize(22);
    vlay->addWidget(orders_table_, 1);

    orders_tab_idx_ = tabs_->addTab(container, tr("ORDERS"));
}

// ── Funds Tab ──────────────────────────────────────────────────────────────

// Build one metric card (caption + value) into a grid; stash the value/caption
// labels so the data setters and retranslateUi can reach them by slot index.
static QFrame* make_metric_card(const QString& caption, QLabel*& cap_out, QLabel*& val_out) {
    auto* card = new QFrame;
    card->setObjectName("eqMetricCard");
    card->setStyleSheet(QString("#eqMetricCard{background:%1;border:1px solid %2;border-radius:4px;}")
                            .arg(fincept::ui::colors::PANEL(), fincept::ui::colors::BORDER()));
    auto* cv = new QVBoxLayout(card);
    cv->setContentsMargins(12, 8, 12, 8);
    cv->setSpacing(3);
    auto* cap = new QLabel(caption);
    cap->setStyleSheet(
        QString("color:%1;font-size:10px;letter-spacing:0.5px;font-weight:600;").arg(fincept::ui::colors::TEXT_SECONDARY()));
    auto* val = new QLabel(QStringLiteral("--"));
    val->setStyleSheet(QString("color:%1;font-size:15px;font-weight:700;").arg(fincept::ui::colors::TEXT_PRIMARY()));
    cv->addWidget(cap);
    cv->addWidget(val);
    cap_out = cap;
    val_out = val;
    return card;
}

void EquityBottomPanel::setup_funds_tab() {
    auto* scroll = new QScrollArea;
    scroll->setObjectName("eqFundsScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* widget = new QWidget;
    auto* outer = new QVBoxLayout(widget);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(10);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);

    const QString caps[kFundCards] = {tr("AVAILABLE BALANCE"), tr("USED MARGIN"),   tr("TOTAL EQUITY"),
                                      tr("REALIZED P&L"),      tr("UNREALIZED P&L"), tr("HOLDINGS VALUE"),
                                      tr("OPENING BALANCE"),   tr("MARGIN USED %"),  tr("COLLATERAL")};
    for (int i = 0; i < kFundCards; ++i)
        grid->addWidget(make_metric_card(caps[i], fund_card_cap_[i], fund_card_val_[i]), i / 3, i % 3);
    for (int c = 0; c < 3; ++c)
        grid->setColumnStretch(c, 1);

    outer->addLayout(grid);
    outer->addStretch(1);
    scroll->setWidget(widget);
    funds_tab_idx_ = tabs_->addTab(scroll, tr("FUNDS"));
}

// ── Stats Tab ──────────────────────────────────────────────────────────────

void EquityBottomPanel::setup_stats_tab() {
    auto* scroll = new QScrollArea;
    scroll->setObjectName("eqStatsScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* widget = new QWidget;
    auto* outer = new QVBoxLayout(widget);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(10);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);

    const QString caps[kStatCards] = {tr("NET P&L"),      tr("TODAY'S P&L"),  tr("RETURN %"),
                                      tr("WIN RATE"),     tr("PROFIT FACTOR"), tr("TOTAL TRADES"),
                                      tr("AVG WIN"),      tr("AVG LOSS"),     tr("TURNOVER"),
                                      tr("LARGEST WIN"),  tr("LARGEST LOSS"), tr("TOTAL CHARGES")};
    for (int i = 0; i < kStatCards; ++i)
        grid->addWidget(make_metric_card(caps[i], stat_card_cap_[i], stat_card_val_[i]), i / 3, i % 3);
    for (int c = 0; c < 3; ++c)
        grid->setColumnStretch(c, 1);

    outer->addLayout(grid);
    outer->addStretch(1);
    scroll->setWidget(widget);
    stats_tab_idx_ = tabs_->addTab(scroll, tr("STATS"));
}

// ── Data Setters ───────────────────────────────────────────────────────────

void EquityBottomPanel::clear_blotter_tables() {
    if (positions_table_) positions_table_->setRowCount(0);
    if (holdings_table_)  holdings_table_->setRowCount(0);
    if (orders_table_)    orders_table_->setRowCount(0);
    last_positions_.clear();
    last_paper_positions_.clear();
    last_holdings_.clear();
    update_positions_summary(); // zero the net-P&L / winners-losers strip
}

void EquityBottomPanel::set_mode(bool is_paper) {
    if (is_paper_ == is_paper)
        return;
    is_paper_ = is_paper;
    // Paper and live share the same tables — clear so the previous mode's rows
    // can't linger under the new one until fresh data arrives.
    clear_blotter_tables();
}

void EquityBottomPanel::set_account_id(const QString& account_id) {
    if (account_id_ == account_id)
        return;
    account_id_ = account_id;
    // Different account → drop the previous account's positions/holdings/orders.
    clear_blotter_tables();
}

void EquityBottomPanel::set_currency(const QString& sym) {
    if (!sym.isEmpty())
        currency_sym_ = sym;
}

void EquityBottomPanel::set_us_market_tabs_visible(bool visible) {
    if (time_sales_tab_idx_ >= 0)
        tabs_->setTabVisible(time_sales_tab_idx_, visible);
    if (auctions_tab_idx_ >= 0)
        tabs_->setTabVisible(auctions_tab_idx_, visible);
    if (calendar_tab_idx_ >= 0)
        tabs_->setTabVisible(calendar_tab_idx_, visible);
}

void EquityBottomPanel::update_positions_summary() {
    double total = 0.0, win_sum = 0.0, loss_sum = 0.0;
    int win_n = 0, loss_n = 0;
    auto accum = [&](double pnl) {
        total += pnl;
        if (pnl > 0.0) {
            win_sum += pnl;
            ++win_n;
        } else if (pnl < 0.0) {
            loss_sum += pnl;
            ++loss_n;
        }
    };
    if (is_paper_)
        for (const auto& p : last_paper_positions_)
            accum(p.unrealized_pnl);
    else
        for (const auto& p : last_positions_)
            accum(p.pnl);

    const QString pos = fincept::ui::colors::POSITIVE();
    const QString neg = fincept::ui::colors::NEGATIVE();
    const QString pri = fincept::ui::colors::TEXT_PRIMARY();
    if (positions_total_pnl_label_) {
        positions_total_pnl_label_->setText(tr("Total P&L  %1").arg(fmt_money(total, currency_sym_, true)));
        positions_total_pnl_label_->setStyleSheet(
            QString("font-size:11px;font-weight:700;color:%1;").arg(total > 0 ? pos : (total < 0 ? neg : pri)));
    }
    if (positions_win_label_)
        positions_win_label_->setText(
            QString::fromUtf8("▲ %1  %2").arg(win_n).arg(fmt_money(win_sum, currency_sym_, true)));
    if (positions_loss_label_)
        positions_loss_label_->setText(
            QString::fromUtf8("▼ %1  %2").arg(loss_n).arg(fmt_money(loss_sum, currency_sym_, true)));
    if (win_squareoff_btn_)
        win_squareoff_btn_->setEnabled(win_n > 0);
    if (loss_squareoff_btn_)
        loss_squareoff_btn_->setEnabled(loss_n > 0);
}

void EquityBottomPanel::set_paper_positions(const QVector<trading::PtPosition>& positions) {
    last_paper_positions_ = positions; // keep row-aligned for live-quote patching
    const QColor pos_color(fincept::ui::colors::POSITIVE());
    const QColor neg_color(fincept::ui::colors::NEGATIVE());
    positions_table_->setRowCount(positions.size());
    for (int i = 0; i < positions.size(); ++i) {
        const auto& p = positions[i];
        ensure_item(positions_table_, i, 0)->setText(p.symbol);
        ensure_item(positions_table_, i, 1)->setText(p.product.isEmpty() ? QStringLiteral("MIS") : p.product.toUpper());

        // Col 2 — "Opened": parse ISO timestamp, convert to local time, fall back to raw string
        {
            const QDateTime dt = QDateTime::fromString(p.opened_at, Qt::ISODate).toLocalTime();
            ensure_item(positions_table_, i, 2)
                ->setText(dt.isValid() ? dt.toString("yyyy-MM-dd HH:mm") : p.opened_at);
        }

        ensure_item(positions_table_, i, 3)->setText(p.side.toUpper());
        ensure_item(positions_table_, i, 4)->setText(QString::number(p.quantity, 'f', 0));
        ensure_item(positions_table_, i, 5)->setText(QString::number(p.entry_price, 'f', 2));
        ensure_item(positions_table_, i, 6)->setText(QString::number(p.current_price, 'f', 2));

        // Col 7 — P&L
        auto* pnl_item = ensure_item(positions_table_, i, 7);
        pnl_item->setText(QString::number(p.unrealized_pnl, 'f', 2));
        pnl_item->setForeground(p.unrealized_pnl >= 0 ? pos_color : neg_color);

        // Col 8 — P&L %. Use notional as the divisor so both entry_price == 0 and
        // quantity == 0 are handled by a single guard (avoids divide-by-zero).
        const double notional = p.entry_price * p.quantity;
        const double pct = notional != 0.0 ? (p.unrealized_pnl / notional) * 100.0 : 0.0;
        auto* pct_item = ensure_item(positions_table_, i, 8);
        pct_item->setText(QString::number(pct, 'f', 2) + "%");
        pct_item->setForeground(pct >= 0 ? pos_color : neg_color);

        // Col 9 — Action: SELL (exit, pre-filled qty) + for intraday (MIS), a
        // "→ CNC" convert-to-delivery button. NRML/CNC rows get SELL only.
        positions_table_->setCellWidget(
            i, 9,
            make_positions_action_cell(p.symbol, p.product, p.quantity,
                                       trading::product_is_intraday(p.product), p.id));
    }
    update_positions_summary();
}

void EquityBottomPanel::set_paper_orders(const QVector<trading::PtOrder>& orders) {
    orders_table_->setRowCount(orders.size());
    for (int i = 0; i < orders.size(); ++i) {
        const auto& o = orders[i];
        ensure_item(orders_table_, i, 0)->setText(o.id.left(8));
        ensure_item(orders_table_, i, 1)->setText(o.symbol);
        ensure_item(orders_table_, i, 2)->setText(o.product.isEmpty() ? QStringLiteral("MIS") : o.product.toUpper());
        ensure_item(orders_table_, i, 3)->setText(o.side.toUpper());
        ensure_item(orders_table_, i, 4)->setText(o.order_type.toUpper());
        ensure_item(orders_table_, i, 5)->setText(QString::number(o.quantity, 'f', 0));
        ensure_item(orders_table_, i, 6)->setText(o.price ? QString::number(*o.price, 'f', 2) : tr("MKT"));

        auto* status_item = ensure_item(orders_table_, i, 7);
        status_item->setText(o.status.toUpper());
        status_item->setForeground(order_status_color(o.status));

        const QDateTime dt = QDateTime::fromString(o.created_at, Qt::ISODate).toLocalTime();
        ensure_item(orders_table_, i, 8)->setText(dt.isValid() ? dt.toString("HH:mm:ss") : o.created_at);
        // Paper orders are filled synchronously / by the matcher; no inline edit.
        orders_table_->setCellWidget(i, 9, nullptr);
        ensure_item(orders_table_, i, 9)->setText("");
    }
}

void EquityBottomPanel::set_funds_view(const EquityFundsView& v) {
    const QString sym = v.currency;
    const QString pos = fincept::ui::colors::POSITIVE();
    const QString neg = fincept::ui::colors::NEGATIVE();
    const QString pri = fincept::ui::colors::TEXT_PRIMARY();

    auto set_plain = [&](int idx, const QString& text) {
        if (!fund_card_val_[idx])
            return;
        fund_card_val_[idx]->setText(text);
        fund_card_val_[idx]->setStyleSheet(QString("color:%1;font-size:15px;font-weight:700;").arg(pri));
    };
    auto set_pnl = [&](int idx, double val) {
        if (!fund_card_val_[idx])
            return;
        fund_card_val_[idx]->setText(fmt_money(val, sym, true));
        fund_card_val_[idx]->setStyleSheet(
            QString("color:%1;font-size:15px;font-weight:700;").arg(val >= 0 ? pos : neg));
    };

    set_plain(FC_Available, fmt_money(v.available, sym));
    set_plain(FC_UsedMargin, fmt_money(v.used_margin, sym));
    set_plain(FC_TotalEquity, fmt_money(v.total_equity, sym));
    set_pnl(FC_RealizedPnl, v.realized_pnl);
    set_pnl(FC_UnrealizedPnl, v.unrealized_pnl);
    set_plain(FC_HoldingsValue, fmt_money(v.holdings_value, sym));
    set_plain(FC_OpeningBalance, fmt_money(v.opening_balance, sym));
    set_plain(FC_MarginUtil, QString::number(v.margin_util_pct, 'f', 1) + "%");
    set_plain(FC_Collateral, fmt_money(v.collateral, sym));
}

void EquityBottomPanel::set_stats_view(const EquityStatsView& v) {
    const QString sym = v.currency;
    const QString pos = fincept::ui::colors::POSITIVE();
    const QString neg = fincept::ui::colors::NEGATIVE();
    const QString pri = fincept::ui::colors::TEXT_PRIMARY();

    auto set_card = [&](int idx, const QString& text, const QString& color) {
        if (!stat_card_val_[idx])
            return;
        stat_card_val_[idx]->setText(text);
        stat_card_val_[idx]->setStyleSheet(QString("color:%1;font-size:15px;font-weight:700;").arg(color));
    };
    auto pnl_color = [&](double x) { return x >= 0 ? pos : neg; };

    set_card(SC_NetPnl, fmt_money(v.net_pnl, sym, true), pnl_color(v.net_pnl));
    set_card(SC_TodayPnl, fmt_money(v.today_pnl, sym, true), pnl_color(v.today_pnl));
    set_card(SC_ReturnPct,
             QString("%1%2%").arg(v.return_pct >= 0 ? "+" : "").arg(QString::number(v.return_pct, 'f', 2)),
             pnl_color(v.return_pct));
    set_card(SC_WinRate,
             QString("%1% (%2/%3)")
                 .arg(QString::number(v.win_rate * 100, 'f', 1))
                 .arg(v.winning_trades)
                 .arg(v.winning_trades + v.losing_trades),
             pri);
    set_card(SC_ProfitFactor, v.profit_factor > 0 ? QString::number(v.profit_factor, 'f', 2) : QStringLiteral("--"),
             pri);
    set_card(SC_TotalTrades, QString::number(v.total_trades), pri);
    set_card(SC_AvgWin, fmt_money(v.avg_win, sym, true), pos);
    set_card(SC_AvgLoss, fmt_money(v.avg_loss, sym, true), neg);
    set_card(SC_Turnover, fmt_money(v.turnover, sym), pri);
    set_card(SC_LargestWin, fmt_money(v.largest_win, sym, true), pos);
    set_card(SC_LargestLoss, fmt_money(v.largest_loss, sym, true), neg);
    set_card(SC_TotalFees, fmt_money(v.total_fees, sym), pri);
}

void EquityBottomPanel::set_orders_date(const QDate& day) {
    if (!orders_date_edit_)
        return;
    suppress_orders_date_signal_ = true;
    orders_date_edit_->setDate(day);
    suppress_orders_date_signal_ = false;
}

QWidget* EquityBottomPanel::make_positions_action_cell(const QString& symbol, const QString& product,
                                                       double qty, bool show_convert,
                                                       const QString& paper_pid) {
    auto* cell = new QWidget;
    auto* lay = new QHBoxLayout(cell);
    lay->setContentsMargins(2, 0, 2, 0);
    lay->setSpacing(4);

    // SELL → opens the order ticket pre-filled with the held quantity (parity with
    // the FNO tab's per-strike order button, instead of right-click-only).
    auto* sell = new QPushButton(tr("SELL"));
    sell->setObjectName("eqTableBtn");
    sell->setFixedHeight(18);
    sell->setCursor(Qt::PointingHandCursor);
    sell->setToolTip(tr("Sell / exit %1 — opens an order ticket pre-filled with the held quantity").arg(symbol));
    sell->setStyleSheet(QString("QPushButton#eqTableBtn{background:rgba(239,68,68,0.15);color:%1;"
                                "border:1px solid %2;font-size:10px;padding:0 6px;border-radius:2px;}"
                                "QPushButton#eqTableBtn:hover{background:rgba(239,68,68,0.30);}")
                            .arg(fincept::ui::colors::NEGATIVE(), fincept::ui::colors::BORDER_MED()));
    const double held = qAbs(qty);
    const QString prod = product.isEmpty() ? QStringLiteral("MIS") : product;
    connect(sell, &QPushButton::clicked, this, [this, symbol, prod, held]() {
        LOG_INFO("sqoff", QString("[panel] per-row SELL clicked: sym='%1' prod='%2' qty=%3")
                              .arg(symbol, prod)
                              .arg(held));
        emit trade_symbol_requested(symbol, prod, false, held);
    });
    lay->addWidget(sell);

    // Paper intraday only: convert MIS → CNC (carry overnight).
    if (show_convert && !paper_pid.isEmpty()) {
        auto* cnc = new QPushButton(tr("→ CNC"));
        cnc->setObjectName("eqTableBtn");
        cnc->setFixedHeight(18);
        cnc->setCursor(Qt::PointingHandCursor);
        cnc->setToolTip(tr("Convert to CNC delivery (carry overnight, locks full cash)"));
        cnc->setStyleSheet(QString("QPushButton#eqTableBtn{background:rgba(37,99,235,0.15);color:%1;"
                                   "border:1px solid %2;font-size:10px;padding:0 6px;border-radius:2px;}"
                                   "QPushButton#eqTableBtn:hover{background:rgba(37,99,235,0.30);}")
                               .arg(fincept::ui::colors::INFO(), fincept::ui::colors::BORDER_MED()));
        connect(cnc, &QPushButton::clicked, this, [this, paper_pid, symbol]() {
            emit convert_position_requested(paper_pid, symbol, QStringLiteral("CNC"));
        });
        lay->addWidget(cnc);
    }
    lay->addStretch();
    return cell;
}

void EquityBottomPanel::set_positions(const QVector<trading::BrokerPosition>& positions) {
    last_positions_ = positions; // keep row-aligned for live-quote patching
    const QColor pos_color(fincept::ui::colors::POSITIVE());
    const QColor neg_color(fincept::ui::colors::NEGATIVE());
    positions_table_->setRowCount(positions.size());
    for (int i = 0; i < positions.size(); ++i) {
        const auto& p = positions[i];
        ensure_item(positions_table_, i, 0)->setText(p.symbol);
        ensure_item(positions_table_, i, 1)->setText(p.product_type.isEmpty() ? "--" : p.product_type.toUpper());
        ensure_item(positions_table_, i, 2)->setText(p.exchange.isEmpty() ? "--" : p.exchange);
        ensure_item(positions_table_, i, 3)->setText(p.side.toUpper());
        ensure_item(positions_table_, i, 4)->setText(QString::number(p.quantity, 'f', 0));
        ensure_item(positions_table_, i, 5)->setText(QString::number(p.avg_price, 'f', 2));
        ensure_item(positions_table_, i, 6)->setText(QString::number(p.ltp, 'f', 2));

        auto* pnl_item = ensure_item(positions_table_, i, 7);
        pnl_item->setText(QString::number(p.pnl, 'f', 2));
        pnl_item->setForeground(p.pnl >= 0 ? pos_color : neg_color);

        auto* pct_item = ensure_item(positions_table_, i, 8);
        pct_item->setText(QString("%1%").arg(p.pnl_pct, 0, 'f', 2));
        pct_item->setForeground(p.pnl_pct >= 0 ? pos_color : neg_color);

        // SELL (exit) action; no MIS→CNC convert in live mode (paper-only).
        positions_table_->setCellWidget(
            i, 9, make_positions_action_cell(p.symbol, p.product_type, p.quantity, false, QString()));
    }
    update_positions_summary();
}

void EquityBottomPanel::update_holding_quote(const QString& symbol, double ltp, double prev_close) {
    // Find this symbol's holding (one per symbol) and re-mark it to the live price.
    trading::BrokerHolding* h = nullptr;
    for (auto& it : last_holdings_)
        if (it.symbol == symbol) {
            h = &it;
            break;
        }
    if (!h)
        return;
    h->ltp = ltp;
    if (prev_close > 0.0)
        h->prev_close = prev_close; // capture today's prev close from the live quote
    h->current_value = h->quantity * ltp;
    h->pnl = h->current_value - h->invested_value;
    h->pnl_pct = h->invested_value > 0.0 ? (h->pnl / h->invested_value) * 100.0 : 0.0;
    const double day_pnl = h->prev_close > 0.0 ? h->quantity * (ltp - h->prev_close) : 0.0;

    const QColor pos_color(fincept::ui::colors::POSITIVE());
    const QColor neg_color(fincept::ui::colors::NEGATIVE());

    // Patch the row in place. Sorting may reorder rows, so match on the symbol cell
    // rather than assuming index alignment. Disable sorting during the writes.
    const bool was_sorting = holdings_table_->isSortingEnabled();
    holdings_table_->setSortingEnabled(false);
    for (int r = 0; r < holdings_table_->rowCount(); ++r) {
        auto* sym_item = holdings_table_->item(r, 0);
        if (!sym_item || sym_item->text() != symbol)
            continue;
        auto set_num = [&](int col, double v) {
            auto* item = ensure_item(holdings_table_, r, col);
            item->setData(Qt::DisplayRole, QString::number(v, 'f', 2));
            item->setData(Qt::EditRole, v);
        };
        set_num(3, ltp);               // LTP
        set_num(5, h->current_value);  // Current
        auto* pnl_item = ensure_item(holdings_table_, r, 6);
        pnl_item->setData(Qt::DisplayRole, QString::number(h->pnl, 'f', 2));
        pnl_item->setData(Qt::EditRole, h->pnl);
        pnl_item->setForeground(h->pnl >= 0 ? pos_color : neg_color);
        auto* pct_item = ensure_item(holdings_table_, r, 7);
        pct_item->setData(Qt::DisplayRole, QString("%1%").arg(h->pnl_pct, 0, 'f', 2));
        pct_item->setData(Qt::EditRole, h->pnl_pct);
        pct_item->setForeground(h->pnl_pct >= 0 ? pos_color : neg_color);
        auto* day_item = ensure_item(holdings_table_, r, 8); // Today's P&L
        day_item->setData(Qt::DisplayRole, QString::number(day_pnl, 'f', 2));
        day_item->setData(Qt::EditRole, day_pnl);
        day_item->setForeground(day_pnl >= 0 ? pos_color : neg_color);
        break; // one row per symbol
    }
    holdings_table_->setSortingEnabled(was_sorting);

    // Refresh the summary strip (CURRENT / TOTAL P&L / TODAY'S P&L / RETURN %); invested is fixed.
    double total_invested = 0.0, total_current = 0.0, total_pnl = 0.0, total_day_pnl = 0.0;
    for (const auto& it : last_holdings_) {
        total_invested += it.invested_value;
        total_current += it.current_value;
        total_pnl += it.pnl;
        if (it.prev_close > 0.0)
            total_day_pnl += it.quantity * (it.ltp - it.prev_close);
    }
    const double total_pct = total_invested > 0.0 ? (total_pnl / total_invested) * 100.0 : 0.0;
    if (holdings_current_label_)
        holdings_current_label_->setText(QString::number(total_current, 'f', 2));
    if (holdings_pnl_label_) {
        holdings_pnl_label_->setText(
            QString("%1%2").arg(total_pnl >= 0 ? "+" : "").arg(QString::number(total_pnl, 'f', 2)));
        holdings_pnl_label_->setStyleSheet(
            QString("color:%1;font-size:13px;font-weight:700;")
                .arg(total_pnl >= 0 ? fincept::ui::colors::POSITIVE() : fincept::ui::colors::NEGATIVE()));
    }
    if (holdings_day_pnl_label_) {
        holdings_day_pnl_label_->setText(
            QString("%1%2").arg(total_day_pnl >= 0 ? "+" : "").arg(QString::number(total_day_pnl, 'f', 2)));
        holdings_day_pnl_label_->setStyleSheet(
            QString("color:%1;font-size:13px;font-weight:700;")
                .arg(total_day_pnl >= 0 ? fincept::ui::colors::POSITIVE() : fincept::ui::colors::NEGATIVE()));
    }
    if (holdings_pnl_pct_label_) {
        holdings_pnl_pct_label_->setText(
            QString("%1%2%").arg(total_pct >= 0 ? "+" : "").arg(QString::number(total_pct, 'f', 2)));
        holdings_pnl_pct_label_->setStyleSheet(
            QString("color:%1;font-size:13px;font-weight:700;")
                .arg(total_pct >= 0 ? fincept::ui::colors::POSITIVE() : fincept::ui::colors::NEGATIVE()));
    }
}

void EquityBottomPanel::update_quote(const QString& symbol, const trading::BrokerQuote& quote) {
    const double ltp = quote.ltp;
    if (ltp <= 0.0 || symbol.isEmpty())
        return; // ignore empty / zero-price ticks — keep the last good values

    {
        // [posdbg] throttled — which quote symbols arrive vs the position symbols.
        static qint64 s_last = 0;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - s_last > 3000) {
            s_last = now;
            QStringList ps;
            if (is_paper_)
                for (const auto& p : last_paper_positions_)
                    ps << p.symbol;
            else
                for (const auto& p : last_positions_)
                    ps << p.symbol;
            LOG_INFO("posdbg", QString("update_quote sym='%1' ltp=%2 (%3) positions=[%4]")
                                   .arg(symbol)
                                   .arg(ltp, 0, 'f', 2)
                                   .arg(is_paper_ ? "paper" : "live", ps.join(',')));
        }
    }

    // Holdings (CNC) live the same quote stream as positions — patch them too.
    // quote.close is today's previous-day close → drives the "Today's P&L" column.
    update_holding_quote(symbol, ltp, quote.close);

    const QColor pos_color(fincept::ui::colors::POSITIVE());
    const QColor neg_color(fincept::ui::colors::NEGATIVE());

    // Columns shift by +1 vs the pre-Product layout: LTP=6, P&L=7, P&L%=8.
    auto paint = [&](int row, double pnl, double pnl_pct) {
        ensure_item(positions_table_, row, 6)->setText(QString::number(ltp, 'f', 2));
        auto* pnl_item = ensure_item(positions_table_, row, 7);
        pnl_item->setText(QString::number(pnl, 'f', 2));
        pnl_item->setForeground(pnl >= 0 ? pos_color : neg_color);
        auto* pct_item = ensure_item(positions_table_, row, 8);
        pct_item->setText(QString("%1%").arg(pnl_pct, 0, 'f', 2));
        pct_item->setForeground(pnl_pct >= 0 ? pos_color : neg_color);
    };

    if (is_paper_) {
        // Paper engine convention: positive quantity + side ("long"/"short").
        // Mirror PaperTradingRepository::update_position_price exactly so the row
        // and the engine never diverge.
        for (int i = 0; i < last_paper_positions_.size(); ++i) {
            auto& p = last_paper_positions_[i];
            if (p.symbol != symbol)
                continue;
            p.current_price = ltp;
            p.unrealized_pnl = (p.side == QLatin1String("long"))
                                   ? (ltp - p.entry_price) * p.quantity
                                   : (p.entry_price - ltp) * p.quantity;
            const double notional = p.entry_price * p.quantity;
            const double pct = notional != 0.0 ? (p.unrealized_pnl / notional) * 100.0 : 0.0;
            paint(i, p.unrealized_pnl, pct);
        }
        update_positions_summary();
        return;
    }

    // Live broker positions. Quantity may be signed (e.g. Fyers netQty < 0 for
    // shorts) or positive-with-side; honor the side flag so direction is correct
    // regardless of broker convention.
    for (int i = 0; i < last_positions_.size(); ++i) {
        auto& p = last_positions_[i];
        if (p.symbol != symbol)
            continue;
        double signed_qty = p.quantity;
        if (p.quantity > 0 && p.side.startsWith(QLatin1Char('s'), Qt::CaseInsensitive))
            signed_qty = -p.quantity; // broker reports a short as +qty with "sell"/"short"
        p.ltp = ltp;
        p.pnl = (ltp - p.avg_price) * signed_qty;
        const double notional = p.avg_price * qAbs(p.quantity);
        p.pnl_pct = notional > 0.0 ? (p.pnl / notional) * 100.0 : 0.0;
        paint(i, p.pnl, p.pnl_pct);
    }
    update_positions_summary();
}

void EquityBottomPanel::set_holdings(const QVector<trading::BrokerHolding>& holdings) {
    // Carry forward prev_close (today's close, learned from the live quote feed)
    // across REST holdings refreshes so the "Today's P&L" column/total don't blink
    // to 0 on every poll — the REST payload doesn't carry prev_close.
    QHash<QString, double> prev_close_by_sym;
    for (const auto& it : last_holdings_)
        if (it.prev_close > 0.0)
            prev_close_by_sym.insert(it.symbol, it.prev_close);
    last_holdings_ = holdings;
    for (auto& it : last_holdings_) {
        const auto pc = prev_close_by_sym.constFind(it.symbol);
        if (pc != prev_close_by_sym.constEnd())
            it.prev_close = pc.value();
    }
    if (holdings_import_btn_)
        holdings_import_btn_->setEnabled(!holdings.isEmpty());
    if (holdings_square_off_btn_)
        holdings_square_off_btn_->setEnabled(!holdings.isEmpty());

    // Disable sorting during population so setItem assignments stay at intended rows.
    const bool was_sorting = holdings_table_->isSortingEnabled();
    holdings_table_->setSortingEnabled(false);
    holdings_table_->setRowCount(holdings.size());

    double total_invested = 0.0;
    double total_current = 0.0;
    double total_pnl = 0.0;
    double total_day_pnl = 0.0;

    const QColor pos_color(fincept::ui::colors::POSITIVE());
    const QColor neg_color(fincept::ui::colors::NEGATIVE());

    auto set_num = [](QTableWidgetItem* it, double v, int precision) {
        it->setData(Qt::DisplayRole, QString::number(v, 'f', precision));
        it->setData(Qt::EditRole, v);
    };

    for (int i = 0; i < last_holdings_.size(); ++i) {
        const auto& h = last_holdings_[i]; // carries prev_close across refreshes
        ensure_item(holdings_table_, i, 0)->setText(h.symbol);
        set_num(ensure_item(holdings_table_, i, 1), h.quantity, 0);
        set_num(ensure_item(holdings_table_, i, 2), h.avg_price, 2);
        set_num(ensure_item(holdings_table_, i, 3), h.ltp, 2);
        set_num(ensure_item(holdings_table_, i, 4), h.invested_value, 2);
        set_num(ensure_item(holdings_table_, i, 5), h.current_value, 2);

        auto* pnl_item = ensure_item(holdings_table_, i, 6);
        pnl_item->setData(Qt::DisplayRole, QString::number(h.pnl, 'f', 2));
        pnl_item->setData(Qt::EditRole, h.pnl);
        pnl_item->setForeground(h.pnl >= 0 ? pos_color : neg_color);

        auto* pct_item = ensure_item(holdings_table_, i, 7);
        pct_item->setData(Qt::DisplayRole, QString("%1%").arg(h.pnl_pct, 0, 'f', 2));
        pct_item->setData(Qt::EditRole, h.pnl_pct);
        pct_item->setForeground(h.pnl_pct >= 0 ? pos_color : neg_color);

        // Today's P&L = qty * (LTP - prev close). prev_close starts 0 (shown as 0)
        // and is filled from the live quote feed in update_holding_quote — generic
        // across brokers, no per-broker holdings parser needed.
        const double day_pnl = h.prev_close > 0.0 ? h.quantity * (h.ltp - h.prev_close) : 0.0;
        auto* day_item = ensure_item(holdings_table_, i, 8);
        day_item->setData(Qt::DisplayRole, QString::number(day_pnl, 'f', 2));
        day_item->setData(Qt::EditRole, day_pnl);
        day_item->setForeground(day_pnl >= 0 ? pos_color : neg_color);
        total_day_pnl += day_pnl;

        // Per-row SELL → square off just this holding (market, CNC). Routed through
        // the screen's close_position path (same as SQUARE OFF ALL) so it reliably
        // reduces the holding instead of opening a counter-position.
        auto* action_cell = new QWidget;
        auto* action_lay = new QHBoxLayout(action_cell);
        action_lay->setContentsMargins(2, 0, 2, 0);
        action_lay->setSpacing(4);
        auto* sell_btn = new QPushButton(tr("SELL"));
        sell_btn->setObjectName("eqTableBtn");
        sell_btn->setFixedHeight(18);
        sell_btn->setCursor(Qt::PointingHandCursor);
        sell_btn->setToolTip(tr("Square off %1 — sells the full holding at market").arg(h.symbol));
        sell_btn->setStyleSheet(QString("QPushButton#eqTableBtn{background:rgba(239,68,68,0.15);color:%1;"
                                        "border:1px solid %2;font-size:10px;padding:0 6px;border-radius:2px;}"
                                        "QPushButton#eqTableBtn:hover{background:rgba(239,68,68,0.30);}")
                                    .arg(fincept::ui::colors::NEGATIVE(), fincept::ui::colors::BORDER_MED()));
        const QString row_sym = h.symbol;
        const QString row_exch = h.exchange;
        connect(sell_btn, &QPushButton::clicked, this, [this, row_sym, row_exch]() {
            LOG_INFO("sqoff", QString("[panel] per-row SELL clicked: sym='%1' exch='%2'").arg(row_sym, row_exch));
            emit square_off_holding_requested(row_sym, row_exch);
        });
        action_lay->addWidget(sell_btn);
        action_lay->addStretch();
        holdings_table_->setCellWidget(i, 9, action_cell);

        total_invested += h.invested_value;
        total_current  += h.current_value;
        total_pnl      += h.pnl;
    }

    const double total_pct = total_invested > 0 ? (total_pnl / total_invested) * 100.0 : 0.0;

    if (holdings_count_label_)
        holdings_count_label_->setText(QString::number(holdings.size()));
    if (holdings_invested_label_)
        holdings_invested_label_->setText(QString::number(total_invested, 'f', 2));
    if (holdings_current_label_)
        holdings_current_label_->setText(QString::number(total_current, 'f', 2));
    if (holdings_pnl_label_) {
        holdings_pnl_label_->setText(QString("%1%2")
                                         .arg(total_pnl >= 0 ? "+" : "")
                                         .arg(QString::number(total_pnl, 'f', 2)));
        holdings_pnl_label_->setStyleSheet(
            QString("color:%1;font-size:13px;font-weight:700;")
                .arg(total_pnl >= 0 ? fincept::ui::colors::POSITIVE() : fincept::ui::colors::NEGATIVE()));
    }
    if (holdings_day_pnl_label_) {
        holdings_day_pnl_label_->setText(QString("%1%2")
                                             .arg(total_day_pnl >= 0 ? "+" : "")
                                             .arg(QString::number(total_day_pnl, 'f', 2)));
        holdings_day_pnl_label_->setStyleSheet(
            QString("color:%1;font-size:13px;font-weight:700;")
                .arg(total_day_pnl >= 0 ? fincept::ui::colors::POSITIVE() : fincept::ui::colors::NEGATIVE()));
    }
    if (holdings_pnl_pct_label_) {
        holdings_pnl_pct_label_->setText(QString("%1%2%")
                                             .arg(total_pct >= 0 ? "+" : "")
                                             .arg(QString::number(total_pct, 'f', 2)));
        holdings_pnl_pct_label_->setStyleSheet(
            QString("color:%1;font-size:13px;font-weight:700;")
                .arg(total_pct >= 0 ? fincept::ui::colors::POSITIVE() : fincept::ui::colors::NEGATIVE()));
    }

    holdings_table_->setSortingEnabled(was_sorting);
}

void EquityBottomPanel::set_orders(const QVector<trading::BrokerOrderInfo>& orders) {
    orders_table_->setRowCount(orders.size());
    for (int i = 0; i < orders.size(); ++i) {
        const auto& o = orders[i];
        ensure_item(orders_table_, i, 0)->setText(o.order_id.left(12));
        ensure_item(orders_table_, i, 1)->setText(o.symbol);
        ensure_item(orders_table_, i, 2)->setText(o.product_type.isEmpty() ? "--" : o.product_type.toUpper());
        ensure_item(orders_table_, i, 3)->setText(o.side.toUpper());
        ensure_item(orders_table_, i, 4)->setText(o.order_type.toUpper());
        ensure_item(orders_table_, i, 5)->setText(QString::number(o.quantity, 'f', 0));
        ensure_item(orders_table_, i, 6)->setText(QString::number(o.price, 'f', 2));
        auto* status_item = ensure_item(orders_table_, i, 7);
        status_item->setText(o.status.toUpper());
        status_item->setForeground(order_status_color(o.status));
        ensure_item(orders_table_, i, 8)->setText(o.timestamp);

        // Action column — MODIFY button for open/pending orders
        const bool modifiable = (o.status == "new" || o.status == "partially_filled" || o.status == "accepted" ||
                                 o.status == "pending_new");
        if (modifiable) {
            auto* btn = new QPushButton(tr("EDIT"));
            btn->setObjectName("eqTableBtn");
            btn->setFixedHeight(18);
            btn->setStyleSheet(QString("QPushButton#eqTableBtn { background: rgba(217,119,6,0.15); "
                                       "color: %1; border: 1px solid %2; font-size: 10px; "
                                       "padding: 0 6px; border-radius: 2px; }")
                                   .arg(fincept::ui::colors::AMBER())
                                   .arg(fincept::ui::colors::AMBER_DIM()));
            btn->setCursor(Qt::PointingHandCursor);
            const QString oid = o.order_id;
            const double qty = o.quantity;
            const double prc = o.price;
            connect(btn, &QPushButton::clicked, this, [this, oid, qty, prc]() {
                // Show inline edit dialog
                auto* dlg = new QDialog(this);
                dlg->setWindowTitle(tr("Modify Order"));
                dlg->setFixedWidth(280);
                dlg->setStyleSheet(QString("QDialog { background: %1; color: %2; }"
                                           "QLabel { color: %3; font-size: 11px; }"
                                           "QLineEdit { background: %4; border: 1px solid %5; "
                                           "  color: %2; padding: 5px; border-radius: 2px; }"
                                           "QLineEdit:focus { border-color: %6; }"
                                           "QPushButton { padding: 6px 14px; font-weight: 700; border-radius: 2px; }")
                                       .arg(fincept::ui::colors::BG_SURFACE())
                                       .arg(fincept::ui::colors::TEXT_PRIMARY())
                                       .arg(fincept::ui::colors::TEXT_SECONDARY())
                                       .arg(fincept::ui::colors::BG_BASE())
                                       .arg(fincept::ui::colors::BORDER_MED())
                                       .arg(fincept::ui::colors::AMBER()));
                auto* vlay = new QVBoxLayout(dlg);
                vlay->setSpacing(6);
                vlay->setContentsMargins(14, 14, 14, 14);

                auto* qty_lbl = new QLabel(tr("QTY"));
                auto* qty_edit = new QLineEdit(QString::number(qty, 'f', 0));
                auto* prc_lbl = new QLabel(tr("LIMIT PRICE"));
                auto* prc_edit = new QLineEdit(QString::number(prc, 'f', 2));

                auto* btn_row = new QHBoxLayout;
                auto* ok_btn = new QPushButton(tr("MODIFY"));
                ok_btn->setStyleSheet(QString("background: rgba(217,119,6,0.15); color: %1; border: 1px solid %2;")
                                          .arg(fincept::ui::colors::AMBER())
                                          .arg(fincept::ui::colors::AMBER_DIM()));
                auto* cancel_btn = new QPushButton(tr("CANCEL"));
                cancel_btn->setStyleSheet(QString("background: rgba(220,38,38,0.1); color: %1; border: 1px solid %2;")
                                              .arg(fincept::ui::colors::NEGATIVE())
                                              .arg(fincept::ui::colors::NEGATIVE_DIM()));

                connect(ok_btn, &QPushButton::clicked, dlg, [dlg, this, oid, qty_edit, prc_edit]() {
                    const double new_qty = qty_edit->text().toDouble();
                    const double new_prc = prc_edit->text().toDouble();
                    if (new_qty > 0)
                        emit modify_order_requested(oid, new_qty, new_prc);
                    dlg->accept();
                });
                connect(cancel_btn, &QPushButton::clicked, dlg, &QDialog::reject);

                vlay->addWidget(qty_lbl);
                vlay->addWidget(qty_edit);
                vlay->addWidget(prc_lbl);
                vlay->addWidget(prc_edit);
                btn_row->addWidget(ok_btn);
                btn_row->addWidget(cancel_btn);
                vlay->addLayout(btn_row);
                dlg->exec();
                dlg->deleteLater();
            });
            orders_table_->setCellWidget(i, 9, btn);
        } else {
            orders_table_->setCellWidget(i, 9, nullptr);
        }
    }
}

// ── Auctions Tab ────────────────────────────────────────────────────────────

void EquityBottomPanel::setup_auctions_tab() {
    auctions_table_ = new QTableWidget;
    auctions_table_->setObjectName("eqTable");
    auctions_table_->setColumnCount(6);
    auctions_table_->setHorizontalHeaderLabels(
        {tr("Date"), tr("Type"), tr("Time"), tr("Price"), tr("Size"), tr("Exchange")});
    auctions_table_->verticalHeader()->setVisible(false);
    auctions_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    auctions_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auctions_table_->setShowGrid(false);
    auctions_table_->horizontalHeader()->setStretchLastSection(true);
    auctions_table_->verticalHeader()->setDefaultSectionSize(20);
    auctions_table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    auctions_tab_idx_ = tabs_->addTab(auctions_table_, tr("AUCTIONS"));
}

void EquityBottomPanel::set_auctions(const QVector<trading::BrokerAuction>& auctions) {
    auctions_table_->setUpdatesEnabled(false);
    auctions_table_->setRowCount(0);
    for (const auto& auction : auctions) {
        for (const auto& entry : auction.entries) {
            const int row = auctions_table_->rowCount();
            auctions_table_->insertRow(row);
            const bool is_open = (entry.auction_type == "O");
            const QColor type_color =
                is_open ? QColor(fincept::ui::colors::AMBER()) : QColor(fincept::ui::colors::INFO());

            auto set = [&](int col, const QString& text,
                           const QColor& fg = QColor(fincept::ui::colors::TEXT_PRIMARY())) {
                auto* item = new QTableWidgetItem(text);
                item->setForeground(fg);
                item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                auctions_table_->setItem(row, col, item);
            };

            const QString time_str =
                QDateTime::fromString(entry.timestamp, Qt::ISODateWithMs).toLocalTime().toString("hh:mm:ss");
            set(0, auction.date, QColor(fincept::ui::colors::TEXT_SECONDARY()));
            set(1, is_open ? tr("OPEN") : tr("CLOSE"), type_color);
            set(2, time_str, QColor(fincept::ui::colors::TEXT_SECONDARY()));
            set(3, QString::number(entry.price, 'f', 2));
            set(4, QString::number(entry.size, 'f', 0), QColor(fincept::ui::colors::TEXT_SECONDARY()));
            set(5, entry.exchange, QColor(fincept::ui::colors::TEXT_SECONDARY()));
        }
    }
    auctions_table_->setUpdatesEnabled(true);
}

void EquityBottomPanel::set_condition_codes(const QMap<QString, QString>& codes) {
    condition_codes_ = codes;
}

// ── Time & Sales Tab ────────────────────────────────────────────────────────

void EquityBottomPanel::setup_time_sales_tab() {
    time_sales_table_ = new QTableWidget;
    time_sales_table_->setObjectName("eqTable");
    time_sales_table_->setColumnCount(6);
    time_sales_table_->setHorizontalHeaderLabels(
        {tr("Time"), tr("Price"), tr("Size"), tr("Exchange"), tr("Conditions"), tr("Tape")});
    time_sales_table_->verticalHeader()->setVisible(false);
    time_sales_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    time_sales_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    time_sales_table_->setShowGrid(false);
    time_sales_table_->horizontalHeader()->setStretchLastSection(true);
    time_sales_table_->verticalHeader()->setDefaultSectionSize(20);
    time_sales_table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    time_sales_table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    time_sales_tab_idx_ = tabs_->addTab(time_sales_table_, tr("TIME & SALES"));
}

static void fill_trade_row(QTableWidget* table, int row, const trading::BrokerTrade& t,
                           const QMap<QString, QString>& codes = {}) {
    // Parse ISO timestamp → time only
    const QString time_str =
        QDateTime::fromString(t.timestamp, Qt::ISODateWithMs).toLocalTime().toString("hh:mm:ss.zzz");

    auto set = [&](int col, const QString& text, const QColor& fg = QColor(fincept::ui::colors::TEXT_PRIMARY())) {
        auto* item = new QTableWidgetItem(text);
        item->setForeground(fg);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        table->setItem(row, col, item);
    };

    set(0, time_str.isEmpty() ? t.timestamp.right(12) : time_str, QColor(fincept::ui::colors::TEXT_SECONDARY()));
    set(1, QString::number(t.price, 'f', 2),
        t.price > 0 ? QColor(fincept::ui::colors::TEXT_PRIMARY()) : QColor(fincept::ui::colors::TEXT_SECONDARY()));
    set(2, QString::number(t.size, 'f', 0), QColor(fincept::ui::colors::TEXT_SECONDARY()));
    set(3, t.exchange, QColor(fincept::ui::colors::TEXT_SECONDARY()));
    QStringList decoded;
    for (const auto& c : t.conditions)
        decoded.append(codes.contains(c) ? codes[c] : c);
    set(4, decoded.join(", "), QColor(fincept::ui::colors::TEXT_TERTIARY()));
    set(5, t.tape, QColor(fincept::ui::colors::TEXT_TERTIARY()));
}

void EquityBottomPanel::set_time_sales(const QVector<trading::BrokerTrade>& trades) {
    time_sales_table_->setUpdatesEnabled(false);
    time_sales_table_->setRowCount(trades.size());
    for (int i = 0; i < trades.size(); ++i)
        fill_trade_row(time_sales_table_, i, trades[i], condition_codes_);
    time_sales_table_->setUpdatesEnabled(true);
    // Scroll to most recent (bottom — trades arrive oldest-first)
    if (!trades.isEmpty())
        time_sales_table_->scrollToBottom();
}

void EquityBottomPanel::prepend_trade(const trading::BrokerTrade& trade) {
    // Insert at row 0 (newest at top), cap at 500 rows to avoid unbounded growth
    time_sales_table_->insertRow(0);
    fill_trade_row(time_sales_table_, 0, trade, condition_codes_);
    if (time_sales_table_->rowCount() > 500)
        time_sales_table_->removeRow(500);
}

// ── Calendar Tab ───────────────────────────────────────────────────────────

void EquityBottomPanel::setup_calendar_tab() {
    auto* container = new QWidget(this);
    auto* vlay = new QVBoxLayout(container);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);

    // Clock status banner
    auto* banner = new QWidget(this);
    banner->setObjectName("calClockBanner");
    banner->setFixedHeight(28);
    banner->setStyleSheet(QString("QWidget#calClockBanner { background: %1; border-bottom: 1px solid %2; }")
                              .arg(fincept::ui::colors::BG_BASE())
                              .arg(fincept::ui::colors::BORDER_DIM()));
    auto* banner_lay = new QHBoxLayout(banner);
    banner_lay->setContentsMargins(10, 0, 10, 0);
    banner_lay->setSpacing(16);

    clock_status_label_ = new QLabel(tr("● MARKET --"));
    clock_status_label_->setObjectName("calClockStatus");
    clock_status_label_->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: 700;").arg(fincept::ui::colors::TEXT_TERTIARY()));

    clock_next_label_ = new QLabel("");
    clock_next_label_->setObjectName("calClockNext");
    clock_next_label_->setStyleSheet(QString("color: %1; font-size: 10px;").arg(fincept::ui::colors::TEXT_TERTIARY()));

    banner_lay->addWidget(clock_status_label_);
    banner_lay->addWidget(clock_next_label_);
    banner_lay->addStretch();
    vlay->addWidget(banner);

    // Calendar table
    calendar_table_ = new QTableWidget;
    calendar_table_->setObjectName("eqTable");
    calendar_table_->setColumnCount(5);
    calendar_table_->setHorizontalHeaderLabels(
        {tr("Date"), tr("Open (ET)"), tr("Close (ET)"), tr("Pre-Market"), tr("After-Hours")});
    calendar_table_->verticalHeader()->setVisible(false);
    calendar_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    calendar_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    calendar_table_->setShowGrid(false);
    calendar_table_->horizontalHeader()->setStretchLastSection(true);
    calendar_table_->verticalHeader()->setDefaultSectionSize(22);
    // Pixel-level smooth scrolling
    calendar_table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    calendar_table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    vlay->addWidget(calendar_table_);

    calendar_tab_idx_ = tabs_->addTab(container, tr("CALENDAR"));
}

void EquityBottomPanel::set_calendar(const QVector<trading::MarketCalendarDay>& days) {
    calendar_table_->setRowCount(days.size());
    const QString today = QDate::currentDate().toString("yyyy-MM-dd");
    for (int i = 0; i < days.size(); ++i) {
        const auto& d = days[i];
        const bool is_today = (d.date == today);
        const QColor row_color = is_today ? QColor(217, 119, 6, 40) : QColor(0, 0, 0, 0);

        auto set = [&](int col, const QString& text) {
            auto* item = ensure_item(calendar_table_, i, col);
            item->setText(text);
            if (is_today) {
                item->setForeground(QColor(fincept::ui::colors::AMBER()));
                item->setBackground(row_color);
            }
        };
        set(0, d.date);
        set(1, d.open);
        set(2, d.close);
        set(3, d.session_open.isEmpty() ? "--" : d.session_open);
        set(4, d.session_close.isEmpty() ? "--" : d.session_close);
    }
    // Scroll to today (or nearest future day) centred in the view
    for (int i = 0; i < days.size(); ++i) {
        if (days[i].date >= today) {
            calendar_table_->scrollToItem(calendar_table_->item(i, 0), QAbstractItemView::PositionAtCenter);
            break;
        }
    }
}

void EquityBottomPanel::set_clock(const trading::MarketClock& clock) {
    if (!clock_status_label_)
        return;

    if (clock.is_open) {
        clock_status_label_->setText(tr("● MARKET OPEN"));
        clock_status_label_->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: 700;").arg(fincept::ui::colors::POSITIVE()));
    } else {
        clock_status_label_->setText(tr("● MARKET CLOSED"));
        clock_status_label_->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: 700;").arg(fincept::ui::colors::NEGATIVE()));
    }

    // Parse ISO timestamps and show local-friendly next event
    if (!clock.next_open.isEmpty() || !clock.next_close.isEmpty()) {
        const QString next_event = clock.is_open ? tr("Closes %1")
                                                       .arg(QDateTime::fromString(clock.next_close, Qt::ISODateWithMs)
                                                                .toLocalTime()
                                                                .toString("MMM d h:mm ap"))
                                                 : tr("Opens %1")
                                                       .arg(QDateTime::fromString(clock.next_open, Qt::ISODateWithMs)
                                                                .toLocalTime()
                                                                .toString("MMM d h:mm ap"));
        clock_next_label_->setText(next_event);
        clock_next_label_->setStyleSheet(
            QString("color: %1; font-size: 10px;").arg(fincept::ui::colors::TEXT_SECONDARY()));
    }
}

} // namespace fincept::screens::equity
