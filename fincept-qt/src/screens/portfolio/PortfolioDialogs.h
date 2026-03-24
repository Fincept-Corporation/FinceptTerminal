// src/screens/portfolio/PortfolioDialogs.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QString>

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
class AddAssetDialog : public QDialog {
    Q_OBJECT
  public:
    explicit AddAssetDialog(QWidget* parent = nullptr);

    QString symbol() const;
    double quantity() const;
    double price() const;

  private:
    QLineEdit* symbol_edit_ = nullptr;
    QLineEdit* quantity_edit_ = nullptr;
    QLineEdit* price_edit_ = nullptr;
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
    QLineEdit* date_edit_ = nullptr;
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
