"""
mcp_server.py — Fincept MCP HTTP server for RD-Agent tool use.

Provides financial data tools that rdagent loops can call via
MCPServerStreamableHTTP. Runs as a standalone FastMCP server.

Tools exposed:
  - market_data        : fetch OHLCV + quote data for a symbol
  - financial_news     : fetch recent news headlines
  - economics_data     : fetch macro indicators (GDP, CPI, rates)
  - factor_backtest    : quick IC/Sharpe estimate for a factor expression
  - symbol_search      : search for ticker symbols

Usage (standalone):
  python mcp_server.py --port 18765

rdagent connects via:
  MCPServerStreamableHTTP("http://localhost:18765/mcp")
"""

from __future__ import annotations

import json
import logging
import os
from datetime import datetime, timedelta
from typing import Any

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Availability checks
# ---------------------------------------------------------------------------

MCP_SERVER_AVAILABLE = False
try:
    from mcp.server.fastmcp import FastMCP
    MCP_SERVER_AVAILABLE = True
except ImportError:
    try:
        from fastmcp import FastMCP  # type: ignore
        MCP_SERVER_AVAILABLE = True
    except ImportError:
        FastMCP = None  # type: ignore

YFINANCE_AVAILABLE = False
try:
    import yfinance as yf
    YFINANCE_AVAILABLE = True
except ImportError:
    pass

REQUESTS_AVAILABLE = False
try:
    import requests
    REQUESTS_AVAILABLE = True
except ImportError:
    pass


# ---------------------------------------------------------------------------
# Server definition
# ---------------------------------------------------------------------------

def build_mcp_server() -> Any:
    """Build and return the FastMCP server instance."""
    if not MCP_SERVER_AVAILABLE:
        raise RuntimeError(
            "FastMCP not installed. Run: pip install mcp[cli] or pip install fastmcp"
        )

    mcp = FastMCP(
        name="fincept-tools",
        instructions=(
            "Financial data tools for quantitative research. "
            "Use market_data to get price history, financial_news for recent headlines, "
            "economics_data for macro indicators, and factor_backtest to evaluate factor expressions."
        ),
    )

    # ── Tool: market_data ────────────────────────────────────────────────────
    @mcp.tool()
    def market_data(
        symbol: str,
        period: str = "1y",
        interval: str = "1d",
        include_fundamentals: bool = False,
    ) -> dict[str, Any]:
        """
        Fetch OHLCV price history and current quote for a symbol.

        Args:
            symbol:               Ticker symbol (e.g. AAPL, 000001.SS, BTC-USD)
            period:               Data period: 1d, 5d, 1mo, 3mo, 6mo, 1y, 2y, 5y, 10y, ytd, max
            interval:             Bar interval: 1m, 5m, 15m, 30m, 1h, 1d, 1wk, 1mo
            include_fundamentals: Include P/E, P/B, market cap, etc.

        Returns:
            dict with keys: symbol, period, bars (list of OHLCV dicts),
            latest_price, change_pct, volume, fundamentals (if requested)
        """
        if not YFINANCE_AVAILABLE:
            return {"error": "yfinance not installed. Run: pip install yfinance"}

        try:
            ticker = yf.Ticker(symbol.upper())
            hist = ticker.history(period=period, interval=interval)

            if hist.empty:
                return {"error": f"No data found for symbol {symbol!r}"}

            bars = []
            for ts, row in hist.iterrows():
                bars.append({
                    "date":   str(ts.date()) if hasattr(ts, "date") else str(ts),
                    "open":   round(float(row["Open"]), 4),
                    "high":   round(float(row["High"]), 4),
                    "low":    round(float(row["Low"]), 4),
                    "close":  round(float(row["Close"]), 4),
                    "volume": int(row["Volume"]),
                })

            latest = bars[-1] if bars else {}
            prev   = bars[-2] if len(bars) > 1 else latest
            change_pct = (
                (latest["close"] - prev["close"]) / prev["close"] * 100
                if prev.get("close") else 0.0
            )

            result: dict[str, Any] = {
                "symbol":       symbol.upper(),
                "period":       period,
                "interval":     interval,
                "bar_count":    len(bars),
                "latest_price": latest.get("close"),
                "change_pct":   round(change_pct, 3),
                "volume":       latest.get("volume"),
                "bars":         bars[-252:],  # cap at ~1 year of daily bars
            }

            if include_fundamentals:
                info = ticker.info
                result["fundamentals"] = {
                    "market_cap":      info.get("marketCap"),
                    "pe_ratio":        info.get("trailingPE"),
                    "pb_ratio":        info.get("priceToBook"),
                    "dividend_yield":  info.get("dividendYield"),
                    "52w_high":        info.get("fiftyTwoWeekHigh"),
                    "52w_low":         info.get("fiftyTwoWeekLow"),
                    "avg_volume":      info.get("averageVolume"),
                    "sector":          info.get("sector"),
                    "industry":        info.get("industry"),
                    "description":     (info.get("longBusinessSummary") or "")[:500],
                }

            return result

        except Exception as e:
            logger.exception("market_data(%s) failed", symbol)
            return {"error": str(e), "symbol": symbol}

    # ── Tool: financial_news ─────────────────────────────────────────────────
    @mcp.tool()
    def financial_news(
        query: str = "",
        symbol: str = "",
        limit: int = 20,
        days_back: int = 7,
    ) -> dict[str, Any]:
        """
        Fetch recent financial news headlines.

        Args:
            query:     Search keywords (e.g. "Fed interest rates", "NVDA earnings")
            symbol:    Ticker symbol to get news for (e.g. AAPL). Used if query is empty.
            limit:     Max number of articles to return (1-50)
            days_back: How many days back to search (1-30)

        Returns:
            dict with keys: articles (list), total, query_used
        """
        limit = max(1, min(50, limit))
        days_back = max(1, min(30, days_back))
        articles = []

        # Try yfinance news for symbol-specific queries
        if symbol and YFINANCE_AVAILABLE:
            try:
                ticker = yf.Ticker(symbol.upper())
                news = ticker.news or []
                cutoff = datetime.now() - timedelta(days=days_back)
                for item in news[:limit]:
                    pub_ts = item.get("providerPublishTime", 0)
                    pub_dt = datetime.fromtimestamp(pub_ts) if pub_ts else None
                    if pub_dt and pub_dt < cutoff:
                        continue
                    articles.append({
                        "title":     item.get("title", ""),
                        "source":    item.get("publisher", ""),
                        "published": pub_dt.isoformat() if pub_dt else "",
                        "url":       item.get("link", ""),
                        "summary":   item.get("summary", ""),
                        "symbol":    symbol.upper(),
                    })
            except Exception as e:
                logger.warning("yfinance news for %s failed: %s", symbol, e)

        # Fallback: NewsAPI if key is configured
        if not articles and REQUESTS_AVAILABLE:
            api_key = os.environ.get("NEWS_API_KEY", "")
            if api_key:
                try:
                    params: dict[str, Any] = {
                        "apiKey":   api_key,
                        "pageSize": limit,
                        "language": "en",
                        "sortBy":   "publishedAt",
                        "from":     (datetime.now() - timedelta(days=days_back)).strftime("%Y-%m-%d"),
                    }
                    if query:
                        params["q"] = query
                    elif symbol:
                        params["q"] = symbol
                    else:
                        params["q"] = "stock market finance"

                    resp = requests.get(
                        "https://newsapi.org/v2/everything", params=params, timeout=10
                    )
                    if resp.ok:
                        for item in resp.json().get("articles", []):
                            articles.append({
                                "title":     item.get("title", ""),
                                "source":    item.get("source", {}).get("name", ""),
                                "published": item.get("publishedAt", ""),
                                "url":       item.get("url", ""),
                                "summary":   item.get("description", ""),
                            })
                except Exception as e:
                    logger.warning("NewsAPI failed: %s", e)

        return {
            "articles":   articles[:limit],
            "total":      len(articles),
            "query_used": query or symbol or "general financial news",
            "days_back":  days_back,
        }

    # ── Tool: economics_data ─────────────────────────────────────────────────
    @mcp.tool()
    def economics_data(
        indicators: list[str] | None = None,
        country: str = "US",
    ) -> dict[str, Any]:
        """
        Fetch macroeconomic indicators using yfinance proxies.

        Args:
            indicators: List of indicators to fetch. Options:
                        ['gdp_growth', 'cpi', 'unemployment', 'fed_rate',
                         'treasury_10y', 'treasury_2y', 'vix', 'dxy',
                         'oil_wti', 'gold', 'sp500']
                        Defaults to all.
            country:    Country code (currently US-focused).

        Returns:
            dict with indicator name → {value, change_pct, date}
        """
        if not YFINANCE_AVAILABLE:
            return {"error": "yfinance not installed. Run: pip install yfinance"}

        # Map indicator names to yfinance tickers
        INDICATOR_TICKERS: dict[str, str] = {
            "treasury_10y": "^TNX",
            "treasury_2y":  "^IRX",
            "vix":          "^VIX",
            "dxy":          "DX-Y.NYB",
            "oil_wti":      "CL=F",
            "gold":         "GC=F",
            "sp500":        "^GSPC",
            "nasdaq":       "^IXIC",
            "dow":          "^DJI",
        }

        # FRED-based indicators via yfinance ETF proxies
        FRED_PROXIES: dict[str, str] = {
            "cpi":          "RINF",   # inflation ETF as proxy
            "unemployment": "^VXX",   # no direct proxy — note in output
        }

        all_indicators = list(INDICATOR_TICKERS.keys()) + ["cpi", "unemployment",
                                                             "fed_rate", "gdp_growth"]
        if indicators is None:
            indicators = all_indicators

        result: dict[str, Any] = {"country": country, "timestamp": datetime.now().isoformat()}
        data: dict[str, Any] = {}

        for ind in indicators:
            ticker_sym = INDICATOR_TICKERS.get(ind) or FRED_PROXIES.get(ind)
            if not ticker_sym:
                # Hardcoded / static values for indicators without direct tickers
                if ind == "fed_rate":
                    data[ind] = {
                        "value":  float(os.environ.get("FED_RATE_OVERRIDE", "5.25")),
                        "note":   "Set FED_RATE_OVERRIDE env var to override, or use FRED API",
                        "source": "static",
                    }
                elif ind == "gdp_growth":
                    data[ind] = {
                        "value":  float(os.environ.get("GDP_GROWTH_OVERRIDE", "2.8")),
                        "note":   "Set GDP_GROWTH_OVERRIDE env var to override, or use FRED API",
                        "source": "static",
                    }
                continue

            try:
                ticker = yf.Ticker(ticker_sym)
                hist = ticker.history(period="5d", interval="1d")
                if hist.empty:
                    data[ind] = {"error": f"No data for {ticker_sym}"}
                    continue

                latest_close = float(hist["Close"].iloc[-1])
                prev_close   = float(hist["Close"].iloc[-2]) if len(hist) > 1 else latest_close
                change_pct   = (latest_close - prev_close) / prev_close * 100 if prev_close else 0

                data[ind] = {
                    "value":      round(latest_close, 4),
                    "change_pct": round(change_pct, 3),
                    "date":       str(hist.index[-1].date()),
                    "ticker":     ticker_sym,
                }
            except Exception as e:
                data[ind] = {"error": str(e), "ticker": ticker_sym}

        result["indicators"] = data
        return result

    # ── Tool: factor_backtest ────────────────────────────────────────────────
    @mcp.tool()
    def factor_backtest(
        symbol: str,
        factor_expr: str,
        period: str = "2y",
        top_pct: float = 0.2,
    ) -> dict[str, Any]:
        """
        Quick IC/Sharpe estimate for a factor expression on a single symbol.

        Computes the factor value for each bar using the expression, then
        calculates next-period return rank correlation (IC) and a simple
        long-top-decile strategy Sharpe.

        Args:
            symbol:      Ticker symbol to test on
            factor_expr: Python expression using columns: open, high, low, close,
                         volume, returns. E.g. "close / close.rolling(20).mean() - 1"
            period:      Data period (1y, 2y, 5y)
            top_pct:     Top percentile to go long (0.1 = top 10%)

        Returns:
            dict with ic, ic_ir, sharpe, win_rate, max_drawdown
        """
        if not YFINANCE_AVAILABLE:
            return {"error": "yfinance not installed"}
        try:
            import pandas as pd
            import numpy as np

            ticker = yf.Ticker(symbol.upper())
            hist = ticker.history(period=period, interval="1d")
            if len(hist) < 60:
                return {"error": f"Insufficient data for {symbol} ({len(hist)} bars)"}

            df = pd.DataFrame({
                "open":   hist["Open"],
                "high":   hist["High"],
                "low":    hist["Low"],
                "close":  hist["Close"],
                "volume": hist["Volume"],
            })
            df["returns"] = df["close"].pct_change()

            # Evaluate factor expression safely
            try:
                factor_vals = eval(  # noqa: S307
                    factor_expr,
                    {"__builtins__": {}},
                    {**{col: df[col] for col in df.columns},
                     "pd": pd, "np": np},
                )
            except Exception as e:
                return {"error": f"Factor expression error: {e}"}

            df["factor"] = factor_vals
            df["fwd_ret"] = df["returns"].shift(-1)
            df = df.dropna()

            if len(df) < 20:
                return {"error": "Too few valid bars after factor computation"}

            # IC = rank correlation between factor and forward return
            ic_series = df["factor"].rolling(20).corr(df["fwd_ret"])
            ic = float(ic_series.mean())
            ic_ir = float(ic / ic_series.std()) if ic_series.std() > 0 else 0.0

            # Simple long-top strategy
            threshold = df["factor"].quantile(1 - top_pct)
            long_mask = df["factor"] >= threshold
            strategy_ret = df["fwd_ret"].where(long_mask, 0.0)
            ann = 252 ** 0.5
            sharpe = float(strategy_ret.mean() / strategy_ret.std() * ann) if strategy_ret.std() > 0 else 0.0
            win_rate = float((strategy_ret[long_mask] > 0).mean()) if long_mask.sum() > 0 else 0.0

            cum = (1 + strategy_ret).cumprod()
            rolling_max = cum.cummax()
            drawdown = (cum - rolling_max) / rolling_max
            max_dd = float(drawdown.min())

            return {
                "symbol":       symbol.upper(),
                "factor_expr":  factor_expr,
                "period":       period,
                "bar_count":    len(df),
                "ic":           round(ic, 4),
                "ic_ir":        round(ic_ir, 4),
                "sharpe":       round(sharpe, 3),
                "win_rate":     round(win_rate, 3),
                "max_drawdown": round(max_dd, 3),
                "long_bars":    int(long_mask.sum()),
            }
        except Exception as e:
            logger.exception("factor_backtest(%s) failed", symbol)
            return {"error": str(e)}

    # ── Tool: symbol_search ──────────────────────────────────────────────────
    @mcp.tool()
    def symbol_search(query: str, limit: int = 10) -> dict[str, Any]:
        """
        Search for ticker symbols matching a company name or keyword.

        Args:
            query: Company name or keyword (e.g. "Apple", "semiconductor ETF")
            limit: Max results to return (1-20)

        Returns:
            dict with results list of {symbol, name, exchange, type}
        """
        if not YFINANCE_AVAILABLE:
            return {"error": "yfinance not installed"}
        try:
            results = yf.Search(query, max_results=min(20, limit))
            quotes = results.quotes if hasattr(results, "quotes") else []
            return {
                "query":   query,
                "results": [
                    {
                        "symbol":   q.get("symbol", ""),
                        "name":     q.get("shortname") or q.get("longname", ""),
                        "exchange": q.get("exchange", ""),
                        "type":     q.get("quoteType", ""),
                    }
                    for q in quotes[:limit]
                ],
                "total": len(quotes),
            }
        except Exception as e:
            return {"error": str(e), "query": query}

    return mcp


# ---------------------------------------------------------------------------
# Entry point — run as standalone HTTP server
# ---------------------------------------------------------------------------

def main() -> None:
    import argparse

    parser = argparse.ArgumentParser(description="Fincept MCP server for RD-Agent")
    parser.add_argument("--port", type=int, default=18765)
    parser.add_argument("--host", default="127.0.0.1")
    args = parser.parse_args()

    if not MCP_SERVER_AVAILABLE:
        print(json.dumps({"error": "mcp[cli] not installed. Run: pip install mcp[cli]"}))
        raise SystemExit(1)

    server = build_mcp_server()
    print(f"Fincept MCP server starting on http://{args.host}:{args.port}/mcp", flush=True)
    server.run(transport="streamable-http", host=args.host, port=args.port, path="/mcp")


if __name__ == "__main__":
    main()
