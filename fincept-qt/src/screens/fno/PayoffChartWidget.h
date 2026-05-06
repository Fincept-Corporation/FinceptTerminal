#pragma once
// PayoffChartWidget — Qt6 Charts payoff renderer for the F&O Builder.
//
// Renders two curves over the same spot axis:
//
//   • Solid line  — P/L at expiry (intrinsic only). Tinted green above zero,
//                   red below zero via two zero-baseline QAreaSeries.
//   • Dashed line — P/L at the user-selected target date (BSM-priced).
//
// Vertical markers: amber line at current spot, white dotted lines at each
// breakeven. Hover crosshair: vertical line snaps to nearest sample, floating
// tooltip shows {spot, expiry P/L, target P/L}.
//
// The widget is purely render-side. Math comes in via `set_payoff()` —
// callers are expected to use `analytics::compute_payoff()` upstream.

#include "services/options/OptionChainTypes.h"

#include <QChartView>
#include <QGraphicsLineItem>
#include <QLabel>
#include <QPointer>
#include <QString>
#include <QVector>

class QChart;
class QLineSeries;
class QAreaSeries;
class QValueAxis;

namespace fincept::screens::fno {

class PayoffChartWidget : public QChartView {
    Q_OBJECT
  public:
    explicit PayoffChartWidget(QWidget* parent = nullptr);

    /// Replace the curve. `current_spot` drives the amber vertical marker;
    /// `breakevens` drive the dotted vertical markers and tooltip context.
    void set_payoff(const QVector<fincept::services::options::PayoffPoint>& curve,
                    double current_spot,
                    const QVector<double>& breakevens);

    /// Toggle the dashed target-day curve on/off.
    void set_target_curve_visible(bool on);

    /// Empty state — call when the strategy has no legs.
    void clear_payoff();

  protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private:
    void update_crosshair(const QPoint& widget_pos);
    void hide_crosshair();
    void rebuild_breakeven_markers(const QVector<double>& breakevens, double y_min, double y_max);
    void rebuild_spot_marker(double current_spot, double y_min, double y_max);

    QChart* chart_ = nullptr;
    QLineSeries* expiry_series_ = nullptr;
    QLineSeries* target_series_ = nullptr;
    // Profit / loss tinting — bounded by clamped curves over a flat zero baseline.
    QLineSeries* profit_curve_ = nullptr;     // max(0, expiry)
    QLineSeries* loss_curve_ = nullptr;       // min(0, expiry)
    QLineSeries* zero_baseline_ = nullptr;    // constant 0 over [spot_min, spot_max]
    QAreaSeries* profit_area_ = nullptr;
    QAreaSeries* loss_area_ = nullptr;
    QValueAxis* axis_x_ = nullptr;
    QValueAxis* axis_y_ = nullptr;

    // Vertical markers
    QGraphicsLineItem* spot_marker_ = nullptr;
    QVector<QGraphicsLineItem*> breakeven_markers_;

    // Crosshair tooltip
    QGraphicsLineItem* hover_line_ = nullptr;
    QPointer<QLabel> tooltip_;

    // Latest curve cache for crosshair lookup
    QVector<fincept::services::options::PayoffPoint> curve_;
    double current_spot_ = 0;
    QVector<double> breakevens_;
};

} // namespace fincept::screens::fno
