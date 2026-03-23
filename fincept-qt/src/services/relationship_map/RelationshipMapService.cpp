// src/services/relationship_map/RelationshipMapService.cpp
#include "services/relationship_map/RelationshipMapService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace fincept::services {

using namespace fincept::relmap;

RelationshipMapService& RelationshipMapService::instance() {
    static RelationshipMapService s;
    return s;
}

void RelationshipMapService::fetch(const QString& ticker) {
    if (loading_)
        return;
    if (ticker.trimmed().isEmpty())
        return;

    current_ticker_ = ticker.toUpper();
    loading_ = true;
    data_ = {};

    emit progress_changed(5, "Starting data fetch for " + current_ticker_ + "...");

    python::PythonRunner::instance().run(
        "relationship_map.py", {current_ticker_},
        [this](python::PythonResult result) {
            loading_ = false;

            if (!result.success || result.output.trimmed().isEmpty()) {
                LOG_ERROR("RelMapService", "Python script failed: exit=" + QString::number(result.exit_code));
                emit fetch_failed("Failed to fetch data for " + current_ticker_);
                return;
            }

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
    data_.company.ticker = co["ticker"].toString();
    data_.company.name = co["name"].toString();
    data_.company.sector = co["sector"].toString();
    data_.company.industry = co["industry"].toString();
    data_.company.market_cap = co["market_cap"].toDouble();
    data_.company.current_price = co["current_price"].toDouble();
    data_.company.pe_ratio = co["pe_ratio"].toDouble();
    data_.company.forward_pe = co["forward_pe"].toDouble();
    data_.company.price_to_book = co["price_to_book"].toDouble();
    data_.company.roe = co["roe"].toDouble();
    data_.company.revenue_growth = co["revenue_growth"].toDouble();
    data_.company.profit_margins = co["profit_margins"].toDouble();
    data_.company.revenue = co["revenue"].toDouble();
    data_.company.ebitda = co["ebitda"].toDouble();
    data_.company.free_cashflow = co["free_cashflow"].toDouble();
    data_.company.total_cash = co["total_cash"].toDouble();
    data_.company.total_debt = co["total_debt"].toDouble();
    data_.company.insider_percent = co["insider_percent"].toDouble();
    data_.company.institutional_percent = co["institutional_percent"].toDouble();
    data_.company.employees = co["employees"].toInt();
    data_.company.recommendation = co["recommendation"].toString();
    data_.company.target_price = co["target_price"].toDouble();
    data_.company.analyst_count = co["analyst_count"].toInt();

    // ── Institutional Holders ────────────────────────────────────────────
    QJsonArray inst_arr = root["institutional_holders"].toArray();
    for (const auto& v : inst_arr) {
        QJsonObject h = v.toObject();
        InstitutionalHolder holder;
        holder.name = h["name"].toString();
        holder.shares = h["shares"].toDouble();
        holder.value = h["value"].toDouble();
        holder.percentage = h["percentage"].toDouble();
        holder.change_percent = h["change_percent"].toDouble();
        holder.fund_family = h["fund_family"].toString();
        if (!holder.name.isEmpty())
            data_.institutional_holders.append(holder);
    }

    // ── Insider Holders ──────────────────────────────────────────────────
    QJsonArray ins_arr = root["insider_holders"].toArray();
    for (const auto& v : ins_arr) {
        QJsonObject h = v.toObject();
        InsiderHolder insider;
        insider.name = h["name"].toString();
        insider.title = h["title"].toString();
        insider.shares = h["shares"].toDouble();
        insider.percentage = h["percentage"].toDouble();
        insider.last_transaction = h["last_transaction"].toString();
        if (!insider.name.isEmpty())
            data_.insider_holders.append(insider);
    }

    // ── Peers ────────────────────────────────────────────────────────────
    QJsonArray peers_arr = root["peers"].toArray();
    for (const auto& v : peers_arr) {
        QJsonObject p = v.toObject();
        PeerCompany peer;
        peer.ticker = p["ticker"].toString();
        peer.name = p["name"].toString();
        peer.market_cap = p["market_cap"].toDouble();
        peer.pe_ratio = p["pe_ratio"].toDouble();
        peer.roe = p["roe"].toDouble();
        peer.revenue_growth = p["revenue_growth"].toDouble();
        peer.profit_margins = p["profit_margins"].toDouble();
        peer.current_price = p["current_price"].toDouble();
        peer.sector = p["sector"].toString();
        if (!peer.ticker.isEmpty())
            data_.peers.append(peer);
    }

    // ── Valuation Signal ─────────────────────────────────────────────────
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

ValuationSignal RelationshipMapService::compute_valuation(const CompanyInfo& co,
                                                           const QVector<PeerCompany>& peers) {
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
    if (pe_ratio_vs_peers < 0.7) score += 2;
    else if (pe_ratio_vs_peers < 0.85) score += 1;
    else if (pe_ratio_vs_peers > 1.3) score -= 2;
    else if (pe_ratio_vs_peers > 1.15) score -= 1;

    // Growth premium
    if (co.revenue_growth > 0.2) score += 1;
    else if (co.revenue_growth < -0.05) score -= 1;

    // Profitability
    if (co.profit_margins > 0.2) score += 0.5;

    sig.score = std::max(-3.0, std::min(3.0, score));

    if (sig.score >= 1.5) { sig.status = "UNDERVALUED"; sig.action = "BUY"; }
    else if (sig.score >= 0.5) { sig.status = "POTENTIALLY UNDERVALUED"; sig.action = "BUY"; }
    else if (sig.score <= -1.5) { sig.status = "OVERVALUED"; sig.action = "SELL"; }
    else if (sig.score <= -0.5) { sig.status = "POTENTIALLY OVERVALUED"; sig.action = "SELL"; }
    else { sig.status = "FAIRLY VALUED"; sig.action = "HOLD"; }

    return sig;
}

} // namespace fincept::services
