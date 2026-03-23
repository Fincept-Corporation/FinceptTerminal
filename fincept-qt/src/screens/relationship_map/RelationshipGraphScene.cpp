// src/screens/relationship_map/RelationshipGraphScene.cpp
#include "screens/relationship_map/RelationshipGraphScene.h"

#include "ui/theme/Theme.h"

#include <QFont>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QPainterPath>
#include <QPen>
#include <QWheelEvent>
#include <QtMath>

namespace fincept::relmap {

// ── Node sizes by category ───────────────────────────────────────────────────

static QSizeF node_size(NodeCategory cat) {
    switch (cat) {
    case NodeCategory::Company:       return {220, 120};
    case NodeCategory::Peer:          return {160, 80};
    case NodeCategory::Institutional: return {150, 60};
    case NodeCategory::FundFamily:    return {170, 80};
    case NodeCategory::Insider:       return {150, 65};
    case NodeCategory::Event:         return {150, 55};
    case NodeCategory::SupplyChain:   return {160, 65};
    case NodeCategory::Metrics:       return {160, 80};
    }
    return {150, 60};
}

// ── Custom node graphics item ────────────────────────────────────────────────

class GraphNodeItem : public QGraphicsRectItem {
  public:
    GraphNodeItem(const GraphNode& data, QGraphicsItem* parent = nullptr)
        : QGraphicsRectItem(parent), node_data_(data) {
        auto sz = node_size(data.category);
        setRect(-sz.width() / 2, -sz.height() / 2, sz.width(), sz.height());

        auto col = category_color(data.category);
        auto bg = category_bg(data.category);

        setPen(QPen(col, data.category == NodeCategory::Company ? 2.0 : 1.0));
        setBrush(QBrush(bg));

        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setCursor(Qt::PointingHandCursor);
        setAcceptHoverEvents(true);

        QFont mono("Consolas", 9);
        QFont mono_bold("Consolas", 9, QFont::Bold);
        QFont mono_small("Consolas", 7);

        // Category badge
        auto* badge = new QGraphicsTextItem(this);
        badge->setPlainText(category_label(data.category));
        badge->setFont(mono_small);
        badge->setDefaultTextColor(col);
        badge->setPos(-sz.width() / 2 + 8, -sz.height() / 2 + 4);

        // Label (main text)
        auto* label = new QGraphicsTextItem(this);
        label->setPlainText(data.label);
        label->setFont(data.category == NodeCategory::Company ? QFont("Consolas", 12, QFont::Bold) : mono_bold);
        label->setDefaultTextColor(QColor("#e5e5e5"));
        label->setPos(-sz.width() / 2 + 8, -sz.height() / 2 + 18);

        // Sublabel
        if (!data.sublabel.isEmpty()) {
            auto* sub = new QGraphicsTextItem(this);
            sub->setPlainText(data.sublabel);
            sub->setFont(mono_small);
            sub->setDefaultTextColor(QColor("#808080"));
            sub->setPos(-sz.width() / 2 + 8, -sz.height() / 2 + 38);
        }

        // Key metric in bottom-right
        if (data.value != 0) {
            auto* val = new QGraphicsTextItem(this);
            QString formatted;
            double v = data.value;
            if (v >= 1e12) formatted = QString("$%1T").arg(v / 1e12, 0, 'f', 1);
            else if (v >= 1e9) formatted = QString("$%1B").arg(v / 1e9, 0, 'f', 1);
            else if (v >= 1e6) formatted = QString("$%1M").arg(v / 1e6, 0, 'f', 1);
            else if (v > 100) formatted = QString("$%1").arg(v, 0, 'f', 0);
            else if (v > 0 && v <= 100) formatted = QString("%1%").arg(v, 0, 'f', 1);
            else formatted = QString::number(v, 'f', 2);

            val->setPlainText(formatted);
            val->setFont(mono_bold);
            val->setDefaultTextColor(col);
            val->setPos(sz.width() / 2 - 70, sz.height() / 2 - 20);
        }
    }

    const GraphNode& node_data() const { return node_data_; }

  protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override {
        auto col = category_color(node_data_.category);
        setPen(QPen(col, 2.5));
        setZValue(10);
        update();
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override {
        auto col = category_color(node_data_.category);
        setPen(QPen(col, node_data_.category == NodeCategory::Company ? 2.0 : 1.0));
        setZValue(0);
        update();
    }

  private:
    GraphNode node_data_;
};

// ── Scene ────────────────────────────────────────────────────────────────────

RelationshipGraphScene::RelationshipGraphScene(QObject* parent)
    : QGraphicsScene(parent) {
    setBackgroundBrush(QBrush(QColor("#080808")));
}

void RelationshipGraphScene::clear_graph() {
    clear();
    node_positions_.clear();
}

void RelationshipGraphScene::build_graph(const RelationshipData& data,
                                          const FilterState& filters,
                                          LayoutMode layout) {
    clear_graph();

    QVector<GraphNode> nodes;
    QVector<GraphEdge> edges;
    const QString company_id = "company-" + data.company.ticker;

    // 1. Company node (always)
    {
        GraphNode n;
        n.id = company_id;
        n.category = NodeCategory::Company;
        n.label = data.company.ticker;
        n.sublabel = data.company.name;
        n.value = data.company.market_cap;
        n.properties["Sector"] = data.company.sector;
        n.properties["Industry"] = data.company.industry;
        n.properties["Price"] = QString("$%1").arg(data.company.current_price, 0, 'f', 2);
        n.properties["Mkt Cap"] = QString("$%1B").arg(data.company.market_cap / 1e9, 0, 'f', 1);
        n.properties["P/E"] = QString::number(data.company.pe_ratio, 'f', 1);
        n.properties["Signal"] = data.valuation.action;
        nodes.append(n);
    }

    // 2. Peers
    if (filters.show_peers) {
        for (const auto& p : data.peers) {
            QString pid = "peer-" + p.ticker;
            GraphNode n;
            n.id = pid;
            n.category = NodeCategory::Peer;
            n.label = p.ticker;
            n.sublabel = p.name;
            n.value = p.market_cap;
            n.properties["Price"] = QString("$%1").arg(p.current_price, 0, 'f', 2);
            n.properties["P/E"] = QString::number(p.pe_ratio, 'f', 1);
            n.properties["ROE"] = QString("%1%").arg(p.roe * 100, 0, 'f', 1);
            nodes.append(n);
            edges.append({company_id, pid, EdgeCategory::Peer, "peer"});
        }
    }

    // 3. Institutional holders
    if (filters.show_institutional) {
        for (const auto& h : data.institutional_holders) {
            if (h.percentage < filters.min_ownership) continue;
            QString hid = "inst-" + h.name.left(20).replace(' ', '-');
            GraphNode n;
            n.id = hid;
            n.category = NodeCategory::Institutional;
            n.label = h.name.left(25);
            n.sublabel = QString("%1%").arg(h.percentage, 0, 'f', 2);
            n.value = h.percentage;
            n.properties["Shares"] = QString::number(h.shares, 'f', 0);
            n.properties["Value"] = QString("$%1M").arg(h.value / 1e6, 0, 'f', 1);
            nodes.append(n);
            edges.append({hid, company_id, EdgeCategory::Ownership, QString("%1%").arg(h.percentage, 0, 'f', 1), h.percentage / 10.0});
        }
    }

    // 4. Insiders
    if (filters.show_insiders) {
        for (const auto& ins : data.insider_holders) {
            QString iid = "insider-" + ins.name.left(20).replace(' ', '-');
            GraphNode n;
            n.id = iid;
            n.category = NodeCategory::Insider;
            n.label = ins.name.left(25);
            n.sublabel = ins.title.left(30);
            n.properties["Title"] = ins.title;
            if (!ins.last_transaction.isEmpty())
                n.properties["Last Tx"] = ins.last_transaction.toUpper();
            nodes.append(n);
            edges.append({iid, company_id, EdgeCategory::Internal, ""});
        }
    }

    // 5. Metrics clusters
    if (filters.show_metrics) {
        // Financials
        if (data.company.revenue > 0) {
            GraphNode n;
            n.id = "metrics-financials";
            n.category = NodeCategory::Metrics;
            n.label = "FINANCIALS";
            n.properties["Revenue"] = QString("$%1B").arg(data.company.revenue / 1e9, 0, 'f', 1);
            n.properties["EBITDA"] = QString("$%1B").arg(data.company.ebitda / 1e9, 0, 'f', 1);
            n.properties["FCF"] = QString("$%1B").arg(data.company.free_cashflow / 1e9, 0, 'f', 1);
            n.properties["Margins"] = QString("%1%").arg(data.company.profit_margins * 100, 0, 'f', 1);
            nodes.append(n);
            edges.append({company_id, "metrics-financials", EdgeCategory::Internal, ""});
        }

        // Balance sheet
        if (data.company.total_cash > 0 || data.company.total_debt > 0) {
            GraphNode n;
            n.id = "metrics-balance";
            n.category = NodeCategory::Metrics;
            n.label = "BALANCE SHEET";
            n.properties["Cash"] = QString("$%1B").arg(data.company.total_cash / 1e9, 0, 'f', 1);
            n.properties["Debt"] = QString("$%1B").arg(data.company.total_debt / 1e9, 0, 'f', 1);
            double net = data.company.total_cash - data.company.total_debt;
            n.properties["Net"] = QString("$%1B").arg(net / 1e9, 0, 'f', 1);
            nodes.append(n);
            edges.append({company_id, "metrics-balance", EdgeCategory::Internal, ""});
        }

        // Valuation
        if (data.company.pe_ratio > 0) {
            GraphNode n;
            n.id = "metrics-valuation";
            n.category = NodeCategory::Metrics;
            n.label = "VALUATION";
            n.properties["P/E"] = QString::number(data.company.pe_ratio, 'f', 1);
            n.properties["Fwd P/E"] = QString::number(data.company.forward_pe, 'f', 1);
            n.properties["P/B"] = QString::number(data.company.price_to_book, 'f', 2);
            n.properties["Signal"] = data.valuation.status;
            nodes.append(n);
            edges.append({company_id, "metrics-valuation", EdgeCategory::Internal, ""});
        }
    }

    // Apply layout
    apply_layout(nodes, edges, layout);

    // Add edge items first (under nodes)
    for (const auto& edge : edges) {
        if (node_positions_.contains(edge.source_id) && node_positions_.contains(edge.target_id)) {
            add_edge_item(node_positions_[edge.source_id], node_positions_[edge.target_id],
                          edge.category, edge.label);
        }
    }

    // Add node items
    for (const auto& node : nodes) {
        if (node_positions_.contains(node.id)) {
            add_node_item(node, node_positions_[node.id]);
        }
    }

    // Set scene rect with padding
    QRectF bounds = itemsBoundingRect();
    setSceneRect(bounds.adjusted(-100, -100, 100, 100));
}

// ── Layout ───────────────────────────────────────────────────────────────────

void RelationshipGraphScene::apply_layout(QVector<GraphNode>& nodes,
                                           const QVector<GraphEdge>&,
                                           LayoutMode mode) {
    node_positions_.clear();
    QPointF center(0, 0);

    // Group indices by category
    QVector<int> company_idx, peer_idx, inst_idx, insider_idx, metrics_idx, event_idx, sc_idx;
    for (int i = 0; i < nodes.size(); ++i) {
        switch (nodes[i].category) {
        case NodeCategory::Company:       company_idx.append(i); break;
        case NodeCategory::Peer:          peer_idx.append(i); break;
        case NodeCategory::Institutional: inst_idx.append(i); break;
        case NodeCategory::Insider:       insider_idx.append(i); break;
        case NodeCategory::Metrics:       metrics_idx.append(i); break;
        case NodeCategory::Event:         event_idx.append(i); break;
        case NodeCategory::SupplyChain:   sc_idx.append(i); break;
        case NodeCategory::FundFamily:    inst_idx.append(i); break; // group with institutional
        }
    }

    QVector<QPointF> positions(nodes.size());

    // Company at center
    for (int i : company_idx) positions[i] = center;

    if (mode == LayoutMode::Layered || mode == LayoutMode::Radial) {
        // Concentric rings
        place_in_ring(metrics_idx, positions, 280, -M_PI / 4, center);
        place_in_ring(insider_idx, positions, 380, 0, center);
        place_in_ring(inst_idx, positions, 550, M_PI / 6, center);
        place_in_ring(peer_idx, positions, 720, 0, center);
        place_in_ring(event_idx, positions, 850, M_PI / 4, center);
        place_in_ring(sc_idx, positions, 650, -M_PI / 3, center);
    } else {
        // Force-like: spread randomly then iterate
        double spread = 200.0;
        for (int i = 0; i < nodes.size(); ++i) {
            if (nodes[i].category == NodeCategory::Company) continue;
            double angle = (double)i / nodes.size() * 2 * M_PI;
            double r = spread + (i % 3) * 150;
            positions[i] = QPointF(r * qCos(angle), r * qSin(angle));
        }
    }

    for (int i = 0; i < nodes.size(); ++i) {
        node_positions_[nodes[i].id] = positions[i];
    }
}

void RelationshipGraphScene::place_in_ring(const QVector<int>& indices,
                                            QVector<QPointF>& positions,
                                            double radius, double start_angle,
                                            const QPointF& center) {
    if (indices.isEmpty()) return;
    double step = 2 * M_PI / std::max((int)indices.size(), 1);
    // Limit arc to avoid overlapping with other rings
    double arc = std::min(2 * M_PI, step * indices.size());
    double actual_step = arc / std::max((int)indices.size(), 1);

    for (int i = 0; i < indices.size(); ++i) {
        double angle = start_angle + i * actual_step;
        double x = center.x() + radius * qCos(angle);
        double y = center.y() + radius * qSin(angle);
        positions[indices[i]] = QPointF(x, y);
    }
}

// ── Item creation ────────────────────────────────────────────────────────────

void RelationshipGraphScene::add_node_item(const GraphNode& node, const QPointF& pos) {
    auto* item = new GraphNodeItem(node);
    item->setPos(pos);
    addItem(item);
}

void RelationshipGraphScene::add_edge_item(const QPointF& from, const QPointF& to,
                                            EdgeCategory cat, const QString& label) {
    QPainterPath path;
    path.moveTo(from);

    // Bezier curve for visual appeal
    QPointF mid = (from + to) / 2;
    double dx = to.x() - from.x();
    double dy = to.y() - from.y();
    QPointF ctrl1(mid.x() - dy * 0.15, mid.y() + dx * 0.15);

    path.quadTo(ctrl1, to);

    auto* line = new QGraphicsPathItem(path);
    QColor col = edge_color(cat);
    QPen pen(col, cat == EdgeCategory::Ownership ? 2.0 : 1.0);
    if (cat == EdgeCategory::Peer || cat == EdgeCategory::Event) {
        pen.setStyle(Qt::DashLine);
    }
    line->setPen(pen);
    line->setZValue(-1);
    addItem(line);

    // Edge label
    if (!label.isEmpty()) {
        auto* text = new QGraphicsTextItem(label);
        text->setFont(QFont("Consolas", 7));
        text->setDefaultTextColor(col);
        text->setPos(mid - QPointF(15, 8));
        text->setZValue(-1);
        addItem(text);
    }
}

// ── Mouse handling ───────────────────────────────────────────────────────────

// Override scene mouse press to detect node clicks vs background
// (handled in the main screen via item selection signals)

// ── View ─────────────────────────────────────────────────────────────────────

RelationshipGraphView::RelationshipGraphView(QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent) {
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setStyleSheet("QGraphicsView { border: none; background: #080808; }");
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Initial zoom to fit
    scale(0.6, 0.6);
}

void RelationshipGraphView::wheelEvent(QWheelEvent* event) {
    double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    scale(factor, factor);
}

void RelationshipGraphView::mousePressEvent(QMouseEvent* event) {
    QGraphicsView::mousePressEvent(event);
}

} // namespace fincept::relmap
