#include "screens/polymarket/PolymarketOrderBlotter.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

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
    switch (c) {
        case ColTime: return QStringLiteral("TIME");
        case ColMarket: return QStringLiteral("MARKET");
        case ColSide: return QStringLiteral("SIDE");
        case ColOutcome: return QStringLiteral("OUTCOME");
        case ColType: return QStringLiteral("TYPE");
        case ColPrice: return QStringLiteral("PRICE");
        case ColSize: return QStringLiteral("SIZE");
        case ColFilled: return QStringLiteral("FILLED");
        case ColStatus: return QStringLiteral("STATUS");
        case ColActions: return QStringLiteral("ACTIONS");
    }
    return {};
}

} // namespace

PolymarketOrderBlotter::PolymarketOrderBlotter(QWidget* parent) : QWidget(parent) {
    setObjectName("polyOrderBlotter");
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* header_bar = new QWidget(this);
    header_bar->setFixedHeight(30);
    header_bar->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;")
            .arg(colors::BG_RAISED(), colors::BORDER_DIM()));
    auto* hhl = new QHBoxLayout(header_bar);
    hhl->setContentsMargins(12, 0, 8, 0);
    hhl->setSpacing(8);

    auto* title = new QLabel(QStringLiteral("OPEN ORDERS"), header_bar);
    title->setStyleSheet(
        QString("color: %1; font-size: 9px; font-weight: 700; background: transparent; "
                "letter-spacing: 0.8px;")
            .arg(colors::TEXT_SECONDARY()));
    hhl->addWidget(title);
    hhl->addStretch(1);

    auto* cancel_all_btn = new QPushButton(QStringLiteral("CANCEL ALL"), header_bar);
    cancel_all_btn->setObjectName("orderBlotterCancelAll");
    cancel_all_btn->setCursor(Qt::PointingHandCursor);
    cancel_all_btn->setStyleSheet(
        QString("QPushButton { background: transparent; color: %1; "
                "  border: 1px solid %1; padding: 2px 8px; font-size: 9px; font-weight: 700; }"
                "QPushButton:hover { background: rgba(239,68,68,0.15); }"
                "QPushButton:disabled { color: %2; border-color: %2; }")
            .arg(colors::NEGATIVE(), colors::TEXT_DIM()));
    connect(cancel_all_btn, &QPushButton::clicked, this,
            &PolymarketOrderBlotter::on_cancel_all_clicked);
    hhl->addWidget(cancel_all_btn);
    vl->addWidget(header_bar);

    table_ = new QTableWidget(this);
    table_->setColumnCount(ColCount);
    QStringList headers;
    for (int c = 0; c < ColCount; ++c) headers << col_label(c);
    table_->setHorizontalHeaderLabels(headers);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);  // double-click drives amend
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(ColMarket, QHeaderView::Stretch);
    table_->setShowGrid(false);
    table_->setStyleSheet(
        QString("QTableWidget { background: %1; color: %2; border: none; font-size: 10px; }"
                "QTableWidget::item { padding: 3px 8px; border-bottom: 1px solid %3; }"
                "QTableWidget::item:selected { background: %4; color: %2; }"
                "QHeaderView::section { background: %5; color: %6; border: none;"
                "  border-bottom: 1px solid %3; padding: 5px 8px;"
                "  font-size: 8px; font-weight: 700; letter-spacing: 0.5px; }"
                "QScrollBar:vertical { background: %1; width: 4px; border: none; }"
                "QScrollBar::handle:vertical { background: %3; min-height: 20px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
            .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(),
                 colors::BG_HOVER(), colors::BG_RAISED(), colors::TEXT_SECONDARY()));
    connect(table_, &QTableWidget::cellDoubleClicked, this,
            &PolymarketOrderBlotter::on_cell_double_clicked);
    vl->addWidget(table_, 1);
}

void PolymarketOrderBlotter::set_orders(const QVector<OpenOrder>& orders) {
    orders_ = orders;
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
    rebuild_rows();
}

void PolymarketOrderBlotter::set_presentation(const ExchangePresentation& p) {
    presentation_ = p;
    rebuild_rows();
}

void PolymarketOrderBlotter::set_capabilities(bool supports_amend, bool supports_batch_cancel) {
    supports_amend_ = supports_amend;
    supports_batch_cancel_ = supports_batch_cancel;
    rebuild_rows();
}

void PolymarketOrderBlotter::rebuild_rows() {
    table_->setSortingEnabled(false);
    table_->setRowCount(orders_.size());
    const int price_decimals = qBound(0, presentation_.price_decimal_places, 6);

    for (int i = 0; i < orders_.size(); ++i) {
        const auto& o = orders_[i];
        table_->setItem(i, ColTime,
                        new QTableWidgetItem(QString::number(o.expires_ms)));  // placeholder: expiry
        table_->setItem(i, ColMarket, new QTableWidgetItem(o.market_id));

        auto* side_item = new QTableWidgetItem(o.side);
        side_item->setForeground(QColor(o.side.compare("BUY", Qt::CaseInsensitive) == 0
                                            ? colors::POSITIVE()
                                            : colors::NEGATIVE()));
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
            price_item->setToolTip(QStringLiteral("Double-click to amend price"));
        }
        table_->setItem(i, ColPrice, price_item);

        auto* size_item = new QTableWidgetItem(QString::number(o.size, 'f', 2));
        size_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, ColSize, size_item);

        auto* filled_item = new QTableWidgetItem(QString::number(o.filled, 'f', 2));
        filled_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, ColFilled, filled_item);

        table_->setItem(i, ColStatus, new QTableWidgetItem(o.status));

        install_row_controls(i, o);
    }

    table_->resizeColumnsToContents();
    table_->horizontalHeader()->setSectionResizeMode(ColMarket, QHeaderView::Stretch);

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

    auto* refresh_btn = new QPushButton(QStringLiteral("↻"), host);
    refresh_btn->setFixedSize(22, 20);
    refresh_btn->setCursor(Qt::PointingHandCursor);
    refresh_btn->setToolTip(QStringLiteral("Refresh order state"));
    refresh_btn->setStyleSheet(
        QString("QPushButton { background: transparent; color: %1; border: 1px solid %2; "
                "font-size: 11px; } QPushButton:hover { color: %3; border-color: %3; }")
            .arg(colors::TEXT_SECONDARY(), colors::BORDER_DIM(),
                 presentation_.accent.name()));
    const QString oid = order.order_id;
    connect(refresh_btn, &QPushButton::clicked, this,
            [this, oid]() { emit refresh_order(oid); });
    rhl->addWidget(refresh_btn);

    auto* cancel_btn = new QPushButton(QStringLiteral("✕"), host);
    cancel_btn->setFixedSize(22, 20);
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setToolTip(QStringLiteral("Cancel order"));
    cancel_btn->setStyleSheet(
        QString("QPushButton { background: transparent; color: %1; border: 1px solid %1; "
                "font-size: 11px; } QPushButton:hover { background: rgba(239,68,68,0.15); }")
            .arg(colors::NEGATIVE()));
    connect(cancel_btn, &QPushButton::clicked, this,
            [this, oid]() { emit cancel_order(oid); });
    rhl->addWidget(cancel_btn);

    rhl->addStretch(1);
    table_->setCellWidget(row, ColActions, host);
}

void PolymarketOrderBlotter::on_cell_double_clicked(int row, int column) {
    if (column != ColPrice || !supports_amend_) return;
    if (row < 0 || row >= orders_.size()) return;
    const auto& o = orders_[row];
    bool ok = false;
    // The Kalshi limit-price domain is 1-99 cents — we constrain the input
    // dialog to the valid range so users can't submit an order that the
    // exchange will reject.
    const double new_price = QInputDialog::getDouble(
        this, QStringLiteral("Amend price"),
        QStringLiteral("New limit price (0.01 – 0.99):"),
        o.price, 0.01, 0.99, 2, &ok);
    if (!ok) return;
    emit amend_order(o.order_id, o.side.toLower(), new_price);
}

void PolymarketOrderBlotter::on_cancel_all_clicked() {
    if (orders_.isEmpty()) return;
    QStringList ids;
    ids.reserve(orders_.size());
    for (const auto& o : orders_)
        if (!o.order_id.isEmpty()) ids.push_back(o.order_id);
    if (!ids.isEmpty()) emit cancel_all(ids);
}

} // namespace fincept::screens::polymarket
