# Crypto Center — Future Phases (2–5)

**Status**: design draft
**Owner**: tilakpatel22
**Last updated**: 2026-04-27
**Predecessor**: `plans/crypto-center-wallet-connect.md` (Phase 1, shipped); Phase 1.5 hardening shipped 2026-04-27 — see §1.5 below.
**Scope**: everything that lands inside `screens/crypto_center/CryptoCenterScreen` after the wallet-connect MVP — all subsequent capability that uses the same screen as its home.

---

## 0. North star

The Crypto Center is the **single home for everything $FNCPT** inside Fincept Terminal. By the end of Phase 5 it should answer four questions at a glance, in this order of priority:

1. **What do I hold?** — SOL + $FNCPT balance, USD value, recent activity. (Phase 1 done; Phase 2 deepens.)
2. **What can I do with it?** — buy/swap/burn, fee discounts, tier perks. (Phase 2.)
3. **What am I locked into?** — staked $FNCPT, lock duration remaining, projected yield. (Phase 3.)
4. **What can I bet on?** — internal prediction markets entry point. (Phase 4.)
5. **Why is this token worth something?** — buyback&burn dashboard with revenue → token flow. (Phase 5.)

Every later phase **adds rows / panels / tabs** to the existing screen — never a new top-level route. The screen grows; the navigation stays simple.

---

## 1. Layout strategy across phases

The screen evolves from a 2-panel grid to a **tabbed full-screen workspace** with a persistent identity bar across the top.

```
┌─ FINCEPT / CRYPTO CENTER ─────────── ● CONNECTED ── 7eV4z…aoowY ──┐
│                                                                   │
│  HOLDINGS BAR (always visible)                                    │
│  SOL 2.41 ─ $FNCPT 12,304 ─ USD $341.22 ─ 24h Δ +3.4%             │
│                                                                   │
├──────┬────────┬───────┬─────────┬──────────┬──────────┬───────────┤
│ HOME │ TRADE  │ STAKE │ MARKETS │ ACTIVITY │ ROADMAP  │ SETTINGS  │
├──────┴────────┴───────┴─────────┴──────────┴──────────┴───────────┤
│                                                                   │
│  ACTIVE TAB CONTENT (fills remaining space)                       │
│                                                                   │
└───────────────────────────────────────────────────────────────────┘
```

Tabs follow the same `b45309` orange pattern as the main app tab bar (`DESIGN_SYSTEM.md` §6). The HOLDINGS BAR is identity-level — visible on every tab. The Phase 1 layout becomes the **HOME** tab content.

### Phase mapping to tabs

| Tab | Lands in | Notes |
|---|---|---|
| HOME | Phase 1 (shipped) | Identity, balances, $FNCPT roadmap |
| TRADE | Phase 2 | Buy / swap / burn, fee discount |
| STAKE | Phase 3 | veFNCPT lock, real-yield dashboard |
| MARKETS | Phase 4 | Internal prediction markets |
| ACTIVITY | Phase 2 | On-chain transaction history (added with TRADE) |
| ROADMAP | Phase 5 (replaces Phase 1 panel) | Buyback & burn dashboard, treasury |
| SETTINGS | Phase 1+ | Mode toggle, RPC, API keys, hardware wallet preferences |

ACTIVITY ships with Phase 2 because the moment we let users sign transactions, they need to see them.

---

## 1.5 Phase 1.5 — Hardening (shipped 2026-04-27)

**Status**: shipped
**Scope**: Phase 1 stayed as-is in capability; this pass closed five correctness bugs and seven UX gaps that surfaced in production use of the HOME tab. Documented here so future-phase work inherits the patterns instead of re-discovering them.

### 1.5.1 Bugs closed

| ID | Bug | Fix |
|---|---|---|
| B1 | POLL/STREAM toggle could leave both buttons unchecked when the active button was clicked | Wrapped the two `QPushButton`s in an exclusive `QButtonGroup`. Click handlers react only to `toggled(true)` events; clicking the active button is a true no-op. |
| B2 | Stream mode never refreshed FNCPT — only SOL got `accountSubscribe` updates, FNCPT stayed at the seed value | Added a per-stream-session 30s `QTimer` heartbeat that re-runs `seed_stream_state` to refresh FNCPT. Until we surface the FNCPT token-account address and `accountSubscribe` to it directly (a Phase 2 concern), this heartbeat is the source of truth for FNCPT in Stream mode. |
| B3 | STREAM appeared to work on the public RPC but `accountSubscribe` was rate-limited to silence | `WalletBalanceProducer::endpoint_supports_streaming()` checks the resolved endpoint against `api.mainnet-beta.solana.com` and falls back to Poll for that session, publishing a `topic_error` so the screen can show the reason. |
| B4 | REFRESH button in Stream mode only republished cached values | New `WalletService::force_balance_refresh()` → `WalletBalanceProducer::force_refresh(pubkey)` which re-runs `seed_stream_state` in Stream and `refresh_one_poll` in Poll. The screen calls this in addition to a `hub.request(price_topic, force=true)`. |
| B5 | Balance panel header status was a hard-coded `"MAINNET"` label | Replaced with a `FeedStatus` enum (`Idle/Connecting/Live/Stale/Error`) driven by mode-changes, balance publishes, topic errors, and a 5s staleness tick. Object-name swap + `style()->polish` so the colour follows the global stylesheet. |

### 1.5.2 UX gaps closed

| ID | Gap | Fix |
|---|---|---|
| U1 | `apply_theme()` called `setStyleSheet(styleSheet() + …)` 9 times — every call triggered a full QSS reparse on every descendant (P7 violation) | Single composed stylesheet, assigned once. All widget styling keyed by `objectName`. |
| U2 | Topic errors were either swallowed (price) or hijacked the "Updated …" label (balance) | New `cryptoCenterErrorStrip` row inside the balance panel. Visible only while an error is live; auto-clears on the next valid publish. |
| U3 | Stat boxes showed bare em-dashes during the loading window with no microcopy | "waiting for first publish…" is shown in the meta row; REFRESH button is disabled until the first publish lands. |
| U4 | On short windows the static $FNCPT ROADMAP panel pushed balances off-screen | `connected_page_` wrapped in a `QScrollArea` (`widgetResizable=true`, no horizontal scroll). |
| U5 | RPC provider was invisible — users couldn't tell why STREAM was misbehaving | `cryptoCenterRpcChip` next to the feed status pill. Reads `PUBLIC` / `HELIUS` / `CUSTOM` from `SecureStorage` keys (`solana.rpc_url`, `solana.helius_api_key`). Tooltip on `PUBLIC` tells the user to add a Helius key for reliable streaming. |
| U6 | Dead `make_panel()` helper declared and defined but never called | Removed. |
| U7 | No staleness detection — a feed that silently stopped publishing kept showing "Updated 5s ago" forever | 5s `QTimer` re-evaluates `Updated …` text and flips `feed_status_` to `Stale` after 90s of silence. Started in `showEvent`, stopped in `hideEvent` (P3). |

### 1.5.3 New patterns introduced (carry into Phase 2+)

These are now the conventions for any new tab/panel added to Crypto Center:

1. **One stylesheet per screen.** Compose the full CSS string once in `apply_theme()` and assign it once. Widget styling lives in `objectName` selectors; never use inline `setStyleSheet` on individual labels for theming.
2. **`FeedStatus` enum drives the live-status pill.** Each tab in Phase 2+ should reuse the same five-state pattern (`Idle / Connecting / Live / Stale / Error`) and render via an object-name swap so the global stylesheet handles colour. Concrete enum lives in `CryptoCenterScreen.h` for now; promote to a shared `ui::widgets::FeedStatusPill` widget when Phase 2 lands a second consumer.
3. **Force-refresh goes through a service method, not directly through the hub.** `hub.request(topic, force=true)` is fine for read-only re-publish, but any path that actually re-fetches data (REST round-trip, WS re-seed) goes through the owning service so the producer can interpret "force" correctly per its mode. Phase 3's `StakingService` and Phase 4's market services should follow the same `force_*` pattern that `WalletService::force_balance_refresh` established.
4. **Per-session heartbeats are owned by the producer, not the screen.** `WalletBalanceProducer::StreamSession::fncpt_heartbeat` is owned by the producer; lifetime ties to the WS session. Screens never spawn timers for data refresh — that's a P3 + D3 violation.
5. **Endpoint-capability checks before opening streams.** Producers that have both Poll and Stream paths must expose an `endpoint_supports_streaming()` predicate and fall back gracefully with a `publish_error` for visibility. Hard-coded blocks: today the public Solana RPC; Phase 2 may add the public Jupiter rate-limited endpoint.
6. **Staleness is a timer-driven concept on the screen, not a producer responsibility.** Producers publish with a `ts_ms`; the screen decides when "old" becomes "STALE" based on its own threshold (90s for balances, will be tighter for swap quotes).
7. **Error strip pattern.** Every Phase 2+ panel that fetches data must have a hidden-by-default error row that surfaces `topic_error` messages and clears on next valid publish. Use the same `cryptoCenterErrorStrip` styling.
8. **RPC-indicator chip.** Any panel that does Solana RPC work surfaces the resolved provider as a `cryptoCenterRpcChip`-styled label so users understand the latency/reliability profile they're getting.

### 1.5.4 Carry-overs to Phase 2

When Phase 2 lifts the layout to the HOLDINGS BAR + tabbed workspace described in §1:

- The `FeedStatus` pill, RPC indicator, and error strip are **identity-level** — they belong on the HOLDINGS BAR, not duplicated per tab.
- The static `$FNCPT ROADMAP` panel still in the HOME tab is the placeholder slated to be replaced by the Phase 5 buyback & burn dashboard (§5.2). Don't invest further in it.
- The POLL/STREAM toggle moves to the SETTINGS tab (§6) but keeps the same `QButtonGroup`-exclusive widget pattern.
- `WalletService::force_balance_refresh()` becomes one of several `force_*` service methods. Standardise the naming.
- `staleness_timer_` (5s tick) is the prototype for the per-tab refresh timers Phase 2 panels will own. Each tab gets exactly one such timer started in `showEvent` and stopped in `hideEvent`.

### 1.5.5 Files touched in Phase 1.5

- `src/screens/crypto_center/CryptoCenterScreen.{h,cpp}` — full rewrite of `apply_theme`, new `FeedStatus` machinery, scroll wrap, RPC chip, error strip, exclusive button group, REFRESH wiring.
- `src/services/wallet/WalletService.{h,cpp}` — added `force_balance_refresh()`.
- `src/services/wallet/WalletBalanceProducer.{h,cpp}` — added `force_refresh()`, `endpoint_supports_streaming()`, FNCPT heartbeat lifecycle on `StreamSession`.

No new hub topics were introduced; no `DATAHUB_TOPICS.md` update required.

---

## 2. Phase 2 — TRADE tab

> **Detailed plan:** see `plans/crypto-center-phase-2.md` (rev 2). This section is the high-level user-story map; the rev-2 doc is the source of truth for build order, file deltas, and acceptance criteria.

**Goal**: every user can swap SOL ↔ $FNCPT inside the terminal in under 30 seconds, see their last 50 wallet operations, and qualify for a 30 % fee discount when holding ≥ 1,000 $FNCPT.

### 2.1 Capabilities

1. **Buy $FNCPT with SOL** — PumpPortal `trade-local` returns a fully-built unsigned tx; the user signs in their wallet.
2. **Sell $FNCPT for SOL** — symmetric; same endpoint, `action=sell`.
3. **Fee discount** — every paid feature in the terminal (deep backtest, AI report, premium screen) accepts $FNCPT at a 30 % discount. HoldingsBar lights up the chip; TRADE tab shows projected savings given current balance.
4. **Activity feed (ACTIVITY tab)** — last 50 on-chain operations involving the wallet, parsed for human-readable events (sent, received, swapped) via Helius parsed-tx.

**Burn deferred to Phase 5.** Real burns require either ~150 lines of client-side Solana primitives in C++ or a custom Rust on-chain program, both disproportionate for a single user-action. Phase 5's buyback worker burns from the Fincept treasury account using off-chain tooling — that's the right place for the supply mechanism. See `plans/crypto-center-phase-2.md` D6 for the full rationale.

### 2.2 New panels

```
┌─ SWAP ──────────── via PumpPortal · pool=auto ─┐  ┌─ FEE DISCOUNT ─ active ─┐
│ DIRECTION   [BUY $FNCPT (SOL → $FNCPT) ▾]      │  │ HOLD    1,000 $FNCPT   │
│ YOU PAY     [   0.50 ]   SOL    Bal: 1.20 SOL  │  │ ELIGIBLE  ✓ 30% off    │
│ YOU GET                  ~12,304  $FNCPT       │  │ APPLIED TO             │
│ ──────────────────────────────────────────────│  │   AI reports           │
│ ROUTE       PumpSwap (auto)                    │  │   Deep backtests       │
│ MAX SLIP    1.00%                              │  │   Premium screens      │
│ [SWAP]                                         │  │                        │
└────────────────────────────────────────────────┘  └────────────────────────┘
```

There is **no separate quote panel**. PumpPortal has no quote endpoint; estimated output is computed locally from the live `market:price:token:<mint>` family (Stage 2A.5 generalised the legacy `market:price:fncpt` topic into a per-mint family — the FNCPT topic remains as a deprecated alias for one phase) multiplied by the slippage budget.

### 2.3 ACTIVITY tab

```
┌─ ACTIVITY ─ last 50 operations ──────────────── refreshing every 30 s ─┐
│ TIMESTAMP            EVENT       ASSET     AMOUNT          STATUS      │
│ 2026-04-28 09:14:02  SWAP        SOL→FNCPT  0.5 → 12,304   confirmed   │
│ 2026-04-27 14:02:11  RECEIVE     SOL        0.10           confirmed   │
│ ...                                                                    │
└────────────────────────────────────────────────────────────────────────┘
```

Click a row → opens Solscan in default browser. Helius parsed-tx covers SWAP / RECEIVE / SEND directly; everything else is OTHER with the signature link.

### 2.4 New services

| Service | Path | Producer | Topics |
|---|---|---|---|
| `PumpFunSwapService` | `src/services/wallet/PumpFunSwapService.{h,cpp}` | **no** (one-shot, user-driven) | — `build_swap()` HTTP-POSTs to `pumpportal.fun/api/trade-local` and returns `tx_base64` directly to the caller |
| `WalletActivityProducer` | `src/services/wallet/WalletActivityProducer.{h,cpp}` | yes | `wallet:activity:<pubkey>` (paged JSON of parsed transfers) |
| `FeeDiscountService` | `src/services/billing/FeeDiscountService.{h,cpp}` | yes | `billing:fncpt_discount:<pubkey>` (eligibility, applied SKUs) |

The one-shot pattern for `PumpFunSwapService` mirrors `WalletService::sign_and_send` (Stage 2.0) — user-initiated actions that don't benefit from caching/coalescing don't need to be Producers. See the Phase 2 plan §3 for the full reasoning.

### 2.5 New hub topics (Phase 2)

| Pattern | Producer | TTL | Min | Notes |
|---|---|---|---|---|
| `wallet:activity:<pubkey>` | `WalletActivityProducer` | 30 s (poll) | 10 s | Parsed last-50 wallet operations via Helius. Falls back to raw signatures if no Helius key. |
| `billing:fncpt_discount:<pubkey>` | `FeeDiscountService` | derived from `wallet:balance:<pubkey>` | — | Republishes whenever balance changes. |
| `market:price:token:<mint>` | `TokenPriceProducer` | 60 s | 30 s | Generalised per-mint USD price, batched into one Jupiter Lite call per refresh. Supersedes the Phase 1 `market:price:fncpt` topic, which remains as a deprecated alias for one phase. See `plans/crypto-center-phase-2.md` §2A.5 for the full upgrade rationale. |

The earlier rev specced `wallet:quote:*` and `wallet:burn_receipt:*` — both retired. PumpPortal has no quote endpoint (output is estimated locally from spot price), and burn is deferred. The `wallet:balance:<pubkey>` topic is still owned by `WalletBalanceProducer`, but the payload shape was upgraded in Stage 2A.5 to carry `QVector<TokenHolding>` instead of FNCPT-only fields — see `docs/DATAHUB_TOPICS.md` for the new shape.

### 2.6 Solana program work

**None this phase.** The earlier rev specced a custom `fincept_burn` Anchor program for on-chain burn receipts. Burn is deferred to Phase 5; that's the correct place for any custodial burn primitive too. Phase 2 ships zero on-chain code.

### 2.7 Security

Phase 1 was read-only. Phase 2 introduces transaction signing, so the threat model expands:

- Every swap re-prompts the wallet UI. We **never** pre-sign or cache.
- The terminal shows the **decoded transaction summary** before sign — never trust the wallet to do this alone, since some users approve blindly.
- Slippage cap default 1 %; user adjustable up to 5 %. SettingsTab clamps the slider.
- PumpPortal MITM mitigation: simulate the unsigned tx against the user's RPC before forwarding, refuse if any non-FNCPT/non-SOL/non-wSOL token balance changes. Optional TLS leaf-cert pinning is a Phase 2.5 polish.
- Replay: refuse to sign if `currentBlockHeight > tx.lastValidBlockHeight - 50`.
- The shared `WalletActionConfirmDialog` enforces a 1.5 s arm-time on the SWAP button.

A separate `crypto-center-trade-security.md` review doc lands with the Phase 2 PR.

### 2.8 Acceptance criteria

- [ ] User can BUY 0.001 SOL of $FNCPT on mainnet and see balance update via DataHub within 5 s of `confirmed`.
- [ ] User can SELL 50 % of resulting $FNCPT and receive SOL back within 30 s.
- [ ] Holding 1,000+ $FNCPT shows "30 % off" badge on HoldingsBar and FeeDiscountPanel.
- [ ] ACTIVITY tab populates with last 50 parsed events.
- [ ] All five Phase 1 P-rules and four Phase 1 D-rules hold (no PythonRunner spawn, hub-first, etc.).
- [ ] No Rust source anywhere in the tree.

---

## 3. Phase 3 — STAKE tab

**Goal**: users can lock $FNCPT for 1–4 years and earn a real-yield share (USDC) of terminal revenue, plus governance weight. Curve `veCRV` model, but sized down for our scale.

### 3.1 Capabilities

1. **veFNCPT lock** — choose amount + duration (3 mo / 6 mo / 1 yr / 2 yr / 4 yr).
2. **Boost preview** — show projected weekly yield in USDC for the chosen lock.
3. **Existing positions** — table of locks, time remaining, current weight.
4. **Extend / increase** — add tokens or extend duration of an existing lock.
5. **Withdraw on expiry** — auto-prompt when a lock unlocks.
6. **Governance preview** — open proposals (read-only this phase; voting in Phase 4).
7. **Tier badge** — bronze / silver / gold based on `veFNCPT` weight, surfaced on HOME tab and across the terminal (gates premium features).

### 3.2 New panels

```
┌─ STAKE / LOCK ──────────────────────────────────────────────────────────────┐
│ AMOUNT       [   1,000 ] $FNCPT      AVAILABLE   12,304                      │
│ DURATION     ◯ 3 MO  ◯ 6 MO  ● 1 YR  ◯ 2 YR  ◯ 4 YR                          │
│ WEIGHT       1,000 veFNCPT × 0.25 (1 yr) = 250 veFNCPT                       │
│ EST. YIELD   $42 / week (USDC) — 2.18% weekly real yield at $341 stake       │
│ TIER         Silver → Gold (after lock)                                      │
│ [PREVIEW LOCK]            [LOCK]                                             │
└──────────────────────────────────────────────────────────────────────────────┘

┌─ ACTIVE LOCKS ─ 3 positions ─ Σ 4,500 veFNCPT ─────────────────────────────┐
│ LOCKED         DURATION   UNLOCKS         WEIGHT      YIELD (LIFETIME)      │
│ 2,000          4 yr       2030-04-27      2,000       $1,240 USDC           │
│ 1,000          1 yr       2027-04-01      250         $42 USDC              │
│ 500            6 mo       2026-10-27      62.5        $14 USDC              │
│ ──────────────────────────────────────────────────────────────────────      │
│ TOTAL          —          —               2,312.5     $1,296 USDC           │
└─────────────────────────────────────────────────────────────────────────────┘

┌─ TIER ──────────────────────────────────── current GOLD ────────────────────┐
│ BRONZE    100+ veFNCPT       basic API quota       [achieved]               │
│ SILVER    1k+ veFNCPT        premium screens       [achieved]               │
│ GOLD      10k+ veFNCPT       all agents + arena    [achieved]               │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.3 New services

| Service | Path | Topics |
|---|---|---|
| `StakingService` | `src/services/wallet/StakingService.{h,cpp}` | `wallet:locks:<pubkey>`, `wallet:vefncpt:<pubkey>` |
| `RealYieldService` | `src/services/wallet/RealYieldService.{h,cpp}` | `wallet:yield:<pubkey>`, `treasury:revenue` (terminal-wide stat) |
| `TierService` | `src/services/billing/TierService.{h,cpp}` | `billing:tier:<pubkey>` |

### 3.4 Hub topics (Phase 3)

| Pattern | Producer | TTL | Min | Notes |
|---|---|---|---|---|
| `wallet:locks:<pubkey>` | `StakingService` | 60 s (poll) / push | 30 s | Vector of `LockPosition{amount_raw, decimals, unlock_ts, weight}`. |
| `wallet:vefncpt:<pubkey>` | `StakingService` | 60 s | 30 s | Aggregated weight + projected next-period yield. |
| `wallet:yield:<pubkey>` | `RealYieldService` | 5 min | 60 s | Lifetime + last-period USDC yield earned. |
| `treasury:revenue` | `RealYieldService` | 1 h | 5 min | Terminal-wide aggregate (weekly bucket) — used in Phase 5 dashboard. |
| `billing:tier:<pubkey>` | `TierService` | 60 s | 30 s | `{tier, weight, next_threshold}`. Drives badges across the terminal. |

### 3.5 Solana program work

The lock program is the heart of Phase 3. Either:

- **(a) Reuse Curve-style ve-token contracts** — community ports exist for Solana (Realms, Squads), but each has different governance assumptions.
- **(b) Build minimal `fincept_lock` Anchor program**:
  - `lock(amount, duration_secs)` → mints non-transferable position NFT, escrows tokens.
  - `extend(position, new_duration)`.
  - `withdraw(position)` — only after unlock.
  - `emit_distribution(epoch, total_usdc)` — admin instruction (Fincept Corp signer) to settle the per-epoch real-yield split.

Decision: **(b)**. Curve-style contracts come with governance baggage (proposal queues, vote escrow boost calculations) we won't use until Phase 4. A small purpose-built program is auditable in a week.

Repo: `solana/programs/fincept_lock/`.

### 3.6 Tier integration with the rest of the terminal

The `TierService` publishes `billing:tier:<pubkey>` and emits a Qt signal `tier_changed`. Existing screens consume:

- **AI Quant Lab**: locked behind Silver+. The screen factory checks tier on `showEvent`; if insufficient, renders a "UPGRADE" pill that deep-links back to the STAKE tab.
- **Alpha Arena**: locked behind Gold.
- **All paid screens**: hidden in nav for Free; revealed for Silver+.
- **API quota**: handled centrally in `services/billing/ApiQuotaService` (already exists for plan-based gating); takes a tier override.

This is the **first cross-screen feature** $FNCPT enables. The HOME tab gets a "TIER STATUS" panel showing what's unlocked.

### 3.7 Acceptance criteria

- [ ] User can lock 1,000 $FNCPT for 1 year and see veFNCPT weight = 250.
- [ ] After a simulated revenue distribution event, the user receives USDC pro-rata to weight.
- [ ] Tier badge appears on HOME and gates AI Quant Lab.
- [ ] User cannot withdraw a non-expired lock (program rejects, UI explains).
- [ ] Locks survive terminal restart (state lives on-chain, not in SecureStorage).

---

## 4. Phase 4 — MARKETS tab (internal prediction markets)

**Goal**: $FNCPT becomes the only collateral on a small, curated prediction-market venue inside the terminal. Synergy with the existing `PredictionExchangeAdapter` (Polymarket, Kalshi).

### 4.1 Capabilities

1. **Market list** — curated prediction markets that settle in $FNCPT only: Fed rate moves, CPI prints, S&P close, weather (maybe), crypto milestones.
2. **YES / NO order entry** — limit + market orders. Off-chain order book, on-chain settlement.
3. **Open positions** — current YES/NO holdings, mark-to-market in $FNCPT.
4. **Settled history** — past markets, P&L, redeem-on-resolve flow.
5. **Create market** (Gold tier only) — propose a market with oracle source and end date; community votes via veFNCPT to list.
6. **Earn from order flow** — veFNCPT lockers receive a share of trading fees (real yield on top of Phase 3).

### 4.2 New panels

```
┌─ MARKETS ─ live ─ 12 open ─ Σ volume 12.3M $FNCPT ──────────────────────────┐
│ MARKET                                  YES   NO    24h VOL    EXPIRES      │
│ Fed cuts in May 2026                   0.62  0.38   1.2M       2026-05-07  │
│ NYC max 80F+ on May 1                  0.71  0.29   83k        2026-05-01  │
│ $FNCPT > $0.0001 by month end          0.45  0.55   421k       2026-05-31  │
│ ...                                                                         │
└─────────────────────────────────────────────────────────────────────────────┘

┌─ ORDER ENTRY ──────────────────────┐  ┌─ YOUR POSITIONS ──────────────────┐
│ MARKET   Fed cuts in May 2026      │  │ MARKET                YES  P&L     │
│ SIDE     ● YES   ◯ NO              │  │ NYC max 80F+         500   +124   │
│ TYPE     ● LIMIT ◯ MARKET          │  │ Fed cuts             1,000 -42    │
│ PRICE    [ 0.62 ] $FNCPT/share     │  │ ...                                │
│ AMOUNT   [ 1000 ] shares           │  └────────────────────────────────────┘
│ COST     620 $FNCPT (escrowed)     │
│ [PLACE]                            │
└────────────────────────────────────┘
```

### 4.3 Architecture

The existing `PredictionExchangeAdapter` already abstracts Polymarket and Kalshi. Add a third adapter:

```
PredictionExchangeRegistry
  ├─ PolymarketAdapter       (existing)
  ├─ KalshiAdapter           (existing)
  └─ FinceptInternalAdapter  (NEW — Phase 4)
```

`FinceptInternalAdapter` speaks to a **terminal-side Fincept matching engine** (HTTP + WebSocket on `markets.fincept.in`), but settlement is on-chain via a `fincept_market` Anchor program.

### 4.4 New services

| Service | Path | Topics |
|---|---|---|
| `FinceptInternalAdapter` | `src/services/prediction/internal/FinceptInternalAdapter.{h,cpp}` | `prediction:fincept:*` (markets, books, trades) |
| `MarketCreationService` | `src/services/prediction/internal/MarketCreationService.{h,cpp}` | `prediction:fincept:proposals` |
| `OracleResolverService` | `src/services/prediction/internal/OracleResolverService.{h,cpp}` | `prediction:fincept:resolutions:<id>` |

Hub topics follow the unified `prediction:*` pattern already documented in `DATAHUB_TOPICS.md` — drop in `prediction:fincept:*` as a sibling family.

### 4.5 Solana program work

`fincept_market` program:
- `create_market(end_ts, resolver_pubkey, fee_bps)`
- `deposit_collateral(market, side, amount)` (called per matched fill)
- `resolve(market, outcome)` — only callable by `resolver_pubkey` (the oracle, multi-sig'd to a 3-of-5 set chosen by veFNCPT vote).
- `redeem(market, position)` — winner withdraws; fees split to staking pool.

Repo: `solana/programs/fincept_market/`.

### 4.6 Oracle layer

Pre-built sources:
- **Pyth Network** — Fed rates, S&P close, BTC price.
- **Switchboard** — custom OOB queries (HTTP fetch from NOAA, IMF).
- **Manual review board** — multi-sig of 3 trusted Fincept staff for ambiguous markets, slashable via veFNCPT vote if they misresolve.

Oracle source is part of the market metadata; UI shows the source pill.

### 4.7 Compliance / scope

Real-money prediction markets are regulated as derivatives in US/EU. We mitigate by:
- $FNCPT is a token, not USD — marketed as a play-money internal mechanic, not a regulated derivative.
- Geo-block US/UK during sign-up if legal counsel insists.
- All markets have a legal disclaimer line in the terminal.

This is a **policy decision** outside the scope of this doc — the engineering plan ships the same either way.

### 4.8 Acceptance criteria

- [ ] User can place a YES order on a market and see it in the order book within 1 s (WS feed).
- [ ] On market resolution, redeem flow auto-prompts in the MARKETS tab.
- [ ] Trading fees flow into the Phase 3 staking pool — `wallet:yield:<pubkey>` reflects new yield within one epoch.
- [ ] Gold-tier user can propose a market; community vote (veFNCPT) lists it.
- [ ] Market data uses `prediction:fincept:*` topics, never bypasses the hub.

---

## 5. Phase 5 — Buyback & Burn dashboard (ROADMAP tab)

**Goal**: make the value loop visible. Every user can see exactly how much terminal revenue has been spent buying $FNCPT off the market and burning it. This is the **demand engine** that justifies the token long-term.

### 5.1 Capabilities

1. **Live buyback ticker** — current epoch's $FNCPT bought from Jupiter using terminal revenue.
2. **Cumulative burn counter** — total $FNCPT burned all-time.
3. **Supply chart** — circulating + total + burned over time.
4. **Revenue breakdown** — terminal subscriptions, fee discount, prediction market fees, what fraction goes to buyback.
5. **Treasury panel** — current treasury size, USDC reserves, runway.

### 5.2 ROADMAP tab replaces Phase 1's static panel

```
┌─ BUYBACK & BURN ─ epoch 47 ─ 2026-04-21 → 2026-04-27 ───────────────────────┐
│                                                                              │
│   THIS EPOCH                                                                 │
│   ─────────────                                                              │
│   Revenue            $42,310 USD (subs $32k · pred-mkt fees $8k · misc $2k)  │
│   Buyback            $21,155 (50% of revenue)                                │
│   $FNCPT bought      82.4M $FNCPT (avg 0.000257 USD)                         │
│   $FNCPT burned      82.4M $FNCPT  ●  burn tx 5kj3w…                         │
│                                                                              │
│   ALL-TIME                                                                   │
│   ─────────────                                                              │
│   Burned             1.42 B $FNCPT                                           │
│   Supply remaining   8.58 B $FNCPT                                           │
│   Spent on buyback   $384,201 USD                                            │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘

┌─ SUPPLY CHART ──────────────────────────────────────────────────────────────┐
│ [ time-series chart: total / circulating / burned, last 12 months ]         │
└─────────────────────────────────────────────────────────────────────────────┘

┌─ TREASURY ──────────────────────────────────────────────────────────────────┐
│ USDC reserves        $1.84M                                                  │
│ SOL reserves         234 SOL ($16k)                                          │
│ Runway @ current     19 months                                               │
│ Multi-sig            3-of-5 (squads.so/fincept-treasury)                     │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 5.3 New services

| Service | Path | Topics |
|---|---|---|
| `BuybackBurnService` | `src/services/wallet/BuybackBurnService.{h,cpp}` | `treasury:buyback_epoch`, `treasury:burn_total`, `treasury:supply_history` |
| `TreasuryService` | `src/services/wallet/TreasuryService.{h,cpp}` | `treasury:reserves`, `treasury:runway` |

These are **terminal-wide** topics (no `<pubkey>` segment) — same data shown to every user.

### 5.4 Automation (off-chain)

The buyback&burn flow runs on a Fincept-operated worker:

1. End of epoch, worker tallies revenue from Stripe + on-chain prediction-market fees.
2. Computes buyback budget (initial split: 50 % buyback, 25 % staker yield, 25 % treasury).
3. Splits into N hourly chunks, executes through Jupiter to avoid frontrunning a single big buy.
4. Burns the bought $FNCPT — direct SPL Token `burn_checked` from a treasury account is sufficient. The signature serves as the receipt; an on-chain event log is *not* required because the worker also publishes the per-epoch summary the dashboard reads (step 5).
5. Publishes per-epoch summary to a Fincept HTTP endpoint that `BuybackBurnService` polls.

Worker code lives in `services/buyback-worker/` (Python or TypeScript — separate repo decision).

### 5.5 Cross-phase reuse

This phase introduces no on-chain program. Direct SPL `burn_checked` from a treasury wallet is enough for a Fincept-operated worker — the receipt is the transaction signature plus the off-chain epoch summary. (User-initiated burns from the TRADE tab were considered for Phase 2 but deferred precisely so this phase, where centralised burn coordination actually pays for itself, owns the burn primitive cleanly.)

### 5.6 Acceptance criteria

- [ ] ROADMAP tab shows live epoch buyback amount, refreshing every 60 s.
- [ ] Supply chart renders last 12 months with three lines.
- [ ] Each burn transaction is clickable → opens Solscan.
- [ ] Treasury runway calculation matches finance team's reconciliation.

---

## 6. SETTINGS tab (lightweight, ships incrementally)

A small SETTINGS tab lands with Phase 2 and grows:

- Phase 2: balance mode (poll/stream), Helius API key entry, default slippage.
- Phase 3: tier display preferences, governance email opt-in.
- Phase 4: market resolution preferences (auto-redeem on win), oracle source filter.
- Phase 5: data refresh cadence per panel.

Keep it boring — `core/keys/KeyConfigManager` plus `SecureStorage` is enough.

---

## 7. Cross-phase concerns

### 7.1 Performance — every panel is a hub subscriber

- Each tab subscribes only when **visible** (already wired via `showEvent` / `hideEvent`).
- Heavy-paint tabs (charts in Phase 5) **must** cache pixmaps per `DESIGN_SYSTEM` + P9.
- The HOLDINGS BAR persists on every tab — single subscription set, owned by the screen, not duplicated per tab.

### 7.2 Storage growth

By Phase 5 the Crypto Center subscribes to roughly 18 hub topics per session. The hub handles topic-state cleanup via `drop_on_idle = true` for per-pubkey topics; terminal-wide topics (`treasury:*`) stay resident.

### 7.3 Wallet permission UX

Every action that prompts the wallet must show **what** is being signed in the terminal **before** the wallet pops:
- Swap: "You will sign a swap of 0.5 SOL for ~12,304 $FNCPT through Jupiter."
- Burn: "You will sign a burn of 100 $FNCPT. This is irreversible."
- Lock: "You will sign a lock of 1,000 $FNCPT for 1 year. You cannot withdraw before <date>."

Pattern: `WalletActionConfirmDialog` (Phase 2) — shared across all phases.

### 7.4 Recovering from drift

If the user disconnects + reconnects with a different wallet, the screen must:
1. Drop subscriptions for the old pubkey.
2. Reseed from on-chain state for the new pubkey.
3. Never show stale balances/locks/positions from the previous session.

This is centralised in `WalletService::wallet_disconnected` → `wallet_connected` signal pair, which every Phase 2+ panel listens to.

### 7.5 Test strategy

Each phase ships with:
- Unit tests for new services (mock RPC and Jupiter responses).
- Integration test against **devnet** (deploy our programs there too).
- Manual smoke test on mainnet with a tiny amount of $FNCPT.
- Security walkthrough doc (like Phase 1's), specifically focused on the new sign-flows.

---

## 8. Order of operations

A reasonable sequencing (each phase ~3–6 weeks of focused work):

| Order | Phase | Why |
|---|---|---|
| 1 | Phase 2 (TRADE + ACTIVITY) | Unlocks revenue (fee discount), ships fastest, lets users actually use $FNCPT. |
| 2 | Phase 5 (Buyback & Burn dashboard) | Marketing surface lights up the moment Phase 2 ships — show the value loop. |
| 3 | Phase 3 (STAKE) | Locks supply once enough users hold the token. |
| 4 | Phase 4 (MARKETS) | Highest engineering risk; benefits most from a captive holder base. |

Phase 5 ships **before** Phase 3 because the dashboard works as soon as buyback automation runs — even if no one is staking yet. It builds confidence. Phase 3 is the hardest UX (yield projection, lock math, governance previews) and benefits from a stable Phase 2.

---

## 9. What we will NOT do across these phases

- **Custodial features.** No "Fincept holds your tokens for you." Ever.
- **Multi-chain.** $FNCPT stays on Solana. Cross-chain bridges are a separate product.
- **Fiat onramp inside the terminal.** Use Phantom/Solflare's built-in onramps; we link, don't host.
- **Auto-trading bots.** `agent:*` topics already exist for that — we don't double-build.
- **NFT marketplace.** Out of scope; partner if needed.
- **Hardware wallet direct USB.** Phantom/Solflare host Ledger fine; direct integration is months of work for marginal UX gain.

These exclusions matter because each one has been pitched in past discussions and each one can swallow a quarter of engineering time without moving the needle.

---

## 10. Success metric

By end of Phase 5, with reasonable adoption (1k DAU on the terminal):

- $FNCPT velocity: > $50k weekly volume on Jupiter pairs.
- Locked supply: > 30 % via veFNCPT.
- Burn rate: ≥ 1 % of supply burned per quarter.
- Real-yield APR: ≥ 6 % USDC for 1-year lockers.
- Prediction market open interest: $250k+ in $FNCPT.

If any of these is < 30 % of target after the corresponding phase, **stop** and fix the loop before adding more features. More mechanics on a token nobody uses is the most common DeFi failure mode, and we are not exempt.

---

## 11. Document maintenance

- Update this doc at each phase kickoff with refined estimates and any scope deltas.
- When a phase ships, **archive** its section into `plans/crypto-center-phase<N>-shipped.md` with the actual implementation diffs from the plan, so future phases inherit verified patterns rather than design speculation.
- Topic catalogue (`docs/DATAHUB_TOPICS.md`) is the source of truth for all hub topics; this doc is the user-story-level map. Discrepancies → fix `DATAHUB_TOPICS.md` first.
