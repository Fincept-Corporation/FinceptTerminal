// src/screens/portfolio/PortfolioDialogs.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
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

  private:
    QLineEdit* name_edit_ = nullptr;
    QLineEdit* owner_edit_ = nullptr;
    QComboBox* currency_cb_ = nullptr;
};

/// Confirmation dialog for deleting a portfolio.
class ConfirmDeleteDialog : public QDialog {
    Q_OBJECT
  public:
    explicit ConfirmDeleteDialog(const QString& portfolio_name, QWidget* parent = nullptr);
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

  private:
    void schedule_search(const QString& query);
    void fire_search(const QString& query);
    void show_results(const QJsonArray& results);
    void select_result(const QString& symbol);
    void position_dropdown();

    QLineEdit* symbol_edit_ = nullptr;
    QLineEdit* quantity_edit_ = nullptr;
    QLineEdit* price_edit_ = nullptr;

    // Ticker search dropdown (parented to dialog, floats over form)
    QFrame* search_frame_ = nullptr;
    QListWidget* search_list_ = nullptr;
    QTimer* search_debounce_ = nullptr;
    QString pending_query_;
    bool selecting_ = false; // guard against recursive text-changed
};

/// Dialog for selling an asset.
class SellAssetDialog : public QDialog {
    Q_OBJECT
  public:
    explicit SellAssetDialog(const QString& symbol, double held_qty, QWidget* parent = nullptr);

    double quantity() const;
    double price() const;

  private:
    QLineEdit* quantity_edit_ = nullptr;
    QLineEdit* price_edit_ = nullptr;
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

  private:
    QLineEdit* quantity_edit_ = nullptr;
    QLineEdit* price_edit_ = nullptr;
    QDateEdit* date_edit_ = nullptr;
    QLineEdit* notes_edit_ = nullptr;
};

/// Dialog for mapping symbols to sectors.
class SectorMappingDialog : public QDialog {
    Q_OBJECT
  public:
    explicit SectorMappingDialog(const QVector<portfolio::HoldingWithQuote>& holdings, QWidget* parent = nullptr);

    QHash<QString, QString> sector_map() const;

  private:
    QHash<QString, QComboBox*> combos_;
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

  private:
    QComboBox* symbol_cb_ = nullptr;
    QLineEdit* amount_edit_ = nullptr;
    QDateEdit* date_edit_ = nullptr;
    QLineEdit* notes_edit_ = nullptr;
};

/// Dialog for importing a portfolio from JSON file.
class ImportPortfolioDialog : public QDialog {
    Q_OBJECT
  public:
    explicit ImportPortfolioDialog(const QVector<portfolio::Portfolio>& portfolios, QWidget* parent = nullptr);

    QString file_path() const;
    portfolio::ImportMode mode() const;
    QString merge_target_id() const;

  private:
    void browse_file();

    QLineEdit* file_edit_ = nullptr;
    QRadioButton* new_radio_ = nullptr;
    QRadioButton* merge_radio_ = nullptr;
    QComboBox* target_cb_ = nullptr;
    QLabel* status_label_ = nullptr;
};

} // namespace fincept::screens
