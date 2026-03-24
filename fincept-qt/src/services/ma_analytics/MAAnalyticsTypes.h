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
    QColor color;
};

inline QVector<ModuleInfo> all_modules() {
    return {
        {ModuleId::Valuation, "valuation", "Valuation Toolkit", "VAL", "CORE", QColor("#FF6B35")},
        {ModuleId::Merger, "merger", "Merger Analysis", "MRG", "CORE", QColor("#00E5FF")},
        {ModuleId::Deals, "deals", "Deal Database", "DDB", "CORE", QColor("#0088FF")},
        {ModuleId::Startup, "startup", "Startup Valuation", "STV", "SPECIALIZED", QColor("#9D4EDD")},
        {ModuleId::Fairness, "fairness", "Fairness Opinion", "FOP", "SPECIALIZED", QColor("#00D66F")},
        {ModuleId::Industry, "industry", "Industry Metrics", "IND", "SPECIALIZED", QColor("#FFC400")},
        {ModuleId::Advanced, "advanced", "Advanced Analytics", "ADV", "ANALYTICS", QColor("#FF3B8E")},
        {ModuleId::Comparison, "comparison", "Deal Comparison", "CMP", "ANALYTICS", QColor("#00BCD4")},
    };
}

inline QStringList module_capabilities(ModuleId id) {
    switch (id) {
        case ModuleId::Valuation:
            return {"DCF Analysis", "LBO Modeling (Returns, Model, Debt Schedule, Sensitivity)", "Trading Comparables",
                    "Precedent Transactions"};
        case ModuleId::Merger:
            return {"Accretion / Dilution Analysis", "Revenue & Cost Synergies",
                    "Deal Structure (Payment, Earnout, Exchange, Collar, CVR)", "Pro Forma Financials"};
        case ModuleId::Deals:
            return {"Deal Database & Search", "SEC Filing Scanner (EDGAR)", "Auto-Parse Deal Indicators",
                    "Confidence Scoring"};
        case ModuleId::Startup:
            return {"Berkus Method",        "Scorecard Method",      "VC Method",
                    "First Chicago Method", "Risk Factor Summation", "Comprehensive (All Methods)"};
        case ModuleId::Fairness:
            return {"Fairness Analysis & Conclusion", "Premium Analysis (Multi-Period)", "Process Quality Assessment"};
        case ModuleId::Industry:
            return {"Technology (SaaS, Marketplace, Semi)", "Healthcare (Pharma, Biotech, Devices)",
                    "Financial Services (Banking, Insurance, AM)"};
        case ModuleId::Advanced:
            return {"Monte Carlo Simulation (1K-100K runs)", "Regression Analysis (OLS, Multiple)"};
        case ModuleId::Comparison:
            return {"Side-by-Side Deal Comparison", "Deal Rankings", "Premium Benchmarking",
                    "Payment Structure Analysis", "Industry Analysis"};
    }
    return {};
}

// ── Result types ────────────────────────────────────────────────────────────

struct DCFResult {
    double wacc = 0;
    double enterprise_value = 0;
    double equity_value = 0;
    double per_share = 0;
    QJsonObject raw;
};

struct LBOResult {
    double irr = 0;
    double moic = 0;
    double gross_proceeds = 0;
    QJsonObject raw;
};

struct MergerResult {
    double standalone_eps = 0;
    double pro_forma_eps = 0;
    double accretion_pct = 0;
    bool is_accretive = false;
    QJsonObject raw;
};

struct StartupValuationResult {
    QString method;
    double valuation = 0;
    double low = 0;
    double high = 0;
    QJsonObject raw;
};

struct FairnessResult {
    bool is_fair = false;
    QString conclusion;
    double weighted_value = 0;
    QJsonObject raw;
};

struct IndustryResult {
    QString sector;
    QJsonObject metrics;
    QJsonObject benchmarks;
    QStringList insights;
    QJsonObject raw;
};

struct MonteCarloResult {
    double mean = 0;
    double std_dev = 0;
    double p5 = 0;
    double p25 = 0;
    double p75 = 0;
    double p95 = 0;
    QVector<double> distribution;
    QJsonObject raw;
};

struct DealComparisonResult {
    QJsonObject raw;
};

struct GenericResult {
    bool success = false;
    QString error;
    QJsonObject data;
};

} // namespace fincept::services::ma
