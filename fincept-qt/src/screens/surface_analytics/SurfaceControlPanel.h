#pragma once
// SurfaceControlPanel — left-side control surface for Surface Analytics.
// Replaces the old SurfaceMetricsPanel + the bottom-strip Databento panel's
// asset/parameter inputs. Reads the SurfaceCapabilities matrix to decide
// which sections to render for the active ChartType.
//
// Owns the SurfaceControlsState and emits controls_changed + fetch_requested
// for the screen to translate into DatabentoFetchParams.

#include "SurfaceCapabilities.h"
#include "SurfaceTypes.h"

#include <QDate>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QWidget>

class QComboBox;
class QCompleter;
class QDateEdit;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStringListModel;
class QTimer;
class QToolButton;
class QVBoxLayout;

namespace fincept::surface {

struct SurfaceControlsState {
    QString symbol = "SPY";
    QString dataset; // empty = use capability default
    QDate start_date;
    QDate end_date;
    int strike_window_pct = 20;
    int dte_min = 7;
    int dte_max = 365;
    QString iv_method = "Brent";
    QStringList basket;
    QStringList venues;
};

class SurfaceControlPanel : public QWidget {
    Q_OBJECT
  public:
    explicit SurfaceControlPanel(QWidget* parent = nullptr);

    // Re-render sections to match the active surface's capability profile.
    // Called by the screen on every chip click.
    void set_capability(ChartType type);

    // Push min/max/mean/median/std/skew/kurt of the active surface's z matrix.
    void update_metrics(const std::vector<std::vector<float>>& z, const QString& units = "");

    // Show a one-line lineage string from the active capability + state.
    void update_lineage(const QString& line);

    SurfaceControlsState state() const { return state_; }

    void apply_state(const SurfaceControlsState& s);

    // Tighten the QDateEdits' min/max bounds (and clamp current values into
    // them) when the screen receives dataset_range metadata. Called once per
    // dataset; subsequent calls are cheap.
    void apply_dataset_range(const QDate& available_start, const QDate& available_end);

    // Update the provider status pill (header). dataset is shown alongside
    // the dot. State is one of "connected", "disconnected", "not configured".
    void set_provider_status(const QString& provider_name, const QString& state,
                             const QString& detail = QString());

  signals:
    void controls_changed();
    void fetch_requested();
    void symbol_changed(const QString& symbol);

  private slots:
    void on_symbol_edited();
    void on_symbol_text_changed(const QString& text);
    void on_search_debounce_fired();
    void on_dataset_changed();
    void on_dates_changed();
    void on_lookback_preset(int days);
    void on_strike_window_changed(int v);
    void on_dte_changed();
    void on_iv_method_changed();
    void on_basket_add();
    void on_basket_remove();
    void on_fetch_clicked();

  private:
    void setup_ui();
    QGroupBox* build_asset_section();
    QGroupBox* build_dates_section();
    QGroupBox* build_options_section();
    QGroupBox* build_basket_section();
    QGroupBox* build_metrics_section();
    QGroupBox* build_lineage_section();
    void apply_capability_visibility();
    void prefill_datasets();

    // ── Sections ───────────────────────────────────────────────────────────
    QGroupBox* asset_box_ = nullptr;
    QGroupBox* dates_box_ = nullptr;
    QGroupBox* options_box_ = nullptr;
    QGroupBox* basket_box_ = nullptr;
    QGroupBox* metrics_box_ = nullptr;
    QGroupBox* lineage_box_ = nullptr;

    // ── Asset section ──────────────────────────────────────────────────────
    QLineEdit* symbol_edit_ = nullptr;
    QComboBox* dataset_combo_ = nullptr;
    QLabel* spot_label_ = nullptr;
    QLabel* tier_badge_ = nullptr;
    // Symbol search autocomplete (Databento)
    QCompleter* symbol_completer_ = nullptr;
    QStringListModel* symbol_completer_model_ = nullptr;
    QTimer* symbol_search_timer_ = nullptr;
    QString pending_search_query_;

    // ── Date range section ─────────────────────────────────────────────────
    QDateEdit* start_edit_ = nullptr;
    QDateEdit* end_edit_ = nullptr;
    // Lookback preset row buttons; clicked -> on_lookback_preset
    QToolButton* lb_1d_ = nullptr;
    QToolButton* lb_5d_ = nullptr;
    QToolButton* lb_1m_ = nullptr;
    QToolButton* lb_3m_ = nullptr;
    QToolButton* lb_1y_ = nullptr;

    // ── Option filters section ─────────────────────────────────────────────
    QSpinBox* strike_window_spin_ = nullptr;
    QSpinBox* dte_min_spin_ = nullptr;
    QSpinBox* dte_max_spin_ = nullptr;
    QComboBox* iv_method_combo_ = nullptr;

    // ── Basket section ─────────────────────────────────────────────────────
    QLineEdit* basket_input_ = nullptr;
    QListWidget* basket_list_ = nullptr;
    QPushButton* basket_add_btn_ = nullptr;
    QPushButton* basket_remove_btn_ = nullptr;

    // ── Metrics section ────────────────────────────────────────────────────
    QLabel* metrics_min_ = nullptr;
    QLabel* metrics_max_ = nullptr;
    QLabel* metrics_mean_ = nullptr;
    QLabel* metrics_median_ = nullptr;
    QLabel* metrics_std_ = nullptr;
    QLabel* metrics_skew_ = nullptr;
    QLabel* metrics_kurt_ = nullptr;
    QLabel* metrics_count_ = nullptr;

    // ── Lineage section ────────────────────────────────────────────────────
    QLabel* lineage_label_ = nullptr;

    // ── Provider status pills (top of panel) ───────────────────────────────
    // QWidget container, populated by set_provider_status(). Future providers
    // (Polygon, Tradier, FRED, etc) will append rows here without layout churn.
    QWidget* providers_box_ = nullptr;
    QHash<QString, QLabel*> provider_dot_;
    QHash<QString, QLabel*> provider_state_;
    QHash<QString, QLabel*> provider_detail_;

    // ── Footer ─────────────────────────────────────────────────────────────
    QPushButton* fetch_btn_ = nullptr;

    ChartType active_type_ = ChartType::Volatility;
    SurfaceControlsState state_;
};

} // namespace fincept::surface
