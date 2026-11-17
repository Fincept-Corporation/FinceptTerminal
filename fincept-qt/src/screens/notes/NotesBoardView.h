// src/screens/notes/NotesBoardView.h
//
// The research board: lanes side by side, cards stacked in each lane, painted by
// NotesCardDelegate over NotesBoardModel — no widget per card. Lanes are laid out
// arithmetically (cards are all the same height, from font metrics), so only the
// lanes and cards inside the viewport are ever touched: thousands of notes scroll
// and drag without work proportional to the library.
//
//   * Each lane scrolls on its own (wheel over it, its thin scrollbar, or keyboard
//     focus); the board scrolls horizontally (Shift+wheel, the scrollbar).
//   * Lane header strip: collapse chevron, accent tick, name, count (with the soft
//     limit when set: "12/10"), tickers up / down today, due count, "+" new card.
//     Collapsed lanes are a narrow strip with the name written vertically.
//     Dragging a header reorders lanes.
//   * Keyboard: arrows move across cards and lanes, Shift+arrows move the card(s),
//     Enter opens, N new card in the focused lane, Space quick-peek, Del delete,
//     P pin, / search, Ctrl+A select all, Esc clears the selection.
//   * Mouse: click / Ctrl / Shift selection, rubber band on empty space, drag one or
//     several cards between lanes or within a lane (the screen's validator decides
//     what may be dropped where; a refused lane says why). A symbol dragged from
//     another panel onto a card adds that ticker.
//   * Empty lanes show a ghost card ("+ New card", or a template suggestion when
//     the notebook is new).
#pragma once

#include "screens/notes/NotesBoardModel.h"

#include <QAbstractItemView>
#include <QHash>
#include <QPointer>
#include <QVector>

#include <functional>

class QTimer;

namespace fincept::screens::notes {

class NotesBoardView : public QAbstractItemView {
    Q_OBJECT
  public:
    /// Empty = the drop is allowed; else the reason shown in the target lane.
    using DropValidator = std::function<QString(const QVector<int>& ids, const QString& from_lane,
                                                const QString& to_lane)>;
    struct GhostTemplate {
        QString template_id;
        QString title;
        QString hint;
    };

    explicit NotesBoardView(QWidget* parent = nullptr);
    ~NotesBoardView() override;

    void setModel(QAbstractItemModel* model) override;
    NotesBoardModel* board_model() const { return model_; }
    NotesCardDelegate* card_delegate() const { return delegate_; }

    QRect visualRect(const QModelIndex& index) const override;
    void scrollTo(const QModelIndex& index, ScrollHint hint = EnsureVisible) override;
    QModelIndex indexAt(const QPoint& point) const override;
    void keyboardSearch(const QString&) override {}

    /// Re-measure cards and lanes (after ThemeManager::appearance_settled).
    void refit();
    void set_show_status(bool on);
    /// Quotes for the ticker chips and the lane headers' up / down counts.
    void set_quotes(const QuoteBook* quotes);
    void set_drop_validator(DropValidator v) { validator_ = std::move(v); }
    /// Template suggestions painted in empty lanes (a new notebook), by lane key.
    void set_ghost_templates(const QHash<QString, GhostTemplate>& ghosts);
    void set_hint_text(const QString& add_card, const QString& new_card, const QString& empty_lane);

    int current_lane() const;
    QString current_lane_key() const;
    /// Focus a lane (its first card, or the lane itself when empty).
    void focus_lane(int lane);
    int current_id() const;
    QVector<int> selected_ids() const;
    /// Select `ids` (those on the board), make `current` the current card.
    void select_ids(const QVector<int>& ids, int current, bool scroll);
    /// Tickers of the cards on screen (first ones of each card), capped.
    QStringList visible_tickers(int cap = 40) const;
    /// Viewport rect of a card (empty when off screen).
    QRect card_rect_for(int id) const;
    /// Keep the per-lane scroll positions across a rebuild (by lane key).
    QHash<QString, int> lane_scrolls() const;
    void restore_lane_scrolls(const QHash<QString, int>& scrolls);

  signals:
    void open_requested(int id, bool full_editor);
    void new_card_requested(const QString& lane_key);
    void ghost_clicked(const QString& lane_key, const QString& template_id);
    void peek_requested(int id);
    void delete_requested();
    void pin_requested();
    void search_requested();
    void escape_pressed();
    /// Shift+arrows: move the selected cards one lane left / right or one place up / down.
    void move_by_key(int dlane, int drow);
    void cards_dropped(const QVector<int>& ids, const QString& from_lane, const QString& to_lane, int to_row);
    void symbol_dropped(int note_id, const QString& symbol);
    void lane_toggle_requested(const QString& lane_key);
    void lane_moved(const QString& lane_key, int to_index);
    void lane_menu_requested(const QString& lane_key, const QPoint& global_pos);
    void card_menu_requested(const QPoint& global_pos);
    void visible_tickers_changed(const QStringList& tickers);
    void current_card_changed(int id);
    void selection_ids_changed();

  protected:
    QModelIndex moveCursor(CursorAction action, Qt::KeyboardModifiers modifiers) override;
    int horizontalOffset() const override;
    int verticalOffset() const override;
    bool isIndexHidden(const QModelIndex& index) const override;
    void setSelection(const QRect& rect, QItemSelectionModel::SelectionFlags command) override;
    QRegion visualRegionForSelection(const QItemSelection& selection) const override;
    void updateGeometries() override;
    void scrollContentsBy(int dx, int dy) override;
    void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override;

    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void startDrag(Qt::DropActions supported) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dragLeaveEvent(QDragLeaveEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    bool viewportEvent(QEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;

  private:
    enum class Hit { None, Header, Chevron, Plus, Card, Ghost, AddRow, Scrollbar, Body };
    struct HitResult {
        Hit kind = Hit::None;
        int lane = -1;
        int row = -1;
    };
    struct LaneGeo {
        int x = 0; ///< content coordinates
        int w = 0;
        bool collapsed = false;
    };

    void relayout();
    void schedule_tickers();
    void emit_tickers();
    int lane_at(int viewport_x) const;
    HitResult hit_test(const QPoint& pos) const;
    QRect lane_rect(int lane) const;   ///< viewport coordinates, full height
    QRect header_rect(int lane) const;
    QRect body_rect(int lane) const;
    QRect chevron_rect(int lane) const;
    QRect plus_rect(int lane) const;
    QRect card_rect(int lane, int row) const;
    QRect add_row_rect(int lane) const;
    QRect scrollbar_rect(int lane) const;
    int step() const;
    int lane_content_h(int lane) const;
    int max_scroll(int lane) const;
    void set_lane_scroll(int lane, int value);
    int insertion_row(int lane, int viewport_y) const;
    int lane_insert_index(int viewport_x) const;
    void paint_lane(QPainter& p, int lane, const QRect& clip);
    void paint_header(QPainter& p, int lane);
    QVector<int> mime_ids(const QMimeData* mime, QString* from_lane) const;
    void clear_drop();
    void autoscroll_tick();

    NotesBoardModel* model_ = nullptr;
    NotesCardDelegate* delegate_ = nullptr;
    const QuoteBook* quotes_ = nullptr;
    bool show_status_ = false;
    CardMetrics m_;
    QVector<LaneGeo> geo_;
    QStringList geo_keys_; ///< lane keys geo_ / scroll_ were laid out for
    QVector<int> scroll_;
    int content_w_ = 0;
    DropValidator validator_;
    QHash<QString, GhostTemplate> ghosts_;
    QString add_card_text_;
    QString new_card_text_;
    QString empty_lane_text_;

    // Hover / press state
    HitResult hover_;
    QPoint press_pos_;
    int press_lane_ = -1;       ///< header pressed (possible lane drag)
    bool lane_dragging_ = false;
    int lane_drop_index_ = -1;
    int scroll_drag_lane_ = -1;
    int scroll_drag_origin_ = 0;
    int scroll_drag_start_y_ = 0;
    bool rubber_ = false;
    QRect rubber_rect_;

    // Drop state
    int drop_lane_ = -1;
    int drop_row_ = -1;
    int drop_card_ = 0;       ///< symbol drop target note id
    QString drop_reason_;
    QPoint drag_pos_;
    QTimer* autoscroll_ = nullptr;

    QTimer* tickers_timer_ = nullptr;
    QStringList last_tickers_;
};

} // namespace fincept::screens::notes
