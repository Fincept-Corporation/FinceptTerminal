
"""Equity Investment Data Providers Module
======================================

Data provider implementations and interfaces

===== DATA SOURCES REQUIRED =====
INPUT:
  - Company financial statements and SEC filings
  - Market price data and trading volume information
  - Industry reports and competitive analysis data
  - Management guidance and analyst estimates
  - Economic indicators affecting equity markets

OUTPUT:
  - Equity valuation models and fair value estimates
  - Fundamental analysis metrics and financial ratios
  - Investment recommendations and target prices
  - Risk assessments and portfolio implications
  - Sector and industry comparative analysis

PARAMETERS:
  - valuation_method: Primary valuation methodology (default: 'DCF')
  - discount_rate: Discount rate for valuation (default: 0.10)
  - terminal_growth: Terminal growth rate assumption (default: 0.025)
  - earnings_multiple: Target earnings multiple (default: 15.0)
  - reporting_currency: Reporting currency (default: 'USD')
"""



import pandas as pd
import numpy as np
import yfinance as yf
import requests
import json
import csv
from typing import Dict, Any, Optional, List, Union
from datetime import datetime, timedelta
from abc import ABC, abstractmethod
import warnings

warnings.filterwarnings('ignore')

from .base_models import (
    DataProvider, CompanyData, MarketData, SecurityType,
    DataProviderError, FinceptAnalyticsError
)


class YahooFinanceProvider(DataProvider):
    """Yahoo Finance data provider implementation"""

    def __init__(self):
        self.name = "Yahoo Finance"
        self.base_url = "https://finance.yahoo.com"

    def get_company_data(self, symbol: str) -> CompanyData:
        """Retrieve comprehensive company data from Yahoo Finance"""
        try:
            ticker = yf.Ticker(symbol)
            info = ticker.info

            # Get current price and basic info
            current_price = info.get('currentPrice') or info.get('regularMarketPrice', 0)
            shares_outstanding = info.get('sharesOutstanding', 0)
            market_cap = info.get('marketCap', current_price * shares_outstanding)

            # Financial data extraction
            financial_data = {
                'revenue': info.get('totalRevenue', 0),
                'net_income': info.get('netIncomeToCommon', 0),
                'total_assets': info.get('totalAssets', 0),
                'total_debt': info.get('totalDebt', 0),
                'book_value': info.get('bookValue', 0),
                'earnings_per_share': info.get('trailingEps', 0),
                'dividend_per_share': info.get('dividendRate', 0),
                'roe': info.get('returnOnEquity', 0),
                'roa': info.get('returnOnAssets', 0),
                'profit_margin': info.get('profitMargins', 0),
                'debt_to_equity': info.get('debtToEquity', 0),
                'current_ratio': info.get('currentRatio', 0),
                'quick_ratio': info.get('quickRatio', 0),
                'ebitda': info.get('ebitda', 0),
                'free_cash_flow': info.get('freeCashflow', 0),
                'operating_cash_flow': info.get('operatingCashflow', 0)
            }

            # Market data extraction
            market_data = {
                'beta': info.get('beta', 1.0),
                'pe_ratio': info.get('trailingPE', 0),
                'forward_pe': info.get('forwardPE', 0),
                'pb_ratio': info.get('priceToBook', 0),
                'ps_ratio': info.get('priceToSalesTrailing12Months', 0),
                'peg_ratio': info.get('pegRatio', 0),
                'dividend_yield': info.get('dividendYield', 0),
                'revenue_growth': info.get('revenueGrowth', 0),
                'earnings_growth': info.get('earningsGrowth', 0),
                '52_week_high': info.get('fiftyTwoWeekHigh', 0),
                '52_week_low': info.get('fiftyTwoWeekLow', 0),
                'average_volume': info.get('averageVolume', 0),
                'float_shares': info.get('floatShares', shares_outstanding)
            }

            return CompanyData(
                symbol=symbol.upper(),
                name=info.get('longName', symbol),
                sector=info.get('sector', 'Unknown'),
                industry=info.get('industry', 'Unknown'),
                market_cap=market_cap,
                shares_outstanding=shares_outstanding,
                current_price=current_price,
                financial_data=financial_data,
                market_data=market_data,
                last_updated=datetime.now()
            )

        except Exception as e:
            raise DataProviderError(f"Failed to retrieve company data for {symbol}: {str(e)}")

    def get_market_data(self, symbol: str) -> MarketData:
        """Retrieve market-specific data for valuation models"""
        try:
            ticker = yf.Ticker(symbol)
            info = ticker.info

            # Get risk-free rate (10-year Treasury)
            treasury = yf.Ticker("^TNX")
            risk_free_rate = treasury.history(period="1d")['Close'].iloc[-1] / 100

            # Get market return (S&P 500 annual return)
            sp500 = yf.Ticker("^GSPC")
            sp500_data = sp500.history(period="1y")
            market_return = (sp500_data['Close'].iloc[-1] / sp500_data['Close'].iloc[0] - 1)

            return MarketData(
                risk_free_rate=risk_free_rate,
                market_return=market_return,
                beta=info.get('beta', 1.0),
                dividend_yield=info.get('dividendYield', 0),
                growth_rate=info.get('earningsGrowth', 0.03),  # Default 3% if not available
                required_return=risk_free_rate + info.get('beta', 1.0) * (market_return - risk_free_rate)
            )

        except Exception as e:
            raise DataProviderError(f"Failed to retrieve market data for {symbol}: {str(e)}")

    def get_financial_statements(self, symbol: str, period: str = "annual") -> Dict[str, pd.DataFrame]:
        """Retrieve financial statements"""
        try:
            ticker = yf.Ticker(symbol)

            if period == "annual":
                income_stmt = ticker.financials
                balance_sheet = ticker.balance_sheet
                cash_flow = ticker.cashflow
            else:
                income_stmt = ticker.quarterly_financials
                balance_sheet = ticker.quarterly_balance_sheet
                cash_flow = ticker.quarterly_cashflow

            return {
                'income_statement': income_stmt,
                'balance_sheet': balance_sheet,
                'cash_flow_statement': cash_flow
            }

        except Exception as e:
            raise DataProviderError(f"Failed to retrieve financial statements for {symbol}: {str(e)}")

    def get_price_data(self, symbol: str, start_date: str, end_date: str) -> pd.DataFrame:
        """Retrieve historical price data"""
        try:
            ticker = yf.Ticker(symbol)
            return ticker.history(start=start_date, end=end_date)
        except Exception as e:
            raise DataProviderError(f"Failed to retrieve price data for {symbol}: {str(e)}")


class AlphaVantageProvider(DataProvider):
    """Alpha Vantage data provider implementation"""

    def __init__(self, api_key: str):
        self.name = "Alpha Vantage"
        self.api_key = api_key
        self.base_url = "https://www.alphavantage.co/query"

    def _make_request(self, params: Dict[str, str]) -> Dict[str, Any]:
        """Make API request to Alpha Vantage"""
        params['apikey'] = self.api_key
        response = requests.get(self.base_url, params=params)
        response.raise_for_status()
        return response.json()

    def get_company_data(self, symbol: str) -> CompanyData:
        """Retrieve company data from Alpha Vantage"""
        try:
            # Get company overview
            overview_params = {
                'function': 'OVERVIEW',
                'symbol': symbol
            }
            overview = self._make_request(overview_params)

            # Get quote data
            quote_params = {
                'function': 'GLOBAL_QUOTE',
                'symbol': symbol
            }
            quote_data = self._make_request(quote_params)
            quote = quote_data.get('Global Quote', {})

            current_price = float(quote.get('05. price', 0))
            shares_outstanding = float(overview.get('SharesOutstanding', 0))

            financial_data = {
                'revenue': float(overview.get('RevenueTTM', 0)),
                'net_income': float(overview.get('ProfitMargin', 0)) * float(overview.get('RevenueTTM', 0)),
                'total_assets': 0,  # Not available in overview
                'book_value': float(overview.get('BookValue', 0)),
                'earnings_per_share': float(overview.get('EPS', 0)),
                'dividend_per_share': float(overview.get('DividendPerShare', 0)),
                'roe': float(overview.get('ReturnOnEquityTTM', 0)),
                'profit_margin': float(overview.get('ProfitMargin', 0)),
                'ebitda': float(overview.get('EBITDA', 0))
            }

            market_data = {
                'beta': float(overview.get('Beta', 1.0)),
                'pe_ratio': float(overview.get('PERatio', 0)),
                'pb_ratio': float(overview.get('PriceToBookRatio', 0)),
                'peg_ratio': float(overview.get('PEGRatio', 0)),
                'dividend_yield': float(overview.get('DividendYield', 0)),
                '52_week_high': float(overview.get('52WeekHigh', 0)),
                '52_week_low': float(overview.get('52WeekLow', 0))
            }

            return CompanyData(
                symbol=symbol.upper(),
                name=overview.get('Name', symbol),
                sector=overview.get('Sector', 'Unknown'),
                industry=overview.get('Industry', 'Unknown'),
                market_cap=float(overview.get('MarketCapitalization', 0)),
                shares_outstanding=shares_outstanding,
                current_price=current_price,
                financial_data=financial_data,
                market_data=market_data,
                last_updated=datetime.now()
            )

        except Exception as e:
            raise DataProviderError(f"Failed to retrieve company data for {symbol}: {str(e)}")

    def get_market_data(self, symbol: str) -> MarketData:
        """Retrieve market data from Alpha Vantage"""
        # Implementation similar to Yahoo Finance but using Alpha Vantage API
        # For brevity, using simplified version
        try:
            overview_params = {
                'function': 'OVERVIEW',
                'symbol': symbol
            }
            overview = self._make_request(overview_params)

            return MarketData(
                risk_free_rate=0.05,  # Default values - would need Treasury API
                market_return=0.10,
                beta=float(overview.get('Beta', 1.0)),
                dividend_yield=float(overview.get('DividendYield', 0)),
                growth_rate=0.03,
                required_return=0.08
            )
        except Exception as e:
            raise DataProviderError(f"Failed to retrieve market data for {symbol}: {str(e)}")

    def get_financial_statements(self, symbol: str, period: str = "annual") -> Dict[str, pd.DataFrame]:
        """Retrieve financial statements from Alpha Vantage"""
        try:
            statements = {}

            # Income Statement
            income_params = {
                'function': 'INCOME_STATEMENT',
                'symbol': symbol
            }
            income_data = self._make_request(income_params)

            if period == "annual":
                statements['income_statement'] = pd.DataFrame(income_data.get('annualReports', []))
            else:
                statements['income_statement'] = pd.DataFrame(income_data.get('quarterlyReports', []))

            # Balance Sheet
            balance_params = {
                'function': 'BALANCE_SHEET',
                'symbol': symbol
            }
            balance_data = self._make_request(balance_params)

            if period == "annual":
                statements['balance_sheet'] = pd.DataFrame(balance_data.get('annualReports', []))
            else:
                statements['balance_sheet'] = pd.DataFrame(balance_data.get('quarterlyReports', []))

            # Cash Flow
            cashflow_params = {
                'function': 'CASH_FLOW',
                'symbol': symbol
            }
            cashflow_data = self._make_request(cashflow_params)

            if period == "annual":
                statements['cash_flow_statement'] = pd.DataFrame(cashflow_data.get('annualReports', []))
            else:
                statements['cash_flow_statement'] = pd.DataFrame(cashflow_data.get('quarterlyReports', []))

            return statements

        except Exception as e:
            raise DataProviderError(f"Failed to retrieve financial statements for {symbol}: {str(e)}")

    def get_price_data(self, symbol: str, start_date: str, end_date: str) -> pd.DataFrame:
        """Retrieve historical price data from Alpha Vantage"""
        try:
            params = {
                'function': 'TIME_SERIES_DAILY_ADJUSTED',
                'symbol': symbol,
                'outputsize': 'full'
            }
            data = self._make_request(params)
            time_series = data.get('Time Series (Daily)', {})

            df = pd.DataFrame.from_dict(time_series, orient='index')
            df.index = pd.to_datetime(df.index)
            df = df.sort_index()

            # Rename columns to match yfinance format
            df.columns = ['Open', 'High', 'Low', 'Close', 'Adj Close', 'Volume', 'Dividend', 'Split']
            df = df.astype(float)

            # Filter by date range
            mask = (df.index >= start_date) & (df.index <= end_date)
            return df.loc[mask]

        except Exception as e:
            raise DataProviderError(f"Failed to retrieve price data for {symbol}: {str(e)}")


class ManualDataProvider(DataProvider):
    """Manual data input provider for user-supplied data"""

    def __init__(self):
        self.name = "Manual Input"
        self.data_cache = {}

    def add_company_data(self, company_data: CompanyData):
        """Add manually input company data"""
        self.data_cache[company_data.symbol] = company_data

    def load_from_csv(self, file_path: str, symbol: str):
        """Load company data from CSV file"""
        try:
            df = pd.read_csv(file_path)

            # Convert CSV data to CompanyData format
            # Assumes specific CSV structure - can be customized
            financial_data = df.to_dict('records')[0] if not df.empty else {}

            company_data = CompanyData(
                symbol=symbol.upper(),
                name=financial_data.get('company_name', symbol),
                sector=financial_data.get('sector', 'Unknown'),
                industry=financial_data.get('industry', 'Unknown'),
                market_cap=float(financial_data.get('market_cap', 0)),
                shares_outstanding=float(financial_data.get('shares_outstanding', 0)),
                current_price=float(financial_data.get('current_price', 0)),
                financial_data=financial_data,
                market_data={},
                last_updated=datetime.now()
            )

            self.add_company_data(company_data)

        except Exception as e:
            raise DataProviderError(f"Failed to load CSV data: {str(e)}")

    def get_company_data(self, symbol: str) -> CompanyData:
        """Retrieve manually input company data"""
        if symbol.upper() not in self.data_cache:
            raise DataProviderError(f"No manual data available for {symbol}")
        return self.data_cache[symbol.upper()]

    def get_market_data(self, symbol: str) -> MarketData:
        """Retrieve market data - requires manual input"""
        # Return default market data or raise error for manual input
        return MarketData(
            risk_free_rate=0.05,
            market_return=0.10,
            beta=1.0,
            dividend_yield=0.02,
            growth_rate=0.03,
            required_return=0.08
        )

    def get_financial_statements(self, symbol: str, period: str = "annual") -> Dict[str, pd.DataFrame]:
        """Manual financial statements not implemented"""
        raise DataProviderError("Manual financial statements input not implemented")

    def get_price_data(self, symbol: str, start_date: str, end_date: str) -> pd.DataFrame:
        """Manual price data not implemented"""
        raise DataProviderError("Manual price data input not implemented")


class DataProviderFactory:
    """Factory class for managing multiple data providers with fallback"""

    def __init__(self):
        self.providers = {}
        self.primary_provider = None
        self.fallback_providers = []

    def register_provider(self, name: str, provider: DataProvider, is_primary: bool = False):
        """Register a data provider"""
        self.providers[name] = provider
        if is_primary:
            self.primary_provider = name
        else:
            self.fallback_providers.append(name)

    def get_provider(self, name: str) -> DataProvider:
        """Get specific provider by name"""
        if name not in self.providers:
            raise DataProviderError(f"Provider {name} not registered")
        return self.providers[name]

    def get_company_data(self, symbol: str, provider_name: Optional[str] = None) -> CompanyData:
        """Get company data with automatic fallback"""
        providers_to_try = [provider_name] if provider_name else [self.primary_provider] + self.fallback_providers

        for provider_name in providers_to_try:
            if provider_name and provider_name in self.providers:
                try:
                    return self.providers[provider_name].get_company_data(symbol)
                except Exception as e:
                    print(f"Provider {provider_name} failed: {str(e)}")
                    continue

        raise DataProviderError(f"All data providers failed for symbol {symbol}")

    def get_market_data(self, symbol: str, provider_name: Optional[str] = None) -> MarketData:
        """Get market data with automatic fallback"""
        providers_to_try = [provider_name] if provider_name else [self.primary_provider] + self.fallback_providers

        for provider_name in providers_to_try:
            if provider_name and provider_name in self.providers:
                try:
                    return self.providers[provider_name].get_market_data(symbol)
                except Exception as e:
                    print(f"Provider {provider_name} failed: {str(e)}")
                    continue

        raise DataProviderError(f"All data providers failed for market data {symbol}")


# Global data provider factory instance
data_factory = DataProviderFactory()


def setup_default_providers(alpha_vantage_key: Optional[str] = None):
    """Setup default data providers"""

    # Register Yahoo Finance as primary
    yahoo_provider = YahooFinanceProvider()
    data_factory.register_provider("yahoo", yahoo_provider, is_primary=True)

    # Register Alpha Vantage if API key provided
    if alpha_vantage_key:
        av_provider = AlphaVantageProvider(alpha_vantage_key)
        data_factory.register_provider("alphavantage", av_provider)

    # Register manual provider
    manual_provider = ManualDataProvider()
    data_factory.register_provider("manual", manual_provider)


def get_company_data(symbol: str, provider: Optional[str] = None) -> CompanyData:
    """Convenience function to get company data"""
    return data_factory.get_company_data(symbol, provider)


def get_market_data(symbol: str, provider: Optional[str] = None) -> MarketData:
    """Convenience function to get market data"""
    return data_factory.get_market_data(symbol, provider)


# Initialize with default providers
setup_default_providers()