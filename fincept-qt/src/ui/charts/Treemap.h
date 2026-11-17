#pragma once
// Treemap.h — squarified treemap (market map): area by weight, colour by a
// signed value on the theme's diverging scale, optional one level of grouping
// (e.g. sectors) with group headers, a colour legend, hover tooltip and
// click / double-click signals.

#include "ui/theme/ChartStyle.h"

#include <QRectF>
#include <QString>
#include <QVector>

#include <functional>

namespace fincept::ui {

struct TreemapItem {
    QString id;       // returned by the click signals
    QString label;    // main cell text (e.g. ticker)
    QString sublabel; // secondary text (e.g. company name), shown when it fits
    QString group;    // grouping key (e.g. sector); empty = "Other"
    double weight = 0; // cell area (e.g. market cap); items with weight <= 0 are skipped
    double value = 0;  // colour value (e.g. % change); NaN = no value (neutral fill)
};

class Treemap : public PaintedChart {
    Q_OBJECT
  public:
    explicit Treemap(QWidget* parent = nullptr);

    void set_items(const QVector<TreemapItem>& items);
    const QVector<TreemapItem>& items() const { return items_; }
    void clear();

    /// One level of grouping by TreemapItem::group, with group headers. Default true.
    void set_grouped(bool grouped);
    /// |value| >= clamp gets the full diverging colour. Default 3.0 (±3 %).
    void set_color_clamp(double clamp);
    /// Colour legend strip at the bottom. Default true.
    void set_show_legend(bool show);
    /// Formats TreemapItem::weight in the tooltip. Default: compact number.
    void set_weight_formatter(Formatter formatter);
    /// Replaces the default tooltip; return rich text or an empty string for none.
    void set_tooltip_provider(std::function<QString(const TreemapItem&)> provider);

    QSize sizeHint() const override { return {480, 320}; }
    QSize minimumSizeHint() const override { return {120, 80}; }

  signals:
    void item_clicked(const QString& id);
    void item_double_clicked(const QString& id);
    void group_clicked(const QString& group);

  protected:
    void paint_chart(QPainter& p, const QRectF& r) override;
    void paint_overlay(QPainter& p, const QRectF& r) override;
    QString default_format(double v) const override;
    bool has_data() const override;
    void on_hover_left() override;

    void mouseMoveEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;

  private:
    struct Cell {
        int item = -1;
        QRectF rect;
    };
    struct GroupBox {
        QString name;
        QRectF rect;
        QRectF header; // empty when too small for a header
        double weight = 0;
        double value = 0; // weight-averaged value of the group's items
    };

    void ensure_layout(const QRectF& r);
    void mark_dirty();
    int cell_at(const QPointF& pos) const;
    int header_at(const QPointF& pos) const;
    QColor fill_for(double value) const;

    QVector<TreemapItem> items_;
    bool grouped_ = true;
    double clamp_ = 3.0;
    bool show_legend_ = true;
    Formatter weight_formatter_;
    std::function<QString(const TreemapItem&)> tooltip_provider_;

    QVector<Cell> cells_;
    QVector<GroupBox> groups_;
    QRectF legend_rect_;
    QRectF layout_for_;
    bool layout_valid_ = false;
    int hover_cell_ = -1;
    int press_cell_ = -1;
    int press_header_ = -1;
};

} // namespace fincept::ui
