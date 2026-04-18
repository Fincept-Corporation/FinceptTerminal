# Phase 9 — AI / MCP / Agents Integration

**Status:** Not started
**Depends on:** Phases 3, 4, 5 (and ideally 6, 7, 8 — agents benefit
from every migrated producer being on the hub, but only 3/4/5 are
structurally required).
**Nominal duration:** 1 week
**Rough size:** Medium (24 MCP modules lightly touched, 1 new MCP
module, agent helper added)

---

## Goal

Let the AI surface — MCP tools, agent frameworks, LLM chat — read
hub topics the same way screens do. Agents stop re-fetching data
that a visible screen already has streaming; LLM tool calls become
cheap peeks instead of kicking off a Python subprocess every time
"what's AAPL's price" is asked.

Critically: **agents are subscribers, not producers.** They don't
publish their own topics (except a narrow set of agent-output
topics documented below). This is a one-directional integration —
reading from the hub, into the agent world.

---

## Targets

### MCP tool modules — 24 modules

Under `src/mcp/tools/`. Relevant modules per grep of `MarketDataService`,
`NewsService`, `EconomicsService`, etc. usage:

- `MarketDataTools` — currently calls `MarketDataService::fetch_quotes`
  directly. Switch to `DataHub::peek("market:quote:<sym>")` first
  (cheap hit if a screen already has it), fall back to
  `DataHub::request()` + subscribe-once + unsubscribe.
- `CryptoTradingTools` — `hub.peek("ws:kraken:ticker:XBT/USD")`.
- `NewsTools` — `hub.peek("news:general")` /
  `hub.peek("news:symbol:<sym>")`.
- `EconomicsTools` — `hub.peek("econ:fred:<series>")`.
- `PortfolioTools` — `hub.peek("broker:<id>:<acct>:positions")`.
- `GeopoliticsTools`, `MaritimeTools`, `MATools`, `GovDataTools`,
  `RelationshipMapTools` — peek the topic families from Phase 8.
- Remaining 14 modules: no change (they don't consume data, they
  operate on files/config/terminal state).

### New MCP module — `DataHubTools`

A generic introspection surface for LLMs:

- `datahub_list_topics` — returns `hub.stats()` formatted as JSON.
  Lets the LLM "see" what data is currently flowing and reason
  about it.
- `datahub_peek(topic)` — reads any topic's current value,
  serialised via the meta-type system. Bounded response size.
- `datahub_request(topic, force=false)` — kicks off a refresh if no
  subscriber exists; consumes a policy-gated slot.
- `datahub_subscribe_briefly(topic, duration_ms)` — subscribes a
  short-lived listener, accumulates values, returns the stream when
  the timer fires. Useful for "watch this ticker for 10 seconds and
  tell me the average".

File: `src/mcp/tools/DataHubTools.{h,cpp}`.

### Agent framework integration

Under `src/services/agents/`. Add `AgentContext::peek(topic)` and
`AgentContext::request(topic)` wrappers around `hub.peek/request`.

Agent code today:
```python
quotes = fincept.market_data.get_quotes(["AAPL", "MSFT"])
```
Migrates to:
```python
aapl = context.peek("market:quote:AAPL")
msft = context.peek("market:quote:MSFT")
if aapl is None: context.request("market:quote:AAPL")
```

The Python bridge (already in `src/python/`) needs a new tiny bridge
function `datahub_peek(topic)` that marshals to C++ and back. Agents
run in background threads; `peek` is thread-safe (Phase 1 guarantee).

### Agent output topics (narrow, opt-in)

A small set of agent-produced topics, so the UI can subscribe to
agent analyses the same way it subscribes to market data:

- `agent:<agent_id>:status` — running / idle / error.
- `agent:<agent_id>:output:<run_id>` — final output of a run.
- `agent:<agent_id>:stream:<run_id>` — streamed intermediate tokens
  (push-only, `coalesce_within_ms = 100`).

The AgentService (already exists) gains Producer implementation for
these three families. Consumers: AI Chat UI, Agent Config panels,
Alpha Arena leaderboard.

### LLM chat integration

`LlmService` / AI Chat screen currently streams LLM responses via
its own signal flow. Preserve that — it's bespoke and works. But:
add a hub-published topic `llm:session:<id>:stream` for other
screens that want to observe an ongoing LLM conversation (e.g.
AgentConfig showing an agent's internal reasoning in real time).

---

## Deliverables

- `DataHubTools` MCP module implemented and registered in the MCP
  server's tool catalog.
- Existing MCP tool modules (the ~10 listed above) modified: their
  service-fetch calls replaced with `hub.peek()` / `hub.request()`,
  preserving the tool API surface as seen by the LLM.
- `AgentContext::peek/request` helpers + Python bridge.
- `AgentService` implements Producer for `agent:*` topics.
- AI Chat: add the `llm:session:<id>:stream` publish path; screen
  stream rendering stays as-is.
- `docs/agents/datahub-guide.md` (new short doc) — one page
  explaining how agents should use the hub, with a code example.
  Referenced from the Python contributor guide.

---

## Success check

- An LLM tool-use: "What is the current price of AAPL?" — the
  `MarketDataTools::get_quote("AAPL")` call hits `hub.peek` first,
  returns in < 5 ms if a screen has AAPL visible. Only falls back
  to Python if the cache is cold.
- Open AI Chat and run an agent; AgentConfig panel (separate
  screen) subscribes to `agent:<id>:stream:<run>` and shows the
  live token stream. No bespoke wiring between the screens.
- `datahub_list_topics` MCP call: LLM receives a JSON structure
  listing every currently-active topic with subscriber counts.
  Useful for debug and for LLMs to reason about what "the
  terminal is currently showing".
- An Agno Trading agent running in the background subscribes to
  `market:quote:AAPL` for a 30-second window; when a screen is
  viewing AAPL, the agent's data comes entirely from the hub's
  cache — no extra Python spawn.
- `git grep -l MarketDataService::instance src/mcp/` returns zero
  (all replaced with hub calls).

---

## Risk

- **Background-thread safety.** Agents run on Python subprocesses;
  the Python → C++ bridge for `datahub_peek` must hop into the Qt
  main thread (or use the hub's internal mutex — hub is
  thread-safe via the Phase 1 guarantee). Mitigation: bridge
  function uses a blocking QMetaObject::invokeMethod with
  `Qt::BlockingQueuedConnection` from the Python worker; fast
  because peek is a hash lookup. **Do not** use
  `Qt::BlockingQueuedConnection` between threads that may call
  into each other — verify the call graph is one-way.
- **`datahub_subscribe_briefly` resource leak.** If the duration
  timer never fires (e.g. hub shutting down), the subscriber
  leaks. Mitigation: bind the owner to an auto-deleting QObject
  that destroys itself on timer expiry, and register a cleanup
  hook on hub teardown.
- **LLM reasoning on stale cache.** `peek` returns whatever is
  cached; if the cache is 30 s old (within TTL), the LLM gets
  stale data without realising. Mitigation: `datahub_peek`
  response includes `age_ms` alongside the value; LLM-facing tool
  description tells the model to consider freshness.
- **Agent output topic explosion.** If a trading agent publishes
  `agent:<id>:output:<run>` per trade and runs 1000 simulated
  trades, the hub accumulates 1000 retired topics. Mitigation: the
  hub gets a `retire_topic(topic)` API (small additive change)
  that the AgentService calls on run completion; the hub drops
  the topic state entirely.
- **Bridge API surface churn.** Adding `datahub_peek` / `request`
  to the Python bridge expands the public Python API for agent
  authors. Mitigation: ship as experimental (`fincept.datahub.*`
  namespace), mark stable after one release.

---

## Rollback

- Revert per-MCP-module changes — tools fall back to direct
  service calls, functionally identical.
- `DataHubTools` module removal is a one-line deregistration;
  LLM tool catalog shrinks, no caller breaks.
- `AgentService` Producer un-register falls back to its existing
  signal path; AgentConfig / AI Chat reconnect to the signal.
- Agent Python helpers are additive; no agent is forced to use
  them yet. Existing agents keep working.

---

## Out of scope

- Rewriting the LLM chat stream engine — out of scope; only
  publishing a shadow topic.
- New agent frameworks beyond the existing ones.
- Changing MCP tool schemas visible to external MCP clients —
  tool names and argument shapes preserved; only the
  implementation changes.
- Agent sandboxing / security review — separate workstream.
- Legacy API deletion — Phase 10.
