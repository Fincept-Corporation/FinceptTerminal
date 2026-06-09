#include "algo_engine/fno/FnoAlgoSelftest.h"

#include "algo_engine/fno/FnoAlgoTypes.h"
#include "algo_engine/fno/FnoDataBridge.h"
#include "algo_engine/fno/FnoExecution.h"
#include "ui/widgets/algo/FnoLegRuleEditor.h"
#include "algo_engine/fno/FnoLegResolver.h"
#include "algo_engine/fno/FnoStrategyPreview.h"
#include "algo_engine/PositionManager.h"
#include "algo_engine/AlgoEngineTypes.h"
#include "trading/instruments/InstrumentTypes.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "services/algo_trading/AlgoTradingTypes.h"
#include "services/options/OptionChainService.h"
#include "storage/sqlite/Database.h"
#include "trading/PaperTrading.h"

#include <QJsonDocument>
#include <QtGlobal>
#include <cstdio>

namespace fincept::algo::fno {
namespace {

int g_failures = 0;

void check(bool cond, const char* label) {
    std::fprintf(stdout, "  %s %s\n", cond ? "[PASS]" : "[FAIL]", label);
    if (!cond)
        ++g_failures;
}

void test_leg_json_roundtrip() {
    std::fprintf(stdout, "[1] leg JSON round-trip\n");
    QVector<AlgoFnoLeg> legs;
    AlgoFnoLeg ce;
    ce.kind = "CE"; ce.side = "BUY"; ce.lots = 2;
    ce.strike_mode = "ATM_OFFSET"; ce.strike_value = 2; ce.expiry_mode = "WEEKLY";
    AlgoFnoLeg pe;
    pe.kind = "PE"; pe.side = "SELL"; pe.lots = 1;
    pe.strike_mode = "ABSOLUTE"; pe.strike_value = 23000;
    pe.expiry_mode = "ABSOLUTE"; pe.expiry_value = "26-JUN-25";
    legs << ce << pe;

    const QVector<AlgoFnoLeg> back = fno_legs_from_json(fno_legs_to_json(legs));
    check(back.size() == 2, "round-trip preserves leg count");
    check(back.size() == 2 && back[0].kind == "CE" && back[0].lots == 2 &&
              back[0].strike_mode == "ATM_OFFSET" && back[0].side == "BUY" &&
              back[0].expiry_value.isEmpty(),
          "leg 0 fields preserved");
    check(back.size() == 2 && back[1].strike_mode == "ABSOLUTE" &&
              qFuzzyCompare(back[1].strike_value, 23000.0) &&
              back[1].expiry_value == "26-JUN-25" && back[1].side == "SELL",
          "leg 1 fields preserved");
}

void test_v046_schema() {
    using fincept::Database;
    std::fprintf(stdout, "[2] v046 schema columns present\n");
    auto has_col = [](const QString& table, const QString& col) -> bool {
        auto q = Database::instance().execute("PRAGMA table_info(" + table + ")", {});
        if (q.is_err())
            return false;
        auto& query = q.value();
        while (query.next())
            if (query.value(1).toString() == col) // col 1 = name in PRAGMA table_info
                return true;
        return false;
    };
    check(has_col("algo_strategies", "instrument_type"), "algo_strategies.instrument_type");
    check(has_col("algo_strategies", "legs_json"), "algo_strategies.legs_json");
    check(has_col("algo_deployments", "instrument_type"), "algo_deployments.instrument_type");
    check(has_col("algo_deployments", "underlying"), "algo_deployments.underlying");
    check(has_col("algo_deployments", "resolved_expiry"), "algo_deployments.resolved_expiry");
    check(has_col("algo_deployments", "resolved_legs_json"), "algo_deployments.resolved_legs_json");
}

void test_persistence_roundtrip() {
    using fincept::Database;
    using fincept::services::algo::AlgoStrategy;
    using fincept::services::algo::AlgoTradingService;
    std::fprintf(stdout, "[3] strategy persistence round-trip\n");

    AlgoStrategy s;
    // Fixed id so a crash between save and the DELETE below self-heals on the next
    // run (the save_strategy UPSERT overwrites the same row).
    s.id = "selftest-fno-algo-strategy";
    s.name = "selftest fno algo (delete me)";
    s.instrument_type = "option";
    QVector<AlgoFnoLeg> legs;
    AlgoFnoLeg l;
    l.kind = "CE"; l.side = "BUY"; l.lots = 1; l.strike_mode = "ATM"; l.expiry_mode = "WEEKLY";
    legs << l;
    s.legs = fno_legs_to_json(legs);

    AlgoTradingService::instance().save_strategy(s);

    auto q = Database::instance().execute(
        "SELECT instrument_type, legs_json FROM algo_strategies WHERE id = ?", {s.id});
    // is_ok() must gate value() — value() on an error Result is unsafe in this codebase.
    const bool ok = q.is_ok() && q.value().next();
    check(ok, "row written and read back");
    if (ok) {
        auto& query = q.value();
        const QString it = query.value(0).toString();
        const QJsonArray got = QJsonDocument::fromJson(query.value(1).toString().toUtf8()).array();
        const QVector<AlgoFnoLeg> back = fno_legs_from_json(got);
        check(it == "option", "instrument_type persisted");
        check(back.size() == 1 && back[0].kind == "CE" && back[0].strike_mode == "ATM",
              "legs persisted");
    }

    // Clean up so the dev DB keeps no selftest residue.
    Database::instance().execute("DELETE FROM algo_strategies WHERE id = ?", {s.id});
}

fincept::services::options::OptionChain make_synthetic_chain() {
    using namespace fincept::services::options;
    OptionChain c;
    c.broker_id = "fyers";
    c.underlying = "NIFTY";
    c.expiry = "26-JUN-25";
    c.spot = 23187;
    c.atm_strike = 23200;
    // Strikes 22800..23600 step 100; CE delta falls as strike rises.
    const double strikes[] = {22800, 22900, 23000, 23100, 23200, 23300, 23400, 23500, 23600};
    const double ce_delta[] = {0.90, 0.82, 0.72, 0.61, 0.50, 0.40, 0.30, 0.22, 0.15};
    int n = int(sizeof(strikes) / sizeof(strikes[0]));
    for (int i = 0; i < n; ++i) {
        OptionChainRow r;
        r.strike = strikes[i];
        r.is_atm = (strikes[i] == c.atm_strike);
        r.ce_token = 1000 + i;
        r.pe_token = 2000 + i;
        r.ce_symbol = QString("NIFTY26JUN25C%1").arg(int(strikes[i]));
        r.pe_symbol = QString("NIFTY26JUN25P%1").arg(int(strikes[i]));
        r.lot_size = 50;
        r.ce_quote.ltp = 100.0 + i;
        r.pe_quote.ltp = 90.0 + i;
        r.ce_iv = 0.15;
        r.pe_iv = 0.16;
        r.ce_greeks.delta = ce_delta[i];
        r.ce_greeks.valid = true;
        r.pe_greeks.delta = ce_delta[i] - 1.0; // put delta ~ call delta - 1
        r.pe_greeks.valid = true;
        c.rows.append(r);
    }
    return c;
}

void test_leg_resolver() {
    std::fprintf(stdout, "[4] FnoLegResolver\n");
    const auto chain = make_synthetic_chain();

    auto mk = [](const QString& kind, const QString& mode, double val) {
        AlgoFnoLeg l;
        l.kind = kind;
        l.side = "BUY";
        l.strike_mode = mode;
        l.strike_value = val;
        return l;
    };

    const auto atm = FnoLegResolver::resolve_one(mk("CE", "ATM", 0), chain);
    check(atm.valid && qFuzzyCompare(atm.strike, 23200.0) && atm.symbol == "NIFTY26JUN25C23200",
          "ATM CE -> 23200");

    const auto off = FnoLegResolver::resolve_one(mk("CE", "ATM_OFFSET", 2), chain);
    check(off.valid && qFuzzyCompare(off.strike, 23400.0), "ATM_OFFSET +2 CE -> 23400");

    const auto abs = FnoLegResolver::resolve_one(mk("PE", "ABSOLUTE", 23000), chain);
    check(abs.valid && qFuzzyCompare(abs.strike, 23000.0) && abs.symbol == "NIFTY26JUN25P23000",
          "ABSOLUTE 23000 PE -> 23000");

    const auto dlt = FnoLegResolver::resolve_one(mk("CE", "DELTA", 0.30), chain);
    check(dlt.valid && qFuzzyCompare(dlt.strike, 23400.0), "DELTA 0.30 CE -> 23400");

    AlgoFnoLeg fut;
    fut.kind = "FUT";
    const auto futr = FnoLegResolver::resolve_one(fut, chain);
    check(!futr.valid, "FUT leg not resolvable from option chain (expected invalid)");

    // A chain row with no tradeable contract (token 0) must resolve as invalid.
    auto chain0 = make_synthetic_chain();
    chain0.rows[2].ce_token = 0; // strike 23000 CE: no contract
    chain0.rows[2].ce_symbol.clear();
    const auto missing = FnoLegResolver::resolve_one(mk("CE", "ABSOLUTE", 23000), chain0);
    check(!missing.valid, "missing contract (token 0) -> invalid");
}

void test_preview_bridge() {
    std::fprintf(stdout, "[5] preview strategy bridge\n");
    const auto chain = make_synthetic_chain();
    QVector<AlgoFnoLeg> legs;
    AlgoFnoLeg ce; ce.kind = "CE"; ce.side = "BUY";  ce.lots = 1; ce.strike_mode = "ATM";
    AlgoFnoLeg pe; pe.kind = "PE"; pe.side = "SELL"; pe.lots = 2; pe.strike_mode = "ABSOLUTE"; pe.strike_value = 23000;
    AlgoFnoLeg fut; fut.kind = "FUT"; // unresolved -> skipped
    legs << ce << pe << fut;

    const auto s = build_preview_strategy(legs, chain);
    check(s.legs.size() == 2, "two resolvable legs converted (FUT skipped)");
    if (s.legs.size() == 2) {
        check(qFuzzyCompare(s.legs[0].strike, 23200.0) && s.legs[0].lots == 1 &&
                  s.legs[0].lot_size == 50 && s.legs[0].type == fincept::trading::InstrumentType::CE &&
                  qFuzzyCompare(s.legs[0].entry_price, 104.0) && qFuzzyCompare(s.legs[0].iv_at_entry, 0.15),
              "CE ATM BUY leg correct (entry/iv from row)");
        check(qFuzzyCompare(s.legs[1].strike, 23000.0) && s.legs[1].lots == -2 &&
                  s.legs[1].type == fincept::trading::InstrumentType::PE &&
                  qFuzzyCompare(s.legs[1].entry_price, 92.0) && qFuzzyCompare(s.legs[1].iv_at_entry, 0.16),
              "PE ABSOLUTE SELL leg has signed lots -2 (entry/iv from row)");
    }
}

void test_leg_rule_editor() {
    std::fprintf(stdout, "[6] FnoLegRuleEditor model\n");
    fincept::ui::algo::FnoLegRuleEditor editor;
    QVector<AlgoFnoLeg> in;
    AlgoFnoLeg a; a.kind = "CE"; a.side = "BUY";  a.lots = 1; a.strike_mode = "ATM";
    AlgoFnoLeg b; b.kind = "PE"; b.side = "SELL"; b.lots = 2; b.strike_mode = "ABSOLUTE"; b.strike_value = 23000;
    in << a << b;
    editor.set_legs(in);
    const auto out = editor.legs();
    check(out.size() == 2, "set_legs/legs preserves count");
    if (out.size() == 2) {
        check(out[0].kind == "CE" && out[0].side == "BUY" && out[0].lots == 1 &&
                  out[0].strike_mode == "ATM",
              "leg 0 round-trips");
        check(out[1].kind == "PE" && out[1].side == "SELL" && out[1].lots == 2 &&
                  out[1].strike_mode == "ABSOLUTE" && qFuzzyCompare(out[1].strike_value, 23000.0),
              "leg 1 round-trips");
    }
}

void test_fno_data_bridge() {
    std::fprintf(stdout, "[7a] FnoDataBridge snapshot + pinning\n");
    fincept::algo::fno::FnoDataBridge bridge;
    auto chain = make_synthetic_chain(); // broker "fyers", underlying "NIFTY", expiry "26-JUN-25"
    bridge.ingest_chain(chain);
    const auto snap = bridge.snapshot("fyers", "NIFTY", "26-JUN-25");
    check(snap.rows.size() == chain.rows.size() && snap.underlying == "NIFTY",
          "bridge caches + returns chain snapshot copy");

    using fincept::services::options::OptionChainService;
    const QString topic = OptionChainService::instance().chain_topic("fyers", "NIFTY", "26-JUN-25");
    OptionChainService::instance().pin_contracts(topic, {"NIFTY26JUN25C23000", "NIFTY26JUN25P23000"});
    const auto pins = OptionChainService::instance().pinned_contracts_for(topic);
    check(pins.contains("NIFTY26JUN25C23000") && pins.contains("NIFTY26JUN25P23000"),
          "pin_contracts stores pinned symbols for the topic");
    OptionChainService::instance().pin_contracts(topic, {}); // cleanup
}

void test_position_manager_multileg() {
    using fincept::algo::PositionManager;
    using fincept::algo::AlgoLegPosition;
    std::fprintf(stdout, "[7b] PositionManager multi-leg basket\n");

    // Construct: sl=30%, tp=50%, trail=0, maxOrderVal=0, maxDailyLoss=0
    PositionManager pm("test", 30.0, 50.0, 0.0, 0.0, 0.0);

    // Build a long straddle: CE long, PE long, each qty=50 entry=100
    QVector<AlgoLegPosition> legs;
    AlgoLegPosition ce;
    ce.symbol      = "CE_SYM";
    ce.is_call     = true;
    ce.side_sign   = 1;  // long
    ce.quantity    = 50;
    ce.entry_price = 100.0;
    ce.current_price = 100.0;

    AlgoLegPosition pe;
    pe.symbol      = "PE_SYM";
    pe.is_call     = false;
    pe.side_sign   = 1;  // long
    pe.quantity    = 50;
    pe.entry_price = 100.0;
    pe.current_price = 100.0;

    legs << ce << pe;
    pm.record_entry_legs(legs, 0);

    check(pm.has_legs(), "has_legs() true after record_entry_legs");

    // CE +30, PE -10 → basket unrealized = (30*50) + (-10*50) = 1500 - 500 = 1000
    pm.update_leg_price("CE_SYM", 130.0);
    pm.update_leg_price("PE_SYM",  90.0);

    const double unrealized = pm.metrics().unrealized_pnl;
    check(std::abs(unrealized - 1000.0) < 1e-6, "basket unrealized_pnl == 1000");

    // check_risk at this point: basket_pnl=1000, entry_value=|100*50|+|100*50|=10000
    // basket_pnl_pct = 1000/10000*100 = 10% → no trigger
    {
        auto sig = pm.check_risk(0);
        check(!sig.has_value(), "no risk trigger at +10% basket P&L");
    }

    // Drive a stop: set both legs to loss so basket_pnl_pct <= -30%
    // Need basket_pnl <= -30% of 10000 = -3000
    // CE: (50-100)*50 = -2500, PE: (50-100)*50 = -2500 → total = -5000 (-50%) → triggers SL
    pm.update_leg_price("CE_SYM", 50.0);
    pm.update_leg_price("PE_SYM", 50.0);

    {
        auto sig = pm.check_risk(0);
        check(sig.has_value() && sig->reason == "stop_loss",
              "stop_loss fires when basket P&L <= -30%");
    }

    // record_exit_legs returns realized P&L and clears legs.
    // Expected basket P&L: CE (50-100)*50*1 + PE (50-100)*50*1 = -2500 + -2500 = -5000
    const double expected_realized = -5000.0;
    const double realized = pm.record_exit_legs(1000);
    check(std::abs(realized - expected_realized) < 1e-6, "record_exit_legs returns correct realized P&L (-5000)");

    // Multi-leg state must be fully cleared after exit
    check(!pm.has_legs(), "has_legs() false after record_exit_legs");
    check(pm.legs().isEmpty(), "legs() returns empty vector after record_exit_legs");
    check(pm.metrics().unrealized_pnl == 0.0, "unrealized_pnl reset to 0 after record_exit_legs");
    check(pm.metrics().total_trades == 1, "total_trades incremented");
    check(std::abs(pm.metrics().total_pnl - expected_realized) < 1e-6,
          "metrics total_pnl reflects basket realized P&L");

    // Verify single-position equity path is unaffected (it was never touched by multi-leg ops)
    check(!pm.has_position(), "single-position path untouched: no position after multi-leg exit");
}

void test_fno_execution() {
    using fincept::algo::fno::resolve_expiry;
    using fincept::algo::fno::resolve_entry_legs;
    using fincept::algo::fno::build_exit_legs;
    using fincept::algo::AlgoLegPosition;

    std::fprintf(stdout, "[7c] FnoExecution helpers\n");

    // ── resolve_expiry ────────────────────────────────────────────────────────
    // Fixed available expiries and today = 2025-06-01.
    //   "26-JUN-25" → DTE 25
    //   "03-JUL-25" → DTE 32
    //   "31-JUL-25" → DTE 60
    //   "28-AUG-25" → DTE 88
    const QStringList expiries{
        QStringLiteral("26-JUN-25"),
        QStringLiteral("03-JUL-25"),
        QStringLiteral("31-JUL-25"),
        QStringLiteral("28-AUG-25"),
    };
    const QDate today(2025, 6, 1);

    // WEEKLY → first future expiry
    check(resolve_expiry(QStringLiteral("WEEKLY"), QString(), expiries, today)
              == QStringLiteral("26-JUN-25"),
          "resolve_expiry WEEKLY -> 26-JUN-25");

    // NEAREST_DTE "40" → first expiry with DTE >= 40 → 31-JUL-25 (DTE=60)
    check(resolve_expiry(QStringLiteral("NEAREST_DTE"), QStringLiteral("40"), expiries, today)
              == QStringLiteral("31-JUL-25"),
          "resolve_expiry NEAREST_DTE 40 -> 31-JUL-25");

    // ABSOLUTE → returned as-is
    check(resolve_expiry(QStringLiteral("ABSOLUTE"), QStringLiteral("28-AUG-25"), expiries, today)
              == QStringLiteral("28-AUG-25"),
          "resolve_expiry ABSOLUTE 28-AUG-25 -> 28-AUG-25");

    // MONTHLY → earliest future month (June) has only 26-JUN-25 → last of June = 26-JUN-25
    check(resolve_expiry(QStringLiteral("MONTHLY"), QString(), expiries, today)
              == QStringLiteral("26-JUN-25"),
          "resolve_expiry MONTHLY -> 26-JUN-25 (last of earliest month)");

    // Edge: empty list → ""
    check(resolve_expiry(QStringLiteral("WEEKLY"), QString(), QStringList{}, today).isEmpty(),
          "resolve_expiry empty list -> empty string");

    // Edge: NEAREST_DTE where all DTEs are below threshold → returns farthest expiry
    check(resolve_expiry(QStringLiteral("NEAREST_DTE"), QStringLiteral("365"), expiries, today)
              == QStringLiteral("28-AUG-25"),
          "resolve_expiry NEAREST_DTE 365 (none qualifies) -> farthest expiry");

    // ── resolve_entry_legs ────────────────────────────────────────────────────
    // Straddle: CE ATM BUY 1 lot + PE ATM SELL 1 lot against make_synthetic_chain().
    // ATM strike = 23200 (index 4).  lot_size=50.
    // CE token=1004, symbol="NIFTY26JUN25C23200", ce_quote.ltp = 100+4 = 104.0
    // PE token=2004, symbol="NIFTY26JUN25P23200", pe_quote.ltp =  90+4 = 94.0
    const auto chain = make_synthetic_chain();

    QVector<AlgoFnoLeg> straddle_rules;
    AlgoFnoLeg ce_rule; ce_rule.kind = "CE"; ce_rule.side = "BUY";  ce_rule.lots = 1;
    ce_rule.strike_mode = "ATM";
    AlgoFnoLeg pe_rule; pe_rule.kind = "PE"; pe_rule.side = "SELL"; pe_rule.lots = 1;
    pe_rule.strike_mode = "ATM";
    straddle_rules << ce_rule << pe_rule;

    const QJsonArray straddle_json = fno_legs_to_json(straddle_rules);
    const auto entry_legs = resolve_entry_legs(chain, straddle_json);

    check(entry_legs.size() == 2, "resolve_entry_legs: straddle produces 2 legs");
    if (entry_legs.size() == 2) {
        const auto& el_ce = entry_legs[0];
        check(el_ce.symbol == QStringLiteral("NIFTY26JUN25C23200") &&
                  el_ce.side == QStringLiteral("BUY") &&
                  qFuzzyCompare(el_ce.quantity, 50.0) &&
                  qFuzzyCompare(el_ce.price, 104.0),
              "resolve_entry_legs: CE leg correct (symbol, side, qty=50, price=104)");

        const auto& el_pe = entry_legs[1];
        check(el_pe.symbol == QStringLiteral("NIFTY26JUN25P23200") &&
                  el_pe.side == QStringLiteral("SELL") &&
                  qFuzzyCompare(el_pe.quantity, 50.0) &&
                  qFuzzyCompare(el_pe.price, 94.0),
              "resolve_entry_legs: PE leg correct (symbol, side, qty=50, price=94)");
    }

    // ── build_exit_legs ───────────────────────────────────────────────────────
    // One long leg (side_sign=+1) → exit side = SELL
    AlgoLegPosition long_leg;
    long_leg.symbol           = QStringLiteral("NIFTY26JUN25C23200");
    long_leg.instrument_token = 1004;
    long_leg.is_call          = true;
    long_leg.side_sign        = 1;   // long BUY
    long_leg.quantity         = 50;
    long_leg.entry_price      = 104.0;
    long_leg.current_price    = 120.0; // mark has moved

    QVector<AlgoLegPosition> open_positions;
    open_positions << long_leg;

    const auto exit_legs = build_exit_legs(open_positions);
    check(exit_legs.size() == 1, "build_exit_legs: 1 open leg -> 1 exit leg");
    if (exit_legs.size() == 1) {
        const auto& xl = exit_legs[0];
        check(xl.symbol == QStringLiteral("NIFTY26JUN25C23200") &&
                  xl.side == QStringLiteral("SELL") &&
                  qFuzzyCompare(xl.quantity, 50.0) &&
                  qFuzzyCompare(xl.price, 120.0),
              "build_exit_legs: long leg flips to SELL, preserves qty + current_price");
    }

    // Short leg (side_sign=-1) → exit side = BUY
    AlgoLegPosition short_leg;
    short_leg.symbol           = QStringLiteral("NIFTY26JUN25P23200");
    short_leg.instrument_token = 2004;
    short_leg.is_call          = false;
    short_leg.side_sign        = -1; // short SELL
    short_leg.quantity         = 50;
    short_leg.entry_price      = 94.0;
    short_leg.current_price    = 80.0;

    const auto exit2 = build_exit_legs({short_leg});
    check(exit2.size() == 1 && exit2[0].side == QStringLiteral("BUY"),
          "build_exit_legs: short leg flips to BUY");
}

// Paper multi-leg basket flow. Drives the exact production seam that
// AlgoEngine::execute_basket (paper branch) uses — resolve legs → build the
// basket request (live helper) → pt_place_order + pt_fill_order per leg into a
// paper portfolio → PositionManager basket entry/mark/exit — without the engine
// QThread/event-loop wiring (build-verified separately). Asserts 2 paper legs
// fill, the basket position is recorded, P&L marks, and exit closes both.
void test_paper_basket_flow() {
    using namespace fincept::trading;
    using fincept::algo::fno::resolve_entry_legs;
    using fincept::algo::fno::build_basket_request;
    using fincept::algo::AlgoLegPosition;
    using fincept::algo::PositionManager;

    std::fprintf(stdout, "[8] paper multi-leg basket flow\n");

    // v047 leg columns must exist (per-leg trade persistence).
    {
        auto has_col = [](const QString& col) -> bool {
            auto q = fincept::Database::instance().execute("PRAGMA table_info(algo_trades)", {});
            if (q.is_err())
                return false;
            auto& query = q.value();
            while (query.next())
                if (query.value(1).toString() == col) // col 1 = name in PRAGMA table_info
                    return true;
            return false;
        };
        check(has_col("leg_symbol") && has_col("leg_index"),
              "v047: algo_trades has leg_symbol + leg_index");
    }

    // Resolve a straddle (CE ATM BUY 1 lot + PE ATM SELL 1 lot) on the synthetic
    // chain: CE NIFTY26JUN25C23200 @104 qty50, PE NIFTY26JUN25P23200 @94 qty50.
    const auto chain = make_synthetic_chain();
    QVector<AlgoFnoLeg> rules;
    AlgoFnoLeg ce; ce.kind = "CE"; ce.side = "BUY";  ce.lots = 1; ce.strike_mode = "ATM";
    AlgoFnoLeg pe; pe.kind = "PE"; pe.side = "SELL"; pe.lots = 1; pe.strike_mode = "ATM";
    rules << ce << pe;
    const auto legs = resolve_entry_legs(chain, fno_legs_to_json(rules));
    check(legs.size() == 2, "resolved 2 entry legs for the straddle");
    if (legs.size() != 2) return;

    // build_basket_request (live broker helper): one NFO market order per leg.
    const auto basket = build_basket_request(legs, QStringLiteral("NRML"));
    check(basket.orders.size() == 2 &&
              basket.orders[0].exchange == QStringLiteral("NFO") &&
              basket.orders[0].side == OrderSide::Buy &&
              basket.orders[1].side == OrderSide::Sell &&
              basket.orders[0].product_type == ProductType::Margin,
          "build_basket_request: 2 NFO margin orders, BUY then SELL");

    // ── Paper placement: place + fill each leg into a fresh paper portfolio ──────
    const auto pf = pt_create_portfolio(QStringLiteral("FNO_ALGO_SELFTEST"), 1000000.0,
                                        QStringLiteral("INR"), 1.0,
                                        QStringLiteral("cross"), 0.0, QStringLiteral("NFO"));
    bool placed_ok = true;
    for (const auto& leg : legs) {
        try {
            const auto po = pt_place_order(pf.id, leg.symbol, leg.side.toLower(),
                                           QStringLiteral("market"), leg.quantity, leg.price,
                                           std::nullopt, false,
                                           QStringLiteral("NFO"), QStringLiteral("NRML"));
            pt_fill_order(po.id, leg.price);
        } catch (const std::exception& e) {
            placed_ok = false;
            std::fprintf(stdout, "    place/fill failed: %s\n", e.what());
        }
    }
    check(placed_ok, "both paper legs placed + filled without throwing");
    check(pt_get_positions(pf.id).size() == 2, "paper portfolio holds 2 open leg positions");

    // ── Basket position lifecycle in PositionManager (sl=50%, tp=100%) ───────────
    QVector<AlgoLegPosition> basket_legs;
    for (const auto& el : legs) {
        AlgoLegPosition lp;
        lp.symbol        = el.symbol;
        lp.side_sign     = (el.side == QLatin1String("BUY")) ? 1 : -1;
        lp.quantity      = el.quantity;
        lp.entry_price   = el.price;
        lp.current_price = el.price;
        basket_legs << lp;
    }
    PositionManager pm("t8", 50.0, 100.0, 0.0, 0.0, 0.0);
    pm.record_entry_legs(basket_legs, 0);
    check(pm.has_legs(), "PositionManager records the basket position");

    // Mark CE 104->120 (+16*50 = +800) and PE 94->80 (short: (80-94)*50*-1 = +700).
    pm.update_leg_price(QStringLiteral("NIFTY26JUN25C23200"), 120.0);
    pm.update_leg_price(QStringLiteral("NIFTY26JUN25P23200"),  80.0);
    check(std::abs(pm.metrics().unrealized_pnl - 1500.0) < 1e-6,
          "basket marks: unrealized P&L == +1500");

    const double realized = pm.record_exit_legs(1);
    check(std::abs(realized - 1500.0) < 1e-6, "exit closes both legs, realized P&L == +1500");
    check(!pm.has_legs(), "basket flat after exit");

    pt_delete_portfolio(pf.id); // keep the selftest DB clean
}

// Multi-leg market data + restart reattach helpers (P3.5): leg-position JSON
// round-trip (used for resolved_legs_json persistence) and chain-snapshot leg
// marking (used to mark an open basket live on each underlying tick).
void test_fno_leg_marks_and_persistence() {
    using fincept::algo::fno::leg_positions_to_json;
    using fincept::algo::fno::leg_positions_from_json;
    using fincept::algo::fno::leg_marks_from_chain;
    using fincept::algo::AlgoLegPosition;

    std::fprintf(stdout, "[9] F&O leg marks + resolved-legs persistence\n");

    // ── JSON round-trip (resolved_legs_json) ────────────────────────────────────
    AlgoLegPosition ce;
    ce.symbol = "NIFTY26JUN25C23200"; ce.instrument_token = 1004; ce.is_call = true;
    ce.strike = 23200; ce.side_sign = 1;  ce.quantity = 50; ce.entry_price = 104.0;
    ce.current_price = 130.0; ce.unrealized_pnl = 1300.0;
    AlgoLegPosition pe;
    pe.symbol = "NIFTY26JUN25P23200"; pe.instrument_token = 2004; pe.is_call = false;
    pe.strike = 23200; pe.side_sign = -1; pe.quantity = 50; pe.entry_price = 94.0;
    pe.current_price = 80.0; pe.unrealized_pnl = 700.0;
    QVector<AlgoLegPosition> basket; basket << ce << pe;

    const auto back = leg_positions_from_json(leg_positions_to_json(basket));
    check(back.size() == 2, "leg_positions JSON round-trip preserves count");
    if (back.size() == 2) {
        check(back[0].symbol == ce.symbol && back[0].instrument_token == 1004 &&
                  back[0].is_call && back[0].side_sign == 1 &&
                  qFuzzyCompare(back[0].quantity, 50.0) && qFuzzyCompare(back[0].entry_price, 104.0) &&
                  qFuzzyCompare(back[0].current_price, 104.0) && back[0].unrealized_pnl == 0.0,
              "CE leg round-trips (current_price=entry, unrealized=0 on restore)");
        check(back[1].instrument_token == 2004 && !back[1].is_call && back[1].side_sign == -1 &&
                  qFuzzyCompare(back[1].entry_price, 94.0),
              "PE leg round-trips (token, is_call=false, side_sign=-1)");
    }

    // ── leg_marks_from_chain: match by token, return symbol->LTP ─────────────────
    const auto chain = make_synthetic_chain(); // CE 1004 ltp=104, PE 2004 ltp=94
    const auto marks = leg_marks_from_chain(basket, chain);
    check(marks.size() == 2 &&
              qFuzzyCompare(marks.value("NIFTY26JUN25C23200"), 104.0) &&
              qFuzzyCompare(marks.value("NIFTY26JUN25P23200"), 94.0),
          "leg_marks_from_chain: both legs marked at chain LTP (CE 104, PE 94)");

    // A leg whose token is not in the chain is omitted (never marked to a stale 0).
    AlgoLegPosition ghost;
    ghost.symbol = "GHOST"; ghost.instrument_token = 999999; ghost.is_call = true;
    const auto marks2 = leg_marks_from_chain({ghost}, chain);
    check(marks2.isEmpty(), "leg_marks_from_chain: unknown-token leg omitted (no stale 0 mark)");
}

} // namespace

int run_fno_algo_selftest() {
    std::setvbuf(stdout, nullptr, _IONBF, 0); // unbuffered so output survives a crash mid-test
    std::fprintf(stdout, "\n=== F&O ALGO SELF-TEST ===\n");
    g_failures = 0;
    test_leg_json_roundtrip();
    test_v046_schema();
    test_persistence_roundtrip();
    test_leg_resolver();
    test_preview_bridge();
    test_leg_rule_editor();
    test_fno_data_bridge();
    test_position_manager_multileg();
    test_fno_execution();
    test_paper_basket_flow();
    test_fno_leg_marks_and_persistence();
    std::fprintf(stdout, "\n%s (%d failure(s))\n", g_failures == 0 ? "OVERALL PASS" : "OVERALL FAIL",
                 g_failures);
    return g_failures == 0 ? 0 : 1;
}

} // namespace fincept::algo::fno
