// src/screens/relationship_map/RelationshipGraphLayout.h
// Where everything of a RelGraphModel goes, for each layout of the map. Pure QtCore and
// deterministic (same model + metrics + seed = same picture): the screen runs it on a
// worker thread; tests/tst_relmap_model.cpp checks overlap-free placement and timing.
//
//   TIERED     the default: owners in a ranked column on the left, the market side (peers,
//              correlated, subsidiaries, supply chain) on the right, the company card in the
//              middle; one row card per entity, S-curve connectors into ports on the card.
//   HIERARCHY  the same groups as columns in two tiers above and below the company card,
//              orthogonal connectors through a bus per group.
//   RADIAL     groups in angular sectors around the company, bubbles sized by value.
//   FORCE      seeded force-directed bubbles, each group pulled towards its own sector.
//   SANKEY     ownership flows: holders (and "other" / retail) into the company's bar; a fund's
//              bar into its holdings.
// (MATRIX is not a scene layout: the screen shows the correlation heatmap instead.)
#pragma once

#include "screens/relationship_map/RelationshipGraphModel.h"

#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

#include <functional>

namespace fincept::relmap {

enum class RelLayoutMode : int { Tiered, Hierarchy, Radial, Force, Sankey, Matrix };
QString layout_key(RelLayoutMode m); ///< "tiered" | "hierarchy" | "radial" | "force" | "sankey" | "matrix"
bool layout_from_key(const QString& key, RelLayoutMode* out);

/// Sizes in scene px, measured by the caller from the user's font (so the map grows with it).
struct RelLayoutMetrics {
    double row_h = 24;     ///< row card height (TIERED / HIERARCHY)
    double row_gap = 3;    ///< between row cards
    double card_w = 320;   ///< row card width
    double center_w = 320; ///< company card
    double center_h = 210;
    double header_h = 30;     ///< group header strip
    double group_gap = 16;    ///< between groups in a column
    double column_gap = 170;  ///< company card <-> columns (room for the connectors)
    double side_caption_h = 26;
    double bubble_min = 9;  ///< bubble radius range (RADIAL / FORCE)
    double bubble_max = 40;
    double label_h = 16;     ///< bubble label height
    double char_w = 7.2;     ///< fallback label width per character
    double placeholder_rows = 3; ///< rows a loading group reserves
    QHash<QString, double> label_w; ///< node id -> bubble label width (measured); missing = char_w * length
    quint32 seed = 0;       ///< 0 = derived from the centre id
};

struct RelNodeGeom {
    QRectF rect;          ///< card / bubble bounds
    QPointF center;
    double radius = 0;    ///< bubbles
    bool bubble = false;  ///< drawn as a shape (RADIAL / FORCE) instead of a row card
    QRectF label;         ///< bubble label rect (empty for cards)
    bool label_visible = true;
    QPointF port;         ///< where its connector attaches on the node
};

struct RelEdgeGeom {
    QPointF p0, c1, c2, p3; ///< cubic Bézier (a straight edge has c1 = p0, c2 = p3)
    QVector<QPointF> poly;  ///< orthogonal route (HIERARCHY); empty = use the curve
    double band = 0;        ///< SANKEY: band thickness in px (0 = a line)
    QPointF label_pos;      ///< where a value label goes
    bool label_slot = false; ///< a label fits there without overlapping (top edges only)
    bool visible = true;
};

struct RelGroupGeom {
    RelGroup group = RelGroup::Count;
    QRectF frame;   ///< header + rows
    QRectF header;  ///< header strip
    bool left = true;
    double mid_angle = 0; ///< RADIAL / FORCE: sector direction (radians)
    QPointF caption;      ///< RADIAL / FORCE: caption anchor
};

struct RelLayoutResult {
    RelLayoutMode mode = RelLayoutMode::Tiered;
    QHash<QString, RelNodeGeom> nodes;
    QVector<RelEdgeGeom> edges; ///< index-aligned with RelGraphModel::edges
    QVector<RelGroupGeom> groups;
    QRectF left_caption, right_caption; ///< TIERED side captions ("WHO OWNS IT" …); empty = none
    QRectF bounds;
    quint32 seed = 0;
    qint64 elapsed_us = 0;
};

/// The layout of `model`. `weights` = edge_weights() (thickness drives SANKEY band sizes and
/// the edge-label choice). `previous` = node centres of the picture on screen: FORCE starts
/// from them so a streamed section does not reshuffle what is already placed.
RelLayoutResult compute_layout(const RelGraphModel& model, RelLayoutMode mode, const RelLayoutMetrics& metrics,
                               const QVector<double>& weights,
                               const QHash<QString, QPointF>& previous = QHash<QString, QPointF>());

// ── Pieces, exposed for tests ───────────────────────────────────────────────

/// Bubble radius from a magnitude on a log scale between the set's min and max.
double bubble_radius(double value, double log_min, double log_max, double r_min, double r_max);

/// Greedy label placement: each label tries right / left / above / below / diagonals of its
/// anchor and takes the first slot overlapping no obstacle and no placed label; labels that
/// fit nowhere are hidden (shown on hover). Higher priority first. Returns rects (empty = hidden).
struct RelLabelRequest {
    QRectF anchor;      ///< the node it labels
    double w = 0;
    double h = 0;
    double priority = 0;
};
QVector<QRectF> place_labels(const QVector<RelLabelRequest>& labels, const QVector<QRectF>& obstacles,
                             double gap = 2.0);

/// Deterministic seed of a map (FNV-1a of the centre id).
quint32 layout_seed(const QString& center_id);

} // namespace fincept::relmap
