// src/screens/portfolio/PortfolioStress.h
//
// Stress scenarios applied to the current book — shared by the RISK view and the
// OPTIMIZE view's STRESS tab. A scenario is a set of shocks:
//   * equity moves per REGION ("north_america", "europe", "uk", "japan", "china",
//     "india", "asia_pacific", "emerging" — see region_of_country()), with
//     `equity_shock` for any region it does not name;
//   * rates (parallel shift in bp: bonds move −duration × Δy) or a direct bond move;
//   * commodities, gold, real estate and crypto moves;
//   * FX: % change of each currency's value in USD. A holding's currency move into the
//     portfolio's BASE currency is (1 + m_ccy) / (1 + m_base) − 1 and compounds with
//     its local move — the FX leg.
// Asset class comes from the quote's quoteType (services/portfolio/PortfolioClassify.h),
// region from the listing venue's country (or a caller-supplied domicile map).
// The built-in presets are APPROXIMATE peak-to-trough moves of the named episodes, for
// orientation only; `window_start` / `window_end` name the real window, which
// PortfolioService::historical_stress() can replay on actual prices. Users can define
// their own scenarios (scenario_to_json / scenario_from_json for persistence).
#pragma once

#include "core/market/MarketSessions.h"
#include "screens/portfolio/PortfolioTypes.h"
#include "services/portfolio/PortfolioClassify.h"

#include <QCoreApplication>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace fincept::screens::portfolio_stress {

using portfolio::AssetClass;

struct Scenario {
    const char* name = "";        ///< translatable preset name (QT_TRANSLATE_NOOP); "" for user scenarios
    const char* description = "";
    double equity_shock = 0;      ///< % move of equities in regions without their own entry
    double bond_shock = 0;        ///< % move of investment-grade bonds (used when rates_bp is NaN)
    double commodity_shock = 0;   ///< % move of a broad commodity basket
    double crypto_shock = portfolio::kAbsent;      ///< %; NaN = equity_shock × kCryptoMultiplier
    double gold_shock = portfolio::kAbsent;        ///< %; NaN = commodity_shock
    double real_estate_shock = portfolio::kAbsent; ///< %; NaN = the region's equity move
    double rates_bp = portfolio::kAbsent;          ///< parallel yield change; NaN = use bond_shock
    QHash<QString, double> equity_by_region;       ///< region key -> %
    QHash<QString, double> fx_vs_usd;              ///< ISO -> % change of the currency's value in USD
    QString id;                                    ///< stable key ("gfc_2008", "user:...")
    QString custom_name;                           ///< user scenario name (preset: empty)
    QString custom_description;
    QString window_start; ///< historical window (YYYY-MM-DD) for historical_stress(); empty = hypothetical
    QString window_end;
};

// Translation context kept from the RISK view, where these strings first lived.
inline constexpr const char* kStressTrContext = "fincept::screens::RiskManagementView";

inline QString scenario_name(const Scenario& s) {
    return !s.custom_name.isEmpty() ? s.custom_name : QCoreApplication::translate(kStressTrContext, s.name);
}
inline QString scenario_description(const Scenario& s) {
    return !s.custom_description.isEmpty() ? s.custom_description
                                           : QCoreApplication::translate(kStressTrContext, s.description);
}

namespace stress_detail {
inline Scenario make(const char* id, const char* name, const char* desc, double eq, double bond, double cmdty,
                     QHash<QString, double> regions = {}, QHash<QString, double> fx = {},
                     double crypto = portfolio::kAbsent, double gold = portfolio::kAbsent,
                     double rates_bp = portfolio::kAbsent, const char* w0 = "", const char* w1 = "") {
    Scenario s;
    s.id = QString::fromLatin1(id);
    s.name = name;
    s.description = desc;
    s.equity_shock = eq;
    s.bond_shock = bond;
    s.commodity_shock = cmdty;
    s.equity_by_region = std::move(regions);
    s.fx_vs_usd = std::move(fx);
    s.crypto_shock = crypto;
    s.gold_shock = gold;
    s.rates_bp = rates_bp;
    s.window_start = QString::fromLatin1(w0);
    s.window_end = QString::fromLatin1(w1);
    return s;
}
inline QHash<QString, double> em_fx(double m) {
    QHash<QString, double> h;
    for (const char* c : {"BRL", "MXN", "ZAR", "TRY", "INR", "IDR", "KRW", "TWD", "THB", "MYR", "PHP", "CLP", "COP",
                          "PLN", "HUF", "CZK", "EGP", "NGN", "RUB", "VND"})
        h.insert(QString::fromLatin1(c), m);
    return h;
}
inline QHash<QString, double> merged(QHash<QString, double> a, const QHash<QString, double>& b) {
    for (auto it = b.constBegin(); it != b.constEnd(); ++it)
        a.insert(it.key(), it.value());
    return a;
}
} // namespace stress_detail

/// Built-in presets (approximate peak-to-trough moves; see the header comment).
inline QVector<Scenario> preset_scenarios() {
    namespace sd = stress_detail;
    QVector<Scenario> v;
    v << sd::make("gfc_2008", QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "2008 Financial Crisis"), QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Lehman collapse, credit freeze"), -38.5, 5.2,
                  -33.0,
                  {{"north_america", -38.0}, {"europe", -45.0}, {"uk", -33.0}, {"japan", -42.0}, {"china", -65.0},
                   {"asia_pacific", -40.0}, {"india", -55.0}, {"emerging", -55.0}},
                  sd::merged(sd::em_fx(-20.0), {{"EUR", -12.0}, {"GBP", -25.0}, {"AUD", -30.0}, {"NZD", -30.0},
                                                 {"CAD", -20.0}, {"JPY", 20.0}, {"CHF", 5.0}}),
                  portfolio::kAbsent, 5.0, portfolio::kAbsent, "2007-10-09", "2009-03-09");
    v << sd::make("covid_2020", QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "2020 COVID Crash"), QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Pandemic selloff (Feb-Mar 2020)"), -33.9, 1.2, -20.0,
                  {{"north_america", -34.0}, {"europe", -38.0}, {"uk", -35.0}, {"japan", -30.0}, {"china", -12.0},
                   {"asia_pacific", -36.0}, {"india", -38.0}, {"emerging", -32.0}},
                  sd::merged(sd::em_fx(-15.0), {{"EUR", -3.0}, {"GBP", -10.0}, {"AUD", -12.0}, {"CAD", -8.0},
                                                 {"JPY", 2.0}, {"CHF", 1.0}}),
                  -50.0, -3.0, portfolio::kAbsent, "2020-02-19", "2020-03-23");
    v << sd::make("dotcom", QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Dot-com Bust"), QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Tech bubble burst (2000-2002)"), -49.1, 8.5, -10.0,
                  {{"north_america", -49.0}, {"europe", -60.0}, {"uk", -45.0}, {"japan", -55.0}, {"emerging", -50.0}},
                  {}, portfolio::kAbsent, 10.0, portfolio::kAbsent, "2000-03-24", "2002-10-09");
    v << sd::make("rates_300", QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Interest Rate Shock +3%"), QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Aggressive Fed tightening"), -15.0,
                  -12.0, -5.0, {}, {}, -40.0, -5.0, 300.0);
    v << sd::make("black_monday", QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Black Monday 1987"), QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Single-day market crash"), -22.6, 2.0, -8.0, {},
                  {}, portfolio::kAbsent, portfolio::kAbsent, portfolio::kAbsent, "1987-10-16", "1987-10-19");
    v << sd::make("inflation", QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Inflation Surge"), QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "1970s-style stagflation"), -10.0, -15.0, 25.0, {},
                  {}, -30.0, 15.0, 200.0);
    v << sd::make("geopolitical", QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Geopolitical Crisis"), QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Major conflict escalation"), -12.0, 3.0, 15.0,
                  {{"europe", -16.0}, {"emerging", -18.0}}, {{"EUR", -4.0}, {"JPY", 3.0}, {"CHF", 3.0}}, -20.0, 10.0);
    v << sd::make("em_fx_crisis", QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Currency Crisis"), QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Emerging market contagion"), -18.0, 1.0, 5.0,
                  {{"emerging", -30.0}, {"india", -25.0}, {"china", -20.0}}, sd::em_fx(-20.0));
    // Non-US episodes.
    v << sd::make("euro_debt_2011", QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Euro Debt Crisis 2011"), QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Sovereign stress in the euro area"), -19.0,
                  6.0, -10.0, {{"europe", -30.0}, {"uk", -20.0}, {"japan", -18.0}, {"emerging", -28.0}},
                  {{"EUR", -8.0}, {"CHF", 10.0}, {"JPY", 3.0}}, portfolio::kAbsent, 15.0, portfolio::kAbsent,
                  "2011-07-01", "2011-10-04");
    v << sd::make("china_2015", QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "China Crash 2015-16"), QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Shanghai bubble burst, yuan devaluation"), -15.0,
                  2.0, -30.0,
                  {{"china", -45.0}, {"asia_pacific", -20.0}, {"emerging", -25.0}, {"japan", -22.0},
                   {"europe", -20.0}, {"north_america", -12.0}},
                  sd::merged(sd::em_fx(-10.0), {{"CNY", -5.0}, {"AUD", -10.0}}), portfolio::kAbsent, 5.0,
                  portfolio::kAbsent, "2015-06-12", "2016-01-28");
    v << sd::make("asia_1997", QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Asian Financial Crisis 1997"), QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Baht devaluation and contagion"), -10.0,
                  3.0, -15.0, {{"emerging", -55.0}, {"asia_pacific", -35.0}, {"japan", -20.0}, {"china", -30.0}},
                  {{"THB", -50.0}, {"KRW", -50.0}, {"IDR", -80.0}, {"MYR", -40.0}, {"PHP", -35.0}, {"JPY", -10.0}},
                  portfolio::kAbsent, portfolio::kAbsent, portfolio::kAbsent, "1997-07-02", "1998-01-12");
    v << sd::make("japan_1990", QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Japan Bubble Burst 1990"), QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Nikkei collapse after the asset bubble"),
                  -15.0, 2.0, 0.0, {{"japan", -40.0}}, {{"JPY", -5.0}}, portfolio::kAbsent, portfolio::kAbsent,
                  portfolio::kAbsent, "1990-01-01", "1990-10-01");
    v << sd::make("brexit_2016", QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Brexit Vote 2016"), QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Referendum shock to sterling and Europe"), -5.0,
                  2.0, 0.0, {{"uk", -6.0}, {"europe", -11.0}, {"japan", -8.0}},
                  {{"GBP", -11.0}, {"EUR", -3.0}, {"JPY", 4.0}, {"CHF", 1.0}}, portfolio::kAbsent, 6.0,
                  portfolio::kAbsent, "2016-06-23", "2016-06-27");
    v << sd::make("rates_2022", QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "2022 Inflation & Rate Shock"), QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Fastest tightening in decades"), -20.0,
                  -15.0, 20.0,
                  {{"north_america", -25.0}, {"europe", -20.0}, {"uk", -8.0}, {"japan", -10.0}, {"china", -30.0},
                   {"emerging", -25.0}},
                  sd::merged(sd::em_fx(-10.0), {{"EUR", -15.0}, {"GBP", -20.0}, {"JPY", -25.0}, {"CHF", -5.0}}),
                  -70.0, -10.0, 250.0, "2022-01-03", "2022-10-12");
    v << sd::make("uk_gilts_2022", QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "UK Gilt Crisis 2022"), QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Mini-budget, LDI forced selling"), -3.0, -10.0,
                  0.0, {{"uk", -5.0}}, {{"GBP", -6.0}}, portfolio::kAbsent, portfolio::kAbsent, 100.0, "2022-09-22",
                  "2022-09-28");
    v << sd::make("taper_2013", QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Taper Tantrum 2013"), QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Fed taper talk, EM outflows"), -5.0, -5.0, -5.0,
                  {{"emerging", -15.0}, {"india", -10.0}},
                  sd::merged(sd::em_fx(-8.0), {{"INR", -10.0}, {"BRL", -12.0}, {"IDR", -8.0}}), portfolio::kAbsent,
                  -10.0, 100.0, "2013-05-22", "2013-06-24");
    v << sd::make("crypto_winter", QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Crypto Winter"), QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "2018-style crypto collapse"), -5.0, 0.0, 0.0, {}, {},
                  -80.0, portfolio::kAbsent, portfolio::kAbsent, "2017-12-17", "2018-12-15");
    return v;
}

/// Built-in presets (kept under the old name the views iterate).
inline const QVector<Scenario> kScenarios = preset_scenarios();

// Estimated betas outside this band are treated as estimation noise.
inline constexpr double kBetaMin = -1.0;
inline constexpr double kBetaMax = 3.0;
// A scenario without a crypto figure moves crypto at 1.5× its equity move (legacy).
inline constexpr double kCryptoMultiplier = 1.5;
// Duration of a bond line when only a rates shock is given (aggregate-index-like).
inline constexpr double kDefaultBondDuration = 7.0;

struct HoldingImpact {
    QString symbol;
    AssetClass asset_class = AssetClass::Equity;
    bool class_guessed = false;
    QString region;
    double weight = 0;   ///< fraction of the valued book
    double local_pct = 0; ///< move in the holding's own currency, %
    double fx_pct = 0;    ///< currency move into the base currency, %
    double total_pct = 0; ///< (1+local)(1+fx) − 1, %
    double pnl = 0;       ///< base currency
    bool beta_used = false;
};

struct Result {
    double impact_pct = 0.0; ///< portfolio move, % of the valued book
    double pnl = 0.0;        ///< base-currency change (signed)
    double fx_pnl = 0.0;     ///< part of pnl from the FX leg (approx.: weight × fx move)
    int equity_lines = 0;    ///< equity holdings in the book
    int beta_lines = 0;      ///< of which moved by their own estimated beta
    int unvalued = 0;        ///< holdings left out (not valued)
    QVector<HoldingImpact> lines;
};

struct ApplyOptions {
    /// Base currency of the book. Empty = inferred from the holdings (a valued holding
    /// whose spot rate is exactly 1 is in the base currency), else USD. Prefer the
    /// apply(Scenario, PortfolioSummary, ...) overload, which passes it.
    QString base_currency;
    double bond_duration = kDefaultBondDuration;
    /// Domicile country per symbol (InfoData::country) — overrides the venue's country.
    const QHash<QString, QString>* country_of = nullptr;
};

/// Impact of `s` on the book (base currency). An equity line whose region has its own
/// shock moves by it; otherwise it moves by beta × equity_shock when an estimated beta
/// exists (clamped to [kBetaMin, kBetaMax]), else by equity_shock. Holdings that are
/// not valued (no price / FX) are left out and counted in `unvalued`.
inline Result apply(const Scenario& s, const QVector<portfolio::HoldingWithQuote>& holdings,
                    const QHash<QString, portfolio::HoldingRisk>* risk, double total_mv,
                    const ApplyOptions& opt = {}) {
    Result r;
    QString base = opt.base_currency.trimmed().toUpper();
    if (base.isEmpty()) {
        for (const auto& h : holdings)
            if (h.valued() && h.fx_rate == 1.0 && !h.currency.isEmpty()) {
                base = h.currency.toUpper();
                break;
            }
        // Deliberate USD (not the display currency): the scenarios' FX moves are quoted
        // against USD (fx_vs_usd), so USD is the model's numeraire; with nothing to infer
        // the base from, the book is measured against that numeraire.
        if (base.isEmpty())
            base = QStringLiteral("USD");
    }
    auto fx_move = [&s](const QString& ccy) {
        return s.fx_vs_usd.value(ccy, 0.0) / 100.0;
    };
    // USD is the numeraire of fx_vs_usd: it has no move of its own.
    const double base_m = base == QLatin1String("USD") ? 0.0 : fx_move(base);
    for (const auto& h : holdings) {
        const double w = h.weight / 100.0;
        if (!h.valued() || !std::isfinite(w)) {
            ++r.unvalued;
            continue;
        }
        HoldingImpact li;
        li.symbol = h.symbol;
        li.weight = w;
        const auto ac = portfolio::classify_asset(h.symbol, h.quote_type, h.name);
        li.asset_class = ac.cls;
        li.class_guessed = ac.guessed;
        QString country;
        if (opt.country_of)
            country = opt.country_of->value(h.symbol);
        if (country.isEmpty()) {
            if (const auto* v = core::market::venue_for_symbol(h.symbol, h.exchange))
                country = v->country;
        }
        li.region = portfolio::region_of_country(country);
        const bool region_has_shock = !li.region.isEmpty() && s.equity_by_region.contains(li.region);
        const double region_eq = region_has_shock ? s.equity_by_region.value(li.region) : s.equity_shock;

        double local = 0.0;
        switch (ac.cls) {
            case AssetClass::Crypto:
                local = std::isfinite(s.crypto_shock) ? s.crypto_shock : s.equity_shock * kCryptoMultiplier;
                break;
            case AssetClass::Bond:
                local = std::isfinite(s.rates_bp) ? -opt.bond_duration * s.rates_bp / 100.0 : s.bond_shock;
                break;
            case AssetClass::Commodity: {
                const bool gold = h.name.toUpper().contains(QLatin1String("GOLD")) ||
                                  h.symbol.startsWith(QLatin1String("GC=")) || h.symbol == QLatin1String("GLD") ||
                                  h.symbol == QLatin1String("IAU");
                local = (gold && std::isfinite(s.gold_shock)) ? s.gold_shock : s.commodity_shock;
                break;
            }
            case AssetClass::RealEstate:
                local = std::isfinite(s.real_estate_shock) ? s.real_estate_shock : region_eq;
                break;
            case AssetClass::Cash:
                local = 0.0;
                break;
            case AssetClass::Equity:
            case AssetClass::Other: {
                ++r.equity_lines;
                if (region_has_shock) {
                    local = region_eq;
                } else {
                    double beta = 1.0;
                    if (risk) {
                        const auto it = risk->constFind(h.symbol);
                        if (it != risk->constEnd() && it->beta.has_value()) {
                            beta = std::clamp(*it->beta, kBetaMin, kBetaMax);
                            ++r.beta_lines;
                            li.beta_used = true;
                        }
                    }
                    local = s.equity_shock * beta;
                }
                break;
            }
        }
        const QString ccy = h.currency.trimmed().toUpper();
        const double ccy_m = ccy == QLatin1String("USD") ? 0.0 : fx_move(ccy);
        li.fx_pct = ((1.0 + ccy_m) / (1.0 + base_m) - 1.0) * 100.0;
        li.local_pct = local;
        li.total_pct = ((1.0 + local / 100.0) * (1.0 + li.fx_pct / 100.0) - 1.0) * 100.0;
        li.pnl = total_mv * w * li.total_pct / 100.0;
        r.impact_pct += w * li.total_pct;
        r.fx_pnl += total_mv * w * li.fx_pct / 100.0;
        r.lines.append(li);
    }
    r.pnl = total_mv * r.impact_pct / 100.0;
    return r;
}

/// Same, with the base currency and the valued total taken from the summary. `total_mv`
/// is the valued holdings' total (cash, even when tracked, is not shocked).
inline Result apply(const Scenario& s, const portfolio::PortfolioSummary& summary,
                    const QHash<QString, portfolio::HoldingRisk>* risk, ApplyOptions opt = {}) {
    if (opt.base_currency.isEmpty())
        opt.base_currency = summary.base_currency.isEmpty() ? summary.portfolio.currency : summary.base_currency;
    double valued = 0;
    for (const auto& h : summary.holdings)
        if (h.valued() && std::isfinite(h.market_value))
            valued += h.market_value;
    return apply(s, summary.holdings, risk, valued, opt);
}

// ── User scenarios (persistence) ─────────────────────────────────────────────

inline QJsonObject scenario_to_json(const Scenario& s) {
    QJsonObject o;
    o["id"] = s.id;
    o["name"] = scenario_name(s);
    o["description"] = scenario_description(s);
    o["equity_shock"] = s.equity_shock;
    o["bond_shock"] = s.bond_shock;
    o["commodity_shock"] = s.commodity_shock;
    if (std::isfinite(s.crypto_shock))
        o["crypto_shock"] = s.crypto_shock;
    if (std::isfinite(s.gold_shock))
        o["gold_shock"] = s.gold_shock;
    if (std::isfinite(s.real_estate_shock))
        o["real_estate_shock"] = s.real_estate_shock;
    if (std::isfinite(s.rates_bp))
        o["rates_bp"] = s.rates_bp;
    QJsonObject reg, fx;
    for (auto it = s.equity_by_region.constBegin(); it != s.equity_by_region.constEnd(); ++it)
        reg.insert(it.key(), it.value());
    for (auto it = s.fx_vs_usd.constBegin(); it != s.fx_vs_usd.constEnd(); ++it)
        fx.insert(it.key(), it.value());
    o["equity_by_region"] = reg;
    o["fx_vs_usd"] = fx;
    if (!s.window_start.isEmpty()) {
        o["window_start"] = s.window_start;
        o["window_end"] = s.window_end;
    }
    return o;
}

/// A user-defined scenario from JSON (keys as scenario_to_json). Missing numbers = 0 /
/// absent as documented on Scenario.
inline Scenario scenario_from_json(const QJsonObject& o) {
    Scenario s;
    auto num = [&o](const char* k, double dflt) {
        const QJsonValue v = o.value(QLatin1String(k));
        return (v.isDouble() && std::isfinite(v.toDouble())) ? v.toDouble() : dflt;
    };
    s.id = o.value("id").toString();
    if (!s.id.startsWith(QLatin1String("user:")))
        s.id.prepend(QLatin1String("user:"));
    s.custom_name = o.value("name").toString();
    s.custom_description = o.value("description").toString();
    s.equity_shock = num("equity_shock", 0.0);
    s.bond_shock = num("bond_shock", 0.0);
    s.commodity_shock = num("commodity_shock", 0.0);
    s.crypto_shock = num("crypto_shock", portfolio::kAbsent);
    s.gold_shock = num("gold_shock", portfolio::kAbsent);
    s.real_estate_shock = num("real_estate_shock", portfolio::kAbsent);
    s.rates_bp = num("rates_bp", portfolio::kAbsent);
    const QJsonObject reg = o.value("equity_by_region").toObject();
    for (auto it = reg.begin(); it != reg.end(); ++it)
        if (it.value().isDouble())
            s.equity_by_region.insert(it.key(), it.value().toDouble());
    const QJsonObject fx = o.value("fx_vs_usd").toObject();
    for (auto it = fx.begin(); it != fx.end(); ++it)
        if (it.value().isDouble())
            s.fx_vs_usd.insert(it.key().toUpper(), it.value().toDouble());
    s.window_start = o.value("window_start").toString();
    s.window_end = o.value("window_end").toString();
    return s;
}

} // namespace fincept::screens::portfolio_stress
