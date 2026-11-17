// src/screens/economics/panels/MarketModelsPanel.h
// Market Models — three analytics built on market data rather than on one provider's series:
//   * Business cycle        Analytics/economics/business_cycle.py get_indicators — latest closes of the
//                           S&P 500, VIX, Treasuries (3M / 10Y / 30Y), gold, WTI and the dollar index, the
//                           curve spreads, S&P 500 trend statistics and a VIX sentiment label.
//   * Equity risk premium   Analytics/economics/equity_risk_premium.py estimate_erp — realised, implied
//                           (earnings yield) and supply-side ERP of the S&P 500 over the 10Y Treasury, with
//                           user overrides for every input.
//   * Yield curve analytics Analytics/fixedIncome/cli.py curve_shape / bootstrap / forward_curve /
//                           nelson_siegel on a par curve: the latest US Treasury curve (Federal Reserve H.15
//                           via federal_reserve_data.py yield_curve, no API key) or any curve the user types
//                           or pastes.
// No API key is needed (a FRED key only upgrades the ERP's inflation input).
// Lazy and cancellable: nothing is built or run with the panel. Each VIEW is a view of the results
// pane: its page is built the first time it is needed, its last result is remembered (also across
// restarts) and shown at once with its "as of" time, its scripts run only while it is on screen
// (leaving it during the automatic load kills them) and a result older than 30 minutes is refreshed
// quietly. FETCH re-runs the view with its current inputs (answers are reused for 10 minutes),
// REFRESH downloads fresh market data.
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QStringList>
#include <QVector>

class QFrame;
class QScrollArea;
class QTableWidget;
class QVBoxLayout;

namespace fincept::ui {
class DataGridEditor;
class KpiStrip;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens {

class MarketModelsPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit MarketModelsPanel(QWidget* parent = nullptr);
    void activate() override;

    /// The base persists the toolbar (VIEW). The inputs on the result pages are added here: the ERP
    /// overrides, the curve's coupon frequency and a curve the user typed or pasted.
    QVariantMap save_panel_state() const override;
    void restore_panel_state(const QVariantMap& state) override;

    /// A VIEW by its id — "cycle" | "erp" | "curve" (also "business_cycle", "equity_risk_premium",
    /// "yield_curve"); -1 for anything else.
    static int view_for_id(const QString& id);
    /// Select a VIEW (a deep link): the VIEW box's own path. False for an unknown id.
    bool select_view(const QString& id);

  protected:
    LoadPolicy load_policy() const override;
    QJsonValue page_payload(int page) const override;
    bool restore_page(int page, const QJsonValue& payload) override;
    void on_query_cancelled() override;
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;
    void refresh_panel_theme() override;
    void retranslateUi() override;

  private:
    enum View { kCycle = 0, kErp = 1, kCurve = 2, kViewCount = 3 };
    enum class TabState { Idle, Loading, Ready, Error };
    struct Tab {
        TabState state = TabState::Idle;
        QString message;      ///< loading text, or the error
        QString pending;      ///< request id the view waits for
        QDateTime fetched_at; ///< UTC retrieval time of the data on display
        bool from_cache = false;
        int page = -1; ///< content-page index
        // What display() shows for the view: title bar, provenance line, CSV / clipboard export.
        QJsonArray rows;
        QString title;
        QStringList columns;
        SeriesMeta meta;
        QString banner;
    };
    static constexpr int kErpFieldCount = 6; // rate / multiple overrides; the windows field is separate

    // ── Construction (lazy: one page per view, built when first needed) ───────
    void ensure_page(int view);
    void build_cycle_page();
    void build_erp_page();
    void build_curve_page();
    /// Restored ERP overrides / curve inputs, applied to a page when it is built.
    void apply_saved_inputs(int view);
    static QString view_key(int view);
    int view_of_page(int page) const;
    void apply_erp_texts();
    void apply_curve_texts();
    /// Every user-visible fixed text (construction and language switch).
    void apply_texts();
    QString page_style() const;

    // ── View state ────────────────────────────────────────────────────────────
    int current_view() const;
    /// The current view's result is ready: hand it to the results pane (draws the page).
    void render_view();
    /// Point the results pane at the current view (its remembered result, or its first load).
    void sync_load_view();
    void publish_rows(int view);
    void show_view_error(int view);
    void set_loading(int view, const QString& message, const QString& request_id);
    void set_error(int view, const QString& message);
    void run_view(int view, bool force);
    void run_script(const QString& script, const QString& command, const QString& args, const QString& request_id,
                    bool force);

    QString error_of(const services::EconomicsResult& result) const;

    // ── Business cycle ────────────────────────────────────────────────────────
    void start_cycle(bool force);
    void on_cycle_result(const services::EconomicsResult& result);
    void populate_cycle();

    // ── Equity risk premium ───────────────────────────────────────────────────
    bool read_erp_inputs(QJsonObject* params, QString* problem) const;
    void run_erp_from_form();
    void start_erp(bool force);
    void on_erp_result(const services::EconomicsResult& result);
    void populate_erp();
    QFrame* make_erp_card(const QString& key, const QJsonObject& estimate);
    QString erp_method_title(const QString& key) const;
    QString erp_component_label(const QString& key) const;
    void show_erp_inputs(const QString& note, bool error);
    void fit_erp_inputs_table();

    // ── Yield curve ───────────────────────────────────────────────────────────
    void load_treasury(bool force);
    void on_treasury_result(const services::EconomicsResult& result);
    void run_curve();
    void run_curve_from_editor();
    void start_forward_curve(const QJsonArray& spot_curve);
    void on_fi_result(const QString& command, const services::EconomicsResult& result);
    void finish_curve_run();
    void populate_curve();
    void clear_curve_results(const QString& message);
    void show_curve_editor(const QString& note, bool error);
    void update_curve_source_label();
    void fill_curve_grid(const QJsonArray& rows);

    // ── Toolbar ───────────────────────────────────────────────────────────────
    QLabel* view_lbl_ = nullptr;
    QComboBox* view_combo_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;

    Tab tabs_[kViewCount];
    bool restoring_ = false;
    bool force_next_ = false;      ///< REFRESH: the next run downloads fresh data
    QVariantMap saved_inputs_;     ///< restored page inputs (ERP overrides, curve), kept until the page exists

    // ── Business cycle page ───────────────────────────────────────────────────
    QWidget* cycle_root_ = nullptr;
    QLabel* cycle_head_ = nullptr;
    QLabel* cycle_warn_ = nullptr;
    QLabel* cycle_rates_lbl_ = nullptr;
    QLabel* cycle_equity_lbl_ = nullptr;
    QLabel* cycle_cmdty_lbl_ = nullptr;
    ui::KpiStrip* cycle_rates_ = nullptr;
    ui::KpiStrip* cycle_equity_ = nullptr;
    ui::KpiStrip* cycle_cmdty_ = nullptr;
    QJsonObject cycle_data_;

    // ── ERP page ──────────────────────────────────────────────────────────────
    QWidget* erp_root_ = nullptr;
    QScrollArea* erp_scroll_ = nullptr;
    QLabel* erp_inputs_title_ = nullptr;
    QLabel* erp_field_lbls_[kErpFieldCount + 1] = {};
    QLineEdit* erp_edits_[kErpFieldCount] = {};
    QLineEdit* erp_windows_ = nullptr;
    QPushButton* erp_run_btn_ = nullptr;
    QPushButton* erp_clear_btn_ = nullptr;
    QLabel* erp_msg_ = nullptr;
    ui::KpiStrip* erp_kpis_ = nullptr;
    QLabel* erp_consensus_note_ = nullptr;
    QLabel* erp_methods_lbl_ = nullptr;
    QVBoxLayout* erp_cards_ = nullptr;
    QLabel* erp_sources_lbl_ = nullptr;
    QTableWidget* erp_inputs_table_ = nullptr;
    QLabel* erp_notes_ = nullptr;
    QJsonObject erp_data_;

    // ── Yield curve page ──────────────────────────────────────────────────────
    QWidget* curve_root_ = nullptr;
    ui::KpiStrip* curve_kpis_ = nullptr;
    QLabel* curve_caption_ = nullptr;
    ui::TimeSeriesChart* curve_chart_ = nullptr;
    QTableWidget* curve_table_ = nullptr;
    QLabel* curve_editor_title_ = nullptr;
    QLabel* curve_src_lbl_ = nullptr;
    ui::DataGridEditor* curve_grid_ = nullptr;
    QLabel* curve_freq_lbl_ = nullptr;
    QComboBox* curve_freq_combo_ = nullptr;
    QPushButton* curve_load_btn_ = nullptr;
    QPushButton* curve_run_btn_ = nullptr;
    QLabel* curve_msg_ = nullptr;
    bool filling_grid_ = false;
    QString curve_source_;        ///< "treasury" (H.15 as loaded), "manual" (typed / pasted / edited) or empty
    QString curve_as_of_;         ///< H.15 observation date of a Treasury curve
    QDateTime curve_fetched_at_;  ///< when that curve was retrieved (UTC)
    bool curve_from_cache_ = false;
    /// One analytics run: its input and what the commands answered.
    struct CurveRun {
        QVector<double> m; ///< maturities in years, ascending
        QVector<double> y; ///< par yields as decimals
        int freq = 2;
        QString source; ///< curve_source_ at the time of the run
        QString as_of;
        QDateTime fetched_at;
        bool from_cache = false;
        QString note;                     ///< soft input warnings
        QHash<QString, QJsonObject> data; ///< command → payload
        QHash<QString, QString> errors;   ///< command → error text
    };
    CurveRun pending_run_; ///< in flight
    CurveRun shown_run_;   ///< on display — only replaced by a run that produced something
    /// Curve run <-> JSON, so a remembered curve result can be drawn again.
    static QJsonObject curve_run_json(const CurveRun& run);
    static CurveRun curve_run_from_json(const QJsonObject& o);
    bool curve_run_active_ = false;
    bool curve_has_results_ = false;
    QHash<QString, QString> fi_pending_; ///< command → request id
};

} // namespace fincept::screens
