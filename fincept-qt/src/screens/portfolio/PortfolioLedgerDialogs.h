// src/screens/portfolio/PortfolioLedgerDialogs.h
//
// Ledger input beyond plain BUY / SELL: the fees / taxes / currency / trade-FX
// block embedded in the trade dialogs, cash movements (deposit, withdrawal, fee,
// tax, interest), stock splits and the portfolio's analytics settings. Every
// dialog only collects input; the caller records it through PortfolioService,
// which validates again and reports refusals (operation_failed).
#pragma once

#include "screens/portfolio/PortfolioTypes.h"

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <optional>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTableWidget;
class QToolButton;

namespace fincept::screens {

/// Shared look of the portfolio dialogs (`accent` = title / focus colour).
QString ledger_dialog_qss(const QString& accent);
/// User-typed number: C locale, then the UI locale, then with group separators removed. 0 when unparsable.
double ledger_parse_number(const QString& text);

/// Optional "fees, taxes & currency" block of a trade dialog. Collapsed by default,
/// expanded when the instrument trades in a currency other than the base.
class TradeCostsBox : public QWidget {
    Q_OBJECT
  public:
    explicit TradeCostsBox(QWidget* parent = nullptr);

    /// Base currency of the portfolio and the instrument's quoted currency (minor units kept).
    void set_context(const QString& base_ccy, const QString& instrument_ccy);
    void set_instrument_currency(const QString& instrument_ccy);
    /// Pre-fill (edit dialog). `currency` empty = the instrument's.
    void set_values(double fees, double taxes, const QString& currency, double fx_rate);
    /// Label of the taxes field ("Taxes / stamp duty", "Withholding tax").
    void set_taxes_label(const QString& text);
    void set_expanded(bool expanded);

    double fees() const;
    double taxes() const;
    /// As selected (minor units allowed); empty = the instrument's quoted currency.
    QString currency() const;
    /// Base units per ONE MAJOR unit of currency(); 0 = not recorded (daily history used).
    double fx_rate() const;
    /// False (with the reason) when a field holds an invalid value.
    bool validate(QString* why) const;

  signals:
    void changed();

  private:
    void update_fx_state();
    void retranslate();

    QToolButton* toggle_ = nullptr;
    QWidget* body_ = nullptr;
    QLineEdit* fees_edit_ = nullptr;
    QLineEdit* taxes_edit_ = nullptr;
    QComboBox* ccy_cb_ = nullptr;
    QLineEdit* fx_edit_ = nullptr;
    QLabel* fees_label_ = nullptr;
    QLabel* taxes_label_ = nullptr;
    QLabel* ccy_label_ = nullptr;
    QLabel* fx_label_ = nullptr;
    QLabel* hint_ = nullptr;
    QString shown_currency() const;

    QString base_ccy_; ///< set by set_context() (the display currency until a portfolio is known)
    QString instrument_ccy_;
    QString taxes_text_;
    /// The user picked / typed a currency (or the edited row had one). Otherwise
    /// currency() stays empty = "the instrument's quoted currency", resolved from
    /// the quote — never a guess from the ticker written into the ledger.
    bool ccy_explicit_ = false;
};

/// DEPOSIT / WITHDRAWAL / FEE / TAX / INTEREST.
class CashMovementDialog : public QDialog {
    Q_OBJECT
  public:
    CashMovementDialog(const QString& base_ccy, bool track_cash, const QStringList& symbols,
                       QWidget* parent = nullptr);
    /// The row to record (portfolio_id not set).
    portfolio::Transaction transaction() const;

  private:
    void update_state();

    QString base_ccy_;
    bool track_cash_ = false;
    QComboBox* type_cb_ = nullptr;
    QLineEdit* amount_edit_ = nullptr;
    QComboBox* ccy_cb_ = nullptr;
    QLineEdit* fx_edit_ = nullptr;
    QLabel* fx_label_ = nullptr;
    QLineEdit* withheld_edit_ = nullptr;
    QLabel* withheld_label_ = nullptr;
    QComboBox* symbol_cb_ = nullptr;
    QLabel* symbol_label_ = nullptr;
    QDateEdit* date_edit_ = nullptr;
    QLineEdit* notes_edit_ = nullptr;
    QLabel* effect_ = nullptr;
    QPushButton* ok_btn_ = nullptr;
};

/// Stock split / reverse split: new units per old unit from an effective date.
class SplitDialog : public QDialog {
    Q_OBJECT
  public:
    SplitDialog(const QVector<portfolio::HoldingWithQuote>& holdings, const QString& preset_symbol,
                QWidget* parent = nullptr);
    QString symbol() const;
    /// New units per old unit (2 = 2-for-1, 0.1 = 1-for-10).
    double factor() const;
    QString date() const;
    QString notes() const;

  private:
    void update_preview();

    QVector<portfolio::HoldingWithQuote> holdings_;
    QComboBox* symbol_cb_ = nullptr;
    QDoubleSpinBox* new_spin_ = nullptr;
    QDoubleSpinBox* old_spin_ = nullptr;
    QDateEdit* date_edit_ = nullptr;
    QLineEdit* notes_edit_ = nullptr;
    QLabel* preview_ = nullptr;
    QPushButton* ok_btn_ = nullptr;
};

/// Benchmark (single or blended), risk-free override, cash tracking, tax-lot method,
/// long-term threshold. The base currency is shown (it is fixed at creation).
class PortfolioSettingsDialog : public QDialog {
    Q_OBJECT
  public:
    explicit PortfolioSettingsDialog(const portfolio::Portfolio& p, QWidget* parent = nullptr);

    /// '' = default for the base currency, "SPY" or "SPY:60,AGG:40".
    QString benchmark() const;
    std::optional<double> risk_free_override() const; ///< annual fraction
    bool track_cash() const;
    QString lot_method() const;
    int long_term_days() const;

  private:
    void add_blend_row(const QString& symbol, double weight_pct);
    void update_state();
    QString blend_spec(QString* error) const;

    portfolio::Portfolio p_;
    QRadioButton* bench_default_ = nullptr;
    QRadioButton* bench_custom_ = nullptr;
    QTableWidget* blend_table_ = nullptr;
    QPushButton* add_row_btn_ = nullptr;
    QPushButton* remove_row_btn_ = nullptr;
    QLabel* bench_status_ = nullptr;
    QCheckBox* rf_auto_ = nullptr;
    QDoubleSpinBox* rf_spin_ = nullptr;
    QLabel* rf_auto_label_ = nullptr;
    QCheckBox* cash_cb_ = nullptr;
    QComboBox* lot_cb_ = nullptr;
    QLabel* lot_hint_ = nullptr;
    QSpinBox* lt_spin_ = nullptr;
    QLabel* recompute_note_ = nullptr;
    QPushButton* ok_btn_ = nullptr;
};

} // namespace fincept::screens
