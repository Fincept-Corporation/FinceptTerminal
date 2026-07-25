#pragma once
// TimeAxisNavigator — shared pan/zoom window state for the Qt Charts
// candlestick views (CryptoChart, EquityChart).
//
// Both charts re-fit their QDateTimeAxis to the candle set on every tick, so a
// user-driven pan or zoom used to be impossible: the next WS candle snapped the
// view straight back (issue #338, "I am not able to scroll"). This class owns
// the user's window instead — once `active()` is true the chart stops auto-
// fitting the time axis and renders `min()..max()`.
//
// Deliberately Qt-free arithmetic (plain `long long`, same unit as the axis:
// real epoch-ms for CryptoChart, synthetic slot-ms for EquityChart) so the
// clamping rules can be reasoned about and tested in isolation.

namespace fincept::ui {

class TimeAxisNavigator {
  public:
    // Hard zoom-in floor and how far past the data the user may scroll,
    // both in slots.
    static constexpr int MIN_SLOTS = 5;
    static constexpr int EDGE_SLOTS = 20;

    // Current data extent + slot width. Call on every axis update so the
    // clamp follows the stream as new candles arrive.
    void set_extent(long long first, long long last, long long slot);

    // Seed the window from the chart's auto-fitted range. No-op once the user
    // has taken over, so live candles can't yank the view back.
    void seed(long long min_v, long long max_v);

    bool active() const { return active_; }
    void reset() { active_ = false; }

    long long min() const { return min_; }
    long long max() const { return max_; }

    // factor < 1 zooms in. `anchor` (a time value, typically under the cursor)
    // keeps its relative screen position. Returns false when there is no
    // window to act on yet.
    bool zoom(double factor, long long anchor);
    bool pan(long long delta);

  private:
    void clamp();

    long long first_ = 0;
    long long last_ = 0;
    long long slot_ = 60000;
    long long min_ = 0;
    long long max_ = 0;
    bool active_ = false;
};

} // namespace fincept::ui
