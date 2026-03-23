#pragma once
// Equity Watchlist — compact Bloomberg-style with change% coloring

#include "trading/TradingTypes.h"

#include <QLabel>
#include <QLineEdit>
#include <QMutex>
#include <QTableWidget>
#include <QVector>
#include <QWidget>

namespace fincept::screens::equity {

struct WatchlistEntry {
    QString symbol;
    double ltp = 0.0;
    double change_pct = 0.0;
    double volume = 0.0;
    bool has_data = false;
};

class EquityWatchlist : public QWidget {
    Q_OBJECT
  public:
    explicit EquityWatchlist(QWidget* parent = nullptr);

    void set_symbols(const QStringList& symbols);
    void update_quotes(const QVector<trading::BrokerQuote>& quotes);
    void set_active_symbol(const QString& symbol);

  signals:
    void symbol_selected(const QString& symbol);

  private slots:
    void on_cell_clicked(int row, int col);
    void on_filter_changed(const QString& text);

  private:
    void rebuild_table();

    QLineEdit* filter_edit_ = nullptr;
    QLabel* count_label_ = nullptr;
    QTableWidget* table_ = nullptr;

    QVector<WatchlistEntry> entries_;
    QString active_symbol_;
    QMutex mutex_;
};

} // namespace fincept::screens::equity
