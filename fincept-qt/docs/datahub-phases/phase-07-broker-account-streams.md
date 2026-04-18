# Phase 7 — Broker Account Streams

**Status:** Not started
**Depends on:** Phase 1 (hub primitive) and Phase 4 (WebSocket producer
pattern + `coalesce_within_ms` + `active_subscriber_delta` signal).
**Nominal duration:** 1–2 weeks (16 adapters × moderate touch each)
**Rough size:** Large — the most surface-area phase in the rollout.

---

## Goal

Unify live account data from all 16 broker integrations behind a
single topic family: `broker:<id>:positions`, `broker:<id>:orders`,
`broker:<id>:balance`, `broker:<id>:ticks:<sym>`. Two accounts on the
same broker stay isolated via the multi-account topic extension
`broker:<id>:<account_id>:<channel>`.

The payoff: a user with three brokers (Zerodha + IBKR + Kraken) and
two screens open (dashboard portfolio summary + equity trading
screen) gets **one** WS connection per broker, not six. Today each
screen owns its own connection; removing the duplication is one of
the largest wins in this refactor.

---

## Targets

### 16 broker adapters (from memory: `project_broker_infra.md`)

Indian brokers (IndianBrokers base):
Zerodha, Angel One, Upstox, Fyers, Dhan, Groww, Kotak, IIFL, 5paisa,
AliceBlue, Shoonya, Motilal.

Global brokers (GlobalBrokers base):
IBKR, Alpaca, Tradier, Saxo.

Each lives under `src/trading/brokers/<broker>/`.

### Cross-cutting infrastructure
- `BrokerInterface` — abstract base, contract unchanged.
- `BrokerHttp` — blocking HTTP via `QEventLoop`. Memory note
  `project_broker_http_blocking.md` warns that it must be called from
  `QtConcurrent::run`. Preserved — hub producers marshal back to the
  hub thread via queued connection.
- `AccountManager`, `AccountDataStream`, `DataStreamManager` (from
  `project_multi_account.md`). Current multi-account plumbing — this
  phase refactors to publish to the hub per-account.

### Consumers
- `EquityTradingScreen` (`src/screens/equity_trading/...`) —
  positions, orders, balance panels.
- `DashboardScreen` widgets: `PortfolioSummaryWidget`,
  `QuickTradeWidget`, `RiskMetricsWidget` already get quote data
  from the hub (Phase 3); their broker-sourced parts move here.
- `PortfolioBlotter` — position rows.
- `AlgoTradingScreen` — order status.
- Multi-account selector UI.

---

## Deliverables

### Topic families

- `broker:<id>:positions` — TTL 5 s, poll fallback if WS not
  available. `<id>` is the canonical lower-case broker slug
  (`zerodha`, `angel_one`, `ibkr`, ...).
- `broker:<id>:orders` — TTL 5 s. Live order book for the account.
- `broker:<id>:balance` — TTL 30 s. Cash + margin.
- `broker:<id>:ticks:<sym>` — push-only. Per-broker market tick feed
  (Zerodha Kite's binary WS, Angel SmartStream, Fyers ticker).

Multi-account extension (for users with multiple accounts on the
same broker): `broker:<id>:<account_id>:<channel>`. If a broker
supports only one account per login, we still use the long form —
`<account_id>` defaults to `default`. Single-account and
multi-account code paths are identical.

### `BrokerProducer` mixin

Add `src/trading/brokers/base/BrokerProducer.{h,cpp}`. Implements
`Producer` on behalf of a broker adapter. Responsibilities:

- `topic_patterns()` returns the four patterns for this broker's
  slug (`broker:zerodha:*`, etc.).
- `refresh(topics)` routes by channel:
  - `broker:<id>:<acct>:positions` → call
    `BrokerInterface::fetch_positions(acct)` inside
    `QtConcurrent::run` (because `BrokerHttp::execute` blocks), then
    publish the result.
  - `broker:<id>:<acct>:orders` → similar.
  - `broker:<id>:<acct>:balance` → similar.
  - `broker:<id>:<acct>:ticks:*` → **no-op**. Ticks are push-only.
- `on_topic_idle(topic)` — for tick topics, unsubscribe the symbol
  from the broker WS; for positions/orders/balance, no-op.
- On the broker's own WS message, call `hub.publish(topic, payload)`.

`BrokerProducer` subclasses — one per broker — so each broker can
override `start_tick_feed(account, symbol)` / `stop_tick_feed` in the
broker's native way (Kite WS protocol is binary; Alpaca uses
Server-Sent Events; Saxo has streaming via OpenAPI). The Producer
base handles the hub-facing side; the subclass handles the
broker-specific protocol.

### `AccountDataStream` / `DataStreamManager` refactor

These classes currently emit Qt signals per-account. Change them to
publish per-account topics. Consumers no longer subscribe to
`AccountDataStream` signals — they subscribe to
`broker:zerodha:my_account:positions` directly.

`AccountManager` gains an `active_account_id(broker_id)` getter; the
equity trading screen uses it to build the correct topic string for
subscribe.

### Consumer migration
- `EquityTradingScreen` — three panels (positions, orders, balance)
  subscribe to their respective topics for the currently selected
  account. Account switch triggers
  `hub.unsubscribe_all(this)` + resubscribe.
- Dashboard widgets consuming broker data — same pattern.
- `AlgoTradingScreen` — subscribes to `broker:<id>:<acct>:orders`
  and pushes state changes into its order-status table.
- Multi-account selector — reads `hub.stats()` to display which
  accounts are currently streaming; no direct subscription of its
  own.

### Cross-thread marshaling audit
Every broker SDK has its own threading model (Zerodha uses a
background thread; Alpaca emits on the Network thread; IBKR's own
thread pool for the TWS API). Every `hub.publish(...)` call from a
broker callback must be safe. The hub guarantees this via
`QueuedConnection` (Phase 1 cross-thread test covers it). Per-broker
smoke test added to verify ticks from the broker's thread arrive on
the UI.

---

## Success check

- Open EquityTradingScreen with Zerodha connected **and** dashboard
  portfolio summary visible. Log shows one active Zerodha WS
  subscription, not two. `hub.stats()` for
  `broker:zerodha:default:positions` shows subscriber_count = 2.
- Connect a second Zerodha account (multi-account). Both accounts'
  positions stream independently under distinct topics; switching
  the "active account" selector swaps the visible panel without
  dropping the other account's stream (it's still alive for
  background widgets that subscribe to both).
- Every `BrokerHttp::execute` call happens inside
  `QtConcurrent::run` — no UI freeze on account switch. Verified by
  adding a temporary `Q_ASSERT(QThread::currentThread() != qApp->thread())`
  at the top of `BrokerHttp::execute` for smoke testing (removed
  before merge).
- Every broker adapter's smoke test: place a paper order, expect
  `broker:<id>:<acct>:orders` to publish within 2 s. Verified across
  all 16 brokers.
- Tick feed consolidation: open CryptoWidget (Kraken via
  `ws:kraken:*`, Phase 4) alongside a Zerodha-powered equity widget
  streaming `broker:zerodha:<acct>:ticks:NIFTY`. Two distinct WS
  connections (one per broker/exchange), not four.

---

## Risk

- **16 brokers × varying SDK quality.** Not all broker SDKs expose
  clean first-subscriber / last-unsubscriber hooks. Mitigation: for
  the problem brokers, fall back to continuous polling of
  `fetch_positions` etc. on a long interval (10 s); the hub's TTL
  already gates outbound calls.
- **Per-account isolation bugs.** If `<account_id>` isn't threaded
  through every code path, two accounts could collide (account B
  sees A's positions). Mitigation: mandatory account_id parameter
  in `BrokerProducer::refresh()`'s topic parse; assertion fires if
  topic lacks the account segment.
- **Credential / auth refresh races.** When a broker's auth token
  expires mid-stream, the producer must re-auth and keep the topic
  alive — not drop the subscriber. Mitigation: audit existing
  auth-refresh flow in each adapter; ensure it stays in place,
  hub-side is unaffected.
- **BrokerHttp blocking trap.** Memory `project_broker_http_blocking.md`
  calls out this sharp edge. Every hub `refresh()` call that invokes
  the broker must wrap in `QtConcurrent::run`. Mitigation: the
  `BrokerProducer::refresh()` base implementation always wraps — the
  subclass can't accidentally call blocking HTTP on the UI thread.
- **Large diff, long integration window.** Mitigation: migrate
  brokers one-by-one in separate PRs; finance-critical flows get
  extra manual testing per broker. This matches the existing
  broker-refactor cadence from memory
  (`feedback_broker_work.md` — one broker at a time).
- **Tick coalescing on broker feeds.** Zerodha's "full mode" tick
  can stream 5+ updates per symbol per second. Apply
  `coalesce_within_ms = 100` on `broker:*:*:ticks:*` by default.

---

## Rollback

- **Per-broker revert.** Each broker's adapter migration is a
  separate PR; revert the broken broker, the other 15 still work.
- **Producer-off switch.** If a broker's hub publishing misfires,
  unregister the Producer — consumers fall back to the legacy
  `AccountDataStream` signal path, which stays alive for the
  duration of this phase. Only removed in Phase 10.
- **Multi-account topic format change.** If the four-segment topic
  string turns out to be wrong (e.g. if a broker ID contains a
  colon), hub can be patched to URL-encode path segments without
  affecting subscribers — they always build topics via a helper
  `broker_topic(broker_id, account_id, channel, ...)` that
  encapsulates the format.

---

## Out of scope

- Order submission / modification flow — unchanged. Hub is for
  **reading** broker state; placing orders still goes through
  `BrokerInterface::place_order()` directly (fire-and-forget;
  result arrives via the orders topic anyway).
- Broker-specific screens beyond the generic equity/portfolio flow
  (e.g. IBKR advanced options chain) — assess per-broker in Phase
  9 or defer.
- Paper trading engine (`src/trading/paper/`) — uses its own
  deterministic "broker" and doesn't need hub publishing until an
  explicit request.
- Deleting the legacy `AccountDataStream` signal API — Phase 10.
