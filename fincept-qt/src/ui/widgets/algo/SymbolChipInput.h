// src/ui/widgets/algo/SymbolChipInput.h
#pragma once
#include <QFrame>
#include <QHash>
#include <QStringList>
#include <QVector>

class QLineEdit;
class QCompleter;
class QStringListModel;

namespace fincept::ui {
class FlowLayout;
}

namespace fincept::ui::algo {

/// A bordered box that holds removable symbol "chips" plus an inline search field
/// with instrument autocomplete. Tab/Enter/click commits a suggestion; the live
/// price is fetched async and shown on the chip. Reusable across panels.
/// Styling: object names of the Algo kit root stylesheet (the box #algoPanel, each chip a
/// QLabel#algoChip + a QPushButton#algoLink "✕", the suggestion list #algoList) — no
/// setStyleSheet; chips wrap with the shared ui::FlowLayout.
class SymbolChipInput : public QFrame {
    Q_OBJECT
  public:
    explicit SymbolChipInput(QWidget* parent = nullptr);

    QStringList symbols() const;               // current chips, in order
    void set_symbols(const QStringList& syms); // replace all chips
    void clear();

  signals:
    void symbols_changed();
    void price_resolved(const QString& symbol, double price); // emitted when a chip's quote arrives

  protected:
    bool eventFilter(QObject* obj, QEvent* ev) override; // Tab-to-complete on the line edit

  private:
    void commit_text(const QString& raw);    // add one-or-many raw (comma/space/newline split)
    void commit_label(const QString& label); // resolve a picked suggestion label → real symbol
    void add_chip(const QString& symbol);
    /// The chip alone (no focus, no signal, no quote) — add_chip / set_symbols finish the job.
    bool insert_chip(const QString& symbol);
    void remove_chip(const QString& symbol);
    void on_text_changed(const QString& text);
    void fetch_price(const QString& symbol);
    /// One quote request for several chips (set_symbols of a whole list).
    void fetch_prices(const QStringList& symbols);

    fincept::ui::FlowLayout* flow_ = nullptr;
    QLineEdit* edit_ = nullptr;
    QCompleter* completer_ = nullptr;
    QStringListModel* model_ = nullptr;
    QHash<QString, QString> sugg_map_; // suggestion label → real symbol
    QStringList symbols_;              // source of truth, upper-cased
    QVector<QWidget*> chips_;          // parallel to symbols_
};

} // namespace fincept::ui::algo
