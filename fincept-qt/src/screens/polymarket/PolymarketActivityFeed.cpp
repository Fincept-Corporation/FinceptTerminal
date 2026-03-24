#include "screens/polymarket/PolymarketActivityFeed.h"

#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QHeaderView>
#include <QVBoxLayout>

namespace fincept::screens::polymarket {

using namespace fincept::ui;
using namespace fincept::services::polymarket;

PolymarketActivityFeed::PolymarketActivityFeed(QWidget* parent) : QWidget(parent) {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);

    table_ = new QTableWidget;
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({"TIME", "TYPE", "SIDE", "PRICE", "SIZE"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setStyleSheet(
        QString("QTableWidget { background: %1; color: %2; border: none; gridline-color: %3; font-size: 11px; }"
                "QTableWidget::item { padding: 2px 6px; border-bottom: 1px solid %3; }"
                "QHeaderView::section { background: %4; color: %5; border: none; "
                "  border-bottom: 1px solid %3; padding: 4px 6px; font-size: 10px; font-weight: 700; }")
            .arg(colors::BG_BASE, colors::TEXT_PRIMARY, colors::BORDER_DIM, colors::BG_RAISED, colors::TEXT_SECONDARY));
    vl->addWidget(table_);
}

void PolymarketActivityFeed::set_activities(const QVector<Activity>& activities) {
    table_->setSortingEnabled(false);
    table_->setRowCount(qMin(activities.size(), 200));

    for (int i = 0; i < qMin(activities.size(), 200); ++i) {
        const auto& a = activities[i];
        QString time = QDateTime::fromSecsSinceEpoch(a.timestamp, Qt::UTC).toString("HH:mm:ss");
        table_->setItem(i, 0, new QTableWidgetItem(time));
        table_->setItem(i, 1, new QTableWidgetItem(a.type));

        auto* side_item = new QTableWidgetItem(a.outcome);
        table_->setItem(i, 2, side_item);

        auto* price_item = new QTableWidgetItem(QString::number(a.price, 'f', 4));
        price_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        price_item->setForeground(QColor(colors::CYAN));
        table_->setItem(i, 3, price_item);

        auto* size_item = new QTableWidgetItem(QString::number(a.amount, 'f', 2));
        size_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 4, size_item);
    }
    table_->resizeColumnsToContents();
    table_->setSortingEnabled(true);
}

void PolymarketActivityFeed::set_trades(const QVector<Trade>& trades) {
    table_->setSortingEnabled(false);
    table_->setRowCount(qMin(trades.size(), 200));

    for (int i = 0; i < qMin(trades.size(), 200); ++i) {
        const auto& t = trades[i];
        QString time = QDateTime::fromSecsSinceEpoch(t.timestamp, Qt::UTC).toString("HH:mm:ss");
        table_->setItem(i, 0, new QTableWidgetItem(time));
        table_->setItem(i, 1, new QTableWidgetItem("TRADE"));

        auto* side_item = new QTableWidgetItem(t.side);
        side_item->setForeground(QColor(t.side == "BUY" ? colors::POSITIVE : colors::NEGATIVE));
        table_->setItem(i, 2, side_item);

        auto* price_item = new QTableWidgetItem(QString::number(t.price, 'f', 4));
        price_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        price_item->setForeground(QColor(colors::CYAN));
        table_->setItem(i, 3, price_item);

        auto* size_item = new QTableWidgetItem(QString::number(t.size, 'f', 2));
        size_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 4, size_item);
    }
    table_->resizeColumnsToContents();
    table_->setSortingEnabled(true);
}

void PolymarketActivityFeed::clear() {
    table_->setRowCount(0);
}

} // namespace fincept::screens::polymarket
