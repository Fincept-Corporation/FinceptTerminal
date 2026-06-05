// src/screens/algo_trading/AlertsPanel.h
#pragma once
#include "algo_engine/ScanMonitor.h"
#include "services/algo_trading/AlgoTradingTypes.h"
#include "storage/repositories/ScanWatchRepository.h"
#include "ui/widgets/algo/ConditionSection.h"
#include "ui/widgets/algo/SymbolChipInput.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

/// Alert/watch manager — build a condition, save it as a background watch, and
/// manage running watches. Reuses ScanWatchRepository + ScanMonitor.
class AlertsPanel : public QWidget {
    Q_OBJECT
  public:
    explicit AlertsPanel(QWidget* parent = nullptr);

    void prefill(const QJsonArray& conditions, const QString& logic,
                 const QStringList& symbols, const QString& timeframe,
                 const QString& data_source, const QString& account_id);

  private slots:
    void on_save_watch();
    void on_refresh_watches();

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void retranslateUi();

    fincept::ui::algo::ConditionSection* section_ = nullptr;
    fincept::ui::algo::SymbolChipInput* symbols_input_ = nullptr;
    QComboBox*    timeframe_combo_   = nullptr;
    QComboBox*    data_source_combo_ = nullptr;
    QComboBox*    account_combo_     = nullptr;
    QSpinBox*     interval_spin_     = nullptr;
    QSpinBox*     cooldown_spin_     = nullptr;
    QCheckBox*    providers_chk_     = nullptr;
    QLineEdit*    name_edit_         = nullptr;
    QPushButton*  save_btn_          = nullptr;
    QTableWidget* watches_table_     = nullptr;
    QTableWidget* history_table_     = nullptr;
    QLabel*       status_label_      = nullptr;
    QString       editing_id_;        // non-empty when editing an existing watch

    QLabel* cond_title_    = nullptr;
    QLabel* sym_title_     = nullptr;
    QLabel* tf_lbl_        = nullptr;
    QLabel* ds_lbl_        = nullptr;
    QLabel* acct_lbl_      = nullptr;
    QLabel* iv_lbl_        = nullptr;
    QLabel* cd_lbl_        = nullptr;
    QLabel* name_lbl_      = nullptr;
    QLabel* watches_title_ = nullptr;
    QLabel* history_title_ = nullptr;

    void prefill_close_from_price(const QString& symbol, double price);
    void refresh_history();
    void load_watch(const fincept::ScanWatch& w); // load an existing watch into the form for editing
};

} // namespace fincept::screens
