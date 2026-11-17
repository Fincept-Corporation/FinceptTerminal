// src/screens/notes/NotesBoardView.cpp
#include "screens/notes/NotesBoardView.h"

#include "core/symbol/SymbolMime.h"
#include "ui/theme/Theme.h"

#include <QAccessible>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace fincept::screens::notes {

namespace colors = fincept::ui::colors;

namespace {

QColor nbv_token(const fincept::ui::ColorToken& t) {
    return QColor(t.get());
}

} // namespace

NotesBoardView::NotesBoardView(QWidget* parent) : QAbstractItemView(parent) {
    setObjectName(QStringLiteral("notesBoardView"));
    delegate_ = new NotesCardDelegate(this);
    setItemDelegate(delegate_);
    m_ = delegate_->metrics();
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectItems);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setDragEnabled(true);
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    setDropIndicatorShown(false);
    setMouseTracking(true);
    viewport()->setAttribute(Qt::WA_Hover, true);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::StrongFocus);
    setAutoScroll(false);

    tickers_timer_ = new QTimer(this);
    tickers_timer_->setSingleShot(true);
    tickers_timer_->setInterval(180);
    connect(tickers_timer_, &QTimer::timeout, this, &NotesBoardView::emit_tickers);
    autoscroll_ = new QTimer(this);
    autoscroll_->setInterval(40);
    connect(autoscroll_, &QTimer::timeout, this, &NotesBoardView::autoscroll_tick);
}

NotesBoardView::~NotesBoardView() = default;

void NotesBoardView::setModel(QAbstractItemModel* model) {
    model_ = qobject_cast<NotesBoardModel*>(model);
    QAbstractItemView::setModel(model);
    if (!model_)
        return;
    const auto relayout_later = [this]() {
        relayout();
        viewport()->update();
        schedule_tickers();
    };
    connect(model_, &QAbstractItemModel::modelReset, this, relayout_later);
    connect(model_, &QAbstractItemModel::layoutChanged, this, relayout_later);
    connect(model_, &QAbstractItemModel::rowsRemoved, this, relayout_later);
    connect(model_, &QAbstractItemModel::rowsInserted, this, relayout_later);
    connect(model_, &QAbstractItemModel::dataChanged, this, [this]() {
        viewport()->update();
        schedule_tickers();
    });
    relayout();
}

void NotesBoardView::refit() {
    delegate_->set_metrics(CardMetrics::compute());
    m_ = delegate_->metrics();
    relayout();
    viewport()->update();
}

void NotesBoardView::set_show_status(bool on) {
    show_status_ = on;
    delegate_->set_show_status(on);
    viewport()->update();
}

void NotesBoardView::set_quotes(const QuoteBook* quotes) {
    quotes_ = quotes;
    delegate_->set_quotes(quotes);
}

void NotesBoardView::set_ghost_templates(const QHash<QString, GhostTemplate>& ghosts) {
    ghosts_ = ghosts;
    viewport()->update();
}

void NotesBoardView::set_hint_text(const QString& add_card, const QString& new_card, const QString& empty_lane) {
    add_card_text_ = add_card;
    new_card_text_ = new_card;
    empty_lane_text_ = empty_lane;
    viewport()->update();
}

// ── Geometry ─────────────────────────────────────────────────────────────────

int NotesBoardView::step() const {
    return m_.card_h + m_.card_gap;
}

void NotesBoardView::relayout() {
    const int n = model_ ? model_->lane_count() : 0;
    const QHash<QString, int> keep = lane_scrolls();
    geo_.resize(n);
    scroll_.resize(n);
    const int vw = std::max(1, viewport()->width());
    int expanded = 0;
    int fixed = m_.lane_gap;
    for (int i = 0; i < n; ++i) {
        const bool c = model_->lane(i).collapsed;
        geo_[i].collapsed = c;
        if (c)
            fixed += m_.collapsed_w + m_.lane_gap;
        else
            ++expanded;
    }
    fixed += expanded * m_.lane_gap;
    int w = m_.lane_w_min;
    if (expanded > 0) {
        const int share = (vw - fixed) / expanded;
        if (share > w)
            w = std::min(m_.lane_w_max, share);
    }
    int x = m_.lane_gap;
    for (int i = 0; i < n; ++i) {
        geo_[i].x = x;
        geo_[i].w = geo_[i].collapsed ? m_.collapsed_w : w;
        x += geo_[i].w + m_.lane_gap;
    }
    content_w_ = x;
    geo_keys_.clear();
    for (int i = 0; i < n; ++i)
        geo_keys_ << model_->lane(i).key;
    QScrollBar* hs = horizontalScrollBar();
    hs->setRange(0, std::max(0, content_w_ - vw));
    hs->setPageStep(vw);
    hs->setSingleStep(std::max(20, w / 4));
    for (int i = 0; i < n; ++i)
        scroll_[i] = std::clamp(keep.value(model_->lane(i).key, 0), 0, max_scroll(i));
}

void NotesBoardView::updateGeometries() {
    relayout();
    QAbstractItemView::updateGeometries();
}

int NotesBoardView::lane_content_h(int lane) const {
    if (!model_ || lane < 0 || lane >= model_->lane_count())
        return 0;
    const int n = static_cast<int>(model_->lane(lane).cards.size());
    const int cards = n > 0 ? n * step() - m_.card_gap : m_.card_h; // an empty lane shows a ghost card
    return m_.lane_pad * 2 + cards + m_.card_gap + m_.add_row_h;
}

int NotesBoardView::max_scroll(int lane) const {
    const int body_h = viewport()->height() - m_.header_h;
    return std::max(0, lane_content_h(lane) - body_h);
}

void NotesBoardView::set_lane_scroll(int lane, int value) {
    if (lane < 0 || lane >= scroll_.size())
        return;
    const int v = std::clamp(value, 0, max_scroll(lane));
    if (v == scroll_.at(lane))
        return;
    scroll_[lane] = v;
    viewport()->update(lane_rect(lane));
    schedule_tickers();
}

QHash<QString, int> NotesBoardView::lane_scrolls() const {
    // Keyed by the lanes the scroll positions were measured for (the model may
    // already hold a new set of lanes when this runs after a reset).
    QHash<QString, int> out;
    for (int i = 0; i < scroll_.size() && i < geo_keys_.size(); ++i)
        if (scroll_.at(i) > 0)
            out.insert(geo_keys_.at(i), scroll_.at(i));
    return out;
}

void NotesBoardView::restore_lane_scrolls(const QHash<QString, int>& scrolls) {
    if (!model_)
        return;
    for (int i = 0; i < model_->lane_count() && i < scroll_.size(); ++i)
        scroll_[i] = std::clamp(scrolls.value(model_->lane(i).key, 0), 0, max_scroll(i));
    viewport()->update();
    schedule_tickers();
}

QRect NotesBoardView::lane_rect(int lane) const {
    if (lane < 0 || lane >= geo_.size())
        return {};
    return QRect(geo_.at(lane).x - horizontalOffset(), 0, geo_.at(lane).w, viewport()->height());
}

QRect NotesBoardView::header_rect(int lane) const {
    const QRect r = lane_rect(lane);
    if (geo_.value(lane).collapsed)
        return r;
    return QRect(r.left(), r.top(), r.width(), m_.header_h);
}

QRect NotesBoardView::body_rect(int lane) const {
    const QRect r = lane_rect(lane);
    if (r.isNull() || geo_.value(lane).collapsed)
        return {};
    return QRect(r.left(), r.top() + m_.header_h, r.width(), std::max(0, r.height() - m_.header_h));
}

QRect NotesBoardView::chevron_rect(int lane) const {
    const QRect h = header_rect(lane);
    if (geo_.value(lane).collapsed)
        return QRect(h.left(), h.top(), h.width(), m_.header_h);
    return QRect(h.left(), h.top(), m_.header_h * 3 / 4, m_.header_h);
}

QRect NotesBoardView::plus_rect(int lane) const {
    const QRect h = header_rect(lane);
    if (geo_.value(lane).collapsed)
        return {};
    return QRect(h.right() - m_.header_h + 1, h.top(), m_.header_h, m_.header_h);
}

QRect NotesBoardView::card_rect(int lane, int row) const {
    const QRect b = body_rect(lane);
    if (b.isNull())
        return {};
    return QRect(b.left() + m_.lane_pad, b.top() + m_.lane_pad + row * step() - scroll_.value(lane),
                 b.width() - 2 * m_.lane_pad, m_.card_h);
}

QRect NotesBoardView::add_row_rect(int lane) const {
    if (!model_ || lane < 0 || lane >= model_->lane_count())
        return {};
    const int n = static_cast<int>(model_->lane(lane).cards.size());
    const QRect last = card_rect(lane, std::max(0, n - 1));
    if (last.isNull())
        return {};
    return QRect(last.left(), last.bottom() + 1 + m_.card_gap, last.width(), m_.add_row_h);
}

QRect NotesBoardView::scrollbar_rect(int lane) const {
    const QRect b = body_rect(lane);
    if (b.isNull() || max_scroll(lane) <= 0)
        return {};
    return QRect(b.right() - 4, b.top() + 2, 4, b.height() - 4);
}

int NotesBoardView::lane_at(int viewport_x) const {
    const int x = viewport_x + horizontalOffset();
    for (int i = 0; i < geo_.size(); ++i)
        if (x >= geo_.at(i).x && x < geo_.at(i).x + geo_.at(i).w)
            return i;
    return -1;
}

NotesBoardView::HitResult NotesBoardView::hit_test(const QPoint& pos) const {
    HitResult h;
    h.lane = lane_at(pos.x());
    if (h.lane < 0 || !model_)
        return h;
    if (geo_.at(h.lane).collapsed) {
        h.kind = Hit::Chevron;
        return h;
    }
    if (pos.y() < m_.header_h) {
        if (chevron_rect(h.lane).contains(pos))
            h.kind = Hit::Chevron;
        else if (plus_rect(h.lane).contains(pos))
            h.kind = Hit::Plus;
        else
            h.kind = Hit::Header;
        return h;
    }
    const QRect sb = scrollbar_rect(h.lane);
    if (!sb.isNull() && pos.x() >= sb.left() - 3) {
        h.kind = Hit::Scrollbar;
        return h;
    }
    const int n = static_cast<int>(model_->lane(h.lane).cards.size());
    if (n == 0) {
        if (card_rect(h.lane, 0).contains(pos)) {
            h.kind = Hit::Ghost;
            return h;
        }
    } else {
        const QRect b = body_rect(h.lane);
        const int local = pos.y() - b.top() - m_.lane_pad + scroll_.value(h.lane);
        if (local >= 0) {
            const int row = local / step();
            if (row < n && local - row * step() < m_.card_h && card_rect(h.lane, row).contains(pos)) {
                h.kind = Hit::Card;
                h.row = row;
                return h;
            }
        }
    }
    if (add_row_rect(h.lane).contains(pos)) {
        h.kind = Hit::AddRow;
        return h;
    }
    h.kind = Hit::Body;
    return h;
}

QRect NotesBoardView::visualRect(const QModelIndex& index) const {
    if (!index.isValid() || !model_)
        return {};
    if (index.internalId() == 0)
        return header_rect(index.row());
    const int lane = static_cast<int>(index.internalId() - 1);
    if (lane < 0 || lane >= geo_.size() || geo_.at(lane).collapsed)
        return {};
    return card_rect(lane, index.row());
}

QModelIndex NotesBoardView::indexAt(const QPoint& point) const {
    if (!model_)
        return {};
    const HitResult h = hit_test(point);
    if (h.kind == Hit::Card)
        return model_->index(h.row, 0, model_->lane_index(h.lane));
    return {};
}

void NotesBoardView::scrollTo(const QModelIndex& index, ScrollHint hint) {
    if (!index.isValid() || !model_)
        return;
    const int lane = index.internalId() == 0 ? index.row() : static_cast<int>(index.internalId() - 1);
    if (lane < 0 || lane >= geo_.size())
        return;
    // Horizontally: the whole lane in view.
    QScrollBar* hs = horizontalScrollBar();
    const int lx = geo_.at(lane).x;
    const int lw = geo_.at(lane).w;
    const int vw = viewport()->width();
    if (lx - m_.lane_gap < hs->value())
        hs->setValue(lx - m_.lane_gap);
    else if (lx + lw + m_.lane_gap > hs->value() + vw)
        hs->setValue(std::min(lx - m_.lane_gap, lx + lw + m_.lane_gap - vw));
    if (index.internalId() == 0 || geo_.at(lane).collapsed)
        return;
    // Vertically within the lane.
    const int body_h = viewport()->height() - m_.header_h;
    const int top = m_.lane_pad + index.row() * step();
    const int bottom = top + m_.card_h;
    int s = scroll_.value(lane);
    if (hint == PositionAtTop)
        s = top - m_.lane_pad;
    else if (hint == PositionAtCenter)
        s = top - (body_h - m_.card_h) / 2;
    else if (top - m_.lane_pad < s)
        s = top - m_.lane_pad;
    else if (bottom + m_.lane_pad > s + body_h)
        s = bottom + m_.lane_pad - body_h;
    set_lane_scroll(lane, s);
}

int NotesBoardView::horizontalOffset() const {
    return horizontalScrollBar()->value();
}

int NotesBoardView::verticalOffset() const {
    return 0; // every lane scrolls on its own
}

bool NotesBoardView::isIndexHidden(const QModelIndex& index) const {
    if (!index.isValid() || index.internalId() == 0)
        return false;
    const int lane = static_cast<int>(index.internalId() - 1);
    return lane >= 0 && lane < geo_.size() && geo_.at(lane).collapsed;
}

void NotesBoardView::setSelection(const QRect& rect, QItemSelectionModel::SelectionFlags command) {
    if (!model_ || !selectionModel())
        return;
    const QRect r = rect.normalized();
    QItemSelection sel;
    for (int lane = 0; lane < geo_.size(); ++lane) {
        if (geo_.at(lane).collapsed || !lane_rect(lane).intersects(r))
            continue;
        const int n = static_cast<int>(model_->lane(lane).cards.size());
        if (n == 0)
            continue;
        const QRect b = body_rect(lane);
        const int y0 = std::max(r.top(), b.top()) - b.top() - m_.lane_pad + scroll_.value(lane);
        const int y1 = std::min(r.bottom(), b.bottom()) - b.top() - m_.lane_pad + scroll_.value(lane);
        if (y1 < 0)
            continue;
        int first = std::max(0, y0 / step());
        int last = std::min(n - 1, y1 / step());
        // Rows whose card actually intersects the rect.
        while (first <= last && !card_rect(lane, first).intersects(r))
            ++first;
        while (last >= first && !card_rect(lane, last).intersects(r))
            --last;
        if (first > last)
            continue;
        const QModelIndex parent = model_->lane_index(lane);
        sel.select(model_->index(first, 0, parent), model_->index(last, 0, parent));
    }
    selectionModel()->select(sel, command);
}

QRegion NotesBoardView::visualRegionForSelection(const QItemSelection& selection) const {
    QRegion region;
    const QRect vp = viewport()->rect();
    for (const QItemSelectionRange& range : selection) {
        if (!range.isValid())
            continue;
        const QModelIndex parent = range.parent();
        if (!parent.isValid())
            continue;
        const int lane = parent.row();
        if (lane < 0 || lane >= geo_.size() || geo_.at(lane).collapsed || !lane_rect(lane).intersects(vp))
            continue;
        const int top = std::max(range.top(), (scroll_.value(lane) - m_.lane_pad) / std::max(1, step()));
        const int bottom = std::min(range.bottom(), top + vp.height() / std::max(1, step()) + 2);
        for (int row = top; row <= bottom; ++row)
            region += card_rect(lane, row);
    }
    return region;
}

void NotesBoardView::scrollContentsBy(int, int) {
    viewport()->update();
    schedule_tickers();
}

// ── Selection / current ──────────────────────────────────────────────────────

int NotesBoardView::current_lane() const {
    const QModelIndex c = currentIndex();
    if (!c.isValid())
        return -1;
    return c.internalId() == 0 ? c.row() : static_cast<int>(c.internalId() - 1);
}

QString NotesBoardView::current_lane_key() const {
    const int lane = current_lane();
    if (model_ && lane >= 0 && lane < model_->lane_count())
        return model_->lane(lane).key;
    // No focus yet: the first expanded lane.
    if (model_)
        for (int i = 0; i < model_->lane_count(); ++i)
            if (!model_->lane(i).collapsed)
                return model_->lane(i).key;
    return {};
}

void NotesBoardView::focus_lane(int lane) {
    if (!model_ || lane < 0 || lane >= model_->lane_count())
        return;
    const QModelIndex parent = model_->lane_index(lane);
    const QModelIndex target = model_->rowCount(parent) > 0 ? model_->index(0, 0, parent) : parent;
    setCurrentIndex(target);
    scrollTo(target);
}

int NotesBoardView::current_id() const {
    const NoteEntry* e = model_ ? model_->card(currentIndex()) : nullptr;
    return e ? e->note.id : 0;
}

QVector<int> NotesBoardView::selected_ids() const {
    QVector<int> out;
    if (!model_ || !selectionModel())
        return out;
    QModelIndexList list = selectionModel()->selectedIndexes();
    std::sort(list.begin(), list.end(), [](const QModelIndex& a, const QModelIndex& b) {
        if (a.internalId() != b.internalId())
            return a.internalId() < b.internalId();
        return a.row() < b.row();
    });
    for (const QModelIndex& i : list)
        if (const NoteEntry* e = model_->card(i))
            out << e->note.id;
    return out;
}

void NotesBoardView::select_ids(const QVector<int>& ids, int current, bool scroll) {
    if (!model_ || !selectionModel())
        return;
    QItemSelection sel;
    for (int id : ids) {
        const QModelIndex i = model_->card_index(id);
        if (i.isValid())
            sel.select(i, i);
    }
    selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect);
    const QModelIndex c = model_->card_index(current);
    if (c.isValid()) {
        selectionModel()->setCurrentIndex(c, QItemSelectionModel::NoUpdate);
        if (scroll)
            scrollTo(c);
    }
    viewport()->update();
}

void NotesBoardView::currentChanged(const QModelIndex& current, const QModelIndex& previous) {
    QAbstractItemView::currentChanged(current, previous);
    viewport()->update();
    const NoteEntry* e = model_ ? model_->card(current) : nullptr;
    if (e && model_) {
        const int lane = static_cast<int>(current.internalId() - 1);
        const auto& l = model_->lane(lane);
        // One .arg() call: the lane title is user text and a chained call would re-scan it.
        setAccessibleDescription(tr("%1 lane, card %2 of %3: %4")
                                     .arg(l.title, QString::number(current.row() + 1),
                                          QString::number(l.cards.size()), e->note.title));
    } else if (current.isValid() && model_ && current.internalId() == 0) {
        setAccessibleDescription(tr("%1 lane, empty").arg(model_->lane(current.row()).title));
    }
    emit current_card_changed(e ? e->note.id : 0);
}

void NotesBoardView::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) {
    QAbstractItemView::selectionChanged(selected, deselected);
    viewport()->update();
    emit selection_ids_changed();
}

QModelIndex NotesBoardView::moveCursor(CursorAction action, Qt::KeyboardModifiers) {
    if (!model_ || model_->lane_count() == 0)
        return {};
    QModelIndex cur = currentIndex();
    int lane = -1;
    int row = -1;
    if (cur.isValid()) {
        lane = cur.internalId() == 0 ? cur.row() : static_cast<int>(cur.internalId() - 1);
        row = cur.internalId() == 0 ? -1 : cur.row();
    }
    const auto card_or_lane = [this](int l, int r) -> QModelIndex {
        const QModelIndex parent = model_->lane_index(l);
        const int n = model_->rowCount(parent);
        if (n == 0)
            return parent;
        return model_->index(std::clamp(r, 0, n - 1), 0, parent);
    };
    if (lane < 0) {
        for (int l = 0; l < model_->lane_count(); ++l)
            if (!model_->lane(l).collapsed)
                return card_or_lane(l, 0);
        return {};
    }
    const int n = static_cast<int>(model_->lane(lane).cards.size());
    const int page = std::max(1, (viewport()->height() - m_.header_h) / std::max(1, step()));
    const auto next_lane = [this](int from, int dir) {
        for (int l = from + dir; l >= 0 && l < model_->lane_count(); l += dir)
            if (!model_->lane(l).collapsed)
                return l;
        return -1;
    };
    switch (action) {
    case MoveUp:
        return row > 0 ? card_or_lane(lane, row - 1) : cur;
    case MoveDown:
        return n > 0 ? card_or_lane(lane, row + 1) : cur;
    case MovePageUp:
        return n > 0 ? card_or_lane(lane, row - page) : cur;
    case MovePageDown:
        return n > 0 ? card_or_lane(lane, row + page) : cur;
    case MoveHome:
        return card_or_lane(lane, 0);
    case MoveEnd:
        return card_or_lane(lane, n - 1);
    case MoveLeft: {
        const int l = next_lane(lane, -1);
        return l < 0 ? cur : card_or_lane(l, std::max(0, row));
    }
    case MoveRight: {
        const int l = next_lane(lane, +1);
        return l < 0 ? cur : card_or_lane(l, std::max(0, row));
    }
    case MoveNext:
    case MovePrevious:
        return {};
    }
    return cur;
}

// ── Visible tickers ──────────────────────────────────────────────────────────

void NotesBoardView::schedule_tickers() {
    if (!tickers_timer_->isActive())
        tickers_timer_->start();
}

QStringList NotesBoardView::visible_tickers(int cap) const {
    QStringList out;
    if (!model_)
        return out;
    const QRect vp = viewport()->rect();
    for (int lane = 0; lane < geo_.size() && out.size() < cap; ++lane) {
        if (geo_.at(lane).collapsed || !lane_rect(lane).intersects(vp))
            continue;
        const auto& cards = model_->lane(lane).cards;
        const int first = std::max(0, (scroll_.value(lane) - m_.lane_pad) / std::max(1, step()));
        const int last = std::min(static_cast<int>(cards.size()) - 1,
                                  first + (vp.height() - m_.header_h) / std::max(1, step()) + 1);
        for (int r = first; r <= last && out.size() < cap; ++r) {
            const auto& t = cards.at(r).ticker_list;
            for (int k = 0; k < t.size() && k < 3 && out.size() < cap; ++k)
                if (!out.contains(t.at(k)))
                    out << t.at(k);
        }
    }
    return out;
}

void NotesBoardView::emit_tickers() {
    if (!isVisible())
        return;
    const QStringList t = visible_tickers();
    if (t == last_tickers_)
        return;
    last_tickers_ = t;
    emit visible_tickers_changed(t);
}

QRect NotesBoardView::card_rect_for(int id) const {
    if (!model_)
        return {};
    const auto pos = model_->position_of(id);
    if (pos.first < 0 || geo_.value(pos.first).collapsed)
        return {};
    const QRect r = card_rect(pos.first, pos.second);
    return r.intersects(viewport()->rect()) ? r : QRect();
}

// ── Painting ─────────────────────────────────────────────────────────────────

void NotesBoardView::paintEvent(QPaintEvent* e) {
    QPainter p(viewport());
    const QRect clip = e->rect();
    p.fillRect(clip, nbv_token(colors::BG_BASE));
    if (!model_)
        return;
    for (int lane = 0; lane < geo_.size(); ++lane) {
        const QRect lr = lane_rect(lane);
        if (!lr.intersects(clip))
            continue;
        paint_lane(p, lane, clip);
    }
    // Lane reorder indicator.
    if (lane_dragging_ && lane_drop_index_ >= 0) {
        int x = 0;
        if (lane_drop_index_ < geo_.size())
            x = geo_.at(lane_drop_index_).x - m_.lane_gap / 2;
        else if (!geo_.isEmpty())
            x = geo_.last().x + geo_.last().w + m_.lane_gap / 2;
        x -= horizontalOffset();
        p.fillRect(QRect(x - 1, 0, 3, viewport()->height()), nbv_token(colors::AMBER));
    }
    if (rubber_ && !rubber_rect_.isNull()) {
        QColor fill = nbv_token(colors::AMBER);
        fill.setAlphaF(0.10f);
        p.fillRect(rubber_rect_, fill);
        p.setPen(nbv_token(colors::AMBER_DIM));
        p.drawRect(rubber_rect_.adjusted(0, 0, -1, -1));
    }
}

void NotesBoardView::paint_header(QPainter& p, int lane) {
    const auto& L = model_->lane(lane);
    const QRect h = header_rect(lane);
    const bool collapsed = geo_.at(lane).collapsed;
    const QFontMetrics fh(m_.header), fc(m_.header_count);
    const QColor accent = L.accent.isValid() ? L.accent : nbv_token(colors::AMBER);
    const int n = static_cast<int>(L.cards.size());
    const bool over = L.soft_limit > 0 && n > L.soft_limit;
    const QString count = L.soft_limit > 0 ? QStringLiteral("%1/%2").arg(n).arg(L.soft_limit) : QString::number(n);

    if (collapsed) {
        p.fillRect(h, nbv_token(colors::BG_SURFACE));
        p.setPen(nbv_token(colors::BORDER_DIM));
        p.drawRect(h.adjusted(0, 0, -1, -1));
        p.fillRect(QRect(h.left(), h.top(), h.width(), 3), accent);
        // Count on top, the name written vertically below it.
        p.setFont(m_.header_count);
        p.setPen(over ? nbv_token(colors::WARNING) : nbv_token(colors::TEXT_TERTIARY));
        p.drawText(QRect(h.left(), h.top() + 6, h.width(), fc.height()), Qt::AlignHCenter | Qt::AlignVCenter, count);
        p.save();
        p.translate(h.center().x() + fh.ascent() / 2 - 1, h.top() + 12 + fc.height());
        p.rotate(90);
        p.setFont(m_.header);
        p.setPen(hover_.lane == lane ? nbv_token(colors::AMBER) : nbv_token(colors::TEXT_PRIMARY));
        p.drawText(QPoint(0, 0), fh.elidedText(L.title.toUpper(), Qt::ElideRight, h.height() - 30 - fc.height()));
        p.restore();
        return;
    }

    p.fillRect(h, nbv_token(colors::BG_SURFACE));
    p.setPen(nbv_token(colors::BORDER_DIM));
    p.drawLine(h.left(), h.bottom(), h.right(), h.bottom());
    // Chevron.
    const QRect cr = chevron_rect(lane);
    {
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);
        const double s = std::max(3.0, fh.height() / 5.0);
        const QPointF c(cr.center().x() + 1, cr.center().y());
        QPainterPath tri;
        tri.moveTo(c.x() - s, c.y() - s / 2);
        tri.lineTo(c.x() + s, c.y() - s / 2);
        tri.lineTo(c.x(), c.y() + s / 2 + 1);
        tri.closeSubpath();
        p.fillPath(tri, hover_.lane == lane && hover_.kind == Hit::Chevron ? nbv_token(colors::AMBER)
                                                                           : nbv_token(colors::TEXT_TERTIARY));
        p.restore();
    }
    int x = cr.right() + 2;
    p.fillRect(QRect(x, h.top() + (h.height() - fh.capHeight() - 4) / 2, 3, fh.capHeight() + 4), accent);
    x += 9;
    // Right side, right to left: "+", due, ▲/▼, count.
    const QRect pr = plus_rect(lane);
    p.setFont(m_.header);
    p.setPen(hover_.lane == lane && hover_.kind == Hit::Plus ? nbv_token(colors::AMBER) : nbv_token(colors::TEXT_TERTIARY));
    p.drawText(pr, Qt::AlignCenter, QStringLiteral("+"));
    int rx = pr.left() - 2;
    p.setFont(m_.header_count);
    const auto draw_right = [&](const QString& text, const QColor& c) {
        if (text.isEmpty())
            return;
        const int w = fc.horizontalAdvance(text);
        if (rx - w < x + 40)
            return;
        rx -= w;
        p.setPen(c);
        p.drawText(QRect(rx, h.top(), w, h.height()), Qt::AlignVCenter | Qt::AlignLeft, text);
        rx -= std::max(6, fc.averageCharWidth());
    };
    if (L.due > 0)
        draw_right(QStringLiteral("◷%1").arg(L.due), nbv_token(colors::WARNING));
    // Up / down today among the lane's tickers that have a quote.
    if (!L.tickers.isEmpty() && quotes_) {
        const auto ud = quotes_->up_down(L.tickers);
        if (ud.second > 0)
            draw_right(QStringLiteral("▼%1").arg(ud.second), nbv_token(colors::NEGATIVE));
        if (ud.first > 0)
            draw_right(QStringLiteral("▲%1").arg(ud.first), nbv_token(colors::POSITIVE));
    }
    draw_right(count, over ? nbv_token(colors::WARNING) : nbv_token(colors::TEXT_TERTIARY));
    // Title.
    p.setFont(m_.header);
    p.setPen(nbv_token(colors::TEXT_PRIMARY));
    p.drawText(QRect(x, h.top(), std::max(0, rx - x), h.height()), Qt::AlignVCenter | Qt::AlignLeft,
               fh.elidedText(L.title.toUpper(), Qt::ElideRight, std::max(0, rx - x)));
}

void NotesBoardView::paint_lane(QPainter& p, int lane, const QRect& clip) {
    const auto& L = model_->lane(lane);
    const QRect lr = lane_rect(lane);
    // Lane body: a faint wash so the board reads as columns; a refused drop is tinted.
    QColor wash = colors::overlay_color(0.025);
    if (drop_lane_ == lane && drop_card_ == 0)
        wash = drop_reason_.isEmpty() ? nbv_token(colors::ACCENT_BG)
                                      : fincept::ui::colors::tint_color(nbv_token(colors::DANGER), 0.12);
    p.fillRect(lr, wash);
    p.setPen(nbv_token(colors::BORDER_DIM));
    p.drawRect(lr.adjusted(0, 0, -1, -1));
    paint_header(p, lane);
    if (geo_.at(lane).collapsed)
        return;

    const QRect body = body_rect(lane);
    p.save();
    p.setClipRect(body.intersected(clip));
    const int n = static_cast<int>(L.cards.size());
    const QModelIndex parent = model_->lane_index(lane);
    const QModelIndex current = currentIndex();
    QItemSelectionModel* sm = selectionModel();
    const bool focused = hasFocus();
    if (n == 0) {
        const QRect g = card_rect(lane, 0);
        const auto it = ghosts_.constFind(L.key);
        const bool hover = hover_.lane == lane && hover_.kind == Hit::Ghost;
        if (it != ghosts_.constEnd())
            paint_ghost_card(&p, g, m_, it->title, it->hint, hover);
        else
            paint_ghost_card(&p, g, m_, new_card_text_, empty_lane_text_, hover);
        if (current.isValid() && current.internalId() == 0 && current.row() == lane && focused) {
            p.setPen(QPen(nbv_token(colors::AMBER), 1, Qt::DotLine));
            p.drawRect(g.adjusted(2, 2, -3, -3));
        }
    } else {
        const int first = std::max(0, (scroll_.value(lane) - m_.lane_pad) / std::max(1, step()));
        const int last = std::min(n - 1, first + body.height() / std::max(1, step()) + 1);
        for (int r = first; r <= last; ++r) {
            const QRect cr = card_rect(lane, r);
            if (!cr.intersects(clip))
                continue;
            const QModelIndex idx = model_->index(r, 0, parent);
            CardState st;
            st.selected = sm && sm->isSelected(idx);
            st.current = idx == current;
            st.view_focused = focused;
            st.hover = hover_.lane == lane && hover_.kind == Hit::Card && hover_.row == r;
            st.drop_target = drop_card_ != 0 && L.cards.at(r).note.id == drop_card_;
            st.show_status = show_status_;
            delegate_->paint_state(&p, cr, idx, st);
        }
    }
    // "+ Add card" row under the cards.
    const QRect add = add_row_rect(lane);
    if (!add.isNull() && add.intersects(clip) && n > 0) {
        const bool hover = hover_.lane == lane && hover_.kind == Hit::AddRow;
        p.setFont(m_.meta);
        p.setPen(hover ? nbv_token(colors::AMBER) : nbv_token(colors::TEXT_TERTIARY));
        p.drawText(add.adjusted(m_.pad_h, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, add_card_text_);
    }
    // Drop indicator.
    if (drop_lane_ == lane && drop_card_ == 0) {
        if (drop_reason_.isEmpty() && drop_row_ >= 0) {
            int y;
            if (n == 0)
                y = card_rect(lane, 0).top() - m_.card_gap / 2;
            else if (drop_row_ >= n)
                y = card_rect(lane, n - 1).bottom() + 1 + m_.card_gap / 2;
            else
                y = card_rect(lane, drop_row_).top() - m_.card_gap / 2 - 1;
            p.fillRect(QRect(body.left() + m_.lane_pad - 2, y - 1, body.width() - 2 * m_.lane_pad + 4, 3),
                       nbv_token(colors::AMBER));
        } else if (!drop_reason_.isEmpty()) {
            const QRect box(body.left() + m_.lane_pad, body.top() + m_.lane_pad, body.width() - 2 * m_.lane_pad,
                            m_.card_h);
            p.fillRect(box, nbv_token(colors::BG_RAISED));
            p.setPen(nbv_token(colors::DANGER));
            p.drawRect(box.adjusted(0, 0, -1, -1));
            p.setFont(m_.preview);
            p.drawText(box.adjusted(m_.pad_h, m_.pad_v, -m_.pad_h, -m_.pad_v),
                       Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, drop_reason_);
        }
    }
    p.restore();
    // Lane scrollbar.
    const QRect sb = scrollbar_rect(lane);
    if (!sb.isNull()) {
        const int content = lane_content_h(lane);
        const int handle_h = std::max(24, sb.height() * body.height() / std::max(1, content));
        const int range = std::max(1, max_scroll(lane));
        const int y = sb.top() + (sb.height() - handle_h) * scroll_.value(lane) / range;
        const bool hot = scroll_drag_lane_ == lane || (hover_.lane == lane && hover_.kind == Hit::Scrollbar);
        p.fillRect(QRect(sb.left(), y, sb.width(), handle_h),
                   hot ? nbv_token(colors::BORDER_BRIGHT) : nbv_token(colors::BORDER_MED));
    }
}

// ── Events ───────────────────────────────────────────────────────────────────

void NotesBoardView::resizeEvent(QResizeEvent* e) {
    QAbstractItemView::resizeEvent(e);
    relayout();
    schedule_tickers();
}

void NotesBoardView::wheelEvent(QWheelEvent* e) {
    const QPoint angle = e->angleDelta();
    const QPoint pixel = e->pixelDelta();
    const bool horizontal = (e->modifiers() & Qt::ShiftModifier) || std::abs(angle.x()) > std::abs(angle.y());
    if (horizontal) {
        const int d = !pixel.isNull() ? (pixel.x() != 0 ? pixel.x() : pixel.y())
                                      : (angle.x() != 0 ? angle.x() : angle.y()) * m_.lane_w_min / 360;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d);
        e->accept();
        return;
    }
    const int lane = lane_at(e->position().toPoint().x());
    if (lane < 0 || geo_.value(lane).collapsed) {
        e->ignore();
        return;
    }
    const int d = !pixel.isNull() ? pixel.y() : angle.y() * step() / 120;
    set_lane_scroll(lane, scroll_.value(lane) - d);
    e->accept();
}

bool NotesBoardView::viewportEvent(QEvent* e) {
    if (e->type() == QEvent::Leave || e->type() == QEvent::HoverLeave) {
        if (hover_.lane >= 0) {
            hover_ = {};
            viewport()->update();
        }
    }
    return QAbstractItemView::viewportEvent(e);
}

void NotesBoardView::focusInEvent(QFocusEvent* e) {
    QAbstractItemView::focusInEvent(e);
    viewport()->update();
}

void NotesBoardView::focusOutEvent(QFocusEvent* e) {
    QAbstractItemView::focusOutEvent(e);
    viewport()->update();
}

void NotesBoardView::mousePressEvent(QMouseEvent* e) {
    const QPoint pos = e->position().toPoint();
    const HitResult h = hit_test(pos);
    press_pos_ = pos;
    press_lane_ = -1;
    if (e->button() == Qt::LeftButton && model_ && h.lane >= 0) {
        const QString key = model_->lane(h.lane).key;
        switch (h.kind) {
        case Hit::Chevron:
            emit lane_toggle_requested(key);
            return;
        case Hit::Plus:
        case Hit::AddRow:
            emit new_card_requested(key);
            return;
        case Hit::Ghost: {
            const auto it = ghosts_.constFind(key);
            if (it != ghosts_.constEnd())
                emit ghost_clicked(key, it->template_id);
            else
                emit new_card_requested(key);
            return;
        }
        case Hit::Scrollbar:
            scroll_drag_lane_ = h.lane;
            scroll_drag_origin_ = scroll_.value(h.lane);
            scroll_drag_start_y_ = pos.y();
            viewport()->update(lane_rect(h.lane));
            return;
        case Hit::Header:
            press_lane_ = h.lane;
            setFocus(Qt::MouseFocusReason);
            return;
        case Hit::Body:
            rubber_ = !(e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier));
            rubber_rect_ = QRect();
            break;
        default:
            break;
        }
    }
    QAbstractItemView::mousePressEvent(e);
}

void NotesBoardView::mouseMoveEvent(QMouseEvent* e) {
    const QPoint pos = e->position().toPoint();
    if (scroll_drag_lane_ >= 0) {
        const QRect sb = scrollbar_rect(scroll_drag_lane_);
        const int range = max_scroll(scroll_drag_lane_);
        if (sb.height() > 0 && range > 0)
            set_lane_scroll(scroll_drag_lane_, scroll_drag_origin_ + (pos.y() - scroll_drag_start_y_) *
                                                                         lane_content_h(scroll_drag_lane_) /
                                                                         std::max(1, sb.height()));
        return;
    }
    if (press_lane_ >= 0 && (e->buttons() & Qt::LeftButton)) {
        if (!lane_dragging_ && (pos - press_pos_).manhattanLength() >= QApplication::startDragDistance())
            lane_dragging_ = true;
        if (lane_dragging_) {
            lane_drop_index_ = lane_insert_index(pos.x());
            viewport()->update();
        }
        return;
    }
    const HitResult h = hit_test(pos);
    if (h.kind != hover_.kind || h.lane != hover_.lane || h.row != hover_.row) {
        const HitResult old = hover_;
        hover_ = h;
        if (old.lane >= 0)
            viewport()->update(lane_rect(old.lane));
        if (h.lane >= 0 && h.lane != old.lane)
            viewport()->update(lane_rect(h.lane));
        const bool pointer = h.kind == Hit::Chevron || h.kind == Hit::Plus || h.kind == Hit::Ghost ||
                             h.kind == Hit::AddRow || h.kind == Hit::Card;
        viewport()->setCursor(pointer ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
    if (rubber_ && (e->buttons() & Qt::LeftButton)) {
        rubber_rect_ = QRect(press_pos_, pos).normalized();
        viewport()->update();
    }
    QAbstractItemView::mouseMoveEvent(e);
}

void NotesBoardView::mouseReleaseEvent(QMouseEvent* e) {
    if (scroll_drag_lane_ >= 0) {
        const int lane = scroll_drag_lane_;
        scroll_drag_lane_ = -1;
        viewport()->update(lane_rect(lane));
        return;
    }
    if (press_lane_ >= 0) {
        const int lane = press_lane_;
        press_lane_ = -1;
        if (lane_dragging_) {
            lane_dragging_ = false;
            const int to = lane_drop_index_;
            lane_drop_index_ = -1;
            viewport()->update();
            if (model_ && to >= 0 && to != lane && to != lane + 1)
                emit lane_moved(model_->lane(lane).key, to > lane ? to - 1 : to);
        } else {
            focus_lane(lane);
        }
        return;
    }
    if (rubber_) {
        rubber_ = false;
        rubber_rect_ = QRect();
        viewport()->update();
    }
    QAbstractItemView::mouseReleaseEvent(e);
}

void NotesBoardView::mouseDoubleClickEvent(QMouseEvent* e) {
    const HitResult h = hit_test(e->position().toPoint());
    if (!model_ || h.lane < 0)
        return;
    if (h.kind == Hit::Card) {
        if (const NoteEntry* c = model_->card(h.lane, h.row))
            emit open_requested(c->note.id, true);
        return;
    }
    if (h.kind == Hit::Body)
        emit new_card_requested(model_->lane(h.lane).key);
    if (h.kind == Hit::Header)
        emit lane_toggle_requested(model_->lane(h.lane).key);
}

int NotesBoardView::lane_insert_index(int viewport_x) const {
    const int x = viewport_x + horizontalOffset();
    for (int i = 0; i < geo_.size(); ++i)
        if (x < geo_.at(i).x + geo_.at(i).w / 2)
            return i;
    return static_cast<int>(geo_.size());
}

void NotesBoardView::keyPressEvent(QKeyEvent* e) {
    const auto mods = e->modifiers();
    const int key = e->key();
    const bool shift = mods & Qt::ShiftModifier;
    const bool ctrl = mods & Qt::ControlModifier;
    if (shift && !ctrl && (key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Up || key == Qt::Key_Down)) {
        emit move_by_key(key == Qt::Key_Left ? -1 : key == Qt::Key_Right ? 1 : 0,
                         key == Qt::Key_Up ? -1 : key == Qt::Key_Down ? 1 : 0);
        e->accept();
        return;
    }
    if (!ctrl && !(mods & Qt::AltModifier)) {
        switch (key) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (const int id = current_id(); id > 0)
                emit open_requested(id, true);
            else if (model_ && current_lane() >= 0)
                emit new_card_requested(current_lane_key());
            e->accept();
            return;
        case Qt::Key_N:
            emit new_card_requested(current_lane_key());
            e->accept();
            return;
        case Qt::Key_Space:
            if (const int id = current_id(); id > 0)
                emit peek_requested(id);
            e->accept();
            return;
        case Qt::Key_Delete:
            emit delete_requested();
            e->accept();
            return;
        case Qt::Key_P:
            emit pin_requested();
            e->accept();
            return;
        case Qt::Key_Slash:
            emit search_requested();
            e->accept();
            return;
        case Qt::Key_Escape:
            if (selectionModel() && selectionModel()->hasSelection())
                selectionModel()->clearSelection();
            emit escape_pressed();
            e->accept();
            return;
        default:
            break;
        }
    }
    if (ctrl && key == Qt::Key_Backspace) {
        emit delete_requested();
        e->accept();
        return;
    }
    if (key == Qt::Key_Tab || key == Qt::Key_Backtab) {
        e->ignore(); // focus leaves the board
        return;
    }
    QAbstractItemView::keyPressEvent(e);
}

void NotesBoardView::contextMenuEvent(QContextMenuEvent* e) {
    if (!model_)
        return;
    const HitResult h = hit_test(e->pos());
    if (h.lane < 0)
        return;
    if (h.kind == Hit::Card) {
        const QModelIndex idx = model_->index(h.row, 0, model_->lane_index(h.lane));
        if (selectionModel() && !selectionModel()->isSelected(idx))
            selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
        emit card_menu_requested(e->globalPos());
        return;
    }
    emit lane_menu_requested(model_->lane(h.lane).key, e->globalPos());
}

// ── Drag and drop ────────────────────────────────────────────────────────────

void NotesBoardView::startDrag(Qt::DropActions) {
    if (!model_ || !selectionModel())
        return;
    QModelIndexList indexes;
    for (const QModelIndex& i : selectionModel()->selectedIndexes())
        if (model_->card(i))
            indexes << i;
    if (indexes.isEmpty())
        return;
    QMimeData* mime = model_->mimeData(indexes);
    if (!mime)
        return;
    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    // A picture of the first card, with the count when several are dragged.
    const QModelIndex first = currentIndex().isValid() && selectionModel()->isSelected(currentIndex())
                                  ? currentIndex()
                                  : indexes.first();
    const QSize sz(std::max(60, m_.card_w), m_.card_h);
    const qreal dpr = devicePixelRatioF();
    QPixmap pm(sz * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        p.setOpacity(0.92);
        CardState st;
        st.selected = true;
        delegate_->paint_state(&p, QRect(QPoint(0, 0), sz), first, st);
        if (indexes.size() > 1) {
            const QString n = QString::number(indexes.size());
            const QFontMetrics fm(m_.header);
            const int w = std::max(fm.height(), fm.horizontalAdvance(n) + 10);
            const QRect badge(sz.width() - w - 4, 4, w, fm.height() + 2);
            p.setOpacity(1.0);
            p.fillRect(badge, nbv_token(colors::AMBER));
            p.setFont(m_.header);
            p.setPen(nbv_token(colors::TEXT_ON_ACCENT));
            p.drawText(badge, Qt::AlignCenter, n);
        }
    }
    drag->setPixmap(pm);
    const QRect fr = visualRect(first);
    drag->setHotSpot(fr.isValid() && fr.contains(press_pos_) ? press_pos_ - fr.topLeft()
                                                             : QPoint(sz.width() / 2, m_.pad_v));
    drag->exec(Qt::MoveAction | Qt::CopyAction, Qt::MoveAction);
    clear_drop();
}

QVector<int> NotesBoardView::mime_ids(const QMimeData* mime, QString* from_lane) const {
    QVector<int> ids;
    if (!mime || !mime->hasFormat(QString::fromLatin1(kNoteCardsMime)))
        return ids;
    const QJsonObject o = QJsonDocument::fromJson(mime->data(QString::fromLatin1(kNoteCardsMime))).object();
    for (const QJsonValue& v : o.value(QStringLiteral("ids")).toArray())
        if (v.toInt() > 0)
            ids << v.toInt();
    if (from_lane)
        *from_lane = o.value(QStringLiteral("from")).toString();
    return ids;
}

int NotesBoardView::insertion_row(int lane, int viewport_y) const {
    if (!model_ || lane < 0)
        return -1;
    const int n = static_cast<int>(model_->lane(lane).cards.size());
    const QRect b = body_rect(lane);
    if (b.isNull())
        return n;
    const int local = viewport_y - b.top() - m_.lane_pad + scroll_.value(lane) + step() / 2;
    return std::clamp(local / std::max(1, step()), 0, n);
}

void NotesBoardView::dragEnterEvent(QDragEnterEvent* e) {
    const QMimeData* mime = e->mimeData();
    if (mime->hasFormat(QString::fromLatin1(kNoteCardsMime)) && e->source() == this) {
        e->setDropAction(Qt::MoveAction);
        e->accept();
        return;
    }
    if (fincept::symbol_mime::has_symbol(mime) && e->source() != this) {
        e->setDropAction(Qt::CopyAction);
        e->accept();
        return;
    }
    e->ignore();
}

void NotesBoardView::dragMoveEvent(QDragMoveEvent* e) {
    const QPoint pos = e->position().toPoint();
    drag_pos_ = pos;
    const QMimeData* mime = e->mimeData();
    const int lane = lane_at(pos.x());
    const int old_lane = drop_lane_;
    if (mime->hasFormat(QString::fromLatin1(kNoteCardsMime)) && e->source() == this) {
        drop_card_ = 0;
        if (lane < 0) {
            clear_drop();
            e->ignore();
            return;
        }
        QString from;
        const QVector<int> ids = mime_ids(mime, &from);
        const QString to = model_->lane(lane).key;
        drop_lane_ = lane;
        drop_row_ = geo_.at(lane).collapsed ? static_cast<int>(model_->lane(lane).cards.size())
                                            : insertion_row(lane, pos.y());
        drop_reason_ = validator_ ? validator_(ids, from, to) : QString();
        if (drop_reason_.isEmpty()) {
            e->setDropAction(Qt::MoveAction);
            e->accept();
        } else {
            e->ignore();
        }
        if (!autoscroll_->isActive())
            autoscroll_->start();
    } else if (fincept::symbol_mime::has_symbol(mime)) {
        const HitResult h = hit_test(pos);
        const int old_card = drop_card_;
        drop_lane_ = -1;
        drop_card_ = 0;
        if (h.kind == Hit::Card)
            if (const NoteEntry* c = model_->card(h.lane, h.row))
                drop_card_ = c->note.id;
        if (drop_card_ != old_card)
            viewport()->update();
        if (drop_card_ > 0) {
            e->setDropAction(Qt::CopyAction);
            e->accept();
        } else {
            e->ignore();
        }
        return;
    } else {
        e->ignore();
        return;
    }
    if (old_lane >= 0 && old_lane != drop_lane_)
        viewport()->update(lane_rect(old_lane));
    if (drop_lane_ >= 0)
        viewport()->update(lane_rect(drop_lane_));
}

void NotesBoardView::dragLeaveEvent(QDragLeaveEvent* e) {
    clear_drop();
    e->accept();
}

void NotesBoardView::dropEvent(QDropEvent* e) {
    const QMimeData* mime = e->mimeData();
    const QPoint pos = e->position().toPoint();
    if (mime->hasFormat(QString::fromLatin1(kNoteCardsMime)) && e->source() == this && model_) {
        const int lane = lane_at(pos.x());
        QString from;
        const QVector<int> ids = mime_ids(mime, &from);
        if (lane >= 0 && !ids.isEmpty()) {
            const QString to = model_->lane(lane).key;
            const QString reason = validator_ ? validator_(ids, from, to) : QString();
            const int row = geo_.at(lane).collapsed ? static_cast<int>(model_->lane(lane).cards.size())
                                                    : insertion_row(lane, pos.y());
            clear_drop();
            if (reason.isEmpty()) {
                e->setDropAction(Qt::MoveAction);
                e->accept();
                emit cards_dropped(ids, from, to, row);
                return;
            }
        }
        clear_drop();
        e->ignore();
        return;
    }
    if (fincept::symbol_mime::has_symbol(mime) && model_) {
        const HitResult h = hit_test(pos);
        clear_drop();
        if (h.kind == Hit::Card) {
            const fincept::SymbolRef ref = fincept::symbol_mime::unpack(mime);
            const NoteEntry* c = model_->card(h.lane, h.row);
            if (c && ref.is_valid()) {
                e->setDropAction(Qt::CopyAction);
                e->accept();
                emit symbol_dropped(c->note.id, ref.symbol);
                return;
            }
        }
    }
    clear_drop();
    e->ignore();
}

void NotesBoardView::clear_drop() {
    autoscroll_->stop();
    if (drop_lane_ < 0 && drop_card_ == 0 && drop_reason_.isEmpty())
        return;
    drop_lane_ = -1;
    drop_row_ = -1;
    drop_card_ = 0;
    drop_reason_.clear();
    viewport()->update();
}

void NotesBoardView::autoscroll_tick() {
    if (drop_lane_ < 0) {
        autoscroll_->stop();
        return;
    }
    const int margin = std::max(24, m_.card_h / 3);
    const QRect vp = viewport()->rect();
    // Horizontal: near the left / right edge.
    if (drag_pos_.x() < margin)
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - m_.lane_w_min / 8);
    else if (drag_pos_.x() > vp.width() - margin)
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() + m_.lane_w_min / 8);
    // Vertical: near the top / bottom of the lane body.
    const QRect b = body_rect(drop_lane_);
    if (!b.isNull()) {
        if (drag_pos_.y() < b.top() + margin)
            set_lane_scroll(drop_lane_, scroll_.value(drop_lane_) - step() / 4);
        else if (drag_pos_.y() > b.bottom() - margin)
            set_lane_scroll(drop_lane_, scroll_.value(drop_lane_) + step() / 4);
    }
}

} // namespace fincept::screens::notes
