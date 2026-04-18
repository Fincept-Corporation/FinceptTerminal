# Phase 4 — WebSocket Producers

**Status:** Not started
**Depends on:** Phase 1 (hub primitive). Does **not** block on Phase 3 —
can run in parallel since it touches different services.
**Nominal duration:** 3–4 days
**Rough size:** Medium (2 producers, ~3 consumer touch points)

---

## Goal

Unify all real-time streaming data under the hub using `push_only`
producers. Today Kraken and HyperLiquid feeds go through
`ExchangeService` signals, and Polymarket order-book / trade streams
are emitted bespoke by `PolymarketWebSocket`. Both paths become
DataHub Producers — one WebSocket connection per exchange/market,
shared across every subscriber.

Key invariant: **WebSocket channels open lazily** when the first
subscriber appears, and close when the last subscriber disappears. This
replaces today's pattern where the screen containing a crypto chart
opens its own WS even if the dashboard crypto widget is already
streaming the same symbol.

---

## Targets

| Producer                   | Source file                                             | Current role |
|----------------------------|---------------------------------------------------------|--------------|
| `ExchangeService`          | `src/trading/ExchangeService.{h,cpp}`                   | Long-running WS subprocess for Kraken + HyperLiquid |
| `PolymarketWebSocket`      | `src/services/polymarket/PolymarketWebSocket.{h,cpp}`   | CLOB WS client for order book + trades |

### Topic families

- `ws:kraken:ticker:<pair>`   — push-only, last-trade ticks
- `ws:kraken:ohlc:<pair>:<interval>` — push-only, candle updates
- `ws:kraken:orderbook:<pair>` — push-only, top-of-book snapshots
- `ws:hyperliquid:ticker:<coin>` — push-only
- `ws:hyperliquid:trades:<coin>` — push-only
- `ws:hyperliquid:orderbook:<coin>` — push-only
- `polymarket:orderbook:<condition_id>` — push-only
- `polymarket:trades:<condition_id>` — push-only
- `polymarket:price:<condition_id>:<outcome>` — push-only

All policies: `push_only=true`, `ttl_ms=0` (never refresh),
`min_interval_ms=0` (publish as fast as the feed sends). Add
`coalesce_within_ms` to `TopicPolicy` (see Risk below) if observed
rates exceed 20 Hz per topic.

---

## Deliverables

### `ExchangeService` → Producer
- Implement `Producer`:
  - `topic_patterns()` → `{"ws:kraken:*", "ws:hyperliquid:*"}`
  - `refresh(topics)` → **no-op**. Push-only producers never schedule.
  - `on_topic_idle(topic)` → unsubscribe the specific WS channel.
    Close the WS connection to an exchange only when all its topics
    are idle (ref-counted inside the service).
- First-subscriber hook: the service needs to know when a new
  subscriber attaches. Add a hub API call it polls for, **or** provide
  a signal on the hub itself: `topic_activated(QString)` emitted when a
  topic's subscriber count transitions 0 → ≥1. Favour the signal — it
  eliminates polling.
  (This adds one small addition to the Phase 1 hub: an
  `active_subscriber_delta` signal. Cheap to add; scoped for this
  phase.)
- On WS message: parse → publish. Example for Kraken ticker:
  ```cpp
  hub.publish("ws:kraken:ticker:" + pair,
              QVariant::fromValue(ticker));
  ```
- Preserve existing signal emission in parallel for one release —
  both the hub and legacy `tickReceived(...)` signal fire. Allows
  consumer-by-consumer migration without a flag day.

### `PolymarketWebSocket` → Producer
- Same shape as ExchangeService, but the WS is per-market.
- `on_topic_idle("polymarket:orderbook:<cid>")` → unsubscribe that
  market's channel; close the WS if zero markets are subscribed.

### Consumer migration
After producers publish to the hub, migrate the visible consumers:

- `CryptoTradingScreen` (`src/screens/crypto_trading/...`) — switch
  ticker and order-book panels from `ExchangeService` signals to
  `hub.subscribe(this, "ws:kraken:orderbook:XBT/USD", ...)`.
- `CryptoWidget` on the dashboard — same, via
  `ws:kraken:ticker:<pair>`.
- `PolymarketScreen` (`src/screens/polymarket/PolymarketScreen.{h,cpp}`)
  and its order-book / chart components — subscribe to the polymarket
  topic families.
- MCP `CryptoTradingTools` module (`src/mcp/tools/`) — use
  `hub.peek()` when an LLM tool asks for "latest price". Lazy: no
  subscription needed for one-shot reads.

Old bespoke signals (`ExchangeService::tickReceived`,
`PolymarketWebSocket::orderBookUpdate`) kept alive for one release,
then deleted in Phase 10.

### Backpressure protection
- Introduce a new `TopicPolicy` field:
  ```cpp
  int coalesce_within_ms = 0;  // 0 = publish every update
  ```
  When non-zero, the hub drops all but the most recent payload for a
  topic within the window, then publishes once. This is the fix for
  Kraken's 100+/sec ticker feed overwhelming a UI thread that can
  only redraw at 60 Hz.
- Apply `coalesce_within_ms = 50` to `ws:kraken:ticker:*` and
  `ws:hyperliquid:ticker:*` by default. Charts can override.

---

## Success check

- Open CryptoTradingScreen and the dashboard CryptoWidget
  simultaneously, both showing BTC/USD. `netstat` / inspect the
  Python WS subprocess: **exactly one** connection to Kraken exists.
- Close both screens; `hub.stats()` shows subscriber_count = 0 on the
  pair. `LOG_INFO("DataHub", "topic_idle ws:kraken:ticker:XBT/USD")`
  is logged, and the WS channel is unsubscribed within ~1 s.
- Re-open CryptoTradingScreen: WS channel reopens; first tick arrives
  in < 500 ms.
- Polymarket: open two different markets in two tabs; confirm WS
  messages flow for both, no cross-market bleed.
- Instrument counter `publish_rate_hz` on ticker topics should show
  ≤ 20 Hz after coalesce applied (baseline without coalescing: 80–120 Hz
  observed on Kraken during volatile hours).
- No duplicate parsing: message body appears in `ExchangeService::on_ws_message`
  exactly once per raw WS message (verified by adding a one-shot
  counter guarded on message timestamp).

---

## Risk

- **Ref-counted WS close logic is bug-prone.** If the delta signal
  fires twice for the same 0 → 1 transition, an extra `subscribe` is
  sent to the exchange; if the 1 → 0 fires before the 0 → 1 for a new
  subscriber, the WS closes then immediately reopens (flapping).
  Mitigation: the hub tracks subscriber count authoritatively; the
  producer only reads it, never maintains its own count. Unit test
  added to `test_datahub.cpp` covers the transition ordering.
- **Backpressure on Kraken feed.** Without `coalesce_within_ms`, 100 Hz
  inbound × N subscribers × UI event loop = stalled UI. Mitigation:
  coalesce field lands in this phase as part of the producer work,
  defaulted on ticker topics.
- **Per-tick allocation cost.** Building `QVariant::fromValue(ticker)`
  at 100 Hz allocates. Use a pre-registered meta-type
  (`Q_DECLARE_METATYPE(Ticker)` — already done in Phase 0) so no
  runtime lookup happens on the hot path.
- **Parallel legacy signals double-fire.** Downside if a consumer
  connects to both the legacy signal and the hub topic: duplicate
  update. Mitigation: consumer audit checklist per migrated screen
  confirms only one path is live.
- **Polymarket reconnection handling.** The existing code has a custom
  reconnect with backoff; ensure that on reconnect, the producer
  re-subscribes to the exchange for every topic with
  `subscriber_count > 0` — do not rely on the hub to trigger it.

---

## Rollback

- Each consumer migration is one commit → revert restores the legacy
  signal path. The hub topic continues to be published in parallel,
  so no reverse migration needed.
- If the producer itself misbehaves, remove the
  `register_producer(this)` call from service init — consumer
  subscriptions become silent (no crash); legacy signals still fire.
- If `coalesce_within_ms` introduces visible tick drops on a chart,
  override policy per-topic to `0` for that chart's feed.
- Irreversible step in this phase: the new `active_subscriber_delta`
  hub signal — additive, low-risk, kept regardless.

---

## Out of scope

- Broker WebSockets (Zerodha Kite, Angel SmartStream, Fyers, etc.) —
  Phase 7.
- Deleting legacy `tickReceived`, `orderBookUpdate` signals — Phase 10.
- Historical OHLC backfill for charts — still goes through
  `MarketDataService::fetch_history` / the new
  `market:history:*` topic family added in Phase 3.
