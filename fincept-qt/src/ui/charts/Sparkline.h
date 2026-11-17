#pragma once
// Sparkline — a word-sized trend line for tiles, headers and table cells.
//
//   auto* s = new ui::Sparkline(this);
//   s->set_data(closes);          // colour follows first → last (up / down)
//   s->set_baseline(prev_close);  // …or last vs a reference, with a guide line
//
// Painting lives in the static Sparkline::paint() so SparklineDelegate
// (src/ui/tables) draws identical lines inside table cells. Long series are
// decimated to pixel columns with min/max preservation, so a 10k-point
// history costs the same as a 200-point one and its spikes stay visible.
//
// The widget caches its rendering in a pixmap at the device pixel ratio and
// rebuilds it on data, size, DPR or theme changes. Its background is
// transparent by default (it sits on whatever tile or row hosts it).

#include <QColor>
#include <QPixmap>
#include <QRectF>
#include <QVector>
#include <QWidget>

#include <optional>

class QPainter;

namespace fincept::ui {

struct SparklineStyle {
    enum class ColorMode {
        Auto,     ///< up colour when last >= reference (baseline, else the first value), down otherwise
        Accent,   ///< theme accent
        Positive, ///< always the up colour
        Negative, ///< always the down colour
        Muted,    ///< tertiary text colour
        Palette,  ///< categorical chart colour `palette_index`
        Custom,   ///< `custom_color`, made legible on the active theme
    };
    ColorMode color_mode = ColorMode::Auto;
    int palette_index = 0;
    QColor custom_color;
    std::optional<double> baseline; ///< reference value for Auto colour (+ guide line)
    bool show_baseline = true;      ///< draw a dotted guide at the baseline when one is set
    bool invert = false;            ///< lower is better (Auto colours swap)
    bool fill = false;              ///< translucent area under the line
    bool last_dot = true;           ///< dot on the latest value
    bool min_max_dots = false;      ///< small up/down dots on the extreme values
    qreal line_width = 1.25;
};

class Sparkline : public QWidget {
    Q_OBJECT
  public:
    explicit Sparkline(QWidget* parent = nullptr);

    /// Values in chronological order. Non-finite entries are gaps.
    void set_data(const QVector<double>& values);
    const QVector<double>& data() const { return data_; }

    void set_baseline(double value);
    void clear_baseline();

    void set_spark_style(const SparklineStyle& style);
    const SparklineStyle& spark_style() const { return style_; }

    void set_color_mode(SparklineStyle::ColorMode mode, int palette_index = 0);
    void set_fill(bool on);
    void set_last_dot(bool on);
    void set_min_max_dots(bool on);
    void set_line_width(qreal w);

    /// Paint the theme's surface colour behind the line instead of leaving
    /// the background transparent.
    void set_opaque(bool on);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    /// Paint `values` into `r` with `style`. Used by the widget and by
    /// SparklineDelegate; safe to call from any paint routine.
    static void paint(QPainter& p, const QRectF& r, const QVector<double>& values, const SparklineStyle& style);
    /// The line colour `paint` would use for these values.
    static QColor resolve_color(const QVector<double>& values, const SparklineStyle& style);

  protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    void invalidate();

    QVector<double> data_;
    SparklineStyle style_;
    QPixmap cache_;
    bool dirty_ = true;
    bool opaque_ = false;
};

} // namespace fincept::ui
