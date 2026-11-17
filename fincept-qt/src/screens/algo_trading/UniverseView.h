// src/screens/algo_trading/UniverseView.h
#pragma once
// UniverseView — the UNIVERSE sub-tab's workbench, a pure view (no service, engine, broker
// or database calls; UniverseScannerPanel is the controller):
//
//   command bar   ALGO [sub-tabs] … START SCAN (STOP SCAN while running) · DEPLOY… · CLEAR
//   strip         STRATEGY [▾]  UNIVERSE [▾]  ACCOUNT [▾]  SWEEP [ms]  COOLDOWN [min]
//   strip         SYMBOLS [chips]                       (only for "Custom symbols")
//   panels        LIVE MATCHES (table, or its designed state) | STRATEGY (what runs)
//   status bar    engine · feed · matches · last match · state
//
// A match row's DEPLOY cell (or Enter / double click / the bar's DEPLOY…) asks the controller
// to open the deploy dialog for that symbol; nothing deploys from here by itself.

#include "screens/algo_trading/AlgoTabKit.h"
#include "ui/widgets/EmptyState.h"

#include <QString>
#include <QVector>
#include <QWidget>

class QComboBox;
class QEvent;
class QHideEvent;
class QLabel;
class QShowEvent;
class QSpinBox;
class QStackedWidget;
class QTableView;

namespace fincept::ui {
class KeySortProxy;
class PanelHeader;
class ScreenCommandBar;
class ScreenStatusBar;
template <typename Row>
class LiveTableModel;
namespace algo {
class SymbolChipInput;
}
} // namespace fincept::ui

namespace fincept::screens {

class UniverseView : public QWidget {
    Q_OBJECT
  public:
    struct Row; ///< one match (defined in the .cpp)

    explicit UniverseView(QWidget* parent = nullptr);
    ~UniverseView() override;

    QComboBox* strategy_combo() const { return strategy_; }
    QComboBox* universe_combo() const { return universe_; }
    QComboBox* account_combo() const { return account_; }
    QSpinBox* sweep_spin() const { return sweep_; }
    QSpinBox* cooldown_spin() const { return cooldown_; }
    fincept::ui::algo::SymbolChipInput* symbols_input() const { return symbols_; }
    fincept::ui::ScreenCommandBar* bar() const { return bar_; }
    fincept::ui::ScreenStatusBar* status_bar() const { return status_; }

    /// The custom-symbols strip (universe = "Custom symbols").
    void set_custom_visible(bool visible);
    /// START SCAN (primary) ↔ STOP SCAN (danger) and the state chip.
    void set_running(bool running);
    /// A one-line status ("Stopped.", "Could not start…") in the status bar, toned.
    void set_status(const QString& text, fincept::screens::algo_kit::Tone tone);
    /// What the selected strategy runs (entry rules in plain language, its bars).
    void set_strategy_summary(const QString& name, const QString& entry_sentence, const QString& meta);

    // ── Matches ──
    /// A live match (newest first; at most 500 kept). `direction` +1 bullish, -1 bearish, 0 none.
    void add_match(qint64 time_ms, const QString& symbol, int direction, double price, const QString& currency,
                   const QString& detail);
    void clear_matches();
    int match_count() const;
    /// Re-render every row's text (number / currency format changed).
    void refresh_text();
    /// The designed state shown while there is no match (its primary emits state_action()).
    void show_state(const fincept::ui::EmptyState::Spec& spec);

  signals:
    /// "start" (START / STOP toggle) | "clear".
    void action_requested(const QString& action);
    /// Open the deploy dialog for this matched symbol.
    void deploy_requested(const QString& symbol);
    void state_action();

  protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    void build_ui();
    void retranslate();
    void update_counts();
    QString selected_symbol() const;

    fincept::ui::ScreenCommandBar* bar_ = nullptr;
    fincept::ui::ScreenStatusBar* status_ = nullptr;
    QComboBox* strategy_ = nullptr;
    QComboBox* universe_ = nullptr;
    QComboBox* account_ = nullptr;
    QSpinBox* sweep_ = nullptr;
    QSpinBox* cooldown_ = nullptr;
    QLabel* strategy_cap_ = nullptr;
    QLabel* universe_cap_ = nullptr;
    QLabel* account_cap_ = nullptr;
    QLabel* sweep_cap_ = nullptr;
    QLabel* cooldown_cap_ = nullptr;
    QLabel* symbols_cap_ = nullptr;
    QWidget* custom_strip_ = nullptr;
    fincept::ui::algo::SymbolChipInput* symbols_ = nullptr;

    fincept::ui::PanelHeader* matches_header_ = nullptr;
    fincept::ui::PanelHeader* strategy_header_ = nullptr;
    QStackedWidget* stack_ = nullptr; ///< 0 table, 1 designed state
    QTableView* table_ = nullptr;
    fincept::ui::LiveTableModel<Row>* model_ = nullptr;
    fincept::ui::KeySortProxy* proxy_ = nullptr;
    fincept::ui::EmptyState* state_ = nullptr;
    QLabel* s_name_ = nullptr;
    QLabel* s_entry_cap_ = nullptr;
    QLabel* s_entry_ = nullptr;
    QLabel* s_meta_ = nullptr;
    QLabel* s_note_ = nullptr;

    QVector<Row> rows_; ///< newest first
    quint64 next_seq_ = 0;
    bool running_ = false;
};

} // namespace fincept::screens
