// CryptoWatchlist.cpp — compact watchlist, 3-column, no horizontal scroll
#include "screens/crypto_trading/CryptoWatchlist.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QMutexLocker>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens::crypto {

using namespace fincept::ui;

// ── Color accessors (theme-aware) ───────────────────────────────────────────
namespace {
inline QColor kColorPrimary()   { return QColor(colors::TEXT_PRIMARY()); }
inline QColor kColorDim()       { return QColor(colors::TEXT_DIM()); }
inline QColor kColorSecondary() { return QColor(colors::TEXT_SECONDARY()); }
inline QColor kColorPos()       { return QColor(colors::POSITIVE()); }
inline QColor kColorNeg()       { return QColor(colors::NEGATIVE()); }
inline QColor kColorAmber()     { return QColor(colors::AMBER()); }
inline QColor kRowEven()        { return QColor(colors::BG_BASE()); }
inline QColor kRowOdd()         { return QColor(colors::ROW_ALT()); }
const QColor kRowPosHint(22, 163, 74, 12);
const QColor kRowNegHint(220, 38, 38, 12);
} // namespace

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

    // Table — 3 columns: Symbol | Price | Chg%
    // No horizontal scrollbar — all 3 columns fit the watchlist width.
    table_ = new QTableWidget;
    table_->setObjectName("cryptoWatchlistTable");
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels({"Symbol", "Price", "%"});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    table_->setColumnWidth(2, 62);
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->hide();
    table_->setShowGrid(false);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->verticalHeader()->setDefaultSectionSize(22);

    connect(table_, &QTableWidget::cellClicked, this, &CryptoWatchlist::on_cell_clicked);
    layout->addWidget(table_, 1);
}

void CryptoWatchlist::set_symbols(const QStringList& symbols) {
    {
        QMutexLocker lock(&mutex_);
        entries_.resize(symbols.size());
        for (int i = 0; i < symbols.size(); ++i) {
            entries_[i].symbol = symbols[i];
            entries_[i].price = 0.0;
            entries_[i].change_pct = 0.0;
            entries_[i].has_data = false;
        }
    }
    rebuild_table();
}

void CryptoWatchlist::set_active_symbol(const QString& symbol) {
    active_symbol_ = symbol;
    rebuild_table();
}

void CryptoWatchlist::update_prices(const QVector<trading::TickerData>& tickers) {
    if (showing_search_)
        return;

    QHash<QString, const trading::TickerData*> ticker_map;
    ticker_map.reserve(tickers.size());
    for (const auto& t : tickers)
        ticker_map.insert(t.symbol, &t);

    int loaded = 0;
    bool any_new = false;
    {
        QMutexLocker lock(&mutex_);
        for (auto& entry : entries_) {
            auto it = ticker_map.find(entry.symbol);
            if (it == ticker_map.end())
                continue;
            const auto* t = *it;
            if (t->last <= 0.0)
                continue;

            const bool was_loaded = entry.has_data;
            entry.price = t->last;
            // Preserve last known non-zero change_pct — OB-derived tickers
            // have percentage=0 (no 24h stats), so only update when we get a real value.
            if (t->percentage != 0.0)
                entry.change_pct = t->percentage;
            entry.has_data = true;
            if (!was_loaded)
                any_new = true;
            ++loaded;
        }
    }

    count_label_->setText(QString("%1/%2").arg(loaded).arg(entries_.size()));

    // If new symbols just got their first tick, do a full structural rebuild.
    // Otherwise fast-patch in-place.
    const QString filter = filter_edit_->text().trimmed().toUpper();
    if (any_new || !filter.isEmpty() || table_->rowCount() != static_cast<int>(entries_.size())) {
        rebuild_table();
        return;
    }

    // Fast-path: patch visible items in-place
    table_->setUpdatesEnabled(false);
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        const auto& e = entries_[i];
        if (!e.has_data)
            continue;

        auto* sym_item   = table_->item(i, 0);
        auto* price_item = table_->item(i, 1);
        auto* chg_item   = table_->item(i, 2);
        if (!sym_item || !price_item || !chg_item)
            continue;

        // Format price — fewer decimals for large prices to save width
        QString price_str;
        if (e.price >= 1000.0)
            price_str = QString::number(e.price, 'f', 2);
        else if (e.price >= 1.0)
            price_str = QString::number(e.price, 'f', 4);
        else
            price_str = QString::number(e.price, 'f', 6);

        price_item->setText(price_str);
        price_item->setForeground(kColorPrimary());

        chg_item->setText(QString("%1%").arg(e.change_pct, 0, 'f', 2));
        chg_item->setForeground(e.change_pct >= 0 ? kColorPos() : kColorNeg());

        QColor bg = (i % 2 == 0) ? kRowEven() : kRowOdd();
        if (e.change_pct > 0.0)  bg = kRowPosHint;
        else if (e.change_pct < 0.0) bg = kRowNegHint;

        sym_item->setForeground(e.symbol == active_symbol_ ? kColorAmber() : kColorPrimary());
        for (int c = 0; c < 3; ++c) {
            auto* it = table_->item(i, c);
            if (it) it->setBackground(bg);
        }
    }
    table_->setUpdatesEnabled(true);
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

// ── rebuild_table ────────────────────────────────────────────────────────────
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
            const QColor bg = (i % 2 == 0) ? kRowEven() : kRowOdd();
            auto ensure = [&](int col, const QString& text, const QColor& fg,
                              int align = Qt::AlignLeft | Qt::AlignVCenter) {
                auto* it = table_->item(i, col);
                if (!it) { it = new QTableWidgetItem; table_->setItem(i, col, it); }
                it->setText(text);
                it->setForeground(fg);
                it->setBackground(bg);
                it->setTextAlignment(align);
            };
            ensure(0, filtered[i].symbol, kColorPrimary());
            ensure(1, filtered[i].type,   kColorSecondary());
            ensure(2, "--", kColorDim(), Qt::AlignRight | Qt::AlignVCenter);
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
        QColor bg = (i % 2 == 0) ? kRowEven() : kRowOdd();
        if (e.has_data && e.change_pct > 0.0) bg = kRowPosHint;
        else if (e.has_data && e.change_pct < 0.0) bg = kRowNegHint;

        auto ensure = [&](int col, const QString& text, const QColor& fg,
                          int align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = table_->item(i, col);
            if (!it) { it = new QTableWidgetItem; table_->setItem(i, col, it); }
            it->setText(text);
            it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };

        QColor sym_color = (e.symbol == active_symbol_) ? kColorAmber() : kColorPrimary();
        ensure(0, e.symbol, sym_color);

        // Price — adaptive decimal places
        QString price_str = "--";
        if (e.has_data) {
            if (e.price >= 1000.0)
                price_str = QString::number(e.price, 'f', 2);
            else if (e.price >= 1.0)
                price_str = QString::number(e.price, 'f', 4);
            else
                price_str = QString::number(e.price, 'f', 6);
        }
        ensure(1, price_str, e.has_data ? kColorPrimary() : kColorDim(),
               Qt::AlignRight | Qt::AlignVCenter);

        ensure(2,
               e.has_data ? QString("%1%").arg(e.change_pct, 0, 'f', 2) : QString("--"),
               e.has_data ? (e.change_pct >= 0.0 ? kColorPos() : kColorNeg()) : kColorDim(),
               Qt::AlignRight | Qt::AlignVCenter);
    }
    table_->setUpdatesEnabled(true);
}

} // namespace fincept::screens::crypto
