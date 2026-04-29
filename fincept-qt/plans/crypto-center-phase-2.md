# Crypto Center — Phase 2 (TRADE + ACTIVITY + Tabbed Layout)

**Status**: rev 2 — pivoted from Jupiter+Anchor to PumpPortal-only after probing the live APIs and confirming pump.fun/PumpSwap routing covers $FNCPT cleanly without any client-side Solana primitives or custom on-chain program.
**Owner**: tilakpatel22
**Last updated**: 2026-04-28
**Predecessors**: `crypto-center-future-phases.md` §2 (high-level), `crypto-center-wallet-connect.md` (Phase 1, shipped), Phase 1.5 hardening (shipped)
**Companion**: `crypto-center-trade-security.md` — security walkthrough, lands with the Phase 2 PR.

---

## 0. North star

Phase 2 turns Crypto Center from a **read-only identity surface** into a **transactional surface**. Every new capability shares one constraint:

> **The user signs every transaction in their wallet, sees a decoded summary in the terminal *before* the wallet pops, and the terminal never holds a private key longer than it takes to forward a base64 transaction over a loopback HTTP bridge.**

Phase 2 ships three user-visible capabilities:

1. **TRADE tab — Buy / Sell.** Buy or sell $FNCPT (vs SOL) via PumpPortal's `trade-local` endpoint. PumpPortal's `pool: "auto"` routing automatically picks the active venue (pump.fun bonding curve before graduation, PumpSwap AMM after — $FNCPT is graduated). Confirm dialog → wallet signs → terminal polls confirmation → balance refreshes.
2. **ACTIVITY tab.** Last 50 parsed wallet operations (sends, receives, swaps), refreshing every 30 s. Helius parsed-tx when a key is configured, signature-only fallback otherwise.
3. **Fee discount eligibility.** Users holding ≥ 1,000 $FNCPT see a "30 % off" chip on HoldingsBar; future paid screens (Phase 3+) read `billing:fncpt_discount:<pubkey>` and render an UPGRADE-to-discount badge.

**Deferred to a later phase (deliberately):**
- **Burn.** PumpPortal has no burn endpoint. A real `burn_checked` requires either client-side Solana primitives in C++ (~150 lines: base58, ATA derivation, instruction encoding) or a custom Anchor program (Rust). Both add complexity disproportionate to the use-case at this stage. Folded into Phase 5's buyback-worker design instead, where centralised burns from terminal revenue do the heavy lifting on the supply side.

**Already shipped before this rev (do not re-do):**
- HoldingsBar (Stage 2.1), HomeTab, SettingsTab, ComingSoonTab, CryptoCenterScreen tabbed shell, TradeTab + SwapPanel scaffolding, WalletActionConfirmDialog, WalletTxBridge, SignTransactionDialog, SolanaRpcClient extensions (`getLatestBlockhash`, `sendTransaction`, `getSignatureStatuses`, `getSignaturesForAddress`), `swap.html` + vendored `web3.js` slot, hub topics in `DATAHUB_TOPICS.md`.

The pivot reuses all of that. The only thing that **changes** is the swap-tx build path — `JupiterSwapService` is replaced by `PumpFunSwapService` which calls PumpPortal. The swap-html page, the wallet-bridge flow, the confirm dialog, the status-poll machinery — all unchanged.

---

## 1. Decisions locked

| # | Decision | Choice | Rationale |
|---|---|---|---|
| D1 | Swap-tx builder | **PumpPortal `trade-local` API** (`POST https://pumpportal.fun/api/trade-local`). Returns a fully-built unsigned versioned transaction in raw binary (verified live: 1009-byte sell tx, 1098-byte buy tx for $FNCPT). | Jupiter's `/swap` works for $FNCPT but routes through PumpSwap anyway. Going direct removes a layer; PumpPortal is the canonical bridge for pump.fun-launched tokens. No API key required. |
| D2 | Pool routing | **`pool: "auto"`** on every request. | $FNCPT is post-graduation (bonding curve complete), so PumpPortal will pick PumpSwap. If Fincept ever launches a new token pre-graduation, the same code routes it correctly. |
| D3 | Quote / preview | **No separate quote topic.** Estimate output from current spot (`market:price:fncpt`) + slippage budget; render in SwapPanel. | PumpPortal has no quote endpoint, only "build me a tx now". The earlier Jupiter design carried `wallet:quote:*` precisely because Jupiter exposed `/quote` separately. Removing it simplifies the panel and lines up with the tx-on-click flow. |
| D4 | Slippage default / cap | Default **1 %**, user-adjustable up to **5 %** in Settings. The slippage bound is sent to PumpPortal as the `slippage` integer (percent). | Matches earlier rev; Phase 1.5 SettingsTab already persists `wallet.default_slippage_bps`. |
| D5 | Priority fee | Default **0.00005 SOL** (~$0.01 at SOL=$170). Hidden behind an "advanced" disclosure in Settings; not exposed per-trade in Phase 2. | Matches PumpPortal's docs example; covers 95th-percentile network congestion. Per-trade override is a Phase 3 polish. |
| D6 | Burn | **Deferred to Phase 5**, when buyback automation owns burns centrally. Phase 2's TRADE tab shows BUY and SELL only; the BurnPanel placeholder is removed (was Stage 2.3). | Avoids ~150 lines of Solana primitives in C++ or a separate Rust program for a single user-action. Phase 5's buyback-worker burns from a Fincept treasury account using off-chain tooling. |
| D7 | Activity feed source | **Helius parsed-tx REST** (`https://api.helius.xyz/v0/addresses/<pubkey>/transactions`) when `solana.helius_api_key` is set in SecureStorage. Fallback: `SolanaRpcClient::get_signatures_for_address` for raw signatures. | Same as the earlier rev — unchanged. |
| D8 | Fee-discount source | Local computation against the latest `wallet:balance:<pubkey>` publish. Threshold + applied SKUs in `services/billing/FeeDiscountConfig.h`. | Same as earlier rev — unchanged. |
| D9 | Fee-discount threshold | **1,000 $FNCPT** for **30 %** off AI reports / deep backtests / premium screens. | Same as earlier rev. Phase 3's TierService will override based on staked weight. |
| D10 | Tabbed shell | Already shipped Stage 2.1: `QTabWidget` with `HoldingsBar` above. Phase 2 only updates content of TRADE and ACTIVITY tabs. | No layout work in this rev. |
| D11 | When wallet is disconnected | All Phase 2 tabs render their empty state. The CONNECT WALLET CTA from Phase 1 remains the only entry point. | Same as earlier rev. |
| D12 | Confirm dialog scope | The shared `WalletActionConfirmDialog` (Stage 2.0) handles BUY and SELL. Reused as-is for Phase 3+ lock / stake. | Same as earlier rev. |
| D13 | Anti-muscle-memory delay | **1500 ms** for both BUY and SELL. (Earlier rev had 2500 ms for burn; with burn deferred, 1500 ms applies uniformly.) | Conservative default; perceptible enough to break click-through, not so long users hate it. |
| D14 | Where the unsigned tx is built | PumpPortal returns the raw bytes; the terminal **base64-encodes** them and forwards to the existing `WalletTxBridge → swap.html → wallet.signAndSendTransaction`. | Reuses the entire signing path from Stage 2.2; zero changes to `swap.html` or `WalletTxBridge`. |
| D15 | Activity row click target | Solscan in default browser. Cluster is mainnet-only (no devnet $FNCPT supply). | Same as earlier rev. |

---

## 2. Threat model — only the entries that still apply

The earlier rev listed five new attacks beyond Phase 1. With burn deferred and Jupiter dropped, three remain unchanged and two retire.

| Attack | Still applies? | Mitigation |
|---|---|---|
| **PumpPortal MITM** — attacker intercepts the HTTPS call and feeds a malicious tx body that drains the wallet | **Yes** (replaces "quote-API spoofing" entry from the Jupiter rev) | (a) Confirm dialog shows decoded summary before sign — the tx itself is opaque to the user, but they verify _amount_, _direction_, and _slippage_ against what they typed; (b) `simulateTransaction` on the user's RPC and refuse to sign if any non-FNCPT/non-SOL/non-wSOL token balance changes. (c) Optionally pin PumpPortal's TLS leaf cert SHA-256 in SecureStorage and verify on each call — Phase 2.5 polish. |
| **Replay of an old swap transaction** | **Yes** | The unsigned tx from PumpPortal has a recent blockhash baked in. Refuse to sign if `currentBlockHeight > tx.lastValidBlockHeight - 50` (about 25 s buffer). Read the embedded blockhash via `web3.js` after deserialise — already part of `swap.html`'s deserialise step. |
| **Anti-muscle-memory** | **Yes** | `WalletActionConfirmDialog` already enforces a 1.5 s arm delay with a visible countdown. Inherited unchanged from Stage 2.0. |
| **Loopback-bridge stale token** | **Yes** | `WalletTxBridge` already mints a single-use UUID per request, expires after 60 s. Inherited unchanged from Stage 2.0. |
| ~~Burn-program ID swap~~ | **No** — burn deferred to Phase 5 | n/a |
| ~~Anchor program tampering via SecureStorage~~ | **No** — no custom program | n/a |

A separate `crypto-center-trade-security.md` lands with the Phase 2 PR; this section is the spec it walks against.

---

## 3. Architecture (delta from what's already shipped)

Phase 2 rev 2 replaces exactly one component (`JupiterSwapService`) and adds two new ones (`WalletActivityProducer`, `FeeDiscountService`). Everything else is reuse.

```
                   ┌──────────────────────────────────────────────────────────────┐
                   │  CryptoCenterScreen (already shipped 2.1)                     │
                   │  ┌──────────────────────────────────────────────────────────┐ │
                   │  │  HoldingsBar (always visible, identity-level)            │ │
                   │  │   SOL · $FNCPT · USD · 24h Δ · ● LIVE · HELIUS · 30% OFF │ │
                   │  └──────────────────────────────────────────────────────────┘ │
                   │  ┌──────────────────────────────────────────────────────────┐ │
                   │  │  QTabWidget                                              │ │
                   │  │  ┌────┬─────┬───────┬─────┬───────┬───────┬─────────┐    │ │
                   │  │  │HOME│TRADE│ACTIV. │SET. │STAKE† │MKTS†  │ROADMAP† │    │ │
                   │  │  └────┴─────┴───────┴─────┴───────┴───────┴─────────┘    │ │
                   │  │  TRADE: SwapPanel only (BurnPanel removed; deferred)     │ │
                   │  │  ACTIV.: ActivityTab — Helius parsed-tx                  │ │
                   │  └──────────────────────────────────────────────────────────┘ │
                   └──────────────────────────────────────────────────────────────┘
                              │ subscribe / unsubscribe
                              ▼
        ┌─────────────────────────────────────────────────────────────────────────┐
        │  DataHub                                                                │
        │   wallet:balance:<pk>            (Phase 1, unchanged)                   │
        │   market:price:fncpt             (Phase 1, unchanged)                   │
        │   wallet:activity:<pk>           (NEW, poll 30 s)                       │
        │   billing:fncpt_discount:<pk>    (NEW, derived from balance)            │
        │                                                                          │
        │   (wallet:quote:* — REMOVED. PumpPortal has no quote endpoint;          │
        │    SwapPanel computes the projected output locally from spot.)          │
        │   (wallet:burn_receipt:* — REMOVED. Burn deferred to Phase 5.)          │
        └─────────────────────────────────────────────────────────────────────────┘
                              ▲ publishes
                              │
        ┌──────────────────────┴──────────────────────────────────────────────────┐
        │                                                                          │
        │  PumpFunSwapService          WalletActivityProducer                     │
        │   - build_swap(action, amt,  - 30 s poll Helius                         │
        │     pubkey, slippage_pct)      parsed-tx endpoint                       │
        │     → tx_base64              - parses tx kinds                          │
        │     (single HTTP POST to       (SWAP, RECEIVE, SEND)                    │
        │      pumpportal.fun)                                                     │
        │                                                                          │
        │  FeeDiscountService                                                     │
        │   - subscribes to wallet:balance:<pk>                                   │
        │   - publishes billing:fncpt_discount:<pk>                               │
        │                                                                          │
        └──────────────────────┬──────────────────────────────────────────────────┘
                              │ uses (already shipped)
                              ▼
        ┌─────────────────────────────────────────────────────────────────────────┐
        │  WalletService::sign_and_send  (Stage 2.0)                              │
        │  WalletTxBridge                (Stage 2.0)                              │
        │  resources/wallet/swap.html    (Stage 2.2)                              │
        │  SolanaRpcClient::getSignatureStatuses  (Stage 2.0)                     │
        │  WalletActionConfirmDialog     (Stage 2.0)                              │
        │  HttpClient                    (Phase 1)                                │
        └─────────────────────────────────────────────────────────────────────────┘
```

The signing path (`WalletService::sign_and_send` → `WalletTxBridge` → `swap.html` → wallet ext → status poll) is reused without modification. The replacement scope is **the C++ service** that produces the `tx_base64` to feed into `sign_and_send`.

---

## 4. Files to add / change / remove

### New files

```
src/services/wallet/
  PumpFunSwapService.{h,cpp}                     // POST pumpportal.fun/api/trade-local
                                                 // → returns base64 versioned tx
  WalletActivityProducer.{h,cpp}                 // Producer for wallet:activity:*
src/services/billing/
  FeeDiscountService.{h,cpp}                     // Producer for billing:fncpt_discount:*
  FeeDiscountConfig.h                            // thresholds (decoupled from service)
src/screens/crypto_center/panels/
  FeeDiscountPanel.{h,cpp}                       // owned by TradeTab — replaces placeholder
src/screens/crypto_center/tabs/
  ActivityTab.{h,cpp}                            // owned by CryptoCenterScreen — replaces ComingSoonTab in slot 3
docs/
  CRYPTO_CENTER_PHASE_2.md                       // user-facing "how to swap"
plans/
  crypto-center-trade-security.md                // walkthrough doc
```

### Existing files modified

```
src/screens/crypto_center/panels/SwapPanel.{h,cpp}
  - Replace JupiterSwapService dependency with PumpFunSwapService
  - Drop the wallet:quote:* subscription path
  - Compute estimated-out locally: `in_amount * spot_price * (1 - slippage_pct/100)`
  - Submit flow: PumpFunSwapService::build_swap() → sign_and_send() (unchanged below this)
  - Burn-related dead code removed (none made it past Stage 2.2, but verify)

src/screens/crypto_center/tabs/TradeTab.{h,cpp}
  - Burn placeholder QFrame removed
  - FeeDiscountPanel slotted in
  - Layout becomes: SwapPanel (top) + FeeDiscountPanel (right)

src/screens/crypto_center/CryptoCenterScreen.{h,cpp}
  - Replace ComingSoonTab in ACTIVITY slot with real ActivityTab

src/services/wallet/WalletService.{h,cpp}
  - swap_service() accessor changes type from JupiterSwapService* to PumpFunSwapService*
  - Drop wallet:quote:* topic policy (no longer needed)

src/datahub/DataHubMetaTypes.{h,cpp}
  - Drop JupiterQuote registration (no longer needed)
  - Add ParsedActivity registration

src/services/wallet/WalletTypes.h
  - Drop JupiterQuote
  - Add ParsedActivity, FncptDiscount

src/app/main.cpp
  - Construct + register WalletActivityProducer, FeeDiscountService with the hub

docs/DATAHUB_TOPICS.md
  - Remove the wallet:quote:* row (added in Stage 2.0; never used)
  - Remove the wallet:burn_receipt:* row (Burn deferred)
  - Keep wallet:activity:* and billing:fncpt_discount:*

CMakeLists.txt
  - Add PumpFunSwapService.cpp, WalletActivityProducer.cpp, FeeDiscountService.cpp,
    FeeDiscountPanel.cpp, ActivityTab.cpp
  - Remove JupiterSwapService.cpp from the source list
```

### Files removed

```
src/services/wallet/JupiterSwapService.{h,cpp}
  Replaced by PumpFunSwapService. Delete completely — no users beyond SwapPanel,
  which moves to the new service in the same PR.
```

### Files unchanged (keep as-is, do not touch)

```
src/services/wallet/WalletTxBridge.{h,cpp}
src/services/wallet/SignTransactionDialog.{h,cpp}
src/services/wallet/SolanaRpcClient.{h,cpp}                  (extensions from Stage 2.0)
src/screens/crypto_center/WalletActionConfirmDialog.{h,cpp}
src/screens/crypto_center/WalletActionSummary.h
src/screens/crypto_center/HoldingsBar.{h,cpp}
src/screens/crypto_center/tabs/HomeTab.{h,cpp}
src/screens/crypto_center/tabs/SettingsTab.{h,cpp}
src/screens/crypto_center/tabs/ComingSoonTab.{h,cpp}        (still used for STAKE/MKTS/ROADMAP)
resources/wallet/swap.html
resources/wallet/vendor/                                     (web3.js slot — same need)
```

---

## 5. Stages & tasks

### Stage 2A — Swap-path pivot

**Goal:** swap (BUY + SELL) works end-to-end against PumpPortal. No new features beyond what Stage 2.2 already promised; the same UI, just a different tx-build backend.

**Task 2A.1 — `PumpFunSwapService`**
- Header: `build_swap(action: Buy|Sell, mint, amount_raw, denominated_in_sol, pubkey, slippage_pct, priority_fee_sol, cb)` → `Result<{ tx_base64, last_valid_block_height_hint }>`. The hint is _optional_ — PumpPortal doesn't expose it directly, so we either parse it from the deserialised tx (cheap, post-receive) or skip the freshness gate and rely on the wallet's own blockhash check.
- One HTTP POST to `https://pumpportal.fun/api/trade-local` with the JSON body:
  ```json
  {
    "publicKey": "<pk>",
    "action": "buy" | "sell",
    "mint": "9LUqJ5aQTjQiUCL93gi33LZcscUoSBJNhVCYpPzEpump",
    "amount": <number-or-percent-string>,
    "denominatedInSol": "true" | "false",
    "slippage": <int-percent>,
    "priorityFee": <sol>,
    "pool": "auto"
  }
  ```
- Response is **raw binary** (octet-stream), about 1 KB. Base64-encode and return.
- Reuses the `HttpClient` singleton; mirror `JupiterSwapService.cpp`'s post pattern.
- **Not a Producer** — there's no topic family. It's a one-shot service like `FncptBurnService` was going to be.
- Verification: unit-test the success path with a mocked HTTP reply; live integration test handled in Task 2A.4.

**Task 2A.2 — Replace `JupiterSwapService` plumbing**
- Delete `JupiterSwapService.{h,cpp}` and any `Q_DECLARE_METATYPE(JupiterQuote)` plus its registration in `DataHubMetaTypes`.
- Drop `WalletService::swap_service()` returning `JupiterSwapService*`; replace with `pumpfun_swap()` returning `PumpFunSwapService*`. Both `WalletService.h` and `WalletService.cpp` change.
- Remove the `wallet:quote:*` topic policy from `WalletService::ensure_registered_with_hub`.
- Remove the `wallet:quote:*` row from `docs/DATAHUB_TOPICS.md`. Same commit (D5 in CLAUDE.md).
- Verification: build is green; the screen loads as before; no dangling references in grep.

**Task 2A.3 — Update `SwapPanel`**
- Remove the `wallet:quote:*` subscription. Keep the debounce timer for input changes — but instead of resubscribing, recompute estimated-out locally:
  ```
  estimated_out = in_amount * spot_price * (1 - slippage_pct/100)
  ```
  using the cached `last_fncpt_usd_price_` (BUY) or `1 / last_fncpt_usd_price_` (SELL).
- Show the same `route` / `impact` rows but populate them from a static "PumpSwap (auto)" / "≤ slippage" — there's no live route impact from PumpPortal.
- On SWAP click: build summary as before but with locally-computed numbers, then call `PumpFunSwapService::build_swap` → `WalletService::sign_and_send` → status-poll. The lower half of the flow is unchanged.
- The `slippage_bps()` accessor stays — but we now pass `int slippage_pct = bps / 100` to PumpPortal. PumpPortal expects whole percent.
- Verification: BUY 0.001 SOL of $FNCPT on mainnet returns a signature within 30 s; balance refreshes; ACTIVITY tab shows the swap row.

**Task 2A.4 — Live smoke test**
- Manual: connect a small mainnet wallet, BUY 0.001 SOL of $FNCPT, then SELL 50 % of the resulting balance. Both must show "confirmed" within 30 s.
- Smoke for the slippage cap: try a swap with the slippage slider at 0.10 % on a low-liquidity moment → expect the wallet to reject the tx, error strip surfaces it cleanly.
- Smoke for cancel: open Confirm, click Cancel → no signature ever requested, no bridge token leaks.

**Stage 2A acceptance:**
- BUY of 0.001 SOL succeeds and balance updates in HoldingsBar within 5 s of `confirmed`.
- SELL of N $FNCPT for the resulting SOL succeeds.
- A swap whose blockhash expired during the dialog is rejected with a clear error. (No special handling needed — wallet rejects the tx; we surface the error string.)
- Slippage > 5 %: SettingsTab clamps the slider; `SwapPanel` rejects user-typed amounts that would breach impact.

### Stage 2A.5 — Multi-token holdings

**Goal:** Crypto Center shows **every** asset the user holds (SOL + every SPL token), with a USD price for each, a portfolio total at the top, and TRADE's swap dropdown populated from real holdings instead of a hard-coded `[SOL, $FNCPT]` pair. Today's FNCPT-only flow only worked because the user happened to be looking at a token they held; the moment someone wants to sell BONK or USDC for $FNCPT, the panel falls over.

The fix is small and self-contained — three producers/services, one new POD, one shared metadata cache. It belongs **before** Stage 2B because the Activity feed wants to render `SWAP SOL → BONK` rows that reference the same metadata cache.

**Why it's a separate stage and not part of 2A:** 2A was already shipped against the FNCPT-only assumption. The pivot to PumpPortal didn't change that assumption (PumpPortal is FNCPT-only by design — it's the issuer's own swap endpoint). Generalising to arbitrary holdings is its own capability with its own data plumbing.

#### 2A.5.0 Decisions locked

| # | Decision | Choice | Rationale |
|---|---|---|---|
| D.5.1 | Source of holdings | `getTokenAccountsByOwner` with `programId = TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA` (no mint filter). Single call returns every SPL account for the wallet. | Already wrapped in `SolanaRpcClient::get_token_balance`; we generalise the existing helper rather than adding a parallel one. |
| D.5.2 | Source of token metadata | **Jupiter "verified" tagged token list** (`https://api.jup.ag/tokens/v2/tagged/verified`) as the primary source; fallback to truncated mint address (`9LUq…pump`) for unverified mints. | Verified live; no key required; covers everything routable. The previous Solana Labs `token-list` repo is frozen. |
| D.5.3 | Source of token prices | **Jupiter Lite Price API** (`lite-api.jup.ag/price/v3?ids=…`) — comma-separated mints. | Already wired for `market:price:fncpt`; same host, same response shape, just batched. Verified live: SOL + USDC + FNCPT in one round-trip. |
| D.5.4 | Spam-token policy | **Hide unverified-list tokens by default**, with a Settings toggle "Show all token accounts (incl. unverified)" that reveals them. Show count in the holdings header: `7 verified · 12 unverified hidden`. | Pump.fun wallets routinely accumulate airdropped junk; defaulting to unverified-only avoids a noisy first-run. The toggle is the escape hatch. |
| D.5.5 | Topic family for prices | Generalise the existing `market:price:fncpt` into `market:price:token:<mint>` family. The `market:price:fncpt` topic stays as an alias of `market:price:token:9LUq…pump` for backward compat with existing subscribers (HoldingsBar, HomeTab, SwapPanel). | Lets any panel subscribe per-mint without a custom service. The producer batches all currently-subscribed mints into one Jupiter call per refresh cycle. |
| D.5.6 | Topic family for holdings | Reuse `wallet:balance:<pubkey>` — change the `WalletBalance` payload shape to carry `QVector<TokenHolding>` instead of just FNCPT fields. | Avoids a separate topic family. All existing consumers (HoldingsBar, HomeTab, SwapPanel, FeeDiscount) already subscribe; they just read different fields. |
| D.5.7 | Where the metadata cache lives | New `TokenMetadataService` (singleton, not a hub Producer — it's a synchronous lookup table fed by one slow background fetch). Cache TTL **24 hours**; persisted to `SecureStorage` so cold starts don't block on a 1 MB download. | A service is the right scope: balance/price producers all need it; the holdings panel needs it; SwapPanel needs it for the FROM/TO combos. |
| D.5.8 | Fallback when metadata is missing | Show `<truncated_mint>` (e.g. `9LUq…pump`) as the symbol, decimals from on-chain `getTokenAccountsByOwner` result, no icon. | Solana RPC always returns decimals in the parsed token-account info, so we never lie about scale even for unknown tokens. |
| D.5.9 | When prices are missing | Show the row but USD column reads `—`; total USD excludes that row and surfaces a footnote `1 token has no price` if any are missing. | A token in the wallet that Jupiter doesn't price is real (held); just unmonetisable for now. Don't hide it — the user might want to swap it. |
| D.5.10 | Total-USD precision | 2 dp when ≥ $1, 6 dp otherwise. Always show even at `$0.00`. | Mirrors Phase 2's Stage 2A formatter fix — zero is a real value, not "unknown". |
| D.5.11 | Trade-FROM token selector | Combo populated from holdings sorted by USD value descending, with verified Jupiter tokens appended as a "swap into" option only on the TO side. | Users sell what they have; users buy what's tradeable. Asymmetric population is the right shape. |
| D.5.12 | Phase-1.5 patterns | Single stylesheet per screen. Producer-owned heartbeats (`TokenPriceProducer` re-fetches all subscribed mints on its TTL). Force-refresh through `WalletService` (existing `force_balance_refresh()` regenerates the holdings list). | Same conventions §1.5.3 established. |
| D.5.13 | Backward compat for `market:price:fncpt` | Keep publishing the alias for one phase. Mark deprecated in `DATAHUB_TOPICS.md`. Remove in Phase 3 cleanup. | Avoids touching every existing consumer in this PR; one cleanup pass later is cheaper than one big PR now. |
| D.5.14 | Filter / sort / search in the holdings table | None this stage. Show top-N by USD value (10 by default), "Show all" link reveals the rest. Search lives with Phase 3 polish. | Keeps stage 2A.5 small. |
| D.5.15 | Out of scope | NFT holdings (Helius DAS would give them; deferred). Hardware-wallet account discovery. Multi-account aggregation (one wallet at a time, same as today). |  |

#### 2A.5.1 Architecture

```
┌──────────────────────────────────────────────────────────────────────────┐
│  TokenMetadataService (singleton, NOT a Producer)                         │
│   - load_from_storage()         // <1ms, called from main()              │
│   - refresh_from_jupiter()      // background, async, fires once a day   │
│   - lookup(mint) → {symbol,name,decimals,icon_url} | not_found           │
│   - notify_changed signal       // panels listen for first-load + daily  │
│   - persistence: SecureStorage key  "token_metadata.cache_v1" (JSON blob)│
│   - hot path is a QReadWriteLock-guarded QHash<QString, TokenMetadata>   │
└────────────┬─────────────────────────────────────────────────────────────┘
             │ called synchronously (read-mostly, ~1µs per lookup)
             ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  WalletBalanceProducer (existing — payload shape upgraded)                │
│   - getTokenAccountsByOwner without a mint filter                         │
│   - merges parsed accounts with TokenMetadataService::lookup              │
│   - publishes WalletBalance{ pubkey, sol_lamports, tokens[], ts_ms }      │
│   - QVector<TokenHolding> tokens — sorted by USD value at publish time   │
│     (price taken from the price producer's last cache; if no cache, raw  │
│     order from RPC).                                                      │
└────────────┬─────────────────────────────────────────────────────────────┘
             │ publishes
             ▼
        wallet:balance:<pk>
             │ subscribed by:
             ▼
   HoldingsBar · HomeTab BalancePanel · SwapPanel · FeeDiscountService

┌──────────────────────────────────────────────────────────────────────────┐
│  TokenPriceProducer (NEW — replaces FncptPriceProducer)                  │
│   - topic_patterns: ["market:price:token:*", "market:price:fncpt"]      │
│   - refresh(topics) — collects all mints from subscribed topics, makes   │
│     ONE Jupiter Lite call: ?ids=<mint1>,<mint2>,…,<mintN>                 │
│   - publishes per-mint TokenPrice{usd, sol, ts_ms, valid}                 │
│   - aliases the FNCPT mint to the legacy market:price:fncpt topic         │
│   - max_requests_per_sec = 2 (one batched call per ~30s, tolerant)       │
└──────────────────────────────────────────────────────────────────────────┘
```

#### 2A.5.2 Topic catalogue changes

| Pattern | Producer | TTL | Min interval | Notes |
|---|---|---|---|---|
| `wallet:balance:<pubkey>` | `WalletBalanceProducer` | 30 s / push | 10 s | **Payload shape upgraded** — see `WalletBalance` POD below. Same producer, same TTL. |
| `market:price:token:<mint>` | `TokenPriceProducer` | 60 s | 30 s | One Jupiter Lite call serves all subscribed mints. Coalesce 250 ms across rapid subscribe events. Shape: `TokenPrice{ mint, usd, sol, ts_ms, valid }`. |
| `market:price:fncpt` (alias) | `TokenPriceProducer` | 60 s | 30 s | Backward-compat alias of `market:price:token:9LUqJ5aQTjQiUCL93gi33LZcscUoSBJNhVCYpPzEpump`. Marked **deprecated** in this rev. Remove in Phase 3. |

#### 2A.5.3 Type changes

```cpp
// services/wallet/WalletTypes.h

struct TokenHolding {
    QString mint;            // base58 SPL mint
    QString symbol;          // resolved via TokenMetadataService; truncated mint if unknown
    QString name;            // "USD Coin" / "" if unknown
    QString amount_raw;      // integer string
    int     decimals = 0;    // always known (RPC parsed value)
    bool    verified = false;// in Jupiter's verified tagged list
    QString icon_url;        // empty if unknown
    double  ui_amount() const noexcept;  // amount_raw / 10^decimals
};

struct WalletBalance {
    QString             pubkey_b58;
    quint64             sol_lamports = 0;
    QVector<TokenHolding> tokens;     // sorted by USD value desc at publish time
    qint64              ts_ms = 0;
    double sol() const noexcept { return sol_lamports / 1e9; }
    // Convenience accessor for legacy callers that still want FNCPT specifically:
    const TokenHolding* fncpt_holding() const noexcept;
    double fncpt_ui() const noexcept;  // 0.0 if not held
    int    fncpt_decimals() const noexcept;
};

struct TokenPrice {
    QString mint;
    double  usd = 0.0;
    double  sol = 0.0;
    qint64  ts_ms = 0;
    bool    valid = false;
};

struct TokenMetadata {
    QString mint;
    QString symbol;
    QString name;
    int     decimals = 0;
    QString icon_url;
    bool    verified = false;
};
```

The legacy `FncptPrice` POD is **kept** as a typedef of `TokenPrice` so `WalletService.cpp` and the price subscribers don't break.

#### 2A.5.4 UI changes

**HoldingsBar** — replaces today's `SOL · $FNCPT · USD · PRICE · UPDATED` with:

```
SOL  9.7425  ·  $FNCPT  0  ·  TOTAL  $1,247.82  ·  UPDATED  3s  ·  ● LIVE  ·  HELIUS
```

`TOTAL` is the sum of `(holding.ui_amount × usd_price)` across SOL + every priced verified holding; rows without prices are excluded with a footnote tooltip on the chip.

**HomeTab / BalancePanel** — replaces the three-stat-box layout with a holdings table:

```
TOKEN          BALANCE         PRICE          USD VALUE       % OF PORT
SOL            9.7425          $84.28         $820.31           65.7%
USDC           420.10          $1.00          $420.10           33.7%
$FNCPT         0               $0.0000185     $0.00              0.0%
WIF            12.500          $1.42          $17.75             1.4%
─── 7 verified · 12 unverified hidden · [Show all] ─────────────────
```

Click a row → SwapPanel pre-fills FROM with that token. Click "Show all" → reveals unverified tokens with a faint "(unverified)" tag and the truncated mint instead of a symbol.

**SwapPanel** — token combos populated from real holdings:
- FROM combo: every token from `wallet:balance` sorted by USD value desc; SOL always at top.
- TO combo: every entry from FROM **plus** Jupiter verified list (filtered to remove the FROM mint). For Phase 2 we'll keep the constraint "$FNCPT must be one side of the trade" until Stage 2C, but the combo wires up to support arbitrary pairs immediately.
- MAX button reads from the selected token's balance.

**SettingsTab** — new "Show unverified tokens" toggle. Persists to `SecureStorage` key `wallet.show_unverified_tokens`.

#### 2A.5.5 Files to add / change / remove

**New files:**

```
src/services/wallet/
  TokenMetadataService.{h,cpp}           // singleton, JSON cache + Jupiter fetch
  TokenPriceProducer.{h,cpp}             // replaces FncptPriceProducer

src/screens/crypto_center/panels/
  HoldingsTable.{h,cpp}                  // owned by HomeTab; replaces three-box layout
```

**Files modified:**

```
src/services/wallet/WalletTypes.h
  - Upgrade WalletBalance shape (TokenHolding vector)
  - Add TokenHolding, TokenPrice, TokenMetadata PODs
  - FncptPrice → typedef of TokenPrice (back-compat)

src/services/wallet/WalletBalanceProducer.{h,cpp}
  - Drop the kFncptMint hard-coding inside the fetch path
  - Call SolanaRpcClient::get_all_token_balances (new helper, see below)
  - Merge metadata via TokenMetadataService::lookup
  - Sort tokens by USD value (read TokenPriceProducer's cache via a non-blocking peek)

src/services/wallet/SolanaRpcClient.{h,cpp}
  - Add: void get_all_token_balances(owner_b58, callback) — getTokenAccountsByOwner
    with programId filter only (no mint filter). Returns vector<SplTokenBalance>.

src/services/wallet/WalletService.{h,cpp}
  - Replace price_producer_ pointer type from FncptPriceProducer* to TokenPriceProducer*
  - ensure_registered_with_hub: register the new producer; update the topic-policy
    pattern from market:price:fncpt to market:price:token:* (alias retained)

src/screens/crypto_center/HoldingsBar.{h,cpp}
  - Replace per-cell SOL/$FNCPT/USD with TOTAL USD + signature SOL/$FNCPT
  - Iterate WalletBalance.tokens to compute total

src/screens/crypto_center/tabs/HomeTab.{h,cpp}
  - Drop the three stat boxes; embed HoldingsTable in their place
  - REFRESH button still calls WalletService::force_balance_refresh

src/screens/crypto_center/panels/SwapPanel.{h,cpp}
  - Populate combos from holdings + Jupiter verified list
  - Compute estimate using selected pair's prices, not FNCPT-only

src/services/billing/FeeDiscountService.{h,cpp}    // when Stage 2C lands
  - Read FNCPT from WalletBalance.fncpt_ui() helper

src/datahub/DataHubMetaTypes.{h,cpp}
  - Register TokenHolding, TokenPrice (FncptPrice still works via typedef)
  - Drop the JupiterQuote registration (already gone, just sanity-check)

src/app/main.cpp
  - Construct + load TokenMetadataService::instance() before WalletService::ensure_registered_with_hub()
  - Service registration already routes through WalletService — no change needed there.

docs/DATAHUB_TOPICS.md
  - Replace the market:price:fncpt row with the new market:price:token:* family
  - Note that market:price:fncpt is a deprecated alias for one phase

CMakeLists.txt
  - Add TokenMetadataService.cpp, TokenPriceProducer.cpp, HoldingsTable.cpp
  - Remove FncptPriceProducer.cpp (file deleted)
```

**Files removed:**

```
src/services/wallet/FncptPriceProducer.{h,cpp}   // replaced by TokenPriceProducer
```

#### 2A.5.6 Tasks

**2A.5.1 — Type plumbing** (foundation; nothing UI-visible)
- New PODs in `WalletTypes.h`; `FncptPrice` typedef of `TokenPrice`.
- Register metatypes in `DataHubMetaTypes.cpp`.
- Verification: builds clean; existing consumers compile against the legacy aliases.

**2A.5.2 — `TokenMetadataService`**
- Singleton; on first instantiation, restore JSON blob from SecureStorage.
- Public API: `lookup(mint) → optional<TokenMetadata>`, `bool is_verified(mint)`.
- Background `refresh_from_jupiter()` runs once on startup if cache is older than 24 h, or on first `lookup()` of a mint absent from cache.
- Backed by `QHash<QString, TokenMetadata>` under a `QReadWriteLock`.
- Verification: unit test the SecureStorage round-trip; manual test that hitting `api.jup.ag/tokens/v2/tagged/verified` (rate-limit-tolerantly) populates the cache.

**2A.5.3 — `SolanaRpcClient::get_all_token_balances`**
- Call `getTokenAccountsByOwner(owner, {programId: TOKEN_PROGRAM_ID}, {encoding: jsonParsed})`.
- Return the same `vector<SplTokenBalance>` shape as today's `get_token_balance`, just unfiltered.
- Verification: against a known-multi-token wallet; expect ≥ 2 entries.

**2A.5.4 — `WalletBalanceProducer` upgrade**
- Replace the FNCPT-only fetch path with `get_all_token_balances`.
- For each `SplTokenBalance`, build a `TokenHolding` with metadata from `TokenMetadataService::lookup()`. Filter spam (decimals=0 amount=1 unless whitelisted) before publish if Settings says so.
- Sort tokens by USD value (read `TokenPriceProducer`'s last-cached value; if no cache yet, sort by raw amount).
- Verification: subscribe to `wallet:balance:<pk>` from a test harness with a multi-token wallet; expect ≥ 2 entries.

**2A.5.5 — `TokenPriceProducer`**
- Topic patterns: `"market:price:token:*"`, `"market:price:fncpt"` (alias).
- `refresh(topics)`: collect all mints, build one `?ids=...` call, publish per-mint.
- Set policy: TTL 60 s, min_interval 30 s, coalesce 250 ms.
- Delete `FncptPriceProducer.{h,cpp}`; route the alias topic into the new producer's mint-extraction logic.
- Verification: subscribe to two arbitrary mints simultaneously; expect a single network call within 250 ms.

**2A.5.6 — `WalletService` rewiring**
- Construct `TokenPriceProducer` instead of `FncptPriceProducer`.
- Update topic-policy registration to cover both `market:price:token:*` and the alias.
- Verification: `market:price:fncpt` still publishes; `market:price:token:So111…1112` (SOL) also publishes.

**2A.5.7 — `HoldingsTable` widget**
- New widget owned by `HomeTab`. Subscribes to `wallet:balance:<pk>` and to `market:price:token:<mint>` for each currently-held mint.
- Renders the table; whole-row click → emits a `select_token(mint)` signal (wired to `SwapPanel` in 2A.5.10).
- "Show all" link toggles unverified visibility for the current session; persisted state lives in SettingsTab.
- Verification: connected wallet shows ≥ 1 row (SOL); a multi-token wallet shows USDC, FNCPT, etc.

**2A.5.8 — `HoldingsBar` rewrite**
- Iterate `WalletBalance.tokens` for the total. Show SOL + signature $FNCPT + TOTAL USD + feed pill + RPC chip.
- Verification: total matches manual sum across SOL + all verified holdings × Jupiter prices.

**2A.5.9 — `HomeTab` BalancePanel layout swap**
- Replace the three stat boxes with the `HoldingsTable` widget. Keep REFRESH + the POLL/STREAM toggle on the panel head.
- Verification: layout renders cleanly at narrow widths (no horizontal scroll).

**2A.5.10 — `SwapPanel` token combos**
- FROM combo populated from `WalletBalance.tokens` sorted by USD value desc. SOL always at top.
- TO combo: union of FROM holdings and Jupiter verified list (minus the FROM mint). For Phase 2, default TO = $FNCPT; Phase 3 lifts the constraint.
- MAX button reads from the **selected** mint's `ui_amount`, with a 0.005 SOL reserve only when the FROM is SOL.
- Estimate: read prices for both FROM and TO from `TokenPriceProducer`; fall back to "estimate unavailable" if either missing.
- Verification: select USDC FROM → MAX → see correct USDC balance prefilled; estimate shows USDC→FNCPT in $FNCPT units.

**2A.5.11 — `SettingsTab` "show unverified" toggle**
- Persist to `SecureStorage` key `wallet.show_unverified_tokens`.
- HoldingsTable + SwapPanel honor the flag.

**2A.5.12 — Doc + topic catalogue update**
- `docs/DATAHUB_TOPICS.md`: replace `market:price:fncpt` row with `market:price:token:*`. Mark legacy alias deprecated. Document `wallet:balance:<pubkey>` shape change.
- `plans/crypto-center-future-phases.md` §2.5: bring the topics list in line.

#### 2A.5.7 Acceptance criteria

- [ ] HoldingsBar shows TOTAL USD = sum of (SOL × SOL price) + Σ (token × token price) for verified holdings.
- [ ] HomeTab shows a holdings table with at least the user's actually-held tokens (SOL, FNCPT, plus whatever airdrops they have if "Show all" is on).
- [ ] SwapPanel FROM combo lists every held token; selecting a non-FNCPT token populates balance correctly and estimate shows in TO units.
- [ ] First-run cold cache: TokenMetadataService loads from SecureStorage in < 5 ms; UI never shows raw mint addresses for verified tokens after the first balance publish.
- [ ] No Rust source. No new on-chain code. No Helius dependency for the verified-tokens path (Helius only used for parsed-tx in Stage 2B).
- [ ] Phase 1.5 patterns honored: single stylesheet per screen, FeedStatus on HoldingsBar still drives via objectName swap, force-refresh through `WalletService::force_balance_refresh`.

#### 2A.5.8 Risks & open questions

1. **Jupiter token-list size.** Verified list is ~500KB-1MB. SecureStorage handles strings of that size today, but worth confirming on first-run write. Fallback if SecureStorage chokes: write to a plain JSON file under `AppData/.../cache/`.
2. **Jupiter rate limits.** The `tokens/v2` endpoint rate-limited me during planning. The price endpoint is fine. We refresh metadata once a day so the rate-limit risk is low; but cold-start on a fresh install needs to handle a 429 by retrying with backoff.
3. **NFTs.** SPL-token-account RPC returns NFTs (decimals=0, amount=1). Spam filter handles them by default; user can opt in via "Show all". A real NFT panel ships in a later phase.
4. **Sort-by-USD when no price.** Producer publishes before the price call returns on first run. Tokens without prices sort to the bottom (alphabetical fallback by symbol), then re-sort on the next publish once prices arrive. Acceptable jitter; documented.
5. **Total-USD precision over time.** Rounding errors across 50 tokens at 6dp could accumulate. Sum at full precision, format only at display.
6. **`market:price:fncpt` alias retirement.** D.5.13 says one phase. Need a follow-up TODO when Phase 3 starts.

---

### Stage 2B — Activity feed

**Goal:** users can see what their wallet has done.

**Task 2B.1 — `WalletActivityProducer`**
- Topic `wallet:activity:<pubkey>`. Policy `ttl_ms=30_000, min_interval_ms=10_000`.
- `refresh()` calls Helius `/v0/addresses/<pk>/transactions?limit=50` (auth via existing `solana.helius_api_key`).
- Parses each tx into `ParsedActivity { ts, kind: SWAP|RECEIVE|SEND|OTHER, asset, amount_ui, signature }`. Helius parsing handles SWAP / RECEIVE / SEND directly; OTHER for everything we can't classify.
- Fall back: if Helius key missing, use `SolanaRpcClient::get_signatures_for_address` for raw signatures + show "Add a Helius key for parsed activity" footer in the tab.
- Verification: subscribe in a test harness, observe publish, change pubkey, observe new topic.

**Task 2B.2 — `ActivityTab` UI**
- `QTableView` with TIMESTAMP / EVENT / ASSET / AMOUNT / STATUS columns + whole-row click → Solscan.
- Filter chips above the table for SWAP / SEND / RECEIVE / OTHER.
- Empty state shows "No transactions yet" with a CONNECT WALLET fallback if disconnected.
- Replaces the `ComingSoonTab` in CryptoCenterScreen's ACTIVITY slot.
- Verification: ACTIVITY tab shows the swap from Stage 2A within one refresh cycle.

**Stage 2B acceptance:**
- ACTIVITY tab populates with the last 50 parsed events for the connected wallet.
- Clicking a row opens Solscan in the default browser.
- Filter chips narrow the list correctly.

### Stage 2C — Fee discount

**Goal:** eligible users see a "30 % off" badge wherever applicable.

**Task 2C.1 — `FeeDiscountConfig.h`**
- Constants: `kThresholdRaw = 1_000 * 10^6` (1,000 $FNCPT, 6 decimals); `kDiscountPct = 30`; `kAppliedSkus = {"ai-report", "deep-backtest", "premium-screen"}`.
- Header-only; no .cpp.

**Task 2C.2 — `FeeDiscountService`**
- Subscribes `wallet:balance:<pk>` internally (consumer-producer pattern from Phase 1's price-balance interplay).
- Publishes `billing:fncpt_discount:<pk>` = `{ eligible, threshold_raw, threshold_decimals, applied_skus }`.
- Re-publishes whenever the balance topic emits.
- Register in main.cpp alongside the other Phase 2 producers.
- Verification: balance publishes ≥ 1,000 → eligibility chip lights up; balance drops below threshold → chip extinguishes within one publish cycle.

**Task 2C.3 — `FeeDiscountPanel`**
- Owned by `TradeTab`; replaces today's placeholder.
- Subscribes `billing:fncpt_discount:<pk>`.
- Shows: balance-vs-threshold bar, applied SKUs as bullet list, projected savings on a hypothetical AI report ($x → $y).
- Read-only. Phase 3's Tier service will swap this for a richer panel.

**Task 2C.4 — `HoldingsBar` chip**
- The `discount_chip_` widget already exists (placeholder in Stage 2.1).
- Wire it: subscribe `billing:fncpt_discount:<current_pubkey>`, show/hide based on `eligible`.
- No new layout work.

**Stage 2C acceptance:**
- HoldingsBar and FeeDiscountPanel show "30 % off" for an account holding ≥ 1,000 $FNCPT.
- Balance dropping below threshold flips the chip back inside one publish cycle.

### Stage 2D — Polish, docs, security walkthrough

- Update `docs/CRYPTO_CENTER_PHASE_2.md` with screenshots of BUY, SELL, ACTIVITY.
- Walk every threat in §2 against the implementation; sign off in `plans/crypto-center-trade-security.md`.
- Update `plans/crypto-center-future-phases.md` to mark Phase 2 shipped (move §2 into a `crypto-center-phase-2-shipped.md` snapshot, including the burn-deferred decision).
- Confirm Phase 1.5 patterns (single stylesheet, FeedStatus pill, force-refresh through services, producer-owned heartbeats, endpoint-capability checks, screen-side staleness, error-strip pattern, RPC chip) were honored throughout.

---

## 6. Risks & open questions

1. **PumpPortal availability.** The endpoint is third-party; if pumpportal.fun goes down, swaps break. Mitigation: the screen renders an empty TRADE tab with "Swap service temporarily unavailable" instead of crashing. We can fall back to Jupiter (the earlier rev's code) on Phase 3 or later if reliability becomes an issue — keeping the swap.html signing path generic means the swap-build service is hot-swappable.
2. **PumpPortal rate limits.** No public spec. Empirically tolerant for low-volume use. If we hit limits, add a `min_interval_ms` on the build call (note: this is one-shot, not a hub topic, so the throttle would live inside `PumpFunSwapService`).
3. **PumpPortal MITM.** TLS pinning would close the residual risk; out of scope this phase, listed as Phase 2.5 polish.
4. **Helius parsed-tx coverage.** Some swap routes don't decode into a clean SWAP row. Acceptable: those show as OTHER with the signature link.
5. **Burn deferral.** Real users will ask for it. Holding line on Phase 5 buyback worker as the right place.
6. **Slippage UX.** 1 %/5 % defaults are correct for $FNCPT today (thin liquidity); revisit when Phase 5 buyback narrows spreads.
7. **Failure during multi-leg swap.** PumpPortal returns a single versioned tx so partial fills aren't a concern; we just surface the error.

---

## 7. Out of scope for Phase 2

- **Burn UI.** Deferred to Phase 5 (buyback automation).
- **Direct Ledger USB.** Use Phantom/Solflare's Ledger support.
- **Multi-chain.** $FNCPT stays on Solana.
- **Server-backed entitlement** (Phase 3 owns this).
- **WebSocket parsed-tx feed for activity** (poll is enough for Phase 2 traffic).
- **Per-trade priority-fee override** (Settings-level only this phase).
- **Limit orders, advanced order types.** PumpPortal doesn't support them; out of scope.

---

## 8. Definition of Done

- [ ] User opens TRADE tab, sees a SwapPanel with current spot price and projected output. BUY of 0.001 SOL on mainnet succeeds in ≤ 30 s.
- [ ] User opens TRADE tab, can SELL 50 % of their $FNCPT and see the resulting SOL within ≤ 30 s.
- [ ] HoldingsBar shows live SOL · $FNCPT · USD · feed pill on every tab.
- [ ] ACTIVITY tab populates with the last 50 parsed events; clicking a row opens Solscan.
- [ ] FeeDiscount chip appears on HoldingsBar at ≥ 1,000 $FNCPT and disappears below.
- [ ] All P-rules and D-rules from CLAUDE.md hold; lint passes.
- [ ] Phase 1.5 patterns (single stylesheet per screen, FeedStatus pill, force-refresh through services) are honored.
- [ ] `docs/DATAHUB_TOPICS.md` lists `wallet:activity:*` and `billing:fncpt_discount:*` (and **does not** list `wallet:quote:*` or `wallet:burn_receipt:*`).
- [ ] `crypto-center-trade-security.md` is signed off.
- [ ] No Rust source anywhere in the tree.

---

## 9. Document maintenance

Update this doc at each stage kickoff with the actual PR diffs. When Phase 2 ships, archive into `crypto-center-phase-2-shipped.md` with the verified-pattern subset of §1.5.3 expanded for transactional flows. The pivot-to-PumpPortal decision (D1) and the burn-deferral (D6) should be called out at the top of that archive so future phases inherit the rationale, not just the diff.
