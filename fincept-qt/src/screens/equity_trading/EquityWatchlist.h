#pragma once
// Equity Watchlist — compact with change% coloring

#include "trading/TradingTypes.h"

#include <QCompleter>
#include <QLabel>
#include <QLineEdit>
#include <QMutex>
#include <QPushButton>
#include <QStringListModel>
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
    void set_broker_id(const QString& broker_id);
    void add_symbol(const QString& symbol);

  signals:
    void symbol_selected(const QString& symbol);
    void symbol_added(const QString& symbol); // emitted when user adds a new symbol

  private slots:
    void on_cell_clicked(int row, int col);
    void on_filter_changed(const QString& text);
    void on_add_symbol_entered();
    void on_add_text_changed(const QString& text);

  private:
    void rebuild_table();

    QLineEdit* filter_edit_ = nullptr;
    QLineEdit* add_edit_ = nullptr; // symbol add input
    QPushButton* add_btn_ = nullptr;
    QStringListModel* completer_model_ = nullptr;
    QCompleter* completer_ = nullptr;
    QLabel* count_label_ = nullptr;
    QTableWidget* table_ = nullptr;

    QVector<WatchlistEntry> entries_;
    QString active_symbol_;
    QString broker_id_;
    QMutex mutex_;
};

} // namespace fincept::screens::equity
