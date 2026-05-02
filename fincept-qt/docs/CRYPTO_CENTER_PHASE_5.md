# Crypto Center — Phase 5 user guide (Buyback & Burn)

The **ROADMAP** tab in the Crypto Center shows Fincept's **value loop**: how much terminal revenue is being spent buying $FNCPT off the market and burning it. This is the demand engine that justifies the token long-term.

---

## What you see

```
┌─ BUYBACK & BURN ─ epoch 47 · 2026-04-21 → 2026-04-27 ─── ● LIVE ────┐
│  THIS EPOCH                                                          │
│    REVENUE          $42,310  (subs $32k · pred-mkt $8k · misc $2k)  │
│    BUYBACK          $21,155  (50% of revenue)                       │
│    $FNCPT BOUGHT    82.4 M $FNCPT  (avg $0.0002570)                 │
│    $FNCPT BURNED    82.4 M $FNCPT                                   │
│    BURN TX          5kj3w…XXXX  (click to open Solscan)             │
│  ────────────────────────────────────────────────────────           │
│  ALL-TIME                                                            │
│    BURNED           1.42 B $FNCPT                                   │
│    SUPPLY REMAINING 8.58 B $FNCPT                                   │
│    SPENT ON BUYBACK $384,201.00                                     │
└──────────────────────────────────────────────────────────────────────┘

┌─ SUPPLY CHART · 12 MONTHS ─── ● TOTAL  ● CIRCULATING  ● BURNED ─────┐
│ [ time-series chart, three lines, last 12 months ]                  │
└──────────────────────────────────────────────────────────────────────┘

┌─ TREASURY ───────────────────────────────────── ● LIVE ─────────────┐
│   USDC RESERVES      $1.84M                                         │
│   SOL RESERVES       234.00 SOL  ($19.7k)                           │
│   TOTAL USD          $1.86M                                         │
│   RUNWAY @ CURRENT   18 months  ($100,000/mo)                       │
│   MULTI-SIG          3-of-5      (click to open Squads)             │
└──────────────────────────────────────────────────────────────────────┘
```

Three panels stacked, scrollable on short windows. Each has its own LIVE / DEMO pill showing whether the data is real or mocked (see below).

---

## Where the data comes from

A separate **Fincept buyback worker** (lives outside this repo, in `services/buyback-worker/`) does the heavy lifting:

1. **Tally revenue** — sums Stripe subscriptions + on-chain prediction-market fees + misc, weekly.
2. **Compute budget** — initial split is **50% buyback / 25% staker yield / 25% treasury**.
3. **Execute** — splits the budget into hourly chunks and routes through Jupiter to avoid frontrunning a single big buy.
4. **Burn** — once $FNCPT is in the treasury account, the worker calls SPL Token `burn_checked` to permanently destroy it. The burn transaction signature is the canonical receipt.
5. **Publish** — the worker POSTs an epoch summary + cumulative totals + 12-month supply history to a Fincept HTTP endpoint that the terminal polls.

The terminal **only reads** — it has no on-chain code in Phase 5, no custodial mechanism, and no signing authority over the treasury vault.

---

## Demo mode

Until the buyback worker endpoint is configured, the dashboard ships built-in mock data so the UI is reviewable. The mock numbers are illustrative:

- **Epoch 47** revenue $42,310 → buyback $21,155 → 82.4 M $FNCPT bought + burned.
- **All-time** 1.42 B $FNCPT burned, 8.58 B remaining, $384,201 spent.
- **Treasury** $1.84M USDC + 234 SOL = ~$1.86M total, 18 months runway @ $100k/mo.

Each panel head shows **DEMO** in amber when any data on it is mocked, and the BURN TX link tooltip reads "Demo signature — connect a treasury endpoint for a real burn tx" so nobody mistakes the placeholders for real on-chain receipts.

---

## Configuring real data

When the worker is deployed, configure the following keys in **SecureStorage**:

| Key | Required | Notes |
|---|---|---|
| `fincept.treasury_endpoint` | yes | HTTP base URL where the worker publishes summaries. The terminal hits `<endpoint>/buyback/current`, `<endpoint>/burn/total`, `<endpoint>/supply/history`. |
| `fincept.treasury_pubkey` | yes | base58 pubkey of the Squads multisig vault. Used by `TreasuryService` to fetch SOL + USDC reserves directly from Solana RPC. |
| `fincept.treasury_monthly_opex_usd` | optional | default $100,000. Used for runway. Update as the team scales. |
| `fincept.treasury_multisig_label` | optional | default `3-of-5`. Free-form chip text. |
| `fincept.treasury_multisig_url` | optional | default `https://app.squads.so/squads/<pubkey>`. Override if you point at a different vault UI. |

Once `fincept.treasury_endpoint` is set, the head pill on **Buyback & Burn** flips from `DEMO` to `LIVE`. Same for the treasury card the moment `fincept.treasury_pubkey` is configured.

---

## Hub topics

Five terminal-wide topics power the dashboard. None carry a `<pubkey>` segment — the same numbers are shown to every user.

| Topic | Producer | TTL | Notes |
|---|---|---|---|
| `treasury:buyback_epoch`  | `BuybackBurnService` | 60 s  | Current epoch summary + burn signature |
| `treasury:burn_total`     | `BuybackBurnService` | 5 min | All-time burned + remaining supply |
| `treasury:supply_history` | `BuybackBurnService` | 1 h   | 12-month time-series (`QVector<SupplyHistoryPoint>`) |
| `treasury:reserves`       | `TreasuryService`    | 5 min | USDC + SOL + USD value, multisig label/url |
| `treasury:runway`         | `TreasuryService`    | 5 min | Derived: `total_usd / monthly_opex_usd` |

See `docs/DATAHUB_TOPICS.md` for full payload shapes.

---

## Where to find this in code

- `src/services/wallet/BuybackBurnService.{h,cpp}` — Producer, three topics, dual real/mock paths
- `src/services/wallet/TreasuryService.{h,cpp}` — Producer, two topics, peeks SOL price from `market:price:token:<wSOL>`
- `src/screens/crypto_center/panels/BuybackBurnPanel.{h,cpp}` — top section, Solscan-clickable burn signature
- `src/screens/crypto_center/panels/SupplyChartPanel.{h,cpp}` — middle section, QtCharts QChartView
- `src/screens/crypto_center/panels/TreasuryPanel.{h,cpp}` — bottom section, Squads-clickable multisig
- `src/screens/crypto_center/tabs/RoadmapTab.{h,cpp}` — wraps the three panels in a QScrollArea
- `plans/crypto-center-future-phases.md` §5 — original plan
- `docs/DATAHUB_TOPICS.md` "Buyback & burn dashboard" section — topic catalogue

---

**Last updated:** 2026-05-01
