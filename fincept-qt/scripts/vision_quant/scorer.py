"""
VisionQuant Multi-Factor Scorer
Combines Vision (V), Fundamental (F), and Technical (Q) scores.

CLI Protocol:
    python scorer.py score '{"symbol":"AAPL","win_rate":72.5,"date":"20250115"}'

Score breakdown:
    V (Vision):      0-3 points — from pattern search win rate
    F (Fundamental):  0-4 points — P/E and ROE from yfinance
    Q (Technical):    0-3 points — MA60 + RSI + MACD histogram

Total: 0-10 -> BUY (>=7) / WAIT (>=5) / SELL (<5)

A part that could not be measured (no fundamentals, too little price history)
has score null and is left out of the total; max_score shrinks with it and the
action thresholds apply to the total scaled back to 0-10 (normalized_score).
A missing / null win_rate (engine search with no measured matches) is refused.
"""

import sys
import json
import math
import numpy as np
import pandas as pd

import os
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from utils import fetch_ohlcv, json_response, output_json, parse_args, with_deadline


def parse_win_rate(value):
    """The win rate as a float in 0..100, or None when absent / null / not a number."""
    if value is None or isinstance(value, bool):
        return None
    try:
        rate = float(value)
    except (TypeError, ValueError):
        return None
    if not math.isfinite(rate) or rate < 0 or rate > 100:
        return None
    return rate


WIN_RATE_REQUIRED = ("win_rate is required: a number 0-100 from engine.py search "
                     "(data.win_rate_stats.win_rate). It is null when no match could be measured, "
                     "and there is then no vision score.")


# ---------------------------------------------------------------------------
# Vision Score (V): 0 - 3
# ---------------------------------------------------------------------------

def compute_vision_score(win_rate: float, config: dict = None) -> dict:
    """
    Convert pattern match win rate to a 0-3 vision score.
    win_rate: percentage 0-100. None (nothing measured) is refused with
    ValueError — it used to arrive as a default 50 and score as if measured.

    Config keys: vision_breakpoints (default [70, 55, 40])
    """
    win_rate = parse_win_rate(win_rate)
    if win_rate is None:
        raise ValueError(WIN_RATE_REQUIRED)
    cfg = config or {}
    bp = cfg.get("vision_breakpoints", [70, 55, 40])
    if len(bp) < 3:
        bp = [70, 55, 40]

    if win_rate >= bp[0]:
        score = 3.0
    elif win_rate >= bp[1]:
        score = 2.0 + (win_rate - bp[1]) / max(bp[0] - bp[1], 1)
    elif win_rate >= bp[2]:
        score = 1.0 + (win_rate - bp[2]) / max(bp[1] - bp[2], 1)
    else:
        score = max(0, win_rate / max(bp[2], 1))

    return {
        "score": round(score, 2),
        "max": 3,
        "win_rate": round(win_rate, 1),
        "label": "Strong" if score >= 2.5 else ("Moderate" if score >= 1.5 else "Weak"),
    }


# ---------------------------------------------------------------------------
# Fundamental Score (F): 0 - 4
# ---------------------------------------------------------------------------

def compute_fundamental_score(symbol: str, config: dict = None) -> dict:
    """
    Score fundamentals using yfinance Ticker.info.
    P/E: 0-2, ROE: 0-2.

    Config keys: pe_thresholds (default [15,25,40,60]),
                 roe_thresholds (default [20,15,10,5])
    """
    cfg = config or {}
    pe_t = cfg.get("pe_thresholds", [15, 25, 40, 60])
    roe_t = cfg.get("roe_thresholds", [20, 15, 10, 5])

    def unavailable(reason):
        # No made-up "neutral" score: the part is left out of the total.
        return {"score": None, "max": 4, "pe": None, "roe": None,
                "pe_score": None, "roe_score": None, "label": "N/A", "error": reason}

    try:
        from utils import fincept_yahoo
        fy = fincept_yahoo()  # the day's profile, shared by every script and process
        info = with_deadline(lambda: fy.info(symbol), what=f"yfinance fundamentals for {symbol}") or {}
    except Exception as e:
        return unavailable(f"fundamentals unavailable: {e}")

    def number(v):
        try:
            f = float(v)
        except (TypeError, ValueError):
            return None
        return f if math.isfinite(f) else None

    # P/E ratio scoring (lower is better for value). A missing or negative P/E
    # is not scored (it used to count as a neutral 1.0).
    pe = number(info.get("trailingPE")) or number(info.get("forwardPE"))
    pe_score = None
    if pe is not None and pe > 0:
        if pe < pe_t[0]:
            pe_score = 2.0
        elif pe < pe_t[1]:
            pe_score = 1.5
        elif pe < pe_t[2]:
            pe_score = 1.0
        elif pe < pe_t[3]:
            pe_score = 0.5
        else:
            pe_score = 0.0

    # ROE scoring (higher is better)
    roe = number(info.get("returnOnEquity"))
    roe_score = None
    if roe is not None:
        roe_pct = roe * 100  # yfinance returns decimal
        if roe_pct >= roe_t[0]:
            roe_score = 2.0
        elif roe_pct >= roe_t[1]:
            roe_score = 1.5
        elif roe_pct >= roe_t[2]:
            roe_score = 1.0
        elif roe_pct >= roe_t[3]:
            roe_score = 0.5
        else:
            roe_score = 0.0

    parts = [s for s in (pe_score, roe_score) if s is not None]
    if not parts:
        return unavailable(f"no P/E or ROE reported for {symbol}")
    total = sum(parts)
    max_score = 2 * len(parts)  # 2 per scored metric
    ratio = total / max_score

    return {
        "score": round(total, 2),
        "max": max_score,
        "pe": round(pe, 2) if pe is not None else None,
        "roe": round(roe * 100, 2) if roe is not None else None,
        "pe_score": round(pe_score, 2) if pe_score is not None else None,
        "roe_score": round(roe_score, 2) if roe_score is not None else None,
        "label": "Strong" if ratio >= 0.75 else ("Moderate" if ratio >= 0.5 else "Weak"),
    }


# ---------------------------------------------------------------------------
# Technical Score (Q): 0 - 3
# ---------------------------------------------------------------------------

def compute_technical_score(symbol: str, date_str: str = None, config: dict = None) -> dict:
    """
    Score technicals: MA signal (0-1), RSI (0-1), MACD histogram (0-1).

    Config keys: ma_period (60), rsi_ranges ([30,40,60,70]),
                 macd_fast (12), macd_slow (26), macd_signal (9),
                 ma_thresholds ([1.02, 1.00, 0.98])
    """
    cfg = config or {}
    ma_period = int(cfg.get("ma_period", 60))
    rsi_ranges = cfg.get("rsi_ranges", [30, 40, 60, 70])
    if len(rsi_ranges) < 4:
        rsi_ranges = [30, 40, 60, 70]
    macd_fast = int(cfg.get("macd_fast", 12))
    macd_slow = int(cfg.get("macd_slow", 26))
    macd_signal_span = int(cfg.get("macd_signal", 9))
    ma_thresh = cfg.get("ma_thresholds", [1.02, 1.00, 0.98])
    if len(ma_thresh) < 3:
        ma_thresh = [1.02, 1.00, 0.98]

    try:
        df = fetch_ohlcv(symbol, period="1y")
        reason = f"insufficient price history for {symbol}"
    except Exception as e:
        df, reason = None, f"price history unavailable: {e}"
    if df is None or len(df) < ma_period:
        # No made-up mid score (it was 1.5): the part is left out of the total.
        return {
            "score": None,
            "max": 3,
            "ma60_signal": None,
            "rsi": None,
            "macd_hist": None,
            "label": "N/A",
            "error": reason,
        }

    close = df["Close"].values

    # If date specified, truncate
    if date_str:
        try:
            target = pd.to_datetime(date_str)
            df_trunc = df[df.index <= target]
            if len(df_trunc) >= ma_period:
                close = df_trunc["Close"].values
        except Exception:
            pass

    # 1. MA signal: price above MA = bullish
    ma = np.mean(close[-ma_period:])
    current = close[-1]
    if current > ma * ma_thresh[0]:
        ma_score = 1.0
    elif current > ma * ma_thresh[1]:
        ma_score = 0.7
    elif current > ma * ma_thresh[2]:
        ma_score = 0.3
    else:
        ma_score = 0.0

    # 2. RSI (14-period)
    deltas = np.diff(close[-15:])
    gains = np.where(deltas > 0, deltas, 0)
    losses = np.where(deltas < 0, -deltas, 0)
    avg_gain = np.mean(gains) if len(gains) > 0 else 0
    avg_loss = np.mean(losses) if len(losses) > 0 else 1e-8
    rs = avg_gain / (avg_loss + 1e-8)
    rsi = 100 - (100 / (1 + rs))

    if rsi_ranges[1] <= rsi <= rsi_ranges[2]:
        rsi_score = 0.5  # neutral
    elif rsi_ranges[0] <= rsi < rsi_ranges[1]:
        rsi_score = 0.7  # slightly oversold = opportunity
    elif rsi < rsi_ranges[0]:
        rsi_score = 1.0  # oversold = strong buy signal
    elif rsi_ranges[2] < rsi <= rsi_ranges[3]:
        rsi_score = 0.3
    else:
        rsi_score = 0.0  # overbought

    # 3. MACD histogram
    ema_fast = pd.Series(close).ewm(span=macd_fast).mean().values
    ema_slow = pd.Series(close).ewm(span=macd_slow).mean().values
    macd_line = ema_fast - ema_slow
    signal_line = pd.Series(macd_line).ewm(span=macd_signal_span).mean().values
    hist = macd_line - signal_line

    if len(hist) >= 2:
        if hist[-1] > 0 and hist[-1] > hist[-2]:
            macd_score = 1.0  # positive and rising
        elif hist[-1] > 0:
            macd_score = 0.7
        elif hist[-1] < 0 and hist[-1] > hist[-2]:
            macd_score = 0.3  # negative but improving
        else:
            macd_score = 0.0
    else:
        macd_score = 0.5

    total = ma_score + rsi_score + macd_score

    return {
        "score": round(total, 2),
        "max": 3,
        "ma60_signal": round(ma_score, 2),
        "rsi": round(rsi, 1),
        "rsi_score": round(rsi_score, 2),
        "macd_hist": round(float(hist[-1]), 4) if len(hist) > 0 else None,
        "macd_score": round(macd_score, 2),
        "label": "Strong" if total >= 2.5 else ("Moderate" if total >= 1.5 else "Weak"),
    }


# ---------------------------------------------------------------------------
# Combined Scorecard
# ---------------------------------------------------------------------------

def compute_scorecard(symbol: str, win_rate: float, date_str: str = None, config: dict = None) -> dict:
    """Compute the full V+F+Q scorecard.

    Config keys: buy_threshold (7), wait_threshold (5), plus all sub-function keys.
    """
    cfg = config or {}
    buy_threshold = float(cfg.get("buy_threshold", 7))
    wait_threshold = float(cfg.get("wait_threshold", 5))

    v = compute_vision_score(win_rate, config=cfg)  # raises on a null win rate
    f = compute_fundamental_score(symbol, config=cfg)
    q = compute_technical_score(symbol, date_str, config=cfg)

    # Unmeasured parts (score None) are left out; the thresholds (on a 0-10
    # scale) apply to the measured total scaled to 0-10.
    parts = {"vision": v, "fundamental": f, "technical": q}
    scored = [p for p in parts.values() if p.get("score") is not None]
    excluded = [name for name, p in parts.items() if p.get("score") is None]
    total = sum(p["score"] for p in scored)
    max_score = sum(p["max"] for p in scored)
    normalized = total / max_score * 10 if max_score > 0 else None

    if normalized is None or len(scored) < 2:
        # One measured part (e.g. an unknown symbol: vision only) is not a
        # basis for BUY / SELL.
        action = None
    elif normalized >= buy_threshold:
        action = "BUY"
    elif normalized >= wait_threshold:
        action = "WAIT"
    else:
        action = "SELL"

    return {
        "total_score": round(total, 2),
        "max_score": max_score,
        "normalized_score": round(normalized, 2) if normalized is not None else None,
        "excluded": excluded,
        "action": action,
        "vision": v,
        "fundamental": f,
        "technical": q,
        "symbol": symbol,
        "date": date_str or "latest",
    }


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

def main():
    command, params = parse_args()

    if command == "score":
        symbol = params.get("symbol", "")
        # No default: a missing or null win rate used to become 50 and was
        # scored as if measured.
        win_rate = parse_win_rate(params.get("win_rate"))
        date = params.get("date")
        if not symbol:
            output_json(json_response("error", error="symbol is required"))
            return
        if win_rate is None:
            output_json(json_response("error", error=WIN_RATE_REQUIRED))
            return
        result = compute_scorecard(symbol, win_rate, date, config=params)
        output_json(json_response("success", data=result))

    elif command == "vision_score":
        win_rate = parse_win_rate(params.get("win_rate"))
        if win_rate is None:
            output_json(json_response("error", error=WIN_RATE_REQUIRED))
            return
        output_json(json_response("success", data=compute_vision_score(win_rate, config=params)))

    elif command in ("fundamental_score", "technical_score"):
        symbol = params.get("symbol", "")
        if not symbol:
            output_json(json_response("error", error="symbol is required"))
            return
        if command == "fundamental_score":
            part = compute_fundamental_score(symbol, config=params)
        else:
            part = compute_technical_score(symbol, params.get("date"), config=params)
        # An unmeasurable part is an error, not a "success" carrying a placeholder.
        if part.get("score") is None:
            output_json(json_response("error", error=part.get("error") or "no data"))
            return
        output_json(json_response("success", data=part))

    else:
        output_json(json_response("error", error=f"Unknown command: {command}"))


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        import traceback
        traceback.print_exc(file=sys.stderr)
        output_json(json_response("error", error=f"{type(e).__name__}: {e}"))
        sys.exit(1)
