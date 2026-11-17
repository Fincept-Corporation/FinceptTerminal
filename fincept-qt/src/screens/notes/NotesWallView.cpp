// src/screens/notes/NotesWallView.cpp
#include "screens/notes/NotesWallView.h"

#include "core/symbol/SymbolMime.h"
#include "screens/notes/NotesBoardModel.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::screens::notes {

namespace colors = fincept::ui::colors;
namespace nb = fincept::notes_board;

namespace {

constexpr double kWallExtent = 60000.0; // scene half-size: room to pan anywhere
constexpr double kMinZoom = 0.12;
constexpr double kMaxZoom = 3.0;

QColor nwv_token(const fincept::ui::ColorToken& t) {
    return QColor(t.get());
}

} // namespace

// ── Card item ────────────────────────────────────────────────────────────────

class WallCardItem : public QGraphicsItem {
  public:
    enum { Type = QGraphicsItem::UserType + 641 };
    WallCardItem(NotesWallView* view, const NoteEntry& e) : view_(view), entry_(e) {
        setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
        setAcceptHoverEvents(true);
        setCacheMode(NoCache);
    }
    int type() const override { return Type; }
    int id() const { return entry_.note.id; }
    const NoteEntry& entry() const { return entry_; }
    void set_entry(const NoteEntry& e) {
        entry_ = e;
        update();
    }
    void set_size(const QSizeF& s) {
        if (s == size_)
            return;
        prepareGeometryChange();
        size_ = s;
    }
    QRectF boundingRect() const override { return QRectF(QPointF(0, 0), size_); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem* option, QWidget*) override {
        const double lod = option->levelOfDetailFromTransform(p->worldTransform());
        CardState st;
        st.selected = isSelected();
        st.hover = hover_;
        st.current = view_ && view_->current_id() == entry_.note.id;
        st.view_focused = view_ && view_->hasFocus();
        st.show_status = view_ && view_->show_status();
        st.lod = lod < 0.32 ? 2 : lod < 0.55 ? 1 : 0;
        const QDateTime now = QDateTime::currentDateTime();
        paint_card(p, boundingRect().toRect(), entry_, view_->metrics(), st, view_->quotes(), view_->highlight(),
                   now.toMSecsSinceEpoch(), now.date());
    }

  protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override {
        if (change == ItemPositionChange && view_ && view_->snap() && !view_->placing()) {
            const double g = view_->grid();
            QPointF p = value.toPointF();
            return QPointF(std::round(p.x() / g) * g, std::round(p.y() / g) * g);
        }
        if (change == ItemPositionHasChanged && view_ && !view_->placing())
            view_->mark_moved(entry_.note.id);
        return QGraphicsItem::itemChange(change, value);
    }
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override {
        hover_ = true;
        update();
    }
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override {
        hover_ = false;
        update();
    }
    void mousePressEvent(QGraphicsSceneMouseEvent* e) override {
        QGraphicsItem::mousePressEvent(e);
        if (view_)
            view_->set_current_from_item(entry_.note.id);
    }

  private:
    NotesWallView* view_ = nullptr;
    NoteEntry entry_;
    QSizeF size_{260, 96};
    bool hover_ = false;
};

// ── Group item ───────────────────────────────────────────────────────────────

class WallGroupItem : public QGraphicsItem {
  public:
    enum { Type = QGraphicsItem::UserType + 642 };
    WallGroupItem(NotesWallView* view, const nb::NoteWallGroup& g) : view_(view), group_(g) {
        setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
        setAcceptHoverEvents(true);
        setZValue(-10);
        setPos(g.x, g.y);
    }
    int type() const override { return Type; }
    nb::NoteWallGroup group() const {
        nb::NoteWallGroup g = group_;
        g.x = pos().x();
        g.y = pos().y();
        return g;
    }
    void set_id(qint64 id) { group_.id = id; }
    void set_title(const QString& t) {
        group_.title = t;
        update();
    }
    QRectF boundingRect() const override { return QRectF(0, 0, group_.w, group_.h); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        const QColor c = colors::chart_color(group_.tone);
        const QRectF r = boundingRect();
        p->fillRect(r, colors::tint_color(c, 0.07));
        QPen pen(isSelected() ? nwv_token(colors::AMBER) : c, 1, isSelected() ? Qt::SolidLine : Qt::DashLine);
        pen.setCosmetic(true);
        p->setPen(pen);
        p->drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
        const CardMetrics& m = view_->metrics();
        const QFontMetrics fh(m.header);
        const QRectF bar(0, 0, r.width(), m.header_h);
        p->fillRect(bar, colors::tint_color(c, 0.16));
        p->fillRect(QRectF(0, 0, 3, m.header_h), c);
        p->setFont(m.header);
        p->setPen(nwv_token(colors::TEXT_PRIMARY));
        const QString title = group_.title.isEmpty() ? QCoreApplication::translate("NotesBoard", "UNTITLED GROUP")
                                                     : group_.title.toUpper();
        p->drawText(bar.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft,
                    fh.elidedText(title, Qt::ElideRight, static_cast<int>(bar.width()) - 20));
        // Resize handle.
        const QRectF h = handle_rect();
        p->setPen(Qt::NoPen);
        p->setBrush(hover_handle_ ? nwv_token(colors::AMBER) : nwv_token(colors::BORDER_MED));
        QPolygonF tri;
        tri << h.topRight() << h.bottomRight() << h.bottomLeft();
        p->drawPolygon(tri);
    }

  protected:
    QRectF handle_rect() const {
        const double s = 14.0;
        return QRectF(group_.w - s, group_.h - s, s, s);
    }
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override {
        if (change == ItemPositionChange && view_ && view_->snap() && !view_->placing()) {
            const double g = view_->grid();
            const QPointF p = value.toPointF();
            return QPointF(std::round(p.x() / g) * g, std::round(p.y() / g) * g);
        }
        if (change == ItemPositionHasChanged && moving_) {
            const QPointF delta = pos() - last_pos_;
            last_pos_ = pos();
            for (const auto& c : contained_)
                if (c)
                    c->moveBy(delta.x(), delta.y()); // marks them moved (user move)
        }
        return QGraphicsItem::itemChange(change, value);
    }
    void hoverMoveEvent(QGraphicsSceneHoverEvent* e) override {
        const bool on = handle_rect().contains(e->pos());
        if (on != hover_handle_) {
            hover_handle_ = on;
            setCursor(on ? Qt::SizeFDiagCursor : Qt::ArrowCursor);
            update();
        }
    }
    void mousePressEvent(QGraphicsSceneMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && handle_rect().contains(e->pos())) {
            resizing_ = true;
            resize_origin_ = e->scenePos();
            resize_size_ = QSizeF(group_.w, group_.h);
            e->accept();
            return;
        }
        // Cards inside the group move with it (those Qt moves itself — still
        // selected after the press — are left to it).
        QVector<QGraphicsItem*> inside;
        if (scene())
            for (QGraphicsItem* it : scene()->items(sceneBoundingRect(), Qt::ContainsItemBoundingRect))
                if (it != this && it->type() == WallCardItem::Type)
                    inside << it;
        moving_ = true;
        last_pos_ = pos();
        press_pos_ = pos();
        QGraphicsItem::mousePressEvent(e);
        contained_.clear();
        for (QGraphicsItem* it : inside)
            if (!it->isSelected())
                contained_ << it;
    }
    void mouseMoveEvent(QGraphicsSceneMouseEvent* e) override {
        if (resizing_) {
            const QPointF d = e->scenePos() - resize_origin_;
            const CardMetrics& m = view_->metrics();
            const double min_w = m.card_w * 0.6;
            const double min_h = m.header_h * 3.0;
            double w = std::max(min_w, resize_size_.width() + d.x());
            double h = std::max(min_h, resize_size_.height() + d.y());
            if (view_->snap()) {
                const double g = view_->grid();
                w = std::round(w / g) * g;
                h = std::round(h / g) * g;
            }
            prepareGeometryChange();
            group_.w = w;
            group_.h = h;
            return;
        }
        QGraphicsItem::mouseMoveEvent(e);
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* e) override {
        const bool was_resizing = resizing_;
        resizing_ = false;
        QGraphicsItem::mouseReleaseEvent(e);
        const bool moved = moving_ && pos() != press_pos_;
        moving_ = false;
        contained_.clear();
        if (view_ && (was_resizing || moved))
            view_->group_edited(this);
    }
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e) override {
        if (e->pos().y() <= view_->metrics().header_h)
            view_->group_rename_requested(this);
        else
            QGraphicsItem::mouseDoubleClickEvent(e);
    }

  private:
    NotesWallView* view_ = nullptr;
    nb::NoteWallGroup group_;
    bool hover_handle_ = false;
    bool resizing_ = false;
    bool moving_ = false;
    QPointF resize_origin_;
    QSizeF resize_size_;
    QPointF last_pos_;
    QPointF press_pos_;
    QVector<QGraphicsItem*> contained_;
};

// ── View ─────────────────────────────────────────────────────────────────────

NotesWallView::NotesWallView(QWidget* parent) : QGraphicsView(parent) {
    setObjectName(QStringLiteral("notesWallView"));
    scene_ = new QGraphicsScene(this);
    scene_->setItemIndexMethod(QGraphicsScene::BspTreeIndex);
    scene_->setSceneRect(-kWallExtent, -kWallExtent, 2 * kWallExtent, 2 * kWallExtent);
    setScene(scene_);
    m_ = CardMetrics::compute();
    setRenderHint(QPainter::Antialiasing, false);
    setRenderHint(QPainter::TextAntialiasing, true);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setDragMode(QGraphicsView::RubberBandDrag);
    setRubberBandSelectionMode(Qt::IntersectsItemShape);
    setFrameShape(QFrame::NoFrame);
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setBackgroundBrush(Qt::NoBrush);

    connect(scene_, &QGraphicsScene::selectionChanged, this, [this]() { emit selection_ids_changed(); });
    view_timer_ = new QTimer(this);
    view_timer_->setSingleShot(true);
    view_timer_->setInterval(400);
    connect(view_timer_, &QTimer::timeout, this, [this]() { emit view_changed(zoom(), center()); });
    tickers_timer_ = new QTimer(this);
    tickers_timer_->setSingleShot(true);
    tickers_timer_->setInterval(200);
    connect(tickers_timer_, &QTimer::timeout, this, [this]() {
        if (!isVisible())
            return;
        const QStringList t = visible_tickers();
        if (t != last_tickers_) {
            last_tickers_ = t;
            emit visible_tickers_changed(t);
        }
    });
}

NotesWallView::~NotesWallView() = default;

double NotesWallView::grid() const {
    return std::max(8, m_.card_gap * 2);
}

void NotesWallView::set_show_status(bool on) {
    show_status_ = on;
    refresh_cards();
}

void NotesWallView::set_highlight(const QStringList& terms) {
    if (terms == highlight_)
        return;
    highlight_ = terms;
    refresh_cards();
}

void NotesWallView::refresh_cards() {
    viewport()->update();
}

void NotesWallView::refit() {
    m_ = CardMetrics::compute();
    const QSizeF sz(m_.card_w, m_.card_h);
    for (WallCardItem* it : std::as_const(cards_))
        it->set_size(sz);
    place_unplaced();
    viewport()->update();
}

void NotesWallView::set_lanes(const QVector<BoardLane>& lanes) {
    QSet<int> keep;
    order_.clear();
    lane_of_.clear();
    const QSizeF sz(m_.card_w, m_.card_h);
    placing_ = true;
    for (int l = 0; l < lanes.size(); ++l) {
        for (const NoteEntry& e : lanes.at(l).cards) {
            const int id = e.note.id;
            keep.insert(id);
            order_ << id;
            lane_of_.insert(id, l);
            auto it = cards_.find(id);
            if (it == cards_.end()) {
                auto* item = new WallCardItem(this, e);
                item->set_size(sz);
                scene_->addItem(item);
                if (saved_.contains(id))
                    item->setPos(saved_.value(id));
                cards_.insert(id, item);
            } else {
                it.value()->set_entry(e);
            }
        }
    }
    for (auto it = cards_.begin(); it != cards_.end();) {
        if (!keep.contains(it.key())) {
            scene_->removeItem(it.value());
            delete it.value();
            it = cards_.erase(it);
        } else {
            ++it;
        }
    }
    placing_ = false;
    place_unplaced();
    schedule_tickers();
}

void NotesWallView::set_saved(qint64 board_id, const QVector<nb::NoteWallItem>& items,
                              const QVector<nb::NoteWallGroup>& groups) {
    board_id_ = board_id;
    saved_.clear();
    for (const auto& w : items)
        saved_.insert(w.note_id, QPointF(w.x, w.y));
    placing_ = true;
    for (auto it = cards_.begin(); it != cards_.end(); ++it)
        if (saved_.contains(it.key()))
            it.value()->setPos(saved_.value(it.key()));
    placing_ = false;
    for (WallGroupItem* g : std::as_const(groups_)) {
        scene_->removeItem(g);
        delete g;
    }
    groups_.clear();
    placing_ = true;
    for (const auto& g : groups) {
        auto* item = new WallGroupItem(this, g);
        scene_->addItem(item);
        groups_.insert(g.id, item);
    }
    placing_ = false;
    moved_.clear();
    place_unplaced();
}

void NotesWallView::place_unplaced() {
    // Columns by lane, below everything that has a saved place.
    double bottom = 0.0;
    bool any_saved = false;
    for (auto it = cards_.cbegin(); it != cards_.cend(); ++it)
        if (saved_.contains(it.key())) {
            bottom = any_saved ? std::max(bottom, it.value()->pos().y() + m_.card_h) : it.value()->pos().y() + m_.card_h;
            any_saved = true;
        }
    for (WallGroupItem* g : std::as_const(groups_)) {
        const double b = g->pos().y() + g->boundingRect().height();
        bottom = any_saved ? std::max(bottom, b) : b;
        any_saved = true;
    }
    const double gap = grid();
    const double top = any_saved ? std::ceil((bottom + gap * 4) / gap) * gap : 0.0;
    const double col_w = m_.card_w + gap * 3;
    const double row_h = m_.card_h + gap;
    QHash<int, int> filled;
    placing_ = true;
    for (int id : std::as_const(order_)) {
        if (saved_.contains(id))
            continue;
        WallCardItem* item = cards_.value(id);
        if (!item)
            continue;
        const int lane = lane_of_.value(id);
        const int row = filled.value(lane);
        filled.insert(lane, row + 1);
        item->setPos(lane * col_w, top + row * row_h);
    }
    placing_ = false;
}

void NotesWallView::place_card(int id, const QPointF& pos) {
    saved_.insert(id, pos);
    if (WallCardItem* it = cards_.value(id)) {
        placing_ = true;
        it->setPos(pos);
        placing_ = false;
    }
}

bool NotesWallView::update_entry(const NoteEntry& e) {
    WallCardItem* it = cards_.value(e.note.id);
    if (!it)
        return false;
    it->set_entry(e);
    return true;
}

// ── Zoom / pan ───────────────────────────────────────────────────────────────

double NotesWallView::zoom() const {
    return transform().m11();
}

void NotesWallView::set_zoom(double z) {
    z = std::clamp(z, kMinZoom, kMaxZoom);
    const QPointF c = center();
    setTransform(QTransform::fromScale(z, z));
    centerOn(c);
    schedule_view_changed();
    schedule_tickers();
}

QPointF NotesWallView::center() const {
    return mapToScene(viewport()->rect().center());
}

void NotesWallView::set_center(const QPointF& c) {
    centerOn(c);
}

void NotesWallView::zoom_by(double factor) {
    const double z = std::clamp(zoom() * factor, kMinZoom, kMaxZoom);
    const double f = z / std::max(1e-6, zoom());
    scale(f, f);
    schedule_view_changed();
    schedule_tickers();
}

void NotesWallView::zoom_to_fit() {
    QRectF r;
    for (WallCardItem* it : std::as_const(cards_))
        r = r.isNull() ? it->sceneBoundingRect() : r.united(it->sceneBoundingRect());
    for (WallGroupItem* g : std::as_const(groups_))
        r = r.isNull() ? g->sceneBoundingRect() : r.united(g->sceneBoundingRect());
    if (r.isNull())
        return;
    r.adjust(-grid() * 2, -grid() * 2, grid() * 2, grid() * 2);
    fitInView(r, Qt::KeepAspectRatio);
    if (zoom() > 1.0)
        set_zoom(1.0);
    else if (zoom() < kMinZoom)
        set_zoom(kMinZoom);
    schedule_view_changed();
    schedule_tickers();
}

void NotesWallView::schedule_view_changed() {
    view_timer_->start();
}

void NotesWallView::schedule_tickers() {
    if (!tickers_timer_->isActive())
        tickers_timer_->start();
}

void NotesWallView::scrollContentsBy(int dx, int dy) {
    QGraphicsView::scrollContentsBy(dx, dy);
    schedule_view_changed();
    schedule_tickers();
}

void NotesWallView::resizeEvent(QResizeEvent* e) {
    QGraphicsView::resizeEvent(e);
    schedule_tickers();
}

void NotesWallView::drawBackground(QPainter* painter, const QRectF& rect) {
    painter->fillRect(rect, nwv_token(colors::BG_BASE));
    if (zoom() < 0.45)
        return; // dots would be noise
    const double g = grid() * 4;
    QColor dot = colors::overlay_color(0.10);
    painter->setPen(QPen(dot, 0));
    const double x0 = std::floor(rect.left() / g) * g;
    const double y0 = std::floor(rect.top() / g) * g;
    QVector<QPointF> pts;
    for (double x = x0; x < rect.right(); x += g)
        for (double y = y0; y < rect.bottom(); y += g)
            pts << QPointF(x, y);
    if (pts.size() < 20000)
        painter->drawPoints(pts.constData(), static_cast<int>(pts.size()));
}

void NotesWallView::wheelEvent(QWheelEvent* e) {
    if (e->modifiers() & Qt::ControlModifier) {
        const int d = e->angleDelta().y();
        if (d != 0)
            zoom_by(std::pow(1.0015, d));
        e->accept();
        return;
    }
    QGraphicsView::wheelEvent(e);
}

// ── Items ────────────────────────────────────────────────────────────────────

WallCardItem* NotesWallView::card_at(const QPoint& viewport_pos) const {
    for (QGraphicsItem* it : items(viewport_pos))
        if (it->zValue() >= 0)
            if (auto* c = qgraphicsitem_cast<WallCardItem*>(it))
                return c;
    return nullptr;
}

void NotesWallView::mark_moved(int id) {
    moved_.insert(id);
}

void NotesWallView::flush_moved() {
    if (moved_.isEmpty())
        return;
    QVector<nb::NoteWallItem> out;
    out.reserve(moved_.size());
    for (int id : std::as_const(moved_)) {
        WallCardItem* it = cards_.value(id);
        if (!it)
            continue;
        nb::NoteWallItem w;
        w.note_id = id;
        w.x = it->pos().x();
        w.y = it->pos().y();
        saved_.insert(id, it->pos());
        out << w;
    }
    moved_.clear();
    if (!out.isEmpty())
        emit positions_changed(out);
}

void NotesWallView::set_current_from_item(int id) {
    if (current_id_ == id)
        return;
    current_id_ = id;
    viewport()->update();
    emit current_card_changed(id);
}

void NotesWallView::group_edited(WallGroupItem* g) {
    if (!g)
        return;
    flush_moved();
    nb::NoteWallGroup out = g->group();
    out.board_id = board_id_;
    emit group_saved(out);
}

void NotesWallView::group_rename_requested(WallGroupItem* g) {
    if (!g)
        return;
    // The screen asks for the title, then calls set_group_title() (which saves).
    const nb::NoteWallGroup grp = g->group();
    emit group_title_requested(grp.id, grp.title);
}

void NotesWallView::set_group_title(qint64 group_id, const QString& title) {
    WallGroupItem* g = groups_.value(group_id);
    if (!g)
        return;
    g->set_title(title.trimmed().left(120));
    group_edited(g);
}

void NotesWallView::group_remove_requested(WallGroupItem* g) {
    if (!g)
        return;
    const qint64 id = g->group().id;
    groups_.remove(id);
    scene_->removeItem(g);
    delete g;
    if (id > 0)
        emit group_removed(id);
}

void NotesWallView::adopt_group_id(qint64 temp_id, qint64 real_id) {
    WallGroupItem* g = groups_.take(temp_id);
    if (!g)
        return;
    g->set_id(real_id);
    groups_.insert(real_id, g);
}

QVector<int> NotesWallView::selected_ids() const {
    QVector<int> out;
    for (QGraphicsItem* it : scene_->selectedItems())
        if (auto* c = qgraphicsitem_cast<WallCardItem*>(it))
            out << c->id();
    std::sort(out.begin(), out.end(), [this](int a, int b) { return order_.indexOf(a) < order_.indexOf(b); });
    return out;
}

void NotesWallView::select_ids(const QVector<int>& ids, int current, bool scroll) {
    const QSignalBlocker block(scene_);
    scene_->clearSelection();
    for (int id : ids)
        if (WallCardItem* it = cards_.value(id))
            it->setSelected(true);
    current_id_ = current;
    if (scroll)
        if (WallCardItem* it = cards_.value(current))
            ensureVisible(it, 40, 40);
    viewport()->update();
}

QStringList NotesWallView::visible_tickers(int cap) const {
    QStringList out;
    if (zoom() < 0.55)
        return out; // chips are not drawn when zoomed out
    const QRectF vis = mapToScene(viewport()->rect()).boundingRect();
    for (QGraphicsItem* it : scene_->items(vis, Qt::IntersectsItemBoundingRect)) {
        if (out.size() >= cap)
            break;
        if (auto* c = qgraphicsitem_cast<WallCardItem*>(it))
            for (int k = 0; k < c->entry().ticker_list.size() && k < 3 && out.size() < cap; ++k)
                if (!out.contains(c->entry().ticker_list.at(k)))
                    out << c->entry().ticker_list.at(k);
    }
    return out;
}

QRect NotesWallView::card_rect_for(int id) const {
    WallCardItem* it = cards_.value(id);
    if (!it)
        return {};
    const QRect r = mapFromScene(it->sceneBoundingRect()).boundingRect();
    return r.intersects(viewport()->rect()) ? r : QRect();
}

QPointF NotesWallView::new_card_point() const {
    const QPointF c = center();
    const double g = grid();
    return QPointF(std::round((c.x() - m_.card_w / 2.0) / g) * g, std::round((c.y() - m_.card_h / 2.0) / g) * g);
}

void NotesWallView::set_draw_group_mode(bool on) {
    draw_group_ = on;
    setDragMode(on ? QGraphicsView::NoDrag : QGraphicsView::RubberBandDrag);
    viewport()->setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
}

// ── Mouse / keys ─────────────────────────────────────────────────────────────

void NotesWallView::mousePressEvent(QMouseEvent* e) {
    const QPoint pos = e->position().toPoint();
    if (e->button() == Qt::MiddleButton || (e->button() == Qt::LeftButton && space_down_)) {
        panning_ = true;
        pan_last_ = pos;
        viewport()->setCursor(Qt::ClosedHandCursor);
        e->accept();
        return;
    }
    if (draw_group_ && e->button() == Qt::LeftButton && !card_at(pos)) {
        drawing_group_ = true;
        group_start_ = mapToScene(pos);
        auto* preview = scene_->addRect(QRectF(group_start_, QSizeF(1, 1)),
                                        QPen(nwv_token(colors::AMBER), 0, Qt::DashLine),
                                        QBrush(colors::tint_color(nwv_token(colors::AMBER), 0.06)));
        preview->setZValue(-5);
        group_preview_ = preview;
        e->accept();
        return;
    }
    QGraphicsView::mousePressEvent(e);
    if (!card_at(pos) && e->button() == Qt::LeftButton && !(e->modifiers() & Qt::ControlModifier))
        set_current_from_item(0);
}

void NotesWallView::mouseMoveEvent(QMouseEvent* e) {
    const QPoint pos = e->position().toPoint();
    if (panning_) {
        const QPoint d = pos - pan_last_;
        pan_last_ = pos;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - d.y());
        e->accept();
        return;
    }
    if (drawing_group_ && group_preview_) {
        const QRectF r = QRectF(group_start_, mapToScene(pos)).normalized();
        static_cast<QGraphicsRectItem*>(group_preview_)->setRect(r);
        e->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(e);
}

void NotesWallView::mouseReleaseEvent(QMouseEvent* e) {
    if (panning_) {
        panning_ = false;
        viewport()->setCursor(draw_group_ ? Qt::CrossCursor : Qt::ArrowCursor);
        e->accept();
        return;
    }
    if (drawing_group_) {
        drawing_group_ = false;
        QRectF r;
        if (group_preview_) {
            r = static_cast<QGraphicsRectItem*>(group_preview_)->rect();
            scene_->removeItem(group_preview_);
            delete group_preview_;
            group_preview_ = nullptr;
        }
        set_draw_group_mode(false);
        emit draw_group_finished();
        const double g = grid();
        if (r.width() >= m_.card_w * 0.5 && r.height() >= m_.header_h * 2) {
            nb::NoteWallGroup grp;
            grp.id = next_temp_group_--;
            grp.board_id = board_id_;
            grp.x = std::round(r.x() / g) * g;
            grp.y = std::round(r.y() / g) * g;
            grp.w = std::round(r.width() / g) * g;
            grp.h = std::round(r.height() / g) * g;
            grp.tone = static_cast<int>(groups_.size() % 12);
            placing_ = true;
            auto* item = new WallGroupItem(this, grp);
            scene_->addItem(item);
            placing_ = false;
            groups_.insert(grp.id, item);
            group_rename_requested(item); // ask for the title; the screen saves it
        }
        e->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(e);
    flush_moved();
}

void NotesWallView::mouseDoubleClickEvent(QMouseEvent* e) {
    const QPoint pos = e->position().toPoint();
    if (WallCardItem* c = card_at(pos)) {
        emit open_requested(c->id(), true);
        return;
    }
    for (QGraphicsItem* it : items(pos))
        if (it->zValue() < 0) {
            QGraphicsView::mouseDoubleClickEvent(e); // group title rename
            return;
        }
    const QPointF sp = mapToScene(pos);
    const double g = grid();
    emit new_card_requested(QPointF(std::round((sp.x() - m_.card_w / 2.0) / g) * g, std::round(sp.y() / g) * g));
}

void NotesWallView::nudge(int dx, int dy) {
    const double g = grid();
    for (QGraphicsItem* it : scene_->selectedItems())
        it->moveBy(dx * g, dy * g);
    flush_moved();
}

void NotesWallView::keyPressEvent(QKeyEvent* e) {
    const auto mods = e->modifiers();
    const bool ctrl = mods & Qt::ControlModifier;
    const int key = e->key();
    if (key == Qt::Key_Space && !e->isAutoRepeat() && scene_->selectedItems().isEmpty()) {
        space_down_ = true; // Space + drag pans
        viewport()->setCursor(Qt::OpenHandCursor);
        e->accept();
        return;
    }
    if (ctrl) {
        if (key == Qt::Key_Plus || key == Qt::Key_Equal) {
            zoom_by(1.2);
            return;
        }
        if (key == Qt::Key_Minus) {
            zoom_by(1.0 / 1.2);
            return;
        }
        if (key == Qt::Key_0) {
            set_zoom(1.0);
            return;
        }
        if (key == Qt::Key_A) {
            for (WallCardItem* it : std::as_const(cards_))
                it->setSelected(true);
            return;
        }
        if (key == Qt::Key_Backspace) {
            emit delete_requested();
            return;
        }
    }
    const int step = (mods & Qt::ShiftModifier) ? 5 : 1;
    switch (key) {
    case Qt::Key_Left:
        nudge(-step, 0);
        return;
    case Qt::Key_Right:
        nudge(step, 0);
        return;
    case Qt::Key_Up:
        nudge(0, -step);
        return;
    case Qt::Key_Down:
        nudge(0, step);
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (current_id_ > 0)
            emit open_requested(current_id_, true);
        return;
    case Qt::Key_Space:
        if (current_id_ > 0)
            emit peek_requested(current_id_);
        return;
    case Qt::Key_Delete:
        emit delete_requested();
        return;
    case Qt::Key_P:
        if (!ctrl)
            emit pin_requested();
        return;
    case Qt::Key_N:
        if (!ctrl)
            emit new_card_requested(new_card_point());
        return;
    case Qt::Key_F:
        if (!ctrl)
            zoom_to_fit();
        return;
    case Qt::Key_Slash:
        emit search_requested();
        return;
    case Qt::Key_Escape:
        if (draw_group_) {
            set_draw_group_mode(false);
            emit draw_group_finished();
        } else {
            scene_->clearSelection();
        }
        return;
    default:
        break;
    }
    QGraphicsView::keyPressEvent(e);
}

void NotesWallView::keyReleaseEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Space && !e->isAutoRepeat() && space_down_) {
        space_down_ = false;
        viewport()->setCursor(draw_group_ ? Qt::CrossCursor : Qt::ArrowCursor);
        e->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(e);
}

void NotesWallView::contextMenuEvent(QContextMenuEvent* e) {
    if (WallCardItem* c = card_at(e->pos())) {
        if (!c->isSelected()) {
            scene_->clearSelection();
            c->setSelected(true);
        }
        set_current_from_item(c->id());
        emit card_menu_requested(e->globalPos());
        return;
    }
    for (QGraphicsItem* it : items(e->pos()))
        if (it->zValue() < 0)
            if (auto* g = qgraphicsitem_cast<WallGroupItem*>(it)) {
                // Groups: rename / remove through the screen's menu built here.
                QMenu menu(this);
                menu.addAction(QCoreApplication::translate("NotesBoard", "Rename group…"), this,
                               [this, g]() { group_rename_requested(g); });
                menu.addAction(QCoreApplication::translate("NotesBoard", "Remove group (cards stay)"), this,
                               [this, g]() { group_remove_requested(g); });
                menu.exec(e->globalPos());
                return;
            }
}

// ── Drop a symbol on a card ──────────────────────────────────────────────────

void NotesWallView::dragEnterEvent(QDragEnterEvent* e) {
    if (fincept::symbol_mime::has_symbol(e->mimeData()) && !e->mimeData()->hasFormat(QString::fromLatin1(kNoteCardsMime)))
        e->acceptProposedAction();
    else
        e->ignore();
}

void NotesWallView::dragMoveEvent(QDragMoveEvent* e) {
    if (card_at(e->position().toPoint()))
        e->acceptProposedAction();
    else
        e->ignore();
}

void NotesWallView::dropEvent(QDropEvent* e) {
    WallCardItem* c = card_at(e->position().toPoint());
    const fincept::SymbolRef ref = fincept::symbol_mime::unpack(e->mimeData());
    if (!c || !ref.is_valid()) {
        e->ignore();
        return;
    }
    e->acceptProposedAction();
    emit symbol_dropped(c->id(), ref.symbol);
}

} // namespace fincept::screens::notes
