// src/screens/algo_trading/AlertsView.h
#pragma once
// AlertsView — the ALERTS sub-tab's layout (no service, repository or engine calls; AlertsPanel
// is the controller that feeds it and acts on its signals):
//
//   [ALGO  1 BUILDER … 4 ALERTS …   SAVE WATCH ⌃S · NEW · REFRESH ⌃R · OPEN SCANNER]
//   ┌ LIVE WATCHES  3 ─────── EDIT TEST DISABLE DELETE ┐┌ FIRED ALERTS  14 ──────────────┐
//   │ name · symbols · tf · rule · status · on · last…  ││ time · watch · symbol · detail │
//   └───────────────────────────────────────────────────┘└────────────────────────────────┘
//   ┌ WATCH COMPOSER  NEW ─────────────── ALL|ANY  + RULE  + GROUP ┐
//   │ NAME … TF … SOURCE … ACCOUNT … POLL … ☐ PROVIDERS            │
//   │ SYMBOLS [chip input slot]  NIFTY TOP 20  BANK NIFTY           │
//   │ rule table (RuleTableEditor, Kind::Alert) + sentence          │
//   └───────────────────────────────────────────────────────────────┘
//   [ENGINE · data · info · as of · state]
//
// The two tables sit side by side from 1200 px, stacked below. Styling: the Algo kit's root
// stylesheet (object names only, no setStyleSheet here).

#include "screens/algo_trading/AlgoTabKit.h"
#include "ui/tables/LiveTableModel.h"

#include <QDateTime>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QSplitter;
class QStackedWidget;
class QTableView;

namespace fincept::ui {
class EmptyState;
class KeySortProxy;
class LoadingSkeleton;
class PanelHeader;
class ScreenCommandBar;
class ScreenStatusBar;
namespace algo {
class RuleTableEditor;
}
} // namespace fincept::ui

namespace fincept::screens {

/// One live watch as the table shows it.
struct AlertsWatchRow {
    QString id;
    QString name;
    QString symbols_text;  ///< "AAPL, MSFT, +3"
    int symbol_count = 0;
    QString timeframe;
    QString rule_text;     ///< plain-language rules
    QString status;        ///< idle | watching | fired | error …
    QString data_source;   ///< Broker | Auto | YFinance
    bool active = true;
    int interval_sec = 60;
    qint64 last_eval_at = 0;  ///< epoch ms (0 = never)
    qint64 last_fired_at = 0; ///< epoch ms (0 = never)
};

/// One fired alert.
struct AlertsFiredRow {
    QString id;
    qint64 fired_at = 0; ///< epoch ms
    QString watch_name;
    QString symbol;
    QString detail;
};

class AlertsView : public QWidget {
    Q_OBJECT
  public:
    explicit AlertsView(QWidget* parent = nullptr);

    // ── Composer inputs (the controller reads / fills them) ─────────────────
    ui::algo::RuleTableEditor* rules() const { return rules_; }
    /// Container (QHBoxLayout) the controller puts the symbol chip input into.
    QWidget* symbols_slot() const { return symbols_slot_; }
    QLineEdit* name_edit() const { return name_edit_; }
    QComboBox* timeframe_combo() const { return timeframe_combo_; }
    QComboBox* data_source_combo() const { return data_source_combo_; }
    QComboBox* account_combo() const { return account_combo_; }
    QSpinBox* interval_spin() const { return interval_spin_; }
    /// Kept for the stored watch format (poll watches are one-shot: never shown).
    QSpinBox* cooldown_spin() const { return cooldown_spin_; }
    QCheckBox* providers_check() const { return providers_check_; }
    ui::ScreenStatusBar* status_bar() const { return status_bar_; }

    /// Broker accounts: (label, account id); the first row is "none". Keeps the selection.
    void set_accounts(const QVector<QPair<QString, QString>>& accounts);
    /// Composer title: empty = a new watch, else "EDITING <name>" (SAVE becomes UPDATE).
    void set_editing(const QString& name);
    /// One line under the composer (validation, saved / updated) in a tone.
    void set_message(const QString& text, algo_kit::Tone tone = algo_kit::Tone::Dim);

    // ── Tables ──────────────────────────────────────────────────────────────
    /// Before the first answer: skeletons in both panes.
    void set_loading();
    void set_watches(const QVector<AlertsWatchRow>& rows);
    void set_watches_error(const QString& error);
    void set_history(const QVector<AlertsFiredRow>& rows);
    void set_history_error(const QString& error);
    void set_as_of(const QDateTime& when);
    QString selected_watch_id() const;
    const AlertsWatchRow* watch(const QString& id) const;

  signals:
    void save_requested();
    void new_requested();
    void refresh_requested();
    void open_scanner_requested();
    void edit_requested(const QString& watch_id);
    void test_requested(const QString& watch_id);
    void active_toggle_requested(const QString& watch_id, bool on);
    void delete_requested(const QString& watch_id);
    void quick_symbols_requested(const QStringList& symbols);
    void retry_watches_requested();
    void retry_history_requested();

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    void build_ui();
    QWidget* build_composer(QWidget* parent);
    void retranslate();
    void update_row_actions();
    void update_info();
    void show_watches_page();
    void show_history_page();

    ui::ScreenCommandBar* bar_ = nullptr;
    ui::ScreenStatusBar* status_bar_ = nullptr;
    QSplitter* main_split_ = nullptr;   ///< tables over composer
    QSplitter* tables_split_ = nullptr; ///< watches | fired (vertical when narrow)

    // Watches
    ui::PanelHeader* watches_header_ = nullptr;
    QStackedWidget* watches_stack_ = nullptr;
    QTableView* watches_table_ = nullptr;
    ui::LiveTableModel<AlertsWatchRow>* watches_model_ = nullptr;
    ui::KeySortProxy* watches_proxy_ = nullptr;
    ui::EmptyState* watches_empty_ = nullptr;
    ui::LoadingSkeleton* watches_loading_ = nullptr;
    QPushButton* edit_btn_ = nullptr;
    QPushButton* test_btn_ = nullptr;
    QPushButton* toggle_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    bool watches_error_ = false;
    bool watches_loaded_ = false;

    // History
    ui::PanelHeader* history_header_ = nullptr;
    QStackedWidget* history_stack_ = nullptr;
    QTableView* history_table_ = nullptr;
    ui::LiveTableModel<AlertsFiredRow>* history_model_ = nullptr;
    ui::KeySortProxy* history_proxy_ = nullptr;
    ui::EmptyState* history_empty_ = nullptr;
    ui::LoadingSkeleton* history_loading_ = nullptr;
    bool history_error_ = false;
    bool history_loaded_ = false;

    // Composer
    ui::PanelHeader* composer_header_ = nullptr;
    ui::algo::RuleTableEditor* rules_ = nullptr;
    QWidget* symbols_slot_ = nullptr;
    QLineEdit* name_edit_ = nullptr;
    QComboBox* timeframe_combo_ = nullptr;
    QComboBox* data_source_combo_ = nullptr;
    QComboBox* account_combo_ = nullptr;
    QSpinBox* interval_spin_ = nullptr;
    QSpinBox* cooldown_spin_ = nullptr;
    QCheckBox* providers_check_ = nullptr;
    QPushButton* nifty_btn_ = nullptr;
    QPushButton* bank_btn_ = nullptr;
    QLabel* message_ = nullptr;
    QLabel* name_lbl_ = nullptr;
    QLabel* tf_lbl_ = nullptr;
    QLabel* source_lbl_ = nullptr;
    QLabel* account_lbl_ = nullptr;
    QLabel* poll_lbl_ = nullptr;
    QLabel* symbols_lbl_ = nullptr;
    QString editing_name_;
    bool narrow_ = false;
};

} // namespace fincept::screens
