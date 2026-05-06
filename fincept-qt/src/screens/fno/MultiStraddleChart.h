#pragma once
// MultiStraddleChart — N-line intraday chart of synthetic premium values.
//
// One QLineSeries per selected straddle/strangle. Each "Selection" carries
// a display label and a vector of (ts_minute_secs, premium ₹) points. The
// orchestrator (MultiStraddleSubTab) joins CE+PE oi:history sample streams
// per minute, sums the LTPs, and hands the merged points here.
//
// Colors rotate through a small palette (POSITIVE / AMBER / TEXT_PRIMARY /
// NEGATIVE) so up to four selections are visually distinguishable.
//
// Hover crosshair: snaps the vertical line to the nearest sample on the
// first series and shows the full y-stack ({label: value}) in a tooltip.

#include <QChartView>
#include <QGraphicsLineItem>
#include <QLabel>
#include <QPointer>
#include <QString>
#include <QVector>

class QDateTimeAxis;
class QLineSeries;
class QValueAxis;

namespace fincept::screens::fno {

class MultiStraddleChart : public QChartView {
    Q_OBJECT
  public:
    struct Sample {
        qint64 ts_secs = 0;   // floored to minute, unix epoch seconds
        double premium = 0;
    };
    struct Selection {
        QString label;
        QVector<Sample> points;
    };

    explicit MultiStraddleChart(QWidget* parent = nullptr);

    /// Replace ALL line series with the supplied selections. An empty
    /// vector clears the chart.
    void set_straddles(const QVector<Selection>& selections);

  protected:
    void mouseMoveEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;

  private:
    void update_crosshair(const QPoint& widget_pos);
    void hide_crosshair();

    QChart* chart_ = nullptr;
    QVector<QLineSeries*> series_;          // owned by the chart
    QDateTimeAxis* axis_x_ = nullptr;
    QValueAxis* axis_y_ = nullptr;
    QGraphicsLineItem* hover_line_ = nullptr;
    QPointer<QLabel> tooltip_;
    QVector<Selection> data_;
};

} // namespace fincept::screens::fno
