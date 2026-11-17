#pragma once
// MatrixHeatmap.h — a rows × columns matrix of coloured cells: correlation
// matrices, FX cross rates, parameter-optimisation grids. Diverging or
// sequential colour scale, optional in-cell values, hover readout with row and
// column highlight, optional hierarchical-clustering order for symmetric
// matrices. Grows its minimum size to keep cells legible, so inside a
// QScrollArea a 50 × 50 matrix scrolls; without one it shrinks, thinning the
// labels and dropping the in-cell text.

#include "ui/theme/ChartStyle.h"

#include <QStringList>
#include <QVector>

#include <limits>

namespace fincept::ui {

class MatrixHeatmap : public PaintedChart {
    Q_OBJECT
  public:
    enum class Scale { Diverging, Sequential };

    explicit MatrixHeatmap(QWidget* parent = nullptr);

    /// `values[row][col]`; NaN = empty cell. Missing entries count as NaN.
    void set_matrix(const QStringList& row_labels, const QStringList& col_labels,
                    const QVector<QVector<double>>& values);
    void clear();

    /// Diverging scale: `min` → full negative colour, `center` → neutral, `max`
    /// → full positive colour (values outside are clamped). NaN bounds = ±the
    /// largest |value|; NaN center = 0 when inside (min, max), else the midpoint.
    /// This is the default, with all three NaN. Correlations: (-1, 1).
    void set_diverging_scale(double min = std::numeric_limits<double>::quiet_NaN(),
                             double max = std::numeric_limits<double>::quiet_NaN(),
                             double center = std::numeric_limits<double>::quiet_NaN());
    /// Sequential scale from `min` (faint) to `max` (strong); NaN = data min / max.
    void set_sequential_scale(double min = std::numeric_limits<double>::quiet_NaN(),
                              double max = std::numeric_limits<double>::quiet_NaN());
    Scale scale() const { return scale_; }

    /// Draw the formatted value inside cells large enough for it. Default true.
    void set_show_values(bool show);
    /// Order rows and columns by average-linkage hierarchical clustering
    /// (square matrices only; distance 1 − value when every value is in
    /// [-1, 1], else Euclidean distance between rows). Default false.
    void set_cluster(bool cluster);
    bool cluster() const { return cluster_; }
    /// Colour legend strip at the bottom. Default true.
    void set_show_legend(bool show);
    /// Smallest cell edge (px) the widget asks its layout for. Default 12.
    void set_min_cell_size(int px);

    /// Display position → original row / column index (after clustering).
    QVector<int> row_order() const { return row_order_; }
    QVector<int> column_order() const { return col_order_; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  signals:
    /// Original (not display) row / column indexes.
    void cell_clicked(int row, int col);
    /// Hovered cell (original indexes); (-1, -1) when the mouse leaves the matrix.
    void cell_hovered(int row, int col);

  protected:
    void paint_chart(QPainter& p, const QRectF& r) override;
    void paint_overlay(QPainter& p, const QRectF& r) override;
    QString default_format(double v) const override;
    bool has_data() const override { return !values_.isEmpty() && !col_labels_.isEmpty(); }
    void on_theme_changed() override { layout_valid_ = false; }
    void on_hover_left() override;
    void resizeEvent(QResizeEvent* e) override;

    void mouseMoveEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

  private:
    struct Layout {
        QRectF grid;      // the cells
        QRectF row_hdr;   // row labels (left of grid)
        QRectF col_hdr;   // column labels (above grid)
        QRectF legend;    // empty when hidden
        qreal cell_w = 0;
        qreal cell_h = 0;
        bool rotate_cols = false;
        int row_label_step = 1; // draw every n-th row label
        int col_label_step = 1;
    };

    void ensure_layout(const QRectF& r);
    void compute_order();
    void resolve_scale();
    double value_at(int row, int col) const; // original indexes
    double t_for(double v) const;
    QColor color_for(double v) const;
    bool cell_at(const QPointF& pos, int* disp_row, int* disp_col) const;
    QSizeF label_extent() const; // widest row label, widest column label

    QStringList row_labels_;
    QStringList col_labels_;
    QVector<QVector<double>> values_;
    Scale scale_ = Scale::Diverging;
    double req_min_ = std::numeric_limits<double>::quiet_NaN();
    double req_max_ = std::numeric_limits<double>::quiet_NaN();
    double req_center_ = std::numeric_limits<double>::quiet_NaN();
    double lo_ = -1;
    double hi_ = 1;
    double center_ = 0;
    bool show_values_ = true;
    bool cluster_ = false;
    bool show_legend_ = true;
    int min_cell_ = 12;

    QVector<int> row_order_;
    QVector<int> col_order_;
    Layout layout_;
    QRectF layout_for_;
    bool layout_valid_ = false;
    int hover_row_ = -1; // display indexes
    int hover_col_ = -1;
    int press_row_ = -1;
    int press_col_ = -1;
};

} // namespace fincept::ui
