# DataHub guide for agents / LLM tool callers

Phase 9 of the DataHub migration exposes the terminal's in-process pub/sub layer directly to LLM tool callers through four generic MCP tools. Everything streaming into any active widget — quotes, order books, news, vessel tracks, broker ticks, geopolitical events, agent outputs, LLM token streams — is observable from an agent via hub topic names.

This guide explains what an agent can do and when to use which tool. The full topic catalogue lives in [`docs/DATAHUB_TOPICS.md`](../DATAHUB_TOPICS.md).

## The four tools

### `datahub_list_topics`

Returns every currently-active topic along with `subscriber_count`, `total_publishes`, `push_only`, and `age_ms` (time since last publish). No arguments.

Use this first when you don't know the topic shape yet. It's also the cheapest way to check whether a data stream is actually flowing — a topic with `publishes > 0` and small `age_ms` is live.

### `datahub_peek`

```json
{ "topic": "market:quote:AAPL" }
```

Returns `{topic, value, age_ms}` — the current cached value without triggering a refresh. `age_ms = -1` means the topic is unknown or has never published. **Check `age_ms` before trusting the value** — stale cached data (say, a market:quote from yesterday) is still served by peek.

Peek is read-only and respects no rate limits. Use it whenever you want the last-known value without paying any producer cost.

### `datahub_request`

```json
{ "topic": "market:quote:AAPL", "force": false }
```

Asks the hub to refresh the topic now. Returns immediately — the refresh is async. Subscribers get the fresh value via the usual publish path; you'd need a subsequent `datahub_peek` or `datahub_subscribe_briefly` to see it.

`force: true` bypasses the topic's `min_interval_ms` rate gate. Per-producer `max_requests_per_sec` is still enforced — an agent can't hammer a rate-limited broker REST API by rage-calling this tool.

### `datahub_subscribe_briefly`

```json
{ "topic": "ws:kraken:ticker:BTC-USD", "duration_ms": 5000 }
```

Subscribes a disposable listener for `duration_ms` (clamped to `[100, 30000]` ms) and returns every value delivered during the window as an array of `{t_ms, value}` entries.

This is the key tool for reasoning about volatile topics. A single `peek` of `market:quote:AAPL` tells you the last tick. A 5-second `subscribe_briefly` tells you the tick rate, the short-term drift, and whether the feed is alive. Use it when you need to *observe behaviour over time*, not just a snapshot.

## Picking the right tool

| Question | Tool |
|---|---|
| "What topics exist?" | `datahub_list_topics` |
| "What's the latest value of X?" | `datahub_peek` |
| "Force a fresh read of X." | `datahub_request` (then `datahub_peek` a moment later) |
| "What is topic X doing *right now*?" | `datahub_subscribe_briefly` |
| "Is this feed alive or stalled?" | `datahub_list_topics` (check `age_ms`) or `datahub_subscribe_briefly` |

## Topic discovery patterns

Topic names follow a consistent shape: `<family>:<sub>:<id>[:<qualifier>]`. Common families:

- `market:quote:<sym>`, `market:sparkline:<sym>`, `market:history:<sym>:<period>:<interval>`
- `ws:kraken:ticker:<pair>`, `ws:hyperliquid:trades:<coin>`
- `news:general`, `news:symbol:<sym>`, `news:category:<cat>`, `news:cluster:<id>`
- `economics:<source>:<indicator>`, `dbnomics:<provider>:<dataset>`, `govdata:<agency>:<series>`
- `broker:<broker_id>:<account_id>:<channel>` (positions, orders, balance, holdings, quote, ticks)
- `polymarket:<channel>`
- `geopolitics:event:<type>`, `geopolitics:country:<name>`, `hdx:<dataset_id>`, `trade:<kind>`
- `maritime:vessel:<imo>`, `maritime:history:<imo>`
- `ma:<context>` (DCF, merger_model, LBO, etc.)
- `agent:output:<run_id>`, `agent:stream:<run_id>`, `agent:status:<run_id>`
- `llm:session:<session_id>:stream`

When unsure, start with `datahub_list_topics` — the live catalogue is always authoritative.

## Behavioural guarantees

**Cache trustworthiness.** Peeked values are the most recent publish. Check `age_ms`. Pull-through topics auto-refresh on access when stale; push-only topics (most `agent:*`, most `ma:*`, broker WebSocket tick channels) only update when their producer pushes.

**Disposable topics.** Some topics (especially `agent:output:<run_id>` and `llm:session:<id>:stream`) are created per-run and retired on completion. Their cached state disappears after retire_topic; subsequent peeks return unknown. This is intentional — the terminal would otherwise grow unbounded memory over a long session with many agent runs.

**Coalescing.** High-rate topics (token streams, tick firehoses) are coalesced at the policy layer — `subscribe_briefly` will see at most one value per coalesce window regardless of how fast the producer emits.

**Thread safety.** All tools are safe to call from the MCP handler thread. `datahub_subscribe_briefly` spins a local event loop bounded by `duration_ms`, so it never blocks the UI thread.

## Worked examples

### "Is the news feed live and what's the latest headline?"

1. `datahub_list_topics` → confirm `news:general` has recent `publishes` and small `age_ms`.
2. `datahub_peek({ topic: "news:general" })` → extract the most-recent article from the value.

### "Watch the BTC-USD order book for 5 seconds and summarise the depth."

```json
datahub_subscribe_briefly({
  "topic": "ws:kraken:book:BTC-USD",
  "duration_ms": 5000
})
```
Summarise the returned `samples` array — count of updates, mid-price drift, book-side imbalance.

### "Tail a running agent's output."

If the user kicked off an agent run and returned a `request_id`:

```json
datahub_subscribe_briefly({
  "topic": "agent:stream:<run_id>",
  "duration_ms": 10000
})
```

You'll receive every token published in that window. For the final result, peek `agent:output:<run_id>` *before* the run completes — after completion the topic is retired.

### "Force a fresh quote for MSFT and read it."

```json
datahub_request({ "topic": "market:quote:MSFT", "force": true })
// wait briefly, then:
datahub_peek({ "topic": "market:quote:MSFT" })
```

The producer rate-limit still applies — `max_requests_per_sec` caps how often the underlying Python script can be invoked regardless of how aggressively you poll `datahub_request`.

## Relation to existing MCP tools

The terminal exposes ~24 domain-specific MCP tool modules (markets, news, portfolio, forum, notes, etc.). Those still exist and are the right tool for write operations (`save_config`, `add_news_monitor`, `execute_trade`), for one-shot lookups that don't flow through the hub, and for actions that need to drive UI navigation.

Use the DataHub tools for:

- Observing *currently flowing* data streams without triggering a full REST fetch.
- Discovering what's live without knowing the exact topic shape ahead of time.
- Sampling a time window of a volatile topic.
- Tailing agent runs and LLM streams.

Use domain MCP tools for:

- Mutation (create, update, delete).
- Operations that must trigger UI changes (navigation, monitor registration).
- Rich filtered queries where the hub cache is only partial.
