// src/screens/relationship_map/RelationshipGraphLayout.cpp
#include "screens/relationship_map/RelationshipGraphLayout.h"

#include <QElapsedTimer>

#include <algorithm>
#include <cmath>
#include <random>

namespace fincept::relmap {

namespace {

constexpr double kPi = 3.14159265358979323846;

bool rgl_finite(double v) {
    return std::isfinite(v);
}

/// Point on a cubic Bézier.
QPointF rgl_bezier(const QPointF& p0, const QPointF& c1, const QPointF& c2, const QPointF& p3, double t) {
    const double u = 1.0 - t;
    return p0 * (u * u * u) + c1 * (3 * u * u * t) + c2 * (3 * u * t * t) + p3 * (t * t * t);
}

RelEdgeGeom rgl_straight(const QPointF& a, const QPointF& b) {
    RelEdgeGeom g;
    g.p0 = a;
    g.c1 = a;
    g.c2 = b;
    g.p3 = b;
    g.label_pos = (a + b) / 2.0;
    return g;
}

/// Horizontal S-curve from a (leaving horizontally) to b (arriving horizontally).
RelEdgeGeom rgl_s_curve(const QPointF& a, const QPointF& b) {
    RelEdgeGeom g;
    const double dx = (b.x() - a.x()) * 0.5;
    g.p0 = a;
    g.c1 = QPointF(a.x() + dx, a.y());
    g.c2 = QPointF(b.x() - dx, b.y());
    g.p3 = b;
    g.label_pos = rgl_bezier(g.p0, g.c1, g.c2, g.p3, 0.42);
    return g;
}

/// Uniform-grid index of rectangles for overlap queries.
class RglGrid {
  public:
    explicit RglGrid(double cell) : cell_(cell > 1 ? cell : 64) {}
    void add(const QRectF& r) {
        const int id = static_cast<int>(rects_.size());
        rects_.append(r);
        for_cells(r, [&](qint64 key) { cells_[key].append(id); });
    }
    bool hits(const QRectF& r) const {
        bool hit = false;
        for_cells(r, [&](qint64 key) {
            if (hit)
                return;
            const auto it = cells_.constFind(key);
            if (it == cells_.constEnd())
                return;
            for (int id : it.value())
                if (rects_[id].intersects(r)) {
                    hit = true;
                    return;
                }
        });
        return hit;
    }

  private:
    template <typename F>
    void for_cells(const QRectF& r, F&& f) const {
        const int x0 = static_cast<int>(std::floor(r.left() / cell_));
        const int x1 = static_cast<int>(std::floor(r.right() / cell_));
        const int y0 = static_cast<int>(std::floor(r.top() / cell_));
        const int y1 = static_cast<int>(std::floor(r.bottom() / cell_));
        for (int x = x0; x <= x1; ++x)
            for (int y = y0; y <= y1; ++y)
                f((static_cast<qint64>(x) << 32) ^ static_cast<qint64>(static_cast<quint32>(y)));
    }
    double cell_;
    QVector<QRectF> rects_;
    QHash<qint64, QVector<int>> cells_;
};

bool rgl_left_group(RelGroup g) {
    return g == RelGroup::Institutions || g == RelGroup::Funds || g == RelGroup::Insiders || g == RelGroup::Sectors;
}
bool rgl_flex_group(RelGroup g) {
    return g == RelGroup::Management;
}
bool rgl_owner_tier(RelGroup g) {
    return rgl_left_group(g) || rgl_flex_group(g);
}

/// Rows a group occupies in a column (cards incl. the cluster; a placeholder its reserve).
double rgl_group_rows(const RelGraphModel& m, const RelGraphGroup& g, const RelLayoutMetrics& met) {
    if (g.node_ids.size() == 1) {
        const RelGraphNode* n = m.node(g.node_ids.first());
        if (n && n->kind == RelNodeKind::Placeholder)
            return n->state == RelSectionState::Loading ? std::max(1.0, met.placeholder_rows) : 1.0;
    }
    return static_cast<double>(g.node_ids.size());
}

double rgl_group_height(const RelGraphModel& m, const RelGraphGroup& g, const RelLayoutMetrics& met) {
    const double rows = rgl_group_rows(m, g, met);
    return met.header_h + rows * (met.row_h + met.row_gap);
}

double rgl_label_w(const RelLayoutMetrics& met, const RelGraphNode& n) {
    const auto it = met.label_w.constFind(n.id);
    if (it != met.label_w.constEnd())
        return it.value();
    return met.char_w * std::min<qsizetype>(18, std::max<qsizetype>(3, n.short_label.size()));
}

/// Edge labels on the heaviest edges of each group that find a free spot.
void rgl_edge_labels(const RelGraphModel& m, const QVector<double>& weights, RelLayoutResult& res,
                     const RelLayoutMetrics& met, int per_group, const QVector<QRectF>& obstacles) {
    RglGrid grid(64);
    for (const auto& o : obstacles)
        grid.add(o);
    QHash<int, QVector<int>> by_group;
    for (int i = 0; i < m.edges.size(); ++i) {
        if (m.edges[i].second_hop || !res.edges[i].visible || !res.edges[i].poly.isEmpty())
            continue;
        const RelGraphNode* a = m.node(m.edges[i].from);
        const RelGraphNode* b = m.node(m.edges[i].to);
        if ((a && a->kind == RelNodeKind::Cluster) || (b && b->kind == RelNodeKind::Cluster))
            continue;
        by_group[static_cast<int>(m.edges[i].group)].append(i);
    }
    const double w = met.char_w * 8.5, h = met.label_h;
    for (const auto& grp : m.groups) {
        QVector<int> ids = by_group.value(static_cast<int>(grp.group));
        std::stable_sort(ids.begin(), ids.end(), [&](int x, int y) { return weights.value(x) > weights.value(y); });
        int placed = 0;
        for (int id : ids) {
            if (placed >= per_group)
                break;
            const QPointF c = res.edges[id].label_pos;
            const QRectF r(c.x() - w / 2, c.y() - h / 2, w, h);
            if (grid.hits(r.adjusted(-2, -2, 2, 2)))
                continue;
            grid.add(r);
            res.edges[id].label_slot = true;
            ++placed;
        }
    }
}

// ── TIERED ──────────────────────────────────────────────────────────────────

void rgl_tiered(const RelGraphModel& m, const RelLayoutMetrics& met, const QVector<double>& weights,
                RelLayoutResult& res) {
    // Three columns: owners (left), the company with the people who run it under it
    // (middle; a fund: its sector weights), the market (right). Balanced heights keep the
    // picture readable at the view's aspect.
    QVector<int> left, right, below;
    for (int i = 0; i < m.groups.size(); ++i) {
        const RelGroup g = m.groups[i].group;
        if (g == RelGroup::Management || g == RelGroup::Sectors)
            below << i;
        else if (rgl_left_group(g))
            left << i;
        else
            right << i;
    }
    auto side_h = [&](const QVector<int>& side) {
        double h = 0;
        for (int i : side)
            h += rgl_group_height(m, m.groups[i], met) + met.group_gap;
        return side.isEmpty() ? 0.0 : h - met.group_gap;
    };
    const double below_gap = std::max(met.group_gap * 2.0, met.row_h * 1.6);
    const double middle_h = met.center_h + (below.isEmpty() ? 0.0 : below_gap + side_h(below));
    const double middle_top = -std::max({middle_h, side_h(left), side_h(right)}) / 2.0;
    const QRectF center(-met.center_w / 2, middle_top, met.center_w, met.center_h);
    RelNodeGeom cg;
    cg.rect = center;
    cg.center = center.center();
    res.nodes.insert(m.center_id, cg);

    const double lx = center.left() - met.column_gap - met.card_w;
    const double rx = center.right() + met.column_gap;
    QVector<QRectF> obstacles{center};

    // The middle column: an org-chart spine from the card's bottom down the left edge,
    // the rows indented to its right.
    {
        const double indent = std::max(16.0, met.row_h * 0.9);
        const double spine_x = center.left() + indent * 0.45;
        const double x = center.left() + indent;
        const double w = met.center_w - indent;
        double y = center.bottom() + below_gap;
        for (int gi : below) {
            const RelGraphGroup& grp = m.groups[gi];
            RelGroupGeom gg;
            gg.group = grp.group;
            gg.left = false;
            const double gh = rgl_group_height(m, grp, met);
            gg.frame = QRectF(x, y, w, gh);
            gg.header = QRectF(x, y, w, met.header_h);
            res.groups << gg;
            obstacles << gg.header;
            double ry = y + met.header_h;
            for (const QString& id : grp.node_ids) {
                const RelGraphNode* n = m.node(id);
                if (!n)
                    continue;
                RelNodeGeom ng;
                const double rows = n->kind == RelNodeKind::Placeholder ? rgl_group_rows(m, grp, met) : 1.0;
                const double h = rows * (met.row_h + met.row_gap) - met.row_gap;
                ng.rect = QRectF(x, ry, w, h);
                ng.center = ng.rect.center();
                ng.port = QPointF(ng.rect.left(), ng.center.y());
                res.nodes.insert(id, ng);
                obstacles << ng.rect;
                for (int i = 0; i < m.edges.size(); ++i) {
                    const RelGraphEdge& e = m.edges[i];
                    if (e.second_hop || (e.from != id && e.to != id))
                        continue;
                    RelEdgeGeom g;
                    g.poly = {ng.port, QPointF(spine_x, ng.center.y()), QPointF(spine_x, center.bottom())};
                    g.p0 = ng.port;
                    g.p3 = QPointF(spine_x, center.bottom());
                    g.c1 = g.p0;
                    g.c2 = g.p3;
                    g.label_pos = QPointF(spine_x, ng.center.y());
                    res.edges[i] = g;
                }
                ry += h + met.row_gap;
            }
            y += gh + met.group_gap;
        }
    }

    auto place_side = [&](const QVector<int>& side, double x, bool is_left) {
        // Columns hang from one top line with the company card: headers align across the map.
        double y = middle_top;
        for (int gi : side) {
            const RelGraphGroup& grp = m.groups[gi];
            RelGroupGeom gg;
            gg.group = grp.group;
            gg.left = is_left;
            const double gh = rgl_group_height(m, grp, met);
            gg.frame = QRectF(x, y, met.card_w, gh);
            gg.header = QRectF(x, y, met.card_w, met.header_h);
            res.groups << gg;
            obstacles << gg.header;
            double ry = y + met.header_h;
            for (const QString& id : grp.node_ids) {
                const RelGraphNode* n = m.node(id);
                if (!n)
                    continue;
                RelNodeGeom ng;
                const double rows = n->kind == RelNodeKind::Placeholder ? rgl_group_rows(m, grp, met) : 1.0;
                const double h = rows * (met.row_h + met.row_gap) - met.row_gap;
                ng.rect = QRectF(x, ry, met.card_w, h);
                ng.center = ng.rect.center();
                ng.port = is_left ? QPointF(ng.rect.right(), ng.center.y()) : QPointF(ng.rect.left(), ng.center.y());
                res.nodes.insert(id, ng);
                obstacles << ng.rect;
                ry += h + met.row_gap;
            }
            y += gh + met.group_gap;
        }
        if (!side.isEmpty()) {
            const QRectF cap(x, middle_top - met.side_caption_h - met.group_gap * 0.5, met.card_w, met.side_caption_h);
            (is_left ? res.left_caption : res.right_caption) = cap;
        }
    };
    place_side(left, lx, true);
    place_side(right, rx, false);

    // Ports on the company card: the column's rows map in order onto the card side, so the
    // S-curves fan in without crossing. (The middle column already has its spine routes.)
    QSet<QString> middle_ids;
    for (int gi : below)
        for (const QString& id : m.groups[gi].node_ids)
            middle_ids.insert(id);
    auto attach = [&](bool is_left) {
        QVector<int> edge_ids;
        double ymin = 1e18, ymax = -1e18;
        for (int i = 0; i < m.edges.size(); ++i) {
            const RelGraphEdge& e = m.edges[i];
            if (e.second_hop || middle_ids.contains(e.from) || middle_ids.contains(e.to))
                continue;
            const QString other = e.from == m.center_id ? e.to : e.from;
            const auto it = res.nodes.constFind(other);
            if (it == res.nodes.constEnd())
                continue;
            const bool node_left = it.value().center.x() < 0;
            if (node_left != is_left)
                continue;
            edge_ids << i;
            ymin = std::min(ymin, it.value().center.y());
            ymax = std::max(ymax, it.value().center.y());
        }
        const double pad = std::min(met.row_h, center.height() * 0.12);
        const double top = center.top() + pad, bottom = center.bottom() - pad;
        for (int i : edge_ids) {
            const RelGraphEdge& e = m.edges[i];
            const QString other = e.from == m.center_id ? e.to : e.from;
            const RelNodeGeom& ng = res.nodes[other];
            const double t = ymax > ymin ? (ng.center.y() - ymin) / (ymax - ymin) : 0.5;
            const QPointF port(is_left ? center.left() : center.right(), top + t * (bottom - top));
            res.edges[i] = rgl_s_curve(ng.port, port);
        }
    };
    attach(true);
    attach(false);

    // Second hops: arcs on the outer side of the column, between two cards of it.
    for (int i = 0; i < m.edges.size(); ++i) {
        const RelGraphEdge& e = m.edges[i];
        if (!e.second_hop)
            continue;
        const auto a = res.nodes.constFind(e.from);
        const auto b = res.nodes.constFind(e.to);
        if (a == res.nodes.constEnd() || b == res.nodes.constEnd()) {
            res.edges[i].visible = false;
            continue;
        }
        const bool a_left = a.value().center.x() < 0;
        const double out = a_left ? -1.0 : 1.0;
        const QPointF pa(a_left ? a.value().rect.left() : a.value().rect.right(), a.value().center.y());
        const QPointF pb(a_left ? b.value().rect.left() : b.value().rect.right(), b.value().center.y());
        const double bulge = 14 + 0.22 * std::abs(pb.y() - pa.y());
        RelEdgeGeom g;
        g.p0 = pa;
        g.c1 = pa + QPointF(out * bulge, 0);
        g.c2 = pb + QPointF(out * bulge, 0);
        g.p3 = pb;
        g.label_pos = rgl_bezier(g.p0, g.c1, g.c2, g.p3, 0.5);
        res.edges[i] = g;
    }
    rgl_edge_labels(m, weights, res, met, 2, obstacles);
}

// ── HIERARCHY ───────────────────────────────────────────────────────────────

void rgl_hierarchy(const RelGraphModel& m, const RelLayoutMetrics& met, RelLayoutResult& res) {
    const QRectF center(-met.center_w / 2, -met.center_h / 2, met.center_w, met.center_h);
    RelNodeGeom cg;
    cg.rect = center;
    cg.center = center.center();
    res.nodes.insert(m.center_id, cg);

    QVector<int> top, bottom;
    for (int i = 0; i < m.groups.size(); ++i)
        (rgl_owner_tier(m.groups[i].group) ? top : bottom) << i;

    const double col_gap = std::max(24.0, met.card_w * 0.14);
    const double bus_gap = std::max(36.0, met.row_h * 1.8);
    const double stub = std::max(10.0, met.row_h * 0.5);

    auto tier = [&](const QVector<int>& groups, bool above) {
        if (groups.isEmpty())
            return;
        const double width = groups.size() * met.card_w + (groups.size() - 1) * col_gap;
        double x = -width / 2.0;
        double tallest = 0;
        for (int gi : groups)
            tallest = std::max(tallest, rgl_group_height(m, m.groups[gi], met));
        // Columns hang from a common top line; the tier bus runs between them and the card.
        const double tier_top = above ? center.top() - bus_gap - tallest : center.bottom() + bus_gap;
        const double bus_y = above ? center.top() - bus_gap * 0.5 : center.bottom() + bus_gap * 0.5;
        const int n_groups = static_cast<int>(groups.size());
        for (int k = 0; k < n_groups; ++k) {
            const RelGraphGroup& grp = m.groups[groups[k]];
            RelGroupGeom gg;
            gg.group = grp.group;
            gg.left = above;
            const double gh = rgl_group_height(m, grp, met);
            gg.frame = QRectF(x, tier_top, met.card_w, gh);
            gg.header = QRectF(x, tier_top, met.card_w, met.header_h);
            res.groups << gg;
            // Where this group's trunk enters the card (spread over the card's top / bottom).
            const double port_x =
                center.left() + center.width() * (n_groups == 1 ? 0.5 : (0.12 + 0.76 * k / (n_groups - 1.0)));
            const QPointF port(port_x, above ? center.top() : center.bottom());
            const double col_bus_x = x - stub;
            double ry = tier_top + met.header_h;
            for (const QString& id : grp.node_ids) {
                const RelGraphNode* n = m.node(id);
                if (!n)
                    continue;
                RelNodeGeom ng;
                const double rows = n->kind == RelNodeKind::Placeholder ? rgl_group_rows(m, grp, met) : 1.0;
                const double h = rows * (met.row_h + met.row_gap) - met.row_gap;
                ng.rect = QRectF(x, ry, met.card_w, h);
                ng.center = ng.rect.center();
                ng.port = QPointF(ng.rect.left(), ng.center.y());
                res.nodes.insert(id, ng);
                ry += h + met.row_gap;
                for (int i = 0; i < m.edges.size(); ++i) {
                    const RelGraphEdge& e = m.edges[i];
                    if (e.second_hop || (e.from != id && e.to != id))
                        continue;
                    RelEdgeGeom g;
                    // Card -> its column bus -> the tier bus -> the card's port.
                    g.poly = {ng.port, QPointF(col_bus_x, ng.center.y()), QPointF(col_bus_x, bus_y),
                              QPointF(port.x(), bus_y), port};
                    g.p0 = ng.port;
                    g.p3 = port;
                    g.c1 = g.p0;
                    g.c2 = g.p3;
                    g.label_pos = QPointF(col_bus_x, ng.center.y());
                    res.edges[i] = g;
                }
            }
            x += met.card_w + col_gap;
        }
    };
    tier(top, true);
    tier(bottom, false);
    for (int i = 0; i < m.edges.size(); ++i) {
        if (!m.edges[i].second_hop)
            continue;
        const auto a = res.nodes.constFind(m.edges[i].from);
        const auto b = res.nodes.constFind(m.edges[i].to);
        if (a == res.nodes.constEnd() || b == res.nodes.constEnd()) {
            res.edges[i].visible = false;
            continue;
        }
        const QPointF pa(a.value().rect.right(), a.value().center.y());
        const QPointF pb(b.value().rect.right(), b.value().center.y());
        const double bulge = 12 + 0.2 * std::abs(pb.y() - pa.y());
        RelEdgeGeom g;
        g.p0 = pa;
        g.c1 = pa + QPointF(bulge, 0);
        g.c2 = pb + QPointF(bulge, 0);
        g.p3 = pb;
        g.label_pos = rgl_bezier(g.p0, g.c1, g.c2, g.p3, 0.5);
        res.edges[i] = g;
    }
}

// ── Bubbles (RADIAL / FORCE) ────────────────────────────────────────────────

struct RglBubbleSet {
    QVector<int> nodes; ///< model indexes drawn as bubbles (centre excluded)
    QVector<double> radius;
    double center_r = 60;
};

RglBubbleSet rgl_bubbles(const RelGraphModel& m, const RelLayoutMetrics& met) {
    RglBubbleSet b;
    double lo = 1e300, hi = -1e300;
    for (int i = 1; i < m.nodes.size(); ++i) {
        const RelGraphNode& n = m.nodes[i];
        if (n.kind == RelNodeKind::Placeholder)
            continue;
        b.nodes << i;
        if (rgl_finite(n.size_value) && n.size_value > 0) {
            lo = std::min(lo, std::log10(n.size_value));
            hi = std::max(hi, std::log10(n.size_value));
        }
    }
    for (int i : b.nodes) {
        const RelGraphNode& n = m.nodes[i];
        double r;
        if (n.kind == RelNodeKind::Cluster)
            r = met.bubble_min * 1.25;
        else if (rgl_finite(n.size_value) && n.size_value > 0 && hi >= lo)
            r = bubble_radius(n.size_value, lo, hi, met.bubble_min, met.bubble_max);
        else
            r = met.bubble_min * 1.1;
        b.radius << r;
    }
    b.center_r = met.bubble_max * 1.55;
    return b;
}

/// Angular sectors per group, proportional to the bubbles in it; owners on the left half.
QVector<QPair<double, double>> rgl_sectors(const RelGraphModel& m, const RglBubbleSet& b) {
    QVector<double> share(m.groups.size(), 0.0);
    for (int gi = 0; gi < m.groups.size(); ++gi) {
        int n = 0;
        for (const QString& id : m.groups[gi].node_ids) {
            const RelGraphNode* node = m.node(id);
            if (node && node->kind != RelNodeKind::Placeholder)
                ++n;
        }
        share[gi] = n + 1.5;
    }
    double total = 0;
    for (double s : share)
        total += s;
    QVector<QPair<double, double>> out(m.groups.size());
    const double gap = m.groups.size() > 1 ? 0.06 : 0.0; // radians between sectors
    const double usable = 2 * kPi - gap * m.groups.size();
    // Start at 6 o'clock and go clockwise (screen y down): owners fill the left half.
    double a = kPi / 2.0 + gap / 2.0;
    for (int gi = 0; gi < m.groups.size(); ++gi) {
        const double span = total > 0 ? usable * share[gi] / total : 0;
        out[gi] = {a, a + span};
        a += span + gap;
    }
    Q_UNUSED(b);
    return out;
}

void rgl_bubble_labels(const RelGraphModel& m, const RelLayoutMetrics& met, const QVector<double>& weights,
                       RelLayoutResult& res) {
    QVector<RelLabelRequest> req;
    QVector<QString> ids;
    QVector<QRectF> obstacles;
    for (const auto& n : m.nodes) {
        const auto it = res.nodes.constFind(n.id);
        if (it == res.nodes.constEnd())
            continue;
        obstacles << it.value().rect;
        if (n.id == m.center_id)
            continue;
        RelLabelRequest r;
        r.anchor = it.value().rect;
        r.w = rgl_label_w(met, n);
        r.h = met.label_h;
        r.priority = (n.kind == RelNodeKind::Cluster ? 0.5 : 1.0) / (1.0 + std::max(0, n.rank)) +
                     it.value().radius / std::max(1.0, met.bubble_max);
        req << r;
        ids << n.id;
    }
    Q_UNUSED(weights);
    const QVector<QRectF> placed = place_labels(req, obstacles, 2.0);
    for (int i = 0; i < ids.size(); ++i) {
        RelNodeGeom& g = res.nodes[ids[i]];
        g.label = placed.value(i);
        g.label_visible = !g.label.isEmpty();
    }
}

void rgl_bubble_edges(const RelGraphModel& m, RelLayoutResult& res) {
    for (int i = 0; i < m.edges.size(); ++i) {
        const auto a = res.nodes.constFind(m.edges[i].from);
        const auto b = res.nodes.constFind(m.edges[i].to);
        if (a == res.nodes.constEnd() || b == res.nodes.constEnd()) {
            res.edges[i].visible = false;
            continue;
        }
        // From rim to rim.
        const QPointF ca = a.value().center, cb = b.value().center;
        const QPointF d = cb - ca;
        const double len = std::hypot(d.x(), d.y());
        if (len < 1e-6) {
            res.edges[i].visible = false;
            continue;
        }
        const QPointF u = d / len;
        const QPointF pa = ca + u * a.value().radius;
        const QPointF pb = cb - u * b.value().radius;
        if (m.edges[i].second_hop) {
            // A gentle arc so second hops read apart from the spokes.
            const QPointF nrm(-u.y(), u.x());
            const QPointF mid = (pa + pb) / 2.0 + nrm * (len * 0.12);
            RelEdgeGeom g;
            g.p0 = pa;
            g.c1 = pa + (mid - pa) * 0.66;
            g.c2 = pb + (mid - pb) * 0.66;
            g.p3 = pb;
            g.label_pos = mid;
            res.edges[i] = g;
        } else {
            res.edges[i] = rgl_straight(pa, pb);
            res.edges[i].label_pos = pa + (pb - pa) * 0.55;
        }
    }
}

void rgl_radial(const RelGraphModel& m, const RelLayoutMetrics& met, const QVector<double>& weights,
                RelLayoutResult& res) {
    const RglBubbleSet b = rgl_bubbles(m, met);
    RelNodeGeom cg;
    cg.center = QPointF(0, 0);
    cg.radius = b.center_r;
    cg.bubble = true;
    cg.rect = QRectF(-b.center_r, -b.center_r, 2 * b.center_r, 2 * b.center_r);
    res.nodes.insert(m.center_id, cg);

    QHash<QString, double> radius_of;
    for (int k = 0; k < b.nodes.size(); ++k)
        radius_of.insert(m.nodes[b.nodes[k]].id, b.radius[k]);
    const auto sectors = rgl_sectors(m, b);
    const double ring1 = b.center_r + met.bubble_max * 2.6 + 40;
    const double ring_step = met.bubble_max * 2.1 + met.label_h;
    double outer = ring1;
    for (int gi = 0; gi < m.groups.size(); ++gi) {
        const auto& grp = m.groups[gi];
        QStringList ids;
        for (const QString& id : grp.node_ids)
            if (radius_of.contains(id))
                ids << id;
        const double a0 = sectors[gi].first, a1 = sectors[gi].second;
        RelGroupGeom gg;
        gg.group = grp.group;
        gg.mid_angle = (a0 + a1) / 2.0;
        gg.left = std::cos(gg.mid_angle) < 0;
        // Rings: as many as it takes for the bubbles of this sector to sit side by side.
        const int n = static_cast<int>(ids.size());
        double need = 0;
        for (const QString& id : ids)
            need += 2 * radius_of.value(id) + 6;
        const double arc1 = (a1 - a0) * ring1;
        const int rings = n == 0 ? 1 : std::clamp(static_cast<int>(std::ceil(need / std::max(1.0, arc1))), 1, 4);
        const int per_ring = n == 0 ? 0 : static_cast<int>(std::ceil(static_cast<double>(n) / rings));
        for (int k = 0; k < n; ++k) {
            const int ring = per_ring > 0 ? k % rings : 0;
            const int place = per_ring > 0 ? k / rings : 0;
            const int place_count =
                per_ring > 0 ? static_cast<int>(std::ceil(static_cast<double>(n - ring) / rings)) : 1;
            const double t = (place + 0.5 + (ring % 2) * 0.25) / std::max(1, place_count);
            const double ang = a0 + (a1 - a0) * std::clamp(t, 0.02, 0.98);
            const double r = ring1 + ring * ring_step;
            outer = std::max(outer, r);
            RelNodeGeom ng;
            ng.bubble = true;
            ng.radius = radius_of.value(ids[k]);
            ng.center = QPointF(r * std::cos(ang), r * std::sin(ang));
            ng.rect = QRectF(ng.center.x() - ng.radius, ng.center.y() - ng.radius, 2 * ng.radius, 2 * ng.radius);
            res.nodes.insert(ids[k], ng);
        }
        res.groups << gg;
    }
    // Group captions beyond the outer ring.
    for (auto& gg : res.groups) {
        const double r = outer + met.bubble_max + met.label_h * 2.2;
        gg.caption = QPointF(r * std::cos(gg.mid_angle), r * std::sin(gg.mid_angle));
        gg.header = QRectF(gg.caption.x() - met.char_w * 9, gg.caption.y() - met.label_h / 2, met.char_w * 18,
                           met.label_h);
        gg.frame = gg.header;
    }
    rgl_bubble_edges(m, res);
    rgl_bubble_labels(m, met, weights, res);
}

void rgl_force(const RelGraphModel& m, const RelLayoutMetrics& met, const QVector<double>& weights,
               const QHash<QString, QPointF>& previous, quint32 seed, RelLayoutResult& res) {
    // Start from the radial picture (or the one on screen), then relax.
    rgl_radial(m, met, weights, res);
    const RglBubbleSet b = rgl_bubbles(m, met);
    const int n = static_cast<int>(b.nodes.size());
    if (n == 0)
        return;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> jitter(-1.0, 1.0);
    QVector<QPointF> pos(n), vel(n, QPointF(0, 0)), anchor(n);
    QVector<double> rad = b.radius;
    QHash<QString, int> slot;
    bool warm = false;
    for (int k = 0; k < n; ++k) {
        const QString& id = m.nodes[b.nodes[k]].id;
        slot.insert(id, k);
        const RelNodeGeom& g = res.nodes[id];
        anchor[k] = g.center;
        const auto it = previous.constFind(id);
        if (it != previous.constEnd()) {
            pos[k] = it.value();
            warm = true;
        } else {
            pos[k] = g.center + QPointF(jitter(rng) * 30, jitter(rng) * 30);
        }
    }
    QVector<QPair<int, int>> springs; // -1 = centre
    QVector<double> rest;
    for (const auto& e : m.edges) {
        const int a = e.from == m.center_id ? -1 : slot.value(e.from, -2);
        const int c = e.to == m.center_id ? -1 : slot.value(e.to, -2);
        if (a == -2 || c == -2)
            continue;
        springs << qMakePair(a, c);
        rest << (a == -1 || c == -1 ? b.center_r + met.bubble_max * 3.2 : met.bubble_max * 3.0);
    }
    const int iters = warm ? 90 : 260;
    double temp = warm ? 6.0 : 26.0;
    const double cool = std::pow(0.5 / temp, 1.0 / iters);
    QVector<QPointF> force(n);
    for (int it = 0; it < iters; ++it) {
        std::fill(force.begin(), force.end(), QPointF(0, 0));
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                QPointF d = pos[i] - pos[j];
                double dist2 = d.x() * d.x() + d.y() * d.y();
                if (dist2 < 1e-4) {
                    d = QPointF(jitter(rng), jitter(rng));
                    dist2 = 1e-2;
                }
                const double dist = std::sqrt(dist2);
                const double min_d = rad[i] + rad[j] + met.label_h;
                double f = 5200.0 / dist2;
                if (dist < min_d)
                    f += (min_d - dist) * 0.9;
                const QPointF u = d / dist;
                force[i] += u * f;
                force[j] -= u * f;
            }
            // Keep clear of the company bubble.
            const double dc = std::hypot(pos[i].x(), pos[i].y());
            const double min_c = b.center_r + rad[i] + met.label_h * 1.5;
            if (dc < min_c && dc > 1e-6)
                force[i] += pos[i] / dc * (min_c - dc) * 1.2;
            // Each group towards its own sector.
            force[i] += (anchor[i] - pos[i]) * 0.035;
        }
        for (int s = 0; s < springs.size(); ++s) {
            const int a = springs[s].first, c = springs[s].second;
            const QPointF pa = a < 0 ? QPointF(0, 0) : pos[a];
            const QPointF pc = c < 0 ? QPointF(0, 0) : pos[c];
            const QPointF d = pc - pa;
            const double dist = std::max(1e-3, std::hypot(d.x(), d.y()));
            const double f = (dist - rest[s]) * 0.04;
            const QPointF u = d / dist;
            if (a >= 0)
                force[a] += u * f;
            if (c >= 0)
                force[c] -= u * f;
        }
        for (int i = 0; i < n; ++i) {
            vel[i] = (vel[i] + force[i]) * 0.55;
            const double sp = std::hypot(vel[i].x(), vel[i].y());
            if (sp > temp)
                vel[i] *= temp / sp;
            pos[i] += vel[i];
        }
        temp *= cool;
    }
    for (int k = 0; k < n; ++k) {
        RelNodeGeom& g = res.nodes[m.nodes[b.nodes[k]].id];
        g.center = pos[k];
        g.rect = QRectF(pos[k].x() - g.radius, pos[k].y() - g.radius, 2 * g.radius, 2 * g.radius);
    }
    // Group captions over their members' centroid, pushed outwards.
    for (auto& gg : res.groups) {
        QPointF sum(0, 0);
        int cnt = 0;
        for (const auto& grp : m.groups)
            if (grp.group == gg.group)
                for (const QString& id : grp.node_ids)
                    if (slot.contains(id)) {
                        sum += pos[slot.value(id)];
                        ++cnt;
                    }
        if (cnt == 0)
            continue;
        const QPointF c = sum / cnt;
        const double len = std::max(1.0, std::hypot(c.x(), c.y()));
        gg.caption = c + c / len * (met.bubble_max * 2.2 + met.label_h);
        gg.header = QRectF(gg.caption.x() - met.char_w * 9, gg.caption.y() - met.label_h / 2, met.char_w * 18,
                           met.label_h);
        gg.frame = gg.header;
    }
    rgl_bubble_edges(m, res);
    rgl_bubble_labels(m, met, weights, res);
}

// ── SANKEY ──────────────────────────────────────────────────────────────────

void rgl_sankey(const RelGraphModel& m, const RelLayoutMetrics& met, RelLayoutResult& res) {
    // Bands: owners (left) into the company bar; a fund's bar into its holdings (right).
    // Band heights are % of shares (or of the fund) on one scale; each band's slot is at
    // least a row tall so the labels never collide, and the bar side stays contiguous.
    const double full = met.row_h * 20.0; // 100 %
    const double bar_w = std::max(14.0, met.row_h * 0.7);
    const double gap = met.column_gap * 1.25;
    struct Band {
        QString id;
        double pct;
        bool left;
    };
    QVector<Band> bands;
    for (const auto& grp : m.groups) {
        const bool owner = grp.group == RelGroup::Institutions || grp.group == RelGroup::Insiders;
        const bool out = grp.group == RelGroup::Holdings;
        if (!owner && !out)
            continue;
        for (const QString& id : grp.node_ids) {
            const RelGraphNode* n = m.node(id);
            if (!n || n->kind == RelNodeKind::Placeholder || !rgl_finite(n->pct) || n->pct <= 0)
                continue;
            bands.append({id, n->pct, owner});
        }
    }
    const QRectF bar(-bar_w / 2, -full / 2, bar_w, full);
    RelNodeGeom cg;
    cg.rect = bar;
    cg.center = bar.center();
    res.nodes.insert(m.center_id, cg);
    double left_slot_y = -full / 2, right_slot_y = -full / 2;
    double left_bar_y = -full / 2, right_bar_y = -full / 2;
    const double min_slot = met.row_h * 0.9;
    for (const Band& bd : bands) {
        const double h = std::max(1.5, bd.pct / 100.0 * full);
        const double slot = std::max(h, min_slot);
        double& sy = bd.left ? left_slot_y : right_slot_y;
        double& by = bd.left ? left_bar_y : right_bar_y;
        RelNodeGeom ng;
        const double x = bd.left ? bar.left() - gap - bar_w : bar.right() + gap;
        ng.rect = QRectF(x, sy + (slot - h) / 2.0, bar_w, h);
        ng.center = ng.rect.center();
        ng.port = bd.left ? QPointF(ng.rect.right(), ng.center.y()) : QPointF(ng.rect.left(), ng.center.y());
        res.nodes.insert(bd.id, ng);
        const QPointF bar_port(bd.left ? bar.left() : bar.right(), by + h / 2.0);
        for (int i = 0; i < m.edges.size(); ++i) {
            const RelGraphEdge& e = m.edges[i];
            if (e.second_hop || (e.from != bd.id && e.to != bd.id))
                continue;
            RelEdgeGeom g = rgl_s_curve(ng.port, bar_port);
            g.band = h;
            res.edges[i] = g;
        }
        sy += slot + 2;
        by += h;
    }
    // Hide what has no band (peers, subsidiaries …): SANKEY is the ownership view.
    for (int i = 0; i < m.edges.size(); ++i) {
        const QString other = m.edges[i].from == m.center_id ? m.edges[i].to : m.edges[i].from;
        if (m.edges[i].second_hop || !res.nodes.contains(other))
            res.edges[i].visible = false;
    }
}

} // namespace

// ── Public ──────────────────────────────────────────────────────────────────

QString layout_key(RelLayoutMode m) {
    switch (m) {
        case RelLayoutMode::Tiered:
            return QStringLiteral("tiered");
        case RelLayoutMode::Hierarchy:
            return QStringLiteral("hierarchy");
        case RelLayoutMode::Radial:
            return QStringLiteral("radial");
        case RelLayoutMode::Force:
            return QStringLiteral("force");
        case RelLayoutMode::Sankey:
            return QStringLiteral("sankey");
        case RelLayoutMode::Matrix:
            return QStringLiteral("matrix");
    }
    return QStringLiteral("tiered");
}

bool layout_from_key(const QString& key, RelLayoutMode* out) {
    for (int i = 0; i <= static_cast<int>(RelLayoutMode::Matrix); ++i) {
        if (layout_key(static_cast<RelLayoutMode>(i)) == key) {
            if (out)
                *out = static_cast<RelLayoutMode>(i);
            return true;
        }
    }
    return false;
}

double bubble_radius(double value, double log_min, double log_max, double r_min, double r_max) {
    if (!rgl_finite(value) || value <= 0)
        return r_min;
    if (!(log_max > log_min))
        return (r_min + r_max) / 2.0;
    const double t = std::clamp((std::log10(value) - log_min) / (log_max - log_min), 0.0, 1.0);
    // Area-true: radius grows with the square root of the (log) magnitude share.
    return r_min + (r_max - r_min) * std::sqrt(t);
}

QVector<QRectF> place_labels(const QVector<RelLabelRequest>& labels, const QVector<QRectF>& obstacles, double gap) {
    QVector<QRectF> out(labels.size());
    QVector<int> order(labels.size());
    for (int i = 0; i < labels.size(); ++i)
        order[i] = i;
    std::stable_sort(order.begin(), order.end(),
                     [&](int a, int b) { return labels[a].priority > labels[b].priority; });
    RglGrid grid(64);
    for (int i = 0; i < obstacles.size(); ++i)
        grid.add(obstacles[i].adjusted(-gap / 2, -gap / 2, gap / 2, gap / 2));
    for (int idx : order) {
        const RelLabelRequest& l = labels[idx];
        const QRectF& a = l.anchor;
        const double w = l.w, h = l.h;
        const QRectF cands[] = {
            QRectF(a.right() + gap, a.center().y() - h / 2, w, h),      // right
            QRectF(a.left() - gap - w, a.center().y() - h / 2, w, h),   // left
            QRectF(a.center().x() - w / 2, a.bottom() + gap, w, h),     // below
            QRectF(a.center().x() - w / 2, a.top() - gap - h, w, h),    // above
            QRectF(a.right() + gap, a.bottom() - h * 0.3, w, h),        // below right
            QRectF(a.right() + gap, a.top() - h * 0.7, w, h),           // above right
            QRectF(a.left() - gap - w, a.bottom() - h * 0.3, w, h),     // below left
            QRectF(a.left() - gap - w, a.top() - h * 0.7, w, h),        // above left
        };
        for (const QRectF& c : cands) {
            if (grid.hits(c))
                continue;
            out[idx] = c;
            grid.add(c.adjusted(-gap / 2, -gap / 2, gap / 2, gap / 2));
            break;
        }
    }
    return out;
}

quint32 layout_seed(const QString& center_id) {
    quint32 h = 2166136261u;
    for (const QChar ch : center_id) {
        h ^= ch.unicode();
        h *= 16777619u;
    }
    return h ? h : 1u;
}

RelLayoutResult compute_layout(const RelGraphModel& model, RelLayoutMode mode, const RelLayoutMetrics& metrics,
                               const QVector<double>& weights, const QHash<QString, QPointF>& previous) {
    QElapsedTimer t;
    t.start();
    RelLayoutResult res;
    res.mode = mode;
    res.seed = metrics.seed ? metrics.seed : layout_seed(model.center_id);
    res.edges.resize(model.edges.size());
    switch (mode) {
        case RelLayoutMode::Tiered:
        case RelLayoutMode::Matrix: // the scene keeps the tiered picture behind the heatmap
            rgl_tiered(model, metrics, weights, res);
            break;
        case RelLayoutMode::Hierarchy:
            rgl_hierarchy(model, metrics, res);
            break;
        case RelLayoutMode::Radial:
            rgl_radial(model, metrics, weights, res);
            break;
        case RelLayoutMode::Force:
            rgl_force(model, metrics, weights, previous, res.seed, res);
            break;
        case RelLayoutMode::Sankey:
            rgl_sankey(model, metrics, res);
            break;
    }
    // Anything without a place (a cluster in SANKEY …) hides its edges.
    for (int i = 0; i < model.edges.size(); ++i)
        if (!res.nodes.contains(model.edges[i].from) || !res.nodes.contains(model.edges[i].to))
            res.edges[i].visible = false;
    QRectF b;
    for (const auto& n : std::as_const(res.nodes)) {
        b = b.united(n.rect);
        if (n.label_visible && !n.label.isEmpty())
            b = b.united(n.label);
    }
    for (const auto& g : std::as_const(res.groups))
        b = b.united(g.frame);
    b = b.united(res.left_caption).united(res.right_caption);
    res.bounds = b.adjusted(-24, -24, 24, 24);
    res.elapsed_us = t.nsecsElapsed() / 1000;
    return res;
}

} // namespace fincept::relmap
