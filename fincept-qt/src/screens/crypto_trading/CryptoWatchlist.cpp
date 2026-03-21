#include "screens/crypto_trading/CryptoWatchlist.h"
#include "trading/TradingTypes.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMutexLocker>
#include <algorithm>

namespace fincept::screens::crypto {

CryptoWatchlist::CryptoWatchlist(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(4);

    auto* header = new QLabel("Watchlist");
    header->setStyleSheet("font-weight: bold; font-size: 13px; color: #00aaff; padding: 0 4px;");
    layout->addWidget(header);

    count_label_ = new QLabel("0/0");
    count_label_->setStyleSheet("color: #666; font-size: 11px; padding: 0 4px;");
    layout->addWidget(count_label_);

    filter_edit_ = new QLineEdit;
    filter_edit_->setPlaceholderText("Search symbol...");
    filter_edit_->setStyleSheet(
        "QLineEdit { background: #1a1a2e; border: 1px solid #333; border-radius: 3px; "
        "color: #ccc; padding: 3px 6px; font-size: 12px; }");
    connect(filter_edit_, &QLineEdit::textChanged, this, &CryptoWatchlist::on_filter_changed);
    layout->addWidget(filter_edit_);

    table_ = new QTableWidget;
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels({"Symbol", "Price", "Chg%"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->hide();
    table_->setShowGrid(false);
    table_->setStyleSheet(
        "QTableWidget { background: #0d1117; border: none; color: #ccc; font-size: 12px; }"
        "QTableWidget::item { padding: 2px 4px; }"
        "QTableWidget::item:selected { background: #1a2940; }");
    connect(table_, &QTableWidget::cellClicked, this, &CryptoWatchlist::on_cell_clicked);
    layout->addWidget(table_, 1);

    setFixedWidth(200);
}

void CryptoWatchlist::set_symbols(const QStringList& symbols) {
    QMutexLocker lock(&mutex_);
    symbols_ = symbols;
    entries_.resize(symbols.size());
    for (int i = 0; i < symbols.size(); ++i) {
        entries_[i].symbol = symbols[i];
    }
    lock.unlock();
    rebuild_table();
}

void CryptoWatchlist::update_prices(const QVector<trading::TickerData>& tickers) {
    QMutexLocker lock(&mutex_);
    int loaded = 0;
    for (auto& entry : entries_) {
        for (const auto& t : tickers) {
            if (t.symbol == entry.symbol) {
                entry.price = t.last;
                entry.change_pct = t.percentage;
                entry.volume = t.base_volume;
                entry.has_data = true;
                loaded++;
                break;
            }
        }
    }
    lock.unlock();

    count_label_->setText(QString("%1/%2").arg(loaded).arg(entries_.size()));
    rebuild_table();
}

void CryptoWatchlist::set_search_results(const QVector<trading::MarketInfo>& markets) {
    QMutexLocker lock(&mutex_);
    search_results_ = markets;
}

void CryptoWatchlist::on_cell_clicked(int row, int /*col*/) {
    auto* item = table_->item(row, 0);
    if (item) emit symbol_selected(item->text());
}

void CryptoWatchlist::on_filter_changed(const QString& text) {
    QString filter = text.trimmed().toUpper();

    if (filter.length() >= 2) {
        showing_search_ = true;
        emit search_requested(filter);
    } else {
        showing_search_ = false;
    }
    rebuild_table();
}

void CryptoWatchlist::rebuild_table() {
    QMutexLocker lock(&mutex_);
    QString filter = filter_edit_->text().trimmed().toUpper();

    // If searching and have results, show search results
    if (showing_search_ && !search_results_.isEmpty()) {
        QVector<trading::MarketInfo> filtered;
        for (const auto& m : search_results_) {
            if (m.symbol.toUpper().contains(filter)) {
                filtered.append(m);
                if (filtered.size() >= 50) break;
            }
        }

        table_->setRowCount(filtered.size());
        for (int i = 0; i < filtered.size(); ++i) {
            table_->setItem(i, 0, new QTableWidgetItem(filtered[i].symbol));
            table_->setItem(i, 1, new QTableWidgetItem(filtered[i].type));
            table_->setItem(i, 2, new QTableWidgetItem("--"));
        }
        return;
    }

    // Normal watchlist
    QVector<WatchlistEntry> visible;
    for (const auto& e : entries_) {
        if (filter.isEmpty() || e.symbol.toUpper().contains(filter)) {
            visible.append(e);
        }
    }

    table_->setRowCount(visible.size());
    for (int i = 0; i < visible.size(); ++i) {
        const auto& e = visible[i];
        table_->setItem(i, 0, new QTableWidgetItem(e.symbol));

        auto* price_item = new QTableWidgetItem(
            e.has_data ? QString::number(e.price, 'f', 2) : "--");
        table_->setItem(i, 1, price_item);

        auto* chg_item = new QTableWidgetItem(
            e.has_data ? QString("%1%").arg(e.change_pct, 0, 'f', 2) : "--");
        chg_item->setForeground(e.change_pct >= 0 ? QColor("#00ff88") : QColor("#ff4444"));
        table_->setItem(i, 2, chg_item);
    }
}

} // namespace fincept::screens::crypto
