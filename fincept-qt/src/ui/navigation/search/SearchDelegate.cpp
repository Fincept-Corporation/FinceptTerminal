// SearchDelegate.cpp — see SearchDelegate.h. Colours and fonts from the live theme at paint time.
#include "ui/navigation/search/SearchDelegate.h"

#include "ui/navigation/search/SearchModel.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QStyle>
#include <QTextLayout>

#include <algorithm>

namespace fincept::ui::search {

namespace {

const SearchRow* sd_row(const QModelIndex& index) {
    return static_cast<const SearchRow*>(index.data(SearchModel::RowRole).value<const void*>());
}

QFont sd_primary_font() {
    return fonts::font(-1, QFont::Normal);
}
QFont sd_lead_font() {
    return fonts::font(-1, QFont::Bold);
}
QFont sd_secondary_font() {
    return fonts::font(-3);
}
QFont sd_chip_font() {
    return fonts::font(-5, QFont::Bold);
}
QFont sd_heading_font() {
    return fonts::font(-4, QFont::Bold);
}
QFont sd_button_font() {
    return fonts::font(-4, QFont::DemiBold);
}

int sd_pad() {
    return std::max(4, QFontMetrics(fonts::font(0)).height() / 3);
}

/// Chip column width: the widest chip text the box uses, so every title starts at the same x.
int sd_chip_width() {
    const QFontMetrics fm(sd_chip_font());
    static const char* const kWidest[] = {"SETTING", "INSTRUMENT", "ACTION", "SCREEN", "CRYPTO", "FUTURE", "RECENT"};
    int w = 0;
    for (const char* s : kWidest)
        w = std::max(w, fm.horizontalAdvance(QString::fromLatin1(s)));
    return w + 2 * std::max(3, fm.averageCharWidth());
}

/// One line of text with emphasised spans, elided to `width`, drawn with QTextLayout (bidi-correct).
void sd_draw_text(QPainter* p, const QRect& rect, const QString& text, const QFont& font, const QColor& color,
                  const QList<command::rank::Span>& spans, const QColor& hl_color, Qt::LayoutDirection dir) {
    if (text.isEmpty() || rect.width() <= 0)
        return;
    const QFontMetrics fm(font);
    const QString shown = fm.elidedText(text, dir == Qt::RightToLeft ? Qt::ElideLeft : Qt::ElideRight, rect.width());
    QTextLayout layout(shown, font);
    QTextOption opt;
    opt.setTextDirection(dir);
    opt.setWrapMode(QTextOption::NoWrap);
    opt.setAlignment(Qt::AlignLeading);
    layout.setTextOption(opt);
    QList<QTextLayout::FormatRange> ranges;
    const int visible = shown == text ? static_cast<int>(text.size()) : std::max(0, static_cast<int>(shown.size()) - 1);
    for (const command::rank::Span& s : spans) {
        if (s.start >= visible || s.length <= 0)
            continue;
        QTextLayout::FormatRange r;
        r.start = s.start;
        r.length = std::min(s.length, visible - s.start);
        r.format.setForeground(hl_color);
        r.format.setFontWeight(QFont::Bold);
        ranges.append(r);
    }
    layout.setFormats(ranges);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    if (line.isValid())
        line.setLineWidth(rect.width());
    layout.endLayout();
    p->setPen(color);
    const qreal y = rect.top() + (rect.height() - fm.height()) / 2.0;
    layout.draw(p, QPointF(rect.left(), y));
}

} // namespace

struct SearchDelegate::Geometry {
    QRect chip;
    QRect primary;
    QRect secondary;
    QRect hint;
    QList<QRect> buttons; ///< parallel to the first actions
};

SearchDelegate::SearchDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

SearchDelegate::Geometry SearchDelegate::layout(const QStyleOptionViewItem& option, const SearchRow& row) const {
    Geometry g;
    const QRect r = option.rect;
    const int pad = sd_pad();
    const QFontMetrics fm_p(sd_primary_font());
    const QFontMetrics fm_s(sd_secondary_font());
    const QFontMetrics fm_b(sd_button_font());
    const int chip_w = sd_chip_width();
    int left = r.left() + pad + 3; // room for the selection bar
    g.chip = QRect(left, r.top() + pad, chip_w, fm_p.height());
    left += chip_w + pad;
    int right = r.right() - pad;

    const bool hovered = option.state & QStyle::State_MouseOver;
    if (hovered && row.kind == SearchRow::Kind::Item && !row.actions.isEmpty()) {
        const int bh = fm_b.height() + 4;
        const int by = r.top() + (r.height() - bh) / 2;
        const int n = std::min(kMaxButtons, static_cast<int>(row.actions.size()));
        QList<QRect> rects;
        for (int i = n - 1; i >= 0; --i) {
            const int bw = fm_b.horizontalAdvance(row.actions[i]) + 2 * fm_b.averageCharWidth();
            QRect b(right - bw + 1, by, bw, bh);
            if (b.left() < left + r.width() / 3)
                break; // never cover the title's first third
            rects.prepend(b);
            right = b.left() - 4;
        }
        g.buttons = rects;
    } else if (!row.hint.isEmpty()) {
        const QFontMetrics fm_h(sd_secondary_font());
        const int hw = std::min(fm_h.horizontalAdvance(row.hint) + pad, r.width() / 3);
        g.hint = QRect(right - hw + 1, r.top(), hw, r.height());
        right = g.hint.left() - pad;
    }
    const int text_w = std::max(0, right - left);
    if (row.kind == SearchRow::Kind::Item || row.kind == SearchRow::Kind::Ask) {
        g.primary = QRect(left, r.top() + pad, text_w, fm_p.height());
        g.secondary = QRect(left, g.primary.bottom() + 1, text_w, fm_s.height());
    } else {
        g.primary = QRect(left, r.top(), text_w, r.height());
    }
    // Right-to-left: mirror every rect inside the row.
    auto mirror = [&](QRect& x) { x = QStyle::visualRect(option.direction, r, x); };
    mirror(g.chip);
    mirror(g.primary);
    mirror(g.secondary);
    mirror(g.hint);
    for (QRect& b : g.buttons)
        mirror(b);
    return g;
}

QSize SearchDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    Q_UNUSED(option);
    const SearchRow* row = sd_row(index);
    const int pad = sd_pad();
    if (!row)
        return {0, 0};
    switch (row->kind) {
        case SearchRow::Kind::Heading:
            return {0, QFontMetrics(sd_heading_font()).height() + pad + pad / 2};
        case SearchRow::Kind::Item:
        case SearchRow::Kind::Ask:
            return {0, QFontMetrics(sd_primary_font()).height() + QFontMetrics(sd_secondary_font()).height() + 2 * pad + 1};
        default:
            return {0, QFontMetrics(sd_primary_font()).height() + pad + pad / 2};
    }
}

int SearchDelegate::action_at(const QStyleOptionViewItem& option, const QModelIndex& index, const QPoint& pos) const {
    const SearchRow* row = sd_row(index);
    if (!row)
        return -1;
    const Geometry g = layout(option, *row);
    for (int i = 0; i < g.buttons.size(); ++i)
        if (g.buttons[i].contains(pos))
            return i;
    return -1;
}

void SearchDelegate::paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const SearchRow* row = sd_row(index);
    if (!row)
        return;
    p->save();
    p->setRenderHint(QPainter::Antialiasing, false);
    const QRect r = option.rect;
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    const Qt::LayoutDirection dir = option.direction;
    const QColor primary(colors::TEXT_PRIMARY());
    const QColor secondary(colors::TEXT_SECONDARY());
    const QColor tertiary(colors::TEXT_TERTIARY());
    const QColor accent(colors::AMBER());

    if (row->kind == SearchRow::Kind::Heading) {
        const QFont f = sd_heading_font();
        p->setFont(f);
        const int pad = sd_pad();
        QRect tr = r.adjusted(pad + 3, pad / 2, -pad, 0);
        QString text = row->title.toUpper();
        if (row->count > 0)
            text += QStringLiteral("  ·  ") + QString::number(row->count);
        p->setPen(tertiary);
        p->drawText(QStyle::visualRect(dir, r, tr), Qt::AlignLeading | Qt::AlignVCenter, text);
        // hairline above every heading but the first
        if (index.row() > 0) {
            p->setPen(QColor(colors::BORDER_DIM()));
            p->drawLine(r.left(), r.top(), r.right(), r.top());
        }
        p->restore();
        return;
    }

    if (selected)
        p->fillRect(r, QColor(colors::BG_RAISED()));
    else if (hovered && row->selectable())
        p->fillRect(r, QColor(colors::BG_HOVER()));
    if (selected) {
        const QRect bar(dir == Qt::RightToLeft ? r.right() - 2 : r.left(), r.top(), 3, r.height());
        p->fillRect(bar, accent);
    }

    const Geometry g = layout(option, *row);

    if (row->kind == SearchRow::Kind::Loading) {
        // Static placeholder bars + caption (no animation needed: it is replaced within a second).
        const QColor ghost(colors::BORDER_MED());
        p->fillRect(QRect(g.chip.left(), g.chip.top() + g.chip.height() / 4, g.chip.width(), g.chip.height() / 2), ghost);
        sd_draw_text(p, g.primary, row->title, sd_secondary_font(), tertiary, {}, tertiary, dir);
        p->restore();
        return;
    }

    if (!row->chip.isEmpty()) {
        p->setFont(sd_chip_font());
        const QFontMetrics fm(sd_chip_font());
        QRect c = g.chip;
        c.setHeight(fm.height() + 2);
        c.moveTop(g.chip.top() + (g.chip.height() - c.height()) / 2);
        const QColor chip_fg = row->kind == SearchRow::Kind::Ask ? accent : secondary;
        p->setPen(QColor(colors::BORDER_MED()));
        p->setBrush(Qt::NoBrush);
        p->drawRect(c.adjusted(0, 0, -1, -1));
        p->setPen(chip_fg);
        p->drawText(c, Qt::AlignCenter, row->chip);
    }

    // Primary line: [context ›] [lead] title
    QRect line = g.primary;
    const QFont pf = sd_primary_font();
    auto advance = [&](const QString& text, const QFont& f, const QColor& c) {
        if (text.isEmpty() || line.width() <= 0)
            return;
        const QFontMetrics fm(f);
        const int w = std::min(fm.horizontalAdvance(text), line.width());
        QRect part = dir == Qt::RightToLeft ? QRect(line.right() - w + 1, line.top(), w, line.height())
                                            : QRect(line.left(), line.top(), w, line.height());
        sd_draw_text(p, part, text, f, c, {}, c, dir);
        const int gap = fm.averageCharWidth();
        if (dir == Qt::RightToLeft)
            line.setRight(part.left() - gap);
        else
            line.setLeft(part.right() + gap);
    };
    if (!row->context.isEmpty())
        advance(row->context + QStringLiteral(" ›"), sd_secondary_font(), tertiary);
    if (!row->lead.isEmpty())
        advance(row->lead, sd_lead_font(), row->kind == SearchRow::Kind::Item ? primary : accent);
    const QColor title_color = row->kind == SearchRow::Kind::More || row->kind == SearchRow::Kind::Hint ? secondary : primary;
    sd_draw_text(p, line, row->title, pf, title_color, row->highlight, accent, dir);

    if (!g.secondary.isEmpty() && !row->subtitle.isEmpty())
        sd_draw_text(p, g.secondary, row->subtitle, sd_secondary_font(), tertiary, {}, tertiary, dir);

    if (!g.hint.isEmpty()) {
        p->setFont(sd_secondary_font());
        p->setPen(QColor(colors::TEXT_DIM()));
        p->drawText(g.hint, Qt::AlignTrailing | Qt::AlignVCenter, row->hint);
    }

    if (!g.buttons.isEmpty()) {
        p->setFont(sd_button_font());
        for (int i = 0; i < g.buttons.size(); ++i) {
            const QRect b = g.buttons[i];
            p->setPen(QColor(colors::BORDER_BRIGHT()));
            p->setBrush(QColor(colors::BG_SURFACE()));
            p->drawRect(b.adjusted(0, 0, -1, -1));
            p->setPen(primary);
            p->drawText(b, Qt::AlignCenter, row->actions[i]);
        }
    }

    if (row->pinned) {
        // Small accent dot after the chip: pinned row.
        const int d = std::max(4, QFontMetrics(pf).height() / 4);
        const QRect dot(g.chip.center().x() - d / 2, g.chip.bottom() + 2, d, d);
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setPen(Qt::NoPen);
        p->setBrush(accent);
        p->drawEllipse(dot);
    }
    p->restore();
}

} // namespace fincept::ui::search
