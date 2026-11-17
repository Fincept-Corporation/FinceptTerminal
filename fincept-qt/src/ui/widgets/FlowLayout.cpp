// src/ui/widgets/FlowLayout.cpp
#include "ui/widgets/FlowLayout.h"

#include "ui/widgets/FlowLayoutGeometry.h"

#include <QApplication>
#include <QFontMetrics>
#include <QWidget>

#include <algorithm>

namespace fincept::ui {

FlowLayout::FlowLayout(QWidget* parent, int h_spacing, int v_spacing)
    : QLayout(parent), h_spacing_(h_spacing), v_spacing_(v_spacing) {
    setContentsMargins(0, 0, 0, 0);
}

FlowLayout::~FlowLayout() {
    while (QLayoutItem* item = FlowLayout::takeAt(0))
        delete item;
}

// ── spacing / look ───────────────────────────────────────────────────────────

void FlowLayout::set_spacing(int h_spacing, int v_spacing) {
    if (h_spacing_ == h_spacing && v_spacing_ == v_spacing)
        return;
    h_spacing_ = h_spacing;
    v_spacing_ = v_spacing;
    invalidate();
}

int FlowLayout::horizontal_spacing() const {
    if (h_spacing_ >= 0)
        return h_spacing_;
    const QWidget* w = parentWidget();
    const QFontMetrics fm(w ? w->font() : QApplication::font());
    return flow_layout::default_h_spacing(fm.averageCharWidth());
}

int FlowLayout::vertical_spacing() const {
    if (v_spacing_ >= 0)
        return v_spacing_;
    const QWidget* w = parentWidget();
    const QFontMetrics fm(w ? w->font() : QApplication::font());
    return flow_layout::default_v_spacing(fm.height());
}

void FlowLayout::setSpacing(int spacing) {
    set_spacing(spacing, spacing);
}

int FlowLayout::spacing() const {
    return horizontal_spacing();
}

void FlowLayout::set_centered(bool centered) {
    if (centered_ == centered)
        return;
    centered_ = centered;
    invalidate();
}

void FlowLayout::set_compact_hint(bool compact) {
    if (compact_hint_ == compact)
        return;
    compact_hint_ = compact;
    invalidate();
}

void FlowLayout::insert_widget(int index, QWidget* w) {
    if (!w)
        return;
    addChildWidget(w);
    items_.insert(std::clamp<qsizetype>(index, 0, items_.size()), new QWidgetItem(w));
    invalidate();
}

// ── QLayout ──────────────────────────────────────────────────────────────────

void FlowLayout::addItem(QLayoutItem* item) {
    items_.append(item);
    invalidate();
}

int FlowLayout::count() const {
    return static_cast<int>(items_.size());
}

QLayoutItem* FlowLayout::itemAt(int index) const {
    return (index >= 0 && index < items_.size()) ? items_.at(index) : nullptr;
}

QLayoutItem* FlowLayout::takeAt(int index) {
    // No invalidate() here: QLayout::removeItem / removeWidget invalidate after taking,
    // and the destructor takes every item while the parent may be going away.
    if (index < 0 || index >= items_.size())
        return nullptr;
    return items_.takeAt(index);
}

Qt::Orientations FlowLayout::expandingDirections() const {
    return {};
}

bool FlowLayout::hasHeightForWidth() const {
    return true;
}

int FlowLayout::heightForWidth(int width) const {
    // Asked several times per layout pass by the parent layout: remember the last answer
    // (invalidate() — any item / spacing / font change — forgets it).
    if (width == cached_width_ && cached_height_ >= 0)
        return cached_height_;
    cached_height_ = arrange(QRect(0, 0, width, 0), false);
    cached_width_ = width;
    return cached_height_;
}

void FlowLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);
    arrange(rect, true);
}

QSize FlowLayout::sizeHint() const {
    if (compact_hint_)
        return minimumSize();
    QVector<flow_layout::Item> items;
    items.reserve(items_.size());
    for (QLayoutItem* it : items_)
        items.append(flow_layout::Item{it->sizeHint(), it->alignment(), it->isEmpty()});
    const QSize line = flow_layout::one_line_size(items, horizontal_spacing());
    const QMargins m = contentsMargins();
    return line + QSize(m.left() + m.right(), m.top() + m.bottom());
}

QSize FlowLayout::minimumSize() const {
    QSize s(0, 0);
    for (QLayoutItem* it : items_)
        if (!it->isEmpty())
            s = s.expandedTo(it->minimumSize());
    const QMargins m = contentsMargins();
    return s + QSize(m.left() + m.right(), m.top() + m.bottom());
}

void FlowLayout::invalidate() {
    cached_width_ = -1;
    cached_height_ = -1;
    QLayout::invalidate();
}

bool FlowLayout::right_to_left() const {
    const QWidget* w = parentWidget();
    return (w ? w->layoutDirection() : QGuiApplication::layoutDirection()) == Qt::RightToLeft;
}

int FlowLayout::arrange(const QRect& rect, bool apply) const {
    const QMargins m = contentsMargins();
    const QRect area = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());

    QVector<flow_layout::Item> items;
    items.reserve(items_.size());
    for (QLayoutItem* it : items_)
        items.append(flow_layout::Item{it->sizeHint(), it->alignment(), it->isEmpty()});

    flow_layout::Params p;
    p.h_spacing = horizontal_spacing();
    p.v_spacing = vertical_spacing();
    p.centered = centered_;
    p.rtl = right_to_left();
    const flow_layout::Result r = flow_layout::arrange(area, items, p);

    if (apply)
        for (int i = 0; i < items_.size(); ++i)
            if (!items[i].skip)
                items_[i]->setGeometry(r.rects[i]);
    return m.top() + r.height + m.bottom();
}

} // namespace fincept::ui
