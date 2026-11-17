// src/services/relationship_map/RelationshipMapParse.cpp
#include "services/relationship_map/RelationshipMapParse.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::relmap {

namespace {

double rmp_num(const QJsonObject& o, const char* key) {
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isDouble()) {
        const double d = v.toDouble();
        return std::isfinite(d) ? d : 0.0;
    }
    if (v.isString()) {
        bool ok = false;
        const double d = v.toString().toDouble(&ok);
        return ok && std::isfinite(d) ? d : 0.0;
    }
    return 0.0;
}

/// NaN for a missing / null number (matrix cells, betas): "not computed" is not zero.
double rmp_num_or_nan(const QJsonValue& v) {
    if (v.isDouble() && std::isfinite(v.toDouble()))
        return v.toDouble();
    return std::numeric_limits<double>::quiet_NaN();
}

QString rmp_str(const QJsonObject& o, const char* key) {
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isString())
        return v.toString().trimmed();
    if (v.isDouble())
        return QString::number(v.toDouble(), 'g', 15);
    return {};
}

int rmp_int(const QJsonObject& o, const char* key) {
    return static_cast<int>(std::llround(rmp_num(o, key)));
}

bool rmp_has(const QJsonObject& o, const char* key) {
    return o.contains(QLatin1String(key));
}

QJsonArray rmp_arr(const QJsonObject& o, const char* key) {
    return o.value(QLatin1String(key)).toArray();
}

QJsonObject rmp_obj(const QJsonObject& o, const char* key) {
    return o.value(QLatin1String(key)).toObject();
}

void rmp_company(CompanyInfo& c, const QJsonObject& co) {
    c.ticker = rmp_str(co, "ticker").toUpper();
    c.name = rmp_str(co, "name");
    c.sector = rmp_str(co, "sector");
    c.industry = rmp_str(co, "industry");
    c.website = rmp_str(co, "website");
    c.description = rmp_str(co, "description");
    c.country = rmp_str(co, "country");
    c.exchange = rmp_str(co, "exchange");
    c.currency = rmp_str(co, "currency");
    c.financial_currency = rmp_str(co, "financial_currency");
    if (c.financial_currency.isEmpty())
        c.financial_currency = rmp_str(co, "financialCurrency");
    c.quote_type = rmp_str(co, "quote_type").toUpper();
    c.employees = rmp_int(co, "employees");
    c.market_cap = rmp_num(co, "market_cap");
    c.current_price = rmp_num(co, "current_price");
    c.previous_close = rmp_num(co, "previous_close");
    c.day_change_pct = rmp_num(co, "day_change_pct");
    c.pe_ratio = rmp_num(co, "pe_ratio");
    c.forward_pe = rmp_num(co, "forward_pe");
    c.price_to_book = rmp_num(co, "price_to_book");
    c.price_to_sales = rmp_num(co, "price_to_sales");
    c.roe = rmp_num(co, "roe");
    c.roa = rmp_num(co, "roa");
    c.revenue_growth = rmp_num(co, "revenue_growth");
    c.earnings_growth = rmp_num(co, "earnings_growth");
    c.profit_margins = rmp_num(co, "profit_margins");
    c.revenue = rmp_num(co, "revenue");
    c.ebitda = rmp_num(co, "ebitda");
    c.free_cashflow = rmp_num(co, "free_cashflow");
    c.operating_cashflow = rmp_num(co, "operating_cashflow");
    c.total_cash = rmp_num(co, "total_cash");
    c.total_debt = rmp_num(co, "total_debt");
    c.insider_percent = rmp_num(co, "insider_percent");
    c.institutional_percent = rmp_num(co, "institutional_percent");
    c.recommendation = rmp_str(co, "recommendation");
    c.recommendation_mean = rmp_num(co, "recommendation_mean");
    c.target_high = rmp_num(co, "target_high");
    c.target_low = rmp_num(co, "target_low");
    c.target_mean = rmp_num(co, "target_mean");
    c.target_median = rmp_num(co, "target_median");
    c.analyst_count = rmp_int(co, "analyst_count");
    c.dividend_yield = rmp_num(co, "dividend_yield");
    c.payout_ratio = rmp_num(co, "payout_ratio");
    c.trailing_eps = rmp_num(co, "trailing_eps");
    c.forward_eps = rmp_num(co, "forward_eps");
    c.shares_outstanding = rmp_num(co, "shares_outstanding");
    c.float_shares = rmp_num(co, "float_shares");
}

QVector<InstitutionalHolder> rmp_holders(const QJsonArray& a, const QString& type) {
    QVector<InstitutionalHolder> out;
    out.reserve(a.size());
    for (const auto& v : a) {
        const QJsonObject h = v.toObject();
        InstitutionalHolder x;
        x.name = rmp_str(h, "name");
        if (x.name.isEmpty())
            continue;
        x.shares = rmp_num(h, "shares");
        x.value = rmp_num(h, "value");
        x.percentage = rmp_num(h, "percentage");
        x.change_percent = rmp_num(h, "change_percent");
        x.date_reported = rmp_str(h, "date_reported").left(10);
        x.fund_family = rmp_str(h, "fund_family");
        x.type = type;
        out.append(x);
    }
    return out;
}

void rmp_correlation(CorrelationSet& c, const QJsonObject& o) {
    c = {};
    c.window = rmp_str(o, "window");
    c.interval = rmp_str(o, "interval");
    c.as_of = rmp_str(o, "as_of").left(10);
    c.observations = rmp_int(o, "observations");
    for (const auto& v : rmp_arr(o, "symbols"))
        c.symbols << v.toString().trimmed().toUpper();
    const QJsonObject labels = rmp_obj(o, "labels");
    for (auto it = labels.begin(); it != labels.end(); ++it)
        c.labels.insert(it.key().toUpper(), it.value().toString());
    const QJsonObject roles = rmp_obj(o, "roles");
    for (auto it = roles.begin(); it != roles.end(); ++it)
        c.roles.insert(it.key().toUpper(), it.value().toString());
    const QJsonArray rows = rmp_arr(o, "matrix");
    const int n = static_cast<int>(c.symbols.size());
    c.matrix.resize(n);
    for (int i = 0; i < n; ++i) {
        const QJsonArray row = rows.at(i).toArray();
        c.matrix[i].resize(n);
        for (int j = 0; j < n; ++j)
            c.matrix[i][j] = rmp_num_or_nan(row.at(j));
    }
    const QJsonObject rets = rmp_obj(o, "returns");
    for (auto it = rets.begin(); it != rets.end(); ++it)
        c.returns.insert(it.key().toUpper(), rmp_num_or_nan(it.value()));
    const QJsonObject betas = rmp_obj(o, "beta");
    for (auto it = betas.begin(); it != betas.end(); ++it)
        c.beta.insert(it.key().toUpper(), rmp_num_or_nan(it.value()));
    const QJsonObject spark = rmp_obj(o, "spark");
    for (auto it = spark.begin(); it != spark.end(); ++it) {
        QVector<double> pts;
        for (const auto& p : it.value().toArray())
            pts << rmp_num_or_nan(p);
        c.spark.insert(it.key().toUpper(), pts);
    }
    c.present = n >= 2;
}

void rmp_fund(FundProfile& f, const QJsonObject& o) {
    f = {};
    f.family = rmp_str(o, "family");
    f.category = rmp_str(o, "category");
    f.legal_type = rmp_str(o, "legal_type");
    f.total_assets = rmp_num(o, "total_assets");
    for (const auto& v : rmp_arr(o, "holdings")) {
        const QJsonObject h = v.toObject();
        FundHolding fh;
        fh.symbol = rmp_str(h, "symbol").toUpper();
        fh.name = rmp_str(h, "name");
        fh.weight = rmp_num(h, "weight");
        if (!fh.symbol.isEmpty() || !fh.name.isEmpty())
            f.holdings.append(fh);
    }
    for (const auto& v : rmp_arr(o, "sector_weights")) {
        const QJsonObject s = v.toObject();
        const QString key = rmp_str(s, "sector");
        if (!key.isEmpty())
            f.sector_weights.append({key, rmp_num(s, "weight")});
    }
    std::stable_sort(f.sector_weights.begin(), f.sector_weights.end(),
                     [](const auto& a, const auto& b) { return a.second > b.second; });
    const QJsonObject ac = rmp_obj(o, "asset_classes");
    for (auto it = ac.begin(); it != ac.end(); ++it)
        f.asset_classes.insert(it.key(), rmp_num_or_nan(it.value()));
    f.present = !f.holdings.isEmpty() || !f.sector_weights.isEmpty() || f.total_assets > 0 || !f.family.isEmpty();
}

/// The script's labels of a v1 partial result -> section id.
QString rmp_section_of_label(const QString& label) {
    const QString l = label.trimmed().toLower();
    if (l.startsWith(QLatin1String("peer ")))
        return QString::fromLatin1(section::kPeers);
    if (l == QLatin1String("institutional holders") || l == QLatin1String("fund holders"))
        return QString::fromLatin1(section::kHolders);
    if (l == QLatin1String("insiders") || l == QLatin1String("insider transactions"))
        return QString::fromLatin1(section::kInsiders);
    if (l == QLatin1String("analyst targets") || l == QLatin1String("recommendations") ||
        l == QLatin1String("upgrades/downgrades"))
        return QString::fromLatin1(section::kAnalysts);
    if (l == QLatin1String("calendar"))
        return QString::fromLatin1(section::kCalendar);
    if (l == QLatin1String("company profile"))
        return QString::fromLatin1(section::kProfile);
    return QString();
}

void rmp_status(RelSectionStatus& s, const QJsonObject& o) {
    s.state = section_state_from_text(rmp_str(o, "state"));
    s.source = rmp_str(o, "source");
    s.as_of = rmp_str(o, "as_of");
    s.ms = rmp_int(o, "ms");
    s.count = rmp_int(o, "count");
    s.note = rmp_str(o, "note");
}

} // namespace

// ── Section states ──────────────────────────────────────────────────────────

RelSectionState section_state_from_text(const QString& text) {
    const QString t = text.trimmed().toLower();
    if (t == QLatin1String("ok"))
        return RelSectionState::Ok;
    if (t == QLatin1String("empty"))
        return RelSectionState::Empty;
    if (t == QLatin1String("timeout"))
        return RelSectionState::Timeout;
    if (t == QLatin1String("skipped"))
        return RelSectionState::Skipped;
    if (t == QLatin1String("error"))
        return RelSectionState::Error;
    if (t == QLatin1String("unavailable"))
        return RelSectionState::Unavailable;
    if (t == QLatin1String("not_applicable"))
        return RelSectionState::NotApplicable;
    if (t == QLatin1String("loading"))
        return RelSectionState::Loading;
    return RelSectionState::Unknown;
}

QString section_state_text(RelSectionState s) {
    switch (s) {
        case RelSectionState::Ok:
            return QStringLiteral("ok");
        case RelSectionState::Empty:
            return QStringLiteral("empty");
        case RelSectionState::Timeout:
            return QStringLiteral("timeout");
        case RelSectionState::Skipped:
            return QStringLiteral("skipped");
        case RelSectionState::Error:
            return QStringLiteral("error");
        case RelSectionState::Unavailable:
            return QStringLiteral("unavailable");
        case RelSectionState::NotApplicable:
            return QStringLiteral("not_applicable");
        case RelSectionState::Loading:
            return QStringLiteral("loading");
        case RelSectionState::Unknown:
            break;
    }
    return QStringLiteral("unknown");
}

QStringList failed_sections(const RelationshipData& d) {
    QStringList out;
    for (auto it = d.sections.begin(); it != d.sections.end(); ++it)
        if (section_failed(it.value().state))
            out << it.key();
    std::sort(out.begin(), out.end());
    return out;
}

// ── Keys ────────────────────────────────────────────────────────────────────

void apply_payload_keys(RelationshipData& d, const QJsonObject& root) {
    if (rmp_has(root, "schema"))
        d.schema = std::max(1, rmp_int(root, "schema"));
    if (rmp_has(root, "fetched_at")) {
        const QDateTime t = QDateTime::fromString(rmp_str(root, "fetched_at"), Qt::ISODate);
        if (t.isValid())
            d.fetched_at = t.toUTC();
    }
    if (rmp_has(root, "company"))
        rmp_company(d.company, rmp_obj(root, "company"));
    if (rmp_has(root, "governance")) {
        const QJsonObject g = rmp_obj(root, "governance");
        d.governance.audit_risk = rmp_int(g, "audit_risk");
        d.governance.board_risk = rmp_int(g, "board_risk");
        d.governance.compensation_risk = rmp_int(g, "compensation_risk");
        d.governance.shareholder_rights_risk = rmp_int(g, "shareholder_rights_risk");
        d.governance.overall_risk = rmp_int(g, "overall_risk");
    }
    if (rmp_has(root, "technicals")) {
        const QJsonObject t = rmp_obj(root, "technicals");
        d.technicals.fifty_two_week_high = rmp_num(t, "fifty_two_week_high");
        d.technicals.fifty_two_week_low = rmp_num(t, "fifty_two_week_low");
        d.technicals.fifty_day_avg = rmp_num(t, "fifty_day_avg");
        d.technicals.two_hundred_day_avg = rmp_num(t, "two_hundred_day_avg");
        d.technicals.beta = rmp_num(t, "beta");
        d.technicals.week52_change_pct = rmp_num(t, "week52_change_pct");
        d.technicals.sp500_52wk_change = rmp_num(t, "sp500_52wk_change");
        d.technicals.avg_volume = rmp_num(t, "avg_volume");
        d.technicals.avg_volume_10d = rmp_num(t, "avg_volume_10d");
    }
    if (rmp_has(root, "short_interest")) {
        const QJsonObject s = rmp_obj(root, "short_interest");
        d.short_interest.shares_short = rmp_num(s, "shares_short");
        d.short_interest.short_ratio = rmp_num(s, "short_ratio");
        d.short_interest.short_pct_float = rmp_num(s, "short_pct_float");
        d.short_interest.float_shares = rmp_num(s, "float_shares");
    }
    if (rmp_has(root, "enterprise")) {
        const QJsonObject e = rmp_obj(root, "enterprise");
        d.enterprise.enterprise_value = rmp_num(e, "enterprise_value");
        d.enterprise.ev_to_revenue = rmp_num(e, "ev_to_revenue");
        d.enterprise.ev_to_ebitda = rmp_num(e, "ev_to_ebitda");
        d.enterprise.peg_ratio = rmp_num(e, "peg_ratio");
        d.enterprise.price_to_sales = rmp_num(e, "price_to_sales");
        d.enterprise.book_value = rmp_num(e, "book_value");
    }
    if (rmp_has(root, "margins")) {
        const QJsonObject m = rmp_obj(root, "margins");
        d.margins.gross = rmp_num(m, "gross");
        d.margins.operating = rmp_num(m, "operating");
        d.margins.ebitda = rmp_num(m, "ebitda");
        d.margins.net = rmp_num(m, "net");
        d.margins.debt_to_equity = rmp_num(m, "debt_to_equity");
        d.margins.current_ratio = rmp_num(m, "current_ratio");
        d.margins.quick_ratio = rmp_num(m, "quick_ratio");
    }
    if (rmp_has(root, "analyst_targets")) {
        const QJsonObject a = rmp_obj(root, "analyst_targets");
        d.analyst_targets.current = rmp_num(a, "current");
        d.analyst_targets.high = rmp_num(a, "high");
        d.analyst_targets.low = rmp_num(a, "low");
        d.analyst_targets.mean = rmp_num(a, "mean");
        d.analyst_targets.median = rmp_num(a, "median");
    }
    if (rmp_has(root, "recommendations_summary")) {
        d.recommendations.clear();
        for (const auto& v : rmp_arr(root, "recommendations_summary")) {
            const QJsonObject r = v.toObject();
            RecommendationSnapshot s;
            s.period = rmp_str(r, "period");
            s.strong_buy = rmp_int(r, "strong_buy");
            s.buy = rmp_int(r, "buy");
            s.hold = rmp_int(r, "hold");
            s.sell = rmp_int(r, "sell");
            s.strong_sell = rmp_int(r, "strong_sell");
            d.recommendations.append(s);
        }
    }
    if (rmp_has(root, "upgrades_downgrades")) {
        d.upgrades_downgrades.clear();
        for (const auto& v : rmp_arr(root, "upgrades_downgrades")) {
            const QJsonObject u = v.toObject();
            AnalystUpgrade g;
            g.date = rmp_str(u, "date").left(10);
            g.firm = rmp_str(u, "firm");
            g.to_grade = rmp_str(u, "to_grade");
            g.from_grade = rmp_str(u, "from_grade");
            g.action = rmp_str(u, "action");
            g.price_target = rmp_num(u, "price_target");
            g.prior_target = rmp_num(u, "prior_target");
            d.upgrades_downgrades.append(g);
        }
    }
    if (rmp_has(root, "officers")) {
        d.officers.clear();
        for (const auto& v : rmp_arr(root, "officers")) {
            const QJsonObject o = v.toObject();
            CompanyOfficer off;
            off.name = rmp_str(o, "name").simplified();
            off.title = rmp_str(o, "title");
            off.total_pay = rmp_num(o, "total_pay");
            off.year_born = rmp_int(o, "year_born");
            if (!off.name.isEmpty())
                d.officers.append(off);
        }
    }
    if (rmp_has(root, "calendar")) {
        const QJsonObject c = rmp_obj(root, "calendar");
        d.calendar.earnings_date = rmp_str(c, "earnings_date").left(10);
        d.calendar.earnings_avg = rmp_num(c, "earnings_avg");
        d.calendar.earnings_low = rmp_num(c, "earnings_low");
        d.calendar.earnings_high = rmp_num(c, "earnings_high");
        d.calendar.revenue_avg = rmp_num(c, "revenue_avg");
        d.calendar.revenue_low = rmp_num(c, "revenue_low");
        d.calendar.revenue_high = rmp_num(c, "revenue_high");
        d.calendar.ex_dividend_date = rmp_str(c, "ex_dividend_date").left(10);
        d.calendar.dividend_date = rmp_str(c, "dividend_date").left(10);
    }
    if (rmp_has(root, "major_holders")) {
        const QJsonObject m = rmp_obj(root, "major_holders");
        d.major_holders.insiders_pct = rmp_num(m, "insiders_pct");
        d.major_holders.institutions_pct = rmp_num(m, "institutions_pct");
        d.major_holders.institutions_float_pct = rmp_num(m, "institutions_float_pct");
        d.major_holders.institutions_count = rmp_int(m, "institutions_count");
        d.major_holders.present = d.major_holders.institutions_pct > 0 || d.major_holders.insiders_pct > 0 ||
                                  d.major_holders.institutions_count > 0;
    }
    if (rmp_has(root, "institutional_holders"))
        d.institutional_holders = rmp_holders(rmp_arr(root, "institutional_holders"), QStringLiteral("institutional"));
    if (rmp_has(root, "mutualfund_holders"))
        d.mutualfund_holders = rmp_holders(rmp_arr(root, "mutualfund_holders"), QStringLiteral("mutualfund"));
    if (rmp_has(root, "insider_holders")) {
        d.insider_holders.clear();
        for (const auto& v : rmp_arr(root, "insider_holders")) {
            const QJsonObject h = v.toObject();
            InsiderHolder i;
            i.name = rmp_str(h, "name");
            if (i.name.isEmpty() || i.name.compare(QLatin1String("nan"), Qt::CaseInsensitive) == 0)
                continue;
            i.title = rmp_str(h, "title");
            i.shares = rmp_num(h, "shares");
            i.percentage = rmp_num(h, "percentage");
            i.last_transaction = rmp_str(h, "last_transaction");
            i.last_transaction_text = rmp_str(h, "last_transaction_text");
            i.last_date = rmp_str(h, "last_date").left(10);
            d.insider_holders.append(i);
        }
    }
    if (rmp_has(root, "insider_transactions")) {
        d.insider_trades.clear();
        for (const auto& v : rmp_arr(root, "insider_transactions")) {
            const QJsonObject t = v.toObject();
            RelInsiderTrade x;
            x.date = rmp_str(t, "date").left(10);
            x.insider = rmp_str(t, "insider");
            x.position = rmp_str(t, "position");
            x.text = rmp_str(t, "text");
            x.kind = rmp_str(t, "kind").toLower();
            x.shares = rmp_num(t, "shares");
            x.value = rmp_num(t, "value");
            x.price = rmp_num(t, "price");
            x.currency = rmp_str(t, "currency");
            x.ownership = rmp_str(t, "ownership");
            if (!x.date.isEmpty())
                d.insider_trades.append(x);
        }
        std::stable_sort(d.insider_trades.begin(), d.insider_trades.end(),
                         [](const RelInsiderTrade& a, const RelInsiderTrade& b) { return a.date > b.date; });
    }
    if (rmp_has(root, "insider_summary")) {
        const QJsonObject s = rmp_obj(root, "insider_summary");
        RelInsiderSummary& m = d.insider_summary;
        m.window = rmp_str(s, "window");
        m.purchases_shares = rmp_num(s, "purchases_shares");
        m.purchases_count = rmp_int(s, "purchases_count");
        m.sales_shares = rmp_num(s, "sales_shares");
        m.sales_count = rmp_int(s, "sales_count");
        m.net_shares = rmp_num(s, "net_shares");
        m.net_pct = rmp_num(s, "net_pct");
        m.total_insider_shares = rmp_num(s, "total_insider_shares");
        m.buy_value = rmp_num(s, "buy_value");
        m.sell_value = rmp_num(s, "sell_value");
        m.currency = rmp_str(s, "currency");
        m.present = !s.isEmpty();
    }
    if (rmp_has(root, "peers")) {
        d.peers.clear();
        for (const auto& v : rmp_arr(root, "peers")) {
            const QJsonObject p = v.toObject();
            PeerCompany peer;
            peer.ticker = rmp_str(p, "ticker").toUpper();
            if (peer.ticker.isEmpty())
                continue;
            peer.name = rmp_str(p, "name");
            peer.market_cap = rmp_num(p, "market_cap");
            peer.pe_ratio = rmp_num(p, "pe_ratio");
            peer.forward_pe = rmp_num(p, "forward_pe");
            peer.roe = rmp_num(p, "roe");
            peer.revenue_growth = rmp_num(p, "revenue_growth");
            peer.profit_margins = rmp_num(p, "profit_margins");
            peer.gross_margins = rmp_num(p, "gross_margins");
            peer.operating_margins = rmp_num(p, "operating_margins");
            peer.revenue = rmp_num(p, "revenue");
            peer.current_price = rmp_num(p, "current_price");
            peer.sector = rmp_str(p, "sector");
            peer.industry = rmp_str(p, "industry");
            peer.country = rmp_str(p, "country");
            peer.beta = rmp_num(p, "beta");
            peer.ev_to_ebitda = rmp_num(p, "ev_to_ebitda");
            peer.price_to_book = rmp_num(p, "price_to_book");
            peer.week52_change = rmp_num(p, "week52_change");
            peer.recommendation = rmp_str(p, "recommendation");
            peer.currency = rmp_str(p, "currency");
            peer.financial_currency = rmp_str(p, "financial_currency");
            d.peers.append(peer);
        }
    }
    if (rmp_has(root, "peer_source"))
        d.peer_source = rmp_str(root, "peer_source");
    if (rmp_has(root, "correlation"))
        rmp_correlation(d.correlation, rmp_obj(root, "correlation"));
    if (rmp_has(root, "fund"))
        rmp_fund(d.fund, rmp_obj(root, "fund"));
    if (rmp_has(root, "subsidiaries")) {
        d.subsidiaries.clear();
        for (const auto& v : rmp_arr(root, "subsidiaries")) {
            const QJsonObject s = v.toObject();
            SubsidiaryEntry e;
            e.name = rmp_str(s, "name").simplified();
            e.jurisdiction = rmp_str(s, "jurisdiction").simplified();
            if (!e.name.isEmpty())
                d.subsidiaries.append(e);
        }
    }
    if (rmp_has(root, "subsidiaries_filing")) {
        const QJsonObject f = rmp_obj(root, "subsidiaries_filing");
        d.subsidiaries_filing.form = rmp_str(f, "form");
        d.subsidiaries_filing.filed = rmp_str(f, "filed").left(10);
        d.subsidiaries_filing.accession = rmp_str(f, "accession");
        d.subsidiaries_filing.url = rmp_str(f, "url");
    }
    if (rmp_has(root, "supply_chain")) {
        d.supply_chain.clear();
        for (const auto& v : rmp_arr(root, "supply_chain")) {
            const QJsonObject s = v.toObject();
            SupplyChainEntity e;
            e.name = rmp_str(s, "name");
            e.relationship = rmp_str(s, "relationship");
            e.revenue_pct = rmp_num(s, "revenue_pct");
            if (!e.name.isEmpty())
                d.supply_chain.append(e);
        }
    }
    if (rmp_has(root, "sections")) {
        const QJsonObject secs = rmp_obj(root, "sections");
        for (auto it = secs.begin(); it != secs.end(); ++it) {
            RelSectionStatus s;
            rmp_status(s, it.value().toObject());
            d.sections.insert(it.key(), s);
        }
    }
    if (rmp_has(root, "data_quality"))
        d.data_quality = rmp_int(root, "data_quality");
    if (rmp_has(root, "partial"))
        d.partial = root.value(QLatin1String("partial")).toBool();
    if (rmp_has(root, "note"))
        d.note = rmp_str(root, "note").simplified();
    if (rmp_has(root, "missing_sections")) {
        d.missing_sections.clear();
        for (const auto& v : rmp_arr(root, "missing_sections")) {
            const QString s = v.toString().trimmed();
            if (!s.isEmpty() && !d.missing_sections.contains(s))
                d.missing_sections << s;
        }
    }
    if (rmp_has(root, "elapsed_s"))
        d.elapsed_s = rmp_num(root, "elapsed_s");
}

void finalize_data(RelationshipData& d) {
    // Float: the profile's when the short-interest block (v1) had none.
    if (d.company.float_shares <= 0 && d.short_interest.float_shares > 0)
        d.company.float_shares = d.short_interest.float_shares;
    // Ownership split: Yahoo's major-holders block, else the profile's fractions (v1).
    if (!d.major_holders.present &&
        (d.company.institutional_percent > 0 || d.company.insider_percent > 0)) {
        d.major_holders.institutions_pct = d.company.institutional_percent;
        d.major_holders.insiders_pct = d.company.insider_percent;
        d.major_holders.present = true;
    }

    // A v1 payload has no section states: derive them from what is there and from the
    // script's "missing" labels. Supply chain never has a free source.
    auto derive = [&d](const char* id, bool has_rows) {
        const QString key = QString::fromLatin1(id);
        if (d.sections.contains(key))
            return;
        RelSectionStatus s;
        s.state = has_rows ? RelSectionState::Ok : RelSectionState::Unknown;
        s.source = QStringLiteral("Yahoo Finance");
        d.sections.insert(key, s);
    };
    if (d.schema < 2) {
        derive(section::kProfile, d.company.current_price > 0 || d.company.market_cap > 0);
        derive(section::kHolders, !d.institutional_holders.isEmpty() || !d.mutualfund_holders.isEmpty());
        derive(section::kInsiders, !d.insider_holders.isEmpty());
        derive(section::kManagement, !d.officers.isEmpty());
        derive(section::kAnalysts, d.company.analyst_count > 0 || !d.recommendations.isEmpty());
        derive(section::kCalendar, !d.calendar.earnings_date.isEmpty());
        derive(section::kPeers, !d.peers.isEmpty());
        for (const QString& label : std::as_const(d.missing_sections)) {
            const QString id = rmp_section_of_label(label);
            if (id.isEmpty())
                continue;
            RelSectionStatus& s = d.sections[id];
            if (s.state != RelSectionState::Ok || id == QLatin1String(section::kPeers))
                s.state = RelSectionState::Timeout;
            s.note = label;
        }
    }
    {
        RelSectionStatus& sc = d.sections[QString::fromLatin1(section::kSupplyChain)];
        if (sc.state == RelSectionState::Unknown && d.supply_chain.isEmpty()) {
            sc.state = RelSectionState::Unavailable;
            if (sc.note.isEmpty())
                sc.note = QStringLiteral("Customers and suppliers are not available from free sources");
        }
    }
    // Row counts the UI shows next to each section.
    auto count = [&d](const char* id, int n) {
        RelSectionStatus& s = d.sections[QString::fromLatin1(id)];
        if (s.count == 0)
            s.count = n;
    };
    count(section::kHolders, static_cast<int>(d.institutional_holders.size() + d.mutualfund_holders.size()));
    count(section::kInsiders, static_cast<int>(d.insider_holders.size()));
    count(section::kManagement, static_cast<int>(d.officers.size()));
    count(section::kPeers, static_cast<int>(d.peers.size()));
    count(section::kSubsidiaries, static_cast<int>(d.subsidiaries.size()));
    count(section::kFund, static_cast<int>(d.fund.holdings.size()));

    // The earnings / dividend calendar as dated events (the only corporate events the data has).
    d.events.clear();
    auto add_event = [&d](const QString& date, const QString& what) {
        if (date.trimmed().isEmpty())
            return;
        d.events.append({date.trimmed(), QStringLiteral("calendar"), what, QStringLiteral("calendar")});
    };
    add_event(d.calendar.earnings_date, QStringLiteral("Earnings"));
    add_event(d.calendar.ex_dividend_date, QStringLiteral("Ex-dividend"));
    add_event(d.calendar.dividend_date, QStringLiteral("Dividend paid"));

    if (d.peer_source == QLatin1String("generic_fallback")) {
        // Mega-cap fallback: a P/E comparison against that set says nothing about this company.
        d.valuation = {};
        d.valuation.status = QStringLiteral("N/A — no comparable peer set");
        d.valuation.action = QStringLiteral("HOLD");
    } else {
        d.valuation = compute_valuation(d.company, d.peers);
    }

    if (!d.partial && (!d.missing_sections.isEmpty() || !failed_sections(d).isEmpty()))
        d.partial = true;
}

RelParseOutcome parse_payload(const QByteArray& json, const QString& requested_ticker) {
    RelParseOutcome out;
    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(json.trimmed(), &perr);
    if (!doc.isObject()) {
        out.error = QStringLiteral("The relationship data could not be read (invalid reply).");
        return out;
    }
    const QJsonObject root = doc.object();
    if (root.contains(QLatin1String("error"))) {
        const QString e = root.value(QLatin1String("error")).toString().simplified();
        out.error = e.isEmpty() ? QStringLiteral("The relationship data could not be loaded.") : e;
        return out;
    }
    RelationshipData d;
    apply_payload_keys(d, root);
    if (d.company.ticker.isEmpty())
        d.company.ticker = requested_ticker.trimmed().toUpper();
    // yfinance answers an unknown / delisted symbol with an empty record rather than an
    // error. No price and no market cap means there is nothing to show.
    if (d.company.current_price <= 0 && d.company.market_cap <= 0) {
        out.error = QStringLiteral("No market data found for %1 - check the symbol").arg(d.company.ticker);
        return out;
    }
    finalize_data(d);
    d.timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    out.ok = true;
    out.data = std::move(d);
    return out;
}

bool parse_stream_line(const QString& line, RelStreamEvent* out) {
    static const QString kMark = QStringLiteral("@@RELMAP ");
    const qsizetype at = line.indexOf(kMark);
    if (at < 0 || !out)
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(line.mid(at + kMark.size()).toUtf8());
    if (!doc.isObject())
        return false;
    const QJsonObject o = doc.object();
    RelStreamEvent ev;
    ev.kind = rmp_str(o, "event");
    ev.ticker = rmp_str(o, "ticker").toUpper();
    ev.id = rmp_str(o, "id");
    for (const auto& v : rmp_arr(o, "sections"))
        ev.planned << v.toString();
    rmp_status(ev.status, rmp_obj(o, "status"));
    ev.data = rmp_obj(o, "data");
    if (ev.kind != QLatin1String("plan") && ev.kind != QLatin1String("section"))
        return false;
    *out = std::move(ev);
    return true;
}

void apply_stream_event(RelationshipData& d, const RelStreamEvent& ev) {
    if (ev.kind == QLatin1String("plan")) {
        for (const QString& id : ev.planned) {
            RelSectionStatus& s = d.sections[id];
            if (s.state == RelSectionState::Unknown)
                s.state = RelSectionState::Loading;
        }
        if (d.company.ticker.isEmpty())
            d.company.ticker = ev.ticker;
        d.schema = std::max(d.schema, 2);
        return;
    }
    apply_payload_keys(d, ev.data);
    if (!ev.id.isEmpty())
        d.sections.insert(ev.id, ev.status);
}

QStringList section_keys(const QString& id) {
    static const QHash<QString, QStringList> kKeys = {
        {QString::fromLatin1(section::kProfile),
         {QStringLiteral("company"), QStringLiteral("governance"), QStringLiteral("technicals"),
          QStringLiteral("short_interest"), QStringLiteral("enterprise"), QStringLiteral("margins")}},
        {QString::fromLatin1(section::kHolders),
         {QStringLiteral("major_holders"), QStringLiteral("institutional_holders"), QStringLiteral("mutualfund_holders")}},
        {QString::fromLatin1(section::kInsiders),
         {QStringLiteral("insider_holders"), QStringLiteral("insider_transactions"), QStringLiteral("insider_summary")}},
        {QString::fromLatin1(section::kManagement), {QStringLiteral("officers")}},
        {QString::fromLatin1(section::kAnalysts),
         {QStringLiteral("analyst_targets"), QStringLiteral("recommendations_summary"),
          QStringLiteral("upgrades_downgrades")}},
        {QString::fromLatin1(section::kCalendar), {QStringLiteral("calendar")}},
        {QString::fromLatin1(section::kPeers), {QStringLiteral("peers"), QStringLiteral("peer_source")}},
        {QString::fromLatin1(section::kCorrelation), {QStringLiteral("correlation")}},
        {QString::fromLatin1(section::kFund), {QStringLiteral("fund")}},
        {QString::fromLatin1(section::kSubsidiaries),
         {QStringLiteral("subsidiaries"), QStringLiteral("subsidiaries_filing")}},
        {QString::fromLatin1(section::kSupplyChain), {QStringLiteral("supply_chain")}},
    };
    return kKeys.value(id);
}

QJsonObject merge_retry_json(const QJsonObject& base, const QJsonObject& retry, const QStringList& ids) {
    QJsonObject out = base;
    QJsonObject sections = base.value(QLatin1String("sections")).toObject();
    const QJsonObject retry_sections = retry.value(QLatin1String("sections")).toObject();
    for (const QString& id : ids) {
        const QJsonObject st = retry_sections.value(id).toObject();
        const RelSectionState state = section_state_from_text(st.value(QLatin1String("state")).toString());
        if (!st.isEmpty())
            sections.insert(id, st);
        if (state != RelSectionState::Ok && state != RelSectionState::Empty)
            continue; // still failing: the base's data (if any) stays, with the new status
        for (const QString& key : section_keys(id))
            if (retry.contains(key))
                out.insert(key, retry.value(key));
    }
    out.insert(QStringLiteral("sections"), sections);
    // Partial again only if a section still failed; the old labels go with the sections retried.
    QStringList failed;
    for (auto it = sections.begin(); it != sections.end(); ++it)
        if (section_failed(section_state_from_text(it.value().toObject().value(QLatin1String("state")).toString())))
            failed << it.key();
    std::sort(failed.begin(), failed.end());
    QJsonArray missing;
    for (const auto& v : base.value(QLatin1String("missing_sections")).toArray()) {
        const QString label = v.toString();
        const QString sec = rmp_section_of_label(label);
        if (!ids.contains(sec) && !ids.contains(label))
            missing.append(label);
    }
    for (const auto& v : retry.value(QLatin1String("missing_sections")).toArray())
        missing.append(v);
    out.insert(QStringLiteral("missing_sections"), missing);
    out.insert(QStringLiteral("partial"), !failed.isEmpty() || !missing.isEmpty());
    if (failed.isEmpty() && missing.isEmpty())
        out.remove(QStringLiteral("note"));
    else
        out.insert(QStringLiteral("note"),
                   QStringLiteral("Some sections did not load: %1").arg(failed.join(QStringLiteral(", "))));
    return out;
}

// ── Relative value ──────────────────────────────────────────────────────────

ValuationSignal compute_valuation(const CompanyInfo& co, const QVector<PeerCompany>& peers) {
    ValuationSignal sig;
    sig.status = QStringLiteral("INSUFFICIENT DATA");
    sig.action = QStringLiteral("HOLD");
    if (peers.isEmpty() || co.pe_ratio <= 0)
        return sig;
    // Only peers quoted in the company's currency: a P/E across markets / currencies is not
    // a like-for-like comparison.
    const QString co_ccy = co.currency.trimmed().toUpper();
    QVector<double> pes;
    for (const auto& p : peers) {
        const QString p_ccy = p.currency.trimmed().toUpper();
        if (p.pe_ratio > 0 && (p_ccy.isEmpty() || co_ccy.isEmpty() || p_ccy == co_ccy))
            pes.append(p.pe_ratio);
    }
    if (pes.isEmpty())
        return sig;
    std::sort(pes.begin(), pes.end());
    const qsizetype n = pes.size();
    const double median = (n % 2 == 1) ? pes[n / 2] : (pes[n / 2 - 1] + pes[n / 2]) / 2.0;
    const double rel = co.pe_ratio / median;
    sig.pe_vs_peers = rel;
    sig.peer_count = static_cast<int>(n);
    double score = 0;
    if (rel < 0.7)
        score += 2;
    else if (rel < 0.85)
        score += 1;
    else if (rel > 1.3)
        score -= 2;
    else if (rel > 1.15)
        score -= 1;
    if (co.revenue_growth > 0.2)
        score += 1;
    else if (co.revenue_growth < -0.05)
        score -= 1;
    if (co.profit_margins > 0.2)
        score += 0.5;
    sig.score = std::max(-3.0, std::min(3.0, score));
    if (sig.score >= 1.5) {
        sig.status = QStringLiteral("UNDERVALUED");
        sig.action = QStringLiteral("BUY");
    } else if (sig.score >= 0.5) {
        sig.status = QStringLiteral("POTENTIALLY UNDERVALUED");
        sig.action = QStringLiteral("BUY");
    } else if (sig.score <= -1.5) {
        sig.status = QStringLiteral("OVERVALUED");
        sig.action = QStringLiteral("SELL");
    } else if (sig.score <= -0.5) {
        sig.status = QStringLiteral("POTENTIALLY OVERVALUED");
        sig.action = QStringLiteral("SELL");
    } else {
        sig.status = QStringLiteral("FAIRLY VALUED");
        sig.action = QStringLiteral("HOLD");
    }
    return sig;
}

} // namespace fincept::relmap
