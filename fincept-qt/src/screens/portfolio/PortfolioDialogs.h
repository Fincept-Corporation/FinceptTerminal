// src/screens/portfolio/PortfolioDialogs.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"
#include "services/markets/MarketSearchService.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QString>
#include <QTimer>

namespace fincept::screens {

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

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void changeEvent(QEvent* event) override;

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
    QPushButton* cancel_btn_ = nullptr;
    QPushButton* add_btn_ = nullptr;

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

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();

    QString symbol_;
    double held_qty_ = 0;
    QLineEdit* quantity_edit_ = nullptr;
    QLineEdit* price_edit_ = nullptr;
    QLabel* title_label_ = nullptr;
    QLabel* held_label_ = nullptr;
    QLabel* quantity_row_label_ = nullptr;
    QLabel* price_row_label_ = nullptr;
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

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();

    QString txn_type_;
    QString txn_symbol_;
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

/// Dialog for mapping symbols to sectors.
class SectorMappingDialog : public QDialog {
    Q_OBJECT
  public:
    explicit SectorMappingDialog(const QVector<portfolio::HoldingWithQuote>& holdings, QWidget* parent = nullptr);

    QHash<QString, QString> sector_map() const;

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();

    QHash<QString, QComboBox*> combos_;
    QLabel* title_label_ = nullptr;
    QLabel* desc_label_ = nullptr;
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

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();

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
