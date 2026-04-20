#include "screens/polymarket/PolymarketActivityFeed.h"

#include "ui/theme/Theme.h"

#include <QBrush>
#include <QDateTime>
#include <QHeaderView>
#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens::polymarket {

using namespace fincept::ui;
using namespace fincept::services::prediction;

namespace {

constexpr int kMaxRows = 200;
constexpr int kFlashMs = 600;

void apply_row(QTableWidget* table, int row, const PredictionTrade& t, int price_decimals) {
    const QString time = QDateTime::fromMSecsSinceEpoch(t.ts_ms, Qt::UTC).toString("HH:mm:ss");
    table->setItem(row, 0, new QTableWidgetItem(time));
    table->setItem(row, 1, new QTableWidgetItem("TRADE"));

    auto* side_item = new QTableWidgetItem(t.side);
    side_item->setForeground(QColor(t.side.compare("BUY", Qt::CaseInsensitive) == 0
                                        ? colors::POSITIVE()
                                        : colors::NEGATIVE()));
    table->setItem(row, 2, side_item);

    auto* price_item = new QTableWidgetItem(QString::number(t.price, 'f', price_decimals));
    price_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    price_item->setForeground(QColor(colors::CYAN()));
    table->setItem(row, 3, price_item);

    auto* size_item = new QTableWidgetItem(QString::number(t.size, 'f', 2));
    size_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table->setItem(row, 4, size_item);
}

void flash_row(QTableWidget* table, int row, const QColor& accent) {
    if (row < 0 || row >= table->rowCount()) return;
    // Tint the row's backgrounds; clear after kFlashMs so the highlight
    // decays without needing per-row animation timers.
    QColor tint = accent;
    tint.setAlpha(90);
    for (int c = 0; c < table->columnCount(); ++c) {
        if (auto* it = table->item(row, c))
            it->setBackground(QBrush(tint));
    }
    QPointer<QTableWidget> guard = table;
    QTimer::singleShot(kFlashMs, table, [guard, row]() {
        if (!guard) return;
        if (row >= guard->rowCount()) return;
        for (int c = 0; c < guard->columnCount(); ++c) {
            if (auto* it = guard->item(row, c))
                it->setBackground(QBrush(Qt::NoBrush));
        }
    });
}

} // namespace

PolymarketActivityFeed::PolymarketActivityFeed(QWidget* parent) : QWidget(parent) {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);

    table_ = new QTableWidget;
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({"TIME", "TYPE", "SIDE", "PRICE", "SIZE"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setShowGrid(false);
    table_->setStyleSheet(
        QString("QTableWidget {"
                "  background: %1; color: %2; border: none; font-size: 10px;"
                "}"
                "QTableWidget::item {"
                "  padding: 3px 8px; border-bottom: 1px solid %3;"
                "}"
                "QTableWidget::item:selected { background: %4; color: %2; }"
                "QHeaderView::section {"
                "  background: %5; color: %6; border: none;"
                "  border-bottom: 1px solid %3;"
                "  padding: 5px 8px; font-size: 8px; font-weight: 700; letter-spacing: 0.5px;"
                "}"
                "QScrollBar:vertical { background: %1; width: 4px; border: none; }"
                "QScrollBar::handle:vertical { background: %3; min-height: 20px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
            .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(),
                 colors::BG_HOVER(), colors::BG_RAISED(), colors::TEXT_SECONDARY()));
    vl->addWidget(table_);
}

void PolymarketActivityFeed::set_trades(const QVector<PredictionTrade>& trades) {
    last_trades_ = trades;
    table_->setSortingEnabled(false);
    const int count = qMin(trades.size(), kMaxRows);
    table_->setRowCount(count);

    const int price_decimals = qBound(0, presentation_.price_decimal_places, 6);
    for (int i = 0; i < count; ++i) {
        apply_row(table_, i, trades[i], price_decimals);
    }
    table_->resizeColumnsToContents();
    table_->setSortingEnabled(true);
}

void PolymarketActivityFeed::append_trade(const PredictionTrade& trade) {
    // Live WS trade — prepend to the top row, cap at kMaxRows. We disable
    // sorting while mutating so insertRow() keeps the newest on top. The
    // in-memory trade vector is also updated so set_presentation() re-renders
    // identically.
    table_->setSortingEnabled(false);
    last_trades_.prepend(trade);
    if (last_trades_.size() > kMaxRows)
        last_trades_.resize(kMaxRows);

    table_->insertRow(0);
    if (table_->rowCount() > kMaxRows)
        table_->removeRow(table_->rowCount() - 1);

    const int price_decimals = qBound(0, presentation_.price_decimal_places, 6);
    apply_row(table_, 0, trade, price_decimals);

    flash_row(table_, 0, presentation_.accent);
    table_->setSortingEnabled(true);
}

void PolymarketActivityFeed::clear() {
    last_trades_.clear();
    table_->setRowCount(0);
}

void PolymarketActivityFeed::set_presentation(const ExchangePresentation& p) {
    const bool decimals_changed = p.price_decimal_places != presentation_.price_decimal_places;
    presentation_ = p;
    if (decimals_changed && !last_trades_.isEmpty()) set_trades(last_trades_);
}

} // namespace fincept::screens::polymarket
