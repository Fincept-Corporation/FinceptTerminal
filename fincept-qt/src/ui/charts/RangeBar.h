#pragma once
// RangeBar.h — a horizontal low…high track with markers: 52-week range with
// the current price, day range, analyst target low / mean / high against the
// price, drift or σ bands. The painting is shared with RangeBarDelegate
// (src/ui/tables/) through RangeBar::paint_range().

#include "ui/theme/ChartStyle.h"

#include <QMetaType>
#include <QString>
#include <QVector>

namespace fincept::ui {

struct RangeMarker {
    /// Current: the main pin (price) above the track, filled track up to it.
    /// Target / Mean: accent tick. Low / High / Neutral: muted tick.
    /// Up / Down: tick in the direction colours.
    enum class Role { Current, Target, Mean, Low, High, Neutral, Up, Down };
    double value = 0;
    QString label; // shown with the value; empty = value only
    Role role = Role::Neutral;
};

struct RangeBand {
    /// Neutral: muted; Up / Down: direction colours; Accent: theme accent;
    /// Caution: the warning colour.
    enum class Tone { Neutral, Up, Down, Accent, Caution };
    double from = 0;
    double to = 0;
    Tone tone = Tone::Neutral;
    QString label; // tooltip only
};

struct RangeBarData {
    double low = 0;  // left end of the track
    double high = 0; // right end of the track
    QVector<RangeBand> bands;
    QVector<RangeMarker> markers;
    /// Fill the track from `low` to the first Current marker. Default true.
    bool fill_to_current = true;
};

class RangeBar : public PaintedChart {
    Q_OBJECT
  public:
    explicit RangeBar(QWidget* parent = nullptr);

    void set_data(const RangeBarData& data);
    const RangeBarData& data() const { return data_; }

    void set_range(double low, double high);
    /// Sets (replaces) the Current marker.
    void set_current(double value, const QString& label = {});
    void add_marker(double value, const QString& label, RangeMarker::Role role);
    void clear_markers();
    void set_bands(const QVector<RangeBand>& bands);
    /// Low / high and marker value labels. Default true.
    void set_show_labels(bool show);

    /// Paints `d` into `r`. With `show_labels` and enough height the low/high
    /// values go under the track ends, the Current value above its pin and
    /// other markers' labels below; in short rects (table rows) low / high sit
    /// left and right of a thin track and markers are ticks only.
    static void paint_range(QPainter& p, const QRectF& r, const RangeBarData& d,
                            const std::function<QString(double)>& fmt, bool show_labels);
    /// The track rect paint_range() uses inside `r` (for hit-testing).
    static QRectF track_rect(const QRectF& r, const RangeBarData& d, const std::function<QString(double)>& fmt,
                             bool show_labels);
    /// Rich-text tooltip listing the range, bands and markers.
    static QString tooltip_for(const RangeBarData& d, const std::function<QString(double)>& fmt);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  protected:
    void paint_chart(QPainter& p, const QRectF& r) override;
    bool has_data() const override;
    void mouseMoveEvent(QMouseEvent* e) override;

  private:
    RangeBarData data_;
    bool show_labels_ = true;
};

} // namespace fincept::ui

Q_DECLARE_METATYPE(fincept::ui::RangeBarData)
