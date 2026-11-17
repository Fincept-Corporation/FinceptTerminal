#pragma once
// BarCellDelegate — in-cell visual encoding for dense numeric tables. Not a
// Q_OBJECT.
//   SignedBar: a bar growing left / right from a centre line (at 0), in the
//              direction colours — % change, P&L, z-scores.
//   Bar:       a bar from the left edge — weights, volume, market cap.
//   Heat:      the cell background on the diverging (signed values) or
//              sequential (Heat + set_heat_sequential) scale, text in
//              contrasting ink — returns grids, correlation tables.
// The cell text is drawn on top (the model's DisplayRole text, or the
// formatter's output), honouring Qt::TextAlignmentRole and ForegroundRole.
//
// The scale is either fixed (set_range / set_heat_clamp) or, by default,
// derived from the column's values; the auto scale is cached per column and
// dropped whenever the model reports a change.
//
// USAGE
//     auto* d = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::SignedBar, view);
//     view->setItemDelegateForColumn(kChangeColumn, d);

#include <QColor>
#include <QHash>
#include <QMetaObject>
#include <QPointer>
#include <QStyledItemDelegate>

#include <functional>
#include <limits>

class QAbstractItemModel;

namespace fincept::ui {

class BarCellDelegate : public QStyledItemDelegate {
  public:
    enum class Mode { SignedBar, Bar, Heat };

    explicit BarCellDelegate(Mode mode = Mode::SignedBar, QObject* parent = nullptr);
    ~BarCellDelegate() override;

    void set_mode(Mode mode) { mode_ = mode; }
    Mode mode() const { return mode_; }
    /// Fixed scale. NaN for both (the default) = auto from the column.
    void set_range(double min = std::numeric_limits<double>::quiet_NaN(),
                   double max = std::numeric_limits<double>::quiet_NaN());
    /// Heat mode: |value| >= clamp gets the full colour. <= 0 (default) = auto
    /// (the column's largest |value|).
    void set_heat_clamp(double clamp) { heat_clamp_ = clamp; }
    /// Heat mode: use the sequential scale (magnitudes) instead of diverging.
    void set_heat_sequential(bool on) { heat_sequential_ = on; }
    /// Role holding the number. Default Qt::EditRole; when that is not a
    /// number the DisplayRole text is parsed ("+1.25%", "1,234.5", "-0.3").
    void set_value_role(int role) { value_role_ = role; }
    /// Text drawn in the cell; default: the model's DisplayRole text.
    void set_formatter(std::function<QString(double)> formatter) { formatter_ = std::move(formatter); }
    void set_show_text(bool show) { show_text_ = show; }
    /// Bar mode colour; invalid (default) = the theme's first categorical colour.
    void set_bar_color(const QColor& color) { bar_color_ = color; }
    /// Drop cached auto scales (also done automatically on model changes).
    void invalidate_range_cache() const;

    /// The numeric value of `index` under `role` (DisplayRole text parsed as a
    /// fallback); NaN when there is none.
    static double value_of(const QModelIndex& index, int role = Qt::EditRole);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

  private:
    struct ColumnRange {
        double min = 0;
        double max = 0;
        double max_abs = 0;
    };
    ColumnRange column_range(const QModelIndex& index) const;
    void watch_model(const QAbstractItemModel* model) const;

    Mode mode_;
    double fixed_min_ = std::numeric_limits<double>::quiet_NaN();
    double fixed_max_ = std::numeric_limits<double>::quiet_NaN();
    double heat_clamp_ = 0;
    bool heat_sequential_ = false;
    int value_role_ = Qt::EditRole;
    std::function<QString(double)> formatter_;
    bool show_text_ = true;
    QColor bar_color_;

    mutable QPointer<const QAbstractItemModel> watched_;
    mutable QList<QMetaObject::Connection> connections_;
    mutable QHash<int, ColumnRange> range_cache_; // column → range
};

} // namespace fincept::ui
