#pragma once
// Equity Watchlist — compact with change% coloring

#include "trading/TradingTypes.h"

#include <QComboBox>
#include <QCompleter>
#include <QEvent>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QMutex>
#include <QPair>
#include <QPushButton>
#include <QStringListModel>
#include <QTableWidget>
#include <QToolButton>
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
    // Patch a single symbol's row in place. No-op if the symbol isn't tracked.
    // Per-tick hot path — must stay O(entries + rows), never a full rebuild.
    void update_quote(const trading::BrokerQuote& quote);
    void set_active_symbol(const QString& symbol);
    void set_broker_id(const QString& broker_id);
    void add_symbol(const QString& symbol);

    // Populate the watchlist switcher. `lists` is (id, name) pairs; `active_id`
    // selects the current entry. Programmatic — does not emit watchlist_selected.
    void set_watchlists(const QVector<QPair<QString, QString>>& lists, const QString& active_id);

  signals:
    void symbol_selected(const QString& symbol);
    void symbol_added(const QString& symbol);    // user added a symbol
    void symbol_removed(const QString& symbol);  // user removed a row from the active list
    void watchlist_selected(const QString& id);  // user picked another list in the combo
    void watchlist_create_requested(const QString& name);
    void watchlist_rename_requested(const QString& id, const QString& name);
    void watchlist_delete_requested(const QString& id);

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_cell_clicked(int row, int col);
    void on_filter_changed(const QString& text);
    void on_add_symbol_entered();
    void on_add_text_changed(const QString& text);
    void on_watchlist_combo_activated(int index);
    void on_watchlist_menu();
    void on_table_context_menu(const QPoint& pos);

  private:
    void rebuild_table();
    void retranslateUi();

    QComboBox* wl_combo_ = nullptr;     // named-watchlist switcher
    QToolButton* wl_menu_btn_ = nullptr; // New / Rename / Delete menu
    QLineEdit* filter_edit_ = nullptr;
    QLineEdit* add_edit_ = nullptr; // symbol add input
    QPushButton* add_btn_ = nullptr;
    QStringListModel* completer_model_ = nullptr;
    QCompleter* completer_ = nullptr;
    // Friendly picker label → canonical symbol (F&O clean names in the add box).
    QHash<QString, QString> add_suggestion_map_;
    QLabel* title_label_ = nullptr;
    QLabel* count_label_ = nullptr;
    QTableWidget* table_ = nullptr;

    QVector<WatchlistEntry> entries_;
    QString active_symbol_;
    QString broker_id_;
    QMutex mutex_;
};

} // namespace fincept::screens::equity
