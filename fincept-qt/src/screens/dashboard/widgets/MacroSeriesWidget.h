#pragma once
// MacroSeriesWidget — one economic time series as a small line chart with the
// latest value and its change vs the previous observation.
//
// Pure DataHub consumer. Two sources:
//   fred      `econ:dashboard:fred_<SERIES>` — EconomicsService runs fred_data.py
//             (needs FRED_API_KEY, Settings › Credentials). The widget registers
//             the dispatch with EconomicsService::remember_topic() and then relies
//             on the hub's cold-start / TTL / forced refresh — it never fetches.
//   dbnomics  `dbnomics:<provider>:<dataset>:<series>` — DBnomicsService REST,
//             no key.
//
// Config (persisted):
//   { "source": "fred" | "dbnomics", "series": "DGS10" | "BLS/ln/LNS14000000",
//     "range": "1Y" | "2Y" | "5Y" | "10Y" }

#include "screens/dashboard/widgets/BaseWidget.h"

#include <QDate>
#include <QPixmap>
#include <QVector>

class QLabel;

namespace fincept::services {
struct EconomicsResult;
struct DbnDataPoint;
} // namespace fincept::services

namespace fincept::screens::widgets {

struct MacroSeriesPoint {
    QDate date;
    double value = 0.0;
};

/// QPainter line chart for MacroSeriesWidget: line + value axis (right) + first/last
/// date (bottom). Rendered into a cached QPixmap, rebuilt only on set_points(),
/// resize or theme change (CLAUDE.md P9).
class MacroSeriesCanvas : public QWidget {
    Q_OBJECT
  public:
    explicit MacroSeriesCanvas(QWidget* parent = nullptr);
    void set_points(const QVector<MacroSeriesPoint>& points);
    /// Drop the cached pixmap so the next paint re-reads the theme tokens.
    void refresh_theme();

  protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void changeEvent(QEvent* event) override;

  private:
    void rebuild_cache();

    QVector<MacroSeriesPoint> points_;
    QPixmap cache_;
    bool dirty_ = true;
};

class MacroSeriesWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit MacroSeriesWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    QDialog* make_config_dialog(QWidget* parent) override;
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void retranslateUi() override;

  private:
    /// Empty when `series` is usable for `source`, else a user-facing reason.
    static QString series_problem(const QString& source, const QString& series);
    static QString title_for(const QString& source, const QString& series);

    void build_ui();
    void apply_styles();
    void update_title();
    void update_link();
    /// Registers the producer side (hub registration + FRED dispatch record) and
    /// returns the topic for the current config.
    QString prepare_topic();
    void hub_resubscribe();
    void hub_unsubscribe();
    void refresh_data();

    void on_fred_result(const services::EconomicsResult& result);
    void on_dbnomics_result(const services::DbnDataPoint& point);
    void on_hub_error(const QString& error);
    QString error_message(const QString& error) const;
    void set_series(QVector<MacroSeriesPoint> points, const QString& title, const QString& units,
                    const QString& frequency);
    /// Re-slices points_ to range_ and refreshes labels + canvas.
    void render();

    QString source_;
    QString series_;
    QString range_;
    QString topic_;

    QVector<MacroSeriesPoint> points_; // full delivered history, ascending
    QString series_title_;
    QString units_;
    QString frequency_;
    QString last_error_; // raw producer error for the current config (empty = none)

    QWidget* body_ = nullptr; // hosts every label + the one stylesheet styling them
    QLabel* value_label_ = nullptr;
    QLabel* change_label_ = nullptr;
    QLabel* date_label_ = nullptr;
    QLabel* name_label_ = nullptr;
    QLabel* meta_label_ = nullptr;
    MacroSeriesCanvas* canvas_ = nullptr;

    bool hub_active_ = false;
    /// Subscription dropped on a permanent error (missing/invalid key, unknown
    /// series) so the hub stops retrying every min_interval; the title-bar refresh
    /// or a config change clears it.
    bool halted_ = false;
};

} // namespace fincept::screens::widgets
