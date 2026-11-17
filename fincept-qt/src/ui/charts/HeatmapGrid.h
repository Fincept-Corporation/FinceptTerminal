#pragma once
// HeatmapGrid.h — equal-size labelled cells in a flowing grid, coloured by a
// signed value on the theme's diverging scale. The small-set sibling of
// Treemap (indices, sectors, currencies, a watchlist): same item type, same
// signals, but every cell gets the same area.

#include "ui/charts/Treemap.h"

namespace fincept::ui {

class HeatmapGrid : public PaintedChart {
    Q_OBJECT
  public:
    explicit HeatmapGrid(QWidget* parent = nullptr);

    /// TreemapItem::weight and ::group are ignored; cells keep the given order.
    void set_items(const QVector<TreemapItem>& items);
    const QVector<TreemapItem>& items() const { return items_; }
    void clear();

    /// |value| >= clamp gets the full diverging colour. Default 3.0 (±3 %).
    void set_color_clamp(double clamp);
    /// Fixed column count; 0 (default) picks as many columns as fit min_cell_width.
    void set_columns(int columns);
    /// Smallest cell size (default 96 × 48 px); cells grow to fill the width.
    void set_min_cell_size(const QSize& size);
    /// Colour legend strip at the bottom. Default false.
    void set_show_legend(bool show);
    /// Replaces the default tooltip; return rich text or an empty string for none.
    void set_tooltip_provider(std::function<QString(const TreemapItem&)> provider);

    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  signals:
    void item_clicked(const QString& id);
    void item_double_clicked(const QString& id);

  protected:
    void paint_chart(QPainter& p, const QRectF& r) override;
    void paint_overlay(QPainter& p, const QRectF& r) override;
    QString default_format(double v) const override;
    bool has_data() const override { return !items_.isEmpty(); }
    void on_hover_left() override;

    void mouseMoveEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;

  private:
    int columns_for(qreal width) const;
    QRectF cell_rect(int index, const QRectF& r) const;
    int cell_at(const QPointF& pos) const;
    qreal grid_height(qreal width) const;

    QVector<TreemapItem> items_;
    double clamp_ = 3.0;
    int columns_ = 0;
    QSize min_cell_{96, 48};
    bool show_legend_ = false;
    std::function<QString(const TreemapItem&)> tooltip_provider_;
    int hover_ = -1;
    int press_ = -1;
};

} // namespace fincept::ui
