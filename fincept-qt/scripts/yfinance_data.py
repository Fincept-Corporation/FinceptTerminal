"""
YFinance Data Fetcher
Fetches real-time stock quotes and historical data using yfinance
Returns JSON output for Qt/C++ integration
"""

import sys
import json
import yfinance as yf
import pandas as pd
from datetime import datetime

def get_quote(symbol):
    """Fetch real-time quote for a single symbol"""
    try:
        import io, contextlib
        ticker = yf.Ticker(symbol)
        _buf = io.StringIO()
        with contextlib.redirect_stdout(_buf):
            info = ticker.info
            hist = ticker.history(period="1d")

        if hist.empty:
            return {"error": "No data available", "symbol": symbol}

        current_price = hist['Close'].iloc[-1]
        previous_close = info.get('previousClose', current_price)
        change = current_price - previous_close
        change_percent = (change / previous_close) * 100 if previous_close else 0

        quote_data = {
            "symbol": symbol,
            "price": round(float(current_price), 2),
            "change": round(float(change), 2),
            "change_percent": round(float(change_percent), 2),
            "volume": int(hist['Volume'].iloc[-1]) if not hist['Volume'].empty else None,
            "high": round(float(hist['High'].iloc[-1]), 2) if not hist['High'].empty else None,
            "low": round(float(hist['Low'].iloc[-1]), 2) if not hist['Low'].empty else None,
            "open": round(float(hist['Open'].iloc[-1]), 2) if not hist['Open'].empty else None,
            "previous_close": round(float(previous_close), 2),
            "timestamp": int(datetime.now().timestamp()),
            "exchange": info.get('exchange', '')
        }

        return quote_data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_historical(symbol, start_date, end_date, interval='1d'):
    """Fetch historical data for a symbol with specified interval

    Args:
        symbol: Stock symbol (e.g., 'AAPL')
        start_date: Start date (YYYY-MM-DD)
        end_date: End date (YYYY-MM-DD)
        interval: Data interval - valid values: 1m, 2m, 5m, 15m, 30m, 60m, 90m, 1h, 1d, 5d, 1wk, 1mo, 3mo
                  Note: Intraday data (< 1d) is only available for last 60 days
    """
    try:
        import io, contextlib
        ticker = yf.Ticker(symbol)
        _buf = io.StringIO()
        with contextlib.redirect_stdout(_buf):
            hist = ticker.history(start=start_date, end=end_date, interval=interval)

        if hist.empty:
            return []

        historical_data = []
        for index, row in hist.iterrows():
            historical_data.append({
                "symbol": symbol,
                "timestamp": int(index.timestamp()),
                "open": round(float(row['Open']), 2),
                "high": round(float(row['High']), 2),
                "low": round(float(row['Low']), 2),
                "close": round(float(row['Close']), 2),
                "volume": int(row['Volume']),
                "adj_close": round(float(row['Close']), 2)
            })

        return historical_data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_historical_price(symbol, target_date):
    """Fetch closing price for a specific date

    Args:
        symbol: Stock symbol (e.g., 'AAPL')
        target_date: Target date (YYYY-MM-DD)

    Returns:
        dict with price info or error
    """
    try:
        from datetime import timedelta

        # Parse target date
        target = datetime.strptime(target_date, '%Y-%m-%d')

        # Fetch a range around the target date (to handle weekends/holidays)
        start = target - timedelta(days=5)
        end = target + timedelta(days=1)

        ticker = yf.Ticker(symbol)
        hist = ticker.history(start=start.strftime('%Y-%m-%d'), end=end.strftime('%Y-%m-%d'), interval='1d')

        if hist.empty:
            return {"found": False, "error": "No data available for this date range", "symbol": symbol}

        # Find the closest date <= target_date
        closest_date = None
        closest_price = None

        for index, row in hist.iterrows():
            idx_date = index.to_pydatetime().replace(tzinfo=None)
            if idx_date.date() <= target.date():
                closest_date = idx_date
                closest_price = round(float(row['Close']), 2)

        if closest_price is None:
            # If no date before or on target, take the first available
            first_row = hist.iloc[0]
            closest_date = hist.index[0].to_pydatetime()
            closest_price = round(float(first_row['Close']), 2)

        return {
            "found": True,
            "symbol": symbol,
            "price": closest_price,
            "date": closest_date.strftime('%Y-%m-%d'),
            "requested_date": target_date
        }
    except Exception as e:
        return {"found": False, "error": str(e), "symbol": symbol}

def get_info(symbol):
    """Fetch company info for a symbol"""
    try:
        ticker = yf.Ticker(symbol)
        info = ticker.info

        # Extract comprehensive information - many more fields available from yfinance
        info_data = {
            "symbol": symbol,
            "company_name": info.get('longName', info.get('shortName', 'N/A')),
            "sector": info.get('sector', 'N/A'),
            "industry": info.get('industry', 'N/A'),
            "market_cap": info.get('marketCap'),
            "pe_ratio": info.get('trailingPE'),
            "forward_pe": info.get('forwardPE'),
            "dividend_yield": info.get('dividendYield'),
            "beta": info.get('beta'),
            "fifty_two_week_high": info.get('fiftyTwoWeekHigh'),
            "fifty_two_week_low": info.get('fiftyTwoWeekLow'),
            "average_volume": info.get('averageVolume'),
            "description": info.get('longBusinessSummary', 'N/A'),
            "website": info.get('website', 'N/A'),
            "country": info.get('country', 'N/A'),
            "currency": info.get('currency', 'USD'),
            "exchange": info.get('exchange', 'N/A'),
            "employees": info.get('fullTimeEmployees'),
            # Additional comprehensive metrics
            "current_price": info.get('currentPrice'),
            "target_high_price": info.get('targetHighPrice'),
            "target_low_price": info.get('targetLowPrice'),
            "target_mean_price": info.get('targetMeanPrice'),
            "recommendation_mean": info.get('recommendationMean'),
            "recommendation_key": info.get('recommendationKey'),
            "number_of_analyst_opinions": info.get('numberOfAnalystOpinions'),
            "total_cash": info.get('totalCash'),
            "total_debt": info.get('totalDebt'),
            "total_revenue": info.get('totalRevenue'),
            "revenue_per_share": info.get('revenuePerShare'),
            "return_on_assets": info.get('returnOnAssets'),
            "return_on_equity": info.get('returnOnEquity'),
            "gross_profits": info.get('grossProfits'),
            "free_cashflow": info.get('freeCashflow'),
            "operating_cashflow": info.get('operatingCashflow'),
            "earnings_growth": info.get('earningsGrowth'),
            "revenue_growth": info.get('revenueGrowth'),
            "gross_margins": info.get('grossMargins'),
            "operating_margins": info.get('operatingMargins'),
            "ebitda_margins": info.get('ebitdaMargins'),
            "profit_margins": info.get('profitMargins'),
            "book_value": info.get('bookValue'),
            "price_to_book": info.get('priceToBook'),
            "enterprise_value": info.get('enterpriseValue'),
            "enterprise_to_revenue": info.get('enterpriseToRevenue'),
            "enterprise_to_ebitda": info.get('enterpriseToEbitda'),
            "shares_outstanding": info.get('sharesOutstanding'),
            "float_shares": info.get('floatShares'),
            "held_percent_insiders": info.get('heldPercentInsiders'),
            "held_percent_institutions": info.get('heldPercentInstitutions'),
            "short_ratio": info.get('shortRatio'),
            "short_percent_of_float": info.get('shortPercentOfFloat'),
            "peg_ratio": info.get('trailingPegRatio'),
            "total_assets": info.get('totalAssets'),
            "total_liabilities": info.get('totalLiab', info.get('totalLiabilities')),
            "book_value_total": (info.get('bookValue') or 0) * (info.get('sharesOutstanding') or 0) if info.get('bookValue') and info.get('sharesOutstanding') else None,
            "timestamp": int(datetime.now().timestamp())
        }

        return info_data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_financials(symbol):
    """Fetch financial statements for a symbol"""
    try:
        ticker = yf.Ticker(symbol)

        # Get income statement, balance sheet, and cash flow
        income_stmt = ticker.financials
        balance_sheet = ticker.balance_sheet
        cash_flow = ticker.cashflow

        def dataframe_to_dict(df):
            """Convert pandas DataFrame to dict with cleaned values"""
            if df is None or df.empty:
                return {}
            result = {}
            for col in df.columns:
                result[str(col)] = {}
                for idx in df.index:
                    value = df.loc[idx, col]
                    if pd.notna(value):
                        result[str(col)][str(idx)] = float(value) if isinstance(value, (int, float)) else str(value)
            return result

        financials_data = {
            "symbol": symbol,
            "income_statement": dataframe_to_dict(income_stmt),
            "balance_sheet": dataframe_to_dict(balance_sheet),
            "cash_flow": dataframe_to_dict(cash_flow),
            "timestamp": int(datetime.now().timestamp())
        }

        return financials_data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_batch_quotes(symbols):
    """Fetch quotes for multiple symbols at once using yfinance batch download"""
    try:
        import io, contextlib
        # Suppress any stdout noise from yfinance (progress bars, deprecation notices)
        # that would corrupt the JSON output parsed by the host
        _buf = io.StringIO()
        with contextlib.redirect_stdout(_buf):
            # Use 5d period to guarantee at least 2 trading days for futures/commodities
            data = yf.download(symbols, period="5d", group_by='ticker', progress=False, threads=True, auto_adjust=True)

        if data is None or data.empty:
            return []

        results = []
        for symbol in symbols:
            try:
                if not isinstance(data.columns, pd.MultiIndex):
                    # Flat columns (rare): use directly
                    hist = data
                else:
                    # MultiIndex columns — detect (Ticker, Price) vs (Price, Ticker)
                    level0 = data.columns.get_level_values(0).unique().tolist()
                    level1 = data.columns.get_level_values(1).unique().tolist()
                    if symbol in level0:
                        # (Ticker, Price) ordering
                        hist = data[symbol]
                    elif symbol in level1:
                        # (Price, Ticker) ordering
                        hist = data.xs(symbol, axis=1, level=1)
                    else:
                        continue

                if hist.empty or hist.dropna(how='all').empty:
                    continue

                hist = hist.dropna(how='all')
                raw_price = hist['Close'].iloc[-1]
                if pd.isna(raw_price):
                    continue
                current_price = float(raw_price)
                # Use previous trading day close for accurate daily change.
                # With period="5d" we always have >= 2 rows for normally-traded instruments.
                raw_prev = hist['Close'].iloc[-2] if len(hist) >= 2 else raw_price
                previous_close = float(raw_prev) if not pd.isna(raw_prev) else current_price
                change = current_price - previous_close
                change_percent = (change / previous_close) * 100 if previous_close else 0

                results.append({
                    "symbol": symbol,
                    "price": round(current_price, 2),
                    "change": round(change, 2),
                    "change_percent": round(change_percent, 2),
                    "volume": int(hist['Volume'].iloc[-1]) if not pd.isna(hist['Volume'].iloc[-1]) else 0,
                    "high": round(float(hist['High'].iloc[-1]), 2) if not pd.isna(hist['High'].iloc[-1]) else None,
                    "low": round(float(hist['Low'].iloc[-1]), 2) if not pd.isna(hist['Low'].iloc[-1]) else None,
                    "open": round(float(hist['Open'].iloc[-1]), 2) if not pd.isna(hist['Open'].iloc[-1]) else None,
                    "previous_close": round(previous_close, 2),
                    "timestamp": int(datetime.now().timestamp()),
                    "exchange": ""
                })
            except Exception:
                continue

        return results
    except Exception as e:
        # Fallback: fetch one by one
        results = []
        for symbol in symbols:
            quote = get_quote(symbol)
            if quote and "error" not in quote:
                results.append(quote)
        return results

def get_batch_sparklines(symbols, period="5d", interval="1h"):
    """Fetch close price series for multiple symbols — used for blotter sparklines.
    Returns: {"AAPL": [170.1, 171.3, ...], "MSFT": [...], ...}
    """
    try:
        import io, contextlib
        _buf = io.StringIO()
        with contextlib.redirect_stdout(_buf):
            data = yf.download(symbols, period=period, interval=interval,
                               group_by='ticker', progress=False, threads=True, auto_adjust=True)

        if data is None or data.empty:
            return {"error": "No data"}

        result = {}
        for symbol in symbols:
            try:
                if not isinstance(data.columns, pd.MultiIndex):
                    hist = data
                else:
                    level0 = data.columns.get_level_values(0).unique().tolist()
                    level1 = data.columns.get_level_values(1).unique().tolist()
                    if symbol in level0:
                        hist = data[symbol]
                    elif symbol in level1:
                        hist = data.xs(symbol, axis=1, level=1)
                    else:
                        continue

                closes = hist['Close'].dropna()
                if closes.empty:
                    continue
                result[symbol] = [round(float(v), 4) for v in closes.tolist()]
            except Exception:
                continue

        return result
    except Exception as e:
        return {"error": str(e)}


def get_batch_all(payload):
    """Unified batch fetcher — returns quotes + sparklines + histories in a single call.

    Called by MarketDataService::refresh() so one DataHub refresh tick spawns a single
    Python process rather than three (quote + sparkline + history). Shares the same
    underlying yf.download paths as the individual endpoints — no behaviour change
    per family, just fewer process spawns.

    payload = {
      "quotes":     [sym, ...],                         # optional
      "sparklines": [sym, ...],                         # optional, 5d/1h
      "histories":  [{"symbol","period","interval"}, ...],  # optional
    }

    Returns:
      {
        "quotes":     [<quote dicts from get_batch_quotes>, ...],
        "sparklines": {sym: [close, ...], ...},
        "histories":  [{"symbol","period","interval","points":[...]}],
      }
    Any family absent from `payload` is omitted from the result.
    """
    out = {}

    quote_syms = payload.get("quotes") or []
    if quote_syms:
        out["quotes"] = get_batch_quotes(quote_syms)

    spark_syms = payload.get("sparklines") or []
    if spark_syms:
        out["sparklines"] = get_batch_sparklines(spark_syms)

    hist_reqs = payload.get("histories") or []
    if hist_reqs:
        histories = []
        for req in hist_reqs:
            try:
                sym = req.get("symbol")
                period = req.get("period", "6mo")
                interval = req.get("interval", "1d")
                if not sym:
                    continue
                points = get_historical_period(sym, period, interval)
                histories.append({
                    "symbol": sym,
                    "period": period,
                    "interval": interval,
                    "points": points if isinstance(points, list) else [],
                })
            except Exception as e:
                histories.append({
                    "symbol": req.get("symbol", ""),
                    "period": req.get("period", ""),
                    "interval": req.get("interval", ""),
                    "points": [],
                    "error": str(e),
                })
        out["histories"] = histories

    return out


def get_company_profile(symbol):
    """Get company profile data formatted for peer comparison"""
    try:
        ticker = yf.Ticker(symbol)
        info = ticker.info

        if not info or 'symbol' not in info:
            return {"error": f"No data found for symbol: {symbol}"}

        # Format data to match FMP structure
        profile = {
            "symbol": info.get("symbol", symbol),
            "companyName": info.get("longName", info.get("shortName", "")),
            "sector": info.get("sector", ""),
            "industry": info.get("industry", ""),
            "website": info.get("website", ""),
            "description": info.get("longBusinessSummary", ""),
            "exchange": info.get("exchange", ""),
            "country": info.get("country", ""),
            "city": info.get("city", ""),
            "address": info.get("address1", ""),
            "phone": info.get("phone", ""),
            "marketCap": info.get("marketCap", 0),
            "employees": info.get("fullTimeEmployees", 0),
            "currency": info.get("currency", "USD"),
            "beta": info.get("beta", 0),
            "price": info.get("currentPrice", info.get("regularMarketPrice", 0)),
            "changes": info.get("regularMarketChangePercent", 0),
        }

        return profile
    except Exception as e:
        return {"error": str(e)}

def get_financial_ratios(symbol):
    """Get financial ratios formatted for peer comparison"""
    try:
        ticker = yf.Ticker(symbol)
        info = ticker.info

        if not info or 'symbol' not in info:
            return {"error": f"No data found for symbol: {symbol}"}

        # Calculate free cash flow per share
        free_cashflow = info.get("freeCashflow", 0)
        shares_outstanding = info.get("sharesOutstanding", 1)
        fcf_per_share = free_cashflow / shares_outstanding if shares_outstanding else 0

        ratios = {
            "symbol": symbol,
            "peRatio": info.get("trailingPE", 0),
            "forwardPE": info.get("forwardPE", 0),
            "priceToBook": info.get("priceToBook", 0),
            "priceToSales": info.get("priceToSalesTrailing12Months", 0),
            "pegRatio": info.get("trailingPegRatio", 0),
            "debtToEquity": info.get("debtToEquity", 0),
            "returnOnEquity": info.get("returnOnEquity", 0),
            "returnOnAssets": info.get("returnOnAssets", 0),
            "profitMargin": info.get("profitMargins", 0),
            "operatingMargin": info.get("operatingMargins", 0),
            "grossMargin": info.get("grossMargins", 0),
            "currentRatio": info.get("currentRatio", 0),
            "quickRatio": info.get("quickRatio", 0),
            "dividendYield": info.get("dividendYield", 0),
            "revenuePerShare": info.get("revenuePerShare", 0),
            "bookValuePerShare": info.get("bookValue", 0),
            "freeCashFlowPerShare": fcf_per_share,
        }

        return ratios
    except Exception as e:
        return {"error": str(e)}

def get_multiple_profiles(symbols):
    """Get company profiles for multiple symbols"""
    results = []
    for symbol in symbols:
        profile = get_company_profile(symbol)
        if profile and "error" not in profile:
            results.append(profile)
    return results

def get_multiple_ratios(symbols):
    """Get financial ratios for multiple symbols"""
    results = []
    for symbol in symbols:
        ratios = get_financial_ratios(symbol)
        if ratios and "error" not in ratios:
            results.append(ratios)
    return results

def search_symbols(query, limit=20):
    """
    Search for stock symbols using yfinance's search API.
    Fast — uses Yahoo Finance's search endpoint instead of brute-force .info calls.
    """
    try:
        from urllib.request import urlopen, Request
        from urllib.parse import quote as url_quote
        import ssl

        results = []
        query_str = query.strip()
        if not query_str:
            return {"results": [], "query": query, "count": 0}

        # Use Yahoo Finance search/autocomplete API (fast, no auth needed)
        url = f"https://query2.finance.yahoo.com/v1/finance/search?q={url_quote(query_str)}&quotesCount={limit}&newsCount=0&enableFuzzyQuery=false&quotesQueryId=tss_match_phrase_query"
        ctx = ssl.create_default_context()
        req = Request(url, headers={"User-Agent": "Mozilla/5.0"})

        try:
            with urlopen(req, timeout=5, context=ctx) as response:
                data = json.loads(response.read().decode())
                quotes = data.get("quotes", [])

                for q in quotes:
                    symbol = q.get("symbol", "")
                    if not symbol:
                        continue
                    results.append({
                        "symbol": symbol,
                        "name": q.get("longname") or q.get("shortname", ""),
                        "exchange": q.get("exchange", ""),
                        "type": q.get("quoteType", "EQUITY"),
                        "currency": q.get("currency", "USD"),
                        "sector": "",
                        "industry": q.get("industry", "")
                    })
        except Exception as search_err:
            # Fallback: try the exact symbol as a yfinance Ticker
            query_upper = query_str.upper()
            suffixes = ["", ".NS", ".BO"]
            for suffix in suffixes:
                candidate = query_upper + suffix
                try:
                    ticker = yf.Ticker(candidate)
                    info = ticker.info
                    if info.get("longName") or info.get("shortName"):
                        results.append({
                            "symbol": candidate,
                            "name": info.get("longName", info.get("shortName", "")),
                            "exchange": info.get("exchange", ""),
                            "type": info.get("quoteType", "EQUITY"),
                            "currency": info.get("currency", "USD"),
                            "sector": info.get("sector", ""),
                            "industry": info.get("industry", "")
                        })
                        break
                except Exception:
                    continue

        return {"results": results, "query": query, "count": len(results)}

    except Exception as e:
        return {"error": str(e), "query": query, "results": []}

def get_news(symbol, count=20):
    """Fetch news articles for a symbol using yfinance"""
    try:
        import re
        ticker = yf.Ticker(symbol)
        raw_news = ticker.news
        if not raw_news:
            return {"articles": [], "symbol": symbol}

        articles = []
        for item in raw_news[:count]:
            content = item.get("content", {})
            title = content.get("title", "")
            if not title:
                continue
            # Clean HTML from summary
            summary = content.get("summary", "")
            summary = re.sub(r'<[^>]+>', '', summary)
            pub_date = content.get("pubDate", "")
            provider = content.get("provider", {})
            publisher = provider.get("displayName", "")
            # Get URL
            url = ""
            click_url = content.get("clickThroughUrl", {})
            if click_url:
                url = click_url.get("url", "")
            if not url:
                canonical = content.get("canonicalUrl", {})
                if canonical:
                    url = canonical.get("url", "")

            articles.append({
                "title": title,
                "description": summary,
                "url": url,
                "publisher": publisher,
                "published_date": pub_date
            })

        return {"articles": articles, "symbol": symbol}
    except Exception as e:
        return {"error": str(e), "symbol": symbol, "articles": []}

def _candidate_yf_symbols(symbol):
    """Generate yfinance ticker candidates for a portfolio symbol.

    Heuristic — try the symbol as-given first, then common non-US exchange
    suffixes. We can't ask the portfolio for currency here, so we just iterate
    until something returns data. Order matters: most likely first.
    """
    s = symbol.strip().upper()
    if not s:
        return []
    # Already has an exchange suffix or is an index/crypto/forex pair → trust it.
    if '.' in s or s.startswith('^') or '-' in s or '=' in s:
        return [s]
    # Bare ticker — try US first, then Toronto, NSE, BSE, LSE, ASX, Hong Kong.
    return [s, f"{s}.TO", f"{s}.NS", f"{s}.BO", f"{s}.L", f"{s}.AX", f"{s}.HK"]


def _resolve_for_history(symbols, period):
    """Resolve every input symbol to a yfinance ticker that returns data
    in the given period, AND return the close-price DataFrame for each.

    Returns: dict[input_symbol] -> (resolved_symbol, closes_series).
    Symbols that fail every candidate are omitted.

    We do this in two passes:
      1. Bulk download all bare symbols + obvious-suffix variants we need to
         cover. yf.download is one HTTP fan-out so this is cheap.
      2. For each input, pick the first candidate that yielded a non-empty
         close series.
    """
    import io, contextlib, logging
    candidates_per_symbol = {s: _candidate_yf_symbols(s) for s in symbols}
    all_candidates = sorted({c for variants in candidates_per_symbol.values() for c in variants})

    if not all_candidates:
        return {}

    # yfinance prints "Failed download" / HTTP error lines straight to stdout
    # AND uses the logging module. Silence both — we report misses via the
    # `missing` key in the returned JSON, and noise on stdout corrupts the
    # JSON the C++ caller is trying to parse.
    yf_logger = logging.getLogger("yfinance")
    prev_level = yf_logger.level
    yf_logger.setLevel(logging.CRITICAL)
    _buf_out = io.StringIO()
    _buf_err = io.StringIO()
    try:
        with contextlib.redirect_stdout(_buf_out), contextlib.redirect_stderr(_buf_err):
            data = yf.download(
                tickers=all_candidates,
                period=period,
                interval="1d",
                group_by="ticker",
                auto_adjust=True,
                progress=False,
                threads=True,
            )
    finally:
        yf_logger.setLevel(prev_level)

    if data is None or data.empty:
        return {}

    single = len(all_candidates) == 1

    def closes_for(ticker):
        try:
            if single:
                return data["Close"].dropna()
            return data[ticker]["Close"].dropna()
        except Exception:
            return None

    resolved = {}
    for s, variants in candidates_per_symbol.items():
        for v in variants:
            ser = closes_for(v)
            if ser is not None and not ser.empty:
                resolved[s] = (v, ser)
                break
    return resolved


def get_portfolio_nav_history(positions, period='6mo'):
    """Reconstruct a daily NAV time series from current positions.

    positions: list of {"symbol": str, "quantity": float} dicts.
    Returns: {"dates": [...], "navs": [...], "resolved": {sym: yf_sym},
              "missing": [sym, ...]}.

    Each input symbol is resolved to a yfinance ticker via _resolve_for_history,
    which tries common exchange suffixes (.TO, .NS, .BO, .L, .AX, .HK) when the
    bare symbol returns no data. Missing closes are forward-filled per symbol so
    a symbol that listed mid-period doesn't drop the whole row.

    NAV on each date = sum(close_i * quantity_i) over all resolved symbols.

    NOTE: this back-projects NAV using CURRENT quantity at every date — it
    does not account for buys/sells inside the window. That's acceptable for
    risk metrics (Beta, MDD) which measure basket variability, not realised P&L.
    """
    try:
        if not positions:
            return {"dates": [], "navs": []}
        symbols = [p["symbol"] for p in positions if p.get("symbol")]
        qty_map = {p["symbol"]: float(p.get("quantity", 0)) for p in positions if p.get("symbol")}
        if not symbols:
            return {"dates": [], "navs": []}

        resolved = _resolve_for_history(symbols, period)
        if not resolved:
            return {"dates": [], "navs": [], "error": "no historical data for any symbol",
                    "missing": symbols}

        nav_series = None
        for sym, (yf_sym, closes) in resolved.items():
            contrib = closes.ffill() * qty_map[sym]
            if nav_series is None:
                nav_series = contrib
            else:
                nav_series = nav_series.add(contrib, fill_value=0)

        if nav_series is None or nav_series.empty:
            return {"dates": [], "navs": [], "error": "no usable closes",
                    "missing": [s for s in symbols if s not in resolved]}

        nav_series = nav_series.dropna()
        dates = [idx.strftime("%Y-%m-%d") for idx in nav_series.index]
        navs = [float(v) for v in nav_series.values]
        return {
            "dates": dates,
            "navs": navs,
            "resolved": {s: yf for s, (yf, _) in resolved.items()},
            "missing": [s for s in symbols if s not in resolved],
        }
    except Exception as e:
        return {"dates": [], "navs": [], "error": str(e)}


def get_portfolio_nav_history_replay(transactions):
    """Reconstruct true daily NAV by replaying transactions over time.

    Unlike get_portfolio_nav_history (which back-projects today's quantities),
    this walks the transaction list and computes position[sym](t) = running
    sum of BUY qty − SELL qty for transactions whose date ≤ t. The chart line
    therefore reflects when capital was actually deployed: it starts at the
    first BUY date (not 1y back from today), steps when buys/sells occur, and
    moves with market prices in between.

    transactions: list of {"date": "YYYY-MM-DD", "symbol": str, "type": str,
                           "quantity": float, "price": float}.
                  Types BUY / SELL drive position changes. DIVIDEND and SPLIT
                  are ignored for now (DIVIDEND adds cash but we don't model
                  cash; SPLIT would multiply qty but is a future addition).

    Returns: {
        "dates":  ["YYYY-MM-DD", ...],   # ascending, trading days only
        "navs":   [float, ...],           # market value of positions on each day
        "costs":  [float, ...],           # cost basis on each day (BUY-WAC, SELL reduces proportionally)
        "resolved": {sym: yf_sym}, "missing": [sym, ...]
    }
    """
    try:
        from datetime import date as _date, timedelta
        import pandas as pd

        if not transactions:
            return {"dates": [], "navs": [], "costs": []}

        # Filter to BUY / SELL only (DIVIDEND / SPLIT don't move position).
        txns = [t for t in transactions
                if str(t.get("type", "")).upper() in ("BUY", "SELL")
                and t.get("symbol") and t.get("date")]
        if not txns:
            return {"dates": [], "navs": [], "costs": []}

        # Sort ascending by date so the replay walk is monotonic.
        txns.sort(key=lambda t: t["date"])

        symbols = sorted({t["symbol"] for t in txns})
        first_date = pd.Timestamp(txns[0]["date"]).normalize()
        today = pd.Timestamp(_date.today()).normalize()
        if first_date > today:
            return {"dates": [], "navs": [], "costs": [],
                    "error": "earliest transaction date is in the future"}

        # Pick a yfinance period string that covers first_date → today with a
        # bit of slack. yfinance period accepts {1mo,3mo,6mo,1y,2y,5y,10y,ytd,max}.
        days_back = (today - first_date).days + 7
        if days_back <= 31:
            period = "1mo"
        elif days_back <= 95:
            period = "3mo"
        elif days_back <= 190:
            period = "6mo"
        elif days_back <= 370:
            period = "1y"
        elif days_back <= 740:
            period = "2y"
        elif days_back <= 1850:
            period = "5y"
        elif days_back <= 3700:
            period = "10y"
        else:
            period = "max"

        resolved = _resolve_for_history(symbols, period)
        if not resolved:
            return {"dates": [], "navs": [], "costs": [],
                    "error": "no historical data for any symbol",
                    "missing": symbols}

        # Build a DataFrame of close prices, columns = symbols, indexed by date.
        # Forward-fill so a symbol that listed mid-period contributes a stable
        # price across non-trading gaps in its own series.
        closes_df = pd.DataFrame({s: ser for s, (_yf, ser) in resolved.items()})
        closes_df = closes_df.sort_index().ffill()

        # Trim the index to the replay window: first_date → today.
        closes_df = closes_df[closes_df.index >= first_date]
        if closes_df.empty:
            return {"dates": [], "navs": [], "costs": [],
                    "error": "no trading days in the requested window"}

        # Walk transactions and the price index in lockstep. For each trading
        # day we know the cumulative position vector — apply close prices to
        # get NAV. Cost basis uses weighted-average cost (WAC): BUYs add to
        # invested capital; SELLs reduce by (sold_qty × current_avg_cost).
        positions = {s: 0.0 for s in symbols}        # qty per symbol
        wac_cost = {s: 0.0 for s in symbols}         # avg cost per share
        cost_basis_total = 0.0                       # running cost basis
        tx_idx = 0
        n_tx = len(txns)

        out_dates = []
        out_navs = []
        out_costs = []

        for ts, row in closes_df.iterrows():
            day = ts.date().isoformat()

            # Apply every transaction whose date ≤ today's trading day.
            while tx_idx < n_tx and txns[tx_idx]["date"] <= day:
                tx = txns[tx_idx]
                sym = tx["symbol"]
                qty = float(tx.get("quantity", 0))
                price = float(tx.get("price", 0))
                kind = str(tx["type"]).upper()
                if sym not in positions:
                    # Symbol has no resolved price history — track position
                    # anyway so we don't desync, but it won't contribute to NAV.
                    positions[sym] = 0.0
                    wac_cost[sym] = 0.0
                if kind == "BUY":
                    new_qty = positions[sym] + qty
                    if new_qty > 0:
                        # Weighted-average cost update.
                        wac_cost[sym] = (positions[sym] * wac_cost[sym] + qty * price) / new_qty
                    positions[sym] = new_qty
                    cost_basis_total += qty * price
                elif kind == "SELL":
                    sell_qty = min(qty, positions[sym])
                    cost_basis_total -= sell_qty * wac_cost[sym]
                    positions[sym] -= sell_qty
                    if positions[sym] <= 1e-9:
                        positions[sym] = 0.0
                        wac_cost[sym] = 0.0
                tx_idx += 1

            # NAV = Σ close × qty for symbols we have prices for on this day.
            nav = 0.0
            for s in symbols:
                if s not in row.index:
                    continue
                close = row[s]
                if pd.isna(close):
                    continue
                nav += float(close) * positions.get(s, 0.0)

            out_dates.append(day)
            out_navs.append(round(nav, 4))
            out_costs.append(round(cost_basis_total, 4))

        return {
            "dates": out_dates,
            "navs": out_navs,
            "costs": out_costs,
            "resolved": {s: yf for s, (yf, _) in resolved.items()},
            "missing": [s for s in symbols if s not in resolved],
        }
    except Exception as e:
        return {"dates": [], "navs": [], "costs": [], "error": str(e)}


def get_historical_period(symbol, period='6mo', interval='1d'):
    """Fetch historical data using a period string (e.g., '1mo', '6mo', '1y', '5y')"""
    try:
        import io, contextlib
        ticker = yf.Ticker(symbol)
        _buf = io.StringIO()
        with contextlib.redirect_stdout(_buf):
            hist = ticker.history(period=period, interval=interval)

        if hist.empty:
            return []

        historical_data = []
        for index, row in hist.iterrows():
            historical_data.append({
                "timestamp": int(index.timestamp()),
                "open": round(float(row['Open']), 2),
                "high": round(float(row['High']), 2),
                "low": round(float(row['Low']), 2),
                "close": round(float(row['Close']), 2),
                "volume": int(row['Volume'])
            })

        return historical_data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def resolve_symbol(symbol):
    """
    Resolve a bare stock symbol to its correct yfinance-compatible form.

    Tries the symbol as-is first, then with common exchange suffixes
    (.NS for NSE India, .BO for BSE India).

    Returns a dict with:
      - resolved_symbol: the working yfinance symbol (e.g., "PIDILITIND.NS")
      - original_symbol: what the user typed (e.g., "PIDILITIND")
      - exchange: detected exchange name (if available)
      - found: bool indicating if any variant returned data
    """
    symbol = symbol.strip().upper()
    if not symbol:
        return {"resolved_symbol": symbol, "original_symbol": symbol, "exchange": "", "found": False}

    # If it already has a suffix, verify it works and return as-is
    if '.' in symbol or symbol.startswith('^') or '-' in symbol or '=' in symbol:
        try:
            ticker = yf.Ticker(symbol)
            hist = ticker.history(period="5d")
            exchange = ticker.info.get("exchange", "") if hasattr(ticker, 'info') else ""
            if not hist.empty:
                return {"resolved_symbol": symbol, "original_symbol": symbol, "exchange": exchange, "found": True}
        except Exception:
            pass
        return {"resolved_symbol": symbol, "original_symbol": symbol, "exchange": "", "found": False}

    # Try bare symbol first (US stocks, crypto, etc.)
    suffixes_to_try = ["", ".NS", ".BO"]
    for suffix in suffixes_to_try:
        candidate = symbol + suffix
        try:
            ticker = yf.Ticker(candidate)
            hist = ticker.history(period="5d")
            if hist is not None and not hist.empty and len(hist) >= 1:
                exchange = ""
                try:
                    exchange = ticker.info.get("exchange", "")
                except Exception:
                    pass
                return {
                    "resolved_symbol": candidate,
                    "original_symbol": symbol,
                    "exchange": exchange,
                    "found": True
                }
        except Exception:
            continue

    # Nothing worked — return original
    return {"resolved_symbol": symbol, "original_symbol": symbol, "exchange": "", "found": False}


def main(args=None):
    # Support both worker pool (args parameter) and subprocess/CLI (sys.argv)
    if args is None:
        args = sys.argv[1:]

    if len(args) < 1:
        return json.dumps({"error": "Usage: python yfinance_data.py <command> <args>"})

    command = args[0]

    if command == "quote":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py quote <symbol>"}
        else:
            symbol = args[1]
            result = get_quote(symbol)

    elif command == "batch_quotes":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py batch_quotes <symbol1> <symbol2> ..."}
        else:
            symbols = args[1:]
            result = get_batch_quotes(symbols)

    elif command == "historical":
        if len(args) < 4:
            result = {"error": "Usage: python yfinance_data.py historical <symbol> <start_date> <end_date> [interval]"}
        else:
            symbol = args[1]
            start_date = args[2]
            end_date = args[3]
            interval = args[4] if len(args) > 4 else '1d'
            result = get_historical(symbol, start_date, end_date, interval)

    elif command == "info":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py info <symbol>"}
        else:
            symbol = args[1]
            result = get_info(symbol)

    elif command == "financials":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py financials <symbol>"}
        else:
            symbol = args[1]
            result = get_financials(symbol)

    elif command == "company_profile":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py company_profile <symbol>"}
        else:
            symbol = args[1]
            result = get_company_profile(symbol)

    elif command == "financial_ratios":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py financial_ratios <symbol>"}
        else:
            symbol = args[1]
            result = get_financial_ratios(symbol)

    elif command == "multiple_profiles":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py multiple_profiles <symbol1,symbol2,...>"}
        else:
            symbols = args[1].split(",")
            result = get_multiple_profiles(symbols)

    elif command == "multiple_ratios":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py multiple_ratios <symbol1,symbol2,...>"}
        else:
            symbols = args[1].split(",")
            result = get_multiple_ratios(symbols)

    elif command == "search":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py search <query> [limit]"}
        else:
            query = args[1]
            limit = int(args[2]) if len(args) > 2 else 50
            result = search_symbols(query, limit)

    elif command == "news":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py news <symbol> [count]"}
        else:
            symbol = args[1]
            count = int(args[2]) if len(args) > 2 else 20
            result = get_news(symbol, count)

    elif command == "portfolio_nav_history":
        # Args layout: <period> <sym1> <qty1> <sym2> <qty2> ...
        # Period defaults to 1y if first arg looks like a symbol.
        if len(args) < 3:
            result = {"error": "Usage: yfinance_data.py portfolio_nav_history <period> <sym1> <qty1> ..."}
        else:
            period = args[1]
            tail = args[2:]
            if len(tail) % 2 != 0:
                result = {"error": "symbol/quantity arguments must be paired"}
            else:
                positions = []
                for i in range(0, len(tail), 2):
                    try:
                        positions.append({"symbol": tail[i], "quantity": float(tail[i + 1])})
                    except ValueError:
                        result = {"error": f"invalid quantity for {tail[i]}"}
                        positions = None
                        break
                if positions is not None:
                    result = get_portfolio_nav_history(positions, period)

    elif command == "portfolio_nav_history_replay":
        # Args layout: <transactions_json_path>
        # The JSON file contains a list of {date, symbol, type, quantity, price}
        # objects. We replay them over time to compute true historical NAV
        # and cost basis instead of back-projecting today's positions.
        if len(args) < 2:
            result = {"error": "Usage: yfinance_data.py portfolio_nav_history_replay <transactions_json_path>"}
        else:
            try:
                with open(args[1], "r", encoding="utf-8") as f:
                    txns = json.load(f)
                if not isinstance(txns, list):
                    result = {"error": "transactions JSON must be a list"}
                else:
                    result = get_portfolio_nav_history_replay(txns)
            except Exception as e:
                result = {"error": f"failed to read transactions JSON: {e}"}

    elif command == "historical_period":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py historical_period <symbol> [period] [interval]"}
        else:
            symbol = args[1]
            period = args[2] if len(args) > 2 else '6mo'
            interval = args[3] if len(args) > 3 else '1d'
            result = get_historical_period(symbol, period, interval)

    elif command == "batch_sparklines":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py batch_sparklines <sym1> <sym2> ..."}
        else:
            symbols = args[1:]
            result = get_batch_sparklines(symbols)

    elif command == "batch_all":
        # Unified hub-refresh endpoint: payload is a single JSON blob on argv[1]
        # (or @tempfile via PythonRunner's arg-spill) describing quotes + sparklines
        # + histories to fetch. One process, one yfinance warm-up, three family
        # results. See MarketDataService::refresh().
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py batch_all <json_payload>"}
        else:
            raw = args[1]
            # Handle PythonRunner arg-spill: args > ~8KB land as "@/tmp/…" and
            # must be read back. Payloads for large symbol lists easily exceed
            # that threshold on Windows (32KB cmdline limit).
            if raw.startswith("@"):
                path = raw[1:]
                try:
                    with open(path, "r", encoding="utf-8") as f:
                        raw = f.read()
                    try:
                        import os as _os
                        _os.remove(path)
                    except Exception:
                        pass
                except Exception as e:
                    result = {"error": f"batch_all: failed to read spill file: {e}"}
                    raw = None
            if raw is not None:
                try:
                    payload = json.loads(raw)
                    result = get_batch_all(payload)
                except json.JSONDecodeError as e:
                    result = {"error": f"batch_all: invalid JSON payload: {e}"}

    elif command == "resolve_symbol":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py resolve_symbol <symbol>"}
        else:
            symbol = args[1]
            result = resolve_symbol(symbol)

    elif command == "historical_price":
        # Get closing price for a specific date
        if len(args) < 3:
            result = {"error": "Usage: python yfinance_data.py historical_price <symbol> <date>"}
        else:
            symbol = args[1]
            target_date = args[2]  # Format: YYYY-MM-DD
            result = get_historical_price(symbol, target_date)

    else:
        result = {"error": f"Unknown command: {command}"}

    # Return JSON for worker pool, print for subprocess/CLI
    # IMPORTANT: Do NOT use indent=2 here. The host subprocess parser
    # looks for the last line starting with '{' or '[' to extract JSON.
    # Pretty-printed JSON puts '{' alone on the first line, breaking parsing.
    output = json.dumps(result)
    print(output)
    return output

# ── Daemon mode ──────────────────────────────────────────────────────────────
#
# Long-lived worker invoked by PythonWorker on the C++ side. Reads length-prefixed
# JSON request frames from stdin, writes length-prefixed JSON response frames to
# stdout. Keeps yfinance + pandas loaded so MarketDataService refreshes don't pay
# the 2–3s import cost on every spawn.
#
# Frame format (stdin + stdout, network byte order):
#   [4 bytes big-endian uint32 length N] [N bytes UTF-8 JSON]
#
# Request JSON:  {"id": <int>, "action": "batch_all"|"batch_quotes"|..., "payload": {...}}
# Response JSON: {"id": <int>, "ok": true, "result": <any>}  or
#                {"id": <int>, "ok": false, "error": "<msg>"}
#
# Special action "shutdown" causes a clean exit. EOF on stdin also exits.

def _daemon_read_frame(stream):
    """Read one length-prefixed frame. Returns bytes payload, or None on EOF."""
    header = b""
    while len(header) < 4:
        chunk = stream.read(4 - len(header))
        if not chunk:
            return None
        header += chunk
    n = int.from_bytes(header, byteorder="big", signed=False)
    if n == 0 or n > 64 * 1024 * 1024:  # sanity: cap frames at 64 MB
        return None
    buf = b""
    while len(buf) < n:
        chunk = stream.read(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


def _daemon_write_frame(stream, data_bytes):
    """Write one length-prefixed frame. `data_bytes` must be bytes."""
    n = len(data_bytes)
    stream.write(n.to_bytes(4, byteorder="big", signed=False))
    stream.write(data_bytes)
    stream.flush()


def _daemon_dispatch(action, payload):
    """Run one action and return the raw result object (not wrapped)."""
    if action == "batch_all":
        return get_batch_all(payload or {})
    if action == "batch_quotes":
        syms = (payload or {}).get("symbols") or []
        return get_batch_quotes(syms)
    if action == "batch_sparklines":
        syms = (payload or {}).get("symbols") or []
        return get_batch_sparklines(syms)
    if action == "historical_period":
        p = payload or {}
        return get_historical_period(
            p.get("symbol"), p.get("period", "6mo"), p.get("interval", "1d"))
    if action == "quote":
        return get_quote((payload or {}).get("symbol"))
    if action == "info":
        return get_info((payload or {}).get("symbol"))
    if action == "news":
        p = payload or {}
        return get_news(p.get("symbol"), p.get("count", 20))
    return {"error": f"Unknown action: {action}"}


def run_daemon():
    """Main daemon loop — read frame, dispatch, write frame, repeat."""
    import io
    # Use the raw binary stdio streams; we do our own framing.
    stdin = sys.stdin.buffer
    stdout = sys.stdout.buffer
    # Ready marker so the C++ host knows imports are done and the worker is
    # ready to accept requests. Uses the same framing.
    try:
        ready = json.dumps({"ready": True, "pid": __import__("os").getpid()}).encode("utf-8")
        _daemon_write_frame(stdout, ready)
    except Exception:
        pass

    while True:
        frame = _daemon_read_frame(stdin)
        if frame is None:
            break  # EOF or bad frame — exit
        try:
            req = json.loads(frame.decode("utf-8"))
        except Exception as e:
            err = {"id": 0, "ok": False, "error": f"bad request JSON: {e}"}
            _daemon_write_frame(stdout, json.dumps(err).encode("utf-8"))
            continue

        req_id = req.get("id", 0)
        action = req.get("action", "")
        if action == "shutdown":
            resp = {"id": req_id, "ok": True, "result": {"shutdown": True}}
            _daemon_write_frame(stdout, json.dumps(resp).encode("utf-8"))
            break

        try:
            result = _daemon_dispatch(action, req.get("payload"))
            resp = {"id": req_id, "ok": True, "result": result}
        except Exception as e:
            resp = {"id": req_id, "ok": False, "error": str(e)}
        try:
            _daemon_write_frame(stdout, json.dumps(resp).encode("utf-8"))
        except Exception:
            break


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--daemon":
        run_daemon()
    else:
        main()
