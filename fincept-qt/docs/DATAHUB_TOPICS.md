# DataHub Topic Registry

Canonical list of every hub topic family, its owning producer, TTL, and refresh policy. Finalised in Phase 10 — current contents are the working registry as producers land phase-by-phase.

Topic segments separate with `:`. The first segment is the domain; subsequent segments are domain-specific keys (symbol, provider, series id, …). Wildcard `*` matches a single segment.

## Market data (Phase 2 / 3)

| Pattern | Producer | TTL | Min interval | Notes |
|---|---|---|---|---|
| `market:quote:<sym>` | `MarketDataService` | 5 s | 1 s | Single symbol quote |
| `market:sparkline:<sym>` | `MarketDataService` | 60 s | 5 s | Intraday sparkline points |
| `market:history:<sym>:<period>:<interval>` | `MarketDataService` | 300 s | 30 s | Bar history |

## WebSocket streams (Phase 4)

Producer changed to `ExchangeSessionManager` with the multi-broker refactor
(Phase 2). Sessions (one per exchange, kept warm for the app lifetime) hand
fan-out data to the manager via `SessionPublisher`; the manager is the sole
hub registrant for `ws:<exchange>:*`.

| Pattern | Producer | TTL | Notes |
|---|---|---|---|
| `ws:kraken:ticker:<pair>` | `ExchangeSessionManager` | push-only | Coalesced 50 ms |
| `ws:kraken:orderbook:<pair>` | `ExchangeSessionManager` | push-only | |
| `ws:kraken:trades:<pair>` | `ExchangeSessionManager` | push-only | |
| `ws:kraken:ohlc:<pair>:<interval>` | `ExchangeSessionManager` | push-only | |
| `ws:hyperliquid:*` | `ExchangeSessionManager` | push-only | Same sub-families as Kraken |
| `prediction:polymarket:price:<asset_id>` | `PolymarketWebSocket` | push-only | Was `polymarket:price:*` before the prediction-markets refactor |
| `prediction:polymarket:orderbook:<asset_id>` | `PolymarketWebSocket` | push-only | Was `polymarket:orderbook:*` before the prediction-markets refactor |
| `prediction:kalshi:price:<ticker>:<side>` | `KalshiWsClient` | push-only | `<side>` is `yes` or `no`. Requires credentials — streaming activates in Phase 7 |
| `prediction:kalshi:orderbook:<ticker>:<side>` | `KalshiWsClient` | push-only | Kalshi REST returns yes+no bids only; asks synthesised client-side |

## News (Phase 5)

| Pattern | Producer | TTL | Min interval | Notes |
|---|---|---|---|---|
| `news:general` | `NewsService` | 5 min | 30 s | Coalesced 250 ms (progressive publish) |
| `news:symbol:<sym>` | `NewsService` | 5 min | 30 s | Filtered slice of news:general |
| `news:category:<cat>` | `NewsService` | 5 min | 30 s | Category strings from NewsArticle |
| `news:cluster:<id>` | `NewsService` | push-only | — | Server-assigned clusters |

## Economics (Phase 6)

| Pattern | Producer | TTL | Min interval | Notes |
|---|---|---|---|---|
| `econ:<source>:<request_id>` | `EconomicsService` | 1 h | 60 s | `source` = fred, worldbank, imf, oecd, etc. |
| `dbnomics:<provider>:<dataset>:<series>` | `DBnomicsService` | 1 h | 60 s | Observations endpoint only |
| `govdata:<provider>:<request_id>` | `GovDataService` | 1 h | 60 s | Gov open-data APIs |

## Broker account streams (Phase 7)

Topic shape: `broker:<broker_id>:<account_id>:<channel>[:<sym>]`. When a
caller has no explicit account id, it passes `default` — single-account
and multi-account code paths share one format. Build topics via
`fincept::trading::broker_topic()` in `src/trading/BrokerTopic.h`.

| Pattern | Producer | TTL | Min interval | Notes |
|---|---|---|---|---|
| `broker:*:*:positions` | `DataStreamManager` | 5 s | 3 s | Open positions for one account |
| `broker:*:*:orders` | `DataStreamManager` | 5 s | 3 s | Live order book for one account |
| `broker:*:*:balance` | `DataStreamManager` | 30 s | 10 s | Cash + margin (`BrokerFunds`) |
| `broker:*:*:holdings` | `DataStreamManager` | 30 s | 10 s | Long-term holdings |
| `broker:*:*:quote:<sym>` | `DataStreamManager` | 5 s | 1 s | Per-symbol quote snapshot |
| `broker:*:*:ticks:<sym>` | `DataStreamManager` | push-only | — | Coalesce 100 ms; broker WS feed |

`DataStreamManager` is currently the sole broker producer; it dual-fires
hub publishes alongside the existing per-account signals. Per-broker
tick feeds migrate in follow-up PRs (one broker per PR per the Phase 7
plan's risk-mitigation cadence).

## Geopolitics / Maritime / Corporate (Phase 8)

### Geopolitics

| Pattern | Producer | TTL | Min interval | Notes |
|---|---|---|---|---|
| `geopolitics:events` | `GeopoliticsService` | 2 min | 30 s | Conflict monitor news events (default params) |
| `geopolitics:countries` | `GeopoliticsService` | 10 min | 60 s | Unique country list w/ event counts |
| `geopolitics:categories` | `GeopoliticsService` | 10 min | 60 s | Unique event category list |
| `geopolitics:cities` | `GeopoliticsService` | 10 min | 60 s | Cities with extracted coordinates |
| `geopolitics:hdx:<context>` | `GeopoliticsService` | 1 h | 60 s | `<context>` = conflicts, humanitarian, country:<iso>, topic:<slug>, search:<q> |
| `geopolitics:trade:<kind>` | `GeopoliticsService` | 15 min (push-only) | — | `<kind>` = benefits, restrictions |
| `geopolitics:geolocation` | `GeopoliticsService` | 15 min (push-only) | — | Extracted coords from headline batch |
| `geopolitics:relationship_graph:<ticker>` | `RelationshipMapService` | 10 min | 2 min | yfinance-backed corporate relationship snapshot |

### Maritime

| Pattern | Producer | TTL | Min interval | Notes |
|---|---|---|---|---|
| `maritime:vessel:<imo>` | `MaritimeService` | 1 min | 30 s | Single vessel position |
| `maritime:vessels:multi` | `MaritimeService` | 1 min | 30 s | Batch vessel snapshot (default preset IMO list) |
| `maritime:history:<imo>` | `MaritimeService` | 5 min | 60 s | Vessel route history |
| `maritime:health` | `MaritimeService` | 5 min | 60 s | Marine API health check |

### M&A Analytics

| Pattern | Producer | TTL | Min interval | Notes |
|---|---|---|---|---|
| `ma:<context>` | `MAAnalyticsService` | 2 min (push-only) | — | `<context>` follows the service method (e.g. `ma:dcf`, `ma:merger_model`, `ma:lbo_returns`). All analytics take caller-supplied params — hub cannot re-run them, so topics are push-only. Callers must drive refresh through the existing method API. |

## AI / Agents / LLM (Phase 9)

Push-only topic families published by `AgentService` and `LlmService`. These topics are **per-run disposable** — the producer calls `DataHub::retire_topic(...)` on completion so cached state is released back to the hub. Subscribers attached via `subscribe(owner, ...)` remain attached across retirement and will receive the next publish normally.

### Agent execution

| Pattern | Producer | TTL | Min interval | Notes |
|---|---|---|---|---|
| `agent:output:<run_id>` | `AgentService` | 10 min (push-only) | — | Final result payload for a run. Retired on completion. Shape: `{request_id, success, response, error, execution_time_ms, final}`. |
| `agent:stream:<run_id>` | `AgentService` | 5 min (push-only, coalesce 50 ms) | — | Token firehose from streaming runs. Shape: `{request_id, token}`. |
| `agent:status:<run_id>` | `AgentService` | 5 min (push-only, coalesce 100 ms) | — | Thinking/tool-call narration. Shape: `{request_id, status}`. |
| `agent:routing:<run_id>` | `AgentService` | 10 min (push-only) | — | One-shot routing decision. Shape: `{request_id, success, agent_id, intent, confidence}`. |
| `agent:error:<context>` | `AgentService` | 2 min (push-only) | — | Error stream keyed by context (discover_agents, create_plan, etc.). Shape: `{context, message}`. |

### LLM session stream

| Pattern | Producer | TTL | Min interval | Notes |
|---|---|---|---|---|
| `llm:session:<session_id>:stream` | `LlmService` | 5 min (push-only, coalesce 50 ms) | — | Shadow publish of every streaming chunk from `chat_streaming()`. Shape: `{session_id, chunk, done}`. Session id is generated per-call. Topic retired on `done=true`. |

### Generic DataHub MCP tools

The MCP module `DataHubTools` exposes four generic introspection tools to any LLM tool caller (see `docs/agents/datahub-guide.md`):

- `datahub_list_topics` — every active topic + subscriber count + last-publish age
- `datahub_peek` — current cached value for a topic (`{value, age_ms}`) without triggering refresh
- `datahub_request` — ask the hub to refresh a topic (subject to policy + `force` flag)
- `datahub_subscribe_briefly` — collect all values published on a topic for a bounded duration (100-30000 ms)

## Crypto / on-chain (Phase 10)

The Crypto Center owns a small set of topics for the user's connected Solana wallet. Read-only data flows through the hub. Transaction-signing paths (Phase 2 swap) **do not** go through the hub — they're one-shot user actions handled by `WalletService` + `WalletTxBridge` directly.

### Wallet balances

| Pattern | Producer | TTL | Min interval | Notes |
|---|---|---|---|---|
| `wallet:balance:<pubkey>` | `WalletBalanceProducer` | 30 s (poll) / push-only (stream) | 10 s (poll) / — (stream) | Two refresh modes, switched at runtime via SecureStorage `wallet.balance_mode` (`poll` / `stream`). **Phase 2 §2A.5 carries every SPL token the wallet holds**, not just $FNCPT. Polling uses Solana RPC `getBalance` + `getTokenAccountsByOwner` (programId-filtered, no mint filter) + `TokenMetadataService::lookup` for symbol/name/icon. Streaming opens a per-pubkey WebSocket to the RPC `wss://` endpoint and `accountSubscribe`s on the wallet pubkey for SOL changes; non-SOL tokens are bootstrapped via REST and re-seeded on a 30 s heartbeat. Stream policy: `push_only=true`, `coalesce_within_ms=250`. Shape: `WalletBalance{pubkey_b58, sol_lamports, tokens: QVector<TokenHolding{mint, symbol, name, amount_raw, decimals, verified, icon_url}>, ts_ms}`. Legacy back-compat accessors `fncpt_holding()` / `fncpt_ui()` / `fncpt_decimals()` are exposed on `WalletBalance` for callers that still want FNCPT specifically. Endpoint priority: `solana.rpc_url` SecureStorage override → Helius (`solana.helius_api_key`) → public mainnet RPC (`https`/`wss` derived). |

### Token price

| Pattern | Producer | TTL | Min interval | Notes |
|---|---|---|---|---|
| `market:price:token:<mint>` | `TokenPriceProducer` | 60 s | 30 s | Jupiter Lite Price API (`lite-api.jup.ag/price/v3`). One batched call per refresh covers every mint that has at least one subscriber: `?ids=<m1>,<m2>,…`. Coalesce 250 ms across rapid subscribe events (panel re-subscribes on every balance update). `<mint>` accepts wrapped-SOL (`So111…1112`) for native SOL pricing. Shape: `TokenPrice{mint, usd, sol, ts_ms, valid}`. The `sol` field is FNCPT-priced-in-SOL when the SOL mint is part of the same batch; 0 otherwise. |
| `market:price:fncpt` | `TokenPriceProducer` | 60 s | 30 s | **Deprecated alias** for `market:price:token:9LUqJ5aQTjQiUCL93gi33LZcscUoSBJNhVCYpPzEpump`. Kept for one phase so existing subscribers (Phase 1's HoldingsBar, BalancePanel, SwapPanel) keep working without churn. Removed in Phase 3 cleanup; new subscribers should use `market:price:token:<mint>` directly. Shape: `TokenPrice` (legacy `FncptPrice` is a typedef alias). |

### Wallet activity (Phase 2)

| Pattern | Producer | TTL | Min interval | Notes |
|---|---|---|---|---|
| `wallet:activity:<pubkey>` | `WalletActivityProducer` | 30 s | 10 s | Last 50 parsed wallet operations. Helius parsed-transactions endpoint (`api.helius.xyz/v0/addresses/<pk>/transactions?limit=50`) when the user has a Helius key in `solana.helius_api_key`; falls back to raw `getSignaturesForAddress` (signatures only) when no key is configured. Shape: `vector<ParsedActivity{ts, kind, asset, amount, signature, status}>` where `kind` ∈ `{SWAP, RECEIVE, SEND, OTHER}`. |

### Fee discount eligibility (Phase 2)

| Pattern | Producer | TTL | Min interval | Notes |
|---|---|---|---|---|
| `billing:fncpt_discount:<pubkey>` | `FeeDiscountService` | derived from `wallet:balance:<pubkey>` (no separate fetch) | — | The service subscribes to the user's balance topic internally and republishes eligibility. Shape: `FncptDiscount{eligible, threshold_raw, threshold_decimals, applied_skus}`. Threshold + applied SKUs come from `services/billing/FeeDiscountConfig.h`; defaults to **1,000 $FNCPT → 30 % off** for AI reports, deep backtests, premium screens. |

> **Phase 2 swap path is not a hub topic.** `PumpFunSwapService::build_swap()` is a one-shot, user-initiated HTTP POST to `pumpportal.fun/api/trade-local` that returns an unsigned versioned-tx body. There's no debounce-coalesce or cache-coherence value to pushing it through DataHub; the result is fed directly into `WalletService::sign_and_send()`. See `plans/crypto-center-phase-2.md` D1.

> **Burn deferred to Phase 5.** Phase 2 ships BUY and SELL only. Real burns (with on-chain receipt) move into Phase 5 alongside the buyback-worker design. No `wallet:burn_receipt:*` topic in Phase 2.

## Force refresh

`DataHub::request(topic, force=true)` bypasses `min_interval_ms` (so user-driven refresh buttons work inside the interval gate). Per-producer `max_requests_per_sec()` is still honoured — rage-clicking cannot hammer upstream.
