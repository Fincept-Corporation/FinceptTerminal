import os
import pandas as pd
import requests

from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.data.cache import get_cache

from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.data.models import (
    CompanyNews,
    CompanyNewsResponse,
    FinancialMetrics,
    FinancialMetricsResponse,
    Price,
    PriceResponse,
    LineItem,
    LineItemResponse,
    InsiderTrade,
    InsiderTradeResponse,
)

_cache = get_cache()

def get_prices(ticker: str, start_date: str, end_date: str) -> list[Price]:
    if cached_data := _cache.get_prices(ticker):
        filtered = [Price(**p) for p in cached_data if start_date <= p["time"] <= end_date]
        if filtered:
            return filtered

    headers = {}
    if api_key := os.environ.get("FINANCIAL_DATASETS_API_KEY"):
        headers["X-API-KEY"] = api_key
    url = f"https://api.financialdatasets.ai/prices/?ticker={ticker}&interval=day&interval_multiplier=1&start_date={start_date}&end_date={end_date}"
    r = requests.get(url, headers=headers)
    if r.status_code != 200:
        raise Exception(f"Error fetching data: {r.status_code} - {r.text}")
    resp = PriceResponse(**r.json())
    prices = resp.prices
    if not prices:
        return []
    _cache.set_prices(ticker, [p.model_dump() for p in prices])
    return prices

def get_financial_metrics(ticker: str, end_date: str, period: str = "ttm", limit: int = 10) -> list[FinancialMetrics]:
    if cached_data := _cache.get_financial_metrics(ticker):
        filtered = [FinancialMetrics(**m) for m in cached_data if m["report_period"] <= end_date]
        filtered.sort(key=lambda x: x.report_period, reverse=True)
        if filtered:
            return filtered[:limit]
    headers = {}
    if api_key := os.environ.get("FINANCIAL_DATASETS_API_KEY"):
        headers["X-API-KEY"] = api_key
    url = f"https://api.financialdatasets.ai/financial-metrics/?ticker={ticker}&report_period_lte={end_date}&limit={limit}&period={period}"
    r = requests.get(url, headers=headers)
    if r.status_code != 200:
        raise Exception(f"Error fetching data: {r.status_code} - {r.text}")
    resp = FinancialMetricsResponse(**r.json())
    fm = resp.financial_metrics
    if not fm:
        return []
    _cache.set_financial_metrics(ticker, [m.model_dump() for m in fm])
    return fm

def search_line_items(ticker: str, line_items: list[str], end_date: str, period: str = "ttm", limit: int = 10) -> list[LineItem]:
    if cached_data := _cache.get_line_items(ticker):
        filtered = [LineItem(**item) for item in cached_data if item["report_period"] <= end_date]
        filtered.sort(key=lambda x: x.report_period, reverse=True)
        if filtered:
            return filtered[:limit]
    headers = {}
    if api_key := os.environ.get("FINANCIAL_DATASETS_API_KEY"):
        headers["X-API-KEY"] = api_key
    url = "https://api.financialdatasets.ai/financials/search/line-items"
    body = {
        "tickers": [ticker],
        "line_items": line_items,
        "end_date": end_date,
        "period": period,
        "limit": limit,
    }
    r = requests.post(url, headers=headers, json=body)
    if r.status_code != 200:
        raise Exception(f"Error fetching data: {r.status_code} - {r.text}")
    data = r.json()
    resp = LineItemResponse(**data)
    results = resp.search_results
    if not results:
        return []
    _cache.set_line_items(ticker, [item.model_dump() for item in results])
    return results[:limit]

def get_insider_trades(ticker: str, end_date: str, start_date: str | None = None, limit: int = 1000) -> list[InsiderTrade]:
    if cached_data := _cache.get_insider_trades(ticker):
        filtered = []
        for trade in cached_data:
            date_field = trade.get("transaction_date") or trade["filing_date"]
            if (start_date is None or date_field >= start_date) and date_field <= end_date:
                filtered.append(InsiderTrade(**trade))
        filtered.sort(key=lambda x: x.transaction_date or x.filing_date, reverse=True)
        if filtered:
            return filtered
    headers = {}
    if api_key := os.environ.get("FINANCIAL_DATASETS_API_KEY"):
        headers["X-API-KEY"] = api_key
    all_trades = []
    current_end = end_date
    while True:
        url = f"https://api.financialdatasets.ai/insider-trades/?ticker={ticker}&filing_date_lte={current_end}"
        if start_date:
            url += f"&filing_date_gte={start_date}"
        url += f"&limit={limit}"
        r = requests.get(url, headers=headers)
        if r.status_code != 200:
            raise Exception(f"Error fetching data: {r.status_code} - {r.text}")
        resp = InsiderTradeResponse(**r.json())
        trades = resp.insider_trades
        if not trades:
            break
        all_trades.extend(trades)
        if not start_date or len(trades) < limit:
            break
        current_end = min(t.filing_date for t in trades).split("T")[0]
        if current_end <= start_date:
            break
    if not all_trades:
        return []
    _cache.set_insider_trades(ticker, [t.model_dump() for t in all_trades])
    return all_trades

def get_company_news(ticker: str, end_date: str, start_date: str | None = None, limit: int = 1000) -> list[CompanyNews]:
    if cached_data := _cache.get_company_news(ticker):
        filtered = []
        for news in cached_data:
            if (start_date is None or news["date"] >= start_date) and news["date"] <= end_date:
                filtered.append(CompanyNews(**news))
        filtered.sort(key=lambda x: x.date, reverse=True)
        if filtered:
            return filtered
    headers = {}
    if api_key := os.environ.get("FINANCIAL_DATASETS_API_KEY"):
        headers["X-API-KEY"] = api_key
    all_news = []
    current_end = end_date
    while True:
        url = f"https://api.financialdatasets.ai/news/?ticker={ticker}&end_date={current_end}"
        if start_date:
            url += f"&start_date={start_date}"
        url += f"&limit={limit}"
        r = requests.get(url, headers=headers)
        if r.status_code != 200:
            raise Exception(f"Error fetching news: {r.status_code} - {r.text}")
        resp = CompanyNewsResponse(**r.json())
        news_list = resp.news
        if not news_list:
            break
        all_news.extend(news_list)
        if not start_date or len(news_list) < limit:
            break
        current_end = min(n.date for n in news_list).split("T")[0]
        if current_end <= start_date:
            break
    if not all_news:
        return []
    _cache.set_company_news(ticker, [n.model_dump() for n in all_news])
    return all_news

def get_market_cap(ticker: str, end_date: str) -> float | None:
    fm = get_financial_metrics(ticker, end_date)
    mc = fm[0].market_cap if fm else None
    return mc


def prices_to_df(prices: list[Price]) -> pd.DataFrame:
    df = pd.DataFrame([p.model_dump() for p in prices])
    df["Date"] = pd.to_datetime(df["time"])
    df.set_index("Date", inplace=True)
    df.rename(columns={
        "Open": "close",   # If you want them all lowercase
        "Close": "close",
        "High": "high",
        "Low": "low",
        "Volume": "volume"
    }, inplace=True)
    df.sort_index(inplace=True)
    return df


def get_historical_data(ticker: str, start_date: str, end_date: str) -> pd.DataFrame:
    p = get_prices(ticker, start_date, end_date)
    return prices_to_df(p)
