# Crypto Center — Phase 4 user guide (MARKETS)

The **MARKETS** tab is Fincept's **internal prediction market** entry point. Markets are denominated and settled in **$FNCPT only** — no USDC, no off-chain ledger. Examples (curated demo dataset until launch):

- "Fed cuts in May 2026"
- "NYC max 80F+ on May 1"
- "$FNCPT > $0.0001 by month end"

This is a third sibling alongside Polymarket and Kalshi in the terminal's prediction-market tooling. Browsing on the MARKETS tab feels identical to those — same `PredictionExchangeAdapter` interface, same data shape, same hub-driven refresh pattern.

---

## What you see today

```
┌─ MARKETS ────────────────────────────────────────────── DEMO ────────┐
│  MARKET                              YES   NO    24h VOL   EXPIRES   │
│  Fed cuts in May 2026               0.62  0.38   1.2M      2026-05-07│
│  NYC max 80F+ on May 1              0.71  0.29   83k       2026-05-01│
│  $FNCPT > $0.0001 by month end      0.45  0.55   421k      2026-05-31│
│                                                                       │
│  Demo dataset. Set `fincept.markets_endpoint` in SecureStorage and    │
│  deploy the fincept_market Anchor program for live trading.           │
└───────────────────────────────────────────────────────────────────────┘
```

---

## Demo mode (current)

Phase 4 ships in **demo mode**. Two things have to land before the tab becomes transactional:

1. The Fincept **matching engine** (HTTP + WebSocket on `markets.fincept.in`). When this is up, set `fincept.markets_endpoint` in SecureStorage and the adapter will fetch live markets, books, and price history.
2. The on-chain **`fincept_market` Anchor program** that escrows YES/NO collateral and resolves payouts. When deployed, set `fincept.market_program_id` and the order-entry path enables.

Until both are present, the MARKETS tab serves the curated three-market dataset baked into `FinceptInternalAdapter` so you can see what the screen will look like and verify the data shapes integrate cleanly with the unified `PredictionExchangeAdapter` (which also serves Polymarket and Kalshi).

---

## How it integrates

The Fincept internal adapter is registered with `PredictionExchangeRegistry` alongside `PolymarketAdapter` and `KalshiAdapter`. Any code that already iterates the registry (Polymarket / Kalshi screen, command bar, MCP tools) can switch to `fincept` without special-casing.

**Hub topics** are reserved (with policies) for the live path:

- `prediction:fincept:markets` — market list, refreshes every 30 s.
- `prediction:fincept:orderbook:<asset_id>` — per-asset book, WebSocket-driven.
- `prediction:fincept:price:<asset_id>` — last-trade price scalar.

When the matching engine ships, the adapter will publish to those topics; existing consumers attach automatically.

---

## What you can't do yet

- **Place orders.** `place_order` returns `error_occurred("not deployed", …)` while in demo mode — the order ticket UI shows a clear "service unavailable" banner.
- **See your positions.** Same gate — needs the on-chain program for any real escrow to exist.
- **Create markets.** Plan §4.1 reserves market creation for **Gold tier** users; the proposal/vote flow lands with the engine.
- **Earn from order flow.** veFNCPT lockers will receive a share of trading fees (Phase 3 §3.4) once the engine is settling real volume.

---

## When the engine ships

You won't need to update the terminal — the seam is already in place. Configuration lives in SecureStorage:

- `fincept.markets_endpoint` — base URL of the matching engine. Empty = demo mode.
- `fincept.market_program_id` — base58 program ID for `fincept_market`. Empty = trading disabled.

Both keys are user-configurable from Settings → Crypto → Advanced. The MARKETS tab will pick them up on the next visibility tick.

---

## Why $FNCPT-only

Real-money prediction markets are regulated as derivatives in many jurisdictions. By denominating in $FNCPT we keep the venue framed as a **play-money internal mechanic** of the terminal, not a regulated financial product. We may geo-block sign-up in regions where legal review concludes otherwise.

This is a policy decision; the engineering plan ships the same regardless. See `plans/crypto-center-future-phases.md` §4.7 for the rationale.
