// EquityBottomPanel.cpp — tabbed portfolio display
#include "screens/equity_trading/EquityBottomPanel.h"

#include "ui/theme/Theme.h"

#include <QDate>
#include <QDateTime>
#include <QDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace fincept::screens::equity {

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
        positions_table_->setHorizontalHeaderLabels(
            {tr("Symbol"), tr("Opened"), tr("Side"), tr("Qty"), tr("Avg Price"), tr("LTP"), tr("P&L"), tr("P&L %")});
    if (holdings_table_)
        holdings_table_->setHorizontalHeaderLabels(
            {tr("Symbol"), tr("Qty"), tr("Avg Price"), tr("LTP"), tr("Invested"), tr("Current"), tr("P&L"),
             tr("P&L %")});
    if (orders_table_)
        orders_table_->setHorizontalHeaderLabels(
            {tr("Order ID"), tr("Symbol"), tr("Side"), tr("Type"), tr("Qty"), tr("Price"), tr("Status"), tr("Time"),
             tr("Action")});
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

    // Holdings summary-strip captions
    if (holdings_count_caption_)    holdings_count_caption_->setText(tr("HOLDINGS"));
    if (holdings_invested_caption_) holdings_invested_caption_->setText(tr("INVESTED"));
    if (holdings_current_caption_)  holdings_current_caption_->setText(tr("CURRENT"));
    if (holdings_pnl_caption_)      holdings_pnl_caption_->setText(tr("TOTAL P&L"));
    if (holdings_pnl_pct_caption_)  holdings_pnl_pct_caption_->setText(tr("RETURN %"));

    // Funds row captions
    if (available_caption_)   available_caption_->setText(tr("Available Balance"));
    if (used_margin_caption_) used_margin_caption_->setText(tr("Used Margin"));
    if (total_caption_)       total_caption_->setText(tr("Total Balance"));
    if (collateral_caption_)  collateral_caption_->setText(tr("Collateral"));

    // Stats row captions
    const QString stat_labels[] = {tr("Total P&L"), tr("Win Rate"), tr("Total Trades"), tr("Largest Win"),
                                   tr("Largest Loss")};
    for (int i = 0; i < 5; ++i)
        if (stat_captions_[i]) stat_captions_[i]->setText(stat_labels[i]);

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
    bar_layout->setSpacing(8);
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
    positions_table_->setColumnCount(8);
    positions_table_->setHorizontalHeaderLabels(
        {tr("Symbol"), tr("Opened"), tr("Side"), tr("Qty"), tr("Avg Price"), tr("LTP"), tr("P&L"), tr("P&L %")});
    positions_table_->verticalHeader()->setVisible(false);
    positions_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    positions_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    positions_table_->setShowGrid(false);
    positions_table_->horizontalHeader()->setStretchLastSection(true);
    positions_table_->verticalHeader()->setDefaultSectionSize(22);
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

    v->addWidget(strip);

    holdings_table_ = new QTableWidget;
    holdings_table_->setObjectName("eqTable");
    holdings_table_->setColumnCount(8);
    holdings_table_->setHorizontalHeaderLabels(
        {tr("Symbol"), tr("Qty"), tr("Avg Price"), tr("LTP"), tr("Invested"), tr("Current"), tr("P&L"), tr("P&L %")});
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
    orders_table_->setColumnCount(9);
    orders_table_->setHorizontalHeaderLabels(
        {tr("Order ID"), tr("Symbol"), tr("Side"), tr("Type"), tr("Qty"), tr("Price"), tr("Status"), tr("Time"),
         tr("Action")});
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

void EquityBottomPanel::setup_funds_tab() {
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto make_row = [&](const QString& label, QLabel*& caption_out) -> QLabel* {
        auto* row = new QHBoxLayout;
        auto* lbl = new QLabel(label);
        lbl->setObjectName("eqOeLabel");
        caption_out = lbl;
        auto* val = new QLabel("--");
        val->setObjectName("eqOeBalance");
        val->setAlignment(Qt::AlignRight);
        row->addWidget(lbl);
        row->addStretch();
        row->addWidget(val);
        layout->addLayout(row);
        return val;
    };

    available_label_ = make_row(tr("Available Balance"), available_caption_);
    used_margin_label_ = make_row(tr("Used Margin"), used_margin_caption_);
    total_label_ = make_row(tr("Total Balance"), total_caption_);
    collateral_label_ = make_row(tr("Collateral"), collateral_caption_);

    layout->addStretch();
    funds_tab_idx_ = tabs_->addTab(widget, tr("FUNDS"));
}

// ── Stats Tab ──────────────────────────────────────────────────────────────

void EquityBottomPanel::setup_stats_tab() {
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    const QString stat_labels[] = {tr("Total P&L"), tr("Win Rate"), tr("Total Trades"), tr("Largest Win"),
                                   tr("Largest Loss")};
    for (int i = 0; i < 5; ++i) {
        auto* row = new QHBoxLayout;
        auto* lbl = new QLabel(stat_labels[i]);
        lbl->setObjectName("eqOeLabel");
        stat_captions_[i] = lbl;
        stat_values_[i] = new QLabel("--");
        stat_values_[i]->setObjectName("eqOeBalance");
        stat_values_[i]->setAlignment(Qt::AlignRight);
        row->addWidget(lbl);
        row->addStretch();
        row->addWidget(stat_values_[i]);
        layout->addLayout(row);
    }

    layout->addStretch();
    stats_tab_idx_ = tabs_->addTab(widget, tr("STATS"));
}

// ── Data Setters ───────────────────────────────────────────────────────────

void EquityBottomPanel::set_mode(bool is_paper) {
    is_paper_ = is_paper;
}

void EquityBottomPanel::set_account_id(const QString& account_id) {
    account_id_ = account_id;
}

void EquityBottomPanel::set_paper_positions(const QVector<trading::PtPosition>& positions) {
    last_paper_positions_ = positions; // keep row-aligned for live-quote patching
    positions_table_->setRowCount(positions.size());
    for (int i = 0; i < positions.size(); ++i) {
        const auto& p = positions[i];
        ensure_item(positions_table_, i, 0)->setText(p.symbol);

        // Col 1 — "Opened": parse ISO timestamp, convert to local time, fall back to raw string
        {
            const QDateTime dt = QDateTime::fromString(p.opened_at, Qt::ISODate).toLocalTime();
            ensure_item(positions_table_, i, 1)
                ->setText(dt.isValid() ? dt.toString("yyyy-MM-dd HH:mm") : p.opened_at);
        }

        ensure_item(positions_table_, i, 2)->setText(p.side);
        ensure_item(positions_table_, i, 3)->setText(QString::number(p.quantity, 'f', 0));
        ensure_item(positions_table_, i, 4)->setText(QString::number(p.entry_price, 'f', 2));
        ensure_item(positions_table_, i, 5)->setText(QString::number(p.current_price, 'f', 2));

        // Col 6 — P&L
        auto* pnl_item = ensure_item(positions_table_, i, 6);
        pnl_item->setText(QString::number(p.unrealized_pnl, 'f', 2));
        pnl_item->setForeground(p.unrealized_pnl >= 0 ? QColor(fincept::ui::colors::POSITIVE())
                                                      : QColor(fincept::ui::colors::NEGATIVE()));

        // Col 7 — P&L %
        // Use notional as the divisor so both entry_price == 0 and quantity == 0
        // are safely handled by a single guard (avoids divide-by-zero).
        const double notional = p.entry_price * p.quantity;
        const double pct = notional != 0.0
            ? (p.unrealized_pnl / notional) * 100.0
            : 0.0;
        auto* pct_item = ensure_item(positions_table_, i, 7);
        pct_item->setText(QString::number(pct, 'f', 2) + "%");
        pct_item->setForeground(
            pct >= 0 ? QColor(fincept::ui::colors::POSITIVE())
                     : QColor(fincept::ui::colors::NEGATIVE()));
    }
}

void EquityBottomPanel::set_paper_orders(const QVector<trading::PtOrder>& orders) {
    orders_table_->setRowCount(orders.size());
    for (int i = 0; i < orders.size(); ++i) {
        const auto& o = orders[i];
        ensure_item(orders_table_, i, 0)->setText(o.id.left(8));
        ensure_item(orders_table_, i, 1)->setText(o.symbol);
        ensure_item(orders_table_, i, 2)->setText(o.side.toUpper());
        ensure_item(orders_table_, i, 3)->setText(o.order_type.toUpper());
        ensure_item(orders_table_, i, 4)->setText(QString::number(o.quantity, 'f', 0));
        ensure_item(orders_table_, i, 5)->setText(o.price ? QString::number(*o.price, 'f', 2) : "MKT");
        ensure_item(orders_table_, i, 6)->setText(o.status.toUpper());
        ensure_item(orders_table_, i, 7)->setText(o.created_at);
        ensure_item(orders_table_, i, 8)->setText("");
    }
}

void EquityBottomPanel::set_paper_trades(const QVector<trading::PtTrade>& trades) {
    // Append trade rows below orders in the orders table (separated by a visual break)
    // We use a dedicated approach: show last 20 trades after orders
    const int base = orders_table_->rowCount();
    if (trades.isEmpty())
        return;

    // Add a separator row
    orders_table_->setRowCount(base + 1 + trades.size());
    auto* sep_item = ensure_item(orders_table_, base, 0);
    sep_item->setText(tr("--- RECENT TRADES ---"));
    sep_item->setForeground(QColor(fincept::ui::colors::AMBER()));
    for (int c = 1; c < 8; ++c)
        ensure_item(orders_table_, base, c)->setText("");

    for (int i = 0; i < trades.size(); ++i) {
        const auto& t = trades[i];
        const int row = base + 1 + i;
        ensure_item(orders_table_, row, 0)->setText(t.id.left(8));
        ensure_item(orders_table_, row, 1)->setText(t.symbol);
        ensure_item(orders_table_, row, 2)->setText(t.side.toUpper());
        ensure_item(orders_table_, row, 3)->setText("TRADE");
        ensure_item(orders_table_, row, 4)->setText(QString::number(t.quantity, 'f', 0));
        ensure_item(orders_table_, row, 5)->setText(QString::number(t.price, 'f', 2));

        auto* pnl_item = ensure_item(orders_table_, row, 6);
        pnl_item->setText(QString::number(t.pnl, 'f', 2));
        pnl_item->setForeground(t.pnl >= 0 ? QColor(fincept::ui::colors::POSITIVE())
                                           : QColor(fincept::ui::colors::NEGATIVE()));

        ensure_item(orders_table_, row, 7)->setText(t.timestamp);
    }
}

void EquityBottomPanel::set_paper_stats(const trading::PtStats& stats) {
    stat_values_[0]->setText(QString::number(stats.total_pnl, 'f', 2));
    stat_values_[0]->setStyleSheet(
        QString("color: %1;")
            .arg(stats.total_pnl >= 0 ? fincept::ui::colors::POSITIVE() : fincept::ui::colors::NEGATIVE()));
    stat_values_[1]->setText(QString("%1%").arg(stats.win_rate * 100, 0, 'f', 1));
    stat_values_[2]->setText(QString::number(stats.total_trades));
    stat_values_[3]->setText(QString::number(stats.largest_win, 'f', 2));
    stat_values_[3]->setStyleSheet(QString("color: %1;").arg(fincept::ui::colors::POSITIVE()));
    stat_values_[4]->setText(QString::number(stats.largest_loss, 'f', 2));
    stat_values_[4]->setStyleSheet(QString("color: %1;").arg(fincept::ui::colors::NEGATIVE()));
}

void EquityBottomPanel::set_positions(const QVector<trading::BrokerPosition>& positions) {
    last_positions_ = positions; // keep row-aligned for live-quote patching
    positions_table_->setRowCount(positions.size());
    for (int i = 0; i < positions.size(); ++i) {
        const auto& p = positions[i];
        ensure_item(positions_table_, i, 0)->setText(p.symbol);
        ensure_item(positions_table_, i, 1)->setText(p.exchange.isEmpty() ? "--" : p.exchange);
        ensure_item(positions_table_, i, 2)->setText(p.side.toUpper());
        ensure_item(positions_table_, i, 3)->setText(QString::number(p.quantity, 'f', 0));
        ensure_item(positions_table_, i, 4)->setText(QString::number(p.avg_price, 'f', 2));
        ensure_item(positions_table_, i, 5)->setText(QString::number(p.ltp, 'f', 2));

        auto* pnl_item = ensure_item(positions_table_, i, 6);
        pnl_item->setText(QString::number(p.pnl, 'f', 2));
        pnl_item->setForeground(p.pnl >= 0 ? QColor(fincept::ui::colors::POSITIVE())
                                           : QColor(fincept::ui::colors::NEGATIVE()));

        auto* pct_item = ensure_item(positions_table_, i, 7);
        pct_item->setText(QString("%1%").arg(p.pnl_pct, 0, 'f', 2));
        pct_item->setForeground(p.pnl_pct >= 0 ? QColor(fincept::ui::colors::POSITIVE())
                                               : QColor(fincept::ui::colors::NEGATIVE()));
    }
}

void EquityBottomPanel::update_quote(const QString& symbol, const trading::BrokerQuote& quote) {
    const double ltp = quote.ltp;
    if (ltp <= 0.0 || symbol.isEmpty())
        return; // ignore empty / zero-price ticks — keep the last good values

    const QColor pos_color(fincept::ui::colors::POSITIVE());
    const QColor neg_color(fincept::ui::colors::NEGATIVE());

    auto paint = [&](int row, double pnl, double pnl_pct) {
        ensure_item(positions_table_, row, 5)->setText(QString::number(ltp, 'f', 2));
        auto* pnl_item = ensure_item(positions_table_, row, 6);
        pnl_item->setText(QString::number(pnl, 'f', 2));
        pnl_item->setForeground(pnl >= 0 ? pos_color : neg_color);
        auto* pct_item = ensure_item(positions_table_, row, 7);
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
}

void EquityBottomPanel::set_holdings(const QVector<trading::BrokerHolding>& holdings) {
    last_holdings_ = holdings;
    if (holdings_import_btn_)
        holdings_import_btn_->setEnabled(!holdings.isEmpty());

    // Disable sorting during population so setItem assignments stay at intended rows.
    const bool was_sorting = holdings_table_->isSortingEnabled();
    holdings_table_->setSortingEnabled(false);
    holdings_table_->setRowCount(holdings.size());

    double total_invested = 0.0;
    double total_current = 0.0;
    double total_pnl = 0.0;

    const QColor pos_color(fincept::ui::colors::POSITIVE());
    const QColor neg_color(fincept::ui::colors::NEGATIVE());

    auto set_num = [](QTableWidgetItem* it, double v, int precision) {
        it->setData(Qt::DisplayRole, QString::number(v, 'f', precision));
        it->setData(Qt::EditRole, v);
    };

    for (int i = 0; i < holdings.size(); ++i) {
        const auto& h = holdings[i];
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
        ensure_item(orders_table_, i, 2)->setText(o.side.toUpper());
        ensure_item(orders_table_, i, 3)->setText(o.order_type.toUpper());
        ensure_item(orders_table_, i, 4)->setText(QString::number(o.quantity, 'f', 0));
        ensure_item(orders_table_, i, 5)->setText(QString::number(o.price, 'f', 2));
        ensure_item(orders_table_, i, 6)->setText(o.status.toUpper());
        ensure_item(orders_table_, i, 7)->setText(o.timestamp);

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
            orders_table_->setCellWidget(i, 8, btn);
        } else {
            orders_table_->setCellWidget(i, 8, nullptr);
        }
    }
}

void EquityBottomPanel::set_funds(const trading::BrokerFunds& funds) {
    available_label_->setText(QString::number(funds.available_balance, 'f', 2));
    used_margin_label_->setText(QString::number(funds.used_margin, 'f', 2));
    total_label_->setText(QString::number(funds.total_balance, 'f', 2));
    collateral_label_->setText(QString::number(funds.collateral, 'f', 2));
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
