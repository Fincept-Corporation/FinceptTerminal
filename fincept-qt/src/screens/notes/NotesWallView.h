// src/screens/notes/NotesWallView.h
//
// The WALL: the same cards placed freely on a large pannable, zoomable surface,
// with snap-to-grid, user-drawn titled groups and positions persisted per board.
//
// Built on QGraphicsView / QGraphicsScene (not a hand-painted surface) because the
// scene's BSP index answers "what is in this rectangle" in O(log n), so painting,
// hit-testing and rubber-band selection stay cheap with 2,000+ cards; the view
// transform gives exact zoom anchored under the mouse; and multi-item moves,
// rubber band and hover come with it. Items are lightweight QGraphicsItems (not
// widgets) painted through the same paint_card() as the board, with a level of
// detail that drops the preview and chips when zoomed out.
//
// Cards without a saved position are laid out in columns by lane (the board's
// grouping) below the placed ones; nothing is written until a card is moved.
#pragma once

#include "screens/notes/NotesCardPainter.h"
#include "screens/notes/NotesStore.h"
#include "storage/repositories/NoteBoardTypes.h"

#include <QGraphicsView>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QVector>

class QGraphicsScene;
class QTimer;

namespace fincept::screens::notes {

class WallCardItem;
class WallGroupItem;

class NotesWallView : public QGraphicsView {
    Q_OBJECT
  public:
    explicit NotesWallView(QWidget* parent = nullptr);
    ~NotesWallView() override;

    void set_quotes(const QuoteBook* quotes) { quotes_ = quotes; }
    const QuoteBook* quotes() const { return quotes_; }
    const CardMetrics& metrics() const { return m_; }
    const QStringList& highlight() const { return highlight_; }
    void set_highlight(const QStringList& terms);
    bool snap() const { return snap_; }
    void set_snap(bool on) { snap_ = on; }
    double grid() const;
    void set_show_status(bool on);
    bool show_status() const { return show_status_; }

    /// The cards to show, lane by lane (the board's grouping places new cards).
    void set_lanes(const QVector<BoardLane>& lanes);
    /// Saved positions and groups of `board_id` (replaces the previous board's).
    void set_saved(qint64 board_id, const QVector<fincept::notes_board::NoteWallItem>& items,
                   const QVector<fincept::notes_board::NoteWallGroup>& groups);
    qint64 board_id() const { return board_id_; }
    /// Replace one card's entry (autosave of the open note).
    bool update_entry(const NoteEntry& e);
    /// Give a card a saved place (a card just created on the wall); moves it when shown.
    void place_card(int id, const QPointF& pos);
    void refresh_cards();
    /// Re-measure the cards (after ThemeManager::appearance_settled).
    void refit();

    double zoom() const;
    void set_zoom(double zoom);
    QPointF center() const;
    void set_center(const QPointF& c);
    void zoom_by(double factor);
    void zoom_to_fit();
    /// Next drag on empty space draws a titled group.
    void set_draw_group_mode(bool on);
    bool draw_group_mode() const { return draw_group_; }

    QVector<int> selected_ids() const;
    void select_ids(const QVector<int>& ids, int current, bool scroll);
    int current_id() const { return current_id_; }
    QStringList visible_tickers(int cap = 40) const;
    /// Viewport rect of a card (empty when off screen).
    QRect card_rect_for(int id) const;
    /// Scene point at the centre of the viewport (where N puts a new card).
    QPointF new_card_point() const;

    /// A new group (temporary negative id) was stored as `real_id`.
    void adopt_group_id(qint64 temp_id, qint64 real_id);
    /// Title chosen for a group (after group_title_requested); saves it.
    void set_group_title(qint64 group_id, const QString& title);

    // Used by the items.
    void mark_moved(int id);
    void group_edited(WallGroupItem* g);
    void group_rename_requested(WallGroupItem* g);
    void group_remove_requested(WallGroupItem* g);
    void set_current_from_item(int id);
    bool placing() const { return placing_; }

  signals:
    void positions_changed(const QVector<fincept::notes_board::NoteWallItem>& moved);
    /// Store this group (id < 0: new — answer with adopt_group_id()).
    void group_saved(const fincept::notes_board::NoteWallGroup& group);
    void group_removed(qint64 group_id);
    /// A group needs a title (drawn just now, or "Rename group…").
    void group_title_requested(qint64 group_id, const QString& current_title);
    void view_changed(double zoom, const QPointF& center);
    void open_requested(int id, bool full_editor);
    void new_card_requested(const QPointF& scene_pos);
    void peek_requested(int id);
    void delete_requested();
    void pin_requested();
    void search_requested();
    void current_card_changed(int id);
    void selection_ids_changed();
    void card_menu_requested(const QPoint& global_pos);
    void visible_tickers_changed(const QStringList& tickers);
    void symbol_dropped(int note_id, const QString& symbol);
    void draw_group_finished();

  protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void wheelEvent(QWheelEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void keyReleaseEvent(QKeyEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void scrollContentsBy(int dx, int dy) override;
    void resizeEvent(QResizeEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dropEvent(QDropEvent* e) override;

  private:
    WallCardItem* card_at(const QPoint& viewport_pos) const;
    void place_unplaced();
    void schedule_view_changed();
    void schedule_tickers();
    void nudge(int dx, int dy);
    void flush_moved();

    QGraphicsScene* scene_ = nullptr;
    const QuoteBook* quotes_ = nullptr;
    CardMetrics m_;
    QStringList highlight_;
    bool snap_ = true;
    bool show_status_ = true;
    qint64 board_id_ = 0;
    QHash<int, WallCardItem*> cards_;
    QHash<qint64, WallGroupItem*> groups_;
    QHash<int, QPointF> saved_;          ///< note id -> saved position (this board)
    QVector<int> order_;                 ///< card ids in lane order
    QHash<int, int> lane_of_;            ///< note id -> lane index (unplaced layout)
    int current_id_ = 0;
    QSet<int> moved_;            ///< cards moved by the user since the last flush
    bool placing_ = false;       ///< programmatic setPos: not a user move
    qint64 next_temp_group_ = -1;
    bool draw_group_ = false;
    bool drawing_group_ = false;
    QPointF group_start_;
    QGraphicsItem* group_preview_ = nullptr;
    bool panning_ = false;
    bool space_down_ = false;
    QPoint pan_last_;
    QTimer* view_timer_ = nullptr;
    QTimer* tickers_timer_ = nullptr;
    QStringList last_tickers_;
};

} // namespace fincept::screens::notes
