#include "ui/tables/SparklineDelegate.h"

#include <QApplication>
#include <QIcon>
#include <QPainter>
#include <QStyle>
#include <QVariant>
#include <QVariantList>

#include <algorithm>
#include <limits>

namespace fincept::ui {

SparklineDelegate::SparklineDelegate(QObject* parent) : QStyledItemDelegate(parent) {
    style_.last_dot = true;
}

QVector<double> SparklineDelegate::values_from(const QVariant& v) {
    if (!v.isValid() || v.isNull())
        return {};
    if (v.metaType() == QMetaType::fromType<QList<double>>())
        return v.value<QList<double>>();
    if (v.metaType() == QMetaType::fromType<QList<float>>()) {
        const auto f = v.value<QList<float>>();
        QVector<double> out;
        out.reserve(f.size());
        for (float x : f)
            out.append(static_cast<double>(x));
        return out;
    }
    if (v.metaType() == QMetaType::fromType<QStringList>()) {
        const QStringList sl = v.toStringList();
        QVector<double> out;
        out.reserve(sl.size());
        for (const auto& s : sl) {
            bool ok = false;
            const double d = s.toDouble(&ok);
            out.append(ok ? d : std::numeric_limits<double>::quiet_NaN());
        }
        return out;
    }
    if (v.canConvert<QVariantList>()) {
        const QVariantList list = v.toList();
        QVector<double> out;
        out.reserve(list.size());
        for (const auto& item : list) {
            bool ok = false;
            const double d = item.toDouble(&ok);
            out.append(ok ? d : std::numeric_limits<double>::quiet_NaN());
        }
        return out;
    }
    return {};
}

void SparklineDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const {
    const QVector<double> values = values_from(idx.data(data_role_));
    if (values.isEmpty()) {
        QStyledItemDelegate::paint(p, opt, idx);
        return;
    }

    // Background, selection and hover remain the style's job.
    QStyleOptionViewItem o(opt);
    initStyleOption(&o, idx);
    o.text.clear();
    o.icon = QIcon();
    const QWidget* w = o.widget;
    QStyle* style = w ? w->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &o, p, w);

    SparklineStyle st = style_;
    if (baseline_role_ >= 0) {
        bool ok = false;
        const double b = idx.data(baseline_role_).toDouble(&ok);
        if (ok)
            st.baseline = b;
    }

    const int hpad = std::max(2, o.fontMetrics.averageCharWidth() / 2);
    const int vpad = std::max(2, o.rect.height() / 6);
    const QRectF r = QRectF(o.rect).adjusted(hpad, vpad, -hpad, -vpad);
    p->save();
    p->setClipRect(o.rect);
    Sparkline::paint(*p, r, values, st);
    p->restore();
}

QSize SparklineDelegate::sizeHint(const QStyleOptionViewItem& opt, const QModelIndex& idx) const {
    QSize s = QStyledItemDelegate::sizeHint(opt, idx);
    // Cheap check — sizeHint runs for every cell when columns resize to contents.
    if (idx.data(data_role_).isValid())
        s.setWidth(std::max(s.width(), opt.fontMetrics.averageCharWidth() * width_chars_));
    return s;
}

} // namespace fincept::ui
