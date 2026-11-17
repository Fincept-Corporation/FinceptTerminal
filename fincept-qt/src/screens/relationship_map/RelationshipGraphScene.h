// src/screens/relationship_map/RelationshipGraphScene.h
// The Relationship Map graph: a QGraphicsScene that draws a RelGraphModel in one of the
// layouts of RelationshipGraphLayout.h, and the zoom / pan view around it.
//
//   * TIERED (default): ranked row cards per group in two columns around a company data card,
//     S-curve connectors whose thickness is the chosen edge metric, value pills on the main
//     edges, group headers with counts and totals, "+ n more" rows that expand on click.
//   * HIERARCHY / RADIAL / FORCE / SANKEY: the same nodes as columns-in-tiers, sized bubbles
//     or ownership bands.
// Layouts are computed on a worker thread (deterministic seed); a new result animates from the
// picture on screen (existing nodes glide, new ones fade in). Painted items read the theme
// tokens at paint time; text is formatted with the shared formatter and re-formatted on
// refresh_text(). Level of detail: far out, card text, thin edges and labels drop out.
#pragma once

#include "screens/relationship_map/RelationshipGraphLayout.h"
#include "screens/relationship_map/RelationshipGraphModel.h"
#include "screens/relationship_map/RelationshipMapData.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHash>
#include <QPointer>
#include <QSet>

class QVariantAnimation;

namespace fincept::relmap {

class RelationshipGraphScene : public QGraphicsScene {
    Q_OBJECT
  public:
    explicit RelationshipGraphScene(QObject* parent = nullptr);
    ~RelationshipGraphScene() override;

    /// Show `model` (built from `data`) in `mode` with edge thickness from `metric`. The layout
    /// runs on a worker thread; `animate` glides the nodes already on screen to their new place.
    void set_graph(const RelationshipData& data, const RelGraphModel& model, RelLayoutMode mode, RelEdgeMetric metric,
                   bool animate = true);
    /// Re-format every text (number format / currency / language changed). No relayout.
    void refresh_text();
    /// Font size changed: measure again and lay out again.
    void relayout();
    void clear_graph();

    RelLayoutMode mode() const { return mode_; }
    RelEdgeMetric metric() const { return metric_; }
    const RelGraphModel& model() const { return model_; }
    /// Timing of the last layout: worker time, and the GUI-thread time to build / update the items.
    qint64 last_layout_us() const { return last_layout_us_; }
    qint64 last_build_us() const { return last_build_us_; }
    int item_count() const { return static_cast<int>(items().size()); }

    // ── Selection / navigation (shared with the tables and the detail pane) ──
    void select_node(const QString& id, bool center_on = false); ///< "" clears; no signal
    QString selected_node() const { return selected_; }
    /// Move the selection: dx / dy = -1 / +1 (Up/Down within a group, Left/Right across columns).
    void step_selection(int dx, int dy);
    /// First node whose name / ticker contains `text` (case-insensitive): selected, centred and
    /// pulsed. Returns its id ("" = none).
    QString find_and_pulse(const QString& text);
    QRectF content_rect() const { return content_rect_; }
    /// The node under a scene position ("" = none).
    QString node_at(const QPointF& scene_pos) const;

  signals:
    void node_clicked(const QString& id);
    void node_double_clicked(const QString& id);
    void node_context_menu(const QString& id, const QPoint& screen_pos);
    void cluster_clicked(int group); ///< RelGroup of the "+ n more" row
    void retry_clicked(const QString& section_id);
    void background_clicked();
    void hovered(const QString& id); ///< "" when the pointer leaves the nodes
    void layout_applied(qint64 layout_us, qint64 build_us);

  protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;

  private:
    struct Measure;
    void start_layout(bool animate);
    void apply_layout(const RelLayoutResult& result, bool animate);
    void set_hover(const QString& id);
    void update_emphasis();
    void set_progress(double t);
    Measure measure() const;

    RelationshipData data_;
    RelGraphModel model_;
    RelLayoutMode mode_ = RelLayoutMode::Tiered;
    RelEdgeMetric metric_ = RelEdgeMetric::Percent;
    QVector<double> weights_;
    RelLayoutResult layout_;
    QHash<QString, QGraphicsItem*> node_items_;
    QVector<QGraphicsItem*> edge_items_;
    QVector<QGraphicsItem*> chrome_items_; ///< group headers, captions
    QString selected_;
    QString hover_;
    QSet<QString> neighbourhood_;
    QRectF content_rect_;
    quint64 layout_seq_ = 0;
    QPointer<QVariantAnimation> anim_;
    QVariantAnimation* pulse_ = nullptr;
    QString pulse_id_;
    qint64 last_layout_us_ = 0;
    qint64 last_build_us_ = 0;
    qint64 pending_build_start_ = 0;
};

/// Zooming / panning view of the graph: wheel zoom (limits 0.12x … 4x) anchored under the
/// pointer, drag to pan, Home = fit, arrow keys / Enter / Esc walk the selection.
class RelationshipGraphView : public QGraphicsView {
    Q_OBJECT
  public:
    explicit RelationshipGraphView(RelationshipGraphScene* scene, QWidget* parent = nullptr);

    void fit_to_content();
    void zoom_by(double factor);
    double zoom() const { return current_zoom_; }

  signals:
    void zoom_changed(double factor);
    void activate_requested(const QString& id); ///< Enter on the selection

  protected:
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private:
    RelationshipGraphScene* rscene_ = nullptr;
    double current_zoom_ = 1.0;
    bool user_zoomed_ = false;
    static constexpr double kMinZoom = 0.12;
    static constexpr double kMaxZoom = 4.0;
};

} // namespace fincept::relmap
