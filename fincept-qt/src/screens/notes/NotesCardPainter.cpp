// src/screens/notes/NotesCardPainter.cpp
#include "screens/notes/NotesCardPainter.h"

#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace fincept::screens::notes {

namespace colors = fincept::ui::colors;
namespace fonts = fincept::ui::fonts;

namespace {

QColor ncp_token(const fincept::ui::ColorToken& t) {
    return QColor(t.get());
}

QString ncp_tr(const char* text) {
    return QCoreApplication::translate("NotesBoard", text);
}

/// Up to `max_lines` lines of `text` within `width`: each but the last broken at
/// a space, the last elided.
QStringList ncp_wrap(const QString& text, const QFontMetrics& fm, int width, int max_lines) {
    QStringList lines;
    if (text.isEmpty() || width <= 8 || max_lines <= 0)
        return lines;
    QString rest = text.trimmed();
    while (!rest.isEmpty()) { // ends: every pass adds a line or stops at max_lines
        if (lines.size() + 1 >= max_lines || fm.horizontalAdvance(rest) <= width) {
            lines << fm.elidedText(rest, Qt::ElideRight, width);
            break;
        }
        // Largest prefix that fits (binary search on the advance).
        int lo = 1;
        int hi = static_cast<int>(std::min<qsizetype>(rest.size(), 400));
        while (lo < hi) {
            const int mid = (lo + hi + 1) / 2;
            if (fm.horizontalAdvance(rest.left(mid)) <= width)
                lo = mid;
            else
                hi = mid - 1;
        }
        int cut = static_cast<int>(rest.lastIndexOf(QLatin1Char(' '), lo));
        if (cut < lo / 2)
            cut = lo;
        lines << rest.left(cut).trimmed();
        rest = rest.mid(cut).trimmed();
    }
    return lines;
}

QString ncp_tag_line(const QStringList& tags) {
    if (tags.isEmpty())
        return {};
    return QLatin1Char('#') + tags.join(QStringLiteral("  #"));
}

} // namespace

// ── Labels / colours ─────────────────────────────────────────────────────────

QColor category_color(const QString& category_key) {
    static const char* const kKnown[] = {"TRADE_IDEA", "RESEARCH", "MARKET_ANALYSIS", "EARNINGS",
                                         "ECONOMIC",   "PORTFOLIO"};
    const QString k = category_key.trimmed().toUpper();
    if (k.isEmpty() || k == QLatin1String("GENERAL"))
        return ncp_token(colors::TEXT_TERTIARY);
    for (int i = 0; i < 6; ++i)
        if (k == QLatin1String(kKnown[i]))
            return fincept::ui::colors::chart_color(i);
    return fincept::ui::colors::chart_color(6 + static_cast<int>(qHash(k) % 6));
}

QColor status_color(const QString& status) {
    namespace nb = fincept::notes_board;
    if (status == nb::kStatusIdea)
        return ncp_token(colors::INFO);
    if (status == nb::kStatusResearching)
        return ncp_token(colors::CYAN);
    if (status == nb::kStatusActive)
        return ncp_token(colors::AMBER);
    if (status == nb::kStatusReview)
        return ncp_token(colors::WARNING);
    if (status == nb::kStatusClosed)
        return ncp_token(colors::TEXT_TERTIARY);
    return ncp_token(colors::TEXT_DIM);
}

QString status_label(const QString& status) {
    namespace nb = fincept::notes_board;
    if (status == nb::kStatusIdea)
        return ncp_tr("IDEA");
    if (status == nb::kStatusResearching)
        return ncp_tr("RESEARCHING");
    if (status == nb::kStatusActive)
        return ncp_tr("ACTIVE / IN POSITION");
    if (status == nb::kStatusReview)
        return ncp_tr("REVIEW");
    if (status == nb::kStatusClosed)
        return ncp_tr("CLOSED");
    if (status == QLatin1String("ARCHIVED"))
        return ncp_tr("ARCHIVED");
    return status.toUpper();
}

QString priority_label(const QString& code) {
    if (code == QLatin1String("CRITICAL"))
        return ncp_tr("CRITICAL");
    if (code == QLatin1String("HIGH"))
        return ncp_tr("HIGH");
    if (code == QLatin1String("MEDIUM"))
        return ncp_tr("MEDIUM");
    if (code == QLatin1String("LOW"))
        return ncp_tr("LOW");
    return ncp_tr("NO PRIORITY");
}

// ── Metrics ──────────────────────────────────────────────────────────────────

CardMetrics CardMetrics::compute() {
    CardMetrics m;
    m.title = fonts::font(-1, QFont::DemiBold);
    m.preview = fonts::font(-3);
    m.meta = fonts::font(-4);
    m.chip = fonts::font(-4, QFont::DemiBold);
    m.header = fonts::font(-3, QFont::Bold);
    m.header.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    m.header_count = fonts::font(-4, QFont::DemiBold);
    const QFontMetrics ft(m.title), fp(m.preview), fm(m.meta), fc(m.chip), fh(m.header);
    m.pad_h = std::max(8, fp.averageCharWidth() + 3);
    m.pad_v = std::max(6, fp.height() / 2);
    m.gap = std::max(2, fp.height() / 5);
    m.edge = 3;
    m.chip_h = fc.height() + 4;
    m.card_h = m.pad_v * 2 + ft.height() + m.gap + m.preview_lines * fp.height() + m.gap + m.chip_h + m.gap +
               fm.height() + 1;
    m.card_gap = std::max(5, fp.height() / 2);
    m.lane_pad = m.card_gap;
    m.lane_gap = m.card_gap;
    m.lane_w_min = std::max(200, fp.averageCharWidth() * 30 + 2 * (m.pad_h + m.lane_pad));
    m.lane_w_max = m.lane_w_min * 17 / 10;
    m.card_w = m.lane_w_min - 2 * m.lane_pad;
    m.header_h = std::max(30, fh.height() + 16);
    m.collapsed_w = m.header_h;
    m.add_row_h = std::max(22, fm.height() + 10);
    return m;
}

// ── Pieces ───────────────────────────────────────────────────────────────────

void paint_highlighted(QPainter* p, const QRect& rect, const QFont& font, const QColor& fg, const QString& text,
                       const QStringList& terms, Qt::TextElideMode elide) {
    if (rect.width() <= 4 || text.isEmpty())
        return;
    p->setFont(font);
    const QFontMetrics fm(font);
    const QString shown = fm.elidedText(text, elide, rect.width());
    const int top = rect.top() + (rect.height() - fm.height()) / 2;
    const int baseline = top + fm.ascent();
    p->setPen(fg);
    const auto ranges = terms.isEmpty() ? QVector<QPair<int, int>>() : match_ranges(shown, terms, 40);
    if (ranges.isEmpty()) {
        p->drawText(QPoint(rect.left(), baseline), shown);
        return;
    }
    QColor wash = ncp_token(colors::AMBER);
    wash.setAlphaF(0.35f);
    int x = rect.left();
    int pos = 0;
    const auto segment = [&](int from, int len, bool hit) {
        if (len <= 0)
            return;
        const QString s = shown.mid(from, len);
        const int w = fm.horizontalAdvance(s);
        if (hit)
            p->fillRect(QRect(x, top, w, fm.height()), wash);
        p->setPen(fg);
        p->drawText(QPoint(x, baseline), s);
        x += w;
    };
    for (const auto& r : ranges) {
        segment(pos, r.first - pos, false);
        segment(r.first, r.second, true);
        pos = r.first + r.second;
    }
    segment(pos, static_cast<int>(shown.size()) - pos, false);
}

void paint_priority_marker(QPainter* p, const QRectF& box, int rank) {
    if (rank <= 0)
        return;
    const double s = std::min(box.width(), box.height());
    const QRectF b(box.center().x() - s / 2, box.center().y() - s / 2, s, s);
    QPainterPath path;
    QColor c;
    bool fill = true;
    switch (rank) {
    case 4: // critical: diamond
        path.moveTo(b.center().x(), b.top());
        path.lineTo(b.right(), b.center().y());
        path.lineTo(b.center().x(), b.bottom());
        path.lineTo(b.left(), b.center().y());
        path.closeSubpath();
        c = ncp_token(colors::DANGER);
        break;
    case 3: // high: triangle
        path.moveTo(b.center().x(), b.top() + s * 0.08);
        path.lineTo(b.right(), b.bottom() - s * 0.08);
        path.lineTo(b.left(), b.bottom() - s * 0.08);
        path.closeSubpath();
        c = ncp_token(colors::WARNING);
        break;
    case 2: // medium: dot
        path.addEllipse(b.adjusted(s * 0.15, s * 0.15, -s * 0.15, -s * 0.15));
        c = ncp_token(colors::INFO);
        break;
    default: // low: ring
        path.addEllipse(b.adjusted(s * 0.2, s * 0.2, -s * 0.2, -s * 0.2));
        c = ncp_token(colors::TEXT_TERTIARY);
        fill = false;
        break;
    }
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    if (fill)
        p->fillPath(path, c);
    else {
        p->setPen(QPen(c, 1.2));
        p->setBrush(Qt::NoBrush);
        p->drawPath(path);
    }
    p->restore();
}

void paint_ghost_card(QPainter* p, const QRect& r, const CardMetrics& m, const QString& title, const QString& hint,
                      bool hover) {
    p->save();
    QPen pen(hover ? ncp_token(colors::AMBER_DIM) : ncp_token(colors::BORDER_MED), 1, Qt::DashLine);
    p->setPen(pen);
    p->setBrush(hover ? QBrush(fincept::ui::colors::overlay_color(0.03)) : Qt::NoBrush);
    p->drawRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5));
    const QFontMetrics ft(m.title), fm(m.meta);
    const QRect inner = r.adjusted(m.pad_h, m.pad_v, -m.pad_h, -m.pad_v);
    p->setFont(m.title);
    p->setPen(hover ? ncp_token(colors::AMBER) : ncp_token(colors::TEXT_TERTIARY));
    p->drawText(QRect(inner.left(), inner.top(), inner.width(), ft.height()), Qt::AlignLeft | Qt::AlignVCenter,
                ft.elidedText(title, Qt::ElideRight, inner.width()));
    if (!hint.isEmpty()) {
        p->setFont(m.meta);
        p->setPen(ncp_token(colors::TEXT_TERTIARY));
        const QRect hr(inner.left(), inner.top() + ft.height() + m.gap, inner.width(),
                       inner.bottom() - (inner.top() + ft.height() + m.gap));
        p->drawText(hr, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, hint);
    }
    p->restore();
}

// ── The card ─────────────────────────────────────────────────────────────────

void paint_card(QPainter* p, const QRect& r, const NoteEntry& e, const CardMetrics& m, const CardState& st,
                const QuoteBook* quotes, const QStringList& highlight, qint64 now_ms, const QDate& today) {
    namespace nb = fincept::notes_board;
    p->save();
    if (e.note.is_archived)
        p->setOpacity(0.55);
    const QColor edge = category_color(e.category);
    const QColor bg = st.selected ? ncp_token(colors::BG_RAISED)
                      : st.hover  ? ncp_token(colors::BG_HOVER)
                                  : ncp_token(colors::BG_SURFACE);

    if (st.lod >= 2) {
        p->fillRect(r, bg);
        p->fillRect(QRect(r.left(), r.top(), std::max(2, r.width() / 12), r.height()), edge);
        if (st.selected) {
            p->setPen(ncp_token(colors::AMBER));
            p->drawRect(r.adjusted(0, 0, -1, -1));
        }
        p->restore();
        return;
    }

    p->fillRect(r, bg);
    p->fillRect(QRect(r.left(), r.top(), m.edge, r.height()), edge);
    // Border: accent when selected, accent-dim for the keyboard focus card.
    QColor border = ncp_token(colors::BORDER_DIM);
    if (st.drop_target)
        border = ncp_token(colors::AMBER);
    else if (st.selected)
        border = ncp_token(colors::AMBER);
    else if (st.current && st.view_focused)
        border = ncp_token(colors::AMBER_DIM);
    p->setPen(border);
    p->drawRect(r.adjusted(0, 0, -1, -1));
    if (st.current && st.view_focused) {
        p->setPen(QPen(ncp_token(colors::AMBER), 1, Qt::DotLine));
        p->drawRect(r.adjusted(2, 2, -3, -3));
    }
    if (st.drop_target) {
        QColor wash = ncp_token(colors::AMBER);
        wash.setAlphaF(0.10f);
        p->fillRect(r.adjusted(1, 1, -1, -1), wash);
    }

    const QFontMetrics ft(m.title), fp(m.preview), fm(m.meta), fc(m.chip);
    const int left = r.left() + m.edge + m.pad_h;
    const int right = r.right() - m.pad_h;
    int y = r.top() + m.pad_v;

    // ── Line 1: pin, title, priority, age ───────────────────────────────────
    const QString age = relative_time(e.updated_ms, now_ms);
    const int age_w = fm.horizontalAdvance(age);
    p->setFont(m.meta);
    p->setPen(ncp_token(colors::TEXT_TERTIARY));
    p->drawText(QRect(right - age_w + 1, y, age_w, ft.height()), Qt::AlignRight | Qt::AlignVCenter, age);
    int title_right = right - age_w - 6;
    if (e.priority > 0) {
        const int s = std::max(7, fm.height() * 3 / 4);
        paint_priority_marker(p, QRectF(title_right - s, y + (ft.height() - s) / 2.0, s, s), e.priority);
        title_right -= s + 6;
    }
    int tx = left;
    if (e.note.is_favorite) {
        const QString star = QStringLiteral("★");
        p->setFont(m.title);
        p->setPen(ncp_token(colors::AMBER));
        const int w = ft.horizontalAdvance(star);
        p->drawText(QRect(tx, y, w, ft.height()), Qt::AlignLeft | Qt::AlignVCenter, star);
        tx += w + std::max(3, ft.averageCharWidth() / 2);
    }
    const QString title = e.note.title.isEmpty() ? ncp_tr("Untitled note") : e.note.title;
    paint_highlighted(p, QRect(tx, y, std::max(0, title_right - tx), ft.height()), m.title,
                      e.note.title.isEmpty() ? ncp_token(colors::TEXT_TERTIARY) : ncp_token(colors::TEXT_PRIMARY), title,
                      highlight);
    y += ft.height() + m.gap;
    if (st.lod == 1) {
        p->restore();
        return;
    }

    // ── Lines 2-3 (4): preview ──────────────────────────────────────────────
    // A card without ticker chips lends their row to one more preview line when
    // the font metrics say it fits there; the card's height (and so every lane's
    // layout and virtualisation) stays CardMetrics::card_h.
    const int text_w = right - left;
    const bool chip_row_free = e.ticker_list.isEmpty() && m.chip_h + 2 * m.gap >= fp.height();
    const int preview_lines = m.preview_lines + (chip_row_free ? 1 : 0);
    if (e.preview.isEmpty()) {
        p->setFont(m.preview);
        p->setPen(ncp_token(colors::TEXT_TERTIARY));
        p->drawText(QRect(left, y, text_w, fp.height()), Qt::AlignLeft | Qt::AlignVCenter, ncp_tr("No text"));
    } else {
        const QStringList lines = ncp_wrap(e.preview, fp, text_w, preview_lines);
        for (int i = 0; i < lines.size() && i < preview_lines; ++i)
            paint_highlighted(p, QRect(left, y + i * fp.height(), text_w, fp.height()), m.preview,
                              ncp_token(colors::TEXT_SECONDARY), lines.at(i), highlight, Qt::ElideNone);
    }
    y += m.preview_lines * fp.height() + m.gap;

    // ── Line 4: ticker chips with live quotes ───────────────────────────────
    {
        p->setRenderHint(QPainter::Antialiasing, false);
        int x = left;
        const int n = static_cast<int>(e.ticker_list.size());
        for (int i = 0; i < n; ++i) {
            const QString sym = e.ticker_list.at(i);
            const QString s1 = QLatin1Char('$') + sym;
            QString s2, s3;
            QColor c3;
            if (quotes) {
                if (const CardQuote* q = quotes->find(sym)) {
                    if (q->price > 0)
                        s2 = fincept::ui::formatting::price(q->price);
                    if (q->has_change) {
                        s3 = fincept::ui::formatting::pct(q->change_pct, 2, true);
                        c3 = q->change_pct > 0   ? ncp_token(colors::POSITIVE)
                             : q->change_pct < 0 ? ncp_token(colors::NEGATIVE)
                                                 : ncp_token(colors::TEXT_TERTIARY);
                    }
                }
            }
            const int sp = fc.horizontalAdvance(QLatin1Char(' '));
            const int w1 = fc.horizontalAdvance(s1);
            const int w2 = s2.isEmpty() ? 0 : sp + fc.horizontalAdvance(s2);
            const int w3 = s3.isEmpty() ? 0 : sp + fc.horizontalAdvance(s3);
            const int cw = 8 + w1 + w2 + w3;
            const QString more = QStringLiteral("+%1").arg(n - i);
            const int more_w = fm.horizontalAdvance(more) + 4;
            const bool last = i == n - 1;
            if (x + cw > right - (last ? 0 : more_w)) {
                p->setFont(m.meta);
                p->setPen(ncp_token(colors::TEXT_TERTIARY));
                p->drawText(QRect(x, y, more_w + 2, m.chip_h), Qt::AlignLeft | Qt::AlignVCenter, more);
                break;
            }
            const QRect chip(x, y, cw, m.chip_h);
            p->fillRect(chip, st.selected ? ncp_token(colors::BG_BASE) : ncp_token(colors::BG_RAISED));
            p->setPen(ncp_token(colors::BORDER_MED));
            p->drawRect(chip.adjusted(0, 0, -1, -1));
            p->setFont(m.chip);
            int cx = x + 4;
            const int base = y + (m.chip_h - fc.height()) / 2 + fc.ascent();
            p->setPen(ncp_token(colors::AMBER));
            p->drawText(QPoint(cx, base), s1);
            cx += w1;
            if (!s2.isEmpty()) {
                p->setPen(ncp_token(colors::TEXT_SECONDARY));
                p->drawText(QPoint(cx + sp, base), s2);
                cx += w2;
            }
            if (!s3.isEmpty()) {
                p->setPen(c3);
                p->drawText(QPoint(cx + sp, base), s3);
            }
            x += cw + 4;
        }
    }
    y += m.chip_h + m.gap;

    // ── Line 5: tags | status, review, linked, sentiment ────────────────────
    {
        struct Badge {
            QString text;
            QColor fg;
            bool filled = false;
        };
        QVector<Badge> badges; // right to left
        const QString sent = e.note.sentiment.trimmed().toUpper();
        if (sent == QLatin1String("BULLISH"))
            badges << Badge{QStringLiteral("▲ ") + ncp_tr("BULL"), ncp_token(colors::POSITIVE)};
        else if (sent == QLatin1String("BEARISH"))
            badges << Badge{QStringLiteral("▼ ") + ncp_tr("BEAR"), ncp_token(colors::NEGATIVE)};
        if (e.linked)
            badges << Badge{QStringLiteral("⇄ ") + ncp_tr("CHAT"), ncp_token(colors::CYAN)};
        if (e.review.isValid() && is_open(e)) {
            if (e.review < today)
                badges << Badge{ncp_tr("OVERDUE"), ncp_token(colors::DANGER), true};
            else if (e.review == today)
                badges << Badge{ncp_tr("DUE"), ncp_token(colors::WARNING), true};
            else
                badges << Badge{QStringLiteral("◷ ") + e.review.toString(QStringLiteral("MMM d")),
                                ncp_token(colors::TEXT_TERTIARY)};
        }
        if (st.show_status && !e.note.is_archived)
            badges << Badge{status_label(e.meta.status).section(QLatin1Char(' '), 0, 0), status_color(e.meta.status)};
        if (e.note.is_archived)
            badges << Badge{ncp_tr("ARCHIVED"), ncp_token(colors::TEXT_TERTIARY)};
        p->setFont(m.meta);
        int bx = right + 1;
        const int line_h = fm.height();
        for (const Badge& b : badges) {
            const int w = fm.horizontalAdvance(b.text) + (b.filled ? 8 : 0);
            if (bx - w < left + 20)
                break;
            bx -= w;
            const QRect br(bx, y, w, line_h);
            if (b.filled) {
                p->fillRect(br, fincept::ui::colors::tint_color(b.fg, 0.28));
                p->setPen(b.fg);
                p->drawText(br, Qt::AlignCenter, b.text);
            } else {
                p->setPen(b.fg);
                p->drawText(br, Qt::AlignRight | Qt::AlignVCenter, b.text);
            }
            bx -= std::max(6, fm.averageCharWidth());
        }
        const QString tags = ncp_tag_line(e.tag_list);
        if (!tags.isEmpty() && bx - left > 16) {
            p->setPen(ncp_token(colors::TEXT_TERTIARY));
            paint_highlighted(p, QRect(left, y, bx - left - 4, line_h), m.meta, ncp_token(colors::TEXT_TERTIARY), tags,
                              highlight);
        }
    }
    p->restore();
}

} // namespace fincept::screens::notes
