#include "algo_engine/fno/FnoAlgoSelftest.h"

#include "algo_engine/fno/FnoAlgoTypes.h"
#include "algo_engine/fno/FnoLegResolver.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "services/algo_trading/AlgoTradingTypes.h"
#include "storage/sqlite/Database.h"

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

} // namespace

int run_fno_algo_selftest() {
    std::setvbuf(stdout, nullptr, _IONBF, 0); // unbuffered so output survives a crash mid-test
    std::fprintf(stdout, "\n=== F&O ALGO SELF-TEST ===\n");
    g_failures = 0;
    test_leg_json_roundtrip();
    test_v046_schema();
    test_persistence_roundtrip();
    test_leg_resolver();
    std::fprintf(stdout, "\n%s (%d failure(s))\n", g_failures == 0 ? "OVERALL PASS" : "OVERALL FAIL",
                 g_failures);
    return g_failures == 0 ? 0 : 1;
}

} // namespace fincept::algo::fno
