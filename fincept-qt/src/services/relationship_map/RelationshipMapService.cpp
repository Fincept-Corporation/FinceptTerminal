// src/services/relationship_map/RelationshipMapService.cpp
#include "services/relationship_map/RelationshipMapService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace fincept::services {

using namespace fincept::relmap;

RelationshipMapService& RelationshipMapService::instance() {
    static RelationshipMapService s;
    return s;
}

static constexpr int kRelMapTtlSec = 10 * 60; // 10 min

void RelationshipMapService::fetch(const QString& ticker) {
    if (loading_)
        return;
    if (ticker.trimmed().isEmpty())
        return;

    current_ticker_ = ticker.toUpper();

    // Check cache first
    const QString cache_key = "relmap:" + current_ticker_;
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        LOG_DEBUG("RelMapService", "Cache hit for " + current_ticker_);
        emit progress_changed(100, "Loaded from cache");
        parse_result(cached.toString());
        return;
    }

    loading_ = true;
    data_ = {};

    emit progress_changed(5, "Starting data fetch for " + current_ticker_ + "...");

    python::PythonRunner::instance().run(
        "relationship_map.py", {current_ticker_}, [this, cache_key](python::PythonResult result) {
            loading_ = false;

            if (!result.success || result.output.trimmed().isEmpty()) {
                LOG_ERROR("RelMapService", "Python script failed: exit=" + QString::number(result.exit_code));
                emit fetch_failed("Failed to fetch data for " + current_ticker_);
                return;
            }

            // Cache raw JSON output
            fincept::CacheManager::instance().put(cache_key, QVariant(result.output), kRelMapTtlSec, "relmap");

            emit progress_changed(70, "Parsing results...");
            parse_result(result.output);
        });
}

void RelationshipMapService::clear() {
    data_ = {};
    loading_ = false;
    current_ticker_.clear();
}

void RelationshipMapService::parse_result(const QString& json_output) {
    QJsonDocument doc = QJsonDocument::fromJson(json_output.toUtf8());
    if (!doc.isObject()) {
        emit fetch_failed("Invalid JSON response");
        return;
    }

    QJsonObject root = doc.object();
    if (root.contains("error")) {
        emit fetch_failed(root["error"].toString());
        return;
    }

    emit progress_changed(80, "Building graph data...");

    // ── Company ──────────────────────────────────────────────────────────
    QJsonObject co = root["company"].toObject();
    data_.company.ticker               = co["ticker"].toString();
    data_.company.name                 = co["name"].toString();
    data_.company.sector               = co["sector"].toString();
    data_.company.industry             = co["industry"].toString();
    data_.company.website              = co["website"].toString();
    data_.company.description          = co["description"].toString();
    data_.company.country              = co["country"].toString();
    data_.company.exchange             = co["exchange"].toString();
    data_.company.currency             = co["currency"].toString();
    data_.company.employees            = co["employees"].toInt();
    data_.company.market_cap           = co["market_cap"].toDouble();
    data_.company.current_price        = co["current_price"].toDouble();
    data_.company.previous_close       = co["previous_close"].toDouble();
    data_.company.day_change_pct       = co["day_change_pct"].toDouble();
    data_.company.pe_ratio             = co["pe_ratio"].toDouble();
    data_.company.forward_pe           = co["forward_pe"].toDouble();
    data_.company.price_to_book        = co["price_to_book"].toDouble();
    data_.company.roe                  = co["roe"].toDouble();
    data_.company.roa                  = co["roa"].toDouble();
    data_.company.revenue_growth       = co["revenue_growth"].toDouble();
    data_.company.earnings_growth      = co["earnings_growth"].toDouble();
    data_.company.profit_margins       = co["profit_margins"].toDouble();
    data_.company.revenue              = co["revenue"].toDouble();
    data_.company.ebitda               = co["ebitda"].toDouble();
    data_.company.free_cashflow        = co["free_cashflow"].toDouble();
    data_.company.operating_cashflow   = co["operating_cashflow"].toDouble();
    data_.company.total_cash           = co["total_cash"].toDouble();
    data_.company.total_debt           = co["total_debt"].toDouble();
    data_.company.insider_percent      = co["insider_percent"].toDouble();
    data_.company.institutional_percent= co["institutional_percent"].toDouble();
    data_.company.recommendation       = co["recommendation"].toString();
    data_.company.recommendation_mean  = co["recommendation_mean"].toDouble();
    data_.company.target_high          = co["target_high"].toDouble();
    data_.company.target_low           = co["target_low"].toDouble();
    data_.company.target_mean          = co["target_mean"].toDouble();
    data_.company.target_median        = co["target_median"].toDouble();
    data_.company.analyst_count        = co["analyst_count"].toInt();
    data_.company.dividend_yield       = co["dividend_yield"].toDouble();
    data_.company.payout_ratio         = co["payout_ratio"].toDouble();
    data_.company.trailing_eps         = co["trailing_eps"].toDouble();
    data_.company.forward_eps          = co["forward_eps"].toDouble();
    data_.company.shares_outstanding   = co["shares_outstanding"].toDouble();

    // ── Governance ───────────────────────────────────────────────────────
    QJsonObject gov = root["governance"].toObject();
    data_.governance.audit_risk               = gov["audit_risk"].toInt();
    data_.governance.board_risk               = gov["board_risk"].toInt();
    data_.governance.compensation_risk        = gov["compensation_risk"].toInt();
    data_.governance.shareholder_rights_risk  = gov["shareholder_rights_risk"].toInt();
    data_.governance.overall_risk             = gov["overall_risk"].toInt();

    // ── Technicals ───────────────────────────────────────────────────────
    QJsonObject tech = root["technicals"].toObject();
    data_.technicals.fifty_two_week_high   = tech["fifty_two_week_high"].toDouble();
    data_.technicals.fifty_two_week_low    = tech["fifty_two_week_low"].toDouble();
    data_.technicals.fifty_day_avg         = tech["fifty_day_avg"].toDouble();
    data_.technicals.two_hundred_day_avg   = tech["two_hundred_day_avg"].toDouble();
    data_.technicals.beta                  = tech["beta"].toDouble();
    data_.technicals.week52_change_pct     = tech["week52_change_pct"].toDouble();
    data_.technicals.sp500_52wk_change     = tech["sp500_52wk_change"].toDouble();
    data_.technicals.avg_volume            = tech["avg_volume"].toInt();
    data_.technicals.avg_volume_10d        = tech["avg_volume_10d"].toInt();

    // ── Short Interest ────────────────────────────────────────────────────
    QJsonObject si = root["short_interest"].toObject();
    data_.short_interest.shares_short     = si["shares_short"].toDouble();
    data_.short_interest.short_ratio      = si["short_ratio"].toDouble();
    data_.short_interest.short_pct_float  = si["short_pct_float"].toDouble();
    data_.short_interest.float_shares     = si["float_shares"].toDouble();

    // ── Enterprise ────────────────────────────────────────────────────────
    QJsonObject ent = root["enterprise"].toObject();
    data_.enterprise.enterprise_value = ent["enterprise_value"].toDouble();
    data_.enterprise.ev_to_revenue    = ent["ev_to_revenue"].toDouble();
    data_.enterprise.ev_to_ebitda     = ent["ev_to_ebitda"].toDouble();
    data_.enterprise.peg_ratio        = ent["peg_ratio"].toDouble();
    data_.enterprise.price_to_sales   = ent["price_to_sales"].toDouble();
    data_.enterprise.book_value       = ent["book_value"].toDouble();

    // ── Margins & Debt ────────────────────────────────────────────────────
    QJsonObject mg = root["margins"].toObject();
    data_.margins.gross          = mg["gross"].toDouble();
    data_.margins.operating      = mg["operating"].toDouble();
    data_.margins.ebitda         = mg["ebitda"].toDouble();
    data_.margins.net            = mg["net"].toDouble();
    data_.margins.debt_to_equity = mg["debt_to_equity"].toDouble();
    data_.margins.current_ratio  = mg["current_ratio"].toDouble();
    data_.margins.quick_ratio    = mg["quick_ratio"].toDouble();

    // ── Analyst Targets ───────────────────────────────────────────────────
    QJsonObject at = root["analyst_targets"].toObject();
    data_.analyst_targets.current = at["current"].toDouble();
    data_.analyst_targets.high    = at["high"].toDouble();
    data_.analyst_targets.low     = at["low"].toDouble();
    data_.analyst_targets.mean    = at["mean"].toDouble();
    data_.analyst_targets.median  = at["median"].toDouble();

    // ── Recommendations Summary ───────────────────────────────────────────
    for (const auto& v : root["recommendations_summary"].toArray()) {
        QJsonObject r = v.toObject();
        RecommendationSnapshot snap;
        snap.period      = r["period"].toString();
        snap.strong_buy  = r["strong_buy"].toInt();
        snap.buy         = r["buy"].toInt();
        snap.hold        = r["hold"].toInt();
        snap.sell        = r["sell"].toInt();
        snap.strong_sell = r["strong_sell"].toInt();
        data_.recommendations.append(snap);
    }

    // ── Upgrades / Downgrades ─────────────────────────────────────────────
    for (const auto& v : root["upgrades_downgrades"].toArray()) {
        QJsonObject u = v.toObject();
        AnalystUpgrade upg;
        upg.date         = u["date"].toString();
        upg.firm         = u["firm"].toString();
        upg.to_grade     = u["to_grade"].toString();
        upg.from_grade   = u["from_grade"].toString();
        upg.action       = u["action"].toString();
        upg.price_target = u["price_target"].toDouble();
        upg.prior_target = u["prior_target"].toDouble();
        data_.upgrades_downgrades.append(upg);
    }

    // ── Officers ──────────────────────────────────────────────────────────
    for (const auto& v : root["officers"].toArray()) {
        QJsonObject o = v.toObject();
        CompanyOfficer off;
        off.name      = o["name"].toString();
        off.title     = o["title"].toString();
        off.total_pay = o["total_pay"].toInt();
        off.year_born = o["year_born"].toInt();
        if (!off.name.isEmpty())
            data_.officers.append(off);
    }

    // ── Calendar ──────────────────────────────────────────────────────────
    QJsonObject cal = root["calendar"].toObject();
    data_.calendar.earnings_date    = cal["earnings_date"].toString();
    data_.calendar.earnings_avg     = cal["earnings_avg"].toDouble();
    data_.calendar.earnings_low     = cal["earnings_low"].toDouble();
    data_.calendar.earnings_high    = cal["earnings_high"].toDouble();
    data_.calendar.revenue_avg      = cal["revenue_avg"].toDouble();
    data_.calendar.revenue_low      = cal["revenue_low"].toDouble();
    data_.calendar.revenue_high     = cal["revenue_high"].toDouble();
    data_.calendar.ex_dividend_date = cal["ex_dividend_date"].toString();
    data_.calendar.dividend_date    = cal["dividend_date"].toString();

    // ── Institutional Holders ─────────────────────────────────────────────
    for (const auto& v : root["institutional_holders"].toArray()) {
        QJsonObject h = v.toObject();
        InstitutionalHolder holder;
        holder.name           = h["name"].toString();
        holder.shares         = h["shares"].toDouble();
        holder.value          = h["value"].toDouble();
        holder.percentage     = h["percentage"].toDouble();
        holder.change_percent = h["change_percent"].toDouble();
        holder.fund_family    = h["fund_family"].toString();
        holder.type           = "institutional";
        if (!holder.name.isEmpty())
            data_.institutional_holders.append(holder);
    }

    // ── Mutual Fund Holders ───────────────────────────────────────────────
    for (const auto& v : root["mutualfund_holders"].toArray()) {
        QJsonObject h = v.toObject();
        InstitutionalHolder holder;
        holder.name           = h["name"].toString();
        holder.shares         = h["shares"].toDouble();
        holder.value          = h["value"].toDouble();
        holder.percentage     = h["percentage"].toDouble();
        holder.change_percent = h["change_percent"].toDouble();
        holder.fund_family    = h["fund_family"].toString();
        holder.type           = "mutualfund";
        if (!holder.name.isEmpty())
            data_.mutualfund_holders.append(holder);
    }

    // ── Insider Holders ───────────────────────────────────────────────────
    for (const auto& v : root["insider_holders"].toArray()) {
        QJsonObject h = v.toObject();
        InsiderHolder insider;
        insider.name             = h["name"].toString();
        insider.title            = h["title"].toString();
        insider.shares           = h["shares"].toDouble();
        insider.percentage       = h["percentage"].toDouble();
        insider.last_transaction = h["last_transaction"].toString();
        if (!insider.name.isEmpty())
            data_.insider_holders.append(insider);
    }

    // ── Peers ─────────────────────────────────────────────────────────────
    for (const auto& v : root["peers"].toArray()) {
        QJsonObject p = v.toObject();
        PeerCompany peer;
        peer.ticker          = p["ticker"].toString();
        peer.name            = p["name"].toString();
        peer.market_cap      = p["market_cap"].toDouble();
        peer.pe_ratio        = p["pe_ratio"].toDouble();
        peer.forward_pe      = p["forward_pe"].toDouble();
        peer.roe             = p["roe"].toDouble();
        peer.revenue_growth  = p["revenue_growth"].toDouble();
        peer.profit_margins  = p["profit_margins"].toDouble();
        peer.gross_margins   = p["gross_margins"].toDouble();
        peer.current_price   = p["current_price"].toDouble();
        peer.sector          = p["sector"].toString();
        peer.beta            = p["beta"].toDouble();
        peer.ev_to_ebitda    = p["ev_to_ebitda"].toDouble();
        peer.price_to_book   = p["price_to_book"].toDouble();
        peer.week52_change   = p["week52_change"].toDouble();
        peer.recommendation  = p["recommendation"].toString();
        if (!peer.ticker.isEmpty())
            data_.peers.append(peer);
    }

    // ── Valuation Signal ──────────────────────────────────────────────────
    data_.valuation = compute_valuation(data_.company, data_.peers);

    data_.data_quality = root["data_quality"].toInt();
    data_.timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    emit progress_changed(100, "Complete");
    emit data_ready(data_);

    LOG_INFO("RelMapService", QString("Loaded %1: %2 inst, %3 insiders, %4 peers, quality=%5%")
                                  .arg(data_.company.ticker)
                                  .arg(data_.institutional_holders.size())
                                  .arg(data_.insider_holders.size())
                                  .arg(data_.peers.size())
                                  .arg(data_.data_quality));
}

ValuationSignal RelationshipMapService::compute_valuation(const CompanyInfo& co, const QVector<PeerCompany>& peers) {
    ValuationSignal sig;
    if (peers.isEmpty() || co.pe_ratio <= 0) {
        sig.status = "INSUFFICIENT DATA";
        sig.action = "HOLD";
        sig.score = 0;
        return sig;
    }

    // Compute peer median PE
    QVector<double> peer_pes;
    for (const auto& p : peers) {
        if (p.pe_ratio > 0)
            peer_pes.append(p.pe_ratio);
    }
    if (peer_pes.isEmpty()) {
        sig.status = "INSUFFICIENT DATA";
        sig.action = "HOLD";
        sig.score = 0;
        return sig;
    }

    std::sort(peer_pes.begin(), peer_pes.end());
    double median_pe = peer_pes[peer_pes.size() / 2];
    double pe_ratio_vs_peers = co.pe_ratio / median_pe;

    double score = 0;
    // PE undervalued if < 0.8x peers
    if (pe_ratio_vs_peers < 0.7)
        score += 2;
    else if (pe_ratio_vs_peers < 0.85)
        score += 1;
    else if (pe_ratio_vs_peers > 1.3)
        score -= 2;
    else if (pe_ratio_vs_peers > 1.15)
        score -= 1;

    // Growth premium
    if (co.revenue_growth > 0.2)
        score += 1;
    else if (co.revenue_growth < -0.05)
        score -= 1;

    // Profitability
    if (co.profit_margins > 0.2)
        score += 0.5;

    sig.score = std::max(-3.0, std::min(3.0, score));

    if (sig.score >= 1.5) {
        sig.status = "UNDERVALUED";
        sig.action = "BUY";
    } else if (sig.score >= 0.5) {
        sig.status = "POTENTIALLY UNDERVALUED";
        sig.action = "BUY";
    } else if (sig.score <= -1.5) {
        sig.status = "OVERVALUED";
        sig.action = "SELL";
    } else if (sig.score <= -0.5) {
        sig.status = "POTENTIALLY OVERVALUED";
        sig.action = "SELL";
    } else {
        sig.status = "FAIRLY VALUED";
        sig.action = "HOLD";
    }

    return sig;
}

} // namespace fincept::services
