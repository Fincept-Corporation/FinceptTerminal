#pragma once
// Crypto Watchlist — compact Bloomberg-style with volume bars and active symbol highlight

#include "trading/TradingTypes.h"

#include <QLabel>
#include <QLineEdit>
#include <QMutex>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QVector>
#include <QWidget>

namespace fincept::screens::crypto {

// Custom delegate: paints a proportional volume bar behind text in column 3
class WatchlistVolumeDelegate : public QStyledItemDelegate {
    Q_OBJECT
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void set_max_volume(double v) { max_volume_ = v; }

  private:
    double max_volume_ = 1.0;
};

struct WatchlistEntry {
    QString symbol;
    double price = 0.0;
    double change_pct = 0.0;
    double volume = 0.0;
    bool has_data = false;
};

class CryptoWatchlist : public QWidget {
    Q_OBJECT
  public:
    explicit CryptoWatchlist(QWidget* parent = nullptr);

    void set_symbols(const QStringList& symbols);
    void update_prices(const QVector<trading::TickerData>& tickers);
    void set_search_results(const QVector<trading::MarketInfo>& markets);
    void set_active_symbol(const QString& symbol);

  signals:
    void symbol_selected(const QString& symbol);
    void search_requested(const QString& filter);

  private slots:
    void on_cell_clicked(int row, int col);
    void on_filter_changed(const QString& text);

  private:
    void rebuild_table();

    QLineEdit* filter_edit_ = nullptr;
    QLabel* count_label_ = nullptr;
    QTableWidget* table_ = nullptr;
    WatchlistVolumeDelegate* vol_delegate_ = nullptr;

    QVector<WatchlistEntry> entries_;
    QVector<trading::MarketInfo> search_results_;
    QString active_symbol_;
    bool showing_search_ = false;
    QMutex mutex_;
};

} // namespace fincept::screens::crypto
