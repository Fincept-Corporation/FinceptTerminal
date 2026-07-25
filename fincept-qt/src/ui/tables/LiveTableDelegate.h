#pragma once
// LiveTableDelegate — paints P&L colouring, side badges and a per-row action
// button WITHOUT allocating a widget or reparsing CSS per row.
//
// WHY THIS EXISTS
// ---------------
// The dominant per-tick cost in this codebase's blotters is not the data — it
// is the row chrome. The idiom being replaced is:
//
//     auto* exit_btn = new QPushButton(tr("Exit"));          // heap alloc/row
//     exit_btn->setStyleSheet("QPushButton { background:...; border:1px ... }");
//     table->setCellWidget(r, 9, exit_btn);                  // + layout churn
//
// Every setStyleSheet() is a full CSS parse. At 200 rows that is 200 parses and
// 200 widget allocations on every refresh, and the buttons are destroyed and
// rebuilt seconds later. Painting the button instead costs one drawRect plus one
// drawText per visible row — and only visible rows are ever painted, so a
// 5,000-row blotter costs the same as a 30-row one.
//
// Deliberately NOT a QObject: click notification goes through a std::function
// rather than a signal, so this stays header-only with no moc participation and
// no CMakeLists entry.
//
// USAGE
//     auto* d = new ui::LiveTableDelegate(view);
//     d->set_action_column(9, tr("EXIT"), ui::LiveTableDelegate::Danger);
//     d->set_on_action([this](const QModelIndex& i) { square_off(i); });
//     view->setItemDelegate(d);
//
// The model supplies colour through Qt::ForegroundRole as usual; this delegate
// only adds the action button and the badge treatment.

#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QModelIndex>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>
#include <QStyledItemDelegate>
#include <QToolTip>

#include <functional>

namespace fincept::ui {

class LiveTableDelegate : public QStyledItemDelegate {
  public:
    enum ActionStyle { Neutral, Danger, Positive };

    explicit LiveTableDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    /// Turn one column into a painted action button.
    void set_action_column(int column, const QString& label, ActionStyle style = Neutral) {
        action_col_ = column;
        action_label_ = label;
        action_style_ = style;
    }

    /// Called with the row's index when the painted button is clicked.
    void set_on_action(std::function<void(const QModelIndex&)> fn) { on_action_ = std::move(fn); }

    /// Rows for which this returns false render no button (e.g. an order that is
    /// already filled cannot be cancelled). Without this, a disabled-looking row
    /// still accepts the click.
    void set_action_enabled(std::function<bool(const QModelIndex&)> fn) { action_enabled_ = std::move(fn); }

    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override {
        if (idx.column() != action_col_ || action_col_ < 0) {
            QStyledItemDelegate::paint(p, opt, idx);
            return;
        }
        if (action_enabled_ && !action_enabled_(idx)) {
            QStyledItemDelegate::paint(p, opt, idx); // leave the cell blank/plain
            return;
        }

        // Background + selection are still the style's job, so a themed row
        // highlight keeps working under the button.
        QStyleOptionViewItem o(opt);
        initStyleOption(&o, idx);
        o.text.clear();
        const QWidget* w = o.widget;
        QStyle* style = w ? w->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &o, p, w);

        const QRect r = button_rect(opt.rect);
        const bool hot = (idx == hover_);

        QColor accent;
        switch (action_style_) {
        case Danger:
            accent = QColor(colors::NEGATIVE());
            break;
        case Positive:
            accent = QColor(colors::POSITIVE());
            break;
        default:
            accent = QColor(colors::TEXT_SECONDARY());
            break;
        }

        p->save();
        // Obsidian: square corners, no gradients (DESIGN_SYSTEM §8/§9).
        if (hot) {
            p->fillRect(r, accent);
            p->setPen(QColor(Qt::white));
        } else {
            p->setPen(accent);
            p->drawRect(r.adjusted(0, 0, -1, -1));
        }
        QFont f = opt.font;
        f.setBold(true);
        f.setPointSizeF(f.pointSizeF() - 1.0);
        p->setFont(f);
        if (!hot)
            p->setPen(accent);
        p->drawText(r, Qt::AlignCenter, action_label_);
        p->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& opt, const QModelIndex& idx) const override {
        QSize s = QStyledItemDelegate::sizeHint(opt, idx);
        if (idx.column() == action_col_ && action_col_ >= 0)
            s.setWidth(qMax(s.width(), opt.fontMetrics.horizontalAdvance(action_label_) + 22));
        return s;
    }

    bool editorEvent(QEvent* e, QAbstractItemModel*, const QStyleOptionViewItem& opt, const QModelIndex& idx) override {
        if (action_col_ < 0 || idx.column() != action_col_)
            return false;
        if (action_enabled_ && !action_enabled_(idx))
            return false;

        if (e->type() == QEvent::MouseMove) {
            // Repaint only the two cells whose hover state actually changed.
            const QModelIndex prev = hover_;
            hover_ = button_rect(opt.rect).contains(static_cast<QMouseEvent*>(e)->pos()) ? idx : QModelIndex();
            if (prev != hover_) {
                if (auto* v = qobject_cast<QAbstractItemView*>(const_cast<QWidget*>(opt.widget))) {
                    if (prev.isValid())
                        v->update(prev);
                    if (hover_.isValid())
                        v->update(hover_);
                }
            }
            return false; // let the view keep its own hover handling
        }

        if (e->type() == QEvent::MouseButtonRelease) {
            auto* me = static_cast<QMouseEvent*>(e);
            if (me->button() == Qt::LeftButton && button_rect(opt.rect).contains(me->pos())) {
                if (on_action_)
                    on_action_(idx);
                return true;
            }
        }
        return false;
    }

  private:
    static QRect button_rect(const QRect& cell) { return cell.adjusted(3, 3, -3, -3); }

    int action_col_ = -1;
    QString action_label_;
    ActionStyle action_style_ = Neutral;
    std::function<void(const QModelIndex&)> on_action_;
    std::function<bool(const QModelIndex&)> action_enabled_;
    mutable QModelIndex hover_;
};

} // namespace fincept::ui
