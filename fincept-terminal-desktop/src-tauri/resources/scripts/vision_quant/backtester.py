"""
VisionQuant Backtester
Adaptive strategy backtest using pattern intelligence signals.

CLI Protocol:
    python backtester.py backtest '{"symbol":"AAPL","start":"20230101","end":"20250101","capital":100000}'
"""

import sys
import json
import os
import numpy as np
import pandas as pd
from datetime import datetime

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from utils import fetch_ohlcv, json_response, output_json, parse_args


def compute_rsi(close, period=14):
    """Compute RSI for a close price series."""
    deltas = np.diff(close)
    gains = np.where(deltas > 0, deltas, 0)
    losses = np.where(deltas < 0, -deltas, 0)

    avg_gain = pd.Series(gains).rolling(period).mean().values
    avg_loss = pd.Series(losses).rolling(period).mean().values

    rs = avg_gain / (avg_loss + 1e-8)
    rsi = 100 - (100 / (1 + rs))
    # Pad to match original length
    return np.concatenate([[50.0], rsi])


def compute_macd_hist(close, fast=12, slow=26, signal=9):
    """Compute MACD histogram."""
    s = pd.Series(close)
    ema_fast = s.ewm(span=fast).mean().values
    ema_slow = s.ewm(span=slow).mean().values
    macd = ema_fast - ema_slow
    sig = pd.Series(macd).ewm(span=signal).mean().values
    return macd - sig


def run_backtest(symbol, start_date, end_date, initial_capital=100000.0,
                 stop_loss_pct=0.03, take_profit_pct=0.05, max_hold=20,
                 entry_rsi=40, exit_rsi=70, ma_period=60,
                 macd_fast=12, macd_slow=26, macd_signal=9):
    """
    Run a strategy backtest using MA + RSI + MACD signals.

    The strategy:
    - BUY when: price > MA AND RSI < entry_rsi AND MACD histogram turning positive
    - SELL when: stop-loss hit, take-profit hit, max holding period, or RSI > exit_rsi

    Returns dict with metrics and equity curve.
    """
    # Fetch data
    df = fetch_ohlcv(symbol, start=start_date, end=end_date)
    min_bars = ma_period + 1
    if df is None or len(df) < min_bars:
        return json_response("error", error=f"Insufficient data for {symbol} in date range")

    close = df["Close"].values
    dates = df.index.tolist()
    n = len(close)

    # Compute indicators
    ma = pd.Series(close).rolling(ma_period).mean().values
    rsi = compute_rsi(close)
    macd_hist = compute_macd_hist(close, fast=macd_fast, slow=macd_slow, signal=macd_signal)

    # Backtest state
    capital = initial_capital
    position = 0  # shares held
    entry_price = 0.0
    entry_idx = 0
    trades = []
    equity_curve = []

    for i in range(ma_period, n):
        current_price = close[i]
        current_equity = capital + position * current_price

        equity_curve.append({
            "date": dates[i].strftime("%Y-%m-%d") if hasattr(dates[i], "strftime") else str(dates[i]),
            "equity": round(current_equity, 2),
        })

        if position > 0:
            # Check exit conditions
            pnl_pct = (current_price - entry_price) / entry_price
            hold_days = i - entry_idx

            exit_signal = False
            exit_reason = ""

            if pnl_pct <= -stop_loss_pct:
                exit_signal = True
                exit_reason = "stop_loss"
            elif pnl_pct >= take_profit_pct:
                exit_signal = True
                exit_reason = "take_profit"
            elif hold_days >= max_hold:
                exit_signal = True
                exit_reason = "max_hold"
            # Also exit if RSI > exit_rsi (overbought)
            elif i < len(rsi) and rsi[i] > exit_rsi:
                exit_signal = True
                exit_reason = "rsi_overbought"

            if exit_signal:
                proceeds = position * current_price
                profit = proceeds - (position * entry_price)
                capital += proceeds
                trades.append({
                    "entry_date": dates[entry_idx].strftime("%Y-%m-%d") if hasattr(dates[entry_idx], "strftime") else str(dates[entry_idx]),
                    "exit_date": dates[i].strftime("%Y-%m-%d") if hasattr(dates[i], "strftime") else str(dates[i]),
                    "entry_price": round(entry_price, 2),
                    "exit_price": round(current_price, 2),
                    "shares": position,
                    "profit": round(profit, 2),
                    "return_pct": round(pnl_pct * 100, 2),
                    "hold_days": hold_days,
                    "exit_reason": exit_reason,
                })
                position = 0
                entry_price = 0.0

        else:
            # Check entry conditions
            if i >= len(rsi) or i >= len(macd_hist):
                continue
            if np.isnan(ma[i]):
                continue

            price_above_ma = current_price > ma[i]
            rsi_oversold = rsi[i] < entry_rsi
            macd_positive = macd_hist[i] > 0 and (i > 0 and macd_hist[i] > macd_hist[i - 1])

            if price_above_ma and rsi_oversold and macd_positive:
                # Buy with full capital
                shares = int(capital // current_price)
                if shares > 0:
                    cost = shares * current_price
                    capital -= cost
                    position = shares
                    entry_price = current_price
                    entry_idx = i

    # Close any remaining position
    if position > 0:
        final_price = close[-1]
        proceeds = position * final_price
        profit = proceeds - (position * entry_price)
        capital += proceeds
        trades.append({
            "entry_date": dates[entry_idx].strftime("%Y-%m-%d") if hasattr(dates[entry_idx], "strftime") else str(dates[entry_idx]),
            "exit_date": dates[-1].strftime("%Y-%m-%d") if hasattr(dates[-1], "strftime") else str(dates[-1]),
            "entry_price": round(entry_price, 2),
            "exit_price": round(final_price, 2),
            "shares": position,
            "profit": round(profit, 2),
            "return_pct": round((final_price - entry_price) / entry_price * 100, 2),
            "hold_days": n - 1 - entry_idx,
            "exit_reason": "end_of_period",
        })
        position = 0

    final_equity = capital
    total_return_pct = (final_equity - initial_capital) / initial_capital * 100

    # Compute metrics
    wins = [t for t in trades if t["profit"] > 0]
    losses = [t for t in trades if t["profit"] <= 0]
    win_rate = len(wins) / len(trades) * 100 if trades else 0

    # Sharpe ratio (annualized, from equity curve)
    if len(equity_curve) >= 2:
        equities = [e["equity"] for e in equity_curve]
        daily_returns = np.diff(equities) / equities[:-1]
        sharpe = float(np.mean(daily_returns) / (np.std(daily_returns) + 1e-8) * np.sqrt(252))
    else:
        sharpe = 0.0

    # Max drawdown
    if equity_curve:
        equities = np.array([e["equity"] for e in equity_curve])
        peak = np.maximum.accumulate(equities)
        dd = (equities - peak) / peak
        max_dd = float(dd.min()) * 100
    else:
        max_dd = 0.0

    # Buy & hold comparison
    bh_return = (close[-1] - close[ma_period]) / close[ma_period] * 100

    return json_response("success", data={
        "symbol": symbol,
        "start_date": start_date,
        "end_date": end_date,
        "initial_capital": initial_capital,
        "final_equity": round(final_equity, 2),
        "return_pct": round(total_return_pct, 2),
        "buy_hold_return_pct": round(bh_return, 2),
        "sharpe_ratio": round(sharpe, 3),
        "max_drawdown_pct": round(max_dd, 2),
        "total_trades": len(trades),
        "win_rate": round(win_rate, 1),
        "wins": len(wins),
        "losses": len(losses),
        "avg_profit": round(np.mean([t["profit"] for t in wins]), 2) if wins else 0,
        "avg_loss": round(np.mean([t["profit"] for t in losses]), 2) if losses else 0,
        "avg_hold_days": round(np.mean([t["hold_days"] for t in trades]), 1) if trades else 0,
        "trades": trades,
        "equity_curve": equity_curve,
    })


def main():
    command, params = parse_args()

    if command == "backtest":
        symbol = params.get("symbol", "")
        start = params.get("start", params.get("start_date", ""))
        end = params.get("end", params.get("end_date", ""))
        capital = float(params.get("capital", params.get("initial_capital", 100000)))
        stop_loss = float(params.get("stop_loss", 0.03))
        take_profit = float(params.get("take_profit", 0.05))
        max_hold = int(params.get("max_hold", 20))
        entry_rsi = float(params.get("entry_rsi", 40))
        exit_rsi = float(params.get("exit_rsi", 70))
        ma_period = int(params.get("ma_period", 60))
        macd_fast = int(params.get("macd_fast", 12))
        macd_slow = int(params.get("macd_slow", 26))
        macd_signal = int(params.get("macd_signal", 9))

        if not symbol:
            output_json(json_response("error", error="symbol is required"))
            return
        if not start or not end:
            output_json(json_response("error", error="start and end dates are required"))
            return

        # Normalize dates
        start = start.replace("-", "")
        end = end.replace("-", "")
        start_fmt = f"{start[:4]}-{start[4:6]}-{start[6:8]}"
        end_fmt = f"{end[:4]}-{end[4:6]}-{end[6:8]}"

        result = run_backtest(symbol, start_fmt, end_fmt, capital, stop_loss, take_profit,
                              max_hold, entry_rsi, exit_rsi, ma_period,
                              macd_fast, macd_slow, macd_signal)
        output_json(result)

    else:
        output_json(json_response("error", error=f"Unknown command: {command}"))


if __name__ == "__main__":
    main()
