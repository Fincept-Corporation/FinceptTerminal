"""
AI Quant Lab — High Frequency Trading Module
Real-world microstructure analysis engine backed by live CCXT market data.

Commands (all accept JSON as second argv):
  fetch_orderbook   {"exchange":"binance","symbol":"BTC/USDT","depth":20}
  fetch_trades      {"exchange":"binance","symbol":"BTC/USDT","limit":100}
  analyze           {"exchange":"binance","symbol":"BTC/USDT","depth":20,"limit":100,"inventory":0,"spread_multiplier":1.5}
  market_making     {"exchange":"binance","symbol":"BTC/USDT","inventory":0,"spread_multiplier":1.5}
  toxic_flow        {"exchange":"binance","symbol":"BTC/USDT","limit":200}
  slippage          {"exchange":"binance","symbol":"BTC/USDT","side":"buy","quantity":1.0}

All commands output a single JSON object to stdout.

Order-book levels are read by position (price = level[0], size = level[1]): ccxt
returns [price, size] on most venues but [price, size, timestamp] on Kraken and
[price, size, 0] on OKX.

The quoting model is an inventory-skewed spread (market spread x multiplier,
shifted against the inventory), not the full Avellaneda-Stoikov solution (no
volatility, horizon or arrival intensity), and the toxicity score is a fixed-weight
heuristic, not an estimated PIN. Labels say so.
"""

import json
import sys
import os
import time
from datetime import datetime, timezone
from typing import Any

import numpy as np

# ── Exchange client path ──────────────────────────────────────────────────────
_SCRIPTS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
_EXCHANGE_DIR = os.path.join(_SCRIPTS_DIR, "exchange")
if _EXCHANGE_DIR not in sys.path:
    sys.path.insert(0, _EXCHANGE_DIR)
if _SCRIPTS_DIR not in sys.path:
    sys.path.insert(0, _SCRIPTS_DIR)

import warnings
warnings.filterwarnings("ignore")

import fincept_json

try:
    from exchange_client import make_exchange
    _CCXT_AVAILABLE = True
except ImportError as e:
    _CCXT_AVAILABLE = False
    _CCXT_ERROR = str(e)

# Order-book depth requested for a slippage walk. Venues cap it differently
# (binance 5000, kraken 500, okx 400, bybit spot 200); the first one accepted wins.
_SLIPPAGE_DEPTHS = (500, 200, 100, 50)


# ── Helpers ───────────────────────────────────────────────────────────────────

def _ok(data: dict) -> None:
    fincept_json.emit({"success": True, **data})

def _err(msg: str) -> None:
    fincept_json.emit({"success": False, "error": msg})

def _utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%f") + "Z"

def _levels(raw: list) -> list:
    """[[price, size], ...] from ccxt levels that may carry a third element."""
    out = []
    for level in raw or []:
        if level is None or len(level) < 2 or level[0] is None or level[1] is None:
            continue
        out.append([float(level[0]), float(level[1])])
    return out

def _params() -> dict:
    if len(sys.argv) > 2:
        try:
            return json.loads(sys.argv[2])
        except (json.JSONDecodeError, ValueError):
            return {}
    return {}


# ── Order book analytics ──────────────────────────────────────────────────────

def _compute_book_metrics(bids: list, asks: list, n_levels: int = 10) -> dict:
    """Compute microstructure metrics from raw [price, size] arrays."""
    if not bids or not asks:
        return {}

    best_bid, best_bid_sz = bids[0][0], bids[0][1]
    best_ask, best_ask_sz = asks[0][0], asks[0][1]
    spread       = best_ask - best_bid
    mid_price    = (best_bid + best_ask) / 2
    spread_bps   = (spread / mid_price) * 10_000

    # Top-N volume
    bid_slice = bids[:n_levels]
    ask_slice = asks[:n_levels]
    bid_vol   = sum(b[1] for b in bid_slice)
    ask_vol   = sum(a[1] for a in ask_slice)
    total_vol = bid_vol + ask_vol

    # Order book imbalance (OBI) — key signal for short-term price direction
    obi = (bid_vol - ask_vol) / total_vol if total_vol > 0 else 0.0

    # VWAP per side (top N levels)
    bid_vwap = (sum(b[0] * b[1] for b in bid_slice) / bid_vol) if bid_vol > 0 else best_bid
    ask_vwap = (sum(a[0] * a[1] for a in ask_slice) / ask_vol) if ask_vol > 0 else best_ask

    # Weighted mid price (accounts for liquidity at each level)
    weighted_mid = (bid_vwap * ask_vol + ask_vwap * bid_vol) / (bid_vol + ask_vol) if total_vol > 0 else mid_price

    return {
        "best_bid":      best_bid,
        "best_bid_size": best_bid_sz,
        "best_ask":      best_ask,
        "best_ask_size": best_ask_sz,
        "spread":        round(spread, 8),
        "spread_bps":    round(spread_bps, 4),
        "mid_price":     mid_price,
        "weighted_mid":  round(weighted_mid, 8),
        "bid_volume":    round(bid_vol, 4),
        "ask_volume":    round(ask_vol, 4),
        "obi":           round(obi, 4),          # +1 = all bids, -1 = all asks
        "bid_vwap":      round(bid_vwap, 8),
        "ask_vwap":      round(ask_vwap, 8),
        "pressure":      "BUY" if obi > 0.1 else "SELL" if obi < -0.1 else "NEUTRAL",
    }


# ── Market making (inventory-skewed quotes) ───────────────────────────────────

def _market_making_quotes(metrics: dict, inventory: float, spread_multiplier: float,
                          risk_aversion: float = 0.01) -> dict:
    """
    Inventory-skewed quotes: the live market spread widened by spread_multiplier,
    both quotes shifted by risk_aversion x inventory x spread (a reservation-price
    skew in the spirit of Avellaneda-Stoikov, without its volatility / horizon /
    arrival-intensity terms). Sizes are relative units (100 = base size).
    inventory: current position in base asset (positive = long, negative = short)
    """
    mid  = metrics["mid_price"]
    sprd = metrics["spread"]

    if mid == 0 or sprd == 0:
        return {"error": "Insufficient market data for quoting"}

    inventory_adj  = risk_aversion * inventory * sprd
    optimal_spread = sprd * spread_multiplier
    base_size      = 100  # normalised quote size

    obi       = metrics.get("obi", 0.0)
    bid_size  = round(base_size * (1 + obi), 2) if obi > 0 else base_size
    ask_size  = round(base_size * (1 - obi), 2) if obi < 0 else base_size

    bid_price = mid - (optimal_spread / 2) - inventory_adj
    ask_price = mid + (optimal_spread / 2) - inventory_adj

    edge_per_side = (optimal_spread / 2)
    edge_bps      = (edge_per_side / mid) * 10_000

    return {
        "model":           "inventory_skewed_spread",
        "bid_price":       round(bid_price, 8),
        "ask_price":       round(ask_price, 8),
        "bid_size":        bid_size,
        "ask_size":        ask_size,
        "quoted_spread":   round(optimal_spread, 8),
        "quoted_spread_bps": round((optimal_spread / mid) * 10_000, 4),
        "edge_per_side_bps": round(edge_bps, 4),
        "inventory_adj":   round(inventory_adj, 8),
        "inventory":       inventory,
        "recommendation":  "WIDEN" if metrics.get("pressure") == "SELL" and inventory > 0 else
                           "TIGHTEN" if metrics.get("pressure") == "BUY" else "MAINTAIN",
    }


# ── Toxic flow detection ──────────────────────────────────────────────────────

def _detect_toxic_flow(trades: list, metrics: dict) -> dict:
    """
    Flow toxicity score (heuristic, 0-100) -- NOT an estimated PIN. Fixed weights:
      40 x |volume imbalance| of the sampled trades
      30 x min(|first-to-last price change| / 10 bps, 1)
      20 x min((max size / mean size - 1) / 9, 1)
      10 x |order-book imbalance|
    """
    if len(trades) < 10:
        return {"error": f"Need at least 10 trades, got {len(trades)}"}

    buys  = [t for t in trades if t.get("side") == "buy"]
    sells = [t for t in trades if t.get("side") == "sell"]

    buy_vol  = sum(t.get("amount", 0) for t in buys)
    sell_vol = sum(t.get("amount", 0) for t in sells)
    total    = buy_vol + sell_vol

    if total == 0:
        return {"error": "No volume in trade data"}

    # Volume imbalance ratio
    vol_imbalance = (buy_vol - sell_vol) / total

    # Price impact: first vs last trade price
    prices      = [t.get("price", 0) for t in trades if t.get("price")]
    price_chg   = ((prices[-1] - prices[0]) / prices[0] * 100) if len(prices) >= 2 and prices[0] else 0.0
    price_chg_bps = price_chg * 100

    # Trade size concentration (large trades = more likely informed)
    amounts     = [t.get("amount", 0) for t in trades]
    avg_size    = np.mean(amounts) if amounts else 0
    max_size    = max(amounts) if amounts else 0
    size_ratio  = (max_size / avg_size) if avg_size > 0 else 1.0

    # OBI from live book
    obi = metrics.get("obi", 0.0)

    # Composite toxicity score (0-100)
    toxicity = (
        abs(vol_imbalance) * 40 +
        min(abs(price_chg_bps) / 10, 1.0) * 30 +
        min((size_ratio - 1) / 9, 1.0) * 20 +
        abs(obi) * 10
    )

    is_toxic       = toxicity > 45
    aligns_with_book = (vol_imbalance > 0 and obi > 0) or (vol_imbalance < 0 and obi < 0)

    return {
        "model":             "heuristic_flow_toxicity",
        "toxicity_score":    round(toxicity, 2),       # 0-100
        "is_toxic":          is_toxic,
        "vol_imbalance":     round(vol_imbalance, 4),  # -1 to +1
        "price_impact_bps":  round(price_chg_bps, 2),
        "avg_trade_size":    round(avg_size, 6),
        "max_trade_size":    round(max_size, 6),
        "size_concentration": round(size_ratio, 2),
        "obi":               round(obi, 4),
        "aligns_with_book":  aligns_with_book,
        "buy_volume":        round(buy_vol, 4),
        "sell_volume":       round(sell_vol, 4),
        "trade_count":       len(trades),
        "action":            "WIDEN_SPREADS" if is_toxic else
                             "STEP_BACK" if toxicity > 30 else "NORMAL",
        "classification":    "HIGH_RISK" if toxicity > 60 else
                             "ELEVATED" if toxicity > 30 else "CLEAN",
    }


# ── Slippage estimation ───────────────────────────────────────────────────────

def _estimate_slippage(bids: list, asks: list, side: str, quantity: float) -> dict:
    """
    Walk the order book to estimate real slippage for a given quantity.
    Returns fills, average price, total cost, and slippage vs mid.
    """
    side = str(side).lower()
    if side not in ("buy", "sell"):
        return {"error": f"side must be 'buy' or 'sell', got {side!r}"}
    if not (quantity > 0):
        return {"error": "quantity must be greater than 0"}
    bids = _levels(bids)
    asks = _levels(asks)
    levels = asks if side == "buy" else bids
    if not levels:
        return {"error": "No liquidity on requested side"}
    if not bids or not asks:
        return {"error": "One-sided order book: no mid price to measure slippage against"}

    mid_price = (bids[0][0] + asks[0][0]) / 2
    remaining = quantity
    fills     = []
    total_cost = 0.0

    for level in levels:
        price, size = level[0], level[1]
        if remaining <= 0:
            break
        fill_qty   = min(remaining, size)
        fill_cost  = fill_qty * price
        fills.append({"price": price, "quantity": round(fill_qty, 8), "cost": round(fill_cost, 6)})
        total_cost += fill_cost
        remaining  -= fill_qty

    if remaining > 0:
        # Only the fetched part of the book was walked; the venue may hold more.
        return {"error": f"Order larger than the {len(levels)} {'ask' if side == 'buy' else 'bid'} levels "
                         f"fetched — {round(remaining, 8)} of {quantity} units unfilled within them "
                         f"(the exchange may have more depth beyond the levels fetched)"}

    filled_qty   = quantity - remaining
    avg_price    = total_cost / filled_qty if filled_qty > 0 else 0
    slippage_bps = abs(avg_price - mid_price) / mid_price * 10_000 if mid_price > 0 else 0

    return {
        "side":           side,
        "quantity":       quantity,
        "filled_quantity": round(filled_qty, 8),
        "average_price":  round(avg_price, 8),
        "total_cost":     round(total_cost, 6),
        "slippage_bps":   round(slippage_bps, 4),
        "mid_price":      round(mid_price, 8),
        "fills":          fills,
        "fill_count":     len(fills),
        "levels_available": len(levels),
        "viable":         slippage_bps < 20,   # < 20 bps vs mid
    }


# ── Command handlers ──────────────────────────────────────────────────────────

def _book(ob: dict) -> tuple:
    return _levels(ob.get("bids")), _levels(ob.get("asks"))


def _fetch_deep_book(ex, symbol: str) -> dict:
    """The deepest book the venue accepts from _SLIPPAGE_DEPTHS (first accepted wins)."""
    last_exc = None
    for depth in _SLIPPAGE_DEPTHS:
        try:
            return ex.fetch_order_book(symbol, limit=depth)
        except Exception as e:      # a venue rejecting this depth: try a shallower one
            last_exc = e
    raise last_exc


def cmd_fetch_orderbook(p: dict) -> None:
    if not _CCXT_AVAILABLE:
        _err(f"CCXT not available: {_CCXT_ERROR}")
        return
    exchange_id = p.get("exchange", "binance")
    symbol      = p.get("symbol", "BTC/USDT")
    depth       = int(p.get("depth", 20))
    t0 = time.monotonic()
    try:
        ex = make_exchange(exchange_id)
        ob = ex.fetch_order_book(symbol, limit=depth)
    except Exception as e:
        _err(str(e))
        return
    latency_ms = round((time.monotonic() - t0) * 1000, 1)
    bids, asks = _book(ob)
    metrics = _compute_book_metrics(bids, asks)
    if not metrics:
        _err(f"{symbol} on {exchange_id} returned an empty or one-sided order book")
        return
    _ok({
        "symbol":     symbol,
        "exchange":   exchange_id,
        "timestamp":  _utc_now(),
        "latency_ms": latency_ms,
        "bids":       bids[:depth],
        "asks":       asks[:depth],
        **metrics,
    })


def cmd_fetch_trades(p: dict) -> None:
    if not _CCXT_AVAILABLE:
        _err(f"CCXT not available: {_CCXT_ERROR}")
        return
    exchange_id = p.get("exchange", "binance")
    symbol      = p.get("symbol", "BTC/USDT")
    limit       = int(p.get("limit", 100))
    try:
        ex     = make_exchange(exchange_id)
        raw    = ex.fetch_trades(symbol, limit=limit)
        trades = [{"id": t.get("id"), "timestamp": t.get("timestamp"),
                   "side": t.get("side"), "price": t.get("price"),
                   "amount": t.get("amount"), "cost": t.get("cost")} for t in raw]
    except Exception as e:
        _err(str(e))
        return
    _ok({"symbol": symbol, "exchange": exchange_id,
         "trades": trades, "count": len(trades)})


def cmd_analyze(p: dict) -> None:
    """Single call: fetch live book + trades → compute all metrics."""
    if not _CCXT_AVAILABLE:
        _err(f"CCXT not available: {_CCXT_ERROR}")
        return
    exchange_id      = p.get("exchange", "binance")
    symbol           = p.get("symbol", "BTC/USDT")
    depth            = int(p.get("depth", 20))
    limit            = int(p.get("limit", 100))
    inventory        = float(p.get("inventory", 0.0))
    spread_mult      = float(p.get("spread_multiplier", 1.5))
    risk_aversion    = float(p.get("risk_aversion", 0.01))
    side             = p.get("side", "buy")
    t0 = time.monotonic()
    try:
        ex     = make_exchange(exchange_id)
        ob     = ex.fetch_order_book(symbol, limit=depth)
        raw    = ex.fetch_trades(symbol, limit=limit)
    except Exception as e:
        _err(str(e))
        return
    latency_ms = round((time.monotonic() - t0) * 1000, 1)

    bids, asks = _book(ob)
    trades = [{"side": t.get("side"), "price": t.get("price"),
               "amount": t.get("amount"), "timestamp": t.get("timestamp")} for t in raw]

    metrics   = _compute_book_metrics(bids, asks)
    if not metrics:
        _err(f"{symbol} on {exchange_id} returned an empty or one-sided order book")
        return
    # Side and risk aversion come from the panel's Slippage / Market-Making inputs
    # (they were fixed at "buy" and 0.01). The slippage walk here uses the `depth`
    # levels of this snapshot; the dedicated slippage command fetches a deeper book.
    mm        = _market_making_quotes(metrics, inventory, spread_mult, risk_aversion)
    toxic     = _detect_toxic_flow(trades, metrics)
    slippage  = _estimate_slippage(bids, asks, side, float(p.get("quantity", 1.0)))

    _ok({
        "symbol":       symbol,
        "exchange":     exchange_id,
        "timestamp":    _utc_now(),
        "latency_ms":   latency_ms,
        "bids":         bids[:depth],
        "asks":         asks[:depth],
        "book_metrics": metrics,
        "market_making": mm,
        "toxic_flow":   toxic,
        "slippage":     slippage,
        "trade_count":  len(trades),
    })


def cmd_market_making(p: dict) -> None:
    if not _CCXT_AVAILABLE:
        _err(f"CCXT not available: {_CCXT_ERROR}")
        return
    exchange_id = p.get("exchange", "binance")
    symbol      = p.get("symbol", "BTC/USDT")
    try:
        ex  = make_exchange(exchange_id)
        ob  = ex.fetch_order_book(symbol, limit=20)
    except Exception as e:
        _err(str(e))
        return
    metrics = _compute_book_metrics(*_book(ob))
    if not metrics:
        _err(f"{symbol} on {exchange_id} returned an empty or one-sided order book")
        return
    mm      = _market_making_quotes(
        metrics,
        float(p.get("inventory", 0.0)),
        float(p.get("spread_multiplier", 1.5)),
        float(p.get("risk_aversion", 0.01)),
    )
    if "error" in mm:       # e.g. zero spread -- a failure, not a result with blank quotes
        _err(mm["error"])
        return
    _ok({"symbol": symbol, "exchange": exchange_id,
         "book_metrics": metrics, "market_making": mm,
         "timestamp": _utc_now()})


def cmd_toxic_flow(p: dict) -> None:
    if not _CCXT_AVAILABLE:
        _err(f"CCXT not available: {_CCXT_ERROR}")
        return
    exchange_id = p.get("exchange", "binance")
    symbol      = p.get("symbol", "BTC/USDT")
    limit       = int(p.get("limit", 200))
    try:
        ex  = make_exchange(exchange_id)
        ob  = ex.fetch_order_book(symbol, limit=10)
        raw = ex.fetch_trades(symbol, limit=limit)
    except Exception as e:
        _err(str(e))
        return
    metrics = _compute_book_metrics(*_book(ob))
    trades  = [{"side": t.get("side"), "price": t.get("price"),
                "amount": t.get("amount")} for t in raw]
    toxic   = _detect_toxic_flow(trades, metrics)
    if "error" in toxic:    # too few trades / no volume: do not render it as a CLEAN reading
        _err(toxic["error"])
        return
    _ok({"symbol": symbol, "exchange": exchange_id,
         "book_metrics": metrics, "toxic_flow": toxic,
         "timestamp": _utc_now()})


def cmd_slippage(p: dict) -> None:
    if not _CCXT_AVAILABLE:
        _err(f"CCXT not available: {_CCXT_ERROR}")
        return
    exchange_id = p.get("exchange", "binance")
    symbol      = p.get("symbol", "BTC/USDT")
    side        = p.get("side", "buy")
    try:
        quantity = float(p.get("quantity", 1.0))
    except (TypeError, ValueError):
        _err("quantity must be a number")
        return
    try:
        ex = make_exchange(exchange_id)
        ob = _fetch_deep_book(ex, symbol)
    except Exception as e:
        _err(str(e))
        return
    bids, asks = _book(ob)
    result = _estimate_slippage(bids, asks, side, quantity)
    if "error" in result:   # unfilled / one-sided: a failure, not a 0-bps estimate
        _err(result["error"])
        return
    _ok({"symbol": symbol, "exchange": exchange_id, **result,
         "timestamp": _utc_now()})


# ── Entrypoint ────────────────────────────────────────────────────────────────

def main() -> None:
    if len(sys.argv) < 2:
        _err("Usage: python qlib_high_frequency.py <command> [json_params]")
        return

    command = sys.argv[1]
    p       = _params()

    dispatch = {
        "fetch_orderbook": cmd_fetch_orderbook,
        "fetch_trades":    cmd_fetch_trades,
        "analyze":         cmd_analyze,
        "market_making":   cmd_market_making,
        "market_making_quotes": cmd_market_making,   # legacy alias
        "toxic_flow":      cmd_toxic_flow,
        "detect_toxic":    cmd_toxic_flow,            # legacy alias
        "slippage":        cmd_slippage,
        "execute_order":   cmd_slippage,              # legacy alias → slippage estimate
    }

    handler = dispatch.get(command)
    if handler is None:
        _err(f"Unknown command: {command}. Available: {list(dispatch)}")
        return

    try:
        handler(p)
    except Exception as e:  # one JSON payload even on an unexpected failure
        import traceback
        traceback.print_exc(file=sys.stderr)
        _err(f"{command} failed: {e}")


if __name__ == "__main__":
    main()
