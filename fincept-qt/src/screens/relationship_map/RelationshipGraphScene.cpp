// src/screens/relationship_map/RelationshipGraphScene.cpp
#include "screens/relationship_map/RelationshipGraphScene.h"

#include "screens/relationship_map/RelationshipMapFormat.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/LoadingSkeleton.h"

#include <QElapsedTimer>
#include <QFontMetricsF>
#include <QFutureWatcher>
#include <QGraphicsItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>

namespace fincept::relmap {

namespace {

namespace colors = fincept::ui::colors;
namespace fonts = fincept::ui::fonts;
namespace fmt = fincept::ui::formatting;

constexpr double kRgsNaN = std::numeric_limits<double>::quiet_NaN();

bool rgs_ok(double v) {
    return std::isfinite(v);
}

QColor rgs_with_alpha(QColor c, double a) {
    c.setAlphaF(static_cast<float>(std::clamp(a, 0.0, 1.0)));
    return c;
}

/// A fixed categorical slot per group (markers only — the cards stay monochrome).
QColor rgs_group_color(RelGroup g) {
    switch (g) {
        case RelGroup::Institutions:
            return colors::chart_color(4);
        case RelGroup::Funds:
            return colors::chart_color(8);
        case RelGroup::Insiders:
            return colors::chart_color(2);
        case RelGroup::Management:
            return colors::chart_color(6);
        case RelGroup::Peers:
            return colors::chart_color(1);
        case RelGroup::Holdings:
            return colors::chart_color(1);
        case RelGroup::Sectors:
            return colors::chart_color(5);
        case RelGroup::Correlated:
            return colors::chart_color(9);
        case RelGroup::Subsidiaries:
            return colors::chart_color(7);
        case RelGroup::SupplyChain:
            return colors::chart_color(11);
        case RelGroup::Count:
            break;
    }
    return QColor(colors::TEXT_TERTIARY());
}

QString rgs_group_title(RelGroup g, bool fund) {
    switch (g) {
        case RelGroup::Institutions:
            return fund ? RelationshipGraphScene::tr("ISSUER") : RelationshipGraphScene::tr("INSTITUTIONAL HOLDERS");
        case RelGroup::Funds:
            return RelationshipGraphScene::tr("FUND HOLDERS");
        case RelGroup::Insiders:
            return RelationshipGraphScene::tr("INSIDERS & BOARD");
        case RelGroup::Management:
            return RelationshipGraphScene::tr("MANAGEMENT");
        case RelGroup::Peers:
            return RelationshipGraphScene::tr("PEERS");
        case RelGroup::Holdings:
            return RelationshipGraphScene::tr("TOP HOLDINGS");
        case RelGroup::Sectors:
            return RelationshipGraphScene::tr("SECTOR WEIGHTS");
        case RelGroup::Correlated:
            return RelationshipGraphScene::tr("CORRELATED");
        case RelGroup::Subsidiaries:
            return RelationshipGraphScene::tr("SUBSIDIARIES");
        case RelGroup::SupplyChain:
            return RelationshipGraphScene::tr("SUPPLY CHAIN");
        case RelGroup::Count:
            break;
    }
    return {};
}

/// Column captions of a group's cards: {c1, c2, c3}; an empty caption = no column.
QStringList rgs_columns(RelGroup g) {
    switch (g) {
        case RelGroup::Institutions:
        case RelGroup::Funds:
            return {RelationshipGraphScene::tr("% HELD"), RelationshipGraphScene::tr("VALUE"), RelationshipGraphScene::tr("CHG")};
        case RelGroup::Insiders:
            return {RelationshipGraphScene::tr("SHARES"), RelationshipGraphScene::tr("VALUE"), RelationshipGraphScene::tr("LAST")};
        case RelGroup::Management:
            return {RelationshipGraphScene::tr("TITLE"), QString(), RelationshipGraphScene::tr("HELD")};
        case RelGroup::Peers:
            return {RelationshipGraphScene::tr("MKT CAP"), RelationshipGraphScene::tr("ρ 1Y"), RelationshipGraphScene::tr("52W")};
        case RelGroup::Holdings:
            return {RelationshipGraphScene::tr("WEIGHT"), RelationshipGraphScene::tr("VALUE"), RelationshipGraphScene::tr("ρ 1Y")};
        case RelGroup::Sectors:
            return {RelationshipGraphScene::tr("WEIGHT"), RelationshipGraphScene::tr("VALUE"), QString()};
        case RelGroup::Correlated:
            return {RelationshipGraphScene::tr("ρ 1Y"), RelationshipGraphScene::tr("β"), RelationshipGraphScene::tr("1Y RET")};
        case RelGroup::Subsidiaries:
            return {QString(), QString(), RelationshipGraphScene::tr("JURISDICTION")};
        case RelGroup::SupplyChain:
        case RelGroup::Count:
            break;
    }
    return {QString(), QString(), QString()};
}

QString rgs_money(double v, const QString& ccy) {
    return rgs_ok(v) && v != 0 ? amount_text(v, ccy) : QString();
}
QString rgs_pct(double v, int dec = 2) {
    return rgs_ok(v) ? fmt::pct(v, dec, false) : QString();
}
QString rgs_rho(double v) {
    return rgs_ok(v) ? fmt::number(v, 2) : QString();
}
/// Signed percent with a direction arrow (direction is never colour alone).
QString rgs_change(double v, int dec = 1) {
    if (!rgs_ok(v))
        return {};
    const QString arrow = v > 0 ? QStringLiteral("▲") : (v < 0 ? QStringLiteral("▼") : QString());
    return arrow + fmt::pct(std::abs(v), dec, false);
}

QString rgs_tx_tag(const QString& kind) {
    const QString k = kind.trimmed().toLower();
    if (k == QLatin1String("buy"))
        return RelationshipGraphScene::tr("▲ BUY");
    if (k == QLatin1String("sell"))
        return RelationshipGraphScene::tr("▼ SELL");
    if (k.isEmpty())
        return {};
    if (k.contains(QLatin1String("gift")))
        return RelationshipGraphScene::tr("GIFT");
    if (k.contains(QLatin1String("grant")) || k.contains(QLatin1String("award")))
        return RelationshipGraphScene::tr("GRANT");
    if (k.contains(QLatin1String("option")) || k.contains(QLatin1String("exercise")) ||
        k.contains(QLatin1String("conversion")))
        return RelationshipGraphScene::tr("OPTION");
    return k.left(8).toUpper();
}

/// The texts of one node, formatted for its group.
struct RgsText {
    QString name;
    QString sub; ///< dim second text (title, jurisdiction)
    QString c[3];
    int dir[3] = {0, 0, 0}; ///< price-direction tint of a cell (-1 / 0 / +1)
    bool span12 = false;    ///< c[0] spans the first two columns (management title)
    bool span_all = false;  ///< c[2] spans all three (jurisdiction)
    QString tooltip;
    QString bubble_value; ///< one number under a bubble label
};

RgsText rgs_text_for(const RelGraphNode& n, const RelationshipData& d) {
    RgsText t;
    t.name = n.kind == RelNodeKind::Company && !n.short_label.isEmpty() && n.group == RelGroup::Peers
                 ? n.short_label + QStringLiteral("  ") + n.label
                 : n.label;
    t.sub = n.subtitle;
    QStringList tip{n.label};
    if (!n.subtitle.isEmpty() && n.subtitle != n.label)
        tip << n.subtitle;
    switch (n.group) {
        case RelGroup::Institutions:
        case RelGroup::Funds:
            t.c[0] = rgs_pct(n.pct);
            t.c[1] = rgs_money(n.value, n.value_currency);
            t.c[2] = rgs_change(n.change);
            t.dir[2] = rgs_ok(n.change) ? (n.change > 0 ? 1 : (n.change < 0 ? -1 : 0)) : 0;
            t.bubble_value = t.c[0];
            if (!n.date.isEmpty())
                tip << RelationshipGraphScene::tr("Reported %1").arg(n.date);
            if (n.kind == RelNodeKind::Institution && n.subtitle == QLatin1String("issuer")) {
                t.c[0].clear();
                t.c[2].clear();
                t.sub = RelationshipGraphScene::tr("issuer");
            }
            break;
        case RelGroup::Insiders:
        case RelGroup::Management:
            if (n.group == RelGroup::Management) {
                t.c[0] = n.subtitle;
                t.span12 = true;
            } else {
                t.c[0] = rgs_ok(n.shares) ? fmt::compact(n.shares) : QString();
                t.c[1] = rgs_money(n.value, n.value_currency);
            }
            t.c[2] = n.group == RelGroup::Management ? rgs_money(n.value, n.value_currency) : rgs_tx_tag(n.note);
            if (n.group == RelGroup::Insiders) {
                const QString k = n.note.toLower();
                t.dir[2] = k == QLatin1String("buy") ? 1 : (k == QLatin1String("sell") ? -1 : 0);
            }
            t.bubble_value = rgs_ok(n.shares) ? fmt::compact(n.shares) : QString();
            if (rgs_ok(n.shares))
                tip << RelationshipGraphScene::tr("%1 shares").arg(fmt::compact(n.shares));
            if (!n.note.isEmpty())
                tip << RelationshipGraphScene::tr("Last transaction: %1 %2").arg(n.note, n.date);
            break;
        case RelGroup::Peers:
            t.c[0] = rgs_money(n.size_value, n.size_currency);
            t.c[1] = rgs_rho(n.corr);
            t.c[2] = rgs_change(n.change);
            t.dir[2] = rgs_ok(n.change) ? (n.change > 0 ? 1 : (n.change < 0 ? -1 : 0)) : 0;
            t.bubble_value = t.c[0];
            break;
        case RelGroup::Holdings:
            t.c[0] = rgs_pct(n.pct);
            t.c[1] = rgs_money(n.value, n.value_currency);
            t.c[2] = rgs_rho(n.corr);
            t.bubble_value = t.c[0];
            break;
        case RelGroup::Sectors:
            t.c[0] = rgs_pct(n.pct, 1);
            t.c[1] = rgs_money(n.value, n.value_currency);
            t.bubble_value = t.c[0];
            break;
        case RelGroup::Correlated: {
            t.c[0] = rgs_rho(n.corr);
            const double beta = d.correlation.beta.value(n.symbol, kRgsNaN);
            t.c[1] = rgs_ok(beta) ? fmt::number(beta, 2) : QString();
            t.c[2] = rgs_change(n.change);
            t.dir[2] = rgs_ok(n.change) ? (n.change > 0 ? 1 : (n.change < 0 ? -1 : 0)) : 0;
            t.bubble_value = t.c[0].isEmpty() ? QString() : QStringLiteral("ρ ") + t.c[0];
            break;
        }
        case RelGroup::Subsidiaries:
            t.c[2] = n.subtitle;
            t.span_all = true;
            t.sub.clear();
            break;
        case RelGroup::SupplyChain:
            t.c[0] = rgs_pct(n.pct, 1);
            break;
        case RelGroup::Count:
            break;
    }
    if (n.kind == RelNodeKind::Cluster) {
        const QString what = rgs_group_title(n.group, d.is_fund()).toLower();
        if (n.hidden_count > 0)
            t.name = RelationshipGraphScene::tr("+ %1 more %2").arg(fmt::number(n.hidden_count, 0), what);
        else
            t.name = RelationshipGraphScene::tr("+ %1 more not listed by the source").arg(fmt::number(n.universe_count, 0));
        for (auto& c : t.c)
            c.clear();
        t.span12 = false;
        t.span_all = false;
        if (rgs_ok(n.hidden_pct) && n.hidden_pct > 0)
            t.c[0] = QStringLiteral("≈") + rgs_pct(n.hidden_pct, 1);
        if (rgs_ok(n.value) && n.group != RelGroup::Peers)
            t.c[1] = rgs_money(n.value, n.value_currency);
        t.tooltip = n.hidden_count > 0 ? RelationshipGraphScene::tr("Click to show all %1").arg(what)
                                       : RelationshipGraphScene::tr("%1 further holders exist; Yahoo lists the largest only")
                                             .arg(fmt::number(n.universe_count, 0));
        t.bubble_value.clear();
        return t;
    }
    if (n.user_holds)
        tip << RelationshipGraphScene::tr("In your portfolio");
    else if (n.in_watchlist)
        tip << RelationshipGraphScene::tr("In your watchlist");
    t.tooltip = tip.join(QLatin1Char('\n'));
    return t;
}

QString rgs_placeholder_text(const RelGraphNode& n) {
    switch (n.state) {
        case RelSectionState::Loading:
            return RelationshipGraphScene::tr("Loading…");
        case RelSectionState::Timeout:
            return RelationshipGraphScene::tr("Not loaded — the source did not answer in time · RETRY");
        case RelSectionState::Skipped:
            return RelationshipGraphScene::tr("Not loaded — time budget used up · RETRY");
        case RelSectionState::Error:
            return RelationshipGraphScene::tr("Not loaded — the source failed · RETRY");
        case RelSectionState::Unavailable:
            return n.state_note.isEmpty() ? RelationshipGraphScene::tr("Not available from free sources")
                                          : n.state_note;
        case RelSectionState::Empty:
            return n.state_note.isEmpty() ? RelationshipGraphScene::tr("None reported by the source") : n.state_note;
        default:
            break;
    }
    return n.state_note;
}

// ── Items ───────────────────────────────────────────────────────────────────

enum class RgsStyle { Card, Center, Bubble, CenterBubble, Bar, CenterBar };

/// Paints a node marker shape (type) in `r`.
void rgs_marker(QPainter* p, const QRectF& r, RelNodeKind kind, const QColor& c) {
    p->setPen(Qt::NoPen);
    p->setBrush(c);
    switch (kind) {
        case RelNodeKind::Institution: {
            QPolygonF d{QPointF(r.center().x(), r.top()), QPointF(r.right(), r.center().y()),
                        QPointF(r.center().x(), r.bottom()), QPointF(r.left(), r.center().y())};
            p->drawPolygon(d);
            break;
        }
        case RelNodeKind::Fund:
            p->setBrush(Qt::NoBrush);
            p->setPen(QPen(c, 1.6));
            p->drawRect(r.adjusted(0.8, 0.8, -0.8, -0.8));
            break;
        case RelNodeKind::Person:
            p->drawEllipse(r);
            break;
        case RelNodeKind::Etf: {
            QPolygonF tri{QPointF(r.center().x(), r.top()), QPointF(r.right(), r.bottom()), QPointF(r.left(), r.bottom())};
            p->drawPolygon(tri);
            break;
        }
        case RelNodeKind::Index:
            p->setBrush(Qt::NoBrush);
            p->setPen(QPen(c, 1.6));
            p->drawEllipse(r.adjusted(0.8, 0.8, -0.8, -0.8));
            p->setPen(Qt::NoPen);
            p->setBrush(c);
            p->drawEllipse(r.center(), r.width() * 0.18, r.height() * 0.18);
            break;
        case RelNodeKind::Sector:
            p->drawRect(QRectF(r.left(), r.center().y() - r.height() * 0.25, r.width(), r.height() * 0.5));
            break;
        case RelNodeKind::Entity:
            p->drawRoundedRect(r.adjusted(1, 1, -1, -1), 1.5, 1.5);
            break;
        case RelNodeKind::Company:
        case RelNodeKind::Center:
            p->drawRect(r);
            break;
        case RelNodeKind::Cluster:
        case RelNodeKind::Placeholder:
            p->setBrush(Qt::NoBrush);
            p->setPen(QPen(c, 1.2, Qt::DotLine));
            p->drawEllipse(r.adjusted(1, 1, -1, -1));
            break;
    }
}

/// The company card's own numbers (formatted once per layout / format change).
struct RgsCenterText {
    QString ticker, venue, name, price, change, line;
    int change_dir = 0;
    QStringList stat_labels, stat_values; ///< 6 cells
    QVector<QPair<QString, double>> split; ///< ownership / sector split: label -> fraction
    QString badge; ///< "IN PORTFOLIO" / "WATCHLIST"
};

RgsCenterText rgs_center_text(const RelationshipData& d, const RelGraphNode& c) {
    RgsCenterText t;
    const CompanyInfo& co = d.company;
    t.ticker = co.ticker;
    QStringList venue;
    if (!co.exchange.isEmpty())
        venue << co.exchange;
    if (!co.currency.isEmpty())
        venue << co.currency;
    t.venue = venue.join(QStringLiteral(" · "));
    t.name = co.name;
    t.price = co.current_price > 0 ? price_text(co.current_price, co.currency) : fmt::placeholder();
    if (co.day_change_pct != 0) {
        t.change = rgs_change(co.day_change_pct, 2);
        t.change_dir = co.day_change_pct > 0 ? 1 : -1;
    }
    auto stat = [&t](const QString& label, const QString& value) {
        t.stat_labels << label;
        t.stat_values << (value.isEmpty() ? fmt::placeholder() : value);
    };
    if (d.is_fund()) {
        stat(RelationshipGraphScene::tr("AUM"), rgs_money(d.fund.total_assets, co.currency));
        stat(RelationshipGraphScene::tr("HOLDINGS"), d.fund.holdings.isEmpty() ? QString() : QString::number(d.fund.holdings.size()));
        stat(RelationshipGraphScene::tr("YIELD"), co.dividend_yield > 0 ? fmt::pct(co.dividend_yield, 2, false) : QString());
        stat(RelationshipGraphScene::tr("52W"), d.technicals.week52_change_pct != 0 ? rgs_change(d.technicals.week52_change_pct * 100) : QString());
        stat(RelationshipGraphScene::tr("FAMILY"), d.fund.family);
        stat(RelationshipGraphScene::tr("CATEGORY"), d.fund.category);
        t.line = d.fund.legal_type;
        for (int i = 0; i < d.fund.sector_weights.size() && i < 4; ++i)
            t.split.append({d.fund.sector_weights[i].first, d.fund.sector_weights[i].second});
    } else {
        stat(RelationshipGraphScene::tr("MKT CAP"), rgs_money(co.market_cap, co.currency));
        stat(RelationshipGraphScene::tr("P/E"), co.pe_ratio > 0 ? fmt::ratio(co.pe_ratio, 1) : QString());
        stat(RelationshipGraphScene::tr("BETA"), d.technicals.beta > 0 ? fmt::number(d.technicals.beta, 2) : QString());
        stat(RelationshipGraphScene::tr("DIV YLD"), co.dividend_yield > 0 ? fmt::pct(co.dividend_yield, 2, false) : QString());
        stat(RelationshipGraphScene::tr("52W"), d.technicals.week52_change_pct != 0 ? rgs_change(d.technicals.week52_change_pct * 100) : QString());
        stat(RelationshipGraphScene::tr("FLOAT"), co.float_shares > 0 ? fmt::compact(co.float_shares) : QString());
        QStringList where;
        if (!co.sector.isEmpty())
            where << co.sector;
        if (!co.industry.isEmpty())
            where << co.industry;
        t.line = where.join(QStringLiteral(" · "));
        if (d.major_holders.present) {
            const double inst = std::clamp(d.major_holders.institutions_pct, 0.0, 1.0);
            const double ins = std::clamp(d.major_holders.insiders_pct, 0.0, 1.0 - inst);
            t.split.append({RelationshipGraphScene::tr("INST"), inst});
            t.split.append({RelationshipGraphScene::tr("INSIDERS"), ins});
            t.split.append({RelationshipGraphScene::tr("OTHER"), std::max(0.0, 1.0 - inst - ins)});
        }
    }
    if (c.user_holds)
        t.badge = RelationshipGraphScene::tr("IN PORTFOLIO");
    else if (c.in_watchlist)
        t.badge = RelationshipGraphScene::tr("WATCHLIST");
    return t;
}

class RgsNodeItem : public QGraphicsItem {
  public:
    enum { Type = QGraphicsItem::UserType + 0x5231 };
    int type() const override { return Type; }

    RgsNodeItem(const RelGraphNode& n, RgsStyle style) : node_(n), style_(style) {
        setAcceptHoverEvents(false);
        setAcceptedMouseButtons(Qt::NoButton); // the scene routes clicks
        setZValue(style == RgsStyle::Center || style == RgsStyle::CenterBubble || style == RgsStyle::CenterBar ? 5 : 3);
    }

    const RelGraphNode& node() const { return node_; }
    void set_node(const RelGraphNode& n) { node_ = n; }
    RgsStyle style() const { return style_; }
    void set_style(RgsStyle s) {
        prepareGeometryChange();
        style_ = s;
    }
    /// Geometry relative to the item position (the node centre).
    void set_geometry(const RelNodeGeom& g, const QStringList& columns, double row_h, double col_w) {
        prepareGeometryChange();
        rect_ = g.rect.translated(-g.center);
        label_ = g.label_visible && !g.label.isEmpty() ? g.label.translated(-g.center) : QRectF();
        radius_ = g.radius;
        columns_ = columns;
        row_h_ = row_h;
        col_w_ = col_w;
    }
    void set_text(const RgsText& t) {
        text_ = t;
        setToolTip(t.tooltip);
    }
    void set_center_text(const RgsCenterText& t) { center_ = t; }
    void set_flags(bool selected, bool dimmed, bool hovered) {
        if (selected == selected_ && dimmed == dimmed_ && hovered == hovered_)
            return;
        selected_ = selected;
        dimmed_ = dimmed;
        hovered_ = hovered;
        update();
    }
    void set_pulse(double p) {
        pulse_ = p;
        update();
    }
    void set_motion(const QPointF& from, const QPointF& to, bool fade_in) {
        from_ = from;
        to_ = to;
        fade_in_ = fade_in;
    }
    void apply_progress(double t) {
        setPos(from_ + (to_ - from_) * t);
        if (fade_in_)
            setOpacity(t);
    }
    void finish_motion() {
        setPos(to_);
        setOpacity(1.0);
        fade_in_ = false;
    }

    QRectF boundingRect() const override {
        QRectF r = rect_.united(label_);
        const double grow = (style_ == RgsStyle::Bubble || style_ == RgsStyle::CenterBubble) ? 10.0 : 6.0;
        return r.adjusted(-grow, -grow, grow, grow);
    }
    QPainterPath shape() const override {
        QPainterPath p;
        if (style_ == RgsStyle::Bubble || style_ == RgsStyle::CenterBubble)
            p.addEllipse(rect_);
        else
            p.addRect(rect_);
        if (!label_.isEmpty())
            p.addRect(label_);
        return p;
    }

    void paint(QPainter* p, const QStyleOptionGraphicsItem* opt, QWidget*) override {
        const double lod = opt ? QStyleOptionGraphicsItem::levelOfDetailFromTransform(p->worldTransform()) : 1.0;
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setRenderHint(QPainter::TextAntialiasing, true);
        p->setOpacity(p->opacity() * (dimmed_ ? 0.26 : 1.0));
        switch (style_) {
            case RgsStyle::Card:
                paint_card(p, lod);
                break;
            case RgsStyle::Center:
                paint_center(p, lod);
                break;
            case RgsStyle::Bubble:
            case RgsStyle::CenterBubble:
                paint_bubble(p, lod);
                break;
            case RgsStyle::Bar:
            case RgsStyle::CenterBar:
                paint_bar(p, lod);
                break;
        }
        if (pulse_ > 0) {
            const QColor acc(colors::AMBER());
            for (int i = 0; i < 2; ++i) {
                const double ph = std::fmod(pulse_ + i * 0.5, 1.0);
                p->setPen(QPen(rgs_with_alpha(acc, (1.0 - ph) * 0.9), 2.0));
                p->setBrush(Qt::NoBrush);
                const double g = 4 + ph * 22;
                if (style_ == RgsStyle::Bubble || style_ == RgsStyle::CenterBubble)
                    p->drawEllipse(rect_.adjusted(-g, -g, g, g));
                else
                    p->drawRect(rect_.adjusted(-g, -g, g, g));
            }
        }
    }

  private:
    void paint_card(QPainter* p, double lod) {
        const QRectF r = rect_;
        const bool placeholder = node_.kind == RelNodeKind::Placeholder;
        const bool cluster = node_.kind == RelNodeKind::Cluster;
        // Surface: monochrome; hover lifts it, the selection takes the accent.
        QColor bg = QColor(colors::BG_SURFACE());
        if (hovered_)
            bg = QColor(colors::BG_HOVER());
        p->setPen(Qt::NoPen);
        p->setBrush(placeholder || cluster ? Qt::NoBrush : QBrush(bg));
        p->drawRect(r);
        const QColor border = selected_ ? QColor(colors::AMBER()) : QColor(colors::BORDER_DIM());
        p->setPen(QPen(border, selected_ ? 1.5 : 1.0, placeholder || cluster ? Qt::DashLine : Qt::SolidLine));
        p->setBrush(Qt::NoBrush);
        p->drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
        if (selected_) {
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(colors::AMBER()));
            p->drawRect(QRectF(r.left(), r.top(), 3, r.height()));
        }
        if (lod < 0.42)
            return; // far out: the card's outline only

        const QFont name_font = fonts::font(-2, QFont::DemiBold);
        const QFont cell_font = fonts::font(-2);
        const QFont tag_font = fonts::font(-3, QFont::Bold);
        const double pad = std::max(6.0, row_h_ * 0.28);
        if (placeholder) {
            const bool loading = node_.state == RelSectionState::Loading;
            if (loading) {
                // Skeleton rows: the size the section will take.
                const int rows = std::max(1, static_cast<int>(std::round((r.height() + 3) / (row_h_ + 3))));
                const QColor sk = colors::overlay_color(0.07);
                for (int i = 0; i < rows; ++i) {
                    const QRectF row(r.left(), r.top() + i * (row_h_ + 3), r.width(), row_h_);
                    p->setPen(Qt::NoPen);
                    p->setBrush(sk);
                    p->drawRect(QRectF(row.left() + pad, row.center().y() - 3, row.width() * (0.45 - 0.05 * (i % 3)), 6));
                    p->drawRect(QRectF(row.right() - pad - col_w_ * 0.8, row.center().y() - 3, col_w_ * 0.8, 6));
                }
            }
            p->setFont(fonts::font(-3, QFont::DemiBold));
            const bool failed = section_failed(node_.state);
            p->setPen(failed ? QColor(colors::WARNING()) : QColor(colors::TEXT_TERTIARY()));
            const QRectF tr = loading ? QRectF(r.left() + pad, r.top(), r.width() - 2 * pad, row_h_)
                                      : r.adjusted(pad, 0, -pad, 0);
            const Qt::Alignment al = loading ? (Qt::AlignRight | Qt::AlignVCenter) : (Qt::AlignLeft | Qt::AlignVCenter);
            p->drawText(tr, al, p->fontMetrics().elidedText(rgs_placeholder_text(node_), Qt::ElideRight, int(tr.width())));
            return;
        }

        // Marker (type by shape), name, then up to three right-aligned number columns.
        const double ms = std::max(6.0, row_h_ * 0.32);
        const QRectF marker(r.left() + pad, r.center().y() - ms / 2, ms, ms);
        rgs_marker(p, marker, node_.kind, cluster ? QColor(colors::TEXT_DIM()) : rgs_group_color(node_.group));
        const double cols_w = 3 * col_w_;
        const double name_left = marker.right() + pad * 0.8;
        double name_right = r.right() - pad - cols_w;
        if (text_.c[0].isEmpty() && text_.c[1].isEmpty() && text_.c[2].isEmpty())
            name_right = r.right() - pad;
        if (text_.span_all)
            name_right = r.right() - pad - col_w_ * 1.6;
        QString name = text_.name;
        p->setFont(name_font);
        p->setPen(cluster ? QColor(colors::TEXT_TERTIARY()) : QColor(colors::TEXT_PRIMARY()));
        QRectF nr(name_left, r.top(), std::max(10.0, name_right - name_left), r.height());
        // "You hold this": a small accent tag before the name.
        if (node_.user_holds || node_.in_watchlist) {
            const QString tag = node_.user_holds ? RelationshipGraphScene::tr("HELD") : RelationshipGraphScene::tr("WL");
            p->setFont(tag_font);
            const double tw = p->fontMetrics().horizontalAdvance(tag) + 6;
            const QRectF tr2(nr.right() - tw, r.center().y() - row_h_ * 0.3, tw, row_h_ * 0.6);
            p->setPen(QPen(QColor(colors::AMBER()), 1.0));
            p->setBrush(Qt::NoBrush);
            p->drawRect(tr2);
            p->drawText(tr2, Qt::AlignCenter, tag);
            nr.setRight(tr2.left() - 4);
            p->setFont(name_font);
            p->setPen(QColor(colors::TEXT_PRIMARY()));
        }
        p->drawText(nr, Qt::AlignLeft | Qt::AlignVCenter, p->fontMetrics().elidedText(name, Qt::ElideRight, int(nr.width())));

        p->setFont(cell_font);
        const QFontMetricsF cfm(cell_font);
        auto cell_color = [&](int i) {
            if (text_.dir[i] > 0)
                return QColor(colors::POSITIVE());
            if (text_.dir[i] < 0)
                return QColor(colors::NEGATIVE());
            return i == 0 ? QColor(colors::TEXT_PRIMARY()) : QColor(colors::TEXT_SECONDARY());
        };
        const double right = r.right() - pad;
        if (text_.span_all) {
            const QRectF cr(name_right + 4, r.top(), right - name_right - 4, r.height());
            p->setPen(QColor(colors::TEXT_SECONDARY()));
            p->drawText(cr, Qt::AlignRight | Qt::AlignVCenter, cfm.elidedText(text_.c[2], Qt::ElideRight, cr.width()));
        } else {
            for (int i = 0; i < 3; ++i) {
                if (text_.c[i].isEmpty())
                    continue;
                double x0 = right - (3 - i) * col_w_;
                double w = col_w_ - 4;
                Qt::Alignment al = Qt::AlignRight | Qt::AlignVCenter;
                if (i == 0 && text_.span12) {
                    w = 2 * col_w_ - 6;
                    al = Qt::AlignLeft | Qt::AlignVCenter;
                    p->setPen(QColor(colors::TEXT_SECONDARY()));
                } else {
                    p->setPen(cell_color(i));
                }
                const QRectF cr(x0, r.top(), w, r.height());
                p->drawText(cr, al, cfm.elidedText(text_.c[i], Qt::ElideRight, cr.width()));
            }
        }
    }

    void paint_center(QPainter* p, double lod) {
        const QRectF r = rect_;
        const QColor acc(colors::AMBER());
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(colors::BG_RAISED()));
        p->drawRect(r);
        p->setPen(QPen(selected_ ? acc : QColor(colors::BORDER_MED()), selected_ ? 1.6 : 1.0));
        p->setBrush(Qt::NoBrush);
        p->drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
        p->setPen(Qt::NoPen);
        p->setBrush(acc);
        p->drawRect(QRectF(r.left(), r.top(), r.width(), 3));
        if (lod < 0.3)
            return;
        const double pad = std::max(10.0, row_h_ * 0.5);
        const double inner_w = r.width() - 2 * pad;
        double y = r.top() + pad * 0.6;
        // Ticker + venue.
        QFont tf = fonts::font(5, QFont::Bold);
        p->setFont(tf);
        QFontMetricsF tfm(tf);
        p->setPen(acc);
        p->drawText(QRectF(r.left() + pad, y, inner_w, tfm.height()), Qt::AlignLeft | Qt::AlignVCenter, center_.ticker);
        QFont vf = fonts::font(-3, QFont::DemiBold);
        p->setFont(vf);
        p->setPen(QColor(colors::TEXT_TERTIARY()));
        QString right_txt = center_.venue;
        p->drawText(QRectF(r.left() + pad, y, inner_w, tfm.height()), Qt::AlignRight | Qt::AlignVCenter, right_txt);
        if (!center_.badge.isEmpty()) {
            const QFontMetricsF vfm(vf);
            const double bw = vfm.horizontalAdvance(center_.badge) + 8;
            const QRectF br(r.right() - pad - bw, y + tfm.height() * 0.62, bw, vfm.height() + 2);
            p->setPen(QPen(acc, 1.0));
            p->drawRect(br);
            p->setPen(acc);
            p->drawText(br, Qt::AlignCenter, center_.badge);
        }
        y += tfm.height();
        // Name.
        QFont nf = fonts::font(-1);
        p->setFont(nf);
        QFontMetricsF nfm(nf);
        p->setPen(QColor(colors::TEXT_SECONDARY()));
        p->drawText(QRectF(r.left() + pad, y, inner_w, nfm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    nfm.elidedText(center_.name, Qt::ElideRight, inner_w));
        y += nfm.height() + 4;
        // Price + change.
        QFont pf = fonts::font(3, QFont::Bold);
        p->setFont(pf);
        QFontMetricsF pfm(pf);
        p->setPen(QColor(colors::TEXT_PRIMARY()));
        p->drawText(QRectF(r.left() + pad, y, inner_w, pfm.height()), Qt::AlignLeft | Qt::AlignVCenter, center_.price);
        if (!center_.change.isEmpty()) {
            const double pw = pfm.horizontalAdvance(center_.price);
            QFont cf = fonts::font(-1, QFont::DemiBold);
            p->setFont(cf);
            p->setPen(center_.change_dir > 0 ? QColor(colors::POSITIVE()) : QColor(colors::NEGATIVE()));
            p->drawText(QRectF(r.left() + pad + pw + 10, y, inner_w - pw - 10, pfm.height()),
                        Qt::AlignLeft | Qt::AlignVCenter, center_.change);
        }
        y += pfm.height() + 6;
        // Hairline, then the 3 x 2 stat grid.
        p->setPen(QPen(QColor(colors::BORDER_DIM()), 1.0));
        p->drawLine(QPointF(r.left() + pad, y), QPointF(r.right() - pad, y));
        y += 6;
        QFont lf = fonts::font(-4, QFont::DemiBold);
        QFont sf = fonts::font(-2, QFont::DemiBold);
        QFontMetricsF lfm(lf), sfm(sf);
        const double cell_w = inner_w / 3.0;
        const double cell_h = lfm.height() + sfm.height() + 4;
        for (int i = 0; i < center_.stat_labels.size() && i < 6; ++i) {
            const double cx = r.left() + pad + (i % 3) * cell_w;
            const double cy = y + (i / 3) * cell_h;
            p->setFont(lf);
            p->setPen(QColor(colors::TEXT_TERTIARY()));
            p->drawText(QRectF(cx, cy, cell_w - 6, lfm.height()), Qt::AlignLeft | Qt::AlignVCenter, center_.stat_labels[i]);
            p->setFont(sf);
            p->setPen(QColor(colors::TEXT_PRIMARY()));
            p->drawText(QRectF(cx, cy + lfm.height(), cell_w - 6, sfm.height() + 2), Qt::AlignLeft | Qt::AlignVCenter,
                        sfm.elidedText(center_.stat_values.value(i), Qt::ElideRight, cell_w - 6));
        }
        y += 2 * cell_h + 4;
        // Sector · industry.
        p->setFont(fonts::font(-3));
        p->setPen(QColor(colors::TEXT_TERTIARY()));
        const QFontMetricsF ifm(fonts::font(-3));
        p->drawText(QRectF(r.left() + pad, y, inner_w, ifm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    ifm.elidedText(center_.line, Qt::ElideRight, inner_w));
        y += ifm.height() + 6;
        // Ownership (or sector) split: one stacked bar with its labels under it.
        if (!center_.split.isEmpty() && y + 10 < r.bottom()) {
            const double bh = std::max(5.0, row_h_ * 0.28);
            double x = r.left() + pad;
            const QColor seg[] = {QColor(colors::TEXT_SECONDARY()), acc, QColor(colors::BORDER_BRIGHT()),
                                  QColor(colors::TEXT_DIM())};
            double total = 0;
            for (const auto& s : center_.split)
                total += std::max(0.0, s.second);
            if (total <= 0)
                total = 1;
            const double scale = std::min(1.0, 1.0 / total);
            QFont kf = fonts::font(-4, QFont::DemiBold);
            const QFontMetricsF kfm(kf);
            double lx = r.left() + pad;
            for (int i = 0; i < center_.split.size(); ++i) {
                const double w = inner_w * std::max(0.0, center_.split[i].second) * scale;
                p->setPen(Qt::NoPen);
                p->setBrush(seg[i % 4]);
                p->drawRect(QRectF(x, y, std::max(0.0, w - 1), bh));
                x += w;
                p->setFont(kf);
                p->setPen(i == 1 ? acc : QColor(colors::TEXT_SECONDARY()));
                const QString lab = center_.split[i].first + QLatin1Char(' ') +
                                    fmt::fraction_pct(center_.split[i].second, 1, false);
                const double lw = kfm.horizontalAdvance(lab) + 10;
                if (lx + lw <= r.right() - pad + 1) {
                    p->drawText(QRectF(lx, y + bh + 2, lw, kfm.height()), Qt::AlignLeft | Qt::AlignVCenter, lab);
                    lx += lw;
                }
            }
        }
    }

    void paint_bubble(QPainter* p, double lod) {
        const bool center = style_ == RgsStyle::CenterBubble;
        const QColor base = center ? QColor(colors::AMBER()) : rgs_group_color(node_.group);
        const QRectF r = rect_;
        p->setBrush(colors::tint_color(base, center ? 0.28 : 0.18));
        p->setPen(QPen(selected_ ? QColor(colors::AMBER()) : rgs_with_alpha(base, hovered_ ? 1.0 : 0.85),
                       selected_ ? 2.2 : 1.2,
                       node_.kind == RelNodeKind::Cluster ? Qt::DashLine : Qt::SolidLine));
        switch (node_.kind) {
            case RelNodeKind::Institution: {
                QPolygonF d{QPointF(0, r.top()), QPointF(r.right(), 0), QPointF(0, r.bottom()), QPointF(r.left(), 0)};
                p->drawPolygon(d);
                break;
            }
            case RelNodeKind::Fund:
            case RelNodeKind::Sector:
                p->drawRoundedRect(r.adjusted(r.width() * 0.08, r.height() * 0.08, -r.width() * 0.08, -r.height() * 0.08),
                                   3, 3);
                break;
            case RelNodeKind::Etf: {
                QPolygonF tri{QPointF(0, r.top()), QPointF(r.right(), r.bottom() * 0.8), QPointF(r.left(), r.bottom() * 0.8)};
                p->drawPolygon(tri);
                break;
            }
            default:
                p->drawEllipse(r);
                break;
        }
        if (center || node_.kind == RelNodeKind::Cluster) {
            p->setFont(fonts::font(center ? 2 : -3, QFont::Bold));
            p->setPen(center ? QColor(colors::AMBER()) : QColor(colors::TEXT_SECONDARY()));
            const QString t = center ? center_.ticker
                                     : (node_.hidden_count > 0 ? QStringLiteral("+") + QString::number(node_.hidden_count)
                                                               : QStringLiteral("+"));
            p->drawText(r, Qt::AlignCenter, t);
            if (center && lod > 0.5) {
                p->setFont(fonts::font(-3));
                p->setPen(QColor(colors::TEXT_SECONDARY()));
                p->drawText(r.adjusted(0, r.height() * 0.42, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
                            center_.stat_values.value(0));
            }
        }
        if (label_.isEmpty() || lod < 0.5)
            return;
        p->setFont(fonts::font(-3, QFont::DemiBold));
        p->setPen(QColor(colors::TEXT_PRIMARY()));
        const QFontMetricsF fm(p->font());
        const QString name = node_.short_label.isEmpty() ? node_.label : node_.short_label;
        const bool right_side = label_.left() >= r.right() - 1;
        const Qt::Alignment al = (right_side ? Qt::AlignLeft : (label_.right() <= r.left() + 1 ? Qt::AlignRight : Qt::AlignHCenter)) |
                                 Qt::AlignVCenter;
        p->drawText(label_, al, fm.elidedText(name, Qt::ElideRight, label_.width()));
    }

    void paint_bar(QPainter* p, double lod) {
        const bool center = style_ == RgsStyle::CenterBar;
        const QRectF r = rect_;
        const QColor c = center ? QColor(colors::AMBER()) : rgs_group_color(node_.group);
        p->setPen(Qt::NoPen);
        p->setBrush(center ? rgs_with_alpha(c, 0.85) : rgs_with_alpha(c, selected_ || hovered_ ? 1.0 : 0.8));
        p->drawRect(r);
        if (lod < 0.4)
            return;
        p->setFont(fonts::font(-3, QFont::DemiBold));
        const QFontMetricsF fm(p->font());
        if (center) {
            p->setPen(QColor(colors::AMBER()));
            p->drawText(QRectF(r.center().x() - 120, r.top() - fm.height() * 2.2, 240, fm.height()),
                        Qt::AlignCenter, center_.ticker);
            p->setPen(QColor(colors::TEXT_SECONDARY()));
            p->drawText(QRectF(r.center().x() - 160, r.top() - fm.height() * 1.2, 320, fm.height()), Qt::AlignCenter,
                        RelationshipGraphScene::tr("100 % of shares"));
            return;
        }
        const double gap = 8;
        const QString label = text_.name + QStringLiteral("  ") + text_.c[0];
        const bool left_side = r.center().x() < 0;
        const double w = 280;
        const QRectF lr = left_side ? QRectF(r.left() - gap - w, r.center().y() - fm.height() / 2, w, fm.height())
                                    : QRectF(r.right() + gap, r.center().y() - fm.height() / 2, w, fm.height());
        p->setPen(selected_ ? QColor(colors::AMBER()) : QColor(colors::TEXT_PRIMARY()));
        p->drawText(lr, (left_side ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter,
                    fm.elidedText(label, Qt::ElideLeft, w));
    }

    RelGraphNode node_;
    RgsStyle style_;
    QRectF rect_;
    QRectF label_;
    double radius_ = 0;
    QStringList columns_;
    double row_h_ = 24;
    double col_w_ = 60;
    RgsText text_;
    RgsCenterText center_;
    bool selected_ = false;
    bool dimmed_ = false;
    bool hovered_ = false;
    double pulse_ = 0;
    QPointF from_, to_;
    bool fade_in_ = false;
};

class RgsEdgeItem : public QGraphicsItem {
  public:
    RgsEdgeItem(const RelGraphEdge& e, const RelEdgeGeom& g, double weight, bool arrow_at_end, bool arrow, bool dashed)
        : edge_(e), weight_(weight), arrow_end_(arrow_at_end), arrow_(arrow), dashed_(dashed) {
        setZValue(e.second_hop ? 0 : 1);
        setAcceptedMouseButtons(Qt::NoButton);
        set_geometry(g);
    }
    const RelGraphEdge& edge() const { return edge_; }
    void set_geometry(const RelEdgeGeom& g) {
        prepareGeometryChange();
        geom_ = g;
        path_ = QPainterPath();
        if (!g.poly.isEmpty()) {
            path_.moveTo(g.poly.first());
            for (int i = 1; i < g.poly.size(); ++i)
                path_.lineTo(g.poly[i]);
        } else {
            path_.moveTo(g.p0);
            path_.cubicTo(g.c1, g.c2, g.p3);
        }
        if (g.band > 0) {
            // A band of constant thickness: the curve offset up and down.
            const double h = g.band / 2.0;
            band_ = QPainterPath();
            band_.moveTo(g.p0 + QPointF(0, -h));
            band_.cubicTo(g.c1 + QPointF(0, -h), g.c2 + QPointF(0, -h), g.p3 + QPointF(0, -h));
            band_.lineTo(g.p3 + QPointF(0, h));
            band_.cubicTo(g.c2 + QPointF(0, h), g.c1 + QPointF(0, h), g.p0 + QPointF(0, h));
            band_.closeSubpath();
        }
        bounds_ = (g.band > 0 ? band_.boundingRect() : path_.boundingRect()).adjusted(-12, -12, 12, 12);
        if (!label_.isEmpty())
            bounds_ = bounds_.united(label_rect());
    }
    void set_label(const QString& text) {
        prepareGeometryChange();
        label_ = text;
        if (!label_.isEmpty())
            bounds_ = bounds_.united(label_rect());
    }
    void set_emphasis(int e) { // -1 dimmed, 0 normal, 1 highlighted
        if (e == emphasis_)
            return;
        emphasis_ = e;
        update();
    }
    void set_weight(double w) {
        weight_ = w;
        update();
    }
    QRectF boundingRect() const override { return bounds_; }
    void paint(QPainter* p, const QStyleOptionGraphicsItem* opt, QWidget*) override {
        const double lod = opt ? QStyleOptionGraphicsItem::levelOfDetailFromTransform(p->worldTransform()) : 1.0;
        if (lod < 0.35 && weight_ < 0.25 && emphasis_ <= 0 && geom_.band <= 0)
            return; // thin edges drop out when far out
        p->setRenderHint(QPainter::Antialiasing, true);
        const QColor acc(colors::AMBER());
        const QColor ink(colors::TEXT_TERTIARY());
        double alpha = 0.22 + 0.55 * weight_;
        if (emphasis_ < 0)
            alpha *= 0.25;
        QColor c = emphasis_ > 0 ? acc : ink;
        if (edge_.second_hop)
            c = emphasis_ > 0 ? acc : QColor(colors::CYAN());
        if (geom_.band > 0) {
            const QColor bc = rgs_group_color(edge_.group);
            p->setPen(Qt::NoPen);
            p->setBrush(rgs_with_alpha(emphasis_ > 0 ? acc : bc, emphasis_ < 0 ? 0.08 : (emphasis_ > 0 ? 0.55 : 0.32)));
            p->drawPath(band_);
            return;
        }
        const double w = edge_.second_hop ? 1.0 : 0.8 + 3.4 * weight_;
        QPen pen(rgs_with_alpha(c, emphasis_ > 0 ? 0.95 : alpha), w);
        pen.setCapStyle(Qt::FlatCap);
        if (dashed_ || edge_.second_hop)
            pen.setDashPattern({3.0, 3.0});
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        p->drawPath(path_);
        if (arrow_ && lod > 0.45) {
            // Arrowhead at the end the relationship points to.
            const double len = path_.length();
            const double t_end = arrow_end_ ? 1.0 : 0.0;
            const double t_back = arrow_end_ ? std::max(0.0, 1.0 - 9.0 / std::max(1.0, len)) : std::min(1.0, 9.0 / std::max(1.0, len));
            const QPointF tip = path_.pointAtPercent(path_.percentAtLength(t_end * len));
            const QPointF back = path_.pointAtPercent(path_.percentAtLength(t_back * len));
            QPointF d = tip - back;
            const double dl = std::hypot(d.x(), d.y());
            if (dl > 0.1) {
                d /= dl;
                const QPointF nrm(-d.y(), d.x());
                const double s = 3.5 + w;
                QPolygonF head{tip, tip - d * (s * 1.6) + nrm * s, tip - d * (s * 1.6) - nrm * s};
                p->setPen(Qt::NoPen);
                p->setBrush(rgs_with_alpha(c, emphasis_ > 0 ? 0.95 : std::min(1.0, alpha + 0.2)));
                p->drawPolygon(head);
            }
        }
        if (!label_.isEmpty() && lod > 0.55 && emphasis_ >= 0) {
            const QRectF lr = label_rect();
            p->setPen(QPen(emphasis_ > 0 ? acc : QColor(colors::BORDER_MED()), 1.0));
            p->setBrush(QColor(colors::BG_BASE()));
            p->drawRoundedRect(lr, 2, 2);
            p->setFont(fonts::font(-4, QFont::DemiBold));
            p->setPen(emphasis_ > 0 ? acc : QColor(colors::TEXT_SECONDARY()));
            p->drawText(lr, Qt::AlignCenter, label_);
        }
    }

  private:
    QRectF label_rect() const {
        const QFontMetricsF fm(fonts::font(-4, QFont::DemiBold));
        const double w = fm.horizontalAdvance(label_) + 10;
        const double h = fm.height() + 2;
        return QRectF(geom_.label_pos.x() - w / 2, geom_.label_pos.y() - h / 2, w, h);
    }
    RelGraphEdge edge_;
    RelEdgeGeom geom_;
    QPainterPath path_;
    QPainterPath band_;
    QRectF bounds_;
    QString label_;
    double weight_ = 0.3;
    bool arrow_end_ = true;
    bool arrow_ = false;
    bool dashed_ = false;
    int emphasis_ = 0;
};

/// Group header (title, summary, column captions) or a side caption.
class RgsChromeItem : public QGraphicsItem {
  public:
    RgsChromeItem(const QRectF& r, const QString& title, const QString& summary, const QStringList& columns,
                  double col_w, bool side_caption, const QColor& tick)
        : rect_(r), title_(title), summary_(summary), columns_(columns), col_w_(col_w), side_(side_caption),
          tick_(tick) {
        setZValue(2);
        setAcceptedMouseButtons(Qt::NoButton);
    }
    QRectF boundingRect() const override { return rect_.adjusted(-2, -2, 2, 2); }
    void paint(QPainter* p, const QStyleOptionGraphicsItem* opt, QWidget*) override {
        const double lod = opt ? QStyleOptionGraphicsItem::levelOfDetailFromTransform(p->worldTransform()) : 1.0;
        p->setRenderHint(QPainter::Antialiasing, false);
        p->setRenderHint(QPainter::TextAntialiasing, true);
        if (side_) {
            p->setPen(QPen(QColor(colors::BORDER_MED()), 1.0));
            p->drawLine(QPointF(rect_.left(), rect_.bottom() - 0.5), QPointF(rect_.right(), rect_.bottom() - 0.5));
            if (lod < 0.3)
                return;
            p->setFont(fonts::font(-2, QFont::Bold));
            p->setPen(QColor(colors::TEXT_SECONDARY()));
            p->drawText(rect_.adjusted(0, 0, 0, -3), Qt::AlignLeft | Qt::AlignBottom, title_);
            return;
        }
        // Header strip: accent-free; a coloured tick says which marker the group uses.
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(colors::BG_BASE()));
        p->drawRect(rect_);
        p->setPen(QPen(QColor(colors::BORDER_MED()), 1.0));
        p->drawLine(QPointF(rect_.left(), rect_.bottom() - 0.5), QPointF(rect_.right(), rect_.bottom() - 0.5));
        p->setPen(Qt::NoPen);
        p->setBrush(tick_);
        const double th = std::max(8.0, rect_.height() * 0.38);
        p->drawRect(QRectF(rect_.left(), rect_.top() + rect_.height() * 0.18, 3, th));
        if (lod < 0.4)
            return;
        const QFont tf = fonts::font(-3, QFont::Bold);
        const QFontMetricsF tfm(tf);
        const double top_h = rect_.height() * 0.56;
        p->setFont(tf);
        p->setPen(QColor(colors::TEXT_PRIMARY()));
        const QRectF title_r(rect_.left() + 9, rect_.top(), rect_.width() - 9, top_h);
        p->drawText(title_r, Qt::AlignLeft | Qt::AlignVCenter, title_);
        const double tw = tfm.horizontalAdvance(title_) + 16;
        p->setFont(fonts::font(-4));
        p->setPen(QColor(colors::TEXT_TERTIARY()));
        const QRectF sum_r(rect_.left() + 9 + tw, rect_.top(), rect_.width() - 9 - tw - 4, top_h);
        p->drawText(sum_r, Qt::AlignRight | Qt::AlignVCenter,
                    QFontMetricsF(p->font()).elidedText(summary_, Qt::ElideRight, sum_r.width()));
        // Column captions, right-aligned over the card columns.
        p->setFont(fonts::font(-5, QFont::DemiBold));
        p->setPen(QColor(colors::TEXT_DIM()));
        const double right = rect_.right() - std::max(6.0, rect_.height() * 0.2);
        for (int i = 0; i < columns_.size() && i < 3; ++i) {
            if (columns_[i].isEmpty())
                continue;
            const double x0 = right - (3 - i) * col_w_;
            const QRectF cr(x0, rect_.top() + top_h - 2, col_w_ - 4, rect_.height() - top_h);
            p->drawText(cr, Qt::AlignRight | Qt::AlignVCenter, columns_[i]);
        }
    }

  private:
    QRectF rect_;
    QString title_;
    QString summary_;
    QStringList columns_;
    double col_w_;
    bool side_;
    QColor tick_;
};

RgsNodeItem* rgs_node_item(QGraphicsItem* it) {
    return it && it->type() == RgsNodeItem::Type ? static_cast<RgsNodeItem*>(it) : nullptr;
}

QString rgs_summary(const RelGraphGroup& g, const RelationshipData& d) {
    using fmt::number;
    QStringList parts;
    const int shown_real = g.shown - ((g.expandable || g.universe > g.total_rows) ? 1 : 0);
    if (g.status.state == RelSectionState::Loading && g.total_rows == 0)
        return RelationshipGraphScene::tr("loading…");
    if (g.total_rows == 0)
        return QString();
    switch (g.group) {
        case RelGroup::Institutions:
            if (d.is_fund())
                return RelationshipGraphScene::tr("manager");
            parts << (g.universe > 0 ? RelationshipGraphScene::tr("%1 of %2").arg(shown_real).arg(number(g.universe, 0))
                                     : RelationshipGraphScene::tr("%1 listed").arg(shown_real));
            if (rgs_ok(g.total_pct))
                parts << RelationshipGraphScene::tr("%1 held").arg(fmt::pct(g.total_pct, 1, false));
            break;
        case RelGroup::Funds:
        case RelGroup::Holdings:
            parts << RelationshipGraphScene::tr("%1 of %2").arg(shown_real).arg(g.total_rows);
            if (rgs_ok(g.rows_pct))
                parts << RelationshipGraphScene::tr("%1 listed").arg(fmt::pct(g.rows_pct, 1, false));
            break;
        case RelGroup::Insiders:
            parts << RelationshipGraphScene::tr("%1 of %2").arg(shown_real).arg(g.total_rows);
            if (rgs_ok(g.total_pct))
                parts << RelationshipGraphScene::tr("insiders hold %1").arg(fmt::pct(g.total_pct, 2, false));
            break;
        case RelGroup::Management:
            parts << RelationshipGraphScene::tr("%1 officers").arg(g.total_rows);
            break;
        case RelGroup::Peers:
            parts << RelationshipGraphScene::tr("%1 of %2").arg(shown_real).arg(g.total_rows);
            if (d.peer_source.startsWith(QLatin1String("screen")))
                parts << RelationshipGraphScene::tr("industry screen");
            else if (d.peer_source == QLatin1String("generic_fallback"))
                parts << RelationshipGraphScene::tr("generic — not comparable");
            break;
        case RelGroup::Sectors:
            parts << RelationshipGraphScene::tr("%1 sectors").arg(g.total_rows);
            break;
        case RelGroup::Correlated:
            parts << RelationshipGraphScene::tr("daily returns %1").arg(d.correlation.window.isEmpty()
                                                                             ? QStringLiteral("1y")
                                                                             : d.correlation.window);
            break;
        case RelGroup::Subsidiaries:
            parts << RelationshipGraphScene::tr("%1 of %2").arg(shown_real).arg(g.total_rows);
            if (!d.subsidiaries_filing.form.isEmpty())
                parts << RelationshipGraphScene::tr("%1 %2").arg(d.subsidiaries_filing.form, d.subsidiaries_filing.filed);
            break;
        case RelGroup::SupplyChain:
        case RelGroup::Count:
            break;
    }
    return parts.join(QStringLiteral(" · "));
}

/// The text on an edge for the chosen metric (the group's stand-in when it has none).
QString rgs_edge_label(const RelGraphEdge& e, RelEdgeMetric m) {
    const RelEdgeMetric chain[] = {m, effective_metric(e.group, m)};
    for (RelEdgeMetric c : chain) {
        const double v = edge_metric_value(e, c);
        if (!rgs_ok(v))
            continue;
        switch (c) {
            case RelEdgeMetric::Percent:
                return fmt::pct(v, 2, false);
            case RelEdgeMetric::Value:
                return amount_text(v, e.currency);
            case RelEdgeMetric::Change:
                return rgs_change(v);
            case RelEdgeMetric::Correlation:
                return QStringLiteral("ρ ") + fmt::number(v, 2);
        }
    }
    return {};
}

} // namespace

// ── Scene ───────────────────────────────────────────────────────────────────

struct RelationshipGraphScene::Measure {
    RelLayoutMetrics met;
    double col_w = 60;
};

RelationshipGraphScene::RelationshipGraphScene(QObject* parent) : QGraphicsScene(parent) {
    setItemIndexMethod(QGraphicsScene::BspTreeIndex);
    setBackgroundBrush(Qt::NoBrush); // drawBackground paints the theme's base colour
    // Painted items read the tokens at paint time: a theme switch is a repaint.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { update(); });
}

RelationshipGraphScene::~RelationshipGraphScene() {
    ++layout_seq_; // a layout still running is dropped
}

RelationshipGraphScene::Measure RelationshipGraphScene::measure() const {
    Measure m;
    const QFontMetricsF fm(fonts::font(-2));
    const double ch = std::max(5.0, fm.horizontalAdvance(QLatin1Char('0')));
    RelLayoutMetrics& met = m.met;
    // Dense rows (the terminal look): about 1.45 lines per card, hairline gaps.
    met.row_h = std::round(fm.height() * 1.45);
    met.row_gap = 2;
    m.col_w = std::round(ch * 7.8);
    met.card_w = std::round(ch * 44.0);
    met.center_w = std::round(ch * 44.0);
    // The company card's height is exactly what paint_center() draws.
    {
        const double pad = std::max(10.0, met.row_h * 0.5);
        const QFontMetricsF big(fonts::font(5, QFont::Bold)), name(fonts::font(-1)), price(fonts::font(3, QFont::Bold));
        const QFontMetricsF lab(fonts::font(-4, QFont::DemiBold)), val(fonts::font(-2, QFont::DemiBold));
        const QFontMetricsF line(fonts::font(-3)), key(fonts::font(-4, QFont::DemiBold));
        const double bar = std::max(5.0, met.row_h * 0.28);
        met.center_h = std::ceil(pad * 0.6 + big.height() + name.height() + 4 + price.height() + 6 + 6 +
                                 2 * (lab.height() + val.height() + 4) + 4 + line.height() + 6 + bar + 2 +
                                 key.height() + pad * 0.6);
    }
    met.header_h = std::round(fm.height() * 1.8);
    met.group_gap = std::round(fm.height() * 0.9);
    met.column_gap = std::round(ch * 17.0);
    met.side_caption_h = std::round(fm.height() * 1.6);
    met.bubble_min = std::round(fm.height() * 0.42);
    met.bubble_max = std::round(fm.height() * 2.2);
    const QFontMetricsF lfm(fonts::font(-3, QFont::DemiBold));
    met.label_h = std::round(lfm.height() + 2);
    met.char_w = lfm.horizontalAdvance(QLatin1Char('0'));
    met.placeholder_rows = 3;
    for (const auto& n : model_.nodes) {
        const QString s = n.short_label.isEmpty() ? n.label : n.short_label;
        met.label_w.insert(n.id, std::min(lfm.horizontalAdvance(s), met.char_w * 20) + 4);
    }
    return m;
}

void RelationshipGraphScene::set_graph(const RelationshipData& data, const RelGraphModel& model, RelLayoutMode mode,
                                       RelEdgeMetric metric, bool animate) {
    const bool same_company = !model_.center_id.isEmpty() && model_.center_id == model.center_id;
    const bool same_mode = mode == mode_;
    data_ = data;
    model_ = model;
    mode_ = mode;
    metric_ = metric;
    weights_ = edge_weights(model_, metric_);
    if (!model_.index.contains(selected_))
        selected_.clear();
    start_layout(animate && same_company && same_mode);
}

void RelationshipGraphScene::relayout() {
    if (!model_.center_id.isEmpty())
        start_layout(false);
}

void RelationshipGraphScene::start_layout(bool animate) {
    const quint64 seq = ++layout_seq_;
    const Measure m = measure();
    QHash<QString, QPointF> previous;
    for (auto it = node_items_.cbegin(); it != node_items_.cend(); ++it)
        previous.insert(it.key(), it.value()->pos());
    const RelGraphModel model = model_;
    const QVector<double> weights = weights_;
    const RelLayoutMode mode = mode_;
    const RelLayoutMetrics met = m.met;
    auto* watcher = new QFutureWatcher<RelLayoutResult>(this);
    connect(watcher, &QFutureWatcher<RelLayoutResult>::finished, this, [this, watcher, seq, animate]() {
        watcher->deleteLater();
        if (seq != layout_seq_)
            return; // superseded by a newer picture
        apply_layout(watcher->result(), animate);
    });
    watcher->setFuture(QtConcurrent::run([model, mode, met, weights, previous]() {
        return compute_layout(model, mode, met, weights, previous);
    }));
}

void RelationshipGraphScene::apply_layout(const RelLayoutResult& result, bool animate) {
    QElapsedTimer t;
    t.start();
    layout_ = result;
    const Measure m = measure();
    const bool bubbles = mode_ == RelLayoutMode::Radial || mode_ == RelLayoutMode::Force;
    const bool bars = mode_ == RelLayoutMode::Sankey;
    const bool motion = animate && ui::motion_enabled();

    if (anim_)
        anim_->stop();
    // Chrome and edges are rebuilt; node items are kept (they glide to their new place).
    for (QGraphicsItem* it : std::as_const(chrome_items_)) {
        removeItem(it);
        delete it;
    }
    chrome_items_.clear();
    for (QGraphicsItem* it : std::as_const(edge_items_)) {
        removeItem(it);
        delete it;
    }
    edge_items_.clear();

    QHash<QString, QGraphicsItem*> next;
    for (const auto& n : model_.nodes) {
        const auto g = layout_.nodes.constFind(n.id);
        if (g == layout_.nodes.constEnd())
            continue;
        const bool center = n.id == model_.center_id;
        const RgsStyle style = center ? (bubbles ? RgsStyle::CenterBubble : (bars ? RgsStyle::CenterBar : RgsStyle::Center))
                                      : (bubbles ? RgsStyle::Bubble : (bars ? RgsStyle::Bar : RgsStyle::Card));
        RgsNodeItem* item = rgs_node_item(node_items_.take(n.id));
        bool fresh = false;
        QPointF from = g.value().center;
        if (!item) {
            item = new RgsNodeItem(n, style);
            addItem(item);
            fresh = true;
        } else {
            from = item->pos();
            item->set_node(n);
            if (item->style() != style)
                item->set_style(style);
        }
        item->set_geometry(g.value(), rgs_columns(n.group), m.met.row_h, m.col_w);
        if (center)
            item->set_center_text(rgs_center_text(data_, n));
        else
            item->set_text(rgs_text_for(n, data_));
        if (center && style == RgsStyle::Center)
            item->setToolTip(data_.company.description);
        item->set_motion(fresh ? g.value().center : from, g.value().center, fresh && motion);
        if (motion)
            item->apply_progress(0.0);
        else
            item->finish_motion();
        next.insert(n.id, item);
    }
    for (QGraphicsItem* gone : std::as_const(node_items_)) {
        removeItem(gone);
        delete gone;
    }
    node_items_ = next;

    // Edges.
    const bool geom_from_node = mode_ == RelLayoutMode::Tiered || mode_ == RelLayoutMode::Hierarchy ||
                                mode_ == RelLayoutMode::Sankey || mode_ == RelLayoutMode::Matrix;
    for (int i = 0; i < model_.edges.size() && i < layout_.edges.size(); ++i) {
        const RelEdgeGeom& eg = layout_.edges[i];
        if (!eg.visible)
            continue;
        const RelGraphEdge& e = model_.edges[i];
        const RelGraphNode* from = model_.node(e.from);
        const RelGraphNode* to = model_.node(e.to);
        const bool cluster = (from && from->kind == RelNodeKind::Cluster) || (to && to->kind == RelNodeKind::Cluster);
        const bool arrow_at_end = geom_from_node ? (e.to == model_.center_id) : true;
        auto* item = new RgsEdgeItem(e, eg, weights_.value(i, 0.3), arrow_at_end, e.directed && !bars, cluster);
        if (eg.label_slot)
            item->set_label(rgs_edge_label(e, metric_));
        addItem(item);
        if (motion)
            item->setOpacity(0.0);
        edge_items_.append(item);
    }

    // Group headers / captions.
    const bool fund = data_.is_fund();
    for (const auto& gg : layout_.groups) {
        const RelGraphGroup* grp = model_.group(gg.group);
        if (!grp || gg.header.isEmpty())
            continue;
        if (bars)
            continue;
        const bool compact = bubbles;
        auto* item = new RgsChromeItem(gg.header, rgs_group_title(gg.group, fund), compact ? QString() : rgs_summary(*grp, data_),
                                       compact ? QStringList() : rgs_columns(gg.group), m.col_w, compact,
                                       rgs_group_color(gg.group));
        addItem(item);
        if (motion)
            item->setOpacity(0.0);
        chrome_items_.append(item);
    }
    if (!layout_.left_caption.isEmpty()) {
        auto* item = new RgsChromeItem(layout_.left_caption,
                                       fund ? tr("ISSUER & SECTORS") : tr("OWNERSHIP & MANAGEMENT"), QString(), {},
                                       m.col_w, true, QColor());
        addItem(item);
        chrome_items_.append(item);
    }
    if (!layout_.right_caption.isEmpty()) {
        auto* item = new RgsChromeItem(layout_.right_caption, fund ? tr("HOLDINGS & MARKET") : tr("MARKET & STRUCTURE"),
                                       QString(), {}, m.col_w, true, QColor());
        addItem(item);
        chrome_items_.append(item);
    }

    content_rect_ = layout_.bounds;
    // Room to pan beyond the content.
    const double mx = std::max(200.0, content_rect_.width() * 0.6), my = std::max(200.0, content_rect_.height() * 0.6);
    setSceneRect(content_rect_.adjusted(-mx, -my, mx, my));
    update_emphasis();
    last_layout_us_ = layout_.elapsed_us;
    last_build_us_ = t.nsecsElapsed() / 1000;

    if (motion) {
        auto* a = new QVariantAnimation(this);
        a->setStartValue(0.0);
        a->setEndValue(1.0);
        a->setDuration(260);
        a->setEasingCurve(QEasingCurve::OutCubic);
        connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { set_progress(v.toDouble()); });
        connect(a, &QVariantAnimation::finished, this, [this]() { set_progress(1.0); });
        anim_ = a;
        a->start(QAbstractAnimation::DeleteWhenStopped);
    }
    emit layout_applied(last_layout_us_, last_build_us_);
}

void RelationshipGraphScene::set_progress(double t) {
    for (QGraphicsItem* it : std::as_const(node_items_))
        if (auto* n = rgs_node_item(it)) {
            if (t >= 1.0)
                n->finish_motion();
            else
                n->apply_progress(t);
        }
    for (QGraphicsItem* it : std::as_const(edge_items_))
        it->setOpacity(t);
    for (QGraphicsItem* it : std::as_const(chrome_items_))
        it->setOpacity(t);
}

void RelationshipGraphScene::refresh_text() {
    for (auto it = node_items_.begin(); it != node_items_.end(); ++it) {
        auto* n = rgs_node_item(it.value());
        if (!n)
            continue;
        if (it.key() == model_.center_id)
            n->set_center_text(rgs_center_text(data_, n->node()));
        else
            n->set_text(rgs_text_for(n->node(), data_));
        n->update();
    }
    // Headers and edge labels carry numbers too: the cheapest correct path is a re-apply.
    if (!model_.center_id.isEmpty())
        apply_layout(layout_, false);
}

void RelationshipGraphScene::clear_graph() {
    ++layout_seq_;
    if (anim_)
        anim_->stop();
    clear();
    node_items_.clear();
    edge_items_.clear();
    chrome_items_.clear();
    if (pulse_)
        pulse_->stop();
    model_ = {};
    data_ = {};
    layout_ = {};
    selected_.clear();
    hover_.clear();
    neighbourhood_.clear();
    content_rect_ = {};
}

// ── Selection, hover, emphasis ──────────────────────────────────────────────

void RelationshipGraphScene::update_emphasis() {
    const QString focus = !hover_.isEmpty() ? hover_ : selected_;
    neighbourhood_.clear();
    if (!focus.isEmpty()) {
        neighbourhood_.insert(focus);
        for (const auto& e : model_.edges) {
            if (e.from == focus)
                neighbourhood_.insert(e.to);
            else if (e.to == focus)
                neighbourhood_.insert(e.from);
        }
    }
    const bool dim_rest = !hover_.isEmpty() && hover_ != model_.center_id;
    for (auto it = node_items_.begin(); it != node_items_.end(); ++it) {
        auto* n = rgs_node_item(it.value());
        if (!n)
            continue;
        const bool in = neighbourhood_.contains(it.key());
        n->set_flags(it.key() == selected_, dim_rest && !in, it.key() == hover_);
    }
    for (QGraphicsItem* it : std::as_const(edge_items_)) {
        auto* e = static_cast<RgsEdgeItem*>(it);
        const bool touches = !focus.isEmpty() && (e->edge().from == focus || e->edge().to == focus);
        e->set_emphasis(touches ? 1 : (dim_rest ? -1 : 0));
    }
}

void RelationshipGraphScene::set_hover(const QString& id) {
    if (id == hover_)
        return;
    hover_ = id;
    update_emphasis();
    emit hovered(id);
}

void RelationshipGraphScene::select_node(const QString& id, bool center_on) {
    selected_ = model_.index.contains(id) ? id : QString();
    update_emphasis();
    if (center_on && !selected_.isEmpty()) {
        if (QGraphicsItem* it = node_items_.value(selected_))
            for (QGraphicsView* v : views())
                v->centerOn(it);
    }
}

QString RelationshipGraphScene::node_at(const QPointF& scene_pos) const {
    for (QGraphicsItem* it : items(scene_pos, Qt::IntersectsItemShape, Qt::DescendingOrder)) {
        if (auto* n = rgs_node_item(it))
            return n->node().id;
    }
    return {};
}

void RelationshipGraphScene::step_selection(int dx, int dy) {
    // Reading order: the groups in model order, the rows in rank order; the centre first.
    QStringList order{model_.center_id};
    for (const auto& g : model_.groups)
        for (const QString& id : g.node_ids)
            if (node_items_.contains(id))
                order << id;
    if (order.size() <= 1)
        return;
    QString cur = selected_.isEmpty() ? model_.center_id : selected_;
    if (dy != 0) {
        const int i = static_cast<int>(order.indexOf(cur));
        cur = order.value(std::clamp(i + dy, 0, static_cast<int>(order.size()) - 1));
    } else if (dx != 0) {
        // Left / right: the nearest node on that side by geometry.
        const QGraphicsItem* from = node_items_.value(cur);
        const QPointF p = from ? from->pos() : QPointF();
        QString best;
        double best_d = 1e18;
        for (auto it = node_items_.cbegin(); it != node_items_.cend(); ++it) {
            const QPointF q = it.value()->pos();
            if ((dx > 0 && q.x() <= p.x() + 1) || (dx < 0 && q.x() >= p.x() - 1))
                continue;
            const double dist = std::abs(q.x() - p.x()) * 0.3 + std::abs(q.y() - p.y());
            if (dist < best_d) {
                best_d = dist;
                best = it.key();
            }
        }
        if (!best.isEmpty())
            cur = best;
    }
    select_node(cur, true);
    emit node_clicked(cur);
}

QString RelationshipGraphScene::find_and_pulse(const QString& text) {
    const QString q = text.trimmed();
    if (q.isEmpty())
        return {};
    QString found;
    for (const auto& n : model_.nodes) {
        if (!node_items_.contains(n.id) || n.kind == RelNodeKind::Placeholder)
            continue;
        if (n.short_label.compare(q, Qt::CaseInsensitive) == 0 || n.symbol.compare(q, Qt::CaseInsensitive) == 0) {
            found = n.id;
            break;
        }
        if (found.isEmpty() && (n.label.contains(q, Qt::CaseInsensitive) || n.short_label.contains(q, Qt::CaseInsensitive)))
            found = n.id;
    }
    if (found.isEmpty())
        return {};
    select_node(found, true);
    if (pulse_) {
        pulse_->stop();
        if (auto* old = rgs_node_item(node_items_.value(pulse_id_)))
            old->set_pulse(0);
    }
    pulse_id_ = found;
    if (!ui::motion_enabled())
        return found;
    if (!pulse_) {
        pulse_ = new QVariantAnimation(this);
        pulse_->setStartValue(0.0);
        pulse_->setEndValue(3.0);
        pulse_->setDuration(1800);
        connect(pulse_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            if (auto* n = rgs_node_item(node_items_.value(pulse_id_))) {
                const double x = v.toDouble();
                n->set_pulse(x >= 2.999 ? 0.0 : std::fmod(x, 1.0) + 1e-3);
            }
        });
    }
    pulse_->start();
    return found;
}

// ── Mouse ───────────────────────────────────────────────────────────────────

void RelationshipGraphScene::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QGraphicsScene::mousePressEvent(event);
        return;
    }
    const QString id = node_at(event->scenePos());
    if (id.isEmpty()) {
        if (!selected_.isEmpty()) {
            selected_.clear();
            update_emphasis();
        }
        emit background_clicked();
        QGraphicsScene::mousePressEvent(event); // not accepted: the view pans
        return;
    }
    event->accept();
    const RelGraphNode* n = model_.node(id);
    if (n && n->kind == RelNodeKind::Cluster) {
        if (n->hidden_count > 0)
            emit cluster_clicked(static_cast<int>(n->group));
        return;
    }
    if (n && n->kind == RelNodeKind::Placeholder) {
        if (section_failed(n->state))
            emit retry_clicked(section_of(n->group));
        return;
    }
    select_node(id);
    emit node_clicked(id);
}

void RelationshipGraphScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    const QString id = node_at(event->scenePos());
    if (event->button() == Qt::LeftButton && !id.isEmpty()) {
        const RelGraphNode* n = model_.node(id);
        if (n && n->kind != RelNodeKind::Cluster && n->kind != RelNodeKind::Placeholder)
            emit node_double_clicked(id);
        event->accept();
        return;
    }
    QGraphicsScene::mouseDoubleClickEvent(event);
}

void RelationshipGraphScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    if (event->buttons() == Qt::NoButton) {
        const QString id = node_at(event->scenePos());
        const RelGraphNode* n = model_.node(id);
        set_hover(n && n->kind != RelNodeKind::Placeholder ? id : QString());
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void RelationshipGraphScene::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
    const QString id = node_at(event->scenePos());
    const RelGraphNode* n = model_.node(id);
    if (!n || n->kind == RelNodeKind::Cluster || n->kind == RelNodeKind::Placeholder) {
        event->ignore();
        return;
    }
    select_node(id);
    emit node_clicked(id);
    emit node_context_menu(id, event->screenPos());
    event->accept();
}

void RelationshipGraphScene::drawBackground(QPainter* painter, const QRectF& rect) {
    painter->fillRect(rect, QColor(colors::BG_BASE()));
    // A hairline grid, faint, every 4 rows: the "chart paper" of the terminal.
    const double lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(painter->worldTransform());
    if (lod < 0.45)
        return;
    const double step = std::max(24.0, QFontMetricsF(fonts::font(-2)).height() * 4.0);
    painter->setPen(QPen(colors::overlay_color(0.035), 0));
    const double x0 = std::floor(rect.left() / step) * step;
    const double y0 = std::floor(rect.top() / step) * step;
    for (double x = x0; x < rect.right(); x += step)
        painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    for (double y = y0; y < rect.bottom(); y += step)
        painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
}

// ── View ────────────────────────────────────────────────────────────────────

RelationshipGraphView::RelationshipGraphView(RelationshipGraphScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent), rscene_(scene) {
    setObjectName(QStringLiteral("relmapGraphView"));
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setMouseTracking(true);
    setAccessibleName(tr("Relationship graph"));
    // A new picture fits itself unless the user has zoomed in on something.
    connect(scene, &RelationshipGraphScene::layout_applied, this, [this](qint64, qint64) {
        if (!user_zoomed_)
            fit_to_content();
    });
}

void RelationshipGraphView::fit_to_content() {
    if (!rscene_ || rscene_->content_rect().isEmpty())
        return;
    const QRectF r = rscene_->content_rect();
    fitInView(r, Qt::KeepAspectRatio);
    // Never blow a small map up past 1.25x (text would look coarse); never below the limit.
    double z = transform().m11();
    if (z > 1.25) {
        resetTransform();
        scale(1.25, 1.25);
        centerOn(r.center());
        z = 1.25;
    }
    if (z < kMinZoom) {
        resetTransform();
        scale(kMinZoom, kMinZoom);
        centerOn(r.center());
        z = kMinZoom;
    }
    current_zoom_ = z;
    user_zoomed_ = false;
    emit zoom_changed(current_zoom_);
}

void RelationshipGraphView::zoom_by(double factor) {
    const double target = std::clamp(current_zoom_ * factor, kMinZoom, kMaxZoom);
    const double f = target / current_zoom_;
    if (std::abs(f - 1.0) < 1e-6)
        return;
    scale(f, f);
    current_zoom_ = target;
    user_zoomed_ = true;
    emit zoom_changed(current_zoom_);
}

void RelationshipGraphView::wheelEvent(QWheelEvent* event) {
    const double delta =
        event->pixelDelta().isNull() ? event->angleDelta().y() / 120.0 : event->pixelDelta().y() / 120.0;
    if (std::abs(delta) < 1e-9) {
        event->ignore();
        return;
    }
    zoom_by(std::pow(1.12, delta));
    event->accept();
}

void RelationshipGraphView::keyPressEvent(QKeyEvent* event) {
    if (!rscene_) {
        QGraphicsView::keyPressEvent(event);
        return;
    }
    switch (event->key()) {
        case Qt::Key_Home:
            fit_to_content();
            break;
        case Qt::Key_Up:
            rscene_->step_selection(0, -1);
            break;
        case Qt::Key_Down:
            rscene_->step_selection(0, 1);
            break;
        case Qt::Key_Left:
            rscene_->step_selection(-1, 0);
            break;
        case Qt::Key_Right:
            rscene_->step_selection(1, 0);
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (!rscene_->selected_node().isEmpty())
                emit activate_requested(rscene_->selected_node());
            break;
        case Qt::Key_Escape:
            rscene_->select_node(QString());
            emit rscene_->background_clicked();
            break;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoom_by(1.2);
            break;
        case Qt::Key_Minus:
            zoom_by(1.0 / 1.2);
            break;
        default:
            QGraphicsView::keyPressEvent(event);
            return;
    }
    event->accept();
}

void RelationshipGraphView::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    if (!user_zoomed_)
        fit_to_content();
}

} // namespace fincept::relmap
