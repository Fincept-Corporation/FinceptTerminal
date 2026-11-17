// src/screens/notes/NotesCardPainter.h
//
// How a note card looks, in one place: the board's delegate, the wall's items and
// the drag pixmap all paint through paint_card(). Every colour and font is read
// from the theme at paint time and every size comes from font metrics
// (CardMetrics::compute), so a theme or font change only needs a repaint and a
// re-measure — no stylesheet, nothing cached across appearances.
//
//   ┃ ★ Title ……………………………………………… ◆  3d
//   ┃ preview line one ……………………………………………
//   ┃ preview line two ……………………………………………
//   ┃ [$AAPL 189.20 +1.2%] [$MSFT 410.10 −0.3%] +2
//   ┃ #macro #semis            ACTIVE  DUE  ⇄ CHAT  ▲ BULL
//
// A card without tickers shows a third preview line in the chip row's place when
// it fits (font metrics); every card is CardMetrics::card_h tall either way.
// The left edge is the category colour; the priority marker is a shape plus a
// state colour (critical ◆ danger, high ▲ warning, medium ● info, low ○ dim);
// ticker changes use the price-direction colours.
#pragma once

#include "screens/notes/NotesStore.h"

#include <QColor>
#include <QDate>
#include <QFont>
#include <QHash>
#include <QRect>
#include <QString>
#include <QStringList>

class QPainter;

namespace fincept::screens::notes {

/// A ticker's quote as the cards show it.
struct CardQuote {
    double price = 0.0;
    double change_pct = 0.0;
    bool has_change = false;
    qint64 at_ms = 0;
    int flash = 0; ///< +1 / -1 for the last tick's direction (list view flash)
};

/// The quotes the screen holds for what is on screen (GUI thread only).
class QuoteBook {
  public:
    const CardQuote* find(const QString& symbol) const {
        auto it = map_.constFind(symbol);
        return it == map_.constEnd() ? nullptr : &it.value();
    }
    /// Store a quote; true when it differs from the one held.
    bool put(const QString& symbol, const CardQuote& q) {
        auto it = map_.find(symbol);
        if (it != map_.end() && it->price == q.price && it->change_pct == q.change_pct &&
            it->has_change == q.has_change)
            return false;
        CardQuote v = q;
        if (it != map_.end() && it->price > 0 && q.price != it->price)
            v.flash = q.price > it->price ? 1 : -1;
        map_.insert(symbol, v);
        return true;
    }
    /// Up / down today among `symbols` that have a quote with a change.
    QPair<int, int> up_down(const QStringList& symbols, int max_count = 200) const {
        int up = 0, down = 0, n = 0;
        for (const QString& s : symbols) {
            if (++n > max_count)
                break;
            const CardQuote* q = find(s);
            if (!q || !q->has_change)
                continue;
            if (q->change_pct > 0)
                ++up;
            else if (q->change_pct < 0)
                ++down;
        }
        return {up, down};
    }
    int size() const { return static_cast<int>(map_.size()); }
    void clear() { map_.clear(); }

  private:
    QHash<QString, CardQuote> map_;
};

/// Fonts and sizes of a card, a lane and its header at the current appearance.
struct CardMetrics {
    QFont title;
    QFont preview;
    QFont meta;
    QFont chip;
    QFont header;
    QFont header_count;
    int pad_h = 10;
    int pad_v = 7;
    int gap = 3;
    int edge = 3;          ///< category edge width
    int chip_h = 16;
    int card_w = 260;      ///< preferred card width (lane width minus padding)
    int card_h = 96;
    int card_gap = 6;      ///< between cards in a lane
    int lane_pad = 6;      ///< lane inner padding
    int lane_gap = 6;      ///< between lanes
    int lane_w_min = 220;
    int lane_w_max = 420;
    int header_h = 30;
    int collapsed_w = 30;
    int add_row_h = 26;    ///< "+ new card" row under a lane's cards
    int preview_lines = 2;

    static CardMetrics compute();
};

struct CardState {
    bool selected = false;
    bool current = false;     ///< keyboard focus card
    bool view_focused = false;
    bool hover = false;
    bool drop_target = false; ///< a symbol is being dropped on it
    bool show_status = false; ///< status badge in the meta line (not when lanes are statuses)
    int lod = 0;              ///< 0 full, 1 title only (zoomed out), 2 block
};

/// Category edge colour (fixed slot per known category, hashed for others).
QColor category_color(const QString& category_key);
/// Accent of a status lane / badge (a state colour, never a price-direction one).
QColor status_color(const QString& status);
/// Translated, upper-case status name ("IDEA", "IN POSITION" …).
QString status_label(const QString& status);
/// Translated, upper-case priority name; empty code -> "NO PRIORITY".
QString priority_label(const QString& code);

/// Paint one card into `r` (the card's full rect).
void paint_card(QPainter* p, const QRect& r, const NoteEntry& e, const CardMetrics& m, const CardState& st,
                const QuoteBook* quotes, const QStringList& highlight, qint64 now_ms, const QDate& today);

/// A dashed placeholder card ("+ New card", a template suggestion).
void paint_ghost_card(QPainter* p, const QRect& r, const CardMetrics& m, const QString& title, const QString& hint,
                      bool hover);

/// The priority marker shape in `box` (rank 4..1; 0 draws nothing).
void paint_priority_marker(QPainter* p, const QRectF& box, int rank);

/// `text` elided into `rect`, with every search term on a highlight wash.
void paint_highlighted(QPainter* p, const QRect& rect, const QFont& font, const QColor& fg, const QString& text,
                       const QStringList& terms, Qt::TextElideMode elide = Qt::ElideRight);

} // namespace fincept::screens::notes
