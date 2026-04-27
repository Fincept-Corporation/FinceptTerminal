// src/screens/economics/panels/EconomicsPresets.h
// Shared FRED preset list — single source of truth for FredPanel and the
// MarketDataNodes "FRED Economic Data" workflow node.
#pragma once

#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::screens {

struct FredPreset {
    QString label;     // human-readable, e.g. "Real GDP (GDPC1)"
    QString series_id; // FRED series id, e.g. "GDPC1"
};

inline const QVector<FredPreset>& fred_presets() {
    static const QVector<FredPreset> kPresets = {
        {"-- Custom series ID --", ""},
        {"Real GDP (GDPC1)", "GDPC1"},
        {"Nominal GDP (GDP)", "GDP"},
        {"CPI All Items (CPIAUCSL)", "CPIAUCSL"},
        {"Core CPI (CPILFESL)", "CPILFESL"},
        {"Unemployment Rate (UNRATE)", "UNRATE"},
        {"Federal Funds Rate (FEDFUNDS)", "FEDFUNDS"},
        {"10-Year Treasury Yield (DGS10)", "DGS10"},
        {"2-Year Treasury Yield (DGS2)", "DGS2"},
        {"30-Year Mortgage Rate (MORTGAGE30US)", "MORTGAGE30US"},
        {"M2 Money Supply (M2SL)", "M2SL"},
        {"Personal Savings Rate (PSAVERT)", "PSAVERT"},
        {"Industrial Production (INDPRO)", "INDPRO"},
        {"Retail Sales (RSXFS)", "RSXFS"},
        {"Housing Starts (HOUST)", "HOUST"},
        {"Trade Balance (BOPGSTB)", "BOPGSTB"},
        {"VIX Volatility Index (VIXCLS)", "VIXCLS"},
        {"S&P 500 (SP500)", "SP500"},
    };
    return kPresets;
}

// Series IDs only — for use as a workflow-node dropdown where users pick a code.
inline QStringList fred_preset_series_ids() {
    QStringList ids;
    for (const auto& p : fred_presets()) {
        if (!p.series_id.isEmpty())
            ids.append(p.series_id);
    }
    return ids;
}

} // namespace fincept::screens
