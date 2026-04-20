// src/screens/relationship_map/RelationshipGraphScene.cpp
// Banded left/right tree layout — zero trig, zero collisions.
// Right side: Peers, Holders, Funds, Analysts
// Left side:  Board, Insiders, Metrics, Events, Supply
// Each cluster owns an exclusive vertical band. No angle math.
#include "screens/relationship_map/RelationshipGraphScene.h"

#include "ui/theme/Theme.h"

#include <QFont>
#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>
#include <QWheelEvent>
#include <QtMath>

#include <cmath>

namespace fincept::relmap {

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr qreal kCenterW      = 220.0;
static constexpr qreal kCenterH      = 110.0;
static constexpr qreal kHubW         = 158.0;
static constexpr qreal kHubH         = 34.0;
static constexpr qreal kLeafW        = 162.0;
static constexpr qreal kLeafH        = 46.0;
static constexpr qreal kLeafGap      = 8.0;   // gap between leaves in same cluster
static constexpr qreal kClusterGap   = 28.0;  // gap between clusters on same side
// Fixed horizontal positions — no trig
static constexpr qreal kHubX_R       =  310.0; // hub center X, right side
static constexpr qreal kLeafX_R      =  500.0; // leaf center X, right side
static constexpr qreal kHubX_L       = -310.0; // hub center X, left side
static constexpr qreal kLeafX_L      = -500.0; // leaf center X, left side
// Kept for ABI compat with header declarations
static constexpr qreal kHubRadius      = 310.0;
static constexpr qreal kLeafRadius     = 500.0;
static constexpr qreal kClusterArcGap  = 0.0;
static constexpr qreal kLeafColumnOffset = 20.0;

// ── Forward declarations ──────────────────────────────────────────────────────
class RelNode;
class RelEdge;

// ── Edge item ─────────────────────────────────────────────────────────────────
class RelEdge : public QGraphicsItem {
  public:
    RelEdge(RelNode* source, RelNode* dest, const QColor& color, bool thin = false)
        : source_(source), dest_(dest), color_(color), thin_(thin) {
        setZValue(-1);
        setAcceptedMouseButtons(Qt::NoButton);
    }
    RelNode* source() const { return source_; }
    RelNode* dest()   const { return dest_; }
    void adjust();
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override;
  private:
    RelNode* source_;
    RelNode* dest_;
    QColor   color_;
    bool     thin_;
    QPointF  source_pt_;
    QPointF  dest_pt_;
};

// ── Node item ─────────────────────────────────────────────────────────────────
class RelNode : public QGraphicsItem {
  public:
    enum class Role { Center, Hub, Leaf };

    RelNode(RelationshipGraphScene* graph, const QString& label, const QString& sub,
            const QColor& accent, qreal w, qreal h, Role role)
        : graph_(graph), label_(label), sub_(sub), accent_(accent)
        , w_(w), h_(h), role_(role) {
        setFlag(QGraphicsItem::ItemIsMovable, role != Role::Center);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges);
        setCacheMode(QGraphicsItem::DeviceCoordinateCache);
        setZValue(role == Role::Center ? 5 : (role == Role::Hub ? 3 : 1));
        if (role == Role::Center) setCursor(Qt::PointingHandCursor);
        else setCursor(Qt::OpenHandCursor);
    }

    void add_edge(RelEdge* edge) { edges_.append(edge); }
    Role role() const { return role_; }

    QRectF boundingRect() const override {
        return QRectF(-w_ / 2 - 2, -h_ / 2 - 2, w_ + 4, h_ + 4);
    }

    QPainterPath shape() const override {
        QPainterPath path;
        path.addRect(-w_ / 2, -h_ / 2, w_, h_);
        return path;
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::TextAntialiasing);

        QRectF r(-w_ / 2, -h_ / 2, w_, h_);

        if (role_ == Role::Center) {
            // Outer glow
            QColor glow = accent_; glow.setAlpha(30);
            painter->setPen(QPen(glow, 6.0));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(r.adjusted(-3, -3, 3, 3));

            // Background gradient
            QLinearGradient bg(r.topLeft(), r.bottomLeft());
            bg.setColorAt(0.0, QColor(28, 16, 4, 252));
            bg.setColorAt(1.0, QColor(14, 8, 2, 252));
            painter->setPen(Qt::NoPen);
            painter->setBrush(QBrush(bg));
            painter->drawRect(r);

            // Top stripe
            QColor stripe = accent_; stripe.setAlpha(220);
            painter->setBrush(QBrush(stripe));
            painter->drawRect(QRectF(r.left(), r.top(), r.width(), 3));

            // Border
            painter->setPen(QPen(accent_, 1.5));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(r);

            // Divider
            qreal divY = r.top() + h_ * 0.42;
            QColor div_col = accent_; div_col.setAlpha(50);
            painter->setPen(QPen(div_col, 1.0));
            painter->drawLine(QPointF(r.left() + 10, divY), QPointF(r.right() - 10, divY));

            // Ticker
            QFont tf("Consolas", 15, QFont::Bold);
            painter->setFont(tf);
            painter->setPen(accent_);
            painter->drawText(QRectF(r.left()+12, r.top()+8, r.width()-24, h_*0.38),
                              Qt::AlignLeft | Qt::AlignVCenter, label_);

            // Sub lines
            if (!sub_.isEmpty()) {
                QStringList lines = sub_.split('\n');
                QFont sf("Consolas", 8);
                painter->setFont(sf);
                qreal line_h = (h_ * 0.54) / qMax(lines.size(), 1);
                for (int i = 0; i < lines.size(); ++i) {
                    QColor col = (i == 0) ? QColor(220,220,220,230) : QColor(160,160,160,190);
                    painter->setPen(col);
                    painter->drawText(QRectF(r.left()+12, divY+5+i*line_h, r.width()-24, line_h),
                                      Qt::AlignLeft | Qt::AlignVCenter, lines[i]);
                }
            }

        } else if (role_ == Role::Hub) {
            // Gradient background
            QLinearGradient bg(r.topLeft(), r.bottomLeft());
            QColor c1 = accent_.darker(130); c1.setAlpha(240);
            QColor c2 = accent_.darker(110); c2.setAlpha(240);
            bg.setColorAt(0.0, c1);
            bg.setColorAt(1.0, c2);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QBrush(bg));
            painter->drawRect(r);

            // Shimmer line
            QColor shimmer = accent_.lighter(180); shimmer.setAlpha(70);
            painter->setPen(QPen(shimmer, 1.0));
            painter->drawLine(QPointF(r.left(), r.top()+1), QPointF(r.right(), r.top()+1));

            // Border
            QColor border = accent_.lighter(140); border.setAlpha(160);
            painter->setPen(QPen(border, 1.0));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(r);

            // Label
            QFont lf("Consolas", 9, QFont::Bold);
            painter->setFont(lf);
            painter->setPen(QColor(255, 255, 255, 235));
            painter->drawText(r.adjusted(8, 0, -8, 0), Qt::AlignCenter, label_);

        } else {
            // Dark gradient background
            QLinearGradient bg(r.topLeft(), r.bottomLeft());
            bg.setColorAt(0.0, QColor(32, 32, 40, 235));
            bg.setColorAt(1.0, QColor(20, 20, 26, 235));
            painter->setPen(Qt::NoPen);
            painter->setBrush(QBrush(bg));
            painter->drawRect(r);

            // Left accent bar
            QColor bar = accent_; bar.setAlpha(255);
            painter->setBrush(QBrush(bar));
            painter->drawRect(QRectF(r.left(), r.top(), 3, r.height()));

            // Top tint
            QLinearGradient tint_g(r.topLeft(), QPointF(r.left(), r.top()+12));
            QColor tint = accent_; tint.setAlpha(25);
            tint_g.setColorAt(0.0, tint);
            tint_g.setColorAt(1.0, Qt::transparent);
            painter->setBrush(QBrush(tint_g));
            painter->drawRect(r);

            // Border
            QColor border = accent_; border.setAlpha(70);
            painter->setPen(QPen(border, 1.0));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(r);

            // Divider
            if (!sub_.isEmpty()) {
                qreal divY = r.top() + r.height() * 0.52;
                painter->setPen(QPen(QColor(255,255,255,12), 1.0));
                painter->drawLine(QPointF(r.left()+10, divY), QPointF(r.right()-4, divY));
            }

            // Label
            QFont lf("Consolas", 8, QFont::Bold);
            painter->setFont(lf);
            painter->setPen(QColor(238, 238, 238, 245));
            QRectF top_r(r.left()+10, r.top()+4, r.width()-16,
                         sub_.isEmpty() ? r.height()-8 : r.height()*0.48);
            painter->drawText(top_r, Qt::AlignLeft | Qt::AlignVCenter, label_);

            // Sub label
            if (!sub_.isEmpty()) {
                QFont sf("Consolas", 7);
                painter->setFont(sf);
                QColor sub_col = accent_.lighter(160); sub_col.setAlpha(180);
                painter->setPen(sub_col);
                QRectF bot_r(r.left()+10, r.top()+r.height()*0.52, r.width()-16, r.height()*0.44);
                painter->drawText(bot_r, Qt::AlignLeft | Qt::AlignVCenter, sub_);
            }
        }
    }

  protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override {
        if (change == ItemPositionHasChanged)
            for (RelEdge* e : std::as_const(edges_)) e->adjust();
        return QGraphicsItem::itemChange(change, value);
    }
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        if (role_ == Role::Center && event->button() == Qt::LeftButton) {
            auto* rms = qobject_cast<RelationshipGraphScene*>(scene());
            if (rms) emit rms->center_card_clicked(label_.split(" ").first());
        }
        update();
        QGraphicsItem::mousePressEvent(event);
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override {
        update();
        QGraphicsItem::mouseReleaseEvent(event);
    }
  private:
    RelationshipGraphScene* graph_;
    QVector<RelEdge*> edges_;
    QString label_, sub_;
    QColor  accent_;
    qreal   w_, h_;
    Role    role_;
};

// ── Edge implementation ───────────────────────────────────────────────────────
void RelEdge::adjust() {
    if (!source_ || !dest_) return;
    QLineF line(mapFromItem(source_, 0, 0), mapFromItem(dest_, 0, 0));
    prepareGeometryChange();
    source_pt_ = line.p1();
    dest_pt_   = line.p2();
}

QRectF RelEdge::boundingRect() const {
    qreal extra = 4.0;
    return QRectF(source_pt_, QSizeF(dest_pt_.x()-source_pt_.x(),
                                      dest_pt_.y()-source_pt_.y()))
        .normalized().adjusted(-extra,-extra,extra,extra);
}

void RelEdge::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    if (!source_ || !dest_) return;
    QLineF line(source_pt_, dest_pt_);
    if (qFuzzyCompare(line.length(), qreal(0.))) return;

    painter->setRenderHint(QPainter::Antialiasing);

    QLinearGradient grad(source_pt_, dest_pt_);
    QColor c_near = color_; c_near.setAlpha(thin_ ? 90 : 140);
    QColor c_far  = color_; c_far.setAlpha(thin_ ? 30 : 55);
    grad.setColorAt(0.0, c_near);
    grad.setColorAt(1.0, c_far);

    QPen pen;
    pen.setBrush(QBrush(grad));
    pen.setWidthF(thin_ ? 1.0 : 1.8);
    pen.setCapStyle(Qt::RoundCap);
    painter->setPen(pen);
    painter->drawLine(source_pt_, dest_pt_);
}

// ── Scene ─────────────────────────────────────────────────────────────────────
RelationshipGraphScene::RelationshipGraphScene(QObject* parent)
    : QGraphicsScene(parent) {
    setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    setSceneRect(-1400, -1100, 2800, 2200);
}

void RelationshipGraphScene::clear_graph() {
    if (sim_timer_) sim_timer_->stop();
    clear();
    node_positions_.clear();
    nodes_.clear();
    timer_ticks_ = 0;
}

// ── build_graph ───────────────────────────────────────────────────────────────
//
// Banded left/right layout — pure rectangle arithmetic, guaranteed no collisions:
//
//   RIGHT side (Peers, Holders, Funds, Analysts):
//     Hub center X = kHubX_R,  Leaf center X = kLeafX_R
//
//   LEFT side (Board, Insiders, Metrics, Events, Supply):
//     Hub center X = kHubX_L,  Leaf center X = kLeafX_L
//
//   Each cluster owns an exclusive vertical band:
//     band_height = max(kHubH, n_leaves*(kLeafH+kLeafGap)-kLeafGap)
//   Clusters stack top-to-bottom with kClusterGap between them.
//   Hub and leaf column are both vertically centered inside their band.
//   Center node sits at Y=0 (visual midpoint).

void RelationshipGraphScene::build_graph(
    const RelationshipData& data, const FilterState& filters, LayoutMode /*layout*/)
{
    clear_graph();

    const QColor cAmber("#d97706");
    const QColor cPeer ("#2563eb");
    const QColor cInst ("#16a34a");
    const QColor cMF   ("#059669");
    const QColor cIns  ("#0891b2");
    const QColor cOff  ("#0e7490");
    const QColor cAnal ("#7c3aed");
    const QColor cEvt  ("#b45309");
    const QColor cSC   ("#ca8a04");
    const QColor cMet  ("#6b7280");

    // ── Center node ──────────────────────────────────────────────────────────
    bool up = data.company.day_change_pct >= 0;
    QString center_sub = QString("%1\n$%2  %3%4%\nMktCap $%5B")
        .arg(data.company.name.left(24))
        .arg(data.company.current_price, 0, 'f', 2)
        .arg(up ? "+" : "")
        .arg(data.company.day_change_pct, 0, 'f', 2)
        .arg(data.company.market_cap / 1e9, 0, 'f', 0);

    auto* center_node = new RelNode(this, data.company.ticker, center_sub,
                                    cAmber, kCenterW, kCenterH, RelNode::Role::Center);
    center_node->setPos(0, 0);
    addItem(center_node);
    nodes_.append(center_node);

    // ── Cluster data ─────────────────────────────────────────────────────────
    struct LeafInfo { QString label, sub; QColor accent; };
    struct ClusterDef {
        QString           label;
        QColor            color;
        QVector<LeafInfo> leaves;
        bool              right; // true = right side
    };

    QVector<ClusterDef> right_side, left_side;

    auto push_right = [&](const QString& lbl, const QColor& col, QVector<LeafInfo>&& lv) {
        if (!lv.isEmpty()) right_side.push_back({lbl, col, std::move(lv), true});
    };
    auto push_left = [&](const QString& lbl, const QColor& col, QVector<LeafInfo>&& lv) {
        if (!lv.isEmpty()) left_side.push_back({lbl, col, std::move(lv), false});
    };

    // PEERS → right
    if (filters.show_peers && !data.peers.isEmpty()) {
        QVector<LeafInfo> lv;
        for (const auto& p : data.peers) {
            if (lv.size() >= 12) break;
            QColor acc = (p.week52_change >= 0) ? cPeer : QColor("#dc2626");
            lv.append({p.ticker, QString("$%1").arg(p.current_price, 0, 'f', 2), acc});
        }
        push_right(QString("Peers (%1/%2)").arg(lv.size()).arg(data.peers.size()),
                   cPeer, std::move(lv));
    }

    // INSTITUTIONAL → right
    if (filters.show_institutional && !data.institutional_holders.isEmpty()) {
        QVector<LeafInfo> lv;
        for (const auto& h : data.institutional_holders) {
            if (lv.size() >= 8) break;
            if (h.percentage < filters.min_ownership) continue;
            lv.append({h.name.left(14), QString("%1%").arg(h.percentage, 0, 'f', 2), cInst});
        }
        push_right(QString("Holders (%1/%2)").arg(lv.size()).arg(data.institutional_holders.size()),
                   cInst, std::move(lv));
    }

    // MUTUAL FUNDS → right
    if (filters.show_institutional && !data.mutualfund_holders.isEmpty()) {
        QVector<LeafInfo> lv;
        for (const auto& h : data.mutualfund_holders) {
            if (lv.size() >= 8) break;
            lv.append({h.name.left(14), QString("%1%").arg(h.percentage, 0, 'f', 2), cMF});
        }
        push_right(QString("Funds (%1/%2)").arg(lv.size()).arg(data.mutualfund_holders.size()),
                   cMF, std::move(lv));
    }

    // ANALYSTS → right
    if (filters.show_analysts) {
        QVector<LeafInfo> lv;
        if (data.company.analyst_count > 0) {
            QString rec = data.company.recommendation.toUpper();
            bool bull = rec.contains("BUY") || rec.contains("OUTPERFORM");
            lv.append({rec, QString("PT $%1").arg(data.company.target_mean, 0, 'f', 0),
                       bull ? QColor("#16a34a") : QColor("#dc2626")});
        }
        for (const auto& ud : data.upgrades_downgrades) {
            if (lv.size() >= 8) break;
            QColor acc = (ud.action == "up") ? QColor("#16a34a")
                       : (ud.action == "down") ? QColor("#dc2626") : cAnal;
            lv.append({ud.firm.left(14), ud.to_grade.left(12), acc});
        }
        push_right(QString("Analysts (%1)").arg(data.company.analyst_count),
                   cAnal, std::move(lv));
    }

    // BOARD → left
    if (filters.show_officers && !data.officers.isEmpty()) {
        QVector<LeafInfo> lv;
        for (const auto& off : data.officers) {
            if (lv.size() >= 10) break;
            lv.append({off.name.left(14), off.title.left(14), cOff});
        }
        push_left(QString("Board (%1/%2)").arg(lv.size()).arg(data.officers.size()),
                  cOff, std::move(lv));
    }

    // INSIDERS → left
    if (filters.show_insiders && !data.insider_holders.isEmpty()) {
        QVector<LeafInfo> lv;
        for (const auto& ins : data.insider_holders) {
            if (lv.size() >= 8) break;
            bool buy = ins.last_transaction.toLower() == "buy";
            QString sub = ins.last_transaction.isEmpty() ? ins.title.left(12)
                                                         : ins.last_transaction.toUpper();
            lv.append({ins.name.left(14), sub, buy ? QColor("#16a34a") : cIns});
        }
        push_left(QString("Insiders (%1/%2)").arg(lv.size()).arg(data.insider_holders.size()),
                  cIns, std::move(lv));
    }

    // METRICS → left
    if (filters.show_metrics) {
        QVector<LeafInfo> lv;
        if (data.company.pe_ratio > 0)
            lv.append({"P/E", QString("%1  fwd %2")
                       .arg(data.company.pe_ratio, 0, 'f', 1)
                       .arg(data.company.forward_pe, 0, 'f', 1), cMet});
        if (data.margins.gross > 0)
            lv.append({"Margins", QString("Gr %1%  Net %2%")
                       .arg(data.margins.gross * 100, 0, 'f', 1)
                       .arg(data.margins.net * 100, 0, 'f', 1), cMet});
        if (data.technicals.beta > 0)
            lv.append({"Beta", QString("%1   52W $%2")
                       .arg(data.technicals.beta, 0, 'f', 2)
                       .arg(data.technicals.fifty_two_week_high, 0, 'f', 0), cMet});
        if (data.short_interest.short_pct_float > 0)
            lv.append({"Short Int", QString("%1%  %2d cvr")
                       .arg(data.short_interest.short_pct_float * 100, 0, 'f', 1)
                       .arg(data.short_interest.short_ratio, 0, 'f', 1),
                       data.short_interest.short_pct_float > 0.05 ? QColor("#dc2626") : cMet});
        if (data.governance.overall_risk > 0)
            lv.append({"Governance", QString("Risk %1/10  Audit %2")
                       .arg(data.governance.overall_risk).arg(data.governance.audit_risk),
                       data.governance.overall_risk > 6 ? QColor("#dc2626") : cMet});
        push_left("Metrics", cMet, std::move(lv));
    }

    // EVENTS → left
    if (filters.show_events && !data.events.isEmpty()) {
        QVector<LeafInfo> lv;
        for (const auto& ev : data.events) {
            if (lv.size() >= 6) break;
            lv.append({ev.description.left(14), ev.date.left(10), cEvt});
        }
        push_left(QString("Events (%1/%2)").arg(lv.size()).arg(data.events.size()),
                  cEvt, std::move(lv));
    }

    // SUPPLY CHAIN → left
    if (filters.show_supply_chain && !data.supply_chain.isEmpty()) {
        QVector<LeafInfo> lv;
        for (const auto& sc : data.supply_chain) {
            if (lv.size() >= 6) break;
            lv.append({sc.name.left(14), sc.relationship.left(10), cSC});
        }
        push_left(QString("Supply (%1)").arg(lv.size()), cSC, std::move(lv));
    }

    // ── Banded placement ──────────────────────────────────────────────────────
    // band_height: the vertical space this cluster needs (hub + leaves, whichever taller)
    auto band_h = [&](const ClusterDef& c) -> qreal {
        int n = c.leaves.size();
        qreal col_h = n * kLeafH + (n - 1) * kLeafGap;
        return qMax(col_h, static_cast<qreal>(kHubH)) + kClusterGap;
    };

    // Total height of each side (subtract one trailing gap)
    auto total_h = [&](const QVector<ClusterDef>& side) -> qreal {
        qreal h = 0;
        for (const auto& c : side) h += band_h(c);
        if (!side.isEmpty()) h -= kClusterGap;
        return h;
    };

    // Place one side: hub at hub_cx, leaves at leaf_cx, stacked vertically
    auto place_side = [&](const QVector<ClusterDef>& side, qreal hub_cx, qreal leaf_cx) {
        qreal tot = total_h(side);
        qreal y   = -tot / 2.0; // top of first band

        for (const auto& c : side) {
            int   n   = c.leaves.size();
            qreal bh  = band_h(c);
            qreal bcy = y + (bh - kClusterGap) / 2.0; // band vertical center

            // Hub
            auto* hub = new RelNode(this, c.label, QString(), c.color,
                                    kHubW, kHubH, RelNode::Role::Hub);
            hub->setPos(hub_cx, bcy);
            addItem(hub);
            nodes_.append(hub);

            // Edge: center → hub
            auto* ce = new RelEdge(center_node, hub, c.color, false);
            addItem(ce);
            ce->adjust();
            center_node->add_edge(ce);
            hub->add_edge(ce);

            // Leaves — vertical column centered on bcy
            qreal col_h = n * kLeafH + (n - 1) * kLeafGap;
            qreal ly0   = bcy - col_h / 2.0; // top-left Y of first leaf

            for (int i = 0; i < n; ++i) {
                qreal lcy = ly0 + i * (kLeafH + kLeafGap) + kLeafH / 2.0;
                auto* leaf = new RelNode(this, c.leaves[i].label, c.leaves[i].sub,
                                         c.leaves[i].accent, kLeafW, kLeafH,
                                         RelNode::Role::Leaf);
                leaf->setPos(leaf_cx, lcy);
                addItem(leaf);
                nodes_.append(leaf);

                auto* le = new RelEdge(hub, leaf, c.color, true);
                addItem(le);
                le->adjust();
                hub->add_edge(le);
                leaf->add_edge(le);
            }

            y += bh; // advance to next band
        }
    };

    place_side(right_side, kHubX_R, kLeafX_R);
    place_side(left_side,  kHubX_L, kLeafX_L);

    // Fit scene rect tightly around content
    QRectF bounds = itemsBoundingRect().adjusted(-80, -80, 80, 80);
    setSceneRect(bounds);
}

// ── No-op stubs ───────────────────────────────────────────────────────────────
void RelationshipGraphScene::start_simulation() {}
void RelationshipGraphScene::simulation_step() {}
void RelationshipGraphScene::apply_layout(QVector<GraphNode>&, const QVector<GraphEdge>&, LayoutMode) {}
void RelationshipGraphScene::place_in_ring(const QVector<int>&, QVector<QPointF>&, double, double, const QPointF&) {}
void RelationshipGraphScene::add_node_item(const GraphNode&, const QPointF&) {}
void RelationshipGraphScene::add_edge_item(const QPointF&, const QPointF&, EdgeCategory, const QString&) {}

// ── View ──────────────────────────────────────────────────────────────────────
RelationshipGraphView::RelationshipGraphView(QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent) {
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setStyleSheet(QString("QGraphicsView { border: none; background: %1; }").arg(ui::colors::BG_BASE()));
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    horizontalScrollBar()->setStyleSheet(
        "QScrollBar:horizontal { height: 6px; background: #0a0a0a; }"
        "QScrollBar::handle:horizontal { background: #2a2a2a; min-width: 40px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }");
    verticalScrollBar()->setStyleSheet(
        "QScrollBar:vertical { width: 6px; background: #0a0a0a; }"
        "QScrollBar::handle:vertical { background: #2a2a2a; min-height: 40px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");
}

void RelationshipGraphView::fit_to_content() {
    if (!scene() || scene()->items().isEmpty()) return;
    fitInView(scene()->itemsBoundingRect().adjusted(-60,-60,60,60), Qt::KeepAspectRatio);
    current_zoom_ = transform().m11();
}

void RelationshipGraphView::reset_zoom() {
    resetTransform();
    current_zoom_ = 1.0;
    fit_to_content();
}

void RelationshipGraphView::wheelEvent(QWheelEvent* event) {
    double delta = event->pixelDelta().isNull()
                 ? event->angleDelta().y() / 120.0
                 : event->pixelDelta().y() / 120.0;
    if (qFuzzyIsNull(delta)) { event->ignore(); return; }
    double factor   = std::pow(1.10, delta);
    double new_zoom = current_zoom_ * factor;
    if (new_zoom < kMinZoom) factor = kMinZoom / current_zoom_;
    if (new_zoom > kMaxZoom) factor = kMaxZoom / current_zoom_;
    scale(factor, factor);
    current_zoom_ *= factor;
    event->accept();
}

void RelationshipGraphView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        setDragMode(QGraphicsView::ScrollHandDrag);
        QMouseEvent fake(event->type(), event->position(), event->scenePosition(),
                         event->globalPosition(), Qt::LeftButton, Qt::LeftButton,
                         event->modifiers());
        QGraphicsView::mousePressEvent(&fake);
    } else {
        QGraphicsView::mousePressEvent(event);
    }
}

void RelationshipGraphView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        QMouseEvent fake(event->type(), event->position(), event->scenePosition(),
                         event->globalPosition(), Qt::LeftButton, Qt::LeftButton,
                         event->modifiers());
        QGraphicsView::mouseReleaseEvent(&fake);
        setDragMode(QGraphicsView::ScrollHandDrag);
    } else {
        QGraphicsView::mouseReleaseEvent(event);
    }
}

void RelationshipGraphView::mouseMoveEvent(QMouseEvent* event) {
    QGraphicsView::mouseMoveEvent(event);
}

} // namespace fincept::relmap
