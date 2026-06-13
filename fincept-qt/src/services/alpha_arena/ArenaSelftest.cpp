// ArenaSelftest.cpp
// Headless self-test for the Alpha Arena rewrite (no GUI/network/LLM/keys).
// Run: QT_QPA_PLATFORM=offscreen FinceptTerminal --selftest-arena --profile aatest
// Sections are appended by each backend task; returns 0 iff every check passes.

#include "services/alpha_arena/ArenaDecisionParser.h"
#include "services/alpha_arena/ArenaEngine.h"
#include "services/alpha_arena/ArenaFakes.h"
#include "services/alpha_arena/ArenaIndicators.h"
#include "services/alpha_arena/ArenaLlmClient.h"
#include "services/alpha_arena/ArenaMarketData.h"
#include "services/alpha_arena/ArenaModelRegistry.h"
#include "services/alpha_arena/ArenaPrompts.h"
#include "services/alpha_arena/ArenaRisk.h"
#include "services/alpha_arena/ArenaSelftest.h"
#include "services/alpha_arena/ArenaStore.h"
#include "services/alpha_arena/PaperExchange.h"
#include "services/alpha_arena/ArenaTypes.h"
#include "services/llm/ProviderCatalog.h"

#include <QDateTime>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <cmath>
#include <cstdio>

namespace fincept::arena {
namespace {
bool approx(double a, double b, double eps = 0.01) { return std::fabs(a - b) <= eps; }
} // namespace

int run_arena_selftest() {
    int failures = 0;
    auto check = [&](const char* label, bool ok) {
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", label);
        if (!ok) ++failures;
    };

    // ── Section 1: store round-trips ────────────────────────────────────────
    auto& store = ArenaStore::instance();
    // Clean any prior selftest leftovers.
    if (auto lr = store.list_competitions(); lr.is_ok())
        for (const auto& c : lr.value())
            if (c.name.startsWith(QStringLiteral("__selftest__")))
                (void)store.delete_competition_cascade(c.id);

    ArenaConfig cfg;
    cfg.name = QStringLiteral("__selftest__arena");
    cfg.instruments = {QStringLiteral("BTC"), QStringLiteral("ETH")};
    cfg.capital_per_agent = 10000;
    ArenaAgentSpec a1; a1.provider = "openai";    a1.model_id = "gpt-test";    a1.display_name = "GPT";    a1.source_kind = "provider";
    ArenaAgentSpec a2; a2.provider = "anthropic"; a2.model_id = "claude-test"; a2.display_name = "Claude"; a2.source_kind = "provider";
    cfg.agents = {a1, a2};

    auto cid_r = store.insert_competition(cfg);
    check("store: insert_competition", cid_r.is_ok());
    const QString cid = cid_r.is_ok() ? cid_r.value() : QString();

    auto comp_r = store.competition(cid);
    check("store: competition fetch", comp_r.is_ok() && comp_r.value().name == cfg.name
          && comp_r.value().instruments == cfg.instruments
          && approx(comp_r.value().capital_per_agent, 10000));

    auto agents_r = store.agents_for(cid);
    check("store: 2 agents persisted", agents_r.is_ok() && agents_r.value().size() == 2);
    const QString aid = agents_r.is_ok() && !agents_r.value().isEmpty() ? agents_r.value()[0].id : QString();

    auto bal_r = store.account_balance(cid, aid);
    check("store: account seeded with capital", bal_r.is_ok() && approx(bal_r.value(), 10000));

    PositionRow pos; pos.competition_id = cid; pos.agent_id = aid; pos.coin = "BTC";
    pos.side = "long"; pos.qty = 0.1; pos.entry_price = 50000; pos.leverage = 3;
    check("store: upsert_position", store.upsert_position(pos).is_ok());
    pos.qty = 0.2;
    check("store: upsert_position update", store.upsert_position(pos).is_ok());
    auto pl_r = store.positions_for(cid, aid);
    check("store: position upserted not duplicated",
          pl_r.is_ok() && pl_r.value().size() == 1 && approx(pl_r.value()[0].qty, 0.2));

    // Rounds / decisions / orders / equity / events round-trips.
    check("store: insert_round", store.insert_round(cid, 1, 1000, "{}").is_ok());
    check("store: complete_round", store.complete_round(cid, 1, 2000, "completed").is_ok());
    auto seq_r = store.last_round_seq(cid);
    check("store: last_round_seq", seq_r.is_ok() && seq_r.value() == 1);

    for (int i = 1; i <= 3; ++i) {
        DecisionRow d;
        d.competition_id = cid; d.agent_id = aid; d.round_seq = i;
        d.status = "ok"; d.actions_json = QStringLiteral("[%1]").arg(i);
        check("store: insert_decision", store.insert_decision(d).is_ok());
    }
    auto recents = store.recent_decisions(cid, aid, 2);
    check("store: recent_decisions oldest-first window",
          recents.is_ok() && recents.value().size() == 2
          && recents.value()[0].round_seq == 2 && recents.value()[1].round_seq == 3);

    OrderRow o;
    o.competition_id = cid; o.agent_id = aid; o.round_seq = 1; o.coin = "BTC";
    o.side = "long"; o.action = "open"; o.qty = 0.1; o.price = 50000;
    o.notional_usd = 5000; o.status = "filled";
    check("store: insert_order", store.insert_order(o).is_ok());
    auto orders_r = store.orders_for(cid, aid, 10);
    check("store: orders_for round-trip",
          orders_r.is_ok() && orders_r.value().size() == 1
          && orders_r.value()[0].coin == "BTC" && approx(orders_r.value()[0].notional_usd, 5000));

    EquitySnapshotRow es;
    es.competition_id = cid; es.agent_id = aid; es.round_seq = 1; es.ts = 1500;
    es.equity = 10100; es.balance = 9000; es.unrealized_pnl = 100;
    check("store: insert_equity_snapshot", store.insert_equity_snapshot(es).is_ok());
    es.equity = 10200;
    check("store: equity snapshot upsert", store.insert_equity_snapshot(es).is_ok());
    auto eq_r = store.equity_series(cid);
    check("store: equity_series upserted not duplicated",
          eq_r.is_ok() && eq_r.value().size() == 1 && approx(eq_r.value()[0].equity, 10200));

    check("store: insert_event", store.insert_event(cid, aid, "test_event", "{\"k\":1}").is_ok());
    check("store: insert_event 2", store.insert_event(cid, aid, "test_event_2", "{}").is_ok());
    auto ev_r = store.recent_events(cid, 10);
    check("store: recent_events oldest-first",
          ev_r.is_ok() && ev_r.value().size() == 2
          && ev_r.value()[0].contains("test_event") && ev_r.value()[1].contains("test_event_2"));

    check("store: status update", store.set_competition_status(cid, "running").is_ok());
    auto running_r = store.competitions_with_status("running");
    check("store: competitions_with_status", running_r.is_ok() && running_r.value().contains(cid));

    check("store: cascade delete", store.delete_competition_cascade(cid).is_ok());
    auto gone = store.competition(cid);
    check("store: competition gone after cascade", gone.is_err());

    // ── Section 2: indicators ───────────────────────────────────────────────
    {
        QVector<Candle> cs;
        for (int i = 0; i < 60; ++i) { Candle k; k.t = i; k.o = k.h = k.l = k.c = 100.0; k.v = 1; cs.append(k); }
        check("ind: flat series ema == price", approx(ind_ema(cs, 20), 100.0));
        check("ind: flat series atr == 0", approx(ind_atr(cs, 14), 0.0));
        check("ind: flat series rsi neutral", approx(ind_rsi(cs, 14), 50.0, 1.0));
        QVector<Candle> up;
        for (int i = 0; i < 60; ++i) { Candle k; k.c = 100.0 + i; k.o = k.c - 1; k.h = k.c + 1; k.l = k.o - 1; up.append(k); }
        check("ind: rising series rsi > 70", ind_rsi(up, 14) > 70.0);
        check("ind: ema20 > ema50 in uptrend", ind_ema(up, 20) > ind_ema(up, 50));
        check("ind: short series safe", approx(ind_ema({}, 20), 0.0) && approx(ind_rsi({}, 14), 50.0));
    }

    // ── Section 3: decision parser ──────────────────────────────────────────
    {
        const QStringList instr = {"BTC", "ETH"};
        auto p1 = parse_decision(R"({"actions":[{"action":"open","coin":"BTC","side":"long","size_usd":2000,"leverage":3,"reason":"r"}]})", instr, 10);
        check("parse: clean json", p1.ok() && p1.actions.size() == 1
              && p1.actions[0].kind == ActionKind::Open && p1.actions[0].coin == "BTC"
              && approx(p1.actions[0].size_usd, 2000) && approx(p1.actions[0].leverage, 3));
        auto p2 = parse_decision("Here is my plan:\n```json\n{\"actions\":[{\"action\":\"hold\",\"reason\":\"wait\"}]}\n```\nthanks", instr, 10);
        check("parse: fenced json", p2.ok() && p2.actions.size() == 1 && p2.actions[0].kind == ActionKind::Hold);
        auto p3 = parse_decision("I think {\"actions\":[{\"action\":\"close\",\"coin\":\"ETH\",\"reason\":\"tp\"}]} fits", instr, 10);
        check("parse: embedded json", p3.ok() && p3.actions[0].kind == ActionKind::Close && p3.actions[0].coin == "ETH");
        auto p3b = parse_decision(
            "ok: {\"actions\":[{\"action\":\"close\",\"coin\":\"ETH\",\"reason\":\"stray } brace\"}]} end", instr, 10);
        check("parse: embedded json with brace inside string", p3b.ok() && p3b.actions[0].coin == "ETH");
        check("parse: garbage fails", !parse_decision("no json here", instr, 10).ok());
        check("parse: unknown coin fails", !parse_decision(R"({"actions":[{"action":"open","coin":"DOGE","side":"long","size_usd":100,"leverage":2}]})", instr, 10).ok());
        check("parse: leverage cap fails", !parse_decision(R"({"actions":[{"action":"open","coin":"BTC","side":"long","size_usd":100,"leverage":50}]})", instr, 10).ok());
        check("parse: bad side fails", !parse_decision(R"({"actions":[{"action":"open","coin":"BTC","side":"sideways","size_usd":100,"leverage":2}]})", instr, 10).ok());
        check("parse: empty actions fails", !parse_decision(R"({"actions":[]})", instr, 10).ok());
    }

    // ── Section 4: paper exchange ───────────────────────────────────────────
    {
        ArenaConfig pcfg;
        pcfg.name = "__selftest__px";
        pcfg.instruments = {"BTC"};
        pcfg.capital_per_agent = 10000;
        ArenaAgentSpec s; s.provider = "openai"; s.model_id = "m"; s.display_name = "A"; s.source_kind = "provider";
        pcfg.agents = {s};
        const QString pcid = store.insert_competition(pcfg).value();
        const QString paid = store.agents_for(pcid).value()[0].id;

        PaperExchange px(store, pcid);
        px.load();
        px.set_marks({{"BTC", 50000.0}});

        AgentAction open; open.kind = ActionKind::Open; open.coin = "BTC"; open.side = "long";
        open.size_usd = 3000; open.leverage = 3;
        auto f1 = px.execute(paid, open, 1);
        check("px: open fills", f1.is_ok() && f1.value().status == "filled");
        // entry = 50000*(1+slippage) = 50010; qty = 3000/50010; margin = 1000; fee = 3000*0.00045 = 1.35
        auto av1 = px.account(paid);
        check("px: margin+fee deducted", approx(av1.balance, 10000 - 1000 - 1.35, 0.05));
        check("px: position open", av1.positions.size() == 1 && av1.positions[0].side == "long");
        // equity ≈ balance + margin + upnl(small negative from slippage)
        check("px: equity near capital", approx(av1.equity, 10000 - 1.35 - 3000 * PaperExchange::kSlippage, 1.5));

        px.set_marks({{"BTC", 55000.0}});   // +~10% on 3x → upnl ≈ 3000*0.0998 ≈ 299
        auto av2 = px.account(paid);
        check("px: upnl rises with mark", av2.upnl.value("BTC") > 250);

        // Same-side open averages in.
        auto f2 = px.execute(paid, open, 2);
        auto av3 = px.account(paid);
        check("px: same-side open increases qty",
              f2.is_ok() && av3.positions.size() == 1 && av3.positions[0].qty > av1.positions[0].qty);

        // Opposite-side open = flip: closes existing, opens new side.
        AgentAction flip = open; flip.side = "short"; flip.size_usd = 1000; flip.leverage = 2;
        auto f3 = px.execute(paid, flip, 3);
        auto av4 = px.account(paid);
        check("px: flip leaves single short", f3.is_ok() && av4.positions.size() == 1 && av4.positions[0].side == "short");
        // Closed a winning long: the flip's close order (persisted, round 3) carries the realized pnl.
        bool flip_pnl = false;
        if (auto fo = store.orders_for(pcid, paid, 50); fo.is_ok())
            for (const auto& ord : fo.value())
                if (ord.round_seq == 3 && ord.action == "close" && ord.realized_pnl > 0) flip_pnl = true;
        check("px: flip realized pnl recorded", flip_pnl);

        // Full close.
        AgentAction close; close.kind = ActionKind::Close; close.coin = "BTC";
        auto f4 = px.execute(paid, close, 4);
        auto av5 = px.account(paid);
        check("px: close flattens", f4.is_ok() && av5.positions.isEmpty());
        check("px: equity == balance when flat", approx(av5.equity, av5.balance, 0.001));

        // Funding accrual: open long, positive funding → cost reduces equity.
        px.execute(paid, open, 5);
        const double eq_before = px.account(paid).equity;
        // (mark_all needs funding rates: set via set_marks + funding map — see impl: funding
        //  rates ride in set_funding())
        px.set_funding({{"BTC", 0.0001}});
        px.mark_all(3600 * 1000, 5);    // one full 1h funding window (HL rates are hourly)
        check("px: positive funding costs longs", px.account(paid).equity < eq_before);

        // Liquidation: crash the mark far below entry.
        px.set_marks({{"BTC", 30000.0}});
        auto liqs = px.mark_all(1000, 6);
        check("px: liquidation fires", liqs.size() == 1 && liqs[0].status == "liquidated");
        check("px: liquidated position removed", px.account(paid).positions.isEmpty());

        // close on empty book is rejected
        check("px: close with no position rejected", px.execute(paid, close, 7).is_err());

        (void)store.delete_competition_cascade(pcid);
    }

    // ── Section 5: risk ─────────────────────────────────────────────────────
    {
        AccountView acct; acct.balance = 10000; acct.equity = 10000;
        RiskLimits lim;
        AgentAction a; a.kind = ActionKind::Open; a.coin = "BTC"; a.side = "long";
        a.size_usd = 4000; a.leverage = 3;
        check("risk: sane open approved", evaluate(a, acct, lim, 0).approved);
        a.size_usd = 6000;
        check("risk: >50% equity rejected", !evaluate(a, acct, lim, 0).approved);
        a.size_usd = 4000; a.leverage = 20;
        check("risk: leverage cap rejected", !evaluate(a, acct, lim, 0).approved);
        a.leverage = 3;
        check("risk: round cap rejected", !evaluate(a, acct, lim, 7000).approved);
        AgentAction c; c.kind = ActionKind::Close; c.coin = "BTC";
        check("risk: close always approved", evaluate(c, acct, lim, 99999).approved);
    }

    // ── Section 6: model registry ───────────────────────────────────────────
    {
        auto& reg = ArenaModelRegistry::instance();
        const auto opts = reg.available_models();
        QSet<QString> keys;
        bool dup = false, blank = false;
        for (const auto& o : opts) {
            const QString k = o.provider.toLower() + "/" + o.model_id;
            if (keys.contains(k)) dup = true;
            keys.insert(k);
            if (o.color_hex.isEmpty() || o.display_name.isEmpty() || o.source_kind.isEmpty()) blank = true;
        }
        check("registry: no duplicate (provider,model)", !dup);
        check("registry: every option fully labelled", !blank);
        check("registry: unknown profile ref fails resolution",
              reg.resolve_credentials("profile", "no-such-id", "openai").is_err());
        check("registry: keyless provider resolves without key",
              reg.resolve_credentials("provider", "ollama", "ollama").is_ok());

        // Browse layer (merge layer 4): every catalog provider with fallback
        // models is listed even with no llm_configs row, so the wizard can
        // offer all providers.
        const auto opts2 = reg.available_models();
        QSet<QString> providers_seen;
        bool ollama_ready = false;
        for (const auto& o : opts2) {
            providers_seen.insert(o.provider.toLower());
            if (o.provider.toLower() == "ollama" && o.ready) ollama_ready = true;
        }
        check("registry: all known providers browsable",
              providers_seen.contains("openai") && providers_seen.contains("anthropic")
              && providers_seen.contains("gemini") && providers_seen.contains("deepseek")
              && providers_seen.contains("xai"));
        // Ollama's fallback list is intentionally empty (models come from a
        // live /api/tags fetch the registry doesn't do), so it may be absent in
        // an unconfigured profile; whenever it IS listed it must be ready
        // (keyless), from any layer.
        check("registry: keyless ollama listed but has no fallback models or is ready",
              !providers_seen.contains("ollama") || ollama_ready);
    }

    // ── Section 7: prompts ──────────────────────────────────────────────────
    {
        CompetitionRow comp; comp.capital_per_agent = 10000; comp.instruments = {"BTC", "ETH"};
        comp.max_leverage = 10; comp.cadence_seconds = 180;
        const QString sys = system_prompt(comp);
        check("prompt: system mentions instruments", sys.contains("BTC, ETH"));
        check("prompt: system carries JSON contract", sys.contains("\"actions\""));
        comp.system_prompt_override = "You are a turtle trader.";
        const QString sys2 = system_prompt(comp);
        check("prompt: override kept + contract appended",
              sys2.startsWith("You are a turtle trader.") && sys2.contains("\"actions\""));

        MarketSnapshot ms; CoinSnapshot btc; btc.coin = "BTC"; btc.mid = 50000; ms.coins["BTC"] = btc;
        ms.ts = 1000LL * 60 * 9;   // 9 min after start → "elapsed 9 min"
        AccountView av; av.balance = av.equity = 10000;
        const QString up = user_prompt(ms, av, {}, 3, /*started_at=*/0);
        check("prompt: user carries round + market + account",
              up.contains("ROUND 3") && up.contains("BTC: mid 50000") && up.contains("no open positions"));
        check("prompt: deterministic from snapshot ts",
              up.contains("elapsed 9 min") && up == user_prompt(ms, av, {}, 3, 0));
    }

    // ── Section 8: llm client pure helpers ──────────────────────────────────
    {
        ArenaLlmRequest rq; rq.provider = "anthropic"; rq.model_id = "claude-x";
        rq.system_prompt = "S"; rq.user_prompt = "U"; rq.max_tokens = 1000;
        const auto abody = QJsonDocument::fromJson(ArenaLlmClient::build_body(rq)).object();
        check("llm: anthropic body has system + messages",
              abody.value("system").toString() == "S" && abody.value("messages").toArray().size() == 1);
        rq.provider = "openai";
        const auto obody = QJsonDocument::fromJson(ArenaLlmClient::build_body(rq)).object();
        check("llm: openai body uses max_completion_tokens",
              obody.contains("max_completion_tokens") && obody.value("messages").toArray().size() == 2);
        rq.provider = "deepseek";
        check("llm: openai-compat body uses max_tokens",
              QJsonDocument::fromJson(ArenaLlmClient::build_body(rq)).object().contains("max_tokens"));

        auto pr1 = ArenaLlmClient::parse_response("openai",
            R"({"choices":[{"message":{"content":"hi"}}],"usage":{"prompt_tokens":5,"completion_tokens":2}})");
        check("llm: openai parse", pr1.success && pr1.content == "hi" && pr1.prompt_tokens == 5);
        auto pr2 = ArenaLlmClient::parse_response("anthropic",
            R"({"content":[{"type":"text","text":"yo"}],"usage":{"input_tokens":7,"output_tokens":3}})");
        check("llm: anthropic parse", pr2.success && pr2.content == "yo" && pr2.completion_tokens == 3);
        auto pr3 = ArenaLlmClient::parse_response("gemini",
            R"({"candidates":[{"content":{"parts":[{"text":"g"}]}}],"usageMetadata":{"promptTokenCount":9,"candidatesTokenCount":4}})");
        check("llm: gemini parse", pr3.success && pr3.content == "g" && pr3.prompt_tokens == 9);
        auto pr4 = ArenaLlmClient::parse_response("openai", R"({"error":{"message":"bad key"}})");
        check("llm: provider error surfaced", !pr4.success && pr4.error == "bad key");
        check("llm: endpoint composition",
              fincept::ai_chat::ProviderCatalog::chat_endpoint("openai", "", "m") == "https://api.openai.com/v1/chat/completions"
              && fincept::ai_chat::ProviderCatalog::chat_endpoint("deepseek", "https://my.proxy/v1", "m") == "https://my.proxy/v1/chat/completions"
              && fincept::ai_chat::ProviderCatalog::chat_endpoint("anthropic", "https://gw.corp", "m") == "https://gw.corp/v1/messages");
    }

    // ── Section 9: market data parsers ──────────────────────────────────────
    {
        const auto mids_doc = QJsonDocument::fromJson(R"({"BTC":"94126.0","ETH":"3120.5","#1000":"0.5"})");
        const auto mids = ArenaMarketData::parse_all_mids(mids_doc, {"BTC", "ETH"});
        check("md: mids parsed + builder assets filtered",
              mids.size() == 2 && approx(mids.value("BTC"), 94126.0) && !mids.contains("#1000"));

        MarketSnapshot snap;
        CoinSnapshot b; b.coin = "BTC"; b.mid = 94126.0; snap.coins["BTC"] = b;
        const auto ctx_doc = QJsonDocument::fromJson(
            R"([{"universe":[{"name":"BTC"},{"name":"ETH"}]},)"
            R"([{"funding":"0.0000125","openInterest":"12345.5","prevDayPx":"90000.0"},)"
            R"({"funding":"0.0001","openInterest":"99","prevDayPx":"3000"}]])");
        ArenaMarketData::apply_asset_ctxs(ctx_doc, snap);
        check("md: ctxs applied by index", approx(snap.coins["BTC"].funding_rate, 0.0000125, 1e-9)
              && approx(snap.coins["BTC"].open_interest, 12345.5)
              && approx(snap.coins["BTC"].day_change_pct, (94126.0 - 90000.0) / 90000.0 * 100.0, 0.01));

        const auto candle_doc = QJsonDocument::fromJson(
            R"([{"t":1765299600000,"o":"94126.0","c":"93858.0","h":"94569.0","l":"93485.0","v":"3559.7"}])");
        const auto candles = ArenaMarketData::parse_candles(candle_doc);
        check("md: candles parsed", candles.size() == 1 && approx(candles[0].c, 93858.0)
              && candles[0].t == 1765299600000LL);
    }

    // ── Section 10: engine round (fakes; no network) ────────────────────────
    {
        auto* fake_md = new FakeMarketData(nullptr);
        CoinSnapshot btc; btc.coin = "BTC"; btc.mid = 50000;
        fake_md->next.coins["BTC"] = btc;
        auto* fake_llm = new FakeLlmClient(nullptr);
        // gpt: clean open. claude: garbage then garbage again (exhausts the 1 retry).
        fake_llm->scripts["gpt-test"].enqueue(R"({"actions":[{"action":"open","coin":"BTC","side":"long","size_usd":2000,"leverage":2,"reason":"momentum"}]})");
        fake_llm->scripts["claude-test"].enqueue("I refuse to answer in JSON.");
        fake_llm->scripts["claude-test"].enqueue("Still prose.");

        auto& eng = ArenaEngine::instance();
        eng.set_market_for_test(fake_md);
        eng.set_llm_for_test(fake_llm);

        ArenaConfig ecfg;
        ecfg.name = "__selftest__engine";
        ecfg.instruments = {"BTC"};
        ecfg.capital_per_agent = 10000;
        ecfg.cadence_seconds = 3600;          // timer never fires during the test
        // Both agents ride the keyless "ollama" provider so resolve_credentials
        // succeeds without any configured key; model_ids select the fake scripts.
        ArenaAgentSpec g; g.provider = "ollama"; g.model_id = "gpt-test";
        g.display_name = "GPT"; g.source_kind = "provider"; g.source_ref = "ollama";
        ArenaAgentSpec c = g; c.model_id = "claude-test"; c.display_name = "Claude";
        ecfg.agents = {g, c};

        const QString ecid = ArenaEngine::instance().create_competition(ecfg).value();
        QEventLoop loop;
        int completed_seq = 0;
        QObject::connect(&eng, &ArenaEngine::round_completed, &loop, [&](int s) { completed_seq = s; loop.quit(); });
        check("engine: start ok", eng.start(ecid).is_ok());
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);   // watchdog
        loop.exec();
        check("engine: round completed", completed_seq == 1);

        auto decs = store.decisions_for_round(ecid, 1);
        check("engine: 2 decisions persisted", decs.is_ok() && decs.value().size() == 2);
        int ok_count = 0, perr_count = 0;
        for (const auto& d : decs.is_ok() ? decs.value() : QVector<DecisionRow>{}) {
            if (d.status == "ok") ++ok_count;
            if (d.status == "parse_error") ++perr_count;
        }
        check("engine: one ok decision, one parse_error (after retry)", ok_count == 1 && perr_count == 1);
        // The parse-error agent consumed both scripted responses (retry happened).
        check("engine: repair retry consumed second response", fake_llm->scripts["claude-test"].isEmpty());

        const auto agents_now = store.agents_for(ecid).value();
        QString gpt_id, claude_id;
        for (const auto& a : agents_now) (a.model_id == "gpt-test" ? gpt_id : claude_id) = a.id;
        auto orders = store.orders_for(ecid, gpt_id, 10);
        check("engine: gpt order filled", orders.is_ok() && orders.value().size() == 1
              && orders.value()[0].status == "filled" && approx(orders.value()[0].notional_usd, 2000));
        auto toks = store.token_totals(ecid);
        check("store: token totals aggregated", toks.is_ok() && toks.value().value(gpt_id) > 0);
        auto eq = store.equity_series(ecid);
        check("engine: equity snapshots for both agents", eq.is_ok() && eq.value().size() == 2);
        check("engine: parse failure did not block round",
              eq.is_ok() && eq.value().size() == 2 && completed_seq == 1);

        // Failure counting: claude has 1 consecutive failure (threshold 3 → still active).
        for (const auto& a : agents_now)
            if (a.id == claude_id)
                check("engine: failure counter incremented", a.consecutive_failures == 1 && a.status == "active");

        // Crash-recovery surface + restart guard.
        check("engine: no phantom crash recoveries", eng.pending_crash_recoveries().isEmpty());

        check("engine: kill_all + stop", eng.kill_all().is_ok());
        (void)store.delete_competition_cascade(ecid);

        // Restart guard: starting a new competition must end the previous one
        // ("never two running"). Fakes are still installed; nothing enqueued so
        // both rounds resolve to default hold — and start()'s first round is
        // async anyway, while these status assertions are synchronous. Any
        // late callbacks are stale-epoch no-ops (Fix: epoch guard).
        ecfg.name = "__selftest__engine2";
        const QString ecid2 = eng.create_competition(ecfg).value();
        check("engine: start second ok", eng.start(ecid2).is_ok());
        ecfg.name = "__selftest__engine3";
        const QString ecid3 = eng.create_competition(ecfg).value();
        check("engine: start third ok (auto-stops second)", eng.start(ecid3).is_ok());
        auto c2 = store.competition(ecid2);
        auto c3 = store.competition(ecid3);
        check("engine: second auto-ended on restart", c2.is_ok() && c2.value().status == "ended");
        check("engine: third running", c3.is_ok() && c3.value().status == "running");
        (void)eng.kill_all();   // only the active (third) competition
        (void)store.delete_competition_cascade(ecid2);
        (void)store.delete_competition_cascade(ecid3);

        // HITL: live_mode competitions queue orders until approved. venue stays
        // "paper" — live_mode alone gates HITL, so no real order mirroring runs.
        ArenaConfig hcfg = ecfg;
        hcfg.name = "__selftest__hitl";
        hcfg.live_mode = true;
        hcfg.agents = {g};
        fake_llm->scripts["gpt-test"].enqueue(
            R"({"actions":[{"action":"open","coin":"BTC","side":"long","size_usd":1000,"leverage":2,"reason":"r"}]})");
        const QString hcid = eng.create_competition(hcfg).value();
        QString approval_id, hitl_agent;
        QObject::connect(&eng, &ArenaEngine::hitl_requested, &loop,
                         [&](QString id, QString aid, QString) { approval_id = id; hitl_agent = aid; });
        int hseq = 0;
        QObject::connect(&eng, &ArenaEngine::round_completed, &loop, [&](int s) { hseq = s; });
        check("hitl: start", eng.start(hcid).is_ok());
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);   // watchdog (round_completed quits via the Section-10 connect)
        loop.exec();
        check("hitl: round completed with approval pending", hseq == 1);
        check("hitl: approval requested", !approval_id.isEmpty());
        const QString hagent = store.agents_for(hcid).value()[0].id;
        check("hitl: request from the right agent", hitl_agent == hagent);
        check("hitl: no order before approval",
              store.orders_for(hcid, hagent, 10).value().isEmpty());
        eng.resume_after_hitl(approval_id, true);
        const auto horders = store.orders_for(hcid, hagent, 10);
        check("hitl: order executed after approval",
              horders.is_ok() && horders.value().size() == 1
                  && horders.value()[0].status == "filled");
        (void)eng.kill_all();
        (void)store.delete_competition_cascade(hcid);

        // Immediate halt (user-reported bug): an agent halted while its LLM
        // call is in flight must execute NOTHING when the callback lands.
        // Event ordering makes this deterministic with the singleShot(0) fakes:
        // start() queues the snapshot reply (timer A); our queued halt (timer B)
        // runs after A — i.e. after on_snapshot dispatched the agent — while the
        // fake LLM reply (timer C, queued during A) fires only on the next event
        // pass. So finish_agent lands on an already-halted agent.
        ArenaConfig kcfg = ecfg;
        kcfg.name = "__selftest__halt";
        kcfg.agents = {g};   // gpt-test only
        fake_llm->scripts["gpt-test"].enqueue(
            R"({"actions":[{"action":"open","coin":"BTC","side":"long","size_usd":1500,"leverage":2,"reason":"r"}]})");
        const QString kcid = eng.create_competition(kcfg).value();
        check("halt: start", eng.start(kcid).is_ok());
        const QString kaid = store.agents_for(kcid).value()[0].id;
        QTimer::singleShot(0, &loop, [&eng, &kaid]() { (void)eng.halt_agent(kaid); });
        completed_seq = 0;
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);   // watchdog
        loop.exec();
        check("halt: round completed", completed_seq == 1);
        auto kdecs = store.decisions_for_round(kcid, 1);
        check("halt: decision persisted with real status",
              kdecs.is_ok() && kdecs.value().size() == 1 && kdecs.value()[0].status == "ok");
        auto korders = store.orders_for(kcid, kaid, 10);
        check("halt: no orders for halted agent", korders.is_ok() && korders.value().isEmpty());
        bool halt_event = false;
        if (auto ev = store.recent_events(kcid, 20); ev.is_ok())
            for (const auto& e : ev.value())
                if (e.contains("actions_skipped_agent_halted")) halt_event = true;
        check("halt: actions_skipped_agent_halted event", halt_event);

        // kill_agent: close positions now + halt ("halted_user") + equity snapshot.
        check("kill: unknown agent rejected", eng.kill_agent("no-such-agent").is_err());
        check("kill: resume_agent", eng.resume_agent(kaid).is_ok());
        fake_llm->scripts["gpt-test"].enqueue(
            R"({"actions":[{"action":"open","coin":"BTC","side":"long","size_usd":2000,"leverage":2,"reason":"r"}]})");
        completed_seq = 0;
        eng.force_round();
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);   // watchdog
        loop.exec();
        auto kpos = store.positions_for(kcid, kaid);
        check("kill: setup round opened a position",
              completed_seq == 2 && kpos.is_ok() && kpos.value().size() == 1);
        check("kill: kill_agent ok", eng.kill_agent(kaid).is_ok());
        kpos = store.positions_for(kcid, kaid);
        check("kill: positions flattened", kpos.is_ok() && kpos.value().isEmpty());
        bool kill_close = false;
        if (auto ko = store.orders_for(kcid, kaid, 50); ko.is_ok())
            for (const auto& o : ko.value())
                if (o.action == "close" && o.status == "filled") kill_close = true;
        check("kill: close order recorded", kill_close);
        QString kstatus;
        const auto kagents = store.agents_for(kcid).value();   // named copy — .value() of a
                                                               // temporary Result dangles in range-for
        for (const auto& a : kagents)
            if (a.id == kaid) kstatus = a.status;
        check("kill: agent halted_user", kstatus == "halted_user");
        bool kill_event = false;
        if (auto ev = store.recent_events(kcid, 20); ev.is_ok())
            for (const auto& e : ev.value())
                if (e.contains("agent_killed")) kill_event = true;
        check("kill: agent_killed event", kill_event);

        // Live marks between rounds: shrink the marks cadence and expect at
        // least one marks_updated (fake fetch_mids roundtrip) within 2s.
        int marks_count = 0;
        QObject::connect(&eng, &ArenaEngine::marks_updated, &loop, [&]() {
            ++marks_count;
            loop.quit();
        });
        eng.set_marks_interval_for_test(50);
        QTimer::singleShot(2000, &loop, &QEventLoop::quit);   // watchdog
        loop.exec();
        check("marks: marks_updated fired between rounds", marks_count >= 1);

        (void)eng.kill_all();
        (void)store.delete_competition_cascade(kcid);
    }

    std::printf("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}

} // namespace fincept::arena
