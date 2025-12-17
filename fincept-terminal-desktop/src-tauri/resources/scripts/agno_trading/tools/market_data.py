"""
Market Data Tools

Provides tools for agents to fetch real-time and historical market data.
Integrates with existing Fincept Terminal data sources.
"""

import json
from typing import List, Dict, Any, Optional
from datetime import datetime, timedelta

try:
    import yfinance as yf
    YFINANCE_AVAILABLE = True
except ImportError:
    YFINANCE_AVAILABLE = False

try:
    import ccxt
    CCXT_AVAILABLE = True
except ImportError:
    CCXT_AVAILABLE = False


def get_current_price(symbol: str, exchange: str = "binance") -> Dict[str, Any]:
    """
    Get current price for a symbol

    Args:
        symbol: Trading symbol (e.g., "BTC/USDT", "AAPL")
        exchange: Exchange name (default: binance)

    Returns:
        Dictionary with price information
    """
    try:
        # Try crypto first with CCXT
        if CCXT_AVAILABLE and "/" in symbol:
            exchange_obj = getattr(ccxt, exchange)()
            ticker = exchange_obj.fetch_ticker(symbol)

            return {
                "symbol": symbol,
                "exchange": exchange,
                "price": ticker['last'],
                "bid": ticker['bid'],
                "ask": ticker['ask'],
                "high_24h": ticker['high'],
                "low_24h": ticker['low'],
                "volume_24h": ticker['baseVolume'],
                "timestamp": datetime.now().isoformat(),
                "source": "ccxt"
            }

        # Try stock with yfinance
        elif YFINANCE_AVAILABLE:
            ticker = yf.Ticker(symbol)
            info = ticker.info

            return {
                "symbol": symbol,
                "price": info.get('currentPrice') or info.get('regularMarketPrice'),
                "bid": info.get('bid'),
                "ask": info.get('ask'),
                "open": info.get('regularMarketOpen'),
                "high": info.get('dayHigh'),
                "low": info.get('dayLow'),
                "volume": info.get('volume'),
                "market_cap": info.get('marketCap'),
                "timestamp": datetime.now().isoformat(),
                "source": "yfinance"
            }

        else:
            return {"error": "No market data library available. Install yfinance or ccxt."}

    except Exception as e:
        return {"error": f"Failed to fetch price: {str(e)}"}


def get_historical_data(
    symbol: str,
    period: str = "1mo",
    interval: str = "1d",
    exchange: str = "binance"
) -> Dict[str, Any]:
    """
    Get historical price data

    Args:
        symbol: Trading symbol
        period: Time period (1d, 5d, 1mo, 3mo, 6mo, 1y, 2y, 5y, 10y, ytd, max)
        interval: Data interval (1m, 2m, 5m, 15m, 30m, 60m, 90m, 1h, 1d, 5d, 1wk, 1mo, 3mo)
        exchange: Exchange name for crypto

    Returns:
        Dictionary with historical data
    """
    try:
        if YFINANCE_AVAILABLE:
            # Convert crypto symbols to yfinance format: BTC/USD -> BTC-USD
            yf_symbol = symbol.replace('/', '-')
            ticker = yf.Ticker(yf_symbol)
            hist = ticker.history(period=period, interval=interval)

            if hist.empty:
                return {"error": "No historical data available"}

            return {
                "symbol": symbol,
                "period": period,
                "interval": interval,
                "data": hist.to_dict('records'),
                "count": len(hist),
                "timestamp": datetime.now().isoformat(),
                "source": "yfinance"
            }

        else:
            return {"error": "yfinance not available. Install with: pip install yfinance"}

    except Exception as e:
        return {"error": f"Failed to fetch historical data: {str(e)}"}


def get_market_summary(symbols: List[str]) -> Dict[str, Any]:
    """
    Get market summary for multiple symbols

    Args:
        symbols: List of symbols to fetch

    Returns:
        Dictionary with summary data for all symbols
    """
    summary = {
        "timestamp": datetime.now().isoformat(),
        "symbols": {},
        "errors": []
    }

    for symbol in symbols:
        try:
            price_data = get_current_price(symbol)
            if "error" not in price_data:
                summary["symbols"][symbol] = price_data
            else:
                summary["errors"].append(f"{symbol}: {price_data['error']}")
        except Exception as e:
            summary["errors"].append(f"{symbol}: {str(e)}")

    return summary


def get_ohlcv_data(
    symbol: str,
    timeframe: str = "1d",
    limit: int = 100,
    exchange: str = "binance"
) -> Dict[str, Any]:
    """
    Get OHLCV (Open, High, Low, Close, Volume) data

    Args:
        symbol: Trading symbol
        timeframe: Timeframe (1m, 5m, 15m, 1h, 4h, 1d, 1w)
        limit: Number of candles to fetch
        exchange: Exchange name

    Returns:
        Dictionary with OHLCV data
    """
    try:
        if CCXT_AVAILABLE and "/" in symbol:
            exchange_obj = getattr(ccxt, exchange)()
            ohlcv = exchange_obj.fetch_ohlcv(symbol, timeframe, limit=limit)

            # Format data
            formatted_data = []
            for candle in ohlcv:
                formatted_data.append({
                    "timestamp": candle[0],
                    "datetime": datetime.fromtimestamp(candle[0] / 1000).isoformat(),
                    "open": candle[1],
                    "high": candle[2],
                    "low": candle[3],
                    "close": candle[4],
                    "volume": candle[5]
                })

            return {
                "symbol": symbol,
                "timeframe": timeframe,
                "exchange": exchange,
                "data": formatted_data,
                "count": len(formatted_data),
                "source": "ccxt"
            }

        else:
            return {"error": "CCXT not available or invalid symbol format"}

    except Exception as e:
        return {"error": f"Failed to fetch OHLCV data: {str(e)}"}


def get_orderbook(symbol: str, exchange: str = "binance", limit: int = 20) -> Dict[str, Any]:
    """
    Get current order book

    Args:
        symbol: Trading symbol
        exchange: Exchange name
        limit: Number of orders to fetch per side

    Returns:
        Dictionary with orderbook data
    """
    try:
        if CCXT_AVAILABLE:
            exchange_obj = getattr(ccxt, exchange)()
            orderbook = exchange_obj.fetch_order_book(symbol, limit=limit)

            return {
                "symbol": symbol,
                "exchange": exchange,
                "bids": orderbook['bids'][:limit],
                "asks": orderbook['asks'][:limit],
                "timestamp": orderbook.get('timestamp'),
                "datetime": orderbook.get('datetime'),
                "source": "ccxt"
            }

        else:
            return {"error": "CCXT not available"}

    except Exception as e:
        return {"error": f"Failed to fetch orderbook: {str(e)}"}


def get_market_data_tools() -> List[Any]:
    """
    Get list of market data tools for Agno agents

    Returns:
        List of tool functions
    """
    return [
        get_current_price,
        get_historical_data,
        get_market_summary,
        get_ohlcv_data,
        get_orderbook,
    ]


# Tool descriptions for LLM
TOOL_DESCRIPTIONS = {
    "get_current_price": "Get the current market price for a symbol. Supports both crypto (BTC/USDT) and stocks (AAPL).",
    "get_historical_data": "Fetch historical price data for analysis. Supports various time periods and intervals.",
    "get_market_summary": "Get a quick summary of multiple symbols at once for market overview.",
    "get_ohlcv_data": "Get OHLCV candlestick data for charting and technical analysis.",
    "get_orderbook": "View the current order book to analyze market depth and liquidity.",
}
