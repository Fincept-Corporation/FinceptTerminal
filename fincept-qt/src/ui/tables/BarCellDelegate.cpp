#include "ui/tables/BarCellDelegate.h"

#include "ui/theme/Theme.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace fincept::ui {

BarCellDelegate::BarCellDelegate(Mode mode, QObject* parent) : QStyledItemDelegate(parent), mode_(mode) {}

BarCellDelegate::~BarCellDelegate() {
    for (const auto& c : connections_)
        QObject::disconnect(c);
}

void BarCellDelegate::set_range(double min, double max) {
    fixed_min_ = min;
    fixed_max_ = max;
}

void BarCellDelegate::invalidate_range_cache() const {
    range_cache_.clear();
}

double BarCellDelegate::value_of(const QModelIndex& index, int role) {
    const QVariant v = index.data(role);
    switch (v.metaType().id()) {
        case QMetaType::Double:
        case QMetaType::Float:
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::Long:
        case QMetaType::ULong:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::Short:
        case QMetaType::UShort:
            return v.toDouble();
        default:
            break;
    }
    // Parse the shown text: "+1.25%", "1,234.50", "−0.3" (U+2212), "$12.4".
    QString s = index.data(Qt::DisplayRole).toString().trimmed();
    s.replace(QChar(0x2212), QLatin1Char('-'));
    QString digits;
    digits.reserve(s.size());
    for (const QChar ch : s) {
        if (ch.isDigit() || ch == QLatin1Char('.') || ch == QLatin1Char('-') || ch == QLatin1Char('e') ||
            ch == QLatin1Char('E'))
            digits.append(ch);
    }
    bool ok = false;
    const double d = digits.toDouble(&ok);
    return ok ? d : std::numeric_limits<double>::quiet_NaN();
}

void BarCellDelegate::watch_model(const QAbstractItemModel* model) const {
    if (watched_ == model)
        return;
    for (const auto& c : connections_)
        QObject::disconnect(c);
    connections_.clear();
    range_cache_.clear();
    watched_ = model;
    if (!model)
        return;
    auto* self = const_cast<BarCellDelegate*>(this);
    const auto drop = [self]() { self->range_cache_.clear(); };
    connections_ << QObject::connect(model, &QAbstractItemModel::dataChanged, self, drop)
                 << QObject::connect(model, &QAbstractItemModel::rowsInserted, self, drop)
                 << QObject::connect(model, &QAbstractItemModel::rowsRemoved, self, drop)
                 << QObject::connect(model, &QAbstractItemModel::columnsInserted, self, drop)
                 << QObject::connect(model, &QAbstractItemModel::columnsRemoved, self, drop)
                 << QObject::connect(model, &QAbstractItemModel::modelReset, self, drop)
                 << QObject::connect(model, &QAbstractItemModel::layoutChanged, self, drop);
}

BarCellDelegate::ColumnRange BarCellDelegate::column_range(const QModelIndex& index) const {
    const QAbstractItemModel* model = index.model();
    watch_model(model);
    const int col = index.column();
    const auto it = range_cache_.constFind(col);
    if (it != range_cache_.constEnd())
        return *it;
    ColumnRange r;
    bool any = false;
    const int rows = model ? model->rowCount(index.parent()) : 0;
    for (int row = 0; row < rows; ++row) {
        const double v = value_of(model->index(row, col, index.parent()), value_role_);
        if (!std::isfinite(v))
            continue;
        if (!any) {
            r.min = r.max = v;
            any = true;
        } else {
            r.min = std::min(r.min, v);
            r.max = std::max(r.max, v);
        }
        r.max_abs = std::max(r.max_abs, std::abs(v));
    }
    range_cache_.insert(col, r);
    return r;
}

void BarCellDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const QString model_text = opt.text;
    opt.text.clear(); // background, selection and focus only
    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    const double v = value_of(index, value_role_);
    const bool selected = (opt.state & QStyle::State_Selected) != 0;
    // Selected rows are painted bg_raised by the global stylesheet, so the
    // palette HighlightedText (text-on-accent) would be unreadable there.
    QColor text_color = selected ? QColor(colors::TEXT_PRIMARY()) : opt.palette.color(QPalette::Text);
    const QRectF cell = QRectF(option.rect).adjusted(2, 1, -2, -1);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (std::isfinite(v)) {
        const bool auto_range = !std::isfinite(fixed_min_) || !std::isfinite(fixed_max_);
        const ColumnRange cr = auto_range ? column_range(index) : ColumnRange{fixed_min_, fixed_max_, 0};
        switch (mode_) {
            case Mode::Heat: {
                QColor fill;
                if (heat_sequential_) {
                    const double lo = std::isfinite(fixed_min_) ? fixed_min_ : cr.min;
                    const double hi = std::isfinite(fixed_max_) ? fixed_max_ : cr.max;
                    fill = colors::sequential_color(hi > lo ? (v - lo) / (hi - lo) : 0.5);
                } else {
                    double clamp = heat_clamp_;
                    if (clamp <= 0)
                        clamp = auto_range ? cr.max_abs : std::max(std::abs(fixed_min_), std::abs(fixed_max_));
                    fill = colors::diverging_color(clamp > 0 ? v / clamp : 0.0);
                }
                painter->fillRect(option.rect, fill);
                if (selected) {
                    painter->setPen(QPen(opt.palette.color(QPalette::Highlight), 2));
                    painter->setBrush(Qt::NoBrush);
                    painter->drawRect(QRectF(option.rect).adjusted(1, 1, -1, -1));
                }
                text_color = colors::contrast_text_on(fill);
                break;
            }
            case Mode::SignedBar: {
                const double lo = std::isfinite(fixed_min_) ? fixed_min_ : std::min(cr.min, 0.0);
                const double hi = std::isfinite(fixed_max_) ? fixed_max_ : std::max(cr.max, 0.0);
                if (!(hi > lo))
                    break;
                const auto xmap = [&](double x) { return cell.left() + (qBound(lo, x, hi) - lo) / (hi - lo) * cell.width(); };
                const qreal x0 = xmap(0.0);
                const qreal xv = xmap(v);
                const qreal h = std::max<qreal>(3, cell.height() * 0.62);
                QColor c(v >= 0 ? colors::POSITIVE() : colors::NEGATIVE());
                c.setAlphaF(0.38f);
                painter->setPen(Qt::NoPen);
                painter->setBrush(c);
                painter->drawRect(QRectF(std::min(x0, xv), cell.center().y() - h / 2, std::max<qreal>(1, std::abs(xv - x0)), h));
                if (lo < 0 && hi > 0) {
                    painter->setPen(QPen(QColor(colors::BORDER_MED()), 1));
                    painter->drawLine(QPointF(std::round(x0) + 0.5, cell.top()), QPointF(std::round(x0) + 0.5, cell.bottom()));
                }
                break;
            }
            case Mode::Bar: {
                const double lo = std::isfinite(fixed_min_) ? fixed_min_ : std::min(cr.min, 0.0);
                const double hi = std::isfinite(fixed_max_) ? fixed_max_ : cr.max;
                if (!(hi > lo))
                    break;
                const qreal w = (qBound(lo, v, hi) - lo) / (hi - lo) * cell.width();
                const qreal h = std::max<qreal>(3, cell.height() * 0.62);
                QColor c = bar_color_.isValid() ? bar_color_ : colors::chart_color(0);
                c.setAlphaF(0.38f);
                painter->setPen(Qt::NoPen);
                painter->setBrush(c);
                painter->drawRect(QRectF(cell.left(), cell.center().y() - h / 2, std::max<qreal>(1, w), h));
                break;
            }
        }
    }
    if (show_text_) {
        const QString text = (formatter_ && std::isfinite(v)) ? formatter_(v) : model_text;
        if (!text.isEmpty()) {
            painter->setFont(opt.font);
            painter->setPen(text_color);
            painter->drawText(cell.adjusted(3, 0, -3, 0), static_cast<int>(opt.displayAlignment) | Qt::TextSingleLine,
                              text);
        }
    }
    painter->restore();
}

} // namespace fincept::ui
