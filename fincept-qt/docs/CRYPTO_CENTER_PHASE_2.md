# Crypto Center — Phase 2 user guide

Phase 2 turns the Crypto Center from a read-only identity surface into a transactional one. You can now:

- See **every token** your wallet holds (SOL + every SPL token), with USD prices and portfolio total.
- **Buy or sell $FNCPT** for SOL through PumpPortal directly inside the terminal.
- Browse **the last 50 on-chain operations** for your wallet, parsed into human-readable events.
- Earn a **30% fee discount** on premium features when you hold ≥ 1,000 $FNCPT.

The terminal never holds your private keys. Every transaction is signed in your wallet (Phantom, Solflare, etc.) — the terminal only forwards the unsigned bytes and waits for the signed result. See `CRYPTO_WALLET_CONNECT.md` for the connection flow.

---

## The screen at a glance

```
┌─ FINCEPT / CRYPTO CENTER ──────────── ● CONNECTED · 7eV4z…aoowY ────┐
│  HOLDINGS BAR                                                        │
│  SOL 9.74  ·  $FNCPT 0  ·  TOTAL $1,247.82  ·  ● LIVE  ·  HELIUS    │
├──────┬───────┬─────────┬────────┬───────┬────────┬─────────────────┤
│ HOME │ TRADE │ ACTIVITY│ SETTINGS│ STAKE│ MARKETS│ ROADMAP         │
└──────┴───────┴─────────┴────────┴───────┴────────┴─────────────────┘
```

The HOLDINGS BAR sits above every tab — your portfolio total never disappears even when you switch tabs. Stake / Markets / Roadmap are placeholders for Phases 3 / 4 / 5 respectively.

---

## HOME tab

The HOME tab now shows a **holdings table** instead of three stat boxes. One row per asset:

| TOKEN | BALANCE | PRICE | USD VALUE | % OF PORT |
|---|---|---|---|---|
| SOL | 9.7425 | $84.28 | $820.31 | 65.7% |
| USDC | 420.10 | $1.00 | $420.10 | 33.7% |
| $FNCPT | 12,304 | $0.0000185 | $0.23 | 0.0% |

Click a row → you're dropped into the **TRADE** tab with that token pre-filled as the FROM side of a swap.

**Verified vs unverified.** By default only Jupiter-verified tokens show — pump.fun wallets accumulate junk airdrops, and showing them all would be noisy. The footer reads `7 verified · 12 unverified hidden` with a **Show all** link. The persistent toggle lives in **Settings**.

**Refresh.** The holdings refresh themselves on a 30-second cadence (or via WebSocket if you're on Helius streaming). Click **REFRESH** to force an immediate re-fetch.

---

## TRADE tab — Swap

```
┌─ SWAP ──────────────────── via PumpPortal · pool=auto ──┐
│  YOU PAY    [   0.50  ] SOL ▾    Balance: 1.20 SOL      │
│             [MAX]                                       │
│                                                         │
│  YOU RECEIVE (EST.)  ≈ 12,304 $FNCPT  (~$0.23)         │
│             [$FNCPT ▾]                                  │
│  ──────────────────────────────────────────────────    │
│  ROUTE         PumpSwap (auto)                          │
│  PRICE IMPACT  set by PumpSwap; capped by slippage      │
│  MAX SLIPPAGE  1.00%                                    │
│  [SWAP]                                                 │
└─────────────────────────────────────────────────────────┘
```

**Direction.** The FROM and TO combos are populated from your real holdings (FROM) and from the Jupiter verified list + your holdings (TO). For Phase 2, **PumpPortal only routes SOL ↔ $FNCPT** — selecting any other pair shows a "Phase 3 supports arbitrary pairs" message and disables the SWAP button. The combo plumbing is already there for Phase 3.

**Estimate.** PumpPortal has no quote endpoint, so the "YOU RECEIVE" estimate is computed locally from live spot prices (`market:price:token:<mint>`) times the slippage budget. The actual fill is set by PumpSwap at execution; the wallet shows you the decoded transaction independently.

**Slippage.** Default 1%, adjustable up to 5% in Settings. PumpPortal expects an integer percent.

**The SWAP click flow:**

1. **Build** — terminal asks PumpPortal for an unsigned versioned tx (~1 KB).
2. **MITM gate** — terminal simulates the tx against your RPC. If the simulation reports a revert, the flow aborts before the wallet ever pops. This catches a malicious PumpPortal response that would drain or otherwise misbehave.
3. **Confirm dialog** — you see a decoded summary: route, amount paid, estimated received, slippage, priority fee, and the simulation result ("OK · N CU"). The SWAP button has a **1.5-second anti-muscle-memory delay** before it arms.
4. **Freshness gate** — right after you click SWAP in the dialog, the terminal re-simulates with the tx's own blockhash (no replacement). If the blockhash has expired during the time you were reading the dialog, the flow aborts and asks you to retry.
5. **Wallet signature** — the terminal forwards the unsigned bytes to your wallet over the local bridge. You sign in the wallet UI.
6. **Submit + poll** — the terminal sends the signed tx to the network and polls `getSignatureStatuses` every 1.5 seconds. After `confirmed`, your balance refreshes automatically (within ~5 seconds).

If anything goes wrong at any step, you see a red error strip with the specific failure. Funds never leave your wallet without your signature.

---

## ACTIVITY tab

The last 50 on-chain operations involving your wallet, refreshed every 30 seconds:

```
TIMESTAMP            EVENT     ASSET          AMOUNT          STATUS
2026-04-28 09:14:02  SWAP      SOL → FNCPT    0.5 → 12,304    confirmed
2026-04-27 14:02:11  RECEIVE   SOL            0.10            confirmed
2026-04-26 11:44:30  SEND      USDC           50.00           confirmed
```

**Filter chips** above the table: ALL · SWAP · SEND · RECEIVE · OTHER.

**Click a row** → opens Solscan in your default browser at the transaction signature.

**Helius parsing.** SWAP / SEND / RECEIVE come from the Helius parsed-tx endpoint (`api.helius.xyz/v0/addresses/<pk>/transactions`). Without a Helius key, the tab falls back to raw signatures only — every event shows as OTHER, but the Solscan link still works. Add a Helius key in **Settings** for real parsing.

---

## Fee discount

Every paid feature in Fincept Terminal — AI reports, deep backtests, premium screens — accepts $FNCPT at a **30% discount** when you hold **≥ 1,000 $FNCPT**.

The eligibility chip lights up on the HOLDINGS BAR (`30% OFF`) and the TRADE tab shows projected savings against your current balance. If your balance drops below the threshold, the chip extinguishes within one publish cycle (~30 seconds).

The threshold and applied SKUs are read from `services/billing/FeeDiscountConfig.h` — header-only constants that get easier to swap as Phase 3's tier system lands.

---

## Settings tab

- **Default slippage** — 1% (default) up to 5%. Stored in SecureStorage as `wallet.default_slippage_bps`.
- **Solana RPC** — paste a custom RPC URL in `solana.rpc_url`, or a Helius API key in `solana.helius_api_key`. The HOLDINGS BAR shows `PUBLIC` / `HELIUS` / `CUSTOM` so you always know which RPC you're hitting. Helius is recommended — the public mainnet RPC is rate-limited and disables WebSocket streaming.
- **Show unverified tokens** — toggle for the holdings table and SWAP combos.

Stake / Markets / Roadmap settings ship with their respective phases.

---

## What's not in Phase 2

These ship in later phases:

- **Burn $FNCPT** — deferred to Phase 5 where the buyback worker burns from a treasury account.
- **Arbitrary pair swaps** — Phase 3 lifts the SOL↔$FNCPT constraint via a generalized router.
- **Stake / lock** — Phase 3, veFNCPT-style lock with USDC yield.
- **Prediction markets** — Phase 4, internal venue settling in $FNCPT.
- **Buyback dashboard** — Phase 5, replaces the static ROADMAP placeholder.

See `plans/crypto-center-future-phases.md` for the full roadmap.

---

## Where to find this in code

- `src/screens/crypto_center/` — screen + tabs + panels
- `src/services/wallet/PumpFunSwapService.{h,cpp}` — PumpPortal client
- `src/services/wallet/SolanaRpcClient.{h,cpp}` — RPC wrapper, simulation, status polling
- `src/services/wallet/WalletActivityProducer.{h,cpp}` — Helius parsed-tx
- `src/services/wallet/TokenPriceProducer.{h,cpp}` — Jupiter Lite Price API, batched per-mint
- `src/services/wallet/TokenMetadataService.{h,cpp}` — Jupiter verified token list, daily refresh
- `src/services/billing/FeeDiscountService.{h,cpp}` — eligibility derived from balance
- `resources/wallet/swap.html` — wallet-side signing bridge
- `docs/DATAHUB_TOPICS.md` — every hub topic the screen subscribes to
- `plans/crypto-center-phase-2.md` — implementation plan
- `plans/crypto-center-trade-security.md` — security walkthrough

---

**Last updated:** 2026-05-01
