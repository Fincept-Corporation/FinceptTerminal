// src/screens/portfolio/PortfolioStress.h
//
// Historical / hypothetical stress scenarios applied to the current book. Shared by the
// RISK view and the OPTIMIZE view's STRESS tab, which used to carry two copies of the
// table and two different formulas (one scaled the whole equity slice by the PORTFOLIO
// beta — a figure that already blends bonds and commodities in — the other ignored beta).
#pragma once

#include "screens/portfolio/PortfolioTypes.h"

#include <QCoreApplication>
#include <QHash>
#include <QString>
#include <QVector>

#include <algorithm>

namespace fincept::screens::portfolio_stress {

struct Scenario {
    const char* name;
    const char* description;
    double equity_shock;    // % move of the broad equity market
    double bond_shock;      // % move of investment-grade bonds
    double commodity_shock; // % move of a broad commodity basket
};

// Translation context kept from the RISK view, where these strings first lived, so
// existing .ts translations still apply.
inline constexpr const char* kStressTrContext = "fincept::screens::RiskManagementView";

inline const Scenario kScenarios[] = {
    {QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "2008 Financial Crisis"),
     QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Lehman collapse, credit freeze"), -38.5, 5.2, -33.0},
    {QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "2020 COVID Crash"),
     QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Pandemic selloff (Feb-Mar 2020)"), -33.9, 1.2, -20.0},
    {QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Dot-com Bust"),
     QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Tech bubble burst (2000-2002)"), -49.1, 8.5, -10.0},
    {QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Interest Rate Shock +3%"),
     QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Aggressive Fed tightening"), -15.0, -12.0, -5.0},
    {QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Black Monday 1987"),
     QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Single-day market crash"), -22.6, 2.0, -8.0},
    {QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Inflation Surge"),
     QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "1970s-style stagflation"), -10.0, -15.0, 25.0},
    {QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Geopolitical Crisis"),
     QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Major conflict escalation"), -12.0, 3.0, 15.0},
    {QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Currency Crisis"),
     QT_TRANSLATE_NOOP("fincept::screens::RiskManagementView", "Emerging market contagion"), -18.0, 1.0, 5.0},
};

inline QString scenario_name(const Scenario& s) {
    return QCoreApplication::translate(kStressTrContext, s.name);
}
inline QString scenario_description(const Scenario& s) {
    return QCoreApplication::translate(kStressTrContext, s.description);
}

enum class AssetClass { Equity, Bond, Commodity, Crypto };

/// Asset-class heuristic from the ticker (distinct from the GICS sector).
inline AssetClass classify(const QString& symbol) {
    const QString s = symbol.toUpper();
    if (s.endsWith(QLatin1String("-USD")) || s.endsWith(QLatin1String("-USDT")) || s.endsWith(QLatin1String("USDT")))
        return AssetClass::Crypto;
    static const QStringList kBonds = {"TLT",  "AGG",  "BND", "IEF", "SHY", "LQD",  "HYG", "EMB",  "TIP", "GOVT",
                                       "VGIT", "VGLT", "BSV", "BIV", "MUB", "SCHZ", "JNK", "VCIT", "VCSH"};
    if (kBonds.contains(s) || s.contains(QLatin1String("BOND")) || s.contains(QLatin1String("TREAS")))
        return AssetClass::Bond;
    static const QStringList kCommodities = {"GLD", "IAU", "SLV", "USO", "DBC", "PDBC", "GSG", "UNG", "SGOL", "CPER"};
    if (kCommodities.contains(s) || s.contains(QLatin1String("GOLD")) || s.contains(QLatin1String("OIL")) ||
        s.endsWith(QLatin1String("=F")))
        return AssetClass::Commodity;
    return AssetClass::Equity;
}

// Estimated betas outside this band are treated as estimation noise.
inline constexpr double kBetaMin = -1.0;
inline constexpr double kBetaMax = 3.0;
// Crypto has no stable equity beta; it is shocked at 1.5x the equity move.
inline constexpr double kCryptoMultiplier = 1.5;

struct Result {
    double impact_pct = 0.0; ///< portfolio move, % of NAV
    double pnl = 0.0;        ///< currency change (signed)
    int equity_lines = 0;    ///< equity holdings in the book
    int beta_lines = 0;      ///< of which moved by their own estimated beta
};

/// Impact of `s` on the book. An equity line with an estimated beta (from the
/// market-analytics run) moves by beta × the scenario's equity shock; one without falls
/// back to the market move. Bonds, commodities and crypto take their asset-class shock.
inline Result apply(const Scenario& s, const QVector<portfolio::HoldingWithQuote>& holdings,
                    const QHash<QString, portfolio::HoldingRisk>* risk, double total_mv) {
    Result r;
    for (const auto& h : holdings) {
        const double w = h.weight / 100.0;
        double shock = 0.0;
        switch (classify(h.symbol)) {
            case AssetClass::Crypto:
                shock = s.equity_shock * kCryptoMultiplier;
                break;
            case AssetClass::Bond:
                shock = s.bond_shock;
                break;
            case AssetClass::Commodity:
                shock = s.commodity_shock;
                break;
            case AssetClass::Equity: {
                ++r.equity_lines;
                double beta = 1.0;
                if (risk) {
                    const auto it = risk->constFind(h.symbol);
                    if (it != risk->constEnd() && it->beta.has_value()) {
                        beta = std::clamp(*it->beta, kBetaMin, kBetaMax);
                        ++r.beta_lines;
                    }
                }
                shock = s.equity_shock * beta;
                break;
            }
        }
        r.impact_pct += w * shock;
    }
    r.pnl = total_mv * r.impact_pct / 100.0;
    return r;
}

} // namespace fincept::screens::portfolio_stress
