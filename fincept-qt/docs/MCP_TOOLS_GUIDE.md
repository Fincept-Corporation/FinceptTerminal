# MCP Tools — Author & Maintainer Guide

This guide documents how the Model Context Protocol (MCP) tool system works
inside Fincept Terminal after the 6-phase refactor (see
`fincept-qt/plans/mcp-refactor-INDEX.md`).

If you are adding a new tool, jump to **§5 Adding a new tool**. If you are
debugging why an LLM call isn't hitting your tool, jump to **§6 Diagnosis**.

---

## 1. Architecture

```
                     ┌──────────────────────────────────────────────┐
                     │              LlmService                      │
                     │  (HTTP / streaming — provider-agnostic)      │
                     └─────────┬────────────────────────────────────┘
                               │  via ToolDispatcher (Phase 5)
                               ▼
                     ┌──────────────────────────────────────────────┐
                     │            ToolDispatcher                    │
                     │  multi-round loop, parallel tool fan-out     │
                     └─────────┬────────────────────────────────────┘
                               │
                               ▼
              ┌────────────────────────────────────┐
              │  McpService::execute_*_async       │   ← unified entry point
              │     ↓                              │
              │  internal: McpProvider             │   ← src/mcp/tools/*.cpp
              │  external: McpManager → McpClient  │   ← JSON-RPC over stdio
              └────────────────────────────────────┘
```

Key files:
- `src/mcp/McpTypes.h` — `ToolDef`, `ToolSchema`, `ToolHandler`, `AsyncToolHandler`, `ToolContext`, `AuthLevel`, `ToolFilter`
- `src/mcp/McpProvider.{h,cpp}` — internal tool registry + sync/async dispatch + auth gate
- `src/mcp/McpService.{h,cpp}` — unified facade (internal + external)
- `src/mcp/SchemaValidator.{h,cpp}` — runs on every call before the handler
- `src/mcp/ToolSchemaBuilder.h` — fluent builder for schemas
- `src/mcp/AsyncDispatch.h` — bridges callback-based services into QPromise
- `src/mcp/dispatch/*` — provider adapters and the dispatcher

---

## 2. Schema declaration (Phase 3)

Every tool must declare its inputs. Two shapes are supported.

### Preferred — `ToolSchemaBuilder`
```cpp
#include "mcp/ToolSchemaBuilder.h"

t.input_schema = ToolSchemaBuilder()
    .string("symbol", "Ticker (AAPL, BTC-USD, etc.)").required().pattern("^[A-Z0-9._-]{1,16}$")
    .string("side", "Order side").required().enums({"buy","sell"})
    .integer("limit", "Max items").default_int(20).between(1, 100)
    .boolean("force", "Bypass cache").default_bool(false)
    .build();
```

### Legacy — raw JSON Schema fragment
Still works; the validator handles both shapes. Migrate when adding constraints
(enums, ranges, patterns) since those are tedious to write inline.

### Validation runs automatically
`McpProvider::call_tool_async` calls `validate_args` *before* the handler.
Defaults are injected, types checked, enums enforced, regex patterns matched.
Handlers receive a normalised `QJsonObject` and don't need defensive `.toString("default")` calls.

---

## 3. Sync vs async handlers (Phase 4)

| Use sync `t.handler` for | Use async `t.async_handler` for |
|---|---|
| Registry / cache / DB lookups (microseconds) | Python script execution |
| EventBus publish + immediate ok return | HTTP / network calls |
| Pure computation on already-loaded data | Service callbacks (NewsService, MarketData, etc.) |
| Anything that takes < 1 ms | Anything that takes > 100 ms |

### Sync example
```cpp
t.handler = [](const QJsonObject& args) -> ToolResult {
    return ToolResult::ok_data(QJsonObject{{"echo", args["msg"].toString()}});
};
```

### Async example
```cpp
#include "mcp/AsyncDispatch.h"
#include <QPromise>

t.default_timeout_ms = 60000;
t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                     std::shared_ptr<QPromise<ToolResult>> promise) {
    auto* runner = &python::PythonRunner::instance();
    AsyncDispatch::callback_to_promise(
        runner, ctx, promise,
        [args, ctx](auto resolve) {
            python::PythonRunner::instance().run("my_script.py", {}, [resolve, ctx](python::PythonResult r) {
                if (ctx.cancelled()) { resolve(ToolResult::fail("cancelled")); return; }
                resolve(r.success ? ToolResult::ok(r.output) : ToolResult::fail(r.error));
            });
        });
};
```

The provider arms a watchdog timer based on `default_timeout_ms`; if the
handler doesn't resolve in time, the promise resolves with a timeout error
and `ctx.is_cancelled()` returns true. Handlers should poll `ctx.cancelled()`
periodically and resolve gracefully when set.

---

## 4. Authorization (Phase 6.3)

Every `ToolDef` declares `auth_required` and `is_destructive`. The provider
gates every call before the handler runs.

| `AuthLevel` | When to use |
|---|---|
| `None` (default) | Read-only tools, navigation, system info |
| `Authenticated` | Authenticated reads (portfolios, settings reads) |
| `Verified` | Sensitive reads requiring email verification |
| `Subscribed` | Premium-only analytics (alt_*, ma_*, edgar_*) |
| `ExplicitConfirm` | User must approve each call (modal) |

`is_destructive = true` triggers the modal regardless of auth level. Pair
with `Authenticated` (or higher) for state-mutating tools:

```cpp
t.auth_required = AuthLevel::Authenticated;
t.is_destructive = true;       // delete_*, set_setting, place_order, run_python_script, etc.
```

The host installs the actual gate via `McpProvider::set_auth_checker(...)` —
no auth/UI imports leak into `McpTypes.h`.

---

## 5. Adding a new tool — step by step

1. **Pick the right module file** in `src/mcp/tools/`. If your tool fits an
   existing category (markets, news, portfolio, …), append to that file. If
   it's a new category, create `MyAreaTools.{h,cpp}` and register it in
   `McpInit.cpp`.

2. **Declare the schema** with `ToolSchemaBuilder` — required params,
   defaults, enums, bounds, pattern.

3. **Pick handler shape** — sync if your work is < 1 ms, async otherwise.

4. **Set auth** — default `None` for reads, `Authenticated + is_destructive`
   for any mutation.

5. **Set timeout** — `default_timeout_ms` matters only for async handlers.
   Defaults to 30 s; set lower for snappy tools, higher for analytics.

6. **Register the factory** in your module's `get_X_tools()` returning
   `std::vector<ToolDef>`. `McpInit::initialize_all_tools` calls it at
   startup.

7. **Update the CMake source list** if you created a new file
   (`MCP_SOURCES` and the `SKIP_UNITY_BUILD_INCLUSION` list in `CMakeLists.txt`).

8. **Test** — write a unit test in `tests/mcp/` if logic is non-trivial.
   Otherwise smoke-test via AI Chat.

### Naming convention (Phase 6.10)

New tools should use dot-separated `<area>.<verb>`:
- `markets.get_quote`, not `get_quote`
- `paper.place_order`, not `pt_place_order`
- `news.get_latest`, not `get_news`

Renames preserve back-compat via `legacy_aliases`:
```cpp
t.name = "markets.get_quote";
t.legacy_aliases = {"get_quote"};   // saved chats / Finagent workflows still resolve
```

`McpProvider::call_tool_async` tries the canonical name first, then any
alias (logging a deprecation note when an alias matches).

---

## 6. Diagnosis

### "Tool not found"
- Did your factory return it? `tools.push_back(std::move(t))` at the end.
- Is your factory registered in `McpInit.cpp`?
- Is your module's `.cpp` listed in `MCP_SOURCES`?
- Run `mcp.health` (the meta-tool) — it returns the per-category counts.

### "Tool requires X auth"
- Check your tool's `auth_required` and the user's session.
- For development, install a permissive checker:
  ```cpp
  McpProvider::instance().set_auth_checker([](AuthLevel, bool) { return true; });
  ```

### Tool succeeds in tests, fails in AI Chat
- AI Chat sends args as JSON; the validator runs first. Run the same args
  through `tests/mcp/test_validator.cpp` to confirm the schema accepts them.
- Check the LLM's argument shape — sometimes it nests args inside an
  unexpected wrapper key. Look for `LOG_INFO(McpProvider, "Tool '%1' rejected: ...")`.

### Long async tool times out
- Bump `t.default_timeout_ms` for that specific tool.
- Per-call override: pass `_meta.timeout_ms` in args (Phase 6 wiring optional).

### Discovery
- `tool.list()` — short list of all tools (name + 1-line description).
- `tool.list({category: "trading"})` — filter to one category.
- `tool.list({search: "quote|price"})` — regex search.
- `tool.describe({name: "markets.get_quote"})` — full schema for one tool.
- `mcp.health()` — provider/external/datahub state.

---

## 7. References

- Refactor plans: `fincept-qt/plans/mcp-refactor-phase-{1..6}-*.md`
- Refactor index: `fincept-qt/plans/mcp-refactor-INDEX.md`
- Datahub: `fincept-qt/DATAHUB_ARCHITECTURE.md`
- Hub topics: `fincept-qt/docs/DATAHUB_TOPICS.md`
- Project rules: `fincept-qt/CLAUDE.md`
