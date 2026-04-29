#pragma once
// SurfaceLineWidget — 1-D curve renderer for surfaces that flatten to a single
// line: yield curves, contango/backwardation, FX forwards, crack spreads,
// monetary policy paths, inflation expectations.
//
// Built on QPainter (matches the rest of the screen) so we don't pull in
// Qt6 Charts as a dependency for a chart that is fundamentally a polyline.

#include <QColor>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <vector>

namespace fincept::surface {

class SurfaceLineWidget : public QWidget {
    Q_OBJECT
  public:
    explicit SurfaceLineWidget(QWidget* parent = nullptr);

    // Render a single named curve.
    // Optional categorical x-labels are rendered as tick labels under the axis;
    // if empty, numeric x_values are formatted directly.
    void set_curve(const QString& title,
                   const std::vector<float>& x_values,
                   const std::vector<float>& y_values,
                   const QStringList& x_labels = {},
                   const QString& x_axis_title = QString(),
                   const QString& y_axis_title = QString(),
                   const QColor& line_color = QColor(88, 166, 255));

    // Render multiple curves on the same axes (e.g. one per commodity for contango).
    struct Series {
        QString name;
        std::vector<float> x_values;
        std::vector<float> y_values;
        QColor color;
    };
    void set_series(const QString& title,
                    const std::vector<Series>& series,
                    const QString& x_axis_title = QString(),
                    const QString& y_axis_title = QString());

    void clear();

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    QString title_;
    QString x_axis_title_;
    QString y_axis_title_;
    QStringList x_labels_;
    std::vector<Series> series_;
    float x_min_ = 0, x_max_ = 1;
    float y_min_ = 0, y_max_ = 1;

    void recompute_bounds();
    QPointF data_to_pixel(float x, float y, const QRectF& plot_area) const;
};

} // namespace fincept::surface
