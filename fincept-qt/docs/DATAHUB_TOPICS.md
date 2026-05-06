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
| `econ:fincept:upcoming_events` | `MacroCalendarService` | 5 min | 60 s | HTTP-backed (`api.fincept.in/macro/upcoming-events?limit=25`). Payload: `QJsonArray` of `{event, country, date, time, importance, actual, forecast, previous}`. Consumed by the dashboard `EconomicCalendarWidget`. |
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
| `geopolitics:events` | `GeopoliticsService` | 2 min | 30 s | Conflict monitor news events (default params). Payload: `EventsPage` (events sorted newest-first + pagination + credits metering). |
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
| `maritime:vessel:<imo>` | `MaritimeService` | 1 min | 30 s | Single vessel position. Payload: `VesselData`. |
| `maritime:vessels:multi` | `MaritimeService` | 1 min | 30 s | Multi-vessel batch (caller supplies IMOs). Payload: `VesselsPage` (vessels + found_count + not_found list + credits metering). User-invoked — hub does not auto-refresh. |
| `maritime:vessels:area` | `MaritimeService` | 1 min | 30 s | Area-search bounding box. Payload: `VesselsPage` (vessels sorted newest-first, total_count, credits metering). User-invoked. |
| `maritime:history:<imo>` | `MaritimeService` | 5 min | 60 s | Vessel route history. Payload: `VesselHistoryPage` (history sorted newest-first + total_records + credits metering). |
| `maritime:health` | `MaritimeService` | 5 min | 60 s | Marine API health check. |

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

### Buyback & burn dashboard (Phase 5)

Terminal-wide topics — same numbers shown to every user, no `<pubkey>` segment. Driven by the Fincept-operated buyback worker (`services/buyback-worker/`) which tallies revenue, executes Jupiter buys, and burns the bought $FNCPT via SPL Token `burn_checked` from the treasury account, then publishes per-epoch summaries to a Fincept HTTP endpoint configured via SecureStorage `fincept.treasury_endpoint`. Until the endpoint is configured, the producers ship a built-in mock payload (each POD carries `is_mock=true`); the dashboard reads "DEMO" in the head pill.

| Pattern | Producer | TTL | Min interval | Notes |
|---|---|---|---|---|
| `treasury:buyback_epoch` | `BuybackBurnService` | 60 s | 30 s | Current epoch summary. Shape: `BuybackEpoch{epoch_no, start/end_ts_ms, revenue_total/subs/predmkt/misc_usd, buyback_usd, staker_yield_usd, treasury_topup_usd, fncpt_bought/burned_raw, fncpt_decimals, avg_buy_price_usd, burn_signature, is_mock}`. The three USD splits (`buyback`, `staker_yield`, `treasury_topup`) implement the plan §5.4 50/25/25 distribution; the worker chooses the actual percentages per epoch. `burn_signature` is base58 — `BuybackBurnPanel` opens it on Solscan. |
| `treasury:burn_total` | `BuybackBurnService` | 5 min | 60 s | All-time totals. Shape: `BurnTotal{total_burned_raw, supply_remaining_raw, decimals, spent_on_buyback_usd, is_mock}`. |
| `treasury:supply_history` | `BuybackBurnService` | 1 h | 5 min | 12-month time-series for the supply chart. Shape: `QVector<SupplyHistoryPoint{ts_ms, total_raw, circulating_raw, burned_raw, decimals}>`. Producer publishes the whole vector on every refresh; subscribers (`SupplyChartPanel`) replace the series wholesale. |
| `treasury:reserves` | `TreasuryService` | 5 min | 60 s | Current SOL + USDC holdings of the treasury multisig. Source: `SolanaRpcClient::get_sol_balance` + `get_token_balance(USDC mint)` against the pubkey in SecureStorage `fincept.treasury_pubkey`. SOL→USD price is peeked from `market:price:token:<wSOL>` so we don't double-fetch. Shape: `TreasuryReserves{pubkey_b58, sol_lamports, usdc_amount, sol_usd_price, total_usd, multisig_label, multisig_url, is_mock}`. |
| `treasury:runway` | `TreasuryService` | 5 min | 60 s | Months of runway at current burn. Computed as `total_usd / monthly_opex_usd`; opex from SecureStorage `fincept.treasury_monthly_opex_usd` (default $100k). Shape: `TreasuryRunway{total_usd, monthly_opex_usd, months, is_mock}`. Re-derived in lock-step with `treasury:reserves`. |

### STAKE / veFNCPT / tier system (Phase 3)

veFNCPT lock surface for the STAKE tab. All four producers ship in **mock mode** until `fincept.lock_program_id` (Anchor program) and `fincept.yield_endpoint` are configured in SecureStorage. Each payload carries `is_mock=true` so panels can surface the state explicitly.

| Pattern | Producer | TTL | Min interval | Notes |
|---|---|---|---|---|
| `wallet:locks:<pubkey>` | `StakingService` | 60 s | 30 s | Vector of `LockPosition{position_id, amount_raw, decimals, lock_start_ts, unlock_ts, duration_secs, weight_raw, lifetime_yield_usdc, is_mock}`. Real path: `getProgramAccounts` against `fincept.lock_program_id`, filter by owner. Mock path: 3 demo positions (2,000 @ 4yr / 1,000 @ 1yr / 500 @ 6mo) per plan §3.2. |
| `wallet:vefncpt:<pubkey>` | `StakingService` | 60 s | 30 s | Aggregate weight + projected next-period yield. Shape: `VeFncptAggregate{pubkey_b58, total_weight_raw, decimals, position_count, projected_next_period_yield_usdc, is_mock}`. Computed by summing `LockPosition.weight_raw` and applying §3.4 25 %-of-revenue staker-share. |
| `wallet:yield:<pubkey>` | `RealYieldService` | 5 min | 60 s | Realised USDC yield. Shape: `YieldSnapshot{pubkey_b58, lifetime_usdc, last_period_usdc, last_period_end_ts, is_mock}`. Real path: `<endpoint>/yield/<pubkey>`. Mock path derives numbers from `treasury:revenue × 25 % / weight share` so demo numbers stay internally consistent with the buyback dashboard. |
| `treasury:revenue` | `RealYieldService` | 1 h | 5 min | Terminal-wide weekly revenue bucket. Shape: `TreasuryRevenue{period_start_ts, period_end_ts, total_usd, is_mock}`. Used by `LockPanel` for "EST. YIELD" before lock-creation; also feeds the Phase 5 dashboard's revenue breakdown. Bucketed weekly to match the buyback worker's epoch cadence. |
| `billing:tier:<pubkey>` | `TierService` | 60 s | 15 s | Derived from `wallet:vefncpt:<pubkey>`; service subscribes to vefncpt internally and republishes whenever weight changes. Shape: `TierStatus{pubkey_b58, tier (Free/Bronze/Silver/Gold), weight_raw, next_threshold_raw, decimals, is_mock}`. Thresholds in `services/billing/TierConfig.h` (100 / 1k / 10k veFNCPT). Drives cross-screen gating (AI Quant Lab, Alpha Arena) via the `tier_changed` Qt signal. |

### Internal prediction markets (Phase 4)

Reserved topic family for the `FinceptInternalAdapter` matching engine. Topics are policy-registered at adapter startup, but **no producer publishes to them yet**: the adapter ships in **demo mode** (curated 3-market dataset emitted via Qt signals only) until `fincept.markets_endpoint` is configured *and* the `fincept_market` Anchor program (`solana/programs/fincept_market/`, separate repo) is deployed.

| Pattern | Producer | TTL | Min interval | Notes |
|---|---|---|---|---|
| `prediction:fincept:markets` | `FinceptInternalAdapter` (planned) | 30 s | 10 s | Curated/live market list. Shape: `QVector<PredictionMarket>` (same shape as `prediction:polymarket:markets`). Fields: `key`, `question`, `category`, `volume`, `outcomes` (binary YES/NO with prices in [0,1]), `end_date_iso`. Demo dataset emitted via `markets_ready` signal; topic publishing waits on the matching engine. |
| `prediction:fincept:orderbook:<asset_id>` | `FinceptInternalAdapter` (planned) | 5 s | 1 s | Per-asset order book. Shape: `PredictionOrderBook{asset_id, bids[], asks[]}` matching the Polymarket/Kalshi shape. WebSocket-driven once live. |
| `prediction:fincept:price:<asset_id>` | `FinceptInternalAdapter` (planned) | 5 s | 1 s | Last-trade price scalar. Same shape as `prediction:polymarket:price:*`. |

## F&O / Options (Phase 11 — Sensibull-style tab)

The F&O screen owns its own producer family. `OptionChainService` is the sole hub registrant for `option:*` and the derived `fno:pcr:*` / `fno:max_pain:*` topics. Phase 1 ships polled REST refresh; WebSocket OI fan-out (Phase 3 of the F&O work) republishes through the same topics so consumers don't change.

### Chain & per-leg streams

| Pattern | Producer | TTL | Min interval | Notes |
|---|---|---|---|---|
| `option:chain:<broker>:<underlying>:<expiry>` | `OptionChainService` | 5 s | 3 s | Coalesce 250 ms. `pause_when_inactive=true`. Payload: `OptionChain` (rows[] sorted by strike asc, spot, ATM, PCR, max_pain, total OI). Producer batches CE+PE+underlying quotes via `IBroker::get_quotes` then assembles. Chain refresh runs on a worker thread to avoid blocking the UI. |
| `option:tick:<broker>:<token>` | `DataStreamManager` (Phase 3) | push-only | — | Per-leg LTP + OI updates from broker WebSocket. Coalesce 100 ms. Reserved — Phase 1 of the F&O work does not publish here yet; consumers should subscribe to `option:chain:*` for snapshot + signal. |
| `option:atm_iv:<broker>:<underlying>` | `OptionChainService` | 5 s | 3 s | ATM implied volatility scalar (decimal, 0.142 = 14.2%). Computed locally via py_vollib in Phase 3; stubbed at 0 in Phase 1 until the Greeks worker lands. |

### Derived analytics

| Pattern | Producer | TTL | Min interval | Notes |
|---|---|---|---|---|
| `fno:pcr:<broker>:<underlying>:<expiry>` | `OptionChainService` | push-only | — | Put/Call Ratio = sum(PE OI) / sum(CE OI). Republished on every chain publish (coalesce 250 ms). Payload: `double`. |
| `fno:max_pain:<broker>:<underlying>:<expiry>` | `OptionChainService` | push-only | — | Strike minimising total option-writer pain at expiry. Payload: `double`. |
| `fno:fii_dii:daily` | `FiiDiiService` (Phase 8 of F&O work) | 1 h | 30 min | Daily institutional flows scraped from NSE. Refreshed once per session post 6 PM IST. Payload: `QVector<FiiDiiDay>`. Reserved — not yet implemented. |
| `oi:history:<broker>:<token>:<window>` | `OISnapshotter` (Phase 3 of F&O work) | 60 s | 30 s | Intraday OI series (rolling minute snapshots) for the OI Analytics sub-tab. `<window>` = `1d` / `5d`. Reserved — not yet implemented. |

> **Underlying spot:** the chain producer always re-fetches the underlying quote alongside the option quotes in the same `get_quotes` batch, so subscribers don't need to cross-subscribe to `market:quote:<sym>`. Index symbols use `NSE_INDEX:<NAME>` (NIFTY/BANKNIFTY/FINNIFTY/MIDCPNIFTY); stocks use `NSE:<SYM>`.

> **Broker requirement:** F&O topics require a connected, instruments-loaded broker. The producer publishes `publish_error("no instruments cached for …")` when the InstrumentService cache is empty; consumers should surface a "connect a broker" prompt when this happens.

## Force refresh

`DataHub::request(topic, force=true)` bypasses `min_interval_ms` (so user-driven refresh buttons work inside the interval gate). Per-producer `max_requests_per_sec()` is still honoured — rage-clicking cannot hammer upstream.
