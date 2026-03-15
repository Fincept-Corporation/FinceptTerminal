"""
WebSocket streaming bridge — long-running Python subprocess.
Connects to exchange via ccxt.pro WebSocket and streams JSON lines to stdout.
C++ reads stdout line-by-line in a background thread.

Usage: python ws_stream.py <exchange_id> <symbol> [extra_symbols...]
Example: python ws_stream.py binance BTC/USDT ETH/USDT SOL/USDT

Each line is a JSON object with a "type" field:
  {"type":"ticker", "symbol":"BTC/USDT", "last":70500.0, ...}
  {"type":"orderbook", "symbol":"BTC/USDT", "bids":[[p,a],...], "asks":[[p,a],...], ...}
  {"type":"ohlcv", "symbol":"BTC/USDT", "timeframe":"1m", "candle":{"timestamp":..., "open":..., ...}}
  {"type":"error", "message":"..."}
  {"type":"status", "connected":true, "exchange":"binance", "symbols":["BTC/USDT",...]}
"""

import asyncio
import json
import sys
import signal
import traceback
import platform

MAX_CONSECUTIVE_ERRORS = 10  # Stop a watcher after this many consecutive errors


def emit(obj):
    """Write JSON line to stdout, flush immediately."""
    try:
        print(json.dumps(obj, default=str), flush=True)
    except Exception:
        pass


async def watch_ticker(exchange, symbol):
    """Stream ticker updates."""
    errors = 0
    while True:
        try:
            ticker = await exchange.watch_ticker(symbol)
            errors = 0  # reset on success
            emit({
                "type": "ticker",
                "symbol": ticker["symbol"],
                "last": ticker.get("last"),
                "bid": ticker.get("bid"),
                "ask": ticker.get("ask"),
                "high": ticker.get("high"),
                "low": ticker.get("low"),
                "open": ticker.get("open"),
                "close": ticker.get("close"),
                "change": ticker.get("change"),
                "percentage": ticker.get("percentage"),
                "base_volume": ticker.get("baseVolume"),
                "quote_volume": ticker.get("quoteVolume"),
                "timestamp": ticker.get("timestamp"),
            })
        except Exception as e:
            errors += 1
            if errors <= 3:  # only emit first few errors
                emit({"type": "error", "source": "ticker", "symbol": symbol, "message": str(e)})
            if errors >= MAX_CONSECUTIVE_ERRORS:
                emit({"type": "error", "source": "ticker", "symbol": symbol,
                      "message": f"Too many errors ({errors}), stopping ticker watcher"})
                return
            await asyncio.sleep(min(errors * 2, 30))  # exponential backoff, max 30s


async def watch_orderbook(exchange, symbol, limit=15):
    """Stream order book updates."""
    errors = 0
    while True:
        try:
            ob = await exchange.watch_order_book(symbol, limit)
            errors = 0
            bids = [[b[0], b[1]] for b in ob["bids"][:limit]]
            asks = [[a[0], a[1]] for a in ob["asks"][:limit]]
            best_bid = bids[0][0] if bids else 0
            best_ask = asks[0][0] if asks else 0
            spread = best_ask - best_bid if best_bid and best_ask else 0
            spread_pct = (spread / best_ask * 100) if best_ask else 0
            emit({
                "type": "orderbook",
                "symbol": symbol,
                "bids": bids,
                "asks": asks,
                "best_bid": best_bid,
                "best_ask": best_ask,
                "spread": spread,
                "spread_pct": spread_pct,
            })
        except Exception as e:
            errors += 1
            if errors <= 3:
                emit({"type": "error", "source": "orderbook", "symbol": symbol, "message": str(e)})
            if errors >= MAX_CONSECUTIVE_ERRORS:
                emit({"type": "error", "source": "orderbook", "symbol": symbol,
                      "message": f"Too many errors ({errors}), stopping orderbook watcher"})
                return
            await asyncio.sleep(min(errors * 2, 30))


async def watch_ohlcv(exchange, symbol, timeframe="1m"):
    """Stream OHLCV candle updates."""
    errors = 0
    while True:
        try:
            candles = await exchange.watch_ohlcv(symbol, timeframe)
            errors = 0
            for c in candles:
                emit({
                    "type": "ohlcv",
                    "symbol": symbol,
                    "timeframe": timeframe,
                    "candle": {
                        "timestamp": c[0],
                        "open": c[1],
                        "high": c[2],
                        "low": c[3],
                        "close": c[4],
                        "volume": c[5],
                    },
                })
        except Exception as e:
            errors += 1
            if errors <= 3:
                emit({"type": "error", "source": "ohlcv", "symbol": symbol, "message": str(e)})
            if errors >= MAX_CONSECUTIVE_ERRORS:
                emit({"type": "error", "source": "ohlcv", "symbol": symbol,
                      "message": f"Too many errors ({errors}), stopping ohlcv watcher"})
                return
            await asyncio.sleep(min(errors * 2, 30))


async def watch_trades_stream(exchange, symbol):
    """Stream real-time public trades (Time & Sales)."""
    errors = 0
    while True:
        try:
            trades = await exchange.watch_trades(symbol)
            errors = 0
            for t in trades:
                emit({
                    "type": "trade",
                    "symbol": symbol,
                    "id": t.get("id"),
                    "timestamp": t.get("timestamp"),
                    "side": t.get("side"),
                    "price": t.get("price"),
                    "amount": t.get("amount"),
                    "cost": t.get("cost"),
                })
        except Exception as e:
            errors += 1
            if errors <= 3:
                emit({"type": "error", "source": "trades", "symbol": symbol, "message": str(e)})
            if errors >= MAX_CONSECUTIVE_ERRORS:
                emit({"type": "error", "source": "trades", "symbol": symbol,
                      "message": f"Too many errors ({errors}), stopping trades watcher"})
                return
            await asyncio.sleep(min(errors * 2, 30))


async def watch_tickers(exchange, symbols):
    """Stream ticker updates for multiple symbols (batch)."""
    errors = 0
    while True:
        try:
            tickers = await exchange.watch_tickers(symbols)
            errors = 0
            for symbol, ticker in tickers.items():
                emit({
                    "type": "ticker",
                    "symbol": ticker["symbol"],
                    "last": ticker.get("last"),
                    "bid": ticker.get("bid"),
                    "ask": ticker.get("ask"),
                    "high": ticker.get("high"),
                    "low": ticker.get("low"),
                    "open": ticker.get("open"),
                    "close": ticker.get("close"),
                    "change": ticker.get("change"),
                    "percentage": ticker.get("percentage"),
                    "base_volume": ticker.get("baseVolume"),
                    "quote_volume": ticker.get("quoteVolume"),
                    "timestamp": ticker.get("timestamp"),
                })
        except Exception as e:
            errors += 1
            if errors <= 3:
                emit({"type": "error", "source": "tickers", "message": str(e)})
            if errors >= MAX_CONSECUTIVE_ERRORS:
                emit({"type": "error", "source": "tickers",
                      "message": f"Too many errors ({errors}), stopping batch ticker watcher"})
                return
            await asyncio.sleep(min(errors * 2, 30))


async def main():
    if len(sys.argv) < 3:
        emit({"type": "error", "message": "Usage: ws_stream.py <exchange_id> <symbol> [symbols...]"})
        sys.exit(1)

    exchange_id = sys.argv[1]
    primary_symbol = sys.argv[2]
    all_symbols = sys.argv[2:]

    # Import ccxt.pro dynamically
    try:
        import ccxt.pro as ccxtpro
    except ImportError:
        emit({"type": "error", "message": "ccxt.pro not available, install ccxt with pip"})
        sys.exit(1)

    exchange_class = getattr(ccxtpro, exchange_id, None)
    if not exchange_class:
        emit({"type": "error", "message": f"Exchange '{exchange_id}' not found in ccxt.pro"})
        sys.exit(1)

    exchange = exchange_class({
        "enableRateLimit": True,
        "timeout": 30000,
        "options": {"defaultType": "spot"},
    })

    # Fix: On Windows, aiodns/pycares async DNS resolver can fail.
    # Force aiohttp to use ThreadedResolver (synchronous DNS in a thread pool)
    # which works reliably across all platforms.
    try:
        import aiohttp
        from aiohttp.resolver import ThreadedResolver
        exchange.session = aiohttp.ClientSession(
            connector=aiohttp.TCPConnector(resolver=ThreadedResolver())
        )
    except Exception as resolver_err:
        emit({"type": "error", "message": f"Failed to set ThreadedResolver: {resolver_err}"})

    # Try to load markets with retry — some exchanges may be temporarily unreachable
    MAX_RETRIES = 5
    markets_loaded = False
    for attempt in range(1, MAX_RETRIES + 1):
        try:
            emit({"type": "status", "connected": False, "exchange": exchange_id,
                  "symbols": all_symbols, "message": f"Loading markets (attempt {attempt}/{MAX_RETRIES})..."})
            await exchange.load_markets()
            emit({
                "type": "status",
                "connected": True,
                "exchange": exchange_id,
                "symbols": all_symbols,
            })
            markets_loaded = True
            break
        except Exception as e:
            emit({"type": "error", "message": f"Failed to load markets (attempt {attempt}): {e}"})
            if attempt < MAX_RETRIES:
                delay = min(attempt * 5, 30)
                emit({"type": "status", "connected": False, "exchange": exchange_id,
                      "symbols": all_symbols, "message": f"Retrying in {delay}s..."})
                await asyncio.sleep(delay)

    if not markets_loaded:
        emit({"type": "status", "connected": False, "exchange": exchange_id,
              "symbols": all_symbols, "message": "load_markets failed after all retries"})
        await exchange.close()
        sys.exit(1)

    try:
        tasks = [
            # Primary symbol gets full treatment: ticker + orderbook + ohlcv
            asyncio.create_task(watch_ticker(exchange, primary_symbol)),
            asyncio.create_task(watch_orderbook(exchange, primary_symbol, 15)),
            asyncio.create_task(watch_ohlcv(exchange, primary_symbol, "1m")),
            asyncio.create_task(watch_trades_stream(exchange, primary_symbol)),
        ]

        # All symbols get batch ticker updates
        if len(all_symbols) > 1:
            tasks.append(asyncio.create_task(watch_tickers(exchange, all_symbols)))

        await asyncio.gather(*tasks)
    except asyncio.CancelledError:
        pass
    except KeyboardInterrupt:
        pass
    except Exception as e:
        emit({"type": "error", "message": f"Fatal: {e}"})
        traceback.print_exc(file=sys.stderr)
    finally:
        emit({"type": "status", "connected": False, "exchange": exchange_id, "symbols": all_symbols})
        await exchange.close()


if __name__ == "__main__":
    # Handle SIGTERM gracefully
    try:
        signal.signal(signal.SIGTERM, lambda s, f: sys.exit(0))
    except Exception:
        pass

    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
