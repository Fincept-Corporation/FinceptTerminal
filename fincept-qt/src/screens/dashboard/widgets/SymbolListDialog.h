#pragma once
// SymbolListDialog — edit a dashboard tile's symbol list with live symbol search
// (MarketSearchService, debounced) instead of a free-text box. Rows can be
// reordered, relabelled and removed; "Region defaults" restores the list the
// current MarketProfile gives the tile. With max_count == 1 it is a single
// symbol picker (stock quote, candle chart).

#include "screens/dashboard/widgets/QuoteGrid.h"

#include <QDialog>
#include <QVector>

#include <functional>

class QLineEdit;
class QListWidget;
class QTableWidget;
class QTimer;
class QLabel;

namespace fincept::screens::widgets {

class SymbolListDialog : public QDialog {
    Q_OBJECT
  public:
    using DefaultsFn = std::function<QVector<QuoteGridEntry>()>;

    /// `defaults` may be empty (no "Region defaults" button).
    SymbolListDialog(const QString& title, const QVector<QuoteGridEntry>& current, DefaultsFn defaults,
                     QWidget* parent = nullptr, int max_count = 60);

    QVector<QuoteGridEntry> entries() const;
    /// The user restored the region defaults and did not edit the list after.
    bool follows_defaults() const { return follows_defaults_; }

  private:
    void run_search();
    void add_entry(const QString& symbol, const QString& label);
    void fill(const QVector<QuoteGridEntry>& list);
    void move_selected(int delta);

    DefaultsFn defaults_;
    int max_count_ = 60;
    bool follows_defaults_ = false;
    bool filling_ = false;
    QString pending_query_;

    QLineEdit* search_ = nullptr;
    QListWidget* results_ = nullptr;
    QTableWidget* list_ = nullptr;
    QLabel* status_ = nullptr;
    QTimer* debounce_ = nullptr;
};

} // namespace fincept::screens::widgets
