"""
Data Fetcher Utility

Unified data fetching for Renaissance Technologies agent.
Supports multiple data sources with fallbacks.
"""

from typing import Dict, Any, Optional, List, Tuple
from dataclasses import dataclass
import json


@dataclass
class StockData:
    """Unified stock data container"""
    ticker: str
    prices: List[float]
    market_cap: float
    avg_daily_volume: Optional[float]
    revenues: Optional[List[float]]
    earnings: Optional[List[float]]
    metrics: Dict[str, Any]
    error: Optional[str] = None


def fetch_stock_data_yfinance(ticker: str, period: str = "1y") -> StockData:
    """
    Fetch stock data using yfinance.

    Args:
        ticker: Stock ticker symbol
        period: Data period (1mo, 3mo, 6mo, 1y, 2y, 5y)

    Returns:
        StockData object with all available data
    """
    try:
        import yfinance as yf

        stock = yf.Ticker(ticker)

        # Get historical prices
        hist = stock.history(period=period)
        if hist.empty:
            return StockData(
                ticker=ticker,
                prices=[],
                market_cap=0,
                avg_daily_volume=None,
                revenues=None,
                earnings=None,
                metrics={},
                error=f"No historical data found for {ticker}"
            )

        # Prices (most recent first)
        prices = hist['Close'].tolist()[::-1]

        # Get info
        info = stock.info

        # Market cap
        market_cap = info.get('marketCap', 0) or 0

        # Average daily volume
        avg_volume_shares = info.get('averageVolume', 0) or 0
        current_price = prices[0] if prices else 0
        avg_daily_volume = avg_volume_shares * current_price

        # Financial metrics
        metrics = {
            'price_to_earnings': info.get('trailingPE'),
            'price_to_book': info.get('priceToBook'),
            'return_on_equity': info.get('returnOnEquity'),
            'return_on_assets': info.get('returnOnAssets'),
            'debt_to_equity': info.get('debtToEquity'),
            'current_ratio': info.get('currentRatio'),
            'profit_margin': info.get('profitMargins'),
            'operating_margin': info.get('operatingMargins'),
            'revenue_growth': info.get('revenueGrowth'),
            'earnings_growth': info.get('earningsGrowth'),
            'dividend_yield': info.get('dividendYield'),
            'beta': info.get('beta'),
            'fifty_two_week_high': info.get('fiftyTwoWeekHigh'),
            'fifty_two_week_low': info.get('fiftyTwoWeekLow'),
            'fifty_day_average': info.get('fiftyDayAverage'),
            'two_hundred_day_average': info.get('twoHundredDayAverage'),
        }

        # Try to get financials
        revenues = None
        earnings = None

        try:
            financials = stock.financials
            if financials is not None and not financials.empty:
                if 'Total Revenue' in financials.index:
                    revenues = financials.loc['Total Revenue'].tolist()
                if 'Net Income' in financials.index:
                    earnings = financials.loc['Net Income'].tolist()
        except Exception:
            pass

        return StockData(
            ticker=ticker,
            prices=prices,
            market_cap=market_cap,
            avg_daily_volume=avg_daily_volume,
            revenues=revenues,
            earnings=earnings,
            metrics=metrics
        )

    except ImportError:
        return StockData(
            ticker=ticker,
            prices=[],
            market_cap=0,
            avg_daily_volume=None,
            revenues=None,
            earnings=None,
            metrics={},
            error="yfinance not installed"
        )
    except Exception as e:
        return StockData(
            ticker=ticker,
            prices=[],
            market_cap=0,
            avg_daily_volume=None,
            revenues=None,
            earnings=None,
            metrics={},
            error=str(e)
        )


def fetch_market_benchmark(period: str = "1y") -> List[float]:
    """
    Fetch market benchmark (SPY) prices.

    Args:
        period: Data period

    Returns:
        List of prices (most recent first)
    """
    try:
        data = fetch_stock_data_yfinance("SPY", period)
        return data.prices
    except Exception:
        return []


def fetch_peer_prices(peer_ticker: str, period: str = "1y") -> List[float]:
    """
    Fetch peer security prices for pairs trading.

    Args:
        peer_ticker: Peer stock ticker
        period: Data period

    Returns:
        List of prices (most recent first)
    """
    try:
        data = fetch_stock_data_yfinance(peer_ticker, period)
        return data.prices
    except Exception:
        return []


def validate_data(data: StockData) -> Tuple[bool, List[str]]:
    """
    Validate fetched data quality.

    Returns:
        Tuple of (is_valid, list of warnings)
    """
    warnings = []
    is_valid = True

    if data.error:
        warnings.append(f"Data error: {data.error}")
        is_valid = False

    if not data.prices:
        warnings.append("No price data available")
        is_valid = False
    elif len(data.prices) < 20:
        warnings.append(f"Limited price history: {len(data.prices)} days")

    if data.market_cap <= 0:
        warnings.append("Market cap not available")

    if data.avg_daily_volume is None or data.avg_daily_volume <= 0:
        warnings.append("Average daily volume not available")

    if not data.revenues:
        warnings.append("Revenue data not available")

    if not data.earnings:
        warnings.append("Earnings data not available")

    # Check for stale data
    if data.prices and len(data.prices) > 1:
        if data.prices[0] == data.prices[1]:
            warnings.append("Prices may be stale (no recent change)")

    return is_valid, warnings


def prepare_analysis_inputs(
    data: StockData,
    trade_value: float = 1_000_000
) -> Dict[str, Any]:
    """
    Prepare inputs for all analysis modules.

    Args:
        data: Fetched stock data
        trade_value: Intended trade value for execution analysis

    Returns:
        Dictionary with prepared inputs for each analysis module
    """
    # Calculate daily volatility from prices
    volatility = 0.02  # Default
    if len(data.prices) >= 20:
        returns = [
            (data.prices[i] / data.prices[i + 1]) - 1
            for i in range(min(20, len(data.prices) - 1))
        ]
        if returns:
            import math
            mean_return = sum(returns) / len(returns)
            variance = sum((r - mean_return) ** 2 for r in returns) / len(returns)
            volatility = math.sqrt(variance)

    return {
        "ticker": data.ticker,
        "prices": data.prices,
        "market_cap": data.market_cap or 10_000_000_000,  # Default $10B
        "avg_daily_volume": data.avg_daily_volume,
        "revenues": data.revenues,
        "earnings": data.earnings,
        "metrics": data.metrics,
        "trade_value": trade_value,
        "volatility": volatility,
    }
