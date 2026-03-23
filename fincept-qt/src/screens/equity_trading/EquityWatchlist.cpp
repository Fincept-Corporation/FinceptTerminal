// EquityWatchlist.cpp — compact watchlist with live quote updates
#include "screens/equity_trading/EquityWatchlist.h"
#include "screens/equity_trading/EquityTypes.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QMutexLocker>
#include <QVBoxLayout>

namespace fincept::screens::equity {

EquityWatchlist::EquityWatchlist(QWidget* parent) : QWidget(parent) {
    setObjectName("eqWatchlist");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header
    auto* header = new QWidget;
    header->setObjectName("eqWatchlistHeader");
    header->setFixedHeight(28);
    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(8, 0, 8, 0);

    auto* title = new QLabel("WATCHLIST");
    title->setObjectName("eqWatchlistTitle");
    h_layout->addWidget(title);
    h_layout->addStretch();

    count_label_ = new QLabel("0");
    count_label_->setObjectName("eqWatchlistCount");
    h_layout->addWidget(count_label_);

    layout->addWidget(header);

    // Search
    filter_edit_ = new QLineEdit;
    filter_edit_->setObjectName("eqWatchlistSearch");
    filter_edit_->setPlaceholderText("Filter...");
    filter_edit_->setFixedHeight(26);
    connect(filter_edit_, &QLineEdit::textChanged, this, &EquityWatchlist::on_filter_changed);
    layout->addWidget(filter_edit_);

    // Table
    table_ = new QTableWidget;
    table_->setObjectName("eqWatchlistTable");
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels({"SYMBOL", "LTP", "CHG%"});
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    table_->setColumnWidth(1, 80);
    table_->setColumnWidth(2, 60);
    table_->verticalHeader()->setDefaultSectionSize(24);

    connect(table_, &QTableWidget::cellClicked, this, &EquityWatchlist::on_cell_clicked);
    layout->addWidget(table_, 1);
}

void EquityWatchlist::set_symbols(const QStringList& symbols) {
    QMutexLocker lock(&mutex_);
    entries_.clear();
    entries_.reserve(symbols.size());
    for (const auto& sym : symbols) {
        WatchlistEntry e;
        e.symbol = sym;
        entries_.append(e);
    }
    count_label_->setText(QString::number(entries_.size()));
    rebuild_table();
}

void EquityWatchlist::update_quotes(const QVector<trading::BrokerQuote>& quotes) {
    QMutexLocker lock(&mutex_);
    for (const auto& q : quotes) {
        for (auto& e : entries_) {
            if (e.symbol == q.symbol) {
                e.ltp = q.ltp;
                e.change_pct = q.change_pct;
                e.volume = q.volume;
                e.has_data = true;
                break;
            }
        }
    }
    // Update visible rows without full rebuild
    for (int row = 0; row < table_->rowCount(); ++row) {
        auto* sym_item = table_->item(row, 0);
        if (!sym_item) continue;
        const QString sym = sym_item->data(Qt::UserRole).toString();
        for (const auto& e : entries_) {
            if (e.symbol == sym && e.has_data) {
                auto* ltp_item = table_->item(row, 1);
                auto* chg_item = table_->item(row, 2);
                if (ltp_item)
                    ltp_item->setText(QString::number(e.ltp, 'f', 2));
                if (chg_item) {
                    chg_item->setText(QString("%1%2%")
                                         .arg(e.change_pct >= 0 ? "+" : "")
                                         .arg(e.change_pct, 0, 'f', 2));
                    chg_item->setForeground(e.change_pct >= 0 ? COLOR_BUY : COLOR_SELL);
                }
                break;
            }
        }
    }
}

void EquityWatchlist::set_active_symbol(const QString& symbol) {
    active_symbol_ = symbol;
    for (int row = 0; row < table_->rowCount(); ++row) {
        auto* item = table_->item(row, 0);
        if (!item) continue;
        const bool active = item->data(Qt::UserRole).toString() == symbol;
        for (int col = 0; col < 3; ++col) {
            auto* cell = table_->item(row, col);
            if (cell) {
                cell->setBackground(active ? BG_HOVER : BG_BASE);
            }
        }
    }
}

void EquityWatchlist::on_cell_clicked(int row, int /*col*/) {
    auto* item = table_->item(row, 0);
    if (item) {
        const QString symbol = item->data(Qt::UserRole).toString();
        if (!symbol.isEmpty()) {
            set_active_symbol(symbol);
            emit symbol_selected(symbol);
        }
    }
}

void EquityWatchlist::on_filter_changed(const QString& text) {
    const QString filter = text.trimmed().toUpper();
    for (int row = 0; row < table_->rowCount(); ++row) {
        auto* item = table_->item(row, 0);
        if (!item) continue;
        const bool match = filter.isEmpty() ||
                           item->data(Qt::UserRole).toString().contains(filter);
        table_->setRowHidden(row, !match);
    }
}

void EquityWatchlist::rebuild_table() {
    table_->setRowCount(entries_.size());
    for (int i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];

        auto* sym_item = new QTableWidgetItem(e.symbol);
        sym_item->setData(Qt::UserRole, e.symbol);
        sym_item->setForeground(TEXT_PRIMARY);
        table_->setItem(i, 0, sym_item);

        auto* ltp_item = new QTableWidgetItem(e.has_data ? QString::number(e.ltp, 'f', 2) : "--");
        ltp_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        ltp_item->setForeground(TEXT_PRIMARY);
        table_->setItem(i, 1, ltp_item);

        auto* chg_item = new QTableWidgetItem(
            e.has_data ? QString("%1%2%").arg(e.change_pct >= 0 ? "+" : "").arg(e.change_pct, 0, 'f', 2) : "--");
        chg_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        chg_item->setForeground(e.change_pct >= 0 ? COLOR_BUY : COLOR_SELL);
        table_->setItem(i, 2, chg_item);
    }
}

} // namespace fincept::screens::equity
