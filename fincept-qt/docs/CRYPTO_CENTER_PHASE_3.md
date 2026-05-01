# Crypto Center — Phase 3 user guide (STAKE)

The **STAKE** tab lets you lock $FNCPT for 3 months to 4 years in exchange for **veFNCPT weight** — a non-transferable token-account that grants two things:

1. A pro-rata share of **terminal revenue paid in USDC** (real yield), settled per epoch by the buyback worker.
2. A **billing tier** (Bronze / Silver / Gold) that unlocks paid screens elsewhere in the terminal — AI Quant Lab, Alpha Arena, premium agents.

---

## What you see

```
┌─ STAKE / LOCK ──────────────────────────────────── DEMO ────────────┐
│  AMOUNT       [   1,000 ] $FNCPT   AVAILABLE   12,304               │
│  DURATION     ◯ 3 MO  ◯ 6 MO  ● 1 YR  ◯ 2 YR  ◯ 4 YR                │
│  WEIGHT       1,000 veFNCPT × 0.25 (1 yr) = 250 veFNCPT             │
│  EST. YIELD   $42 / week (USDC)                                     │
│  TIER         Silver → Gold (after lock)                            │
│  [PREVIEW LOCK]    [LOCK]                                           │
└──────────────────────────────────────────────────────────────────────┘

┌─ ACTIVE LOCKS · 3 positions · Σ 4,500 veFNCPT ── DEMO ──────────────┐
│  LOCKED   DURATION   UNLOCKS         WEIGHT      YIELD (LIFETIME)   │
│  2,000    4 yr       2030-04-27      2,000       $1,240 USDC        │
│  1,000    1 yr       2027-04-01      250         $42 USDC           │
│  500      6 mo       2026-10-27      62.5        $14 USDC           │
└──────────────────────────────────────────────────────────────────────┘

┌─ TIER ────────────────────────────────── current GOLD ──────────────┐
│  BRONZE   100+ veFNCPT       basic API quota                        │
│  SILVER   1k+ veFNCPT        premium screens                        │
│  GOLD     10k+ veFNCPT       all agents + arena                     │
└──────────────────────────────────────────────────────────────────────┘
```

---

## Demo mode

Phase 3 ships in **demo mode** today. The on-chain `fincept_lock` Anchor program (the contract that actually escrows your $FNCPT and mints the veFNCPT position NFT) is not yet deployed. While in demo mode:

- Every payload carries `is_mock=true`; the head pill on each panel reads **DEMO**.
- The **PREVIEW LOCK** button shows the math you'd get from the multiplier table.
- The **LOCK** button is disabled with a "fincept_lock not deployed" tooltip.
- `ActiveLocks` shows three illustrative positions so you can see the table layout. These are not your positions; they are the same demo numbers shown to every user.

When the program ships:

1. We'll publish the program ID.
2. You set `fincept.lock_program_id` in `SecureStorage` (Settings → Crypto → Advanced).
3. The next refresh moves you off mock data: `wallet:locks:<pubkey>` will query `getProgramAccounts` and render your real positions; the **LOCK** button enables and routes through `WalletService::sign_and_send`.

No code changes on your side — the seam is wired.

---

## How the multiplier works

veFNCPT weight is computed at lock time and **decays linearly to zero** as you approach the unlock date. The duration multipliers (vs. a 4-yr max-weight lock):

| Duration | Multiplier |
|---|---|
| 3 months | 0.0625 |
| 6 months | 0.125 |
| 1 year   | 0.25 |
| 2 years  | 0.5 |
| 4 years  | 1.0 |

Example: locking 1,000 $FNCPT for 1 year = 250 veFNCPT. Locking 1,000 $FNCPT for 4 years = 1,000 veFNCPT.

The expected weekly USDC yield is `your_weight / total_weight × 25 % × terminal_revenue`. 25 % is the staker share of revenue per the Phase 5 buyback split (50 / 25 / 25 across buyback / staker yield / treasury top-up).

---

## Tier-driven gating

Once `billing:tier:<your_pubkey>` lands at Silver+, paid screens that were hidden in the nav appear. Specifically (plan §3.6):

- **Silver** (1k+ veFNCPT) reveals premium screens (AI Quant Lab access).
- **Gold** (10k+ veFNCPT) reveals Alpha Arena + every agent.

The `TierService` emits `tier_changed(pubkey, tier)` and the gated screens listen for it — so the moment your tier increases, the nav updates without a restart. If you can't see a screen you expect, check the TIER panel on the STAKE tab; the `next_threshold_raw` field tells you exactly how much more veFNCPT you need.

---

## What's not here yet

- **Governance voting.** Phase 3 ships the lock surface only; voting on `fincept_market` listings (Phase 4) and on protocol parameters (Phase 5) lands in a follow-up.
- **Lock extension UI.** You can only create new locks today. Extending an existing lock will land alongside the program deployment — the helper exists in `StakingService::build_extend_tx` but the UI button is hidden in mock mode.
- **Withdraw on expiry.** Same as above — the auto-prompt fires when `unlock_ts <= now`, but only against real on-chain positions.
