// src/screens/relationship_map/RelationshipMapScreen_Detail.cpp
// The detail pane (the selected entity and its relationship to the company), the context
// menu of graph nodes and table rows, and the exports.
#include "core/agent/AgentRiskMarkers.h"
#include "core/events/EventBus.h"
#include "core/instrument/VenueSymbols.h"
#include "screens/relationship_map/RelationshipGraphScene.h"
#include "screens/relationship_map/RelationshipLinkedViews.h"
#include "screens/relationship_map/RelationshipMapFormat.h"
#include "screens/relationship_map/RelationshipMapScreen.h"
#include "services/relationship_map/RelationshipMapParse.h"
#include "storage/repositories/WatchlistRepository.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/DonutChart.h"
#include "ui/charts/Gauge.h"
#include "ui/charts/MatrixHeatmap.h"
#include "ui/charts/RangeBar.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/DataTable.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/StatTile.h"

#include <QApplication>
#include <QClipboard>
#include <QDate>
#include <QDir>
#include <QFileDialog>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

using namespace fincept::ui;
using namespace fincept::relmap;
namespace fmt = fincept::ui::formatting;

namespace {

bool rmsd_ok(double v) {
    return std::isfinite(v);
}

bool rmsd_is_mappable(const QString& yahoo_symbol) {
    const auto parts = core::instrument::from_yahoo(yahoo_symbol);
    return parts.valid() && (parts.cls == core::instrument::AssetClass::Equity ||
                             parts.cls == core::instrument::AssetClass::Etf ||
                             parts.cls == core::instrument::AssetClass::Fund);
}

void rmsd_open_in(const QString& screen_id, const QString& symbol) {
    EventBus::instance().publish("nav.open_symbol", {{"screen_id", screen_id}, {"symbol", symbol}});
}

QString rmsd_csv(const QString& s) {
    if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"')) || s.contains(QLatin1Char('\n'))) {
        QString q = s;
        q.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + q + QLatin1Char('"');
    }
    return s;
}

QString rmsd_num(double v) {
    return rmsd_ok(v) ? fmt::export_number(v) : QString();
}

QString rmsd_kind_caption(const RelGraphNode& n, bool fund) {
    switch (n.group) {
        case RelGroup::Institutions:
            return fund ? RelationshipMapScreen::tr("FUND ISSUER") : RelationshipMapScreen::tr("INSTITUTIONAL HOLDER");
        case RelGroup::Funds:
            return RelationshipMapScreen::tr("FUND HOLDER");
        case RelGroup::Insiders:
            return RelationshipMapScreen::tr("INSIDER");
        case RelGroup::Management:
            return RelationshipMapScreen::tr("OFFICER");
        case RelGroup::Peers:
            return RelationshipMapScreen::tr("PEER");
        case RelGroup::Holdings:
            return RelationshipMapScreen::tr("HOLDING");
        case RelGroup::Sectors:
            return RelationshipMapScreen::tr("SECTOR WEIGHT");
        case RelGroup::Correlated:
            return n.kind == RelNodeKind::Index ? RelationshipMapScreen::tr("BENCHMARK INDEX")
                                                : RelationshipMapScreen::tr("SECTOR ETF");
        case RelGroup::Subsidiaries:
            return RelationshipMapScreen::tr("SUBSIDIARY");
        case RelGroup::SupplyChain:
            return RelationshipMapScreen::tr("SUPPLY CHAIN");
        case RelGroup::Count:
            break;
    }
    return fund ? RelationshipMapScreen::tr("FUND") : RelationshipMapScreen::tr("COMPANY");
}

/// Old detail-panel kind codes for RelationshipMapFormat.h's missing_for_kind().
QString rmsd_legacy_kind(RelGroup g) {
    switch (g) {
        case RelGroup::Institutions:
            return QStringLiteral("institutional");
        case RelGroup::Funds:
            return QStringLiteral("mutualfund");
        case RelGroup::Insiders:
        case RelGroup::Management:
            return QStringLiteral("insider");
        case RelGroup::Peers:
            return QStringLiteral("peer");
        default:
            break;
    }
    return QStringLiteral("company");
}

/// One sentence: how this entity relates to the company, with its numbers and dates.
QString rmsd_sentence(const RelGraphNode& n, const RelationshipData& d) {
    const QString t = d.company.ticker;
    switch (n.group) {
        case RelGroup::Institutions:
        case RelGroup::Funds: {
            if (d.is_fund())
                return RelationshipMapScreen::tr("Issuer and manager of %1%2.")
                    .arg(t, rmsd_ok(n.value) ? RelationshipMapScreen::tr(" (%1 under management)")
                                                   .arg(amount_text(n.value, n.value_currency))
                                             : QString());
            QStringList s;
            s << RelationshipMapScreen::tr("Holds %1 of %2").arg(fmt::pct(n.pct, 2, false), t);
            if (rmsd_ok(n.value))
                s << RelationshipMapScreen::tr("= %1").arg(amount_text(n.value, n.value_currency));
            if (rmsd_ok(n.shares))
                s << RelationshipMapScreen::tr("(%1 shares)").arg(fmt::compact(n.shares));
            QString out = s.join(QLatin1Char(' '));
            if (!n.date.isEmpty())
                out += RelationshipMapScreen::tr(" as of %1").arg(n.date);
            if (rmsd_ok(n.change) && n.change != 0)
                out += RelationshipMapScreen::tr("; position %1 %2 since the previous report")
                           .arg(n.change > 0 ? RelationshipMapScreen::tr("▲ up") : RelationshipMapScreen::tr("▼ down"),
                                fmt::pct(std::abs(n.change), 2, false));
            return out + QLatin1Char('.');
        }
        case RelGroup::Insiders:
        case RelGroup::Management: {
            QStringList parts;
            if (!n.subtitle.isEmpty())
                parts << RelationshipMapScreen::tr("%1 of %2").arg(n.subtitle, t);
            if (rmsd_ok(n.shares))
                parts << RelationshipMapScreen::tr("holds %1 shares%2%3")
                             .arg(fmt::compact(n.shares),
                                  rmsd_ok(n.value) ? RelationshipMapScreen::tr(" ≈ %1").arg(amount_text(n.value, n.value_currency))
                                                   : QString(),
                                  rmsd_ok(n.pct) ? RelationshipMapScreen::tr(" (%1)").arg(fmt::pct(n.pct, 4, false))
                                                 : QString());
            if (!n.note.isEmpty())
                parts << RelationshipMapScreen::tr("last transaction: %1%2")
                             .arg(n.note, n.date.isEmpty() ? QString() : RelationshipMapScreen::tr(" on %1").arg(n.date));
            return parts.isEmpty() ? QString() : parts.join(QStringLiteral("; ")) + QLatin1Char('.');
        }
        case RelGroup::Peers: {
            QStringList parts;
            parts << RelationshipMapScreen::tr("Competitor%1").arg(
                n.subtitle.isEmpty() ? QString() : RelationshipMapScreen::tr(" in %1").arg(n.subtitle));
            if (rmsd_ok(n.corr))
                parts << RelationshipMapScreen::tr("ρ %1 with %2 (1y daily returns)").arg(fmt::number(n.corr, 2), t);
            if (rmsd_ok(n.size_value))
                parts << RelationshipMapScreen::tr("market cap %1").arg(amount_text(n.size_value, n.size_currency));
            if (rmsd_ok(n.change))
                parts << RelationshipMapScreen::tr("52-week %1%2")
                             .arg(n.change >= 0 ? QStringLiteral("▲") : QStringLiteral("▼"),
                                  fmt::pct(std::abs(n.change), 1, false));
            return parts.join(QStringLiteral("; ")) + QLatin1Char('.');
        }
        case RelGroup::Holdings:
            return RelationshipMapScreen::tr("%1 of %2%3.")
                .arg(fmt::pct(n.pct, 2, false), t,
                     rmsd_ok(n.value) ? RelationshipMapScreen::tr(" ≈ %1").arg(amount_text(n.value, n.value_currency))
                                      : QString());
        case RelGroup::Sectors:
            return RelationshipMapScreen::tr("%1 of %2 is invested in %3.").arg(fmt::pct(n.pct, 1, false), t, n.label);
        case RelGroup::Correlated: {
            QStringList parts;
            if (rmsd_ok(n.corr))
                parts << RelationshipMapScreen::tr("ρ %1 with %2 (1y daily returns)").arg(fmt::number(n.corr, 2), t);
            const double beta = d.correlation.beta.value(n.symbol, std::numeric_limits<double>::quiet_NaN());
            if (rmsd_ok(beta))
                parts << RelationshipMapScreen::tr("β of %1 against it %2").arg(t, fmt::number(beta, 2));
            if (rmsd_ok(n.change))
                parts << RelationshipMapScreen::tr("1y return %1%2")
                             .arg(n.change >= 0 ? QStringLiteral("▲") : QStringLiteral("▼"),
                                  fmt::pct(std::abs(n.change), 1, false));
            return parts.join(QStringLiteral("; ")) + QLatin1Char('.');
        }
        case RelGroup::Subsidiaries:
            return RelationshipMapScreen::tr("Subsidiary of %1%2, listed in its %3 filed %4 (SEC Exhibit 21).")
                .arg(t, n.subtitle.isEmpty() ? QString() : RelationshipMapScreen::tr(" incorporated in %1").arg(n.subtitle),
                     d.subsidiaries_filing.form.isEmpty() ? QStringLiteral("annual report") : d.subsidiaries_filing.form,
                     d.subsidiaries_filing.filed);
        case RelGroup::SupplyChain:
        case RelGroup::Count:
            break;
    }
    return {};
}

} // namespace

QWidget* RelationshipMapScreen::build_detail_pane() {
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("relmapDetail"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumWidth(std::max(280, QFontMetrics(fonts::font(-2)).horizontalAdvance(QString(34, QLatin1Char('M')))));
    scroll->setStyleSheet(QStringLiteral("QScrollArea#relmapDetail { background: %1; border-left: 1px solid %2; }"
                                         "QWidget#relmapDetailHost { background: %1; }"
                                         "QLabel#relmapDetailKind { color: %3; font-weight: 700; letter-spacing: 1px;"
                                         " font-size:%4px; }"
                                         "QLabel#relmapDetailTitle { color: %5; font-weight: 700; font-size:%6px; }"
                                         "QLabel#relmapDetailSub { color: %7; font-size:%8px; }"
                                         "QLabel#relmapDetailText { color: %5; font-size:%8px; }"
                                         "QLabel#relmapDetailSection { color: %3; font-weight: 700; letter-spacing: 1px;"
                                         " font-size:%9px; padding-top: 6px; }"
                                         "QLabel#relmapDetailWarn { color: %10; background: %11; border-left: 2px solid %10;"
                                         " padding: 4px 6px; font-size:%9px; }"
                                         "QPushButton#relmapDetailAction { background: transparent; color: %12;"
                                         " border: 1px solid %13; padding: 4px 8px; font-weight: 700; font-size:%9px;"
                                         " text-align: left; }"
                                         "QPushButton#relmapDetailAction:hover { background: %12; color: %14; }")
                              .arg(colors::BG_SURFACE(), colors::BORDER_DIM(), colors::TEXT_TERTIARY(), fonts::px(-4),
                                   colors::TEXT_PRIMARY(), fonts::px(1), colors::TEXT_SECONDARY(), fonts::px(-2),
                                   fonts::px(-4), colors::WARNING(), colors::alpha(colors::WARNING, 0.08),
                                   colors::AMBER(), colors::AMBER_DIM(), colors::TEXT_ON_ACCENT()));
    detail_host_ = new QWidget(scroll);
    detail_host_->setObjectName(QStringLiteral("relmapDetailHost"));
    detail_layout_ = new QVBoxLayout(detail_host_);
    const int pad = std::max(8, QFontMetrics(fonts::font(-2)).height() / 2 + 2);
    detail_layout_->setContentsMargins(pad, pad, pad, pad);
    detail_layout_->setSpacing(6);
    scroll->setWidget(detail_host_);
    return scroll;
}

void RelationshipMapScreen::select_entity(const QString& node_id, bool from_table) {
    const QString id = node_id.isEmpty() ? model_.center_id : node_id;
    selected_id_ = id;
    scene_->select_node(id == model_.center_id ? QString() : id, from_table);
    if (!from_table)
        linked_->set_selected(id == model_.center_id ? QString() : id);
    show_detail(id);
}

void RelationshipMapScreen::activate_entity(const QString& node_id) {
    const RelGraphNode* n = model_.node(node_id);
    if (!n)
        return;
    if (n->kind == RelNodeKind::Center) {
        rmsd_open_in(QStringLiteral("equity_research"), n->symbol);
        return;
    }
    // A listed company or fund: walk the graph — the map re-centres on it.
    if (!n->symbol.isEmpty() && n->kind != RelNodeKind::Index && rmsd_is_mappable(n->symbol)) {
        analyze(n->symbol);
        return;
    }
    if (!n->symbol.isEmpty()) {
        rmsd_open_in(QStringLiteral("markets"), n->symbol);
        return;
    }
    select_entity(node_id, false); // a person, holder or subsidiary: its detail
}

void RelationshipMapScreen::show_detail(const QString& wanted_id) {
    if (!detail_layout_)
        return;
    while (QLayoutItem* child = detail_layout_->takeAt(0)) {
        if (QWidget* w = child->widget())
            w->deleteLater();
        delete child;
    }
    QWidget* host = detail_host_;
    if (!has_data_) {
        auto* hint = new QLabel(tr("Pick a company or fund above. Click a node or a table row to see how it relates "
                                   "to the company; double-click a listed company to re-centre the map on it."),
                                host);
        hint->setObjectName(QStringLiteral("relmapDetailSub"));
        hint->setWordWrap(true);
        detail_layout_->addWidget(hint);
        detail_layout_->addStretch(1);
        return;
    }
    const RelationshipData& d = current_data_;
    const CompanyInfo& co = d.company;
    const RelGraphNode* n = model_.node(wanted_id.isEmpty() ? model_.center_id : wanted_id);
    if (!n)
        n = model_.node(model_.center_id);
    if (!n)
        return;
    const bool center = n->kind == RelNodeKind::Center;
    const bool fund = d.is_fund();
    const QString ccy = co.currency;
    const QString fin_ccy = statement_currency(co.financial_currency, co.currency);
    using Tone = StatTile::Tone;

    auto label = [&](const QString& text, const char* name) {
        auto* l = new QLabel(text, host);
        l->setObjectName(QString::fromLatin1(name));
        l->setWordWrap(true);
        l->setTextFormat(Qt::PlainText);
        detail_layout_->addWidget(l);
        return l;
    };
    auto strip = [&]() {
        auto* s = new ui::KpiStrip(host);
        s->set_max_columns(2);
        s->set_spacing(4);
        detail_layout_->addWidget(s);
        return s;
    };
    auto tile = [](ui::KpiStrip* s, const QString& l, const QString& v, Tone tone = Tone::Neutral) {
        auto* t = s->add_tile(l);
        t->set_compact(true);
        t->set_value(v.isEmpty() ? fmt::placeholder() : v);
        t->set_tone(tone);
        return t;
    };
    auto sign_tone = [](double v) { return v > 0 ? Tone::Positive : (v < 0 ? Tone::Negative : Tone::Neutral); };
    auto action = [&](const QString& text, std::function<void()> fn) {
        auto* b = new QPushButton(text, host);
        b->setObjectName(QStringLiteral("relmapDetailAction"));
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [fn]() { fn(); });
        detail_layout_->addWidget(b);
        return b;
    };
    auto kv = [&](const QVector<QPair<QString, QString>>& rows) {
        QVector<QPair<QString, QString>> shown;
        for (const auto& r : rows)
            if (!r.second.trimmed().isEmpty())
                shown << r;
        if (shown.isEmpty())
            return;
        auto* t = new ui::DataTable(host);
        t->set_headers({tr("Field"), tr("Value")});
        t->horizontalHeader()->hide();
        t->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        t->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        t->setRowCount(static_cast<int>(shown.size()));
        for (int r = 0; r < shown.size(); ++r) {
            auto* k = new QTableWidgetItem(shown[r].first);
            k->setForeground(QColor(colors::TEXT_SECONDARY()));
            auto* v = new QTableWidgetItem(shown[r].second);
            v->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            v->setForeground(QColor(colors::TEXT_PRIMARY()));
            v->setToolTip(shown[r].second);
            t->setItem(r, 0, k);
            t->setItem(r, 1, v);
        }
        t->resizeColumnToContents(0);
        t->setFixedHeight(t->verticalHeader()->defaultSectionSize() * static_cast<int>(shown.size()) + 2 * t->frameWidth());
        detail_layout_->addWidget(t);
    };

    // Header.
    label(rmsd_kind_caption(*n, fund), "relmapDetailKind");
    label(center ? (co.name.isEmpty() ? co.ticker : co.name) : n->label, "relmapDetailTitle");
    if (center) {
        QStringList where{co.ticker};
        if (!co.exchange.isEmpty())
            where << co.exchange;
        if (!co.sector.isEmpty())
            where << co.sector;
        if (!co.industry.isEmpty())
            where << co.industry;
        label(where.join(QStringLiteral(" · ")), "relmapDetailSub");
    } else if (!n->subtitle.isEmpty() && n->group != RelGroup::Subsidiaries) {
        label(n->symbol.isEmpty() ? n->subtitle : n->symbol + QStringLiteral(" · ") + n->subtitle, "relmapDetailSub");
    }
    // Sections a slow source left out that this entity's view depends on.
    {
        QStringList missing = missing_for_kind(center ? QStringLiteral("company") : rmsd_legacy_kind(n->group),
                                               d.missing_sections);
        if (center)
            for (const QString& s : failed_sections(d))
                if (!missing.contains(s))
                    missing << s;
        if (!missing.isEmpty()) {
            label(tr("Not loaded this time: %1. Use RETRY THESE on the notice above the map.")
                      .arg(missing.join(QStringLiteral(", "))),
                  "relmapDetailWarn");
        }
    }

    if (!center) {
        const QString sentence = rmsd_sentence(*n, d);
        if (!sentence.isEmpty())
            label(sentence, "relmapDetailText");
        auto* s = strip();
        switch (n->group) {
            case RelGroup::Institutions:
            case RelGroup::Funds:
                if (!fund)
                    tile(s, tr("% HELD"), rmsd_ok(n->pct) ? fmt::pct(n->pct, 2, false) : QString());
                tile(s, fund ? tr("AUM") : tr("VALUE"), rmsd_ok(n->value) ? amount_text(n->value, n->value_currency) : QString());
                if (!fund) {
                    tile(s, tr("CHANGE"), rmsd_ok(n->change) ? fmt::pct(n->change, 2) : QString(),
                         rmsd_ok(n->change) ? sign_tone(n->change) : Tone::Neutral);
                    tile(s, tr("SHARES"), rmsd_ok(n->shares) ? fmt::compact(n->shares) : QString());
                    tile(s, tr("REPORTED"), n->date);
                    tile(s, tr("RANK"), n->rank >= 0 ? QStringLiteral("#%1").arg(n->rank + 1) : QString());
                }
                break;
            case RelGroup::Insiders:
            case RelGroup::Management: {
                tile(s, tr("SHARES"), rmsd_ok(n->shares) ? fmt::compact(n->shares) : QString());
                tile(s, tr("VALUE"), rmsd_ok(n->value) ? amount_text(n->value, n->value_currency) : QString());
                const QString k = n->note.toLower();
                tile(s, tr("LAST TRANSACTION"), n->note.isEmpty() ? QString() : n->note.toUpper(),
                     k == QLatin1String("buy") ? Tone::Positive : (k == QLatin1String("sell") ? Tone::Negative : Tone::Neutral));
                tile(s, tr("DATE"), n->date);
                if (n->group == RelGroup::Management) {
                    for (const auto& o : d.officers) {
                        if (relmap::node_id(RelGroup::Management, o.name) != n->id)
                            continue;
                        tile(s, tr("PAY"), o.total_pay > 0 ? amount_text(o.total_pay, fin_ccy) : QString());
                        tile(s, tr("AGE"), o.year_born > 0 ? QString::number(QDate::currentDate().year() - o.year_born)
                                                           : QString());
                    }
                }
                // This person's own transactions (Form 4 data via Yahoo).
                QVector<QPair<QString, QString>> rows;
                const QString surname = n->label.section(QLatin1Char(' '), -1).toUpper();
                for (const auto& tr_row : d.insider_trades) {
                    const bool same = same_person(n->label, tr_row.insider) || tr_row.insider.compare(n->label, Qt::CaseInsensitive) == 0 ||
                                      (n->group == RelGroup::Insiders && tr_row.insider.toUpper().startsWith(surname));
                    if (!same)
                        continue;
                    rows << qMakePair(tr_row.date, QStringLiteral("%1 %2 %3")
                                                       .arg(tr_row.kind.toUpper(), fmt::compact(tr_row.shares),
                                                            tr_row.value > 0 ? amount_text(tr_row.value, tr_row.currency)
                                                                             : QString()));
                    if (rows.size() >= 8)
                        break;
                }
                if (!rows.isEmpty()) {
                    label(tr("RECENT TRANSACTIONS"), "relmapDetailSection");
                    kv(rows);
                }
                break;
            }
            case RelGroup::Peers: {
                const PeerCompany* p = nullptr;
                for (const auto& x : d.peers)
                    if (relmap::node_id(RelGroup::Peers, x.ticker) == n->id)
                        p = &x;
                if (p) {
                    tile(s, tr("PRICE"), p->current_price > 0 ? price_text(p->current_price, p->currency) : QString());
                    tile(s, tr("MKT CAP"), p->market_cap > 0 ? amount_text(p->market_cap, p->currency) : QString());
                    auto* pe = tile(s, tr("P/E"), p->pe_ratio > 0 ? fmt::ratio(p->pe_ratio, 1) : QString());
                    if (p->pe_ratio > 0 && co.pe_ratio > 0)
                        pe->set_sub_label(tr("%1 %2").arg(co.ticker, fmt::ratio(co.pe_ratio, 1)));
                    tile(s, tr("ρ 1Y"), rmsd_ok(n->corr) ? fmt::number(n->corr, 2) : QString());
                    tile(s, tr("REV GROWTH"), p->revenue_growth != 0 ? fmt::fraction_pct(p->revenue_growth, 1) : QString(),
                         sign_tone(p->revenue_growth));
                    tile(s, tr("52W"), p->week52_change != 0 ? fmt::fraction_pct(p->week52_change, 1) : QString(),
                         sign_tone(p->week52_change));
                    kv({{tr("Industry"), p->industry.isEmpty() ? p->sector : p->industry},
                        {tr("Country"), p->country},
                        {tr("Net margin"), p->profit_margins != 0 ? fmt::fraction_pct(p->profit_margins, 1, false) : QString()},
                        {tr("ROE"), p->roe != 0 ? fmt::fraction_pct(p->roe, 1, false) : QString()},
                        {tr("EV / EBITDA"), p->ev_to_ebitda > 0 ? fmt::ratio(p->ev_to_ebitda, 1) : QString()},
                        {tr("Beta"), p->beta > 0 ? fmt::number(p->beta, 2) : QString()},
                        {tr("Rating"), p->recommendation.toUpper()}});
                }
                break;
            }
            case RelGroup::Holdings:
            case RelGroup::Sectors:
                tile(s, tr("WEIGHT"), rmsd_ok(n->pct) ? fmt::pct(n->pct, 2, false) : QString());
                tile(s, tr("VALUE"), rmsd_ok(n->value) ? amount_text(n->value, n->value_currency) : QString());
                if (rmsd_ok(n->corr))
                    tile(s, tr("ρ 1Y"), fmt::number(n->corr, 2));
                break;
            case RelGroup::Correlated: {
                tile(s, tr("ρ 1Y"), rmsd_ok(n->corr) ? fmt::number(n->corr, 2) : QString());
                const double beta = d.correlation.beta.value(n->symbol, std::numeric_limits<double>::quiet_NaN());
                tile(s, tr("β"), rmsd_ok(beta) ? fmt::number(beta, 2) : QString());
                tile(s, tr("1Y RETURN"), rmsd_ok(n->change) ? fmt::pct(n->change, 1) : QString(),
                     rmsd_ok(n->change) ? sign_tone(n->change) : Tone::Neutral);
                tile(s, tr("OBSERVATIONS"), d.correlation.observations > 0 ? QString::number(d.correlation.observations)
                                                                           : QString());
                break;
            }
            case RelGroup::Subsidiaries:
                tile(s, tr("JURISDICTION"), n->subtitle);
                tile(s, tr("FILING"), d.subsidiaries_filing.form.isEmpty()
                                          ? QString()
                                          : d.subsidiaries_filing.form + QLatin1Char(' ') + d.subsidiaries_filing.filed);
                break;
            case RelGroup::SupplyChain:
            case RelGroup::Count:
                break;
        }
        if (n->user_holds || n->in_watchlist)
            label(n->user_holds ? tr("You hold %1 in a portfolio.").arg(n->symbol)
                                : tr("%1 is in one of your watchlists.").arg(n->symbol),
                  "relmapDetailSub");
        // Where to take it next.
        if (!n->symbol.isEmpty()) {
            const QString sym = n->symbol;
            action(tr("OPEN IN EQUITY RESEARCH"), [sym]() { rmsd_open_in(QStringLiteral("equity_research"), sym); });
            action(tr("OPEN IN MARKETS"), [sym]() { rmsd_open_in(QStringLiteral("markets"), sym); });
            if (n->kind != RelNodeKind::Index && rmsd_is_mappable(sym)) {
                QPushButton* b = action(tr("RE-CENTRE MAP ON %1").arg(sym), [this, sym]() { analyze(sym); });
                b->setProperty(core::agent::kAgentRiskProperty, "spend"); // starts a new map download
            }
        }
        const QString id = n->id;
        action(tr("SHOW IN TABLE"), [this, id]() { linked_->set_selected(id, true); });
        detail_layout_->addStretch(1);
        return;
    }

    // ── The company / fund itself ──────────────────────────────────────────
    {
        auto* s = strip();
        auto* price = tile(s, tr("PRICE"), co.current_price > 0 ? price_text(co.current_price, ccy) : QString());
        if (co.day_change_pct != 0)
            price->set_delta(co.day_change_pct);
        if (fund) {
            tile(s, tr("AUM"), d.fund.total_assets > 0 ? amount_text(d.fund.total_assets, ccy) : QString());
            tile(s, tr("HOLDINGS"), d.fund.holdings.isEmpty() ? QString() : QString::number(d.fund.holdings.size()));
            tile(s, tr("YIELD"), co.dividend_yield > 0 ? fmt::pct(co.dividend_yield, 2, false) : QString());
        } else {
            tile(s, tr("MKT CAP"), co.market_cap > 0 ? amount_text(co.market_cap, ccy) : QString());
            auto* pe = tile(s, tr("P/E"), co.pe_ratio > 0 ? fmt::ratio(co.pe_ratio, 1) : QString());
            if (d.valuation.pe_vs_peers > 0)
                pe->set_sub_label(tr("%1 peer median").arg(fmt::ratio(d.valuation.pe_vs_peers, 2)));
            tile(s, tr("ROE"), co.roe != 0 ? fmt::fraction_pct(co.roe, 1) : QString(), sign_tone(co.roe));
            tile(s, tr("REV GROWTH"), co.revenue_growth != 0 ? fmt::fraction_pct(co.revenue_growth, 1) : QString(),
                 sign_tone(co.revenue_growth));
            const double net = d.margins.net != 0 ? d.margins.net : co.profit_margins;
            tile(s, tr("NET MARGIN"), net != 0 ? fmt::fraction_pct(net, 1) : QString(), sign_tone(net));
        }
    }
    // 52-week range and analyst targets.
    auto range = [&](double low, double high, const QVector<RangeMarker>& markers) {
        auto* rb = new ui::RangeBar(host);
        rb->setMinimumHeight(QFontMetrics(fonts::font(-3)).height() * 3 + 8);
        RangeBarData rd;
        rd.low = low;
        rd.high = high;
        rd.markers = markers;
        rb->set_data(rd);
        rb->set_value_formatter([ccy](double v) { return price_text(v, ccy); });
        detail_layout_->addWidget(rb);
    };
    auto marker = [](double v, const QString& l, RangeMarker::Role role) {
        RangeMarker m;
        m.value = v;
        m.label = l;
        m.role = role;
        return m;
    };
    const double price = co.current_price;
    if (d.technicals.fifty_two_week_high > d.technicals.fifty_two_week_low && d.technicals.fifty_two_week_low > 0) {
        label(tr("52-WEEK RANGE"), "relmapDetailSection");
        QVector<RangeMarker> m;
        if (price > 0)
            m << marker(price, tr("now"), RangeMarker::Role::Current);
        range(d.technicals.fifty_two_week_low, d.technicals.fifty_two_week_high, m);
    }
    const double t_low = d.analyst_targets.low > 0 ? d.analyst_targets.low : co.target_low;
    const double t_high = d.analyst_targets.high > 0 ? d.analyst_targets.high : co.target_high;
    const double t_mean = d.analyst_targets.mean > 0 ? d.analyst_targets.mean : co.target_mean;
    if (t_high > t_low && t_low > 0) {
        label(tr("ANALYST PRICE TARGETS · %1 ANALYSTS · %2")
                  .arg(co.analyst_count)
                  .arg(co.recommendation.toUpper().replace(QLatin1Char('_'), QLatin1Char(' '))),
              "relmapDetailSection");
        QVector<RangeMarker> m;
        if (t_mean > 0)
            m << marker(t_mean, tr("mean"), RangeMarker::Role::Mean);
        if (price > 0)
            m << marker(price, tr("now"), RangeMarker::Role::Current);
        range(std::min(t_low, price > 0 ? price : t_low), std::max(t_high, price), m);
    }
    // Analyst ratings (latest month) and their trend.
    if (!d.recommendations.isEmpty()) {
        const QVector<QColor> rc = {QColor(colors::POSITIVE()), QColor(colors::POSITIVE_DIM()), QColor(colors::WARNING()),
                                    QColor(colors::NEGATIVE_DIM()), QColor(colors::NEGATIVE())};
        const QStringList rn = {tr("Strong buy"), tr("Buy"), tr("Hold"), tr("Sell"), tr("Strong sell")};
        const auto& latest = d.recommendations.first();
        const int counts[5] = {latest.strong_buy, latest.buy, latest.hold, latest.sell, latest.strong_sell};
        QVector<DonutSlice> slices;
        for (int i = 0; i < 5; ++i)
            slices.append({QString::number(i), rn[i], static_cast<double>(counts[i]), rc[i]});
        label(tr("ANALYST RATINGS"), "relmapDetailSection");
        auto* donut = new ui::DonutChart(host);
        donut->setMinimumHeight(QFontMetrics(fonts::font(-3)).height() * 9);
        donut->set_slices(slices);
        donut->set_center_text(tr("analysts"));
        donut->set_legend_position(ui::DonutChart::LegendPosition::Right);
        donut->set_value_formatter([](double v) { return fmt::number(std::round(v), 0); });
        detail_layout_->addWidget(donut);
        if (d.recommendations.size() > 1) {
            QStringList periods;
            QVector<ui::BarSeries> series(5);
            for (int i = 0; i < 5; ++i) {
                series[i].name = rn[i];
                series[i].color = rc[i];
            }
            for (int p = static_cast<int>(d.recommendations.size()) - 1; p >= 0; --p) {
                const auto& snap = d.recommendations[p];
                periods << (snap.period == QLatin1String("0m") ? tr("Now") : snap.period);
                const int v[5] = {snap.strong_buy, snap.buy, snap.hold, snap.sell, snap.strong_sell};
                for (int i = 0; i < 5; ++i)
                    series[i].values.append(v[i]);
            }
            auto* bars = new ui::BarChart(host);
            bars->setMinimumHeight(QFontMetrics(fonts::font(-3)).height() * 8);
            bars->set_mode(ui::BarChart::Mode::Stacked);
            bars->set_categories(periods);
            bars->set_series(series);
            bars->set_value_formatter([](double v) { return fmt::number(std::round(v), 0); });
            detail_layout_->addWidget(bars);
        }
    }
    if (d.governance.overall_risk > 0) {
        label(tr("GOVERNANCE RISK (1 = LOW, 10 = HIGH)"), "relmapDetailSection");
        auto* g = new ui::Gauge(host);
        g->set_style(ui::Gauge::Style::Linear);
        g->set_range(1, 10);
        g->set_zones({{1, 3.5, GaugeZone::Tone::Good, tr("Low")},
                      {3.5, 6.5, GaugeZone::Tone::Caution, tr("Moderate")},
                      {6.5, 10, GaugeZone::Tone::Bad, tr("High")}});
        g->set_value(d.governance.overall_risk);
        g->set_caption(tr("Overall"));
        g->setMinimumHeight(QFontMetrics(fonts::font(-3)).height() * 4);
        detail_layout_->addWidget(g);
    }
    label(tr("PROFILE"), "relmapDetailSection");
    kv({{tr("Sector"), co.sector},
        {tr("Industry"), co.industry},
        {tr("Country"), co.country},
        {tr("Employees"), co.employees > 0 ? fmt::number(co.employees, 0) : QString()},
        {tr("Relative value"), d.valuation.pe_vs_peers > 0
                                   ? tr("%1 (%2 peers)").arg(d.valuation.status).arg(d.valuation.peer_count)
                                   : d.valuation.status},
        {tr("Forward P/E"), co.forward_pe > 0 ? fmt::ratio(co.forward_pe, 1) : QString()},
        {tr("Revenue"), co.revenue > 0 ? amount_text(co.revenue, fin_ccy) : QString()},
        {tr("Gross margin"), d.margins.gross != 0 ? fmt::fraction_pct(d.margins.gross, 1, false) : QString()},
        {tr("Operating margin"), d.margins.operating != 0 ? fmt::fraction_pct(d.margins.operating, 1, false) : QString()},
        {tr("Debt / equity"), d.margins.debt_to_equity > 0 ? fmt::number(d.margins.debt_to_equity, 1) : QString()},
        {tr("Next earnings"), d.calendar.earnings_date},
        {tr("Ex-dividend"), d.calendar.ex_dividend_date},
        {tr("Reports in"), co.financial_currency}});
    if (!co.description.isEmpty()) {
        label(tr("BUSINESS"), "relmapDetailSection");
        label(co.description, "relmapDetailSub");
    }
    const QString sym = co.ticker;
    action(tr("OPEN IN EQUITY RESEARCH"), [sym]() { rmsd_open_in(QStringLiteral("equity_research"), sym); });
    action(tr("OPEN IN MARKETS"), [sym]() { rmsd_open_in(QStringLiteral("markets"), sym); });
    detail_layout_->addStretch(1);
}

void RelationshipMapScreen::show_context_menu(const QString& node_id, const QPoint& global_pos) {
    const RelGraphNode* n = model_.node(node_id);
    if (!n)
        return;
    QMenu menu(this);
    menu.setStyleSheet(ui::ScreenCommandBar::menu_css());
    QAction* head = menu.addAction(n->kind == RelNodeKind::Center ? current_data_.company.name : n->label);
    head->setEnabled(false);
    menu.addSeparator();
    const QString sym = n->symbol;
    if (!sym.isEmpty()) {
        connect(menu.addAction(tr("Open in Equity Research")), &QAction::triggered, this,
                [sym]() { rmsd_open_in(QStringLiteral("equity_research"), sym); });
        connect(menu.addAction(tr("Open in Markets")), &QAction::triggered, this,
                [sym]() { rmsd_open_in(QStringLiteral("markets"), sym); });
        if (n->kind != RelNodeKind::Center && n->kind != RelNodeKind::Index && rmsd_is_mappable(sym)) {
            QAction* re = menu.addAction(tr("Re-centre the map on %1").arg(sym));
            re->setProperty(core::agent::kAgentRiskProperty, "spend");
            connect(re, &QAction::triggered, this, [this, sym]() { analyze(sym); });
        }
        if (!watchlists_.isEmpty()) {
            QMenu* wl = menu.addMenu(tr("Add to watchlist"));
            wl->setProperty(core::agent::kAgentRiskProperty, "account"); // writes the user's watchlist
            const QString name = n->label;
            for (const auto& [id, list_name] : watchlists_) {
                QAction* a = wl->addAction(list_name);
                a->setProperty(core::agent::kAgentRiskProperty, "account");
                connect(a, &QAction::triggered, this, [this, sym, name, id = id, list_name = list_name]() {
                    auto& repo = fincept::WatchlistRepository::instance();
                    const auto r = repo.add_stock(id, sym, name);
                    if (r.is_ok()) {
                        user_watchlist_.insert(sym.toUpper());
                        EventBus::instance().publish(QStringLiteral("watchlist.updated"),
                                                     QVariantMap{{QStringLiteral("action"), QStringLiteral("add")},
                                                                 {QStringLiteral("symbol"), sym}});
                        rebuild(true);
                    }
                    status_->show_notice(r.is_ok() ? tr("Added %1 to %2").arg(sym, list_name)
                                                   : tr("Could not add %1 to %2").arg(sym, list_name));
                });
            }
        }
        menu.addSeparator();
        connect(menu.addAction(tr("Copy ticker")), &QAction::triggered, this,
                [sym]() { QApplication::clipboard()->setText(sym); });
    }
    const QString label = n->kind == RelNodeKind::Center ? current_data_.company.name : n->label;
    connect(menu.addAction(tr("Copy name")), &QAction::triggered, this,
            [label]() { QApplication::clipboard()->setText(label); });
    const QString sentence = rmsd_sentence(*n, current_data_);
    if (!sentence.isEmpty())
        connect(menu.addAction(tr("Copy relationship")), &QAction::triggered, this,
                [sentence]() { QApplication::clipboard()->setText(sentence); });
    if (n->kind != RelNodeKind::Center) {
        const QString id = n->id;
        connect(menu.addAction(tr("Show in table")), &QAction::triggered, this,
                [this, id]() { linked_->set_selected(id, true); });
    }
    menu.exec(global_pos);
}

// ── Exports ─────────────────────────────────────────────────────────────────

QImage RelationshipMapScreen::render_graph_image(double scale) const {
    if (!has_data_)
        return {};
    if (graph_stack_->currentWidget() == matrix_)
        return matrix_->grab().toImage();
    const QRectF src = scene_->content_rect();
    if (src.isEmpty())
        return {};
    // Cap the long side so a huge expanded map stays a sane file.
    const double s = std::min(scale, 8000.0 / std::max(src.width(), src.height()));
    QImage img(QSize(static_cast<int>(std::ceil(src.width() * s)), static_cast<int>(std::ceil(src.height() * s))),
               QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(colors::BG_BASE()));
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    scene_->render(&p, QRectF(QPointF(0, 0), QSizeF(img.size())), src);
    p.end();
    return img;
}

void RelationshipMapScreen::export_png() {
    const QImage img = render_graph_image(2.0);
    if (img.isNull()) {
        status_->show_notice(tr("Nothing to export yet"));
        return;
    }
    const QString suggested = QDir::home().filePath(
        QStringLiteral("relationship_map_%1_%2.png").arg(loaded_ticker_, layout_key(layout_mode_)));
    const QString path = QFileDialog::getSaveFileName(this, tr("Export graph"), suggested, tr("PNG image (*.png)"));
    if (path.isEmpty())
        return;
    status_->show_notice(img.save(path, "PNG") ? tr("Saved %1").arg(QDir::toNativeSeparators(path))
                                               : tr("Could not save %1").arg(QDir::toNativeSeparators(path)));
}

void RelationshipMapScreen::export_csv(bool edges) {
    if (!has_data_) {
        status_->show_notice(tr("Nothing to export yet"));
        return;
    }
    QStringList lines;
    if (edges) {
        lines << QStringLiteral("from,to,group,directed,second_hop,pct_held,value,currency,change_pct,correlation");
        for (const auto& e : model_.edges)
            lines << QStringList{rmsd_csv(e.from), rmsd_csv(e.to), group_key(e.group),
                                 e.directed ? QStringLiteral("1") : QStringLiteral("0"),
                                 e.second_hop ? QStringLiteral("1") : QStringLiteral("0"), rmsd_num(e.pct),
                                 rmsd_num(e.value), e.currency, rmsd_num(e.change), rmsd_num(e.corr)}
                         .join(QLatin1Char(','));
    } else {
        lines << QStringLiteral("id,group,name,symbol,subtitle,pct_held,value,currency,change_pct,correlation,shares,date,note");
        for (const auto& n : model_.nodes) {
            if (n.kind == RelNodeKind::Placeholder)
                continue;
            lines << QStringList{rmsd_csv(n.id), n.kind == RelNodeKind::Center ? QStringLiteral("center") : group_key(n.group),
                                 rmsd_csv(n.label), n.symbol, rmsd_csv(n.subtitle), rmsd_num(n.pct), rmsd_num(n.value),
                                 n.value_currency, rmsd_num(n.change), rmsd_num(n.corr), rmsd_num(n.shares), n.date,
                                 rmsd_csv(n.note)}
                         .join(QLatin1Char(','));
        }
    }
    const QString suggested = QDir::home().filePath(
        QStringLiteral("relationship_map_%1_%2.csv").arg(loaded_ticker_, edges ? QStringLiteral("edges") : QStringLiteral("nodes")));
    const QString path = QFileDialog::getSaveFileName(this, tr("Export CSV"), suggested, tr("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QSaveFile f(path);
    bool ok = f.open(QIODevice::WriteOnly | QIODevice::Text);
    if (ok) {
        f.write(lines.join(QLatin1Char('\n')).toUtf8());
        f.write("\n");
        ok = f.commit();
    }
    status_->show_notice(ok ? tr("Saved %1").arg(QDir::toNativeSeparators(path))
                            : tr("Could not save %1").arg(QDir::toNativeSeparators(path)));
}

void RelationshipMapScreen::export_table_csv() {
    const QString csv = linked_->current_csv();
    if (csv.trimmed().isEmpty()) {
        status_->show_notice(tr("The table is empty"));
        return;
    }
    const QString suggested = QDir::home().filePath(
        QStringLiteral("relationship_map_%1_%2.csv").arg(loaded_ticker_, linked_->current_view()));
    const QString path = QFileDialog::getSaveFileName(this, tr("Export table"), suggested, tr("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QSaveFile f(path);
    bool ok = f.open(QIODevice::WriteOnly | QIODevice::Text);
    if (ok) {
        f.write(csv.toUtf8());
        ok = f.commit();
    }
    status_->show_notice(ok ? tr("Saved %1").arg(QDir::toNativeSeparators(path))
                            : tr("Could not save %1").arg(QDir::toNativeSeparators(path)));
}

} // namespace fincept::screens
