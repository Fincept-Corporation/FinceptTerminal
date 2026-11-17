// src/screens/portfolio/PortfolioTxnPanel.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace fincept::screens {

/// Transaction history panel — the WHOLE ledger of the selected portfolio (no row
/// cap), every transaction type, with fees, taxes, currency, trade FX and the net
/// cash effect of each row, a type filter, a text filter and CSV export.
class PortfolioTxnPanel : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioTxnPanel(QWidget* parent = nullptr);

    /// Populate with the ledger (any order; shown newest first).
    void set_transactions(const QVector<portfolio::Transaction>& txns);
    /// Base currency: the currency of cash rows recorded without one.
    void set_base_currency(const QString& ccy);

    /// Clear the table (e.g. when portfolio changes).
    void clear();
    void refresh_theme();

  protected:
    void changeEvent(QEvent* event) override;

  signals:
    /// Emitted when the user toggles the collapse chevron in the header.
    void collapse_toggled(bool collapsed);
    /// Double-click on a transaction row: the owner opens that ticker elsewhere.
    void symbol_activated(QString symbol);
    /// Row menu "Open in …" (screen id, ticker) — published as nav.open_symbol.
    void open_symbol_requested(QString screen_id, QString symbol);
    /// Row menu edit / delete of a ledger row, by transaction id.
    void edit_transaction_requested(QString transaction_id);
    void delete_transaction_requested(QString transaction_id);
    /// Row menu "Export Full Ledger (CSV)".
    void export_requested();

  public:
    /// Kept for source compatibility: the panel shows the whole ledger now.
    static constexpr int kRecentRows = 1000000;

  private:
    void build_ui();
    void populate();
    void apply_collapsed_state();
    void apply_styles();
    void retranslateUi();
    void export_visible();

    QLabel* title_label_ = nullptr;
    QLabel* count_label_ = nullptr;
    QComboBox* type_cb_ = nullptr;
    QLineEdit* search_edit_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    QPushButton* collapse_btn_ = nullptr;
    QTableWidget* table_ = nullptr;
    bool collapsed_ = false;
    bool sized_ = false;
    QString base_ccy_; ///< portfolio base currency; empty until set_base_currency()

    QVector<portfolio::Transaction> txns_;
};

} // namespace fincept::screens
