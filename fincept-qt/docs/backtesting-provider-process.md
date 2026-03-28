# Backtesting Provider Integration Process

Follow this checklist **exactly** for each provider, in order.

## Provider Order
1. ~~BT~~ ✅
2. ~~VectorBT~~ ✅
3. ~~Backtesting.py~~ ✅
4. ~~FastTrade~~ ✅
5. ~~Zipline~~ ✅
6. ~~Fincept~~ ✅

> **Architecture note (applied after providers 1-3):**
> Python is now the single source of truth. `default_strategies()` and `all_indicators()` return `{}`.
> On provider switch, C++ calls `load_strategies()`, `get_indicators`, and `load_command_options()`.
> All combos are populated dynamically. `strategies_from_json()` handles BT and VectorBT shapes.

---

## Step-by-Step Process

### STEP 1 — Audit Python files
- Read all `*_provider.py` + any sibling strategy/indicator files
- List every exposed command (what CLI commands the provider handles)
- List every strategy ID + category + parameters (name, type, options if select)
- List every indicator ID + parameters
- Note any missing library coverage (compare against library docs/pip)

### STEP 2 — Expand Python coverage
- If library has meaningful algos not yet registered, add them
- New strategy files follow pattern: `{provider}_{topic}.py`
- Import new files in `{provider}_provider.py` and `__init__.py`
- Fix any import/path collisions (use aliased imports if needed)
- Test: `python {provider}_provider.py get_strategies '{}'` must return valid JSON

### STEP 3 — Cross-check C++ vs Python (BacktestingTypes.h)
Verify these 5 things strictly:

| Check | What to verify |
|-------|---------------|
| Commands | Provider's command list in `all_providers()` matches what Python actually handles |
| Strategy IDs | Every ID in `default_strategies()` fallback matches a real Python strategy ID |
| Strategy categories | `strategy_categories()` contains all Python categories (exact case/spelling) |
| Indicator IDs | Every ID in `all_indicators()` fallback matches Python INDICATOR_CATALOG |
| Param names | Param `name` fields in C++ structs match exact keys Python reads from JSON |

### STEP 4 — Fix all mismatches
- Fix IDs, categories, param names in `BacktestingTypes.h`
- Remove C++ strategies/indicators with no Python equivalent
- Add C++ fallback entries for Python strategies missing from fallback
- Keep `default_strategies()` as an accurate subset (not exhaustive — dynamic load handles the rest)

### STEP 5 — Check for dead code
- Grep every `inline` function in `BacktestingTypes.h` — confirm each is called in `BacktestingScreen.cpp`
- Remove any that are not referenced anywhere

### STEP 6 — Build & verify
- Build: `cmake --build build --config Debug`
- Zero new errors (pre-existing warnings are acceptable)
- Launch app, navigate to Backtesting tab, select the provider
- Confirm strategy list loads dynamically from Python (not just fallback)
- Confirm indicators load correctly
- Run one backtest end-to-end

---

## Rules
- Never change `all_providers()` command lists without verifying Python handles them
- Category strings must match Python exactly (camelCase where Python uses camelCase)
- Fallback lists exist for offline/error state — keep them accurate but minimal
- One provider at a time — complete all 6 steps before moving to next
