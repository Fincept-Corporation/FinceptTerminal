// CryptoWatchlist.cpp — Bloomberg-style compact watchlist with volume bars
#include "screens/crypto_trading/CryptoWatchlist.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QMutexLocker>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens::crypto {

// ── Static color cache ──────────────────────────────────────────────────────
namespace {
const QColor kColorPrimary("#e5e5e5");
const QColor kColorDim("#404040");
const QColor kColorSecondary("#808080");
const QColor kColorPos("#16a34a");
const QColor kColorNeg("#dc2626");
const QColor kColorAmber("#d97706");
const QColor kRowEven("#080808");
const QColor kRowOdd("#0c0c0c");
const QColor kRowPosHint(22, 163, 74, 8); // 3% green tint
const QColor kRowNegHint(220, 38, 38, 8); // 3% red tint
const QColor kActiveHighlight("#d97706");
} // namespace

// ── Volume delegate ─────────────────────────────────────────────────────────
void WatchlistVolumeDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const {
    // Paint default background first
    QStyledItemDelegate::paint(painter, option, index);

    // Draw volume bar behind text for column 3
    if (index.column() != 3 || max_volume_ <= 0)
        return;

    const double vol = index.data(Qt::UserRole).toDouble();
    if (vol <= 0)
        return;

    const double ratio = std::min(vol / max_volume_, 1.0);
    const int bar_w = static_cast<int>(option.rect.width() * ratio);

    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(128, 128, 128, 30)); // subtle gray bar
    painter->drawRect(option.rect.x(), option.rect.y(), bar_w, option.rect.height());
    painter->restore();
}

// ── Constructor ─────────────────────────────────────────────────────────────
CryptoWatchlist::CryptoWatchlist(QWidget* parent) : QWidget(parent) {
    setObjectName("cryptoWatchlist");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header
    auto* header_widget = new QWidget;
    header_widget->setObjectName("cryptoWatchlistHeader");
    header_widget->setFixedHeight(30);
    auto* header_layout = new QHBoxLayout(header_widget);
    header_layout->setContentsMargins(8, 0, 8, 0);

    auto* header = new QLabel("WATCHLIST");
    header->setObjectName("cryptoWatchlistTitle");
    header_layout->addWidget(header);
    header_layout->addStretch();

    count_label_ = new QLabel("0/0");
    count_label_->setObjectName("cryptoWatchlistCount");
    header_layout->addWidget(count_label_);
    layout->addWidget(header_widget);

    // Search
    filter_edit_ = new QLineEdit;
    filter_edit_->setObjectName("cryptoWatchlistSearch");
    filter_edit_->setPlaceholderText("Search...");
    filter_edit_->setFixedHeight(24);
    connect(filter_edit_, &QLineEdit::textChanged, this, &CryptoWatchlist::on_filter_changed);
    layout->addWidget(filter_edit_);

    // Table — 4 columns: Symbol, Price, Chg%, Vol
    table_ = new QTableWidget;
    table_->setObjectName("cryptoWatchlistTable");
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({"Sym", "Price", "%", "Vol"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->hide();
    table_->setShowGrid(false);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->verticalHeader()->setDefaultSectionSize(22); // compact rows

    // Volume delegate for column 3
    vol_delegate_ = new WatchlistVolumeDelegate(table_);
    table_->setItemDelegateForColumn(3, vol_delegate_);

    connect(table_, &QTableWidget::cellClicked, this, &CryptoWatchlist::on_cell_clicked);
    layout->addWidget(table_, 1);
}

void CryptoWatchlist::set_symbols(const QStringList& symbols) {
    {
        QMutexLocker lock(&mutex_);
        entries_.resize(symbols.size());
        for (int i = 0; i < symbols.size(); ++i)
            entries_[i].symbol = symbols[i];
    }
    rebuild_table();
}

void CryptoWatchlist::set_active_symbol(const QString& symbol) {
    active_symbol_ = symbol;
    // Force visual update on next rebuild or inline patch
    rebuild_table();
}

void CryptoWatchlist::update_prices(const QVector<trading::TickerData>& tickers) {
    if (showing_search_)
        return;

    // O(1) lookup
    QHash<QString, const trading::TickerData*> ticker_map;
    ticker_map.reserve(tickers.size());
    for (const auto& t : tickers)
        ticker_map.insert(t.symbol, &t);

    int loaded = 0;
    double max_vol = 0;
    {
        QMutexLocker lock(&mutex_);
        for (auto& entry : entries_) {
            auto it = ticker_map.find(entry.symbol);
            if (it != ticker_map.end()) {
                entry.price = (*it)->last;
                entry.change_pct = (*it)->percentage;
                entry.volume = (*it)->base_volume;
                entry.has_data = true;
                max_vol = std::max(max_vol, entry.volume);
                ++loaded;
            }
        }
    }

    count_label_->setText(QString("%1/%2").arg(loaded).arg(entries_.size()));
    vol_delegate_->set_max_volume(max_vol > 0 ? max_vol : 1.0);

    // Fast-path: patch in-place if no filter and row count matches
    const QString filter = filter_edit_->text().trimmed().toUpper();
    if (filter.isEmpty() && table_->rowCount() == static_cast<int>(entries_.size())) {
        table_->setUpdatesEnabled(false);
        for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
            const auto& e = entries_[i];
            if (!e.has_data)
                continue;

            auto* price_item = table_->item(i, 1);
            auto* chg_item = table_->item(i, 2);
            auto* vol_item = table_->item(i, 3);
            if (!price_item || !chg_item || !vol_item)
                continue;

            price_item->setText(QString::number(e.price, 'f', 2));
            price_item->setForeground(kColorPrimary);

            chg_item->setText(QString("%1%").arg(e.change_pct, 0, 'f', 2));
            chg_item->setForeground(e.change_pct >= 0 ? kColorPos : kColorNeg);

            // Volume with UserRole for delegate bar
            QString vol_text;
            if (e.volume >= 1e9)
                vol_text = QString("%1B").arg(e.volume / 1e9, 0, 'f', 1);
            else if (e.volume >= 1e6)
                vol_text = QString("%1M").arg(e.volume / 1e6, 0, 'f', 1);
            else
                vol_text = QString::number(e.volume, 'f', 0);
            vol_item->setText(vol_text);
            vol_item->setData(Qt::UserRole, e.volume);

            // Subtle row tint by direction
            QColor bg = (i % 2 == 0) ? kRowEven : kRowOdd;
            if (e.change_pct > 0)
                bg = kRowPosHint;
            else if (e.change_pct < 0)
                bg = kRowNegHint;

            // Active symbol amber left indicator via first column
            auto* sym_item = table_->item(i, 0);
            if (sym_item) {
                sym_item->setForeground(e.symbol == active_symbol_ ? kColorAmber : kColorPrimary);
            }

            for (int c = 0; c < 4; ++c) {
                auto* it = table_->item(i, c);
                if (it)
                    it->setBackground(bg);
            }
        }
        table_->setUpdatesEnabled(true);
        return;
    }

    rebuild_table();
}

void CryptoWatchlist::set_search_results(const QVector<trading::MarketInfo>& markets) {
    {
        QMutexLocker lock(&mutex_);
        search_results_ = markets;
    }
    if (showing_search_)
        rebuild_table();
}

void CryptoWatchlist::on_cell_clicked(int row, int /*col*/) {
    auto* item = table_->item(row, 0);
    if (item)
        emit symbol_selected(item->text());
}

void CryptoWatchlist::on_filter_changed(const QString& text) {
    const QString filter = text.trimmed().toUpper();
    showing_search_ = (filter.length() >= 2);
    if (showing_search_)
        emit search_requested(filter);
    rebuild_table();
}

// ── rebuild_table: structural rebuild ───────────────────────────────────────
void CryptoWatchlist::rebuild_table() {
    QMutexLocker lock(&mutex_);
    const QString filter = filter_edit_->text().trimmed().toUpper();

    if (showing_search_ && !search_results_.isEmpty()) {
        QVector<trading::MarketInfo> filtered;
        filtered.reserve(50);
        for (const auto& m : search_results_) {
            if (m.symbol.toUpper().contains(filter)) {
                filtered.append(m);
                if (filtered.size() >= 50)
                    break;
            }
        }

        const int n = filtered.size();
        table_->setUpdatesEnabled(false);
        if (table_->rowCount() != n)
            table_->setRowCount(n);

        for (int i = 0; i < n; ++i) {
            const QColor& bg = (i % 2 == 0) ? kRowEven : kRowOdd;
            auto ensure = [&](int col, const QString& text, const QColor& fg,
                              int align = Qt::AlignLeft | Qt::AlignVCenter) {
                auto* it = table_->item(i, col);
                if (!it) {
                    it = new QTableWidgetItem;
                    table_->setItem(i, col, it);
                }
                it->setText(text);
                it->setForeground(fg);
                it->setBackground(bg);
                it->setTextAlignment(align);
            };
            ensure(0, filtered[i].symbol, kColorPrimary);
            ensure(1, filtered[i].type, kColorSecondary);
            ensure(2, "--", kColorDim, Qt::AlignRight | Qt::AlignVCenter);
            ensure(3, "", kColorDim, Qt::AlignRight | Qt::AlignVCenter);
        }
        table_->setUpdatesEnabled(true);
        return;
    }

    // Normal watchlist
    QVector<WatchlistEntry> visible;
    visible.reserve(entries_.size());
    for (const auto& e : entries_) {
        if (filter.isEmpty() || e.symbol.toUpper().contains(filter))
            visible.append(e);
    }

    const int n = visible.size();
    table_->setUpdatesEnabled(false);
    if (table_->rowCount() != n)
        table_->setRowCount(n);

    for (int i = 0; i < n; ++i) {
        const auto& e = visible[i];
        QColor bg = (i % 2 == 0) ? kRowEven : kRowOdd;
        if (e.has_data && e.change_pct > 0)
            bg = kRowPosHint;
        else if (e.has_data && e.change_pct < 0)
            bg = kRowNegHint;

        auto ensure = [&](int col, const QString& text, const QColor& fg,
                          int align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = table_->item(i, col);
            if (!it) {
                it = new QTableWidgetItem;
                table_->setItem(i, col, it);
            }
            it->setText(text);
            it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };

        // Active symbol gets amber text
        QColor sym_color = (e.symbol == active_symbol_) ? kColorAmber : kColorPrimary;
        ensure(0, e.symbol, sym_color);

        ensure(1, e.has_data ? QString::number(e.price, 'f', 2) : QString("--"), e.has_data ? kColorPrimary : kColorDim,
               Qt::AlignRight | Qt::AlignVCenter);

        ensure(2, e.has_data ? QString("%1%").arg(e.change_pct, 0, 'f', 2) : QString("--"),
               (e.change_pct >= 0) ? kColorPos : kColorNeg, Qt::AlignRight | Qt::AlignVCenter);

        // Volume column with UserRole for delegate bar
        QString vol_text = "--";
        if (e.has_data) {
            if (e.volume >= 1e9)
                vol_text = QString("%1B").arg(e.volume / 1e9, 0, 'f', 1);
            else if (e.volume >= 1e6)
                vol_text = QString("%1M").arg(e.volume / 1e6, 0, 'f', 1);
            else
                vol_text = QString::number(e.volume, 'f', 0);
        }
        auto* vol_it = table_->item(i, 3);
        if (!vol_it) {
            vol_it = new QTableWidgetItem;
            table_->setItem(i, 3, vol_it);
        }
        vol_it->setText(vol_text);
        vol_it->setForeground(kColorSecondary);
        vol_it->setBackground(bg);
        vol_it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vol_it->setData(Qt::UserRole, e.volume);
    }
    table_->setUpdatesEnabled(true);
}

} // namespace fincept::screens::crypto
