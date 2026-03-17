# Crypto Trading High-Value Features Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add three high-value features to the C++ crypto trading tab: order book view modes (OB/VOL/IMBAL/SIG), stop-loss/take-profit fields in the order form, and real-time fee estimation with corrected balance validation.

**Architecture:** All changes are confined to two existing files — `crypto_types.h` (data structures) and `crypto_trading_screen.cpp` (UI + logic). No new files, no new Python scripts, no database schema changes. The tick history for signal detection is computed entirely from the existing WS orderbook callback.

**Tech Stack:** C++17, ImGui, existing `ExchangeService` callbacks, `OrderMatcher` paper trading engine, `MarketInfoData` (already stores maker/taker fees).

---

## Context for Implementer

### File locations
- `src/screens/crypto_trading/crypto_types.h` — data structures, enums
- `src/screens/crypto_trading/crypto_trading_screen.h` — class declaration
- `src/screens/crypto_trading/crypto_trading_screen.cpp` — all rendering + logic (~2700 lines)

### Key line anchors in crypto_trading_screen.cpp
- **Constants block**: lines 22–33
- **WS orderbook callback** (where tick history capture goes): lines 302–321
- **`render_order_entry()`**: lines 2154–2311
- **`render_orderbook()`**: lines 2316–2434
- **`submit_order()`**: lines 2521–end

### Named constants already defined (do NOT duplicate)
```cpp
static constexpr int    OHLCV_FETCH_COUNT             = 200;
static constexpr double DEFAULT_PAPER_BALANCE          = 100000.0;
static constexpr int    PORTFOLIO_CLEANUP_THRESH       = 10;
static constexpr float  TICKER_POLL_INTERVAL           = 3.0f;
static constexpr float  ORDERBOOK_POLL_INTERVAL        = 3.0f;
static constexpr float  WATCHLIST_POLL_INTERVAL        = 15.0f;
static constexpr float  PORTFOLIO_REFRESH_INTERVAL     = 1.5f;
static constexpr float  MARKET_INFO_INTERVAL           = 30.0f;
static constexpr int    PRICE_FEED_POLL_SEC            = 3;
static constexpr int    WS_MAX_WATCHLIST_SYMBOLS       = 10;
static constexpr int    MAX_CANDLE_BUFFER              = 500;
static constexpr int    PORTFOLIO_CLEANUP_TRADE_LIMIT  = 1;
static constexpr int    MAX_TRADES                     = 200;  // in header
```

### ImGui color aliases already in scope
```cpp
using namespace theme::colors;
// Available: BG_PANEL, BG_WIDGET, BG_INPUT, ACCENT, TEXT_PRIMARY, TEXT_SECONDARY,
//            TEXT_DIM, MARKET_GREEN, MARKET_RED, WARNING, BORDER_BRIGHT
```

### C++ Core Guidelines in force
- ES.45: no magic numbers — use named `constexpr`
- Con.1: `const` by default
- CP.20: RAII locks (`std::lock_guard`)
- CP.26: no `.detach()` — use `launch_async()`
- R.11: no raw `new`/`delete`

---

## Task 1: Add types and state to crypto_types.h

**Files:**
- Modify: `src/screens/crypto_trading/crypto_types.h`

### Step 1: Read the file to confirm current state

```bash
# Verify line count and content
head -70 src/screens/crypto_trading/crypto_types.h
```

### Step 2: Add `ObViewMode` enum and `TickSnapshot` struct and SL/TP buffers

Add after the existing `// Trading mode` enum (line 67), and expand `OrderFormState`.

**Final state of `crypto_types.h`** — replace the whole file with:

```cpp
#pragma once
// Crypto Trading types — watchlist, order book, UI state

#include "trading/exchange_service.h"
#include "portfolio/portfolio_types.h"
#include <string>
#include <vector>

namespace fincept::crypto {

// Watchlist entry with live price
struct WatchlistEntry {
    std::string symbol;
    double price = 0.0;
    double change = 0.0;
    double change_pct = 0.0;
    double volume = 0.0;
    bool has_data = false;
};

// Order book level
struct OBLevel {
    double price = 0.0;
    double amount = 0.0;
    double cumulative = 0.0; // running total for depth bar
};

// Order book view mode
enum class ObViewMode { Book, Volume, Imbalance, Signals };

// Tick snapshot — captured once per second from WS orderbook callback
// Used for imbalance computation and rise-ratio signal detection
struct TickSnapshot {
    int64_t timestamp    = 0;
    double  best_bid     = 0.0;
    double  best_ask     = 0.0;
    double  bid_qty[3]   = {};   // top-3 bid quantities (weighted imbalance)
    double  ask_qty[3]   = {};   // top-3 ask quantities (weighted imbalance)
    double  imbalance    = 0.0;  // (wBid - wAsk) / (wBid + wAsk), range [-1, +1]
    double  rise_ratio_60 = 0.0; // % price change over last 60 seconds
};

// Order entry form state
struct OrderFormState {
    int side_idx = 0;           // 0=Buy, 1=Sell
    int type_idx = 0;           // 0=Market, 1=Limit, 2=Stop, 3=StopLimit
    char quantity_buf[32]   = "";
    char price_buf[32]      = "";
    char stop_price_buf[32] = "";
    char sl_price_buf[32]   = "";  // Stop Loss  (optional, non-Market orders)
    char tp_price_buf[32]   = "";  // Take Profit (optional, non-Market orders)
    bool reduce_only = false;
    std::string error;
    std::string success;
    float msg_timer = 0;
};

// Trade entry for Time & Sales feed
struct TradeEntry {
    std::string id;
    std::string side;     // "buy" | "sell"
    double price = 0.0;
    double amount = 0.0;
    int64_t timestamp = 0;
};

// Market info (funding rate, open interest, etc.)
struct MarketInfoData {
    double funding_rate = 0.0;
    double mark_price = 0.0;
    double index_price = 0.0;
    double open_interest = 0.0;
    double open_interest_value = 0.0;
    double maker_fee = 0.0;
    double taker_fee = 0.0;
    int64_t next_funding_time = 0;
    bool has_data = false;
};

// Bottom panel tab
enum class BottomTab { Positions, Orders, History, Trades, MarketInfo, Stats };

// Trading mode
enum class TradingMode { Paper, Live };

} // namespace fincept::crypto
```

### Step 3: Verify it compiles (no build yet — just check syntax mentally)

Ensure:
- `TickSnapshot::bid_qty[3]` and `ask_qty[3]` are zero-initialized via `= {}`
- `sl_price_buf` and `tp_price_buf` are `char[32]`, consistent with existing buffers
- `ObViewMode` is a scoped enum (Enum.3 — `enum class`)

### Step 4: Commit

```bash
git add src/screens/crypto_trading/crypto_types.h
git commit -m "feat(crypto): add ObViewMode enum, TickSnapshot struct, SL/TP buffers to OrderFormState"
```

---

## Task 2: Add new state members and constants to the screen class

**Files:**
- Modify: `src/screens/crypto_trading/crypto_trading_screen.h` (add 3 members)
- Modify: `src/screens/crypto_trading/crypto_trading_screen.cpp` (add 6 constants)

### Step 1: Add state members to crypto_trading_screen.h

In the `// --- Order book ---` section (around line 97), add after `ob_spread_pct_`:

```cpp
    // --- Order book view mode & tick history ---
    ObViewMode ob_view_mode_ = ObViewMode::Book;
    std::vector<TickSnapshot> tick_history_;   // capped at OB_MAX_TICK_HISTORY
    int64_t last_tick_capture_ms_ = 0;         // epoch ms — throttle to 1 capture/sec
```

The full `// --- Order book ---` block should look like:

```cpp
    // --- Order book ---
    std::vector<OBLevel> ob_bids_;
    std::vector<OBLevel> ob_asks_;
    double ob_spread_ = 0.0;
    double ob_spread_pct_ = 0.0;
    std::atomic<bool> ob_loading_{true};
    std::atomic<bool> ob_fetching_{false};

    // --- Order book view mode & tick history ---
    ObViewMode ob_view_mode_ = ObViewMode::Book;
    std::vector<TickSnapshot> tick_history_;   // capped at OB_MAX_TICK_HISTORY
    int64_t last_tick_capture_ms_ = 0;         // epoch ms — throttle to 1 capture/sec
```

### Step 2: Add named constants to crypto_trading_screen.cpp

In the constants block (lines 22–33), add after the last existing constant:

```cpp
// Order book analytics constants (ported from Tauri CryptoOrderBook.tsx)
static constexpr double OB_IMBALANCE_BUY_THRESHOLD  =  0.30;  // signal=BUY  if imbalance > this
static constexpr double OB_IMBALANCE_SELL_THRESHOLD = -0.30;  // signal=SELL if imbalance < this
static constexpr double OB_RISE_BUY_THRESHOLD       =  0.15;  // % rise over 60s required for BUY
static constexpr double OB_RISE_SELL_THRESHOLD      = -0.15;  // % rise over 60s required for SELL
static constexpr int    OB_MAX_TICK_HISTORY         = 3600;   // 1 hour at 1 snapshot/sec
static constexpr int    OB_MAX_OB_DISPLAY_LEVELS    = 12;     // visible rows per side
```

### Step 3: Build to verify no syntax errors

```bash
cmake --build build --target FinceptTerminal --config Debug 2>&1 | tail -20
```

Expected: compile errors only if you made a typo. Zero new warnings expected.

### Step 4: Commit

```bash
git add src/screens/crypto_trading/crypto_trading_screen.h \
        src/screens/crypto_trading/crypto_trading_screen.cpp
git commit -m "feat(crypto): add OB view mode state + analytics named constants"
```

---

## Task 3: Tick history capture in WS orderbook callback

**Files:**
- Modify: `src/screens/crypto_trading/crypto_trading_screen.cpp` lines 302–321

### Step 1: Understand the existing callback

The current callback at line 302 is:

```cpp
ws_ob_cb_id_ = svc.on_orderbook_update([this](const std::string& symbol, const trading::OrderBookData& ob) {
    if (symbol != selected_symbol_) return;
    std::lock_guard<std::mutex> lock(data_mutex_);
    ob_bids_.clear();
    ob_asks_.clear();
    double cum = 0;
    for (auto& [price, amount] : ob.bids) {
        cum += amount;
        ob_bids_.push_back({price, amount, cum});
    }
    cum = 0;
    for (auto& [price, amount] : ob.asks) {
        cum += amount;
        ob_asks_.push_back({price, amount, cum});
    }
    ob_spread_ = ob.spread;
    ob_spread_pct_ = ob.spread_pct;
    ob_loading_ = false;
    last_data_update_ = time(nullptr);
});
```

### Step 2: Replace the callback with tick-history-capturing version

Replace the entire callback (lines 302–321) with:

```cpp
ws_ob_cb_id_ = svc.on_orderbook_update([this](const std::string& symbol, const trading::OrderBookData& ob) {
    if (symbol != selected_symbol_) return;
    std::lock_guard<std::mutex> lock(data_mutex_);

    // Parse bids/asks into cumulative levels (unchanged behaviour)
    ob_bids_.clear();
    ob_asks_.clear();
    {
        double cum = 0;
        for (const auto& [price, amount] : ob.bids) {
            cum += amount;
            ob_bids_.push_back({price, amount, cum});
        }
    }
    {
        double cum = 0;
        for (const auto& [price, amount] : ob.asks) {
            cum += amount;
            ob_asks_.push_back({price, amount, cum});
        }
    }
    ob_spread_ = ob.spread;
    ob_spread_pct_ = ob.spread_pct;
    ob_loading_ = false;
    last_data_update_ = time(nullptr);

    // --- Tick history capture (throttled to 1 snapshot/sec) ---
    // Used by Imbalance and Signals view modes
    const int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    if (now_ms - last_tick_capture_ms_ >= OB_TICK_CAPTURE_MS) {
        last_tick_capture_ms_ = now_ms;

        TickSnapshot snap;
        snap.timestamp = now_ms;
        snap.best_bid  = ob.best_bid;
        snap.best_ask  = ob.best_ask;

        // Top-3 quantities from each side
        for (int i = 0; i < 3 && i < (int)ob_bids_.size(); ++i)
            snap.bid_qty[i] = ob_bids_[i].amount;
        for (int i = 0; i < 3 && i < (int)ob_asks_.size(); ++i)
            snap.ask_qty[i] = ob_asks_[i].amount;

        // Weighted imbalance (50/30/20 weighting of top-3 levels — SGX formula)
        const double wBid = 50.0 * snap.bid_qty[0] + 30.0 * snap.bid_qty[1] + 20.0 * snap.bid_qty[2];
        const double wAsk = 50.0 * snap.ask_qty[0] + 30.0 * snap.ask_qty[1] + 20.0 * snap.ask_qty[2];
        snap.imbalance = (wBid + wAsk > 0.0) ? (wBid - wAsk) / (wBid + wAsk) : 0.0;

        // Rise ratio over 60 seconds
        const int64_t cutoff_60 = now_ms - 60000;
        snap.rise_ratio_60 = 0.0;
        for (int i = (int)tick_history_.size() - 1; i >= 0; --i) {
            if (tick_history_[i].timestamp <= cutoff_60 && tick_history_[i].best_ask > 0.0) {
                snap.rise_ratio_60 = ((ob.best_ask - tick_history_[i].best_ask)
                                      / tick_history_[i].best_ask) * 100.0;
                break;
            }
        }

        tick_history_.push_back(snap);
        // Cap to 1 hour of history
        if ((int)tick_history_.size() > OB_MAX_TICK_HISTORY)
            tick_history_.erase(tick_history_.begin());
    }
});
```

**Important:** You also need to add the constant `OB_TICK_CAPTURE_MS` to the constants block (Task 2). Add it there:

```cpp
static constexpr int    OB_TICK_CAPTURE_MS          = 1000;   // ms between tick snapshots
```

(Total 7 new constants, not 6 — add this one too.)

### Step 3: Build to verify

```bash
cmake --build build --target FinceptTerminal --config Debug 2>&1 | tail -20
```

Expected: clean build. If you get `'OB_TICK_CAPTURE_MS' was not declared`, you forgot to add it in Task 2.

### Step 4: Commit

```bash
git add src/screens/crypto_trading/crypto_trading_screen.cpp
git commit -m "feat(crypto): capture tick history snapshots in WS orderbook callback for OB analytics"
```

---

## Task 4: Order book view mode switcher UI

**Files:**
- Modify: `src/screens/crypto_trading/crypto_trading_screen.cpp` — `render_orderbook()` lines 2316–2434

### Step 1: Understand what needs to change

The current `render_orderbook()` starts with:
```cpp
ImGui::TextColored(ACCENT, "ORDER BOOK");
ImGui::SameLine();
// spread...
ImGui::Separator();
// header row (PRICE / AMOUNT / TOTAL)
// asks child
// spread separator
// bids child
```

We need to:
1. Replace the header row with a **4-button mode switcher**: `[OB] [VOL] [IMBAL] [SIG]`
2. Keep the asks/bids child structure intact for Book and Volume modes
3. Add Imbalance and Signals render paths as early returns after the asks/bids section

### Step 2: Replace `render_orderbook()` entirely

Replace lines 2316–2434 with the following implementation:

```cpp
// ============================================================================
// Order Book — right panel bottom
// Supports 4 view modes: Book (default), Volume, Imbalance, Signals
// ============================================================================
void CryptoTradingScreen::render_orderbook(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##crypto_orderbook", ImVec2(w, h), true);

    // Title + spread
    ImGui::TextColored(ACCENT, "ORDER BOOK");
    if (ob_spread_ > 0) {
        ImGui::SameLine();
        char spread_buf[32];
        std::snprintf(spread_buf, sizeof(spread_buf), "Spread: %.2f (%.3f%%)", ob_spread_, ob_spread_pct_);
        ImGui::TextColored(TEXT_DIM, "%s", spread_buf);
    }
    if (ob_fetching_) {
        ImGui::SameLine();
        ImGui::TextColored(WARNING, "[*]");
    }

    // --- Mode switcher (4 segmented buttons) ---
    {
        struct ModeBtn { const char* label; ObViewMode mode; };
        static constexpr ModeBtn modes[] = {
            {"OB",    ObViewMode::Book},
            {"VOL",   ObViewMode::Volume},
            {"IMBAL", ObViewMode::Imbalance},
            {"SIG",   ObViewMode::Signals},
        };
        const float btn_w = (w - 24.0f) / 4.0f;
        ImGui::SameLine(0, 8);
        for (int i = 0; i < 4; ++i) {
            if (i > 0) ImGui::SameLine(0, 2);
            const bool active = (ob_view_mode_ == modes[i].mode);
            ImGui::PushStyleColor(ImGuiCol_Button,
                active ? ImVec4(0.2f, 0.4f, 0.7f, 1.0f) : BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_Text,
                active ? TEXT_PRIMARY : TEXT_DIM);
            if (ImGui::SmallButton(modes[i].label))
                ob_view_mode_ = modes[i].mode;
            ImGui::PopStyleColor(2);
        }
    }

    ImGui::Separator();

    // Empty state
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        if (ob_asks_.empty() && ob_bids_.empty()) {
            if (ob_loading_) {
                theme::LoadingSpinner("Loading order book...");
            } else {
                ImGui::TextColored(TEXT_DIM, "No order book data");
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
            return;
        }
    }

    // Dispatch to the active mode renderer
    switch (ob_view_mode_) {
        case ObViewMode::Book:      render_ob_mode_book(w);      break;
        case ObViewMode::Volume:    render_ob_mode_volume(w);    break;
        case ObViewMode::Imbalance: render_ob_mode_imbalance(w); break;
        case ObViewMode::Signals:   render_ob_mode_signals(w);   break;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}
```

**Note:** This introduces 4 private helper methods. Add their declarations to `crypto_trading_screen.h` in the `// Layout panels` private section:

```cpp
    // Order book mode renderers
    void render_ob_mode_book(float w);
    void render_ob_mode_volume(float w);
    void render_ob_mode_imbalance(float w);
    void render_ob_mode_signals(float w);
```

### Step 3: Implement `render_ob_mode_book()` — the existing Book view

This is a direct extraction of the existing asks/bids rendering from the old `render_orderbook()`. Add this function right after `render_orderbook()` ends:

```cpp
void CryptoTradingScreen::render_ob_mode_book(float w) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    const float avail_h = ImGui::GetContentRegionAvail().y;
    const float half_h  = avail_h / 2.0f;
    const float col_w   = (w - 24.0f) / 3.0f;

    // Max cumulative for depth bar scaling
    double max_cum = 1.0;
    if (!ob_bids_.empty()) max_cum = std::max(max_cum, ob_bids_.back().cumulative);
    if (!ob_asks_.empty()) max_cum = std::max(max_cum, ob_asks_.back().cumulative);

    // Column headers
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    ImGui::Text("PRICE");
    ImGui::SameLine(col_w + 4); ImGui::Text("AMOUNT");
    ImGui::SameLine(col_w * 2 + 8); ImGui::Text("TOTAL");
    ImGui::PopStyleColor();

    // Asks (sell side) — show lowest ask nearest spread
    ImGui::BeginChild("##ob_asks", ImVec2(0, half_h - 12), false);
    {
        char buf[32];
        const int start = std::max(0, (int)ob_asks_.size() - OB_MAX_OB_DISPLAY_LEVELS);
        for (int i = start; i < (int)ob_asks_.size(); ++i) {
            const auto& lvl = ob_asks_[i];
            ImVec2 p = ImGui::GetCursorScreenPos();
            const float row_w = ImGui::GetContentRegionAvail().x;
            const float bar_w = (float)(lvl.cumulative / max_cum) * row_w;
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(p.x + row_w - bar_w, p.y),
                ImVec2(p.x + row_w, p.y + ImGui::GetTextLineHeight()),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.6f, 0.1f, 0.1f, 0.15f)));
            std::snprintf(buf, sizeof(buf), "%.2f", lvl.price);
            ImGui::TextColored(MARKET_RED, "%s", buf);
            ImGui::SameLine(col_w + 4);
            std::snprintf(buf, sizeof(buf), "%.4f", lvl.amount);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);
            ImGui::SameLine(col_w * 2 + 8);
            std::snprintf(buf, sizeof(buf), "%.4f", lvl.cumulative);
            ImGui::TextColored(TEXT_DIM, "%s", buf);
        }
    }
    ImGui::EndChild();

    // Spread separator
    ImGui::PushStyleColor(ImGuiCol_Separator, BORDER_BRIGHT);
    ImGui::Separator();
    ImGui::PopStyleColor();

    // Bids (buy side) — highest to lowest
    ImGui::BeginChild("##ob_bids", ImVec2(0, 0), false);
    {
        char buf[32];
        const int count = std::min((int)ob_bids_.size(), OB_MAX_OB_DISPLAY_LEVELS);
        for (int i = 0; i < count; ++i) {
            const auto& lvl = ob_bids_[i];
            ImVec2 p = ImGui::GetCursorScreenPos();
            const float row_w = ImGui::GetContentRegionAvail().x;
            const float bar_w = (float)(lvl.cumulative / max_cum) * row_w;
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(p.x + row_w - bar_w, p.y),
                ImVec2(p.x + row_w, p.y + ImGui::GetTextLineHeight()),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.5f, 0.2f, 0.15f)));
            std::snprintf(buf, sizeof(buf), "%.2f", lvl.price);
            ImGui::TextColored(MARKET_GREEN, "%s", buf);
            ImGui::SameLine(col_w + 4);
            std::snprintf(buf, sizeof(buf), "%.4f", lvl.amount);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);
            ImGui::SameLine(col_w * 2 + 8);
            std::snprintf(buf, sizeof(buf), "%.4f", lvl.cumulative);
            ImGui::TextColored(TEXT_DIM, "%s", buf);
        }
    }
    ImGui::EndChild();
}
```

### Step 4: Implement `render_ob_mode_volume()` — Volume-at-Price view

Adds after `render_ob_mode_book()`:

```cpp
void CryptoTradingScreen::render_ob_mode_volume(float w) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    const float col_w = (w - 24.0f) / 3.0f;

    // Compute total book volume for % share
    double total_vol = 0.0;
    for (const auto& lvl : ob_asks_) total_vol += lvl.amount;
    for (const auto& lvl : ob_bids_) total_vol += lvl.amount;
    if (total_vol <= 0.0) total_vol = 1.0;

    // Headers
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    ImGui::Text("PRICE");
    ImGui::SameLine(col_w + 4); ImGui::Text("AMOUNT");
    ImGui::SameLine(col_w * 2 + 8); ImGui::Text("VOL%");
    ImGui::PopStyleColor();

    const float avail_h = ImGui::GetContentRegionAvail().y;
    const float half_h  = avail_h / 2.0f;

    // Asks — sorted by volume descending visually via bar width
    ImGui::BeginChild("##vol_asks", ImVec2(0, half_h - 12), false);
    {
        char buf[32];
        const int start = std::max(0, (int)ob_asks_.size() - OB_MAX_OB_DISPLAY_LEVELS);
        for (int i = start; i < (int)ob_asks_.size(); ++i) {
            const auto& lvl = ob_asks_[i];
            const double vol_pct = (lvl.amount / total_vol) * 100.0;
            ImVec2 p = ImGui::GetCursorScreenPos();
            const float row_w = ImGui::GetContentRegionAvail().x;
            const float bar_w = (float)(lvl.amount / (total_vol / (double)OB_MAX_OB_DISPLAY_LEVELS * 2.0)) * row_w;
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(p.x, p.y),
                ImVec2(p.x + std::min(bar_w, row_w), p.y + ImGui::GetTextLineHeight()),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.6f, 0.1f, 0.1f, 0.20f)));
            std::snprintf(buf, sizeof(buf), "%.2f", lvl.price);
            ImGui::TextColored(MARKET_RED, "%s", buf);
            ImGui::SameLine(col_w + 4);
            std::snprintf(buf, sizeof(buf), "%.4f", lvl.amount);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);
            ImGui::SameLine(col_w * 2 + 8);
            std::snprintf(buf, sizeof(buf), "%.2f%%", vol_pct);
            ImGui::TextColored(TEXT_DIM, "%s", buf);
        }
    }
    ImGui::EndChild();

    ImGui::PushStyleColor(ImGuiCol_Separator, BORDER_BRIGHT);
    ImGui::Separator();
    ImGui::PopStyleColor();

    // Bids
    ImGui::BeginChild("##vol_bids", ImVec2(0, 0), false);
    {
        char buf[32];
        const int count = std::min((int)ob_bids_.size(), OB_MAX_OB_DISPLAY_LEVELS);
        for (int i = 0; i < count; ++i) {
            const auto& lvl = ob_bids_[i];
            const double vol_pct = (lvl.amount / total_vol) * 100.0;
            ImVec2 p = ImGui::GetCursorScreenPos();
            const float row_w = ImGui::GetContentRegionAvail().x;
            const float bar_w = (float)(lvl.amount / (total_vol / (double)OB_MAX_OB_DISPLAY_LEVELS * 2.0)) * row_w;
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(p.x, p.y),
                ImVec2(p.x + std::min(bar_w, row_w), p.y + ImGui::GetTextLineHeight()),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.5f, 0.2f, 0.20f)));
            std::snprintf(buf, sizeof(buf), "%.2f", lvl.price);
            ImGui::TextColored(MARKET_GREEN, "%s", buf);
            ImGui::SameLine(col_w + 4);
            std::snprintf(buf, sizeof(buf), "%.4f", lvl.amount);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);
            ImGui::SameLine(col_w * 2 + 8);
            std::snprintf(buf, sizeof(buf), "%.2f%%", vol_pct);
            ImGui::TextColored(TEXT_DIM, "%s", buf);
        }
    }
    ImGui::EndChild();
}
```

### Step 5: Implement `render_ob_mode_imbalance()` — Weighted Imbalance view

```cpp
void CryptoTradingScreen::render_ob_mode_imbalance(float w) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    if (tick_history_.empty()) {
        ImGui::TextColored(TEXT_DIM, "Collecting data...");
        return;
    }

    const auto& latest = tick_history_.back();
    const double imbalance = latest.imbalance;

    // Large imbalance gauge at top
    {
        char gauge_buf[64];
        std::snprintf(gauge_buf, sizeof(gauge_buf), "Imbalance: %+.3f", imbalance);
        ImVec4 gauge_col = (imbalance > OB_IMBALANCE_BUY_THRESHOLD)  ? MARKET_GREEN :
                           (imbalance < OB_IMBALANCE_SELL_THRESHOLD) ? MARKET_RED   :
                           TEXT_SECONDARY;
        ImGui::TextColored(gauge_col, "%s", gauge_buf);
    }

    // Horizontal bar showing bid/ask weight ratio
    {
        const float bar_total_w = w - 24.0f;
        const float bid_frac    = (float)((imbalance + 1.0) / 2.0); // map [-1,1] -> [0,1]
        ImVec2 p = ImGui::GetCursorScreenPos();
        const float bar_h = 8.0f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        // Bid side (green, left)
        dl->AddRectFilled(p, ImVec2(p.x + bar_total_w * bid_frac, p.y + bar_h),
                          ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.55f, 0.25f, 0.8f)));
        // Ask side (red, right)
        dl->AddRectFilled(ImVec2(p.x + bar_total_w * bid_frac, p.y),
                          ImVec2(p.x + bar_total_w, p.y + bar_h),
                          ImGui::ColorConvertFloat4ToU32(ImVec4(0.65f, 0.1f, 0.1f, 0.8f)));
        ImGui::Dummy(ImVec2(bar_total_w, bar_h + 2.0f));
    }

    ImGui::Spacing();

    // Per-level imbalance table (top 3 levels, weighted)
    static const float weights[3] = {50.0f, 30.0f, 20.0f};
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    ImGui::Text("LEVEL  WEIGHT  BID QTY   ASK QTY   IMBAL");
    ImGui::PopStyleColor();
    ImGui::Separator();

    for (int i = 0; i < 3; ++i) {
        const double bq  = latest.bid_qty[i];
        const double aq  = latest.ask_qty[i];
        const double wB  = weights[i] * bq;
        const double wA  = weights[i] * aq;
        const double lvl_imbal = (wB + wA > 0.0) ? (wB - wA) / (wB + wA) : 0.0;
        const ImVec4 col = (lvl_imbal > 0.1) ? MARKET_GREEN :
                           (lvl_imbal < -0.1) ? MARKET_RED  : TEXT_SECONDARY;
        char row[96];
        std::snprintf(row, sizeof(row), "  L%d    %3.0f%%  %9.4f  %9.4f  %+.3f",
                      i + 1, weights[i], bq, aq, lvl_imbal);
        ImGui::TextColored(col, "%s", row);
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Rise ratio info
    {
        char rise_buf[48];
        std::snprintf(rise_buf, sizeof(rise_buf), "60s rise: %+.3f%%", latest.rise_ratio_60);
        ImGui::TextColored(latest.rise_ratio_60 >= 0.0 ? MARKET_GREEN : MARKET_RED, "%s", rise_buf);
    }
}
```

### Step 6: Implement `render_ob_mode_signals()` — Signal Detection view

```cpp
void CryptoTradingScreen::render_ob_mode_signals(float w) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    if (tick_history_.empty()) {
        ImGui::TextColored(TEXT_DIM, "Collecting data... (need ~5s)");
        return;
    }

    const auto& latest = tick_history_.back();
    const double imbalance  = latest.imbalance;
    const double rise_60    = latest.rise_ratio_60;

    // Determine signal
    const bool is_buy  = (imbalance > OB_IMBALANCE_BUY_THRESHOLD  && rise_60 > OB_RISE_BUY_THRESHOLD);
    const bool is_sell = (imbalance < OB_IMBALANCE_SELL_THRESHOLD && rise_60 < OB_RISE_SELL_THRESHOLD);

    // Large signal badge
    {
        const char* sig_label = is_buy ? "  BUY  " : (is_sell ? "  SELL  " : " NEUTRAL ");
        const ImVec4 sig_bg   = is_buy ? ImVec4(0.0f, 0.55f, 0.25f, 1.0f) :
                                is_sell ? ImVec4(0.65f, 0.1f, 0.1f, 1.0f) :
                                ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, sig_bg);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
        ImGui::Button(sig_label, ImVec2(w - 24.0f, 28.0f));
        ImGui::PopStyleColor(2);
    }

    ImGui::Spacing();

    // Signal metrics
    {
        char buf[64];
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::Text("Imbalance  Rise 60s  Threshold");
        ImGui::PopStyleColor();
        ImGui::Separator();

        std::snprintf(buf, sizeof(buf), "%+.3f", imbalance);
        ImGui::TextColored(imbalance > 0 ? MARKET_GREEN : MARKET_RED, "%s", buf);
        ImGui::SameLine(80);
        std::snprintf(buf, sizeof(buf), "%+.3f%%", rise_60);
        ImGui::TextColored(rise_60 >= 0 ? MARKET_GREEN : MARKET_RED, "%s", buf);
        ImGui::SameLine(160);
        ImGui::TextColored(TEXT_DIM, "BUY>%.2f|SELL<%.2f",
                           OB_IMBALANCE_BUY_THRESHOLD, OB_IMBALANCE_SELL_THRESHOLD);
    }

    ImGui::Spacing();
    ImGui::Separator();

    // History count
    {
        char hist_buf[48];
        std::snprintf(hist_buf, sizeof(hist_buf), "History: %d/%d snapshots",
                      (int)tick_history_.size(), OB_MAX_TICK_HISTORY);
        ImGui::TextColored(TEXT_DIM, "%s", hist_buf);
    }
}
```

### Step 7: Build and verify

```bash
cmake --build build --target FinceptTerminal --config Debug 2>&1 | tail -30
```

Expected: clean build. If you get `render_ob_mode_book not declared`, check Task 4 Step 2 — the declarations in the header.

### Step 8: Commit

```bash
git add src/screens/crypto_trading/crypto_trading_screen.h \
        src/screens/crypto_trading/crypto_trading_screen.cpp
git commit -m "feat(crypto): order book view modes — OB/VOL/IMBAL/SIG with tick history analytics"
```

---

## Task 5: Stop-Loss / Take-Profit fields in order form

**Files:**
- Modify: `src/screens/crypto_trading/crypto_trading_screen.cpp`
  - `render_order_entry()` lines 2154–2311
  - `submit_order()` lines 2521+

### Step 1: Add SL/TP input fields to `render_order_entry()`

After the stop price block (which ends around line 2255), add the SL/TP fields. They are only shown when type is NOT Market (`type_idx != 0`):

Find this block in `render_order_entry()`:
```cpp
    // Stop price (for stop/stoplimit)
    if (order_form_.type_idx >= 2) {
        ImGui::TextColored(TEXT_DIM, "Stop Price");
        ImGui::PushItemWidth(input_w);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::InputText("##stop_price", order_form_.stop_price_buf, sizeof(order_form_.stop_price_buf),
                         ImGuiInputTextFlags_CharsDecimal);
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();
    }

    ImGui::Spacing();

    // Reduce only checkbox
```

Replace with:
```cpp
    // Stop price (for stop/stoplimit)
    if (order_form_.type_idx >= 2) {
        ImGui::TextColored(TEXT_DIM, "Stop Price");
        ImGui::PushItemWidth(input_w);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::InputText("##stop_price", order_form_.stop_price_buf, sizeof(order_form_.stop_price_buf),
                         ImGuiInputTextFlags_CharsDecimal);
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();
    }

    // Stop Loss / Take Profit — only for non-Market orders (optional risk management)
    if (order_form_.type_idx != 0) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::Text("-- Risk Management (optional) --");
        ImGui::PopStyleColor();

        ImGui::TextColored(TEXT_DIM, "Stop Loss");
        ImGui::PushItemWidth(input_w);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::InputText("##sl_price", order_form_.sl_price_buf, sizeof(order_form_.sl_price_buf),
                         ImGuiInputTextFlags_CharsDecimal);
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();

        ImGui::TextColored(TEXT_DIM, "Take Profit");
        ImGui::PushItemWidth(input_w);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::InputText("##tp_price", order_form_.tp_price_buf, sizeof(order_form_.tp_price_buf),
                         ImGuiInputTextFlags_CharsDecimal);
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();
    }

    ImGui::Spacing();

    // Reduce only checkbox
```

### Step 2: Clear SL/TP buffers on form reset

In `submit_order()`, find the two places where form buffers are cleared after submission:

**Live path** (around line 2636):
```cpp
            order_form_.quantity_buf[0] = '\0';
            order_form_.price_buf[0] = '\0';
            order_form_.stop_price_buf[0] = '\0';
```

Replace with:
```cpp
            order_form_.quantity_buf[0]   = '\0';
            order_form_.price_buf[0]      = '\0';
            order_form_.stop_price_buf[0] = '\0';
            order_form_.sl_price_buf[0]   = '\0';
            order_form_.tp_price_buf[0]   = '\0';
```

**Paper path** — find where buffers are cleared after paper order success (search for `order_form_.quantity_buf[0] = '\0'` again after line 2640). Apply the same addition.

### Step 3: Pass SL/TP to paper trading OrderMatcher

In `submit_order()`, after the paper trading path places the order, add SL/TP trigger registration.

Find the section inside the paper trading block where `pt_place_order` is called. After the order is placed and the market order is immediately filled, add:

```cpp
        // Register SL/TP triggers with OrderMatcher if provided
        {
            const double sl = (order_form_.sl_price_buf[0] != '\0')
                              ? std::atof(order_form_.sl_price_buf) : 0.0;
            const double tp = (order_form_.tp_price_buf[0] != '\0')
                              ? std::atof(order_form_.tp_price_buf) : 0.0;
            if ((sl > 0.0 || tp > 0.0) && !order.id.empty()) {
                // OrderMatcher::set_sl_tp stores the levels and closes position
                // when price crosses them during the portfolio refresh cycle
                trading::OrderMatcher::instance().set_sl_tp(
                    portfolio_id_, selected_symbol_, order.id, sl, tp);
                LOG_INFO(TAG, "SL/TP registered: order=%s sl=%.4f tp=%.4f",
                         order.id.c_str(), sl, tp);
            }
        }
```

**Note:** `OrderMatcher::set_sl_tp()` likely does not exist yet. Check the OrderMatcher header before implementing. If it doesn't exist, implement it in Task 6.

### Step 4: Build

```bash
cmake --build build --target FinceptTerminal --config Debug 2>&1 | tail -20
```

If `set_sl_tp` is missing, it will fail here — that is expected, Task 6 adds it.

### Step 5: Commit (UI only — SL/TP fields appear, logic in Task 6)

```bash
git add src/screens/crypto_trading/crypto_trading_screen.cpp
git commit -m "feat(crypto): add Stop Loss and Take Profit input fields to order form"
```

---

## Task 6: OrderMatcher SL/TP trigger engine

**Files:**
- Read first: `src/trading/order_matcher.h`
- Read first: `src/trading/order_matcher.cpp`
- Modify: both files

### Step 1: Read OrderMatcher to understand its structure

```bash
cat src/trading/order_matcher.h
```

Identify:
- How the singleton is accessed (`instance()`)
- What methods exist for order matching
- How portfolios and positions are stored

### Step 2: Add `SLTPTrigger` struct and `set_sl_tp()` / `check_sl_tp_triggers()` declarations to header

In `order_matcher.h`, add to the public interface:

```cpp
    // SL/TP trigger registration and checking
    // Called from CryptoTradingScreen after order placement
    void set_sl_tp(const std::string& portfolio_id,
                   const std::string& symbol,
                   const std::string& order_id,
                   double sl_price,
                   double tp_price);

    // Called from portfolio refresh cycle (every PORTFOLIO_REFRESH_INTERVAL)
    // Checks if any SL/TP triggers have been crossed and closes the position
    void check_sl_tp_triggers(const std::string& portfolio_id,
                               const std::string& symbol,
                               double current_price);
```

Add a private struct and member:
```cpp
private:
    struct SLTPTrigger {
        std::string portfolio_id;
        std::string symbol;
        std::string order_id;
        double sl_price = 0.0;   // 0 = not set
        double tp_price = 0.0;   // 0 = not set
        bool   triggered = false;
    };
    std::vector<SLTPTrigger> sl_tp_triggers_;
    std::mutex sl_tp_mutex_;
```

### Step 3: Implement `set_sl_tp()` and `check_sl_tp_triggers()` in order_matcher.cpp

```cpp
void OrderMatcher::set_sl_tp(const std::string& portfolio_id,
                               const std::string& symbol,
                               const std::string& order_id,
                               double sl_price,
                               double tp_price) {
    std::lock_guard<std::mutex> lock(sl_tp_mutex_);
    // Remove any existing trigger for same symbol in same portfolio
    sl_tp_triggers_.erase(
        std::remove_if(sl_tp_triggers_.begin(), sl_tp_triggers_.end(),
                       [&](const SLTPTrigger& t) {
                           return t.portfolio_id == portfolio_id && t.symbol == symbol;
                       }),
        sl_tp_triggers_.end());
    if (sl_price > 0.0 || tp_price > 0.0) {
        sl_tp_triggers_.push_back({portfolio_id, symbol, order_id, sl_price, tp_price, false});
    }
}

void OrderMatcher::check_sl_tp_triggers(const std::string& portfolio_id,
                                         const std::string& symbol,
                                         double current_price) {
    std::lock_guard<std::mutex> lock(sl_tp_mutex_);
    for (auto& trigger : sl_tp_triggers_) {
        if (trigger.triggered) continue;
        if (trigger.portfolio_id != portfolio_id || trigger.symbol != symbol) continue;
        if (current_price <= 0.0) continue;

        const bool hit_sl = (trigger.sl_price > 0.0 && current_price <= trigger.sl_price);
        const bool hit_tp = (trigger.tp_price > 0.0 && current_price >= trigger.tp_price);

        if (hit_sl || hit_tp) {
            trigger.triggered = true;
            const char* reason = hit_sl ? "SL" : "TP";
            LOG_INFO("OrderMatcher", "Trigger %s hit: symbol=%s price=%.4f %s=%.4f",
                     reason, symbol.c_str(), current_price,
                     reason, hit_sl ? trigger.sl_price : trigger.tp_price);
            // Close the position via paper trading close order at market
            // Use pt_close_position if it exists, else place a reverse market order
            // (inspect paper_trading.h to find the right function)
        }
    }
    // Remove triggered entries
    sl_tp_triggers_.erase(
        std::remove_if(sl_tp_triggers_.begin(), sl_tp_triggers_.end(),
                       [](const SLTPTrigger& t) { return t.triggered; }),
        sl_tp_triggers_.end());
}
```

**Note:** You must inspect `paper_trading.h` to find the correct function for closing a position at market price. Use `pt_close_position()` if it exists, otherwise use `pt_place_order(..., "market", ...)` with the opposite side and full position quantity.

### Step 4: Call `check_sl_tp_triggers` from the portfolio refresh cycle

In `crypto_trading_screen.cpp`, find `refresh_portfolio_data()`. After positions are refreshed, add:

```cpp
    // Check SL/TP triggers for all open positions
    auto& matcher = trading::OrderMatcher::instance();
    for (const auto& pos : positions_) {
        const double current = trading::ExchangeService::instance()
                                   .get_cached_price(pos.symbol).last;
        if (current > 0.0)
            matcher.check_sl_tp_triggers(portfolio_id_, pos.symbol, current);
    }
```

### Step 5: Build

```bash
cmake --build build --target FinceptTerminal --config Debug 2>&1 | tail -20
```

Expected: clean build.

### Step 6: Commit

```bash
git add src/trading/order_matcher.h src/trading/order_matcher.cpp \
        src/screens/crypto_trading/crypto_trading_screen.cpp
git commit -m "feat(crypto): OrderMatcher SL/TP trigger engine — auto-close positions on price cross"
```

---

## Task 7: Fee estimation display in order form

**Files:**
- Modify: `src/screens/crypto_trading/crypto_trading_screen.cpp` — `render_order_entry()` and `submit_order()`

### Step 1: Add fee estimation block to `render_order_entry()`

Find the submit button block (starts around line 2265). **Before** the submit button (before `ImGui::PushStyleColor(ImGuiCol_Button, btn_col)`), insert:

```cpp
    // --- Fee Estimation (shown when fee data is available and qty/price are set) ---
    if (market_info_.has_data) {
        const double qty   = std::atof(order_form_.quantity_buf);
        // Price: use entered price for limit orders, current ticker for market orders
        const double price = (order_form_.type_idx == 0)
                             ? current_ticker_.last
                             : std::atof(order_form_.price_buf);

        if (qty > 0.0 && price > 0.0) {
            // Limit orders are maker (posted to book); all others are taker (immediate fill)
            const bool   is_maker      = (order_form_.type_idx == 1);
            const double effective_fee = is_maker ? market_info_.maker_fee : market_info_.taker_fee;
            const double notional      = qty * price;
            const double fee_amount    = notional * effective_fee;
            const double total_incl    = notional + fee_amount;

            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);

            char fee_buf[64];
            std::snprintf(fee_buf, sizeof(fee_buf), "Est. Fee (%.2f%% %s)",
                          effective_fee * 100.0, is_maker ? "maker" : "taker");
            ImGui::Text("%s", fee_buf);
            ImGui::SameLine(input_w - 60);
            std::snprintf(fee_buf, sizeof(fee_buf), "%.4f", fee_amount);
            ImGui::Text("%s", fee_buf);

            ImGui::Text("Total incl. fee");
            ImGui::SameLine(input_w - 60);
            std::snprintf(fee_buf, sizeof(fee_buf), "%.2f", total_incl);
            ImGui::Text("%s", fee_buf);

            ImGui::PopStyleColor();
            ImGui::Separator();
        }
    }
```

### Step 2: Fix balance validation in `submit_order()` to include fees

Find the balance check in `submit_order()`. Currently it's:
```cpp
    if (qty <= 0) {
        order_form_.error = "Quantity must be > 0";
```

The balance check is deeper in the function, in the paper trading path. Find the section that checks balance for BUY orders — it looks like:

```cpp
        if (order_form_.side_idx == 0) {
            // ... balance check
        }
```

When you find it, update it to include fees:
```cpp
        if (order_form_.side_idx == 0) {
            const double notional = qty * price.value_or(current_ticker_.last);
            const bool   is_maker = (order_form_.type_idx == 1);
            const double fee_rate = is_maker ? market_info_.maker_fee : market_info_.taker_fee;
            const double total_incl_fee = notional * (1.0 + fee_rate);

            if (total_incl_fee > portfolio_.balance) {
                char bal_buf[128];
                std::snprintf(bal_buf, sizeof(bal_buf),
                              "Insufficient balance ($%.2f available, $%.2f needed incl. fee)",
                              portfolio_.balance, total_incl_fee);
                order_form_.error = bal_buf;
                order_form_.msg_timer = 5.0f;
                return;
            }
        }
```

### Step 3: Build

```bash
cmake --build build --target FinceptTerminal --config Debug 2>&1 | tail -20
```

### Step 4: Commit

```bash
git add src/screens/crypto_trading/crypto_trading_screen.cpp
git commit -m "feat(crypto): real-time fee estimation in order form + fee-inclusive balance validation"
```

---

## Task 8: Release build + smoke test

### Step 1: Build Release

```bash
cmake --build build --target FinceptTerminal --config Release 2>&1 | tail -30
```

Expected: clean build, zero warnings on new code.

### Step 2: Manual smoke test checklist

Launch the terminal and navigate to the Crypto Trading tab. Verify:

**Order Book Modes:**
- [ ] `[OB]` button active by default, shows existing price/amount/total columns
- [ ] `[VOL]` button shows VOL% column with left-aligned bars
- [ ] `[IMBAL]` button shows imbalance gauge + 3-level table (may show "Collecting data..." for first few seconds)
- [ ] `[SIG]` button shows BUY/SELL/NEUTRAL badge + metrics
- [ ] Mode switcher persists when switching symbols (it's screen-level state, not per-symbol)
- [ ] No crash when switching modes while order book is empty

**SL/TP Fields:**
- [ ] Market order type: SL/TP section NOT shown
- [ ] Limit order type: "Risk Management (optional)" section appears with Stop Loss + Take Profit inputs
- [ ] Both fields accept decimal input
- [ ] Both fields clear when order is submitted

**Fee Estimation:**
- [ ] Fee block appears when Market Info data has loaded (may take up to 30s on first load)
- [ ] Limit order shows "maker" fee, Market/Stop orders show "taker" fee
- [ ] Fee block hidden when qty or price is zero
- [ ] Balance error includes fee in the "needed" amount

### Step 3: Final commit if any last fixes applied

```bash
git add -p   # stage only what you fixed
git commit -m "fix(crypto): smoke test fixes — <describe what you fixed>"
```

---

## Summary

| Task | Feature | Key Files |
|------|---------|-----------|
| 1 | Types: `ObViewMode`, `TickSnapshot`, SL/TP buffers | `crypto_types.h` |
| 2 | State members + 7 named constants | `screen.h`, `screen.cpp` |
| 3 | Tick history capture in WS callback | `screen.cpp:302` |
| 4 | OB mode switcher + 4 render functions | `screen.h`, `screen.cpp:2316` |
| 5 | SL/TP input fields + form clear | `screen.cpp:2246` |
| 6 | OrderMatcher SL/TP trigger engine | `order_matcher.h/.cpp`, `screen.cpp` |
| 7 | Fee estimation display + balance validation fix | `screen.cpp:2265` |
| 8 | Release build + smoke test | — |
