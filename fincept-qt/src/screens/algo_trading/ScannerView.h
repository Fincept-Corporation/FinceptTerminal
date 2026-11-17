// src/screens/algo_trading/ScannerView.h
#pragma once
// ScannerView — the SCANNER sub-tab's workbench, a pure view (no service, engine, broker
// or database calls; ScannerPanel is the controller):
//
//   command bar   ALGO [sub-tabs] … RUN SCAN Ctrl+↵ · CREATE ALERT · OPEN IN BUILDER · EXPORT CSV
//   strip         PRESET [▾]  UNIVERSE [▾] LOAD  SAVE LIST…  BARS [▾]  RANGE [▾]  SOURCE [▾]  ACCOUNT [▾]
//   panels        RULES (ui::algo::RuleTableEditor)      | RESULTS (match strength, price in the
//                 SYMBOLS (ui::algo::SymbolChipInput)    |   symbol's currency, one column per rule)
//                                                         | DETAIL (candles of the selected symbol +
//                                                         |   the value each rule was evaluated on)
//   status bar    engine · source · matches · as of · state
//
// The results area is hosted by the controller's ui::AutoLoadPane: results_host() is the
// slot, build_results_content(pane->content_parent()) builds the table / "no match" page
// under the pane.

#include "ui/charts/CandleChart.h"
#include "ui/widgets/EmptyState.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

class QComboBox;
class QEvent;
class QHideEvent;
class QLabel;
class QPushButton;
class QShowEvent;
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
class RuleTableEditor;
class SymbolChipInput;
} // namespace algo
} // namespace fincept::ui

namespace fincept::screens {

class ScannerView : public QWidget {
    Q_OBJECT
  public:
    struct Row; ///< one result row (defined in the .cpp)

    explicit ScannerView(QWidget* parent = nullptr);
    ~ScannerView() override;

    // ── Inputs (the controller fills / tracks them) ──
    fincept::ui::algo::RuleTableEditor* rules() const { return rules_; }
    fincept::ui::algo::SymbolChipInput* symbols() const { return symbols_; }
    QComboBox* preset_combo() const { return preset_; }
    QComboBox* universe_combo() const { return universe_; }
    QComboBox* timeframe_combo() const { return timeframe_; }
    QComboBox* range_combo() const { return range_; }
    QComboBox* source_combo() const { return source_; }
    QComboBox* account_combo() const { return account_; }
    QPushButton* load_universe_button() const { return load_universe_; }
    QPushButton* save_universe_button() const { return save_universe_; }
    fincept::ui::ScreenCommandBar* bar() const { return bar_; }
    fincept::ui::ScreenStatusBar* status_bar() const { return status_; }
    /// "as of 14:02 · refreshing…" (kept by the controller's AutoLoadPane).
    QLabel* asof_label() const { return asof_; }

    // ── Results ──
    /// The slot the controller's AutoLoadPane goes into (has a QVBoxLayout).
    QWidget* results_host() const { return results_host_; }
    /// Builds the result table / "no match" stack under `parent` (once) and returns it.
    QWidget* build_results_content(QWidget* parent);
    /// A scan's payload (AlgoScanner::scan_complete) → the table. `currency_of(symbol)`
    /// names each symbol's quote currency ("" = unknown: no unit).
    void set_scan(const QJsonObject& payload, const std::function<QString(const QString&)>& currency_of);
    /// "No symbol matches" / "nothing evaluated" (its primary emits no_match_action).
    void show_no_match(const fincept::ui::EmptyState::Spec& spec);
    /// Symbols that could not be evaluated (with why); empty hides the list.
    void set_unevaluated(const QStringList& lines);
    /// RUN SCAN ↔ SCANNING… while a scan runs.
    void set_running(bool running);
    int result_count() const;
    QString selected_symbol() const;
    /// The rule columns' headers (the rules as written) for the CSV export.
    QStringList rule_headers() const { return rule_headers_; }

    // ── Detail ──
    void set_detail_bars(const QString& symbol, const QVector<fincept::ui::CandleChart::Bar>& bars,
                         const QString& currency);
    void set_detail_chart_state(fincept::ui::CandleChart::State state, const QString& message = {});

    /// Show / hide the account combo (YFinance-only scans need none).
    void set_account_visible(bool visible);
    /// The range combo is meaningless for some bar sizes (hidden with its caption).
    void set_range_visible(bool visible);

  signals:
    /// "run" | "alert" | "builder" | "export" (bar actions and their keys).
    void action_requested(const QString& action);
    /// The selected result changed ("" = none).
    void selection_changed(const QString& symbol);
    /// Double-click / Enter on a result: open it in the Builder.
    void activated(const QString& symbol);
    void no_match_action();

  protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    void build_ui();
    void install_shortcuts();
    void retranslate();
    void update_detail_values();
    void update_summary();

    fincept::ui::ScreenCommandBar* bar_ = nullptr;
    fincept::ui::ScreenStatusBar* status_ = nullptr;
    QComboBox* preset_ = nullptr;
    QComboBox* universe_ = nullptr;
    QPushButton* load_universe_ = nullptr;
    QPushButton* save_universe_ = nullptr;
    QComboBox* timeframe_ = nullptr;
    QComboBox* range_ = nullptr;
    QComboBox* source_ = nullptr;
    QComboBox* account_ = nullptr;
    QWidget* range_group_ = nullptr;
    QWidget* account_group_ = nullptr;
    QLabel* preset_cap_ = nullptr;
    QLabel* universe_cap_ = nullptr;
    QLabel* tf_cap_ = nullptr;
    QLabel* range_cap_ = nullptr;
    QLabel* source_cap_ = nullptr;
    QLabel* account_cap_ = nullptr;

    fincept::ui::PanelHeader* rules_header_ = nullptr;
    fincept::ui::PanelHeader* symbols_header_ = nullptr;
    fincept::ui::PanelHeader* results_header_ = nullptr;
    fincept::ui::PanelHeader* detail_header_ = nullptr;
    fincept::ui::algo::RuleTableEditor* rules_ = nullptr;
    fincept::ui::algo::SymbolChipInput* symbols_ = nullptr;

    QWidget* results_host_ = nullptr;
    QLabel* asof_ = nullptr;
    QPushButton* errors_btn_ = nullptr;
    QLabel* errors_ = nullptr;
    QStackedWidget* results_stack_ = nullptr; ///< 0 table, 1 "no match"
    QTableView* table_ = nullptr;
    fincept::ui::LiveTableModel<Row>* model_ = nullptr;
    fincept::ui::KeySortProxy* proxy_ = nullptr;
    fincept::ui::EmptyState* no_match_ = nullptr;

    fincept::ui::CandleChart* chart_ = nullptr;
    QLabel* detail_empty_ = nullptr;
    QWidget* values_box_ = nullptr;
    QLabel* values_ = nullptr;
    QString chart_symbol_;

    QStringList rule_headers_;
    int condition_count_ = 0;
    int total_scanned_ = 0;
    bool running_ = false;
};

} // namespace fincept::screens
