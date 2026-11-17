#pragma once
// DonutChart.h — donut / pie of parts of a whole (allocations, sector or
// asset-class weights): categorical colours by entity, small slices folded
// into "Other" beyond a maximum count, centre label (total by default),
// legend with values and shares, hover highlight and tooltip.

#include "ui/theme/ChartStyle.h"

#include <QColor>
#include <QPainterPath>
#include <QString>
#include <QVector>

namespace fincept::ui {

struct DonutSlice {
    QString id;    // returned by the signals; the folded bucket is "__other__"
    QString label; // legend / tooltip text
    double value = 0; // slices with value <= 0 are skipped
    QColor color;  // invalid = categorical palette colour of the slice's position in the input
};

class DonutChart : public PaintedChart {
    Q_OBJECT
  public:
    enum class LegendPosition { Auto, Right, Bottom, None };

    explicit DonutChart(QWidget* parent = nullptr);

    /// Colours follow the slices' order in `slices` (keep that order stable
    /// across updates so an entity keeps its colour); slices are drawn
    /// largest first.
    void set_slices(const QVector<DonutSlice>& slices);
    void clear();
    /// At most `n` slices are drawn; the smallest are folded into one
    /// "Other" slice (default 8; 0 = never fold).
    void set_max_slices(int n);
    void set_other_label(const QString& label);
    /// Centre text: `title` (muted, small) over `value` (large). An empty
    /// `value` shows the formatted total. Empty title and value = total only.
    void set_center_text(const QString& title, const QString& value = {});
    /// Inner radius as a fraction of the outer (0 = pie). Default 0.62.
    void set_hole_ratio(double ratio);
    void set_legend_position(LegendPosition position);
    /// Show the share (%) next to each legend value. Default true.
    void set_show_percent(bool show);

    double total() const { return total_; }

    QSize sizeHint() const override { return {360, 220}; }
    QSize minimumSizeHint() const override { return {120, 100}; }

  signals:
    void slice_clicked(const QString& id);
    /// Empty id when the mouse leaves the slices.
    void slice_hovered(const QString& id);

  protected:
    void paint_chart(QPainter& p, const QRectF& r) override;
    void paint_overlay(QPainter& p, const QRectF& r) override;
    bool has_data() const override { return !shown_.isEmpty(); }
    void on_hover_left() override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

  private:
    struct Shown {
        QString id;
        QString label;
        double value = 0;
        QColor color;
        int count = 1; // slices folded into this one
    };
    struct Geometry {
        QPointF center;
        qreal outer = 0;
        qreal inner = 0;
        QRectF legend;
        bool legend_right = true;
    };
    void rebuild();
    Geometry geometry(const QRectF& r) const;
    QPainterPath slice_path(const Geometry& g, int index, qreal grow) const;
    int slice_at(const QPointF& pos) const;
    int legend_row_at(const QPointF& pos) const;
    void paint_legend(QPainter& p, const Geometry& g) const;

    QVector<DonutSlice> input_;
    QVector<Shown> shown_;
    QVector<double> start_deg_; // per shown slice, clockwise from 12 o'clock
    QVector<double> span_deg_;
    double total_ = 0;
    int max_slices_ = 8;
    QString other_label_;
    QString center_title_;
    QString center_value_;
    double hole_ = 0.62;
    LegendPosition legend_pos_ = LegendPosition::Auto;
    bool show_percent_ = true;
    int hover_ = -1;
};

} // namespace fincept::ui
