// src/services/ma_analytics/MAAnalyticsTypes.h
#pragma once
#include <QColor>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace fincept::services::ma {

// ── Module definitions ──────────────────────────────────────────────────────

enum class ModuleId { Valuation, Merger, Deals, Startup, Fairness, Industry, Advanced, Comparison };

struct ModuleInfo {
    ModuleId id;
    QString key; // string key for switch/map
    QString label;
    QString short_label;
    QString category; // CORE, SPECIALIZED, ANALYTICS
    // Identity colour, written for the dark theme: draw it through
    // ui::colors::legible() (text, lines) or ui::colors::tint() (fills).
    QColor color;
};

inline QVector<ModuleInfo> all_modules() {
    return {
        {ModuleId::Valuation, "valuation", "Valuation Toolkit", "VAL", "CORE", QColor("#FF6B35")},
        {ModuleId::Merger, "merger", "Merger Analysis", "MRG", "CORE", QColor("#00E5FF")},
        {ModuleId::Deals, "deals", "Deal Database", "DDB", "CORE", QColor("#0088FF")},
        {ModuleId::Startup, "startup", "Startup Valuation", "STV", "SPECIALIZED", QColor("#9D4EDD")},
        // Key "fairness" kept (saved state, cache keys); shown as a valuation range check, not a fairness opinion.
        {ModuleId::Fairness, "fairness", "Valuation Range Check", "VRC", "SPECIALIZED", QColor("#00D66F")},
        {ModuleId::Industry, "industry", "Industry Metrics", "IND", "SPECIALIZED", QColor("#FFC400")},
        {ModuleId::Advanced, "advanced", "Advanced Analytics", "ADV", "ANALYTICS", QColor("#FF3B8E")},
        {ModuleId::Comparison, "comparison", "Deal Comparison", "CMP", "ANALYTICS", QColor("#00BCD4")},
    };
}

inline QStringList module_capabilities(ModuleId id) {
    switch (id) {
        case ModuleId::Valuation:
            return {"Start from a ticker (market data + latest statements, sources shown)",
                    "DCF with visible drivers + WACC x growth sensitivity grid",
                    "LBO Modeling (Returns, Model, Debt Schedule, entry x exit IRR / MOIC grid)",
                    "Trading Comparables (peer suggestion)", "Precedent Transactions (editable deal grid)",
                    "Football field from your own runs"};
        case ModuleId::Merger:
            return {"Accretion / Dilution: EPS bridge, 3 years, breakeven synergies",
                    "Premium x synergies and premium x cash-mix sensitivity grids", "Revenue & Cost Synergies",
                    "Deal Structure (Payment, Earnout, Exchange, Collar, CVR)", "Pro Forma Financials"};
        case ModuleId::Deals:
            return {"Deal Database: add, edit, verify, CSV export", "SEC Filing Scanner (EDGAR, progress + cancel)",
                    "Deal extraction from filings (marked UNVERIFIED until checked)", "Confidence Scoring"};
        case ModuleId::Startup:
            return {"Berkus Method",        "Scorecard Method",      "VC Method",
                    "First Chicago Method", "Risk Factor Summation", "Comprehensive (All Methods)"};
        case ModuleId::Fairness:
            return {"Offer vs each method's valuation range (not a fairness opinion)",
                    "Premium Analysis (reference prices from market data)", "Process Quality Assessment"};
        case ModuleId::Industry:
            return {"Technology (SaaS, Marketplace, Semi)", "Healthcare (Pharma, Biotech, Devices)",
                    "Financial Services (Banking, Insurance, AM)"};
        case ModuleId::Advanced:
            return {"Monte Carlo Simulation (1K-100K runs)", "Regression Analysis (OLS, Multiple)"};
        case ModuleId::Comparison:
            return {"One editable deal grid (paste, CSV import, verified deals)", "Side-by-Side Deal Comparison",
                    "Deal Rankings", "Premium Benchmarking", "Payment Structure Analysis", "Industry Analysis"};
    }
    return {};
}

} // namespace fincept::services::ma
