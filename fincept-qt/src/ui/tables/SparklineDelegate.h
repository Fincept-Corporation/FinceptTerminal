#pragma once
// SparklineDelegate — paints a sparkline inside a table cell.
//
// The model stores the series in a role of the cell (QVector<double> via
// QVariant::fromValue, or a QVariantList of numbers):
//
//   item->setData(QVariant::fromValue(closes), ui::SparklineDelegate::kDefaultRole);
//   view->setItemDelegateForColumn(kTrendCol, new ui::SparklineDelegate(view));
//
// Cells whose role holds no series fall back to ordinary text painting, so the
// delegate may also be installed on a whole view. Selection / hover
// backgrounds are still drawn by the style underneath the line. Painting is
// shared with ui::Sparkline (same colours, decimation, dots), and colours are
// read from the theme at paint time — a theme switch only needs the view to
// repaint, which the application-wide re-style already triggers.

#include "ui/charts/Sparkline.h"

#include <QStyledItemDelegate>
#include <QVector>

namespace fincept::ui {

class SparklineDelegate : public QStyledItemDelegate {
  public:
    /// The role read by default: Qt::UserRole + 40 (chosen to stay clear of
    /// the small UserRole offsets models commonly use for sort keys).
    static constexpr int kDefaultRole = Qt::UserRole + 40;

    explicit SparklineDelegate(QObject* parent = nullptr);

    /// Role that holds the values (QVector<double> or QVariantList).
    void set_data_role(int role) { data_role_ = role; }
    int data_role() const { return data_role_; }

    /// Optional role holding a per-row baseline (double), e.g. previous close.
    /// -1 (default) uses the style's baseline.
    void set_baseline_role(int role) { baseline_role_ = role; }

    void set_spark_style(const SparklineStyle& style) { style_ = style; }
    const SparklineStyle& spark_style() const { return style_; }

    /// Preferred cell width in average character widths (default 12).
    void set_width_chars(int chars) { width_chars_ = chars > 0 ? chars : 12; }

    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override;
    QSize sizeHint(const QStyleOptionViewItem& opt, const QModelIndex& idx) const override;

    /// Extract the series from a cell value (QVector<double>, QList<float>,
    /// QVariantList or a QStringList of numbers). Empty when it holds none.
    static QVector<double> values_from(const QVariant& v);

  private:
    int data_role_ = kDefaultRole;
    int baseline_role_ = -1;
    int width_chars_ = 12;
    SparklineStyle style_;
};

} // namespace fincept::ui
