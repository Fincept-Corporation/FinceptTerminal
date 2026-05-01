#pragma once

#include <QString>
#include <QVariant>
#include <QWidget>

class QHideEvent;
class QLabel;
class QShowEvent;
class QChartView;
QT_FORWARD_DECLARE_CLASS(QChart)
QT_FORWARD_DECLARE_CLASS(QLineSeries)
QT_FORWARD_DECLARE_CLASS(QDateTimeAxis)
QT_FORWARD_DECLARE_CLASS(QValueAxis)

namespace fincept::screens::panels {

/// SupplyChartPanel — middle section of the ROADMAP tab (Phase 5 §5.2).
///
/// QChartView with three QLineSeries over the last 12 months:
///   - **Total**          (flat, supply ceiling)
///   - **Circulating**    (decreases as burns accumulate)
///   - **Burned**         (increases monotonically — amber, the headline)
///
/// Subscribes (visibility-driven, P3): `treasury:supply_history`. Topic
/// payload is `QVector<SupplyHistoryPoint>`.
///
/// Per P9 the chart is rebuilt only on data change or resize — not on
/// repaint. QtCharts itself caches the series rendering; we just avoid
/// rebuilding the chart from scratch unless the underlying data changed.
class SupplyChartPanel : public QWidget {
    Q_OBJECT
  public:
    explicit SupplyChartPanel(QWidget* parent = nullptr);
    ~SupplyChartPanel() override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void build_ui();
    void apply_theme();

    void on_supply_history_update(const QVariant& v);
    void on_topic_error(const QString& topic, const QString& error);
    void show_error_strip(const QString& msg);
    void clear_error_strip();
    void update_demo_chip(bool is_mock);

    // Head
    QLabel* status_pill_ = nullptr;

    // Chart
    QChartView* chart_view_ = nullptr;
    QChart* chart_ = nullptr;
    QLineSeries* total_series_ = nullptr;
    QLineSeries* circulating_series_ = nullptr;
    QLineSeries* burned_series_ = nullptr;
    QDateTimeAxis* x_axis_ = nullptr;
    QValueAxis* y_axis_ = nullptr;

    // Error strip
    QWidget* error_strip_ = nullptr;
    QLabel* error_text_ = nullptr;
};

} // namespace fincept::screens::panels
