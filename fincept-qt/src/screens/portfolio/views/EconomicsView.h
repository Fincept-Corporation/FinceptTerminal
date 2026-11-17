// src/screens/portfolio/views/EconomicsView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QJsonValue>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QString>
#include <QTableWidget>
#include <QWidget>

namespace fincept::ui {
class AutoLoadPane;
class BarChart;
class EmptyState;
class KpiStrip;
} // namespace fincept::ui

namespace fincept::screens {

/// Economics view: per-holding economics, the live US macro backdrop (FRED) and
/// the macro-shock presets of RISK › STRESS applied to today's book.
///
///   * PORTFOLIO ECONOMICS OVERVIEW — local, from the summary (class A).
///   * US MACRO CONDITIONS — five FRED series through EconomicsService, hosted by a
///     ui::AutoLoadPane: without a FRED_API_KEY the pane shows the "needs setup"
///     page (Settings > Credentials); with one it loads on first show (class B,
///     remembered with "as of", refreshed after 6 h) into a KPI strip.
///   * MACRO SHOCKS — the rate / inflation / currency / commodity / equity presets
///     (and the user's own scenarios) of RISK › STRESS, applied locally (class A).
class EconomicsView : public QWidget {
    Q_OBJECT
  public:
    explicit EconomicsView(QWidget* parent = nullptr);
    ~EconomicsView() override;

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);

  signals:
    /// "OPEN RISK › STRESS" — the host opens the RISK view on its STRESS tab.
    void open_stress_requested();
    /// BUY on the empty-book state — the host opens the add-position dialog.
    void add_position_requested();

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    QWidget* build_macro_section(QWidget* parent);
    QWidget* build_shocks_section(QWidget* parent);
    void retranslateUi();
    /// Re-colours the tables (item colours are not restyled by the theme).
    void refresh_theme();
    void update_indicators();
    /// Applies the macro-shock presets to the book (local, instant).
    void update_shocks();

    // FRED pane
    void start_macro(quint64 request_id);
    void render_macro(const QJsonValue& payload);
    void prompt_fred_key();

    // Section titles/notes
    QLabel* ind_title_ = nullptr;
    QLabel* ind_note_ = nullptr;
    QLabel* sens_title_ = nullptr;
    QLabel* sens_note_ = nullptr;

    // Macro indicators table
    QTableWidget* indicators_table_ = nullptr;

    // Live macro conditions (FRED)
    QLabel* macro_title_ = nullptr;
    QLabel* macro_status_ = nullptr;      ///< "as of 14:02" (kept by the pane)
    QPushButton* macro_key_btn_ = nullptr; ///< inline key entry, visible while the pane needs setup
    QPushButton* macro_refresh_btn_ = nullptr;
    ui::AutoLoadPane* macro_pane_ = nullptr;
    ui::KpiStrip* macro_strip_ = nullptr;
    quint64 macro_pending_id_ = 0; ///< pane request waiting for EconomicsService (0 = none)
    bool macro_force_ = false;     ///< next fetch bypasses EconomicsService's cache (a key was just saved)

    // Macro shocks
    QPushButton* shocks_open_btn_ = nullptr;
    QStackedWidget* shocks_stack_ = nullptr; ///< 0 = chart + table, 1 = empty book
    ui::BarChart* shocks_chart_ = nullptr;
    QTableWidget* shocks_table_ = nullptr;
    ui::EmptyState* shocks_empty_ = nullptr;

    portfolio::PortfolioSummary summary_;
    QString currency_;
    bool has_data_ = false;
};

} // namespace fincept::screens
