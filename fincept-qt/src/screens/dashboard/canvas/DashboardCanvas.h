#pragma once
#include "screens/dashboard/canvas/GridLayout.h"
#include "screens/dashboard/canvas/PlaceholderOverlay.h"
#include "screens/dashboard/canvas/WidgetTile.h"
#include "ui/theme/ThemeTokens.h"

#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

/// Absolute-positioned canvas — react-grid-layout port.
/// 12 columns, drag from title bar, resize from bottom-right, vertical compaction.
class DashboardCanvas : public QWidget {
    Q_OBJECT
  public:
    /// How load_layout()/apply_template() construct the tiles.
    ///  * Immediate   — every tile exists when the call returns (programmatic
    ///                  callers such as the MCP tools that inspect tiles next).
    ///  * Progressive — the grid geometry is final immediately (skeleton
    ///                  frames are painted for pending cells), and tiles are
    ///                  constructed one per event-loop turn, on-screen ones
    ///                  first. Used for the user-facing restore / template
    ///                  paths so a 15-tile dashboard never blocks the UI
    ///                  thread for the whole construction.
    enum class TileBuild { Immediate, Progressive };

    explicit DashboardCanvas(QWidget* parent = nullptr);
    ~DashboardCanvas() override;

    void load_layout(const GridLayout& layout, TileBuild mode = TileBuild::Immediate);
    /// The layout as the user designed it. While the visible grid is only a
    /// narrower fold of that layout (small panel, no edits since), this returns
    /// the original rather than the squashed fold.
    GridLayout current_layout() const;
    void apply_template(const QString& template_id, TileBuild mode = TileBuild::Immediate);
    void add_widget(const QString& widget_type_id);
    /// Add a tile with a per-instance config and, optionally, an explicit
    /// position/size (x/y < 0 = auto-place, w/h <= 0 = widget default). The
    /// widget is constructed with its config — no rebuild of the other tiles.
    /// Returns the new instance id, or an empty string on failure.
    QString add_widget_with(const QString& widget_type_id, const QJsonObject& config, int x = -1, int y = -1,
                            int w = 0, int h = 0);
    void remove_widget(const QString& instance_id);
    /// Move and/or resize one tile (x/y < 0 or w/h <= 0 keep the current
    /// value), pushing colliding tiles down and re-compacting. Returns false
    /// for an unknown instance id.
    bool set_item_cell(const QString& instance_id, int x, int y, int w, int h);
    /// Store a tile's per-instance config in the layout (persisted with the
    /// next save) and apply it to the live widget — without rebuilding tiles.
    bool set_item_config(const QString& instance_id, const QJsonObject& config);
    void set_row_height(int px);

    /// Synchronously construct every tile a Progressive load still has queued.
    void flush_pending_tiles();
    bool has_pending_tiles() const { return !pending_build_.isEmpty(); }

    int tile_count() const { return tiles_.size(); }

  signals:
    void layout_changed(GridLayout layout);
    void widget_count_changed(int count);

  protected:
    void resizeEvent(QResizeEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void showEvent(QShowEvent* e) override;

  private slots:
    void on_drag_started(WidgetTile* tile, QPoint canvas_pos);
    void on_drag_moved(WidgetTile* tile, QPoint canvas_pos);
    void on_drag_released(WidgetTile* tile, QPoint canvas_pos);
    void on_resize_started(WidgetTile* tile, QPoint canvas_pos);
    void on_resize_moved(WidgetTile* tile, QPoint canvas_pos);
    void on_resize_released(WidgetTile* tile);
    void on_tile_close(WidgetTile* tile);
    void on_scroll_tick();

  private:
    void apply_bg();
    /// Clamp every item into layout_.cols columns and re-compact.
    void fit_items_to_cols();
    void reflow_tiles(bool animate = false);
    void update_canvas_height();
    void update_placeholder(const GridCell& cell);
    void hide_placeholder();
    void connect_tile(WidgetTile* tile);
    WidgetTile* tile_for_id(const QString& instance_id) const;
    GridItem* item_for_id(const QString& instance_id);

    /// Construct + place the tile for one layout item. Returns false (and
    /// leaves no tile) when the widget factory fails.
    bool build_tile(const GridItem& item);
    /// Progressive-build step: construct the next queued tile, then re-arm.
    void build_next_pending_tile(quint64 generation);
    /// Drop an item whose widget could not be constructed and close the gap.
    void drop_unbuildable_item(const QString& instance_id);
    /// Cancel any queued progressive build (new layout / template / teardown).
    void cancel_pending_build();

    GridLayout layout_;
    QVector<WidgetTile*> tiles_;
    PlaceholderOverlay* placeholder_ = nullptr;

    // Progressive build queue (instance ids, build order) and its generation:
    // a newer load_layout() bumps the generation so stale queued steps no-op.
    QStringList pending_build_;
    QSet<QString> pending_ids_;
    quint64 build_generation_ = 0;

    // Drag state
    WidgetTile* dragging_tile_ = nullptr;
    QPoint drag_offset_;
    GridCell ghost_cell_;
    GridLayout pre_drag_layout_;

    // Resize state
    WidgetTile* resizing_tile_ = nullptr;
    GridCell resize_origin_cell_;
    GridLayout pre_resize_layout_;

    // Auto-scroll during drag
    QTimer* scroll_timer_ = nullptr;
    QPoint last_drag_canvas_pos_;

    // Debounce rapid resize events (e.g. ADS splitter drag).
    // Column recalculation and compaction only happen after the timer fires,
    // preventing layout destruction during transient width changes.
    QTimer* resize_timer_ = nullptr;
    int pending_resize_w_ = 0;

    // The column count the user last explicitly set (via load/apply/drag).
    // Responsive shrink is allowed, but we restore this when width permits.
    int canonical_cols_ = 12;
    /// The items exactly as loaded/designed (at canonical_cols_), kept so a
    /// narrow fold can be undone when the panel widens. Valid until the user
    /// edits the arrangement (add/remove/drag/resize).
    QVector<GridItem> canonical_items_;
    bool canonical_valid_ = false;

    fincept::ui::ThemeTokens tokens_{};
};

} // namespace fincept::screens
