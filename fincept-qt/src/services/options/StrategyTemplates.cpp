#include "services/options/StrategyTemplates.h"

#include "core/logging/Logger.h"

#include <QDateTime>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::services::options {

using fincept::trading::InstrumentType;

namespace {

// One-shot catalogue builder. Stored in a static initialiser so the order
// is stable across calls and the VTable / metaobject overhead is paid once.
QVector<StrategyTemplate> build_catalog() {
    QVector<StrategyTemplate> v;
    v.reserve(20);

    // ── Bullish (6) ──────────────────────────────────────────────────────
    v.append({
        "long_call", "Long Call", StrategyCategory::Bullish,
        "Buy 1 ATM call option.",
        "Strongly bullish — expect significant upside, willing to pay premium.",
        {{InstrumentType::CE, 0, +1}},
        /*supports_width=*/false, /*default_width=*/1,
    });
    v.append({
        "short_put", "Short Put", StrategyCategory::Bullish,
        "Sell 1 ATM put option.",
        "Mildly bullish — expect price to stay above the strike. Earns premium.",
        {{InstrumentType::PE, 0, -1}},
        false, 1,
    });
    v.append({
        "bull_call_spread", "Bull Call Spread", StrategyCategory::Bullish,
        "Buy ATM call, sell OTM call (limited risk + reward).",
        "Moderately bullish — capped upside but lower cost than long call.",
        {{InstrumentType::CE, 0, +1}, {InstrumentType::CE, +1, -1}},
        true, 1,
    });
    v.append({
        "bull_put_spread", "Bull Put Spread", StrategyCategory::Bullish,
        "Sell ATM put, buy OTM put for protection.",
        "Mildly bullish — credit spread; max profit when price stays above ATM.",
        {{InstrumentType::PE, 0, -1}, {InstrumentType::PE, -1, +1}},
        true, 1,
    });
    v.append({
        "call_ratio_back_spread", "Call Ratio Back Spread", StrategyCategory::Bullish,
        "Sell 1 ITM call, buy 2 OTM calls.",
        "Strongly bullish with limited downside — profits from a sharp move up.",
        {{InstrumentType::CE, -1, -1}, {InstrumentType::CE, +1, +2}},
        true, 1,
    });
    v.append({
        "synthetic_long", "Synthetic Long", StrategyCategory::Bullish,
        "Buy ATM call + sell ATM put — mimics a long futures position.",
        "Strongly bullish — equivalent risk to long futures, often cheaper to margin.",
        {{InstrumentType::CE, 0, +1}, {InstrumentType::PE, 0, -1}},
        false, 1,
    });

    // ── Bearish (6) ──────────────────────────────────────────────────────
    v.append({
        "long_put", "Long Put", StrategyCategory::Bearish,
        "Buy 1 ATM put option.",
        "Strongly bearish — expect significant downside, willing to pay premium.",
        {{InstrumentType::PE, 0, +1}},
        false, 1,
    });
    v.append({
        "short_call", "Short Call", StrategyCategory::Bearish,
        "Sell 1 ATM call option.",
        "Mildly bearish — expect price to stay below the strike. Earns premium.",
        {{InstrumentType::CE, 0, -1}},
        false, 1,
    });
    v.append({
        "bear_put_spread", "Bear Put Spread", StrategyCategory::Bearish,
        "Buy ATM put, sell OTM put (limited risk + reward).",
        "Moderately bearish — capped downside profit but lower cost than long put.",
        {{InstrumentType::PE, 0, +1}, {InstrumentType::PE, -1, -1}},
        true, 1,
    });
    v.append({
        "bear_call_spread", "Bear Call Spread", StrategyCategory::Bearish,
        "Sell ATM call, buy OTM call for protection.",
        "Mildly bearish — credit spread; max profit when price stays below ATM.",
        {{InstrumentType::CE, 0, -1}, {InstrumentType::CE, +1, +1}},
        true, 1,
    });
    v.append({
        "put_ratio_back_spread", "Put Ratio Back Spread", StrategyCategory::Bearish,
        "Sell 1 ITM put, buy 2 OTM puts.",
        "Strongly bearish with limited upside — profits from a sharp move down.",
        {{InstrumentType::PE, +1, -1}, {InstrumentType::PE, -1, +2}},
        true, 1,
    });
    v.append({
        "synthetic_short", "Synthetic Short", StrategyCategory::Bearish,
        "Sell ATM call + buy ATM put — mimics a short futures position.",
        "Strongly bearish — equivalent risk to short futures.",
        {{InstrumentType::CE, 0, -1}, {InstrumentType::PE, 0, +1}},
        false, 1,
    });

    // ── Neutral (4) ──────────────────────────────────────────────────────
    v.append({
        "iron_condor", "Iron Condor", StrategyCategory::Neutral,
        "Sell OTM put + sell OTM call, both wings hedged.",
        "Range-bound view — earn premium when price stays inside the inner strikes.",
        {{InstrumentType::PE, -1, -1}, {InstrumentType::PE, -2, +1},
         {InstrumentType::CE, +1, -1}, {InstrumentType::CE, +2, +1}},
        true, 1,
    });
    v.append({
        "iron_butterfly", "Iron Butterfly", StrategyCategory::Neutral,
        "Short ATM call + short ATM put, hedged with OTM wings.",
        "Sharp range-bound view at ATM — max profit when price expires at ATM.",
        {{InstrumentType::CE, 0, -1}, {InstrumentType::PE, 0, -1},
         {InstrumentType::CE, +1, +1}, {InstrumentType::PE, -1, +1}},
        true, 1,
    });
    v.append({
        "short_straddle", "Short Straddle", StrategyCategory::Neutral,
        "Sell ATM call + sell ATM put.",
        "Strong neutral view — earn maximum premium, unlimited risk on either side.",
        {{InstrumentType::CE, 0, -1}, {InstrumentType::PE, 0, -1}},
        false, 1,
    });
    v.append({
        "short_strangle", "Short Strangle", StrategyCategory::Neutral,
        "Sell OTM call + sell OTM put.",
        "Range-bound view with cushion — wider profit zone than short straddle.",
        {{InstrumentType::CE, +1, -1}, {InstrumentType::PE, -1, -1}},
        true, 1,
    });

    // ── Volatility (2) ───────────────────────────────────────────────────
    v.append({
        "long_straddle", "Long Straddle", StrategyCategory::Volatility,
        "Buy ATM call + buy ATM put.",
        "Expect a big move in either direction — profits from volatility expansion.",
        {{InstrumentType::CE, 0, +1}, {InstrumentType::PE, 0, +1}},
        false, 1,
    });
    v.append({
        "long_strangle", "Long Strangle", StrategyCategory::Volatility,
        "Buy OTM call + buy OTM put.",
        "Expect a big move in either direction — cheaper than straddle, needs bigger move.",
        {{InstrumentType::CE, +1, +1}, {InstrumentType::PE, -1, +1}},
        true, 1,
    });

    // ── Others (2) ───────────────────────────────────────────────────────
    v.append({
        "call_butterfly", "Call Butterfly", StrategyCategory::Others,
        "Buy 1 OTM call, sell 2 ATM calls, buy 1 OTM call further out.",
        "Pinpoint neutral — max profit if price expires at ATM, capped risk.",
        {{InstrumentType::CE, -1, +1}, {InstrumentType::CE, 0, -2}, {InstrumentType::CE, +1, +1}},
        true, 1,
    });
    v.append({
        "put_butterfly", "Put Butterfly", StrategyCategory::Others,
        "Buy 1 ITM put, sell 2 ATM puts, buy 1 OTM put.",
        "Pinpoint neutral — max profit if price expires at ATM, capped risk.",
        {{InstrumentType::PE, +1, +1}, {InstrumentType::PE, 0, -2}, {InstrumentType::PE, -1, +1}},
        true, 1,
    });

    return v;
}

}  // namespace

const QVector<StrategyTemplate>& catalog() {
    static const QVector<StrategyTemplate> kCatalog = build_catalog();
    return kCatalog;
}

const StrategyTemplate* find(const QString& id) {
    const auto& cat = catalog();
    for (const auto& t : cat)
        if (t.id == id)
            return &t;
    return nullptr;
}

namespace {

/// Locate the chain row whose `is_atm` flag is set, falling back to the
/// strike closest to `chain.atm_strike` if the flag wasn't applied.
int find_atm_index(const OptionChain& chain) {
    for (int i = 0; i < chain.rows.size(); ++i)
        if (chain.rows[i].is_atm)
            return i;
    if (chain.atm_strike <= 0)
        return -1;
    int best = -1;
    double best_diff = std::numeric_limits<double>::infinity();
    for (int i = 0; i < chain.rows.size(); ++i) {
        const double d = std::abs(chain.rows[i].strike - chain.atm_strike);
        if (d < best_diff) {
            best_diff = d;
            best = i;
        }
    }
    return best;
}

double leg_entry_price(const fincept::trading::BrokerQuote& q) {
    if (q.bid > 0 && q.ask > 0)
        return 0.5 * (q.bid + q.ask);
    return q.ltp;
}

}  // namespace

Result<Strategy> instantiate(const StrategyTemplate& tpl, const OptionChain& chain,
                             const StrategyInstantiationOptions& options) {
    if (chain.rows.isEmpty())
        return Result<Strategy>::err("chain has no rows");
    const int atm_idx = find_atm_index(chain);
    if (atm_idx < 0)
        return Result<Strategy>::err("could not locate ATM row in chain");

    const int width = tpl.supports_width ? std::max(1, options.width) : 1;
    const int shift = options.shift;
    const int lot_mul = std::max(1, options.default_lots);

    Strategy strat;
    strat.name = tpl.name;
    strat.underlying = chain.underlying;
    strat.expiry = chain.expiry;
    strat.created = QDateTime::currentDateTime();
    strat.modified = strat.created;
    strat.legs.reserve(tpl.legs.size());

    for (const auto& recipe : tpl.legs) {
        const int offset = recipe.offset_index * width + shift;
        const int target = atm_idx + offset;
        if (target < 0 || target >= chain.rows.size()) {
            return Result<Strategy>::err(
                QString("leg out of chain bounds (template=%1 offset=%2 target=%3 size=%4) — "
                        "try a smaller width or shift")
                    .arg(tpl.id).arg(offset).arg(target).arg(chain.rows.size())
                    .toStdString());
        }

        const OptionChainRow& row = chain.rows.at(target);
        StrategyLeg leg;
        leg.type = recipe.type;
        leg.strike = row.strike;
        leg.expiry = chain.expiry;
        leg.lot_size = row.lot_size;
        leg.lots = recipe.lots * lot_mul;
        leg.is_active = true;

        if (recipe.type == InstrumentType::CE) {
            if (row.ce_token == 0)
                return Result<Strategy>::err(
                    QString("no CE token at strike %1 (template=%2)")
                        .arg(row.strike).arg(tpl.id).toStdString());
            leg.instrument_token = row.ce_token;
            leg.symbol = row.ce_symbol;
            leg.entry_price = leg_entry_price(row.ce_quote);
            leg.iv_at_entry = row.ce_iv;
        } else if (recipe.type == InstrumentType::PE) {
            if (row.pe_token == 0)
                return Result<Strategy>::err(
                    QString("no PE token at strike %1 (template=%2)")
                        .arg(row.strike).arg(tpl.id).toStdString());
            leg.instrument_token = row.pe_token;
            leg.symbol = row.pe_symbol;
            leg.entry_price = leg_entry_price(row.pe_quote);
            leg.iv_at_entry = row.pe_iv;
        } else {
            return Result<Strategy>::err(
                QString("template %1 has unsupported leg type (CE/PE only in v1)")
                    .arg(tpl.id).toStdString());
        }
        strat.legs.append(leg);
    }
    return Result<Strategy>::ok(std::move(strat));
}

} // namespace fincept::services::options
