# Alpha Arena

Native C++ replay-faithful Nof1-style competition runtime, integrated with the
Fincept Terminal. Pits multiple LLM agents against each other on the same
crypto-perp universe, tick-by-tick, with cryptographically auditable replay.

The implementation lives entirely in the C++ engine; the Python footprint is a
single 200-line LLM subprocess with no business logic.

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│ AlphaArenaScreen (Qt UI)                                 │
│  • Header (venue, tick countdown, FORCE TICK, KILL ALL)  │
│  • LEADERBOARD (left)                                    │
│  • Right tabs: MODEL CHAT │ POSITIONS │ HITL │ RISK │ AUDIT
└──────────────────────────────────────────────────────────┘
              │  signals/slots only
              ▼
┌──────────────────────────────────────────────────────────┐
│ AlphaArenaEngine  (singleton service, persistent)         │
│  ├─ TickClock (QTimer, monotonic)                         │
│  ├─ ContextBuilder (deterministic prompt, byte-equal)     │
│  ├─ Indicators (EMA/MACD/RSI/ATR — pure C++)              │
│  ├─ ModelDispatcher (fan tick → N python subprocesses)    │
│  ├─ RiskEngine (pure fn: action → accept|reject|amend)    │
│  ├─ OrderRouter (route to IExchangeVenue)                 │
│  └─ HitlGate (modal/tray approvals, 60s timeout)          │
└──────────────────────────────────────────────────────────┘
              │
              ├──► IExchangeVenue (interface)
              │      ├─ HyperliquidVenue (live, REST+WS, EIP-712 sign)
              │      └─ PaperVenue (mirrors HL fees/funding/liq math)
              │
              ├──► AlphaArenaRepo (Qt SQL, WAL mode)
              │      Tables: aa_competitions, aa_agents, aa_ticks,
              │              aa_prompts, aa_decisions, aa_orders,
              │              aa_fills, aa_positions, aa_pnl_snapshots,
              │              aa_events, aa_hitl_approvals
              │
              └──► PythonRunner → scripts/alpha_arena/llm_call.py
                     (one subprocess per model per tick;
                      input: prompt+model+secret_handle on stdin;
                      output: response_text+usage on stdout)
```

## Replication fidelity vs Nof1 S1

| Aspect | Nof1 S1 | Fincept Alpha Arena |
|---|---|---|
| Action grammar | 4 signals | 4 signals (`buy_to_enter` / `sell_to_enter` / `hold` / `close`) |
| Modes | Baseline / Monk / Situational | Identical |
| Cadence | 60–600 s | Configurable, default 180 s |
| Universe | 6 perps (BTC/ETH/SOL/BNB/DOGE/XRP) | Identical (`kPerpUniverse()`) |
| Leverage cap | 20× | 20× (enforced server-side) |
| Risk per trade | 2% of equity | 2% of equity (server-recomputed) |
| Liquidation buffer | 15% of mark | 15% of mark |
| One position per coin | Yes | Yes |
| Fees (live & paper) | Hyperliquid taker 0.045 % / maker 0.015 % | Identical |
| Funding | Hyperliquid 8 h schedule | Identical (real funding rate) |
| Slippage (paper) | n/a | 5 bps each side, 200 ms latency |

## Paper vs live differences

Paper mode exists not as a degraded preview but as a high-fidelity simulator:
mark prices come from real Hyperliquid public WebSocket, fee/funding/liq math
mirrors HL exactly. The differences:

* **Slippage** — paper assumes 5 bps each side. Real HL slippage depends on the
  agent order's size against the live book.
* **Latency** — paper uses 200 ms artificial latency. Live latency depends on
  network + HL match-engine.
* **Settlement** — paper settles instantly into local book. Live settles on
  Hyperliquid's L1.

## Replay & audit

Every tick is reproducible from the database. To replay an agent's history:

```sql
SELECT t.seq, p.text, d.raw_response, d.parsed_actions_json, d.risk_verdict_json
FROM aa_ticks t
JOIN aa_decisions d ON d.tick_id = t.id
JOIN aa_prompts p ON p.sha256 = d.user_prompt_sha256
WHERE t.competition_id = ?
  AND d.agent_id = ?
ORDER BY t.seq ASC;
```

`aa_events` is append-only with a strictly monotonic `seq`. The AUDIT tab tails
this stream live; replay tooling can rebuild any tick's full picture from
`aa_ticks` + `aa_prompts` + `aa_decisions` + `aa_orders` + `aa_fills`.

## Kill switches

* **KILL ALL** — closes every agent's open positions (market IOC) and stops
  the TickClock. Marks the competition `halted_by_user`.
* **Per-agent halt** — double-click a leaderboard row, choose "Halt".
* **Auto kill switch (per agent)** — opens after 3 consecutive parse failures,
  3 consecutive risk rejects, or 50 % drawdown.
* **Crash recovery** — on app start, any competition still in `running` state
  is surfaced via `crash_recovery_pending`; the user picks Resume / Halt /
  Reconcile.

## Live mode

Live mode is gated by:

1. A typed acknowledgement (`I UNDERSTAND THIS IS REAL MONEY`).
2. Two mandatory checkboxes (age ≥ 18, not in sanctioned jurisdiction).
3. Geofence on `QLocale::system().territory()` blocking US, NK, IR.
4. Per-competition agent-wallet private key, stored only in `SecureStorage`.

The geofence is a development-time guard — final responsibility for compliance
remains with the user. Build with `-DFINCEPT_UNSAFE_DISABLE_GEOFENCE=ON` to
bypass for development on a flagged locale; release builds must leave it OFF.

## Regulatory disclaimers

* **AGPL-3.0-or-later** — see `LICENSE`.
* **Not financial advice** — Alpha Arena is a research and benchmarking tool.
  Past performance ≠ future results. Models may produce poor or harmful trades.
  Live mode forwards real orders to a real venue and may incur real losses.
* **Not regulated as a brokerage** — Fincept Terminal does not custody funds.
  Live mode signs transactions with a private key the user controls.

## Dependencies

* Qt 6.8.3 (Widgets, Network, WebSockets, Sql)
* Python 3.11.9 (subprocess only — no Python in the engine)
* Hyperliquid live mode requires libsecp256k1 + a keccak256 primitive (pending
  FetchContent — Phase 5c follow-up).

## Testing

```powershell
cmake --preset win-release -DFINCEPT_BUILD_TESTS=ON
cmake --build --preset win-release
ctest --test-dir build/win-release --output-on-failure -R aa_
```

Test slices:

* `aa_schema_roundtrip` — JSON parse / serialize.
* `aa_schema_python_mirror` — drift between C++ schema header and `schema.json`.
* `aa_indicators` — EMA / RSI / MACD / ATR algebraic invariants.
* `aa_context_builder` — byte-equal-across-agents in baseline mode.
* `aa_repo` — replay determinism + WAL concurrency tolerance + monotonic events.
* `aa_risk_engine` — 30+ accept/reject/amend cases + circuit-breaker.
* `aa_paper_venue` — fees, liquidation, funding.
* `aa_tick_clock` — skip-on-in-flight invariant.
* `aa_hyperliquid_signer` — gated stub until secp256k1 + keccak land.
* `aa_panels_lifecycle` — panel construct / refresh smoke.
* `aa_ops` — crash recovery + state transitions.
* `aa_50tick_replay` — repo replay across 50 ticks × 4 agents.

## References

* `.grill-me/alpha-arena-grill.md` — design discussion (decision tree).
* `.grill-me/alpha-arena-production-refactor.md` — the implementation plan.
* `src/services/alpha_arena/AlphaArenaSchema.h` — single-source-of-truth grammar.
* `scripts/alpha_arena/schema.json` — Python mirror (drift-checked in CI).
