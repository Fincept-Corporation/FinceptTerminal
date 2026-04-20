#include "screens/polymarket/PolymarketLeaderboard.h"

#include "ui/theme/Theme.h"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace fincept::screens::polymarket {

using namespace fincept::ui;
using namespace fincept::services::polymarket;

static QString fmt_pnl(double v) {
    QString sign = v >= 0 ? "+" : "";
    if (qAbs(v) >= 1e6)
        return sign + QString("$%1M").arg(v / 1e6, 0, 'f', 1);
    if (qAbs(v) >= 1e3)
        return sign + QString("$%1K").arg(v / 1e3, 0, 'f', 1);
    return sign + QString("$%1").arg(v, 0, 'f', 0);
}

PolymarketLeaderboard::PolymarketLeaderboard(QWidget* parent) : QWidget(parent) {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* header_bar = new QWidget;
    header_bar->setFixedHeight(30);
    header_bar->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;")
            .arg(colors::BG_RAISED(), colors::BORDER_DIM()));
    auto* hbl = new QHBoxLayout(header_bar);
    hbl->setContentsMargins(12, 0, 8, 0);
    auto* header = new QLabel("LEADERBOARD");
    header->setStyleSheet(
        QString("color: %1; font-size: 9px; font-weight: 700; letter-spacing: 0.8px; "
                "background: transparent;")
            .arg(colors::TEXT_SECONDARY()));
    hbl->addWidget(header);
    hbl->addStretch(1);
    vl->addWidget(header_bar);

    table_ = new QTableWidget;
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({"#", "TRADER", "PNL", "VOLUME", "TRADES"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
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
    vl->addWidget(table_, 1);
}

void PolymarketLeaderboard::set_entries(const QVector<LeaderboardEntry>& entries) {
    table_->setSortingEnabled(false);
    table_->setRowCount(entries.size());

    for (int i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];

        auto* rank_item = new QTableWidgetItem(QString::number(e.rank > 0 ? e.rank : i + 1));
        rank_item->setTextAlignment(Qt::AlignCenter);
        table_->setItem(i, 0, rank_item);

        table_->setItem(i, 1, new QTableWidgetItem(e.display_name));

        auto* pnl_item = new QTableWidgetItem(fmt_pnl(e.pnl));
        pnl_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        pnl_item->setForeground(QColor(e.pnl >= 0 ? colors::POSITIVE() : colors::NEGATIVE()));
        table_->setItem(i, 2, pnl_item);

        auto* vol_item = new QTableWidgetItem(fmt_pnl(e.volume).replace("+", ""));
        vol_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 3, vol_item);

        auto* trades_item = new QTableWidgetItem(QString::number(e.num_trades));
        trades_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 4, trades_item);
    }

    table_->resizeColumnsToContents();
    table_->setSortingEnabled(true);
}

void PolymarketLeaderboard::set_loading(bool loading) {
    if (loading) {
        table_->setRowCount(0);
    }
}

} // namespace fincept::screens::polymarket
