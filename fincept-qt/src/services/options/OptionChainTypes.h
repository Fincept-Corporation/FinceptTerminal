#pragma once
// OptionChainTypes — F&O data model.
//
// Shared between OptionChainService (producer), FnoScreen (consumer), and
// the strategy builder. All structs are POD-ish (no signals/slots) and are
// registered with Qt's metatype system in DataHubMetaTypes.cpp so they can
// flow through DataHub topics.

#include "trading/TradingTypes.h"
#include "trading/instruments/InstrumentTypes.h"

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>

namespace fincept::services::options {

// ── Underlying classification ──────────────────────────────────────────────

enum class UnderlyingKind { Index, Stock, Currency, Commodity, Unknown };

inline const char* underlying_kind_str(UnderlyingKind k) {
    switch (k) {
        case UnderlyingKind::Index:     return "INDEX";
        case UnderlyingKind::Stock:     return "STOCK";
        case UnderlyingKind::Currency:  return "CURRENCY";
        case UnderlyingKind::Commodity: return "COMMODITY";
        default:                        return "UNKNOWN";
    }
}

// ── Greeks ────────────────────────────────────────────────────────────────
//
// Computed locally from BSM via the Python worker (`option_greeks_batch`
// action, py_vollib backend). All values are per-share (not per-lot); the
// strategy analytics layer multiplies by lot_size × lots when aggregating.

struct OptionGreeks {
    double delta = 0;   // dV/dS                       — [-1, 1]
    double gamma = 0;   // d²V/dS²                     — typically [0, 0.01]
    double theta = 0;   // dV/dt (per day, NOT per yr) — typically negative for longs
    double vega  = 0;   // dV/dσ (per 1.00 σ — divide by 100 for per-1%-vol)
    double rho   = 0;   // dV/dr (per 1.00 r — divide by 100 for per-1%-rate)
    bool valid   = false;
};

// ── Single chain row (one strike, both CE and PE) ─────────────────────────

struct OptionChainRow {
    double  strike = 0;
    int     lot_size = 0;
    bool    is_atm = false;          // |strike - spot| minimum across chain

    // Per-leg instrument identity (resolved from InstrumentRepository)
    qint64  ce_token = 0;
    qint64  pe_token = 0;
    QString ce_symbol;               // normalised, e.g. "NIFTY24MAY24200CE"
    QString pe_symbol;

    // Live snapshots from broker quotes / WS ticks
    fincept::trading::BrokerQuote ce_quote;
    fincept::trading::BrokerQuote pe_quote;

    // Computed locally
    double  ce_iv = 0;               // implied vol, decimal (0.142 = 14.2%)
    double  pe_iv = 0;
    OptionGreeks ce_greeks;
    OptionGreeks pe_greeks;
};

// ── Full chain snapshot for one (underlying, expiry) ───────────────────────

struct OptionChain {
    QString broker_id;
    QString underlying;              // "NIFTY", "BANKNIFTY", "RELIANCE", …
    QString expiry;                  // "DD-MMM-YY" matching Instrument.expiry
    UnderlyingKind kind = UnderlyingKind::Unknown;

    double  spot = 0;                // underlying LTP
    double  atm_strike = 0;          // strike closest to spot
    double  pcr = 0;                 // sum(PE oi) / sum(CE oi)
    double  max_pain = 0;            // strike minimising total writer pain
    double  total_ce_oi = 0;
    double  total_pe_oi = 0;

    QVector<OptionChainRow> rows;    // sorted by strike asc
    qint64  timestamp_ms = 0;        // when the snapshot was assembled
};

// ── Strategy builder data model ────────────────────────────────────────────

struct StrategyLeg {
    qint64  instrument_token = 0;
    QString symbol;                  // normalised
    fincept::trading::InstrumentType type = fincept::trading::InstrumentType::UNKNOWN;
    double  strike = 0;
    QString expiry;
    int     lot_size = 0;
    int     lots = 0;                // signed: + buy, - sell
    double  entry_price = 0;         // user-edited or live LTP
    double  iv_at_entry = 0;
    bool    is_active = true;        // toggle off without deletion
};

struct Strategy {
    QString name;                    // "Custom" or template name
    QString underlying;
    QString expiry;                  // primary expiry (legs may differ — calendars)
    QVector<StrategyLeg> legs;
    QDateTime created;
    QDateTime modified;
    QString notes;
};

// One point on the payoff curve at a given underlying spot price.
struct PayoffPoint {
    double spot = 0;
    double pnl_target = 0;           // P/L on chosen target date (pre-expiry, BSM)
    double pnl_expiry = 0;           // P/L at expiry (intrinsic only, piecewise linear)
};

// Aggregated analytics for a Strategy at the current market state.
struct StrategyAnalytics {
    OptionGreeks combined;            // sum of leg greeks × signed lots × lot_size
    double max_profit = 0;            // bounded value (use std::numeric_limits<double>::infinity for unlimited)
    double max_loss = 0;              // negative or zero
    QVector<double> breakevens;       // sorted asc
    double pop = 0;                   // probability of profit (BSM-derived)
    double premium_paid = 0;          // net debit (positive = paid, negative = received)
    double margin_required = 0;       // from IBroker::get_basket_margins
    bool   margin_estimated = false;  // true when margin came from heuristic, not broker
};

} // namespace fincept::services::options

// ── Qt metatype registrations (for DataHub QVariant fan-out) ──────────────
Q_DECLARE_METATYPE(fincept::services::options::OptionGreeks)
Q_DECLARE_METATYPE(fincept::services::options::OptionChainRow)
Q_DECLARE_METATYPE(fincept::services::options::OptionChain)
Q_DECLARE_METATYPE(fincept::services::options::StrategyLeg)
Q_DECLARE_METATYPE(fincept::services::options::Strategy)
Q_DECLARE_METATYPE(fincept::services::options::PayoffPoint)
Q_DECLARE_METATYPE(fincept::services::options::StrategyAnalytics)
Q_DECLARE_METATYPE(QVector<fincept::services::options::OptionChainRow>)
Q_DECLARE_METATYPE(QVector<fincept::services::options::PayoffPoint>)
Q_DECLARE_METATYPE(QVector<fincept::services::options::StrategyLeg>)
