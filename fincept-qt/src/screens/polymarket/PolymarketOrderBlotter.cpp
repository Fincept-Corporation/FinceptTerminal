#include "screens/polymarket/PolymarketOrderBlotter.h"

#include "core/agent/AgentRiskMarkers.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include <cmath>

namespace pm_fmt = fincept::ui::formatting;

namespace fincept::screens::polymarket {

using namespace fincept::ui;
using namespace fincept::services::prediction;

namespace {

// Column indices — kept as an enum so the row-build and cell-update paths
// stay in lockstep when we add a column.
enum Column : int {
    ColTime = 0,
    ColMarket,
    ColSide,
    ColOutcome,
    ColType,
    ColPrice,
    ColSize,
    ColFilled,
    ColStatus,
    ColActions,
    ColCount,
};

QString col_label(int c) {
    // Anonymous-namespace helper — translate via QCoreApplication under the
    // owning widget's context so column captions localize.
    auto t = [](const char* s) { return QCoreApplication::translate("PolymarketOrderBlotter", s); };
    switch (c) {
        case ColTime:
            return t("TIME");
        case ColMarket:
            return t("MARKET");
        case ColSide:
            return t("SIDE");
        case ColOutcome:
            return t("OUTCOME");
        case ColType:
            return t("TYPE");
        case ColPrice:
            return t("PRICE");
        case ColSize:
            return t("SIZE");
        case ColFilled:
            return t("FILLED");
        case ColStatus:
            return t("STATUS");
        case ColActions:
            return t("ACTIONS");
    }
    return {};
}

// ── Account history ─────────────────────────────────────────────────────────
// List keys — the same strings KalshiAdapter::history_*() return.
const QString kSettlements = QStringLiteral("settlements");
const QString kFills = QStringLiteral("fills");
const QString kOrders = QStringLiteral("orders");

QString blotter_tr(const char* s) {
    return QCoreApplication::translate("PolymarketOrderBlotter", s);
}

QString history_kind_label(const QString& kind) {
    if (kind == kSettlements)
        return blotter_tr("SETTLEMENTS");
    if (kind == kFills)
        return blotter_tr("FILLS");
    return blotter_tr("ORDERS");
}

QString history_kind_tooltip(const QString& kind) {
    if (kind == kSettlements)
        return blotter_tr("Markets you held a position in when they settled: payout, cost and fees");
    if (kind == kFills)
        return blotter_tr("Fills from Kalshi's historical archive (older than the live fills list)");
    return blotter_tr("Orders from Kalshi's historical archive (finished orders past the live window)");
}

QString history_empty_text(const QString& kind) {
    if (kind == kSettlements)
        return blotter_tr("No settlements on this account");
    if (kind == kFills)
        return blotter_tr("No archived fills on this account");
    return blotter_tr("No archived orders on this account");
}

QStringList history_headers(const QString& kind) {
    if (kind == kSettlements)
        return {blotter_tr("SETTLED"), blotter_tr("MARKET"), blotter_tr("RESULT"), blotter_tr("YES"),
                blotter_tr("NO"),      blotter_tr("COST"),   blotter_tr("PAYOUT"), blotter_tr("FEES"),
                blotter_tr("NET")};
    if (kind == kFills)
        return {blotter_tr("TIME"),  blotter_tr("MARKET"), blotter_tr("SIDE"), blotter_tr("OUTCOME"),
                blotter_tr("PRICE"), blotter_tr("SIZE"),   blotter_tr("FEES"), blotter_tr("ROLE")};
    return {blotter_tr("CREATED"), blotter_tr("MARKET"), blotter_tr("SIDE"),   blotter_tr("OUTCOME"),
            blotter_tr("TYPE"),    blotter_tr("PRICE"),  blotter_tr("SIZE"),   blotter_tr("FILLED"),
            blotter_tr("STATUS"),  blotter_tr("FEES")};
}
constexpr int kHistoryMarketCol = 1; // MARKET stretches in every list

QString history_time(qint64 ms) {
    return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                  : QStringLiteral("—");
}

/// An amount of the account's money in the venue's settlement currency (`ccy`).
QString money_text(double v, const QString& ccy) {
    return pm_fmt::money(v, ccy);
}

QString count_text(double c) {
    return std::abs(c - std::round(c)) < 1e-9 ? QString::number(qint64(std::llround(c))) : QString::number(c, 'f', 2);
}

// Sub-cent prices keep their digits; rounding would show a price that never traded.
QString history_price_text(double p, int decimals) {
    if (std::abs(p * 100.0 - std::round(p * 100.0)) > 1e-6)
        decimals = qMax(decimals, 4);
    return QString::number(p, 'f', decimals);
}

QTableWidgetItem* right_item(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
}

} // namespace

PolymarketOrderBlotter::PolymarketOrderBlotter(QWidget* parent) : QWidget(parent) {
    setObjectName("polyOrderBlotter");
    history_kind_ = kSettlements;
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    header_bar_ = new QWidget(this);
    auto* header_bar = header_bar_;
    header_bar->setObjectName("polyBlotterHeader");
    header_bar->setFixedHeight(30);
    auto* hhl = new QHBoxLayout(header_bar);
    hhl->setContentsMargins(12, 0, 8, 0);
    hhl->setSpacing(8);

    title_label_ = new QLabel(tr("OPEN ORDERS"), header_bar);
    title_label_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; background: transparent; "
                                        "letter-spacing: 0.8px;")
                                    .arg(colors::TEXT_SECONDARY()));
    hhl->addWidget(title_label_);

    // OPEN ORDERS / ACCOUNT HISTORY tabs — replace the plain title where the
    // adapter has account history (set_history_supported).
    orders_tab_btn_ = new QPushButton(tr("OPEN ORDERS"), header_bar);
    history_tab_btn_ = new QPushButton(tr("ACCOUNT HISTORY"), header_bar);
    for (auto* btn : {orders_tab_btn_, history_tab_btn_}) {
        btn->setObjectName(QStringLiteral("blotterModeTab"));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setVisible(false);
        hhl->addWidget(btn);
    }
    orders_tab_btn_->setAccessibleName(tr("Open orders"));
    history_tab_btn_->setAccessibleName(tr("Account history"));
    history_tab_btn_->setToolTip(tr("Settlements, archived fills and archived orders of the connected account"));
    connect(orders_tab_btn_, &QPushButton::clicked, this, [this]() { show_history(false); });
    connect(history_tab_btn_, &QPushButton::clicked, this, [this]() { show_history(true); });

    history_kinds_ = new QWidget(header_bar);
    auto* khl = new QHBoxLayout(history_kinds_);
    khl->setContentsMargins(8, 0, 0, 0);
    khl->setSpacing(4);
    for (const QString& kind : {kSettlements, kFills, kOrders}) {
        auto* pill = new QPushButton(history_kind_label(kind), history_kinds_);
        pill->setObjectName(QStringLiteral("blotterKindPill"));
        pill->setProperty("kind", kind);
        pill->setCursor(Qt::PointingHandCursor);
        pill->setToolTip(history_kind_tooltip(kind));
        pill->setAccessibleName(history_kind_label(kind));
        connect(pill, &QPushButton::clicked, this, [this, kind]() { select_history_kind(kind); });
        khl->addWidget(pill);
        history_kind_btns_.append(pill);
    }
    history_kinds_->setVisible(false);
    hhl->addWidget(history_kinds_);
    hhl->addStretch(1);

    history_count_lbl_ = new QLabel(header_bar);
    history_count_lbl_->setObjectName(QStringLiteral("blotterHistoryCount"));
    history_count_lbl_->setVisible(false);
    hhl->addWidget(history_count_lbl_);

    load_more_btn_ = new QPushButton(tr("LOAD MORE"), header_bar);
    load_more_btn_->setObjectName(QStringLiteral("blotterLoadMore"));
    load_more_btn_->setCursor(Qt::PointingHandCursor);
    load_more_btn_->setAccessibleName(tr("Load the next page"));
    load_more_btn_->setVisible(false);
    connect(load_more_btn_, &QPushButton::clicked, this, [this]() {
        const auto it = history_.constFind(history_kind_);
        if (it != history_.constEnd() && !it->next_cursor.isEmpty())
            request_history(history_kind_, it->next_cursor);
    });
    hhl->addWidget(load_more_btn_);

    history_refresh_btn_ = new QPushButton(QStringLiteral("↻"), header_bar);
    history_refresh_btn_->setObjectName(QStringLiteral("blotterHistoryRefresh"));
    history_refresh_btn_->setCursor(Qt::PointingHandCursor);
    history_refresh_btn_->setMinimumSize(22, 20);
    history_refresh_btn_->setToolTip(tr("Reload from the first page"));
    history_refresh_btn_->setAccessibleName(tr("Reload account history"));
    history_refresh_btn_->setVisible(false);
    connect(history_refresh_btn_, &QPushButton::clicked, this, [this]() { request_history(history_kind_, QString()); });
    hhl->addWidget(history_refresh_btn_);

    cancel_all_btn_ = new QPushButton(tr("CANCEL ALL"), header_bar);
    auto* cancel_all_btn = cancel_all_btn_;
    cancel_all_btn->setObjectName("orderBlotterCancelAll");
    cancel_all_btn->setProperty("fincept_agent_risk", "order");
    cancel_all_btn->setCursor(Qt::PointingHandCursor);
    cancel_all_btn->setStyleSheet(
        QString("QPushButton { background: transparent; color: %1; "
                "  border: 1px solid %1; padding: 2px 8px; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; }"
                "QPushButton:hover { background: %3; }"
                "QPushButton:disabled { color: %2; border-color: %2; }")
            .arg(colors::DANGER(), colors::TEXT_TERTIARY(), colors::alpha(colors::DANGER, 0.15)));
    connect(cancel_all_btn, &QPushButton::clicked, this, &PolymarketOrderBlotter::on_cancel_all_clicked);
    hhl->addWidget(cancel_all_btn);
    vl->addWidget(header_bar);

    stack_ = new QStackedWidget(this);
    vl->addWidget(stack_, 1);

    table_ = new QTableWidget(stack_);
    table_->setColumnCount(ColCount);
    QStringList headers;
    for (int c = 0; c < ColCount; ++c)
        headers << col_label(c);
    table_->setHorizontalHeaderLabels(headers);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers); // double-click drives amend
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(ColMarket, QHeaderView::Stretch);
    table_->setShowGrid(false);
    table_->setAccessibleName(tr("Open orders"));
    connect(table_, &QTableWidget::cellDoubleClicked, this, &PolymarketOrderBlotter::on_cell_double_clicked);
    stack_->addWidget(table_); // 0

    // Account history: one table, its columns swapped per list (rebuild_history).
    history_table_ = new QTableWidget(stack_);
    history_table_->verticalHeader()->setVisible(false);
    history_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    history_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    history_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    history_table_->horizontalHeader()->setStretchLastSection(false);
    history_table_->setShowGrid(false);
    history_table_->setAccessibleName(tr("Account history"));
    stack_->addWidget(history_table_); // 1

    apply_table_style();
    apply_header_style();
    sync_header();

    // A number / currency format change re-renders the history amounts from the
    // pages already loaded (no re-fetch); the open-orders table prints no money.
    pm_fmt::on_format_changed(this, [this]() {
        if (history_shown_)
            rebuild_history();
    });
}

void PolymarketOrderBlotter::apply_header_style() {
    if (!header_bar_)
        return;
    // One sheet for the bar and everything on it (tabs, list pills, load-more,
    // reload, row count); the accent follows the exchange presentation. Set only
    // when the text changed (re-run on every presentation / appearance change).
    const QString accent = presentation_.accent.name();
    const QString sheet =
        QString("QWidget#polyBlotterHeader { background: %1; border-bottom: 1px solid %2; }"
                "QPushButton#blotterModeTab { background: transparent; color: %3; border: none;"
                "  border-bottom: 2px solid transparent; padding: 0 10px; min-height: 28px;"
                "  font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; letter-spacing: 0.8px; }"
                "QPushButton#blotterModeTab:hover { color: %4; }"
                "QPushButton#blotterModeTab[active=\"true\"] { color: %5; border-bottom-color: %5; }"
                "QPushButton#blotterKindPill { background: transparent; color: %6; border: 1px solid %2;"
                "  padding: 1px 8px; font-size: " + fincept::ui::fonts::px(-6) + "px; font-weight: 700; }"
                "QPushButton#blotterKindPill:hover { color: %4; border-color: %7; }"
                "QPushButton#blotterKindPill[active=\"true\"] { color: %5; border-color: %5; background: %8; }"
                "QPushButton#blotterLoadMore { background: transparent; color: %5; border: 1px solid %5;"
                "  padding: 2px 8px; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; }"
                "QPushButton#blotterLoadMore:hover { background: %8; }"
                "QPushButton#blotterHistoryRefresh { background: transparent; color: %3; border: 1px solid %2;"
                "  font-size: " + fincept::ui::fonts::px(-3) + "px; }"
                "QPushButton#blotterHistoryRefresh:hover { color: %5; border-color: %5; }"
                "QPushButton#blotterLoadMore:disabled, QPushButton#blotterHistoryRefresh:disabled {"
                "  color: %6; border-color: %2; background: transparent; }"
                "QLabel#blotterHistoryCount { color: %6; background: transparent;"
                "  font-size: " + fincept::ui::fonts::px(-6) + "px; }")
            .arg(colors::BG_RAISED(), colors::BORDER_DIM(), colors::TEXT_SECONDARY(), colors::TEXT_PRIMARY(), accent,
                 colors::TEXT_TERTIARY(), colors::BORDER_BRIGHT(), colors::alpha(presentation_.accent, 0.12));
    if (header_bar_->styleSheet() != sheet)
        header_bar_->setStyleSheet(sheet);
}

void PolymarketOrderBlotter::apply_table_style() {
    if (!table_)
        return;
    // One stylesheet for the table AND its per-row action buttons. The row
    // controls are children of the table's viewport, so object-name selectors
    // here reach them — that replaces two setStyleSheet() calls per row (a
    // full CSS reparse each) on every set_orders()/update_order() rebuild.
    // The history table shares it (it has no row controls).
    const QString css =
        QString("QTableWidget { background: %1; color: %2; border: none; font-size: " + fincept::ui::fonts::px(-4) + "px; }"
                "QTableWidget::item { padding: 3px 8px; border-bottom: 1px solid %3; }"
                "QTableWidget::item:selected { background: %4; color: %2; }"
                "QHeaderView::section { background: %5; color: %6; border: none;"
                "  border-bottom: 1px solid %3; padding: 5px 8px;"
                "  font-size: " + fincept::ui::fonts::px(-6) + "px; font-weight: 700; letter-spacing: 0.5px; }"
                "QScrollBar:vertical { background: %1; width: 4px; border: none; }"
                "QScrollBar::handle:vertical { background: %3; min-height: 20px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                "QPushButton#blotterRowRefresh { background: transparent; color: %6; border: 1px solid %3;"
                "  font-size: " + fincept::ui::fonts::px(-3) + "px; }"
                "QPushButton#blotterRowRefresh:hover { color: %7; border-color: %7; }"
                "QPushButton#blotterRowCancel { background: transparent; color: %8; border: 1px solid %8;"
                "  font-size: " + fincept::ui::fonts::px(-3) + "px; }"
                "QPushButton#blotterRowCancel:hover { background: %9; }")
            .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(), colors::BG_HOVER(),
                 colors::BG_RAISED(), colors::TEXT_SECONDARY(), presentation_.accent.name(), colors::DANGER(),
                 colors::alpha(colors::DANGER, 0.15));
    // Only when the text changed: re-run on every presentation / appearance change.
    if (table_->styleSheet() != css)
        table_->setStyleSheet(css);
    if (history_table_ && history_table_->styleSheet() != css)
        history_table_->setStyleSheet(css);
}

void PolymarketOrderBlotter::set_orders(const QVector<OpenOrder>& orders) {
    orders_ = orders;
    // A cancel still in flight keeps its row's state; an order the exchange no longer
    // lists needs no answer any more.
    QSet<QString> listed;
    for (const auto& o : orders_)
        listed.insert(o.order_id);
    cancelling_.intersect(listed);
    rebuild_rows();
}

void PolymarketOrderBlotter::set_cancel_pending(const QString& order_id) {
    if (order_id.isEmpty() || cancelling_.contains(order_id))
        return;
    cancelling_.insert(order_id);
    rebuild_rows();
}

void PolymarketOrderBlotter::set_cancel_done(const QString& order_id, bool cancelled) {
    const bool was_pending = cancelling_.remove(order_id);
    bool removed = false;
    if (cancelled) {
        for (int i = 0; i < orders_.size(); ++i) {
            if (orders_[i].order_id == order_id) {
                orders_.remove(i);
                removed = true;
                break;
            }
        }
    }
    if (was_pending || removed)
        rebuild_rows();
}

void PolymarketOrderBlotter::clear_cancels_pending() {
    if (cancelling_.isEmpty())
        return;
    cancelling_.clear();
    rebuild_rows();
}

void PolymarketOrderBlotter::update_order(const OpenOrder& order) {
    for (int i = 0; i < orders_.size(); ++i) {
        if (orders_[i].order_id == order.order_id) {
            orders_[i] = order;
            rebuild_rows();
            return;
        }
    }
    orders_.push_back(order);
    rebuild_rows();
}

void PolymarketOrderBlotter::clear() {
    orders_.clear();
    cancelling_.clear();
    rebuild_rows();
    history_.clear();
    if (history_shown_)
        rebuild_history();
    sync_header();
}

void PolymarketOrderBlotter::set_presentation(const ExchangePresentation& p) {
    presentation_ = p;
    apply_table_style(); // accent drives the row refresh-button hover colour
    apply_header_style();
    rebuild_rows();
    if (history_shown_)
        rebuild_history(); // item colours are theme tokens resolved at build time
}

// ── Account history ─────────────────────────────────────────────────────────

void PolymarketOrderBlotter::set_history_supported(bool supported) {
    history_supported_ = supported;
    if (!supported && history_shown_)
        show_history(false);
    sync_header();
}

void PolymarketOrderBlotter::reset_history() {
    history_.clear();
    if (history_shown_)
        select_history_kind(history_kind_); // reloads the list on screen
    else
        sync_header();
}

void PolymarketOrderBlotter::show_history(bool on) {
    history_shown_ = on && history_supported_;
    stack_->setCurrentIndex(history_shown_ ? 1 : 0);
    if (history_shown_)
        select_history_kind(history_kind_); // first view of a list loads it
    else
        sync_header();
}

void PolymarketOrderBlotter::select_history_kind(const QString& kind) {
    history_kind_ = kind;
    const auto it = history_.constFind(kind);
    if (it == history_.constEnd() || (!it->loaded && !it->loading))
        request_history(kind, QString()); // rebuilds with the loading state
    else
        rebuild_history();
}

void PolymarketOrderBlotter::request_history(const QString& kind, const QString& cursor) {
    HistoryList& list = history_[kind];
    if (list.loading)
        return; // one page in flight per list
    list.loading = true;
    list.error.clear();
    list.pending_cursor = cursor;
    if (history_shown_ && kind == history_kind_)
        rebuild_history();
    // Emitted last: a request refused on the spot (no credentials) comes straight
    // back through set_history_error and must find the loading state set.
    emit history_requested(kind, cursor);
}

void PolymarketOrderBlotter::set_history_page(const QString& kind, const QVariantList& rows,
                                              const QString& request_cursor, const QString& next_cursor) {
    auto it = history_.find(kind);
    // Not the page asked for: the list was reset (account / exchange change) or
    // this reply is for a request that was superseded.
    if (it == history_.end() || !it->loading || it->pending_cursor != request_cursor)
        return;
    HistoryList& list = *it;
    list.loading = false;
    list.loaded = true;
    list.error.clear();
    if (request_cursor.isEmpty())
        list.rows = rows;
    else
        list.rows += rows;
    // An empty page or a cursor that does not move ends the paging.
    list.next_cursor = (rows.isEmpty() || next_cursor == request_cursor) ? QString() : next_cursor;
    if (history_shown_ && kind == history_kind_)
        rebuild_history();
    else
        sync_header();
}

void PolymarketOrderBlotter::set_history_error(const QString& kind, const QString& message) {
    auto it = history_.find(kind);
    if (it == history_.end() || !it->loading)
        return;
    it->loading = false;
    it->error = message.isEmpty() ? tr("Request failed") : message;
    // rows / next_cursor are kept: a failed "load more" can be retried.
    if (history_shown_ && kind == history_kind_)
        rebuild_history();
    else
        sync_header();
}

void PolymarketOrderBlotter::set_history_message(const QString& text, bool is_error) {
    // Message rows span the table, like the detail panel's empty states.
    const int row = history_table_->rowCount();
    history_table_->setRowCount(row + 1);
    if (history_table_->columnCount() > 1)
        history_table_->setSpan(row, 0, 1, history_table_->columnCount());
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    item->setForeground(QColor(is_error ? colors::DANGER() : colors::TEXT_DIM()));
    item->setFlags(Qt::ItemIsEnabled);
    if (is_error)
        item->setToolTip(text);
    history_table_->setItem(row, 0, item);
}

void PolymarketOrderBlotter::rebuild_history() {
    if (!history_table_)
        return;
    const HistoryList list = history_.value(history_kind_);
    const QStringList headers = history_headers(history_kind_);
    const int price_decimals = qBound(0, presentation_.price_decimal_places, 6);
    const QColor accent = presentation_.accent;
    const QColor positive(colors::POSITIVE());
    const QColor negative(colors::NEGATIVE());
    const QColor dim(colors::TEXT_DIM());
    // Account history exists on Kalshi only: amounts in its settlement currency (USD).
    const QString ccy = presentation_.currency.isEmpty() ? QStringLiteral("USD") : presentation_.currency;

    history_table_->setSortingEnabled(false);
    history_table_->clearSpans();
    history_table_->setRowCount(0);
    history_table_->setColumnCount(headers.size());
    history_table_->setHorizontalHeaderLabels(headers);
    history_table_->setRowCount(list.rows.size());

    const auto side_item = [&](const QString& action) {
        auto* item = new QTableWidgetItem(action.isEmpty() ? QStringLiteral("—") : action);
        if (action.isEmpty()) {
            item->setForeground(dim);
            item->setToolTip(tr("Kalshi reports only the outcome this positioned you for"));
        } else {
            item->setForeground(action == QStringLiteral("BUY") ? positive : negative);
        }
        return item;
    };
    const auto price_item = [&](double price) {
        auto* item = right_item(history_price_text(price, price_decimals));
        item->setForeground(accent);
        return item;
    };
    const auto time_item = [](qint64 ms) {
        auto* item = new QTableWidgetItem(history_time(ms));
        if (ms > 0)
            item->setToolTip(QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        return item;
    };

    for (int i = 0; i < list.rows.size(); ++i) {
        const QVariantMap r = list.rows[i].toMap();
        const QString market = r.value(QStringLiteral("market_id")).toString();
        auto* market_item = new QTableWidgetItem(market);
        if (history_kind_ == kSettlements) {
            history_table_->setItem(i, 0, time_item(r.value(QStringLiteral("ts_ms")).toLongLong()));
            market_item->setToolTip(tr("Event: %1").arg(r.value(QStringLiteral("event_id")).toString()));
            history_table_->setItem(i, 1, market_item);
            const QString result = r.value(QStringLiteral("result")).toString();
            history_table_->setItem(i, 2, new QTableWidgetItem(result.isEmpty() ? QStringLiteral("—") : result));
            history_table_->setItem(i, 3, right_item(count_text(r.value(QStringLiteral("yes_count")).toDouble())));
            history_table_->setItem(i, 4, right_item(count_text(r.value(QStringLiteral("no_count")).toDouble())));
            history_table_->setItem(i, 5, right_item(money_text(r.value(QStringLiteral("cost")).toDouble(), ccy)));
            history_table_->setItem(i, 6, right_item(money_text(r.value(QStringLiteral("payout")).toDouble(), ccy)));
            history_table_->setItem(i, 7, right_item(money_text(r.value(QStringLiteral("fees")).toDouble(), ccy)));
            const double net = r.value(QStringLiteral("net")).toDouble();
            // Signed like any P&L ("+" / "-"), so the colour is never the only cue.
            auto* net_item = right_item(pm_fmt::signed_money(net, ccy));
            if (net >= 0.005)
                net_item->setForeground(positive);
            else if (net <= -0.005)
                net_item->setForeground(negative);
            net_item->setToolTip(tr("Payout − cost − fees"));
            history_table_->setItem(i, 8, net_item);
        } else if (history_kind_ == kFills) {
            history_table_->setItem(i, 0, time_item(r.value(QStringLiteral("ts_ms")).toLongLong()));
            market_item->setToolTip(tr("Order ID: %1\nFill ID: %2")
                                        .arg(r.value(QStringLiteral("order_id")).toString(),
                                             r.value(QStringLiteral("fill_id")).toString()));
            history_table_->setItem(i, 1, market_item);
            history_table_->setItem(i, 2, side_item(r.value(QStringLiteral("action")).toString()));
            history_table_->setItem(i, 3, new QTableWidgetItem(r.value(QStringLiteral("outcome")).toString()));
            history_table_->setItem(i, 4, price_item(r.value(QStringLiteral("price")).toDouble()));
            history_table_->setItem(i, 5, right_item(count_text(r.value(QStringLiteral("size")).toDouble())));
            history_table_->setItem(i, 6, right_item(money_text(r.value(QStringLiteral("fees")).toDouble(), ccy)));
            history_table_->setItem(i, 7, new QTableWidgetItem(r.value(QStringLiteral("is_taker")).toBool()
                                                                   ? tr("TAKER")
                                                                   : tr("MAKER")));
        } else {
            history_table_->setItem(i, 0, time_item(r.value(QStringLiteral("ts_ms")).toLongLong()));
            const qint64 updated = r.value(QStringLiteral("updated_ms")).toLongLong();
            market_item->setToolTip(updated > 0 ? tr("Order ID: %1\nLast update: %2")
                                                      .arg(r.value(QStringLiteral("order_id")).toString(),
                                                           QDateTime::fromMSecsSinceEpoch(updated).toString(
                                                               QStringLiteral("yyyy-MM-dd HH:mm:ss")))
                                                : tr("Order ID: %1").arg(r.value(QStringLiteral("order_id")).toString()));
            history_table_->setItem(i, 1, market_item);
            history_table_->setItem(i, 2, side_item(r.value(QStringLiteral("action")).toString()));
            history_table_->setItem(i, 3, new QTableWidgetItem(r.value(QStringLiteral("outcome")).toString()));
            history_table_->setItem(i, 4, new QTableWidgetItem(r.value(QStringLiteral("order_type")).toString()));
            history_table_->setItem(i, 5, price_item(r.value(QStringLiteral("price")).toDouble()));
            history_table_->setItem(i, 6, right_item(count_text(r.value(QStringLiteral("size")).toDouble())));
            history_table_->setItem(i, 7, right_item(count_text(r.value(QStringLiteral("filled")).toDouble())));
            history_table_->setItem(i, 8, new QTableWidgetItem(r.value(QStringLiteral("status")).toString()));
            history_table_->setItem(i, 9, right_item(money_text(r.value(QStringLiteral("fees")).toDouble(), ccy)));
        }
    }

    // Sized from the data rows only: a spanning state row must not widen column 0.
    history_table_->resizeColumnsToContents();
    history_table_->horizontalHeader()->setSectionResizeMode(kHistoryMarketCol, QHeaderView::Stretch);

    // State rows: loading / error / empty, below any rows already loaded (a
    // "load more" in flight or failed keeps the earlier pages on screen).
    if (list.loading)
        set_history_message(list.pending_cursor.isEmpty() ? tr("Loading…") : tr("Loading more…"), false);
    else if (!list.error.isEmpty())
        set_history_message(list.error, true);
    else if (list.loaded && list.rows.isEmpty())
        set_history_message(history_empty_text(history_kind_), false);
    sync_header();
}

void PolymarketOrderBlotter::sync_header() {
    if (!header_bar_)
        return;
    const auto repolish = [](QWidget* w) {
        w->style()->unpolish(w);
        w->style()->polish(w);
    };
    title_label_->setVisible(!history_supported_);
    orders_tab_btn_->setVisible(history_supported_);
    history_tab_btn_->setVisible(history_supported_);
    orders_tab_btn_->setProperty("active", !history_shown_);
    history_tab_btn_->setProperty("active", history_shown_);
    repolish(orders_tab_btn_);
    repolish(history_tab_btn_);

    history_kinds_->setVisible(history_shown_);
    history_count_lbl_->setVisible(history_shown_);
    load_more_btn_->setVisible(history_shown_);
    history_refresh_btn_->setVisible(history_shown_);
    cancel_all_btn_->setVisible(!history_shown_);
    if (!history_shown_)
        return;

    for (auto* pill : history_kind_btns_) {
        pill->setProperty("active", pill->property("kind").toString() == history_kind_);
        repolish(pill);
    }
    const HistoryList list = history_.value(history_kind_);
    const bool more = !list.next_cursor.isEmpty();
    load_more_btn_->setEnabled(list.loaded && !list.loading && more);
    load_more_btn_->setToolTip(more ? tr("Load the next page") : tr("Every page is loaded"));
    history_refresh_btn_->setEnabled(!list.loading);
    if (list.loaded && !list.rows.isEmpty())
        history_count_lbl_->setText(more ? tr("%1 rows").arg(list.rows.size())
                                         : tr("%1 rows · all loaded").arg(list.rows.size()));
    else
        history_count_lbl_->clear();
}

void PolymarketOrderBlotter::set_capabilities(bool supports_amend, bool supports_batch_cancel) {
    supports_amend_ = supports_amend;
    supports_batch_cancel_ = supports_batch_cancel;
    rebuild_rows();
}

void PolymarketOrderBlotter::rebuild_rows() {
    table_->setSortingEnabled(false);
    table_->clearSpans();
    table_->setRowCount(orders_.size());
    const int price_decimals = qBound(0, presentation_.price_decimal_places, 6);

    for (int i = 0; i < orders_.size(); ++i) {
        const auto& o = orders_[i];
        // The TIME column used to render `expires_ms` as a raw 13-digit epoch
        // integer — unreadable, and the wrong field for the heading. Show the
        // order's creation time formatted, with the expiry in the tooltip.
        auto* time_item = new QTableWidgetItem(
            o.created_ms > 0 ? QDateTime::fromMSecsSinceEpoch(o.created_ms).toString(QStringLiteral("MM-dd HH:mm:ss"))
                             : QStringLiteral("—"));
        time_item->setData(Qt::UserRole, QVariant::fromValue(o.created_ms));
        time_item->setToolTip(o.expires_ms > 0
                                  ? tr("Expires %1").arg(QDateTime::fromMSecsSinceEpoch(o.expires_ms).toString(
                                        QStringLiteral("yyyy-MM-dd HH:mm:ss")))
                                  : tr("No expiry (good-till-cancelled)"));
        table_->setItem(i, ColTime, time_item);

        auto* market_item = new QTableWidgetItem(o.market_id);
        // The cancel button targets order_id — surface it so the user can
        // verify which order a row's ✕ will actually cancel.
        market_item->setToolTip(tr("Order ID: %1").arg(o.order_id));
        table_->setItem(i, ColMarket, market_item);

        auto* side_item = new QTableWidgetItem(o.side);
        side_item->setForeground(
            QColor(o.side.compare("BUY", Qt::CaseInsensitive) == 0 ? colors::POSITIVE() : colors::NEGATIVE()));
        table_->setItem(i, ColSide, side_item);

        table_->setItem(i, ColOutcome, new QTableWidgetItem(o.outcome));
        table_->setItem(i, ColType, new QTableWidgetItem(o.order_type));

        auto* price_item = new QTableWidgetItem(QString::number(o.price, 'f', price_decimals));
        price_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        price_item->setForeground(presentation_.accent);
        // The double-click-to-amend affordance: mark the cell enabled only
        // when the adapter supports amend. Leaves the cell display-only on
        // Polymarket so users don't double-click expecting behaviour.
        if (supports_amend_) {
            price_item->setFlags(price_item->flags() | Qt::ItemIsEnabled);
            price_item->setToolTip(tr("Double-click to amend price"));
            // Double-click / Enter on this cell amends a live order (agents operating the UI).
            price_item->setData(fincept::core::agent::kAgentRiskRole, QStringLiteral("order"));
        }
        table_->setItem(i, ColPrice, price_item);

        auto* size_item = new QTableWidgetItem(QString::number(o.size, 'f', 2));
        size_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, ColSize, size_item);

        auto* filled_item = new QTableWidgetItem(QString::number(o.filled, 'f', 2));
        filled_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, ColFilled, filled_item);

        if (cancelling_.contains(o.order_id)) {
            // Cancel sent, no answer from the exchange yet.
            auto* status_item = new QTableWidgetItem(tr("cancelling…"));
            status_item->setForeground(QColor(colors::WARNING()));
            status_item->setToolTip(tr("Cancel sent — waiting for the exchange to confirm (was %1)").arg(o.status));
            table_->setItem(i, ColStatus, status_item);
        } else {
            table_->setItem(i, ColStatus, new QTableWidgetItem(o.status));
        }

        install_row_controls(i, o);
    }

    table_->resizeColumnsToContents();
    table_->horizontalHeader()->setSectionResizeMode(ColMarket, QHeaderView::Stretch);
    if (orders_.isEmpty()) {
        // Say it instead of a bare header (same message row as the history lists;
        // a double-click on it is ignored — it is past the end of orders_).
        table_->setRowCount(1);
        table_->setSpan(0, 0, 1, table_->columnCount());
        auto* item = new QTableWidgetItem(tr("No open orders on this account — orders you place on the TRADE tab "
                                             "appear here until they fill or are cancelled."));
        item->setTextAlignment(Qt::AlignCenter);
        item->setForeground(QColor(colors::TEXT_DIM()));
        item->setFlags(Qt::ItemIsEnabled);
        table_->setItem(0, 0, item);
    }

    // "Cancel All" enable state — only meaningful when there's at least
    // one row and the adapter supports either single- or batch-cancel.
    if (auto* btn = findChild<QPushButton*>(QStringLiteral("orderBlotterCancelAll"))) {
        btn->setEnabled(!orders_.isEmpty());
    }
}

void PolymarketOrderBlotter::install_row_controls(int row, const OpenOrder& order) {
    auto* host = new QWidget(table_);
    auto* rhl = new QHBoxLayout(host);
    rhl->setContentsMargins(4, 0, 4, 0);
    rhl->setSpacing(4);

    // Styling comes from the table-level stylesheet (see apply_table_style)
    // via these object names — no per-row setStyleSheet.
    auto* refresh_btn = new QPushButton(QStringLiteral("↻"), host);
    refresh_btn->setObjectName(QStringLiteral("blotterRowRefresh"));
    refresh_btn->setMinimumSize(22, 20);
    refresh_btn->setCursor(Qt::PointingHandCursor);
    refresh_btn->setToolTip(tr("Refresh order state"));
    refresh_btn->setAccessibleName(tr("Refresh order %1").arg(order.order_id));
    const QString oid = order.order_id;
    connect(refresh_btn, &QPushButton::clicked, this, [this, oid]() { emit refresh_order(oid); });
    rhl->addWidget(refresh_btn);

    auto* cancel_btn = new QPushButton(QStringLiteral("✕"), host);
    cancel_btn->setObjectName(QStringLiteral("blotterRowCancel"));
    cancel_btn->setProperty("fincept_agent_risk", "order");
    cancel_btn->setMinimumSize(22, 20);
    cancel_btn->setCursor(Qt::PointingHandCursor);
    // Spell out exactly which order the ✕ targets — the row's own id, not the
    // selected row's.
    cancel_btn->setToolTip(tr("Cancel %1 %2 @ %3 (id %4)")
                               .arg(order.side, order.outcome)
                               .arg(order.price, 0, 'f', qBound(0, presentation_.price_decimal_places, 6))
                               .arg(order.order_id));
    cancel_btn->setAccessibleName(tr("Cancel order %1").arg(order.order_id));
    if (cancelling_.contains(oid)) {
        // One cancel per order at a time: the answer restores or removes the row.
        cancel_btn->setEnabled(false);
        cancel_btn->setToolTip(tr("Cancelling %1 — waiting for the exchange").arg(order.order_id));
    }
    connect(cancel_btn, &QPushButton::clicked, this, [this, oid]() { emit cancel_order(oid); });
    rhl->addWidget(cancel_btn);

    rhl->addStretch(1);
    table_->setCellWidget(row, ColActions, host);
}

void PolymarketOrderBlotter::on_cell_double_clicked(int row, int column) {
    if (column != ColPrice || !supports_amend_)
        return;
    if (row < 0 || row >= orders_.size())
        return;
    // Copy: the blocking dialog below runs an event loop, and a refresh landing
    // meanwhile rebuilds orders_ (a reference into it would dangle).
    const OpenOrder o = orders_[row];
    // The price on the wire is per OUTCOME (Kalshi yes_price / no_price). `side` is
    // the action (BUY / SELL), never "yes"/"no", so emitting it sent every amend as
    // a NO price — a YES order amended to 0.40 went out as no_price=40 (YES 60c).
    const QString outcome = o.outcome.trimmed().toLower();
    if (outcome != QLatin1String("yes") && outcome != QLatin1String("no")) {
        QMessageBox::warning(this, tr("Amend price"),
                             tr("Cannot amend order %1: its outcome (\"%2\") is not YES or NO.")
                                 .arg(o.order_id, o.outcome));
        return;
    }
    bool ok = false;
    // The Kalshi limit-price domain is 1-99 cents — we constrain the input
    // dialog to the valid range so users can't submit an order that the
    // exchange will reject. The label names the outcome the price applies to.
    const double new_price =
        QInputDialog::getDouble(this, tr("Amend price"),
                                tr("New %1 limit price for this %2 %1 order (0.01 – 0.99):")
                                    .arg(o.outcome.toUpper(), o.side.toUpper()),
                                o.price, 0.01, 0.99, 2, &ok);
    if (!ok)
        return;
    emit amend_order(o.order_id, outcome, new_price);
}

void PolymarketOrderBlotter::on_cancel_all_clicked() {
    if (orders_.isEmpty())
        return;
    QStringList ids;
    ids.reserve(orders_.size());
    for (const auto& o : orders_)
        if (!o.order_id.isEmpty())
            ids.push_back(o.order_id);
    if (ids.isEmpty())
        return;
    // Pulling every resting order off the book is destructive and was a
    // single unguarded click. Confirm, and say how many.
    const auto choice = QMessageBox::warning(
        this, tr("Cancel all orders?"),
        tr("Cancel all %1 open order(s) on %2?\n\nThis cannot be undone — any partially filled quantity stays filled.")
            .arg(ids.size())
            .arg(presentation_.display_name.isEmpty() ? tr("this exchange") : presentation_.display_name),
        QMessageBox::Cancel | QMessageBox::Yes, QMessageBox::Cancel);
    if (choice != QMessageBox::Yes)
        return;
    emit cancel_all(ids);
}

void PolymarketOrderBlotter::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PolymarketOrderBlotter::retranslateUi() {
    if (title_label_)
        title_label_->setText(tr("OPEN ORDERS"));
    if (cancel_all_btn_)
        cancel_all_btn_->setText(tr("CANCEL ALL"));
    if (table_) {
        QStringList headers;
        for (int c = 0; c < ColCount; ++c)
            headers << col_label(c);
        table_->setHorizontalHeaderLabels(headers);
    }
    // Per-row tooltips re-apply on the next rebuild_rows(); row data is
    // exchange data (side / status / outcome) and is not translated.
    rebuild_rows();

    if (orders_tab_btn_) {
        orders_tab_btn_->setText(tr("OPEN ORDERS"));
        orders_tab_btn_->setAccessibleName(tr("Open orders"));
    }
    if (history_tab_btn_) {
        history_tab_btn_->setText(tr("ACCOUNT HISTORY"));
        history_tab_btn_->setAccessibleName(tr("Account history"));
        history_tab_btn_->setToolTip(tr("Settlements, archived fills and archived orders of the connected account"));
    }
    for (auto* pill : history_kind_btns_) {
        const QString kind = pill->property("kind").toString();
        pill->setText(history_kind_label(kind));
        pill->setToolTip(history_kind_tooltip(kind));
        pill->setAccessibleName(history_kind_label(kind));
    }
    if (load_more_btn_) {
        load_more_btn_->setText(tr("LOAD MORE"));
        load_more_btn_->setAccessibleName(tr("Load the next page"));
    }
    if (history_refresh_btn_) {
        history_refresh_btn_->setToolTip(tr("Reload from the first page"));
        history_refresh_btn_->setAccessibleName(tr("Reload account history"));
    }
    if (history_table_)
        history_table_->setAccessibleName(tr("Account history"));
    if (history_shown_)
        rebuild_history(); // headers, state rows and the row count
    else
        sync_header();
}

} // namespace fincept::screens::polymarket
