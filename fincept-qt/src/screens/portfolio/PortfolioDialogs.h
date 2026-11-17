// src/screens/portfolio/PortfolioDialogs.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"
#include "services/markets/MarketSearchService.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QEvent>
#include <QFrame>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QMoveEvent>
#include <QString>
#include <QTimer>

namespace fincept::screens {

class TradeCostsBox; // PortfolioLedgerDialogs.h

/// Dialog for creating a new portfolio.
class CreatePortfolioDialog : public QDialog {
    Q_OBJECT
  public:
    explicit CreatePortfolioDialog(QWidget* parent = nullptr);

    QString name() const;
    QString owner() const;
    QString currency() const;

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();

    QLineEdit* name_edit_ = nullptr;
    QLineEdit* owner_edit_ = nullptr;
    QComboBox* currency_cb_ = nullptr;
    QLabel* title_label_ = nullptr;
    QLabel* name_row_label_ = nullptr;
    QLabel* owner_row_label_ = nullptr;
    QLabel* currency_row_label_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
    QPushButton* create_btn_ = nullptr;
};

/// Confirmation dialog for deleting a portfolio.
class ConfirmDeleteDialog : public QDialog {
    Q_OBJECT
  public:
    explicit ConfirmDeleteDialog(const QString& portfolio_name, QWidget* parent = nullptr);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();

    QString portfolio_name_;
    QLabel* icon_label_ = nullptr;
    QLabel* msg_label_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
};

/// Dialog for adding an asset (BUY).
/// The symbol field has inline search: type a name or ticker and a dropdown
/// appears with matching results fetched from /market/search.
class AddAssetDialog : public QDialog {
    Q_OBJECT
  public:
    explicit AddAssetDialog(QWidget* parent = nullptr);

    QString symbol() const;
    double quantity() const;
    double price() const;
    /// Trade date, YYYY-MM-DD (defaults to today, never in the future).
    QString date() const;

    /// Pre-fill the ticker (and optionally the last price) and jump focus to
    /// the quantity field. Used when BUY is invoked with a position already
    /// selected — the user should not have to retype a symbol the terminal
    /// already knows.
    void preset(const QString& symbol, double price = 0.0);
    /// Portfolio base currency (the costs block's FX field is relative to it).
    void set_base_currency(const QString& base_ccy);
    /// Fees, taxes, trade currency and trade FX.
    TradeCostsBox* costs() const { return costs_; }

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void moveEvent(QMoveEvent* event) override;

  private:
    void retranslateUi();
    void schedule_search(const QString& query);
    void fire_search(const QString& query);
    void show_results(const QList<fincept::services::MarketSearchService::Item>& results);
    void select_result(const QString& symbol);
    void position_dropdown();

    QLineEdit* symbol_edit_ = nullptr;
    QLineEdit* quantity_edit_ = nullptr;
    QLineEdit* price_edit_ = nullptr;
    QLabel* title_label_ = nullptr;
    QLabel* hint_label_ = nullptr;
    QLabel* symbol_row_label_ = nullptr;
    QLabel* quantity_row_label_ = nullptr;
    QLabel* price_row_label_ = nullptr;
    QDateEdit* date_edit_ = nullptr;
    QLabel* date_row_label_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    TradeCostsBox* costs_ = nullptr;
    QString base_ccy_;

    // Ticker search dropdown (parented to dialog, floats over form)
    QFrame* search_frame_ = nullptr;
    QListWidget* search_list_ = nullptr;
    QTimer* search_debounce_ = nullptr;
    QString pending_query_;
    bool selecting_ = false;        // guard against recursive text-changed
    bool search_connected_ = false; // one-shot signal hookup
};

/// Dialog for selling an asset.
class SellAssetDialog : public QDialog {
    Q_OBJECT
  public:
    explicit SellAssetDialog(const QString& symbol, double held_qty, QWidget* parent = nullptr);

    double quantity() const;
    double price() const;
    /// Trade date, YYYY-MM-DD (defaults to today, never in the future).
    QString date() const;
    /// Pre-fill the sale price (e.g. with the last price).
    void preset_price(double price);
    /// Base currency + the instrument's quoted currency for the costs block.
    void set_currency_context(const QString& base_ccy, const QString& instrument_ccy);
    TradeCostsBox* costs() const { return costs_; }

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();

    QString symbol_;
    double held_qty_ = 0;
    TradeCostsBox* costs_ = nullptr;
    QLineEdit* quantity_edit_ = nullptr;
    QLineEdit* price_edit_ = nullptr;
    QLabel* title_label_ = nullptr;
    QLabel* held_label_ = nullptr;
    QLabel* quantity_row_label_ = nullptr;
    QLabel* price_row_label_ = nullptr;
    QDateEdit* date_edit_ = nullptr;
    QLabel* date_row_label_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
    QPushButton* sell_btn_ = nullptr;
};

/// Dialog for editing an existing transaction.
class EditTransactionDialog : public QDialog {
    Q_OBJECT
  public:
    explicit EditTransactionDialog(const portfolio::Transaction& txn, QWidget* parent = nullptr);

    double quantity() const;
    double price() const;
    QString date() const;
    QString notes() const;
    /// Base currency for the costs block (not shown for a SPLIT).
    void set_base_currency(const QString& base_ccy);
    /// nullptr for a SPLIT (no costs).
    TradeCostsBox* costs() const { return costs_; }
    /// True when fees / taxes / currency / FX differ from the row as opened.
    bool costs_changed() const;

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();

    QString txn_type_;
    QString txn_symbol_;
    portfolio::Transaction original_;
    TradeCostsBox* costs_ = nullptr;
    QLineEdit* quantity_edit_ = nullptr;
    QLineEdit* price_edit_ = nullptr;
    QDateEdit* date_edit_ = nullptr;
    QLineEdit* notes_edit_ = nullptr;
    QLabel* title_label_ = nullptr;
    QLabel* quantity_row_label_ = nullptr;
    QLabel* price_row_label_ = nullptr;
    QLabel* date_row_label_ = nullptr;
    QLabel* notes_row_label_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
    QPushButton* save_btn_ = nullptr;
};

/// Dialog for recording a dividend payment for a holding.
class AddDividendDialog : public QDialog {
    Q_OBJECT
  public:
    explicit AddDividendDialog(const QStringList& symbols, QWidget* parent = nullptr);

    QString symbol() const;
    double amount_per_share() const;
    QString date() const;
    QString notes() const;
    /// Units entitled — pre-filled with the ledger quantity held at the start of the
    /// ex-date (editable; falls back to today's holding when the ledger has none).
    double quantity() const;
    /// Ledger + current holdings for the entitlement, and the base currency.
    void set_context(const QString& base_ccy, const QVector<portfolio::Transaction>& ledger,
                     const QHash<QString, double>& held_now, const QHash<QString, QString>& quote_ccy);
    /// Withholding tax (taxes), charges (fees), currency and FX of the payment.
    TradeCostsBox* costs() const { return costs_; }
    /// Pre-select the paying holding.
    void set_symbol(const QString& symbol);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();
    void update_entitlement();

    QVector<portfolio::Transaction> ledger_;
    QHash<QString, double> held_now_;
    QHash<QString, QString> quote_ccy_;
    QLineEdit* qty_edit_ = nullptr;
    QLabel* qty_row_label_ = nullptr;
    QLabel* entitled_label_ = nullptr;
    TradeCostsBox* costs_ = nullptr;
    bool qty_user_edited_ = false;
    QComboBox* symbol_cb_ = nullptr;
    QLineEdit* amount_edit_ = nullptr;
    QDateEdit* date_edit_ = nullptr;
    QLineEdit* notes_edit_ = nullptr;
    QLabel* title_label_ = nullptr;
    QLabel* symbol_row_label_ = nullptr;
    QLabel* amount_row_label_ = nullptr;
    QLabel* date_row_label_ = nullptr;
    QLabel* notes_row_label_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
    QPushButton* record_btn_ = nullptr;
};

/// Dialog for importing a portfolio from JSON file.
class ImportPortfolioDialog : public QDialog {
    Q_OBJECT
  public:
    explicit ImportPortfolioDialog(const QVector<portfolio::Portfolio>& portfolios, QWidget* parent = nullptr);

    QString file_path() const;
    portfolio::ImportMode mode() const;
    QString merge_target_id() const;

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void browse_file();
    void retranslateUi();

    QLineEdit* file_edit_ = nullptr;
    QRadioButton* new_radio_ = nullptr;
    QRadioButton* merge_radio_ = nullptr;
    QComboBox* target_cb_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* title_label_ = nullptr;
    QLabel* demo_hint_label_ = nullptr;
    QLabel* mode_label_ = nullptr;
    QPushButton* browse_btn_ = nullptr;
    QPushButton* demo_dl_btn_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
    QPushButton* import_btn_ = nullptr;
};

} // namespace fincept::screens
