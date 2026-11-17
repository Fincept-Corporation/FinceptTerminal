#pragma once
// Gauge.h — a score gauge: semi-circular dial or linear bar with coloured
// zones, the value in the middle and an optional caption. For ratings,
// sentiment / fear-greed scores, health and risk scores.

#include "ui/theme/ChartStyle.h"

#include <QString>
#include <QVector>

#include <cmath>
#include <limits>

namespace fincept::ui {

struct GaugeZone {
    /// Bad / Caution / Good are status colours (danger / warning / success,
    /// never affected by the direction palette); Down / Up are the price
    /// direction colours; Neutral is muted; Accent the theme accent.
    enum class Tone { Bad, Caution, Neutral, Good, Down, Up, Accent };
    double from = 0;
    double to = 0;
    Tone tone = Tone::Neutral;
    QString label; // shown in the tooltip and, in the linear style, under the zone when it fits
};

class Gauge : public PaintedChart {
    Q_OBJECT
  public:
    enum class Style { SemiCircle, Linear };

    explicit Gauge(QWidget* parent = nullptr);

    void set_style(Style style);
    Style style() const { return style_; }
    void set_range(double min, double max);
    /// NaN clears the value (the Empty state is drawn).
    void set_value(double value);
    double value() const { return value_; }
    void set_zones(const QVector<GaugeZone>& zones);
    /// Text under the value (semi-circle) or beside it (linear), e.g. "Greed".
    void set_caption(const QString& caption);
    /// Min / max labels at the ends. Default true.
    void set_show_range_labels(bool show);

    /// The zone containing `v`, or nullptr.
    const GaugeZone* zone_for(double v) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    bool hasHeightForWidth() const override { return style_ == Style::SemiCircle; }
    int heightForWidth(int w) const override;

  protected:
    void paint_chart(QPainter& p, const QRectF& r) override;
    bool has_data() const override { return std::isfinite(value_) && max_ > min_; }
    void mouseMoveEvent(QMouseEvent* e) override;

  private:
    void paint_semicircle(QPainter& p, const QRectF& r);
    void paint_linear(QPainter& p, const QRectF& r);
    QColor tone_color(GaugeZone::Tone tone) const;
    QColor value_color() const;

    Style style_ = Style::SemiCircle;
    double min_ = 0;
    double max_ = 100;
    double value_ = std::numeric_limits<double>::quiet_NaN();
    QVector<GaugeZone> zones_;
    QString caption_;
    bool show_range_labels_ = true;
};

} // namespace fincept::ui
