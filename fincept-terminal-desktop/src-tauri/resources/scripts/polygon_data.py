"""
Polygon.io Data Fetcher
Fetches financial market data from Polygon.io API
Returns JSON output for Rust integration
"""

import sys
import json
import os
import requests
import asyncio
from datetime import datetime
from typing import Optional, Dict, Any, List

# Polygon.io API Configuration
POLYGON_API_BASE = "https://api.polygon.io"
POLYGON_API_KEY = os.environ.get('POLYGON_API_KEY', '')  # API key from environment variable

def make_polygon_request(endpoint: str, params: Dict[str, Any]) -> Dict:
    """Make request to Polygon.io API"""
    if not POLYGON_API_KEY:
        return {"error": "Polygon.io API key not configured. Please set POLYGON_API_KEY environment variable."}

    params['apikey'] = POLYGON_API_KEY

    url = f"{POLYGON_API_BASE}{endpoint}"
    try:
        response = requests.get(url, params=params, timeout=10)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        return {"error": str(e)}

def get_all_tickers(
    ticker: Optional[str] = None,
    type: Optional[str] = None,
    market: Optional[str] = None,
    exchange: Optional[str] = None,
    cusip: Optional[str] = None,
    cik: Optional[str] = None,
    date: Optional[str] = None,
    search: Optional[str] = None,
    active: Optional[bool] = None,
    ticker_gte: Optional[str] = None,
    ticker_gt: Optional[str] = None,
    ticker_lte: Optional[str] = None,
    ticker_lt: Optional[str] = None,
    order: Optional[str] = None,
    limit: Optional[int] = None,
    sort: Optional[str] = None
) -> Dict:
    """
    Retrieve a comprehensive list of ticker symbols supported by Polygon.io

    Args:
        ticker: Specify a ticker symbol
        type: Specify the type of the tickers (stocks, crypto, fx, etc.)
        market: Filter by market type (stocks, crypto, fx, otc, indices)
        exchange: Specify the asset's primary exchange Market Identifier Code (MIC)
        cusip: Specify the CUSIP code of the asset
        cik: Specify the CIK of the asset
        date: Specify a point in time to retrieve tickers available on that date (YYYY-MM-DD)
        search: Search for terms within the ticker and/or company name
        active: Specify if the tickers returned should be actively traded (default: true)
        ticker_gte: Range by ticker (greater than or equal)
        ticker_gt: Range by ticker (greater than)
        ticker_lte: Range by ticker (less than or equal)
        ticker_lt: Range by ticker (less than)
        order: Order results based on the sort field
        limit: Limit the number of results returned (default: 100, max: 1000)
        sort: Sort field used for ordering

    Returns:
        Dictionary containing ticker data or error information
    """
    try:
        # Build parameters dictionary, only include non-None values
        params = {}

        if ticker is not None:
            params['ticker'] = ticker
        if type is not None:
            params['type'] = type
        if market is not None:
            params['market'] = market
        if exchange is not None:
            params['exchange'] = exchange
        if cusip is not None:
            params['cusip'] = cusip
        if cik is not None:
            params['cik'] = cik
        if date is not None:
            params['date'] = date
        if search is not None:
            params['search'] = search
        if active is not None:
            params['active'] = str(active).lower()
        if ticker_gte is not None:
            params['ticker.gte'] = ticker_gte
        if ticker_gt is not None:
            params['ticker.gt'] = ticker_gt
        if ticker_lte is not None:
            params['ticker.lte'] = ticker_lte
        if ticker_lt is not None:
            params['ticker.lt'] = ticker_lt
        if order is not None:
            params['order'] = order
        if limit is not None:
            params['limit'] = min(limit, 1000)  # Enforce max limit of 1000
        if sort is not None:
            params['sort'] = sort

        result = make_polygon_request('/v3/reference/tickers', params)

        if 'error' in result:
            return result

        # Extract and format relevant data from response
        tickers = []
        for ticker_data in result.get('results', []):
            ticker_info = {
                'ticker': ticker_data.get('ticker'),
                'name': ticker_data.get('name'),
                'market': ticker_data.get('market'),
                'locale': ticker_data.get('locale'),
                'type': ticker_data.get('type'),
                'active': ticker_data.get('active'),
                'currency_name': ticker_data.get('currency_name'),
                'currency_symbol': ticker_data.get('currency_symbol'),
                'base_currency_name': ticker_data.get('base_currency_name'),
                'base_currency_symbol': ticker_data.get('base_currency_symbol'),
                'primary_exchange': ticker_data.get('primary_exchange'),
                'cik': ticker_data.get('cik'),
                'composite_figi': ticker_data.get('composite_figi'),
                'share_class_figi': ticker_data.get('share_class_figi'),
                'last_updated_utc': ticker_data.get('last_updated_utc'),
                'delisted_utc': ticker_data.get('delisted_utc')
            }
            tickers.append(ticker_info)

        return {
            'success': True,
            'count': result.get('count', len(tickers)),
            'tickers': tickers,
            'next_url': result.get('next_url'),
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": str(e), "function": "get_all_tickers"}

def get_ticker_details(ticker: str, date: Optional[str] = None) -> Dict:
    """
    Retrieve comprehensive details for a single ticker supported by Polygon.io

    Args:
        ticker: The ticker symbol (case-sensitive)
        date: Specify a point in time to get information about the ticker (YYYY-MM-DD)

    Returns:
        Dictionary containing ticker details or error information
    """
    try:
        # Build parameters dictionary
        params = {}

        if date is not None:
            params['date'] = date

        # Make API request to get ticker details
        result = make_polygon_request(f'/v3/reference/tickers/{ticker}', params)

        if 'error' in result:
            return result

        # Extract and format ticker details
        ticker_data = result.get('results', {})

        if not ticker_data:
            return {"error": f"No ticker data found for {ticker}"}

        # Extract address information if available
        address_info = ticker_data.get('address', {})
        address = {
            'address1': address_info.get('address1'),
            'city': address_info.get('city'),
            'state': address_info.get('state'),
            'postal_code': address_info.get('postal_code')
        } if address_info else {}

        # Extract branding information if available
        branding_info = ticker_data.get('branding', {})
        branding = {
            'icon_url': branding_info.get('icon_url'),
            'logo_url': branding_info.get('logo_url')
        } if branding_info else {}

        # Build comprehensive ticker details
        ticker_details = {
            'ticker': ticker_data.get('ticker'),
            'name': ticker_data.get('name'),
            'description': ticker_data.get('description'),
            'market': ticker_data.get('market'),
            'locale': ticker_data.get('locale'),
            'type': ticker_data.get('type'),
            'active': ticker_data.get('active'),
            'currency_name': ticker_data.get('currency_name'),
            'primary_exchange': ticker_data.get('primary_exchange'),
            'cik': ticker_data.get('cik'),
            'composite_figi': ticker_data.get('composite_figi'),
            'share_class_figi': ticker_data.get('share_class_figi'),
            'market_cap': ticker_data.get('market_cap'),
            'homepage_url': ticker_data.get('homepage_url'),
            'phone_number': ticker_data.get('phone_number'),
            'list_date': ticker_data.get('list_date'),
            'delisted_utc': ticker_data.get('delisted_utc'),
            'ticker_root': ticker_data.get('ticker_root'),
            'ticker_suffix': ticker_data.get('ticker_suffix'),
            'round_lot': ticker_data.get('round_lot'),
            'share_class_shares_outstanding': ticker_data.get('share_class_shares_outstanding'),
            'weighted_shares_outstanding': ticker_data.get('weighted_shares_outstanding'),
            'total_employees': ticker_data.get('total_employees'),
            'sic_code': ticker_data.get('sic_code'),
            'sic_description': ticker_data.get('sic_description'),
            'address': address,
            'branding': branding
        }

        return {
            'success': True,
            'ticker_details': ticker_details,
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": str(e), "function": "get_ticker_details", "ticker": ticker}

def get_ticker_types(asset_class: Optional[str] = None, locale: Optional[str] = None) -> Dict:
    """
    Retrieve a list of all ticker types supported by Polygon.io

    Args:
        asset_class: Filter by asset class (stocks, options, crypto, fx, indices)
        locale: Filter by locale (us, global)

    Returns:
        Dictionary containing ticker types or error information
    """
    try:
        # Build parameters dictionary
        params = {}

        if asset_class is not None:
            params['asset_class'] = asset_class
        if locale is not None:
            params['locale'] = locale

        # Make API request to get ticker types
        result = make_polygon_request('/v3/reference/tickers/types', params)

        if 'error' in result:
            return result

        # Extract and format ticker types
        ticker_types = []
        for type_data in result.get('results', []):
            type_info = {
                'code': type_data.get('code'),
                'description': type_data.get('description'),
                'asset_class': type_data.get('asset_class'),
                'locale': type_data.get('locale')
            }
            ticker_types.append(type_info)

        return {
            'success': True,
            'count': result.get('count', len(ticker_types)),
            'ticker_types': ticker_types,
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": str(e), "function": "get_ticker_types"}

def get_related_tickers(ticker: str) -> Dict:
    """
    Retrieve a list of tickers related to a specified ticker, identified through
    an analysis of news coverage and returns data

    Args:
        ticker: The ticker symbol to search

    Returns:
        Dictionary containing related tickers or error information
    """
    try:
        # Make API request to get related companies (Note: this uses v1 API)
        result = make_polygon_request(f'/v1/related-companies/{ticker}', {})

        if 'error' in result:
            return result

        # Extract and format related tickers
        related_tickers = []
        for ticker_data in result.get('results', []):
            related_tickers.append(ticker_data.get('ticker'))

        return {
            'success': True,
            'ticker': ticker,
            'related_tickers': related_tickers,
            'count': len(related_tickers),
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": str(e), "function": "get_related_tickers", "ticker": ticker}

def get_news(
    ticker: Optional[str] = None,
    published_utc: Optional[str] = None,
    ticker_gte: Optional[str] = None,
    ticker_gt: Optional[str] = None,
    ticker_lte: Optional[str] = None,
    ticker_lt: Optional[str] = None,
    published_utc_gte: Optional[str] = None,
    published_utc_gt: Optional[str] = None,
    published_utc_lte: Optional[str] = None,
    published_utc_lt: Optional[str] = None,
    order: Optional[str] = None,
    limit: Optional[int] = None,
    sort: Optional[str] = None
) -> Dict:
    """
    Retrieve the most recent news articles related to a specified ticker, along with
    summaries, source details, and sentiment analysis

    Args:
        ticker: Specify a case-sensitive ticker symbol
        published_utc: Return results published on, before, or after this date
        ticker_gte: Search by ticker (greater than or equal)
        ticker_gt: Search by ticker (greater than)
        ticker_lte: Search by ticker (less than or equal)
        ticker_lt: Search by ticker (less than)
        published_utc_gte: Search by published_utc (greater than or equal)
        published_utc_gt: Search by published_utc (greater than)
        published_utc_lte: Search by published_utc (less than or equal)
        published_utc_lt: Search by published_utc (less than)
        order: Order results based on the sort field
        limit: Limit the number of results returned (default: 10, max: 1000)
        sort: Sort field used for ordering

    Returns:
        Dictionary containing news articles or error information
    """
    try:
        # Build parameters dictionary, only include non-None values
        params = {}

        if ticker is not None:
            params['ticker'] = ticker
        if published_utc is not None:
            params['published_utc'] = published_utc
        if ticker_gte is not None:
            params['ticker.gte'] = ticker_gte
        if ticker_gt is not None:
            params['ticker.gt'] = ticker_gt
        if ticker_lte is not None:
            params['ticker.lte'] = ticker_lte
        if ticker_lt is not None:
            params['ticker.lt'] = ticker_lt
        if published_utc_gte is not None:
            params['published_utc.gte'] = published_utc_gte
        if published_utc_gt is not None:
            params['published_utc.gt'] = published_utc_gt
        if published_utc_lte is not None:
            params['published_utc.lte'] = published_utc_lte
        if published_utc_lt is not None:
            params['published_utc.lt'] = published_utc_lt
        if order is not None:
            params['order'] = order
        if limit is not None:
            params['limit'] = min(limit, 1000)  # Enforce max limit of 1000
        if sort is not None:
            params['sort'] = sort

        # Make API request to get news (Note: this uses v2 API)
        result = make_polygon_request('/v2/reference/news', params)

        if 'error' in result:
            return result

        # Extract and format news articles
        news_articles = []
        for article in result.get('results', []):
            # Extract publisher information if available
            publisher_info = article.get('publisher', {})
            publisher = {
                'name': publisher_info.get('name'),
                'homepage_url': publisher_info.get('homepage_url'),
                'logo_url': publisher_info.get('logo_url'),
                'favicon_url': publisher_info.get('favicon_url')
            } if publisher_info else {}

            # Extract insights information if available
            insights_list = []
            for insight in article.get('insights', []):
                insight_data = {
                    'ticker': insight.get('ticker'),
                    'sentiment': insight.get('sentiment'),
                    'sentiment_reasoning': insight.get('sentiment_reasoning')
                }
                insights_list.append(insight_data)

            # Build article object
            article_data = {
                'id': article.get('id'),
                'title': article.get('title'),
                'description': article.get('description'),
                'author': article.get('author'),
                'published_utc': article.get('published_utc'),
                'article_url': article.get('article_url'),
                'amp_url': article.get('amp_url'),
                'image_url': article.get('image_url'),
                'keywords': article.get('keywords', []),
                'tickers': article.get('tickers', []),
                'publisher': publisher,
                'insights': insights_list
            }
            news_articles.append(article_data)

        return {
            'success': True,
            'count': result.get('count', len(news_articles)),
            'news_articles': news_articles,
            'next_url': result.get('next_url'),
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": str(e), "function": "get_news"}

def get_ipos(
    ticker: Optional[str] = None,
    us_code: Optional[str] = None,
    isin: Optional[str] = None,
    listing_date: Optional[str] = None,
    ipo_status: Optional[str] = None,
    listing_date_gte: Optional[str] = None,
    listing_date_gt: Optional[str] = None,
    listing_date_lte: Optional[str] = None,
    listing_date_lt: Optional[str] = None,
    order: Optional[str] = None,
    limit: Optional[int] = None,
    sort: Optional[str] = None
) -> Dict:
    """
    Retrieve comprehensive information on Initial Public Offerings (IPOs), including
    upcoming and historical events, starting from the year 2008

    Args:
        ticker: Specify a case-sensitive ticker symbol
        us_code: Specify a us_code (unique nine-character alphanumeric code)
        isin: Specify an International Securities Identification Number (ISIN)
        listing_date: Specify a listing date (first trading date)
        ipo_status: Specify an IPO status (direct_listing_process, history, new, pending, postponed, rumor, withdrawn)
        listing_date_gte: Range by listing_date (greater than or equal)
        listing_date_gt: Range by listing_date (greater than)
        listing_date_lte: Range by listing_date (less than or equal)
        listing_date_lt: Range by listing_date (less than)
        order: Order results based on the sort field
        limit: Limit the number of results returned (default: 10, max: 1000)
        sort: Sort field used for ordering

    Returns:
        Dictionary containing IPO data or error information
    """
    try:
        # Build parameters dictionary, only include non-None values
        params = {}

        if ticker is not None:
            params['ticker'] = ticker
        if us_code is not None:
            params['us_code'] = us_code
        if isin is not None:
            params['isin'] = isin
        if listing_date is not None:
            params['listing_date'] = listing_date
        if ipo_status is not None:
            params['ipo_status'] = ipo_status
        if listing_date_gte is not None:
            params['listing_date.gte'] = listing_date_gte
        if listing_date_gt is not None:
            params['listing_date.gt'] = listing_date_gt
        if listing_date_lte is not None:
            params['listing_date.lte'] = listing_date_lte
        if listing_date_lt is not None:
            params['listing_date.lt'] = listing_date_lt
        if order is not None:
            params['order'] = order
        if limit is not None:
            params['limit'] = min(limit, 1000)  # Enforce max limit of 1000
        if sort is not None:
            params['sort'] = sort

        # Make API request to get IPOs (Note: this uses vX API)
        result = make_polygon_request('/vX/reference/ipos', params)

        if 'error' in result:
            return result

        # Extract and format IPO data
        ipos = []
        for ipo_data in result.get('results', []):
            # Build comprehensive IPO object
            ipo_info = {
                'ticker': ipo_data.get('ticker'),
                'issuer_name': ipo_data.get('issuer_name'),
                'security_description': ipo_data.get('security_description'),
                'security_type': ipo_data.get('security_type'),
                'currency_code': ipo_data.get('currency_code'),
                'primary_exchange': ipo_data.get('primary_exchange'),
                'ipo_status': ipo_data.get('ipo_status'),
                'announced_date': ipo_data.get('announced_date'),
                'listing_date': ipo_data.get('listing_date'),
                'issue_start_date': ipo_data.get('issue_start_date'),
                'issue_end_date': ipo_data.get('issue_end_date'),
                'last_updated': ipo_data.get('last_updated'),
                'lowest_offer_price': ipo_data.get('lowest_offer_price'),
                'highest_offer_price': ipo_data.get('highest_offer_price'),
                'final_issue_price': ipo_data.get('final_issue_price'),
                'min_shares_offered': ipo_data.get('min_shares_offered'),
                'max_shares_offered': ipo_data.get('max_shares_offered'),
                'shares_outstanding': ipo_data.get('shares_outstanding'),
                'lot_size': ipo_data.get('lot_size'),
                'total_offer_size': ipo_data.get('total_offer_size'),
                'us_code': ipo_data.get('us_code'),
                'isin': ipo_data.get('isin')
            }
            ipos.append(ipo_info)

        return {
            'success': True,
            'count': len(ipos),
            'ipos': ipos,
            'next_url': result.get('next_url'),
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": str(e), "function": "get_ipos"}

def get_splits(
    ticker: Optional[str] = None,
    execution_date: Optional[str] = None,
    reverse_split: Optional[bool] = None,
    ticker_gte: Optional[str] = None,
    ticker_gt: Optional[str] = None,
    ticker_lte: Optional[str] = None,
    ticker_lt: Optional[str] = None,
    execution_date_gte: Optional[str] = None,
    execution_date_gt: Optional[str] = None,
    execution_date_lte: Optional[str] = None,
    execution_date_lt: Optional[str] = None,
    order: Optional[str] = None,
    limit: Optional[int] = None,
    sort: Optional[str] = None
) -> Dict:
    """
    Retrieve historical stock split events, including execution dates and ratio factors,
    to understand changes in a company's share structure over time

    Args:
        ticker: Specify a case-sensitive ticker symbol
        execution_date: Query by execution date with the format YYYY-MM-DD
        reverse_split: Query for reverse stock splits (split_from > split_to)
        ticker_gte: Range by ticker (greater than or equal)
        ticker_gt: Range by ticker (greater than)
        ticker_lte: Range by ticker (less than or equal)
        ticker_lt: Range by ticker (less than)
        execution_date_gte: Range by execution_date (greater than or equal)
        execution_date_gt: Range by execution_date (greater than)
        execution_date_lte: Range by execution_date (less than or equal)
        execution_date_lt: Range by execution_date (less than)
        order: Order results based on the sort field
        limit: Limit the number of results returned (default: 10, max: 1000)
        sort: Sort field used for ordering

    Returns:
        Dictionary containing stock split data or error information
    """
    try:
        # Build parameters dictionary, only include non-None values
        params = {}

        if ticker is not None:
            params['ticker'] = ticker
        if execution_date is not None:
            params['execution_date'] = execution_date
        if reverse_split is not None:
            params['reverse_split'] = str(reverse_split).lower()
        if ticker_gte is not None:
            params['ticker.gte'] = ticker_gte
        if ticker_gt is not None:
            params['ticker.gt'] = ticker_gt
        if ticker_lte is not None:
            params['ticker.lte'] = ticker_lte
        if ticker_lt is not None:
            params['ticker.lt'] = ticker_lt
        if execution_date_gte is not None:
            params['execution_date.gte'] = execution_date_gte
        if execution_date_gt is not None:
            params['execution_date.gt'] = execution_date_gt
        if execution_date_lte is not None:
            params['execution_date.lte'] = execution_date_lte
        if execution_date_lt is not None:
            params['execution_date.lt'] = execution_date_lt
        if order is not None:
            params['order'] = order
        if limit is not None:
            params['limit'] = min(limit, 1000)  # Enforce max limit of 1000
        if sort is not None:
            params['sort'] = sort

        # Make API request to get stock splits (Note: this uses v3 API)
        result = make_polygon_request('/v3/reference/splits', params)

        if 'error' in result:
            return result

        # Extract and format stock split data
        splits = []
        for split_data in result.get('results', []):
            # Build comprehensive split object
            split_info = {
                'id': split_data.get('id'),
                'ticker': split_data.get('ticker'),
                'execution_date': split_data.get('execution_date'),
                'split_from': split_data.get('split_from'),
                'split_to': split_data.get('split_to'),
                'split_ratio': f"{split_data.get('split_to', 1)}-for-{split_data.get('split_from', 1)}",
                'reverse_split': split_data.get('split_from', 0) > split_data.get('split_to', 0)
            }
            splits.append(split_info)

        return {
            'success': True,
            'count': len(splits),
            'splits': splits,
            'next_url': result.get('next_url'),
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": str(e), "function": "get_splits"}

def get_dividends(
    ticker: Optional[str] = None,
    ex_dividend_date: Optional[str] = None,
    record_date: Optional[str] = None,
    declaration_date: Optional[str] = None,
    pay_date: Optional[str] = None,
    frequency: Optional[int] = None,
    cash_amount: Optional[float] = None,
    dividend_type: Optional[str] = None,
    ticker_gte: Optional[str] = None,
    ticker_gt: Optional[str] = None,
    ticker_lte: Optional[str] = None,
    ticker_lt: Optional[str] = None,
    ex_dividend_date_gte: Optional[str] = None,
    ex_dividend_date_gt: Optional[str] = None,
    ex_dividend_date_lte: Optional[str] = None,
    ex_dividend_date_lt: Optional[str] = None,
    record_date_gte: Optional[str] = None,
    record_date_gt: Optional[str] = None,
    record_date_lte: Optional[str] = None,
    record_date_lt: Optional[str] = None,
    declaration_date_gte: Optional[str] = None,
    declaration_date_gt: Optional[str] = None,
    declaration_date_lte: Optional[str] = None,
    declaration_date_lt: Optional[str] = None,
    pay_date_gte: Optional[str] = None,
    pay_date_gt: Optional[str] = None,
    pay_date_lte: Optional[str] = None,
    pay_date_lt: Optional[str] = None,
    cash_amount_gte: Optional[float] = None,
    cash_amount_gt: Optional[float] = None,
    cash_amount_lte: Optional[float] = None,
    cash_amount_lt: Optional[float] = None,
    order: Optional[str] = None,
    limit: Optional[int] = None,
    sort: Optional[str] = None
) -> Dict:
    """
    Retrieve a historical record of cash dividend distributions for a given ticker,
    including declaration, ex-dividend, record, and pay dates, as well as payout amounts and frequency

    Args:
        ticker: Specify a case-sensitive ticker symbol
        ex_dividend_date: Query by ex-dividend date with the format YYYY-MM-DD
        record_date: Query by record date with the format YYYY-MM-DD
        declaration_date: Query by declaration date with the format YYYY-MM-DD
        pay_date: Query by pay date with the format YYYY-MM-DD
        frequency: Query by the number of times per year the dividend is paid out (0, 1, 2, 4, 12, 24, 52)
        cash_amount: Query by the cash amount of the dividend
        dividend_type: Query by the type of dividend (CD, SC, LT, ST)
        ticker_gte: Range by ticker (greater than or equal)
        ticker_gt: Range by ticker (greater than)
        ticker_lte: Range by ticker (less than or equal)
        ticker_lt: Range by ticker (less than)
        ex_dividend_date_gte: Range by ex_dividend_date (greater than or equal)
        ex_dividend_date_gt: Range by ex_dividend_date (greater than)
        ex_dividend_date_lte: Range by ex_dividend_date (less than or equal)
        ex_dividend_date_lt: Range by ex_dividend_date (less than)
        record_date_gte: Range by record_date (greater than or equal)
        record_date_gt: Range by record_date (greater than)
        record_date_lte: Range by record_date (less than or equal)
        record_date_lt: Range by record_date (less than)
        declaration_date_gte: Range by declaration_date (greater than or equal)
        declaration_date_gt: Range by declaration_date (greater than)
        declaration_date_lte: Range by declaration_date (less than or equal)
        declaration_date_lt: Range by declaration_date (less than)
        pay_date_gte: Range by pay_date (greater than or equal)
        pay_date_gt: Range by pay_date (greater than)
        pay_date_lte: Range by pay_date (less than or equal)
        pay_date_lt: Range by pay_date (less than)
        cash_amount_gte: Range by cash_amount (greater than or equal)
        cash_amount_gt: Range by cash_amount (greater than)
        cash_amount_lte: Range by cash_amount (less than or equal)
        cash_amount_lt: Range by cash_amount (less than)
        order: Order results based on the sort field
        limit: Limit the number of results returned (default: 10, max: 1000)
        sort: Sort field used for ordering

    Returns:
        Dictionary containing dividend data or error information
    """
    try:
        # Build parameters dictionary, only include non-None values
        params = {}

        if ticker is not None:
            params['ticker'] = ticker
        if ex_dividend_date is not None:
            params['ex_dividend_date'] = ex_dividend_date
        if record_date is not None:
            params['record_date'] = record_date
        if declaration_date is not None:
            params['declaration_date'] = declaration_date
        if pay_date is not None:
            params['pay_date'] = pay_date
        if frequency is not None:
            params['frequency'] = frequency
        if cash_amount is not None:
            params['cash_amount'] = cash_amount
        if dividend_type is not None:
            params['dividend_type'] = dividend_type

        # Range parameters
        if ticker_gte is not None:
            params['ticker.gte'] = ticker_gte
        if ticker_gt is not None:
            params['ticker.gt'] = ticker_gt
        if ticker_lte is not None:
            params['ticker.lte'] = ticker_lte
        if ticker_lt is not None:
            params['ticker.lt'] = ticker_lt

        if ex_dividend_date_gte is not None:
            params['ex_dividend_date.gte'] = ex_dividend_date_gte
        if ex_dividend_date_gt is not None:
            params['ex_dividend_date.gt'] = ex_dividend_date_gt
        if ex_dividend_date_lte is not None:
            params['ex_dividend_date.lte'] = ex_dividend_date_lte
        if ex_dividend_date_lt is not None:
            params['ex_dividend_date.lt'] = ex_dividend_date_lt

        if record_date_gte is not None:
            params['record_date.gte'] = record_date_gte
        if record_date_gt is not None:
            params['record_date.gt'] = record_date_gt
        if record_date_lte is not None:
            params['record_date.lte'] = record_date_lte
        if record_date_lt is not None:
            params['record_date.lt'] = record_date_lt

        if declaration_date_gte is not None:
            params['declaration_date.gte'] = declaration_date_gte
        if declaration_date_gt is not None:
            params['declaration_date.gt'] = declaration_date_gt
        if declaration_date_lte is not None:
            params['declaration_date.lte'] = declaration_date_lte
        if declaration_date_lt is not None:
            params['declaration_date.lt'] = declaration_date_lt

        if pay_date_gte is not None:
            params['pay_date.gte'] = pay_date_gte
        if pay_date_gt is not None:
            params['pay_date.gt'] = pay_date_gt
        if pay_date_lte is not None:
            params['pay_date.lte'] = pay_date_lte
        if pay_date_lt is not None:
            params['pay_date.lt'] = pay_date_lt

        if cash_amount_gte is not None:
            params['cash_amount.gte'] = cash_amount_gte
        if cash_amount_gt is not None:
            params['cash_amount.gt'] = cash_amount_gt
        if cash_amount_lte is not None:
            params['cash_amount.lte'] = cash_amount_lte
        if cash_amount_lt is not None:
            params['cash_amount.lt'] = cash_amount_lt

        if order is not None:
            params['order'] = order
        if limit is not None:
            params['limit'] = min(limit, 1000)  # Enforce max limit of 1000
        if sort is not None:
            params['sort'] = sort

        # Make API request to get dividends (Note: this uses v3 API)
        result = make_polygon_request('/v3/reference/dividends', params)

        if 'error' in result:
            return result

        # Extract and format dividend data
        dividends = []
        for dividend_data in result.get('results', []):
            # Build comprehensive dividend object
            dividend_info = {
                'id': dividend_data.get('id'),
                'ticker': dividend_data.get('ticker'),
                'ex_dividend_date': dividend_data.get('ex_dividend_date'),
                'record_date': dividend_data.get('record_date'),
                'declaration_date': dividend_data.get('declaration_date'),
                'pay_date': dividend_data.get('pay_date'),
                'cash_amount': dividend_data.get('cash_amount'),
                'currency': dividend_data.get('currency'),
                'frequency': dividend_data.get('frequency'),
                'dividend_type': dividend_data.get('dividend_type'),
                'frequency_description': _get_frequency_description(dividend_data.get('frequency')),
                'dividend_type_description': _get_dividend_type_description(dividend_data.get('dividend_type'))
            }
            dividends.append(dividend_info)

        return {
            'success': True,
            'count': len(dividends),
            'dividends': dividends,
            'next_url': result.get('next_url'),
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": str(e), "function": "get_dividends"}

def get_ticker_events(identifier: str, types: Optional[str] = None) -> Dict:
    """
    Retrieve a timeline of key events associated with a given ticker, CUSIP, or Composite FIGI.
    This endpoint is experimental and highlights ticker changes, such as symbol renaming or rebranding

    Args:
        identifier: Identifier of an asset (Ticker, CUSIP, or Composite FIGI). Case-sensitive.
        types: A comma-separated list of the types of event to include (currently only ticker_change supported)

    Returns:
        Dictionary containing ticker events or error information
    """
    try:
        # Build parameters dictionary
        params = {}

        if types is not None:
            params['types'] = types

        # Make API request to get ticker events (Note: this uses vX API)
        result = make_polygon_request(f'/vX/reference/tickers/{identifier}/events', params)

        if 'error' in result:
            return result

        # Extract and format ticker events data
        results_data = result.get('results', {})

        if not results_data:
            return {"error": f"No ticker events found for {identifier}"}

        # Extract events
        events = []
        for event_data in results_data.get('events', []):
            # Extract ticker change information if available
            ticker_change = event_data.get('ticker_change', {})

            # Build comprehensive event object
            event_info = {
                'date': event_data.get('date'),
                'type': event_data.get('type'),
                'ticker_change': {
                    'ticker': ticker_change.get('ticker')
                } if ticker_change else None
            }
            events.append(event_info)

        # Build final response
        ticker_events_data = {
            'identifier': identifier,
            'name': results_data.get('name'),
            'events': events,
            'event_count': len(events)
        }

        return {
            'success': True,
            'ticker_events': ticker_events_data,
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": str(e), "function": "get_ticker_events", "identifier": identifier}

def get_exchanges(asset_class: Optional[str] = None, locale: Optional[str] = None) -> Dict:
    """
    Retrieve a list of known exchanges, including their identifiers, names, market types,
    and other relevant attributes

    Args:
        asset_class: Filter by asset class (stocks, options, crypto, fx, futures)
        locale: Filter by locale (us, global)

    Returns:
        Dictionary containing exchange data or error information
    """
    try:
        # Build parameters dictionary
        params = {}

        if asset_class is not None:
            params['asset_class'] = asset_class
        if locale is not None:
            params['locale'] = locale

        # Make API request to get exchanges
        result = make_polygon_request('/v3/reference/exchanges', params)

        if 'error' in result:
            return result

        # Extract and format exchange data
        exchanges = []
        for exchange_data in result.get('results', []):
            # Build comprehensive exchange object
            exchange_info = {
                'id': exchange_data.get('id'),
                'name': exchange_data.get('name'),
                'acronym': exchange_data.get('acronym'),
                'mic': exchange_data.get('mic'),
                'operating_mic': exchange_data.get('operating_mic'),
                'participant_id': exchange_data.get('participant_id'),
                'type': exchange_data.get('type'),
                'asset_class': exchange_data.get('asset_class'),
                'locale': exchange_data.get('locale'),
                'url': exchange_data.get('url')
            }
            exchanges.append(exchange_info)

        return {
            'success': True,
            'count': result.get('count', len(exchanges)),
            'exchanges': exchanges,
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": str(e), "function": "get_exchanges"}

def get_market_holidays() -> Dict:
    """
    Retrieve upcoming market holidays and their corresponding open/close times.
    This endpoint is forward-looking only, listing future holidays that affect market hours

    Returns:
        Dictionary containing market holidays data or error information
    """
    try:
        # Make API request to get market holidays (Note: this uses v1 API)
        result = make_polygon_request('/v1/marketstatus/upcoming', {})

        if 'error' in result:
            return result

        # Extract and format market holidays data
        holidays = []
        for holiday_data in result:
            # Build comprehensive holiday object
            holiday_info = {
                'date': holiday_data.get('date'),
                'exchange': holiday_data.get('exchange'),
                'name': holiday_data.get('name'),
                'status': holiday_data.get('status'),
                'open': holiday_data.get('open'),
                'close': holiday_data.get('close')
            }
            holidays.append(holiday_info)

        return {
            'success': True,
            'count': len(holidays),
            'market_holidays': holidays,
            'status': 'OK'
        }

    except Exception as e:
        return {"error": str(e), "function": "get_market_holidays"}

def get_market_status() -> Dict:
    """
    Retrieve the current trading status for various exchanges and overall financial markets.
    This endpoint provides real-time indicators of whether markets are open, closed, or
    operating in pre-market/after-hours sessions, along with timing details

    Returns:
        Dictionary containing market status data or error information
    """
    try:
        # Make API request to get current market status (Note: this uses v1 API)
        result = make_polygon_request('/v1/marketstatus/now', {})

        if 'error' in result:
            return result

        # Extract and format market status data
        market_status_data = {
            'server_time': result.get('serverTime'),
            'market': result.get('market'),
            'after_hours': result.get('afterHours'),
            'early_hours': result.get('earlyHours'),
            'exchanges': result.get('exchanges', {}),
            'currencies': result.get('currencies', {}),
            'indices_groups': result.get('indicesGroups', {})
        }

        return {
            'success': True,
            'market_status': market_status_data,
            'status': 'OK'
        }

    except Exception as e:
        return {"error": str(e), "function": "get_market_status"}

def get_condition_codes(
    asset_class: Optional[str] = None,
    data_type: Optional[str] = None,
    id: Optional[int] = None,
    sip: Optional[str] = None,
    order: Optional[str] = None,
    limit: Optional[int] = None,
    sort: Optional[str] = None
) -> Dict:
    """
    Retrieve a unified and comprehensive list of trade and quote conditions from various
    upstream market data providers (e.g., CTA, UTP, OPRA, FINRA)

    Args:
        asset_class: Filter for conditions within a given asset class (stocks, options, crypto, fx)
        data_type: Filter by data type (trade, quote, etc.)
        id: Filter for conditions with a given ID
        sip: Filter by SIP (CTA, UTP, OPRA, FINRA)
        order: Order results based on the sort field
        limit: Limit the number of results returned (default: 10, max: 1000)
        sort: Sort field used for ordering

    Returns:
        Dictionary containing condition codes data or error information
    """
    try:
        # Build parameters dictionary, only include non-None values
        params = {}

        if asset_class is not None:
            params['asset_class'] = asset_class
        if data_type is not None:
            params['data_type'] = data_type
        if id is not None:
            params['id'] = id
        if sip is not None:
            params['sip'] = sip
        if order is not None:
            params['order'] = order
        if limit is not None:
            params['limit'] = min(limit, 1000)  # Enforce max limit of 1000
        if sort is not None:
            params['sort'] = sort

        # Make API request to get condition codes (Note: this uses v3 API)
        result = make_polygon_request('/v3/reference/conditions', params)

        if 'error' in result:
            return result

        # Extract and format condition codes data
        conditions = []
        for condition_data in result.get('results', []):
            # Build comprehensive condition object
            condition_info = {
                'id': condition_data.get('id'),
                'name': condition_data.get('name'),
                'abbreviation': condition_data.get('abbreviation'),
                'description': condition_data.get('description'),
                'type': condition_data.get('type'),
                'asset_class': condition_data.get('asset_class'),
                'data_types': condition_data.get('data_types', []),
                'exchange': condition_data.get('exchange'),
                'legacy': condition_data.get('legacy'),
                'sip_mapping': condition_data.get('sip_mapping', {}),
                'update_rules': condition_data.get('update_rules', {})
            }
            conditions.append(condition_info)

        return {
            'success': True,
            'count': result.get('count', len(conditions)),
            'condition_codes': conditions,
            'next_url': result.get('next_url'),
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": str(e), "function": "get_condition_codes"}

def get_single_ticker_snapshot(ticker: str) -> Dict:
    """
    Retrieve the most recent market data snapshot for a single ticker.
    This endpoint consolidates the latest trade, quote, and aggregated data (minute, day, and previous day)

    Args:
        ticker: Specify a case-sensitive ticker symbol (e.g., AAPL for Apple Inc.)

    Returns:
        Dictionary containing ticker snapshot data or error information
    """
    try:
        # Make API request to get single ticker snapshot (Note: this uses v2 API)
        result = make_polygon_request(f'/v2/snapshot/locale/us/markets/stocks/tickers/{ticker}', {})

        if 'error' in result:
            return result

        # Extract ticker data
        ticker_data = result.get('ticker', {})

        if not ticker_data:
            return {"error": f"No snapshot data found for {ticker}"}

        # Build comprehensive snapshot object
        snapshot_data = {
            'ticker': ticker_data.get('ticker'),
            'updated': ticker_data.get('updated'),
            'todays_change': ticker_data.get('todaysChange'),
            'todays_change_perc': ticker_data.get('todaysChangePerc'),

            # Day-level aggregated data
            'day': {
                'open': ticker_data.get('day', {}).get('o'),
                'high': ticker_data.get('day', {}).get('h'),
                'low': ticker_data.get('day', {}).get('l'),
                'close': ticker_data.get('day', {}).get('c'),
                'volume': ticker_data.get('day', {}).get('v'),
                'vwap': ticker_data.get('day', {}).get('vw')
            },

            # Previous day data
            'prev_day': {
                'open': ticker_data.get('prevDay', {}).get('o'),
                'high': ticker_data.get('prevDay', {}).get('h'),
                'low': ticker_data.get('prevDay', {}).get('l'),
                'close': ticker_data.get('prevDay', {}).get('c'),
                'volume': ticker_data.get('prevDay', {}).get('v'),
                'vwap': ticker_data.get('prevDay', {}).get('vw')
            },

            # Most recent trade
            'last_trade': {
                'price': ticker_data.get('lastTrade', {}).get('p'),
                'size': ticker_data.get('lastTrade', {}).get('s'),
                'timestamp': ticker_data.get('lastTrade', {}).get('t'),
                'exchange': ticker_data.get('lastTrade', {}).get('x'),
                'conditions': ticker_data.get('lastTrade', {}).get('c')
            },

            # Most recent quote
            'last_quote': {
                'bid_price': ticker_data.get('lastQuote', {}).get('P'),
                'bid_size': ticker_data.get('lastQuote', {}).get('S'),
                'ask_price': ticker_data.get('lastQuote', {}).get('p'),
                'ask_size': ticker_data.get('lastQuote', {}).get('s'),
                'timestamp': ticker_data.get('lastQuote', {}).get('t'),
                'conditions': ticker_data.get('lastQuote', {}).get('c')
            },

            # Minute-level aggregated data
            'minute': {
                'open': ticker_data.get('min', {}).get('o'),
                'high': ticker_data.get('min', {}).get('h'),
                'low': ticker_data.get('min', {}).get('l'),
                'close': ticker_data.get('min', {}).get('c'),
                'volume': ticker_data.get('min', {}).get('v'),
                'vwap': ticker_data.get('min', {}).get('vw'),
                'num_trades': ticker_data.get('min', {}).get('n'),
                'timestamp': ticker_data.get('min', {}).get('t')
            }
        }

        return {
            'success': True,
            'snapshot': snapshot_data,
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": str(e), "function": "get_single_ticker_snapshot", "ticker": ticker}

def get_full_market_snapshot(tickers: Optional[List[str]] = None, include_otc: bool = False) -> Dict:
    """
    Get a comprehensive snapshot of the entire U.S. stock market or specific tickers

    Args:
        tickers: List of ticker symbols to get snapshots for (None = all tickers)
        include_otc: Include OTC securities in response

    Returns:
        Full market snapshot with comprehensive data for all requested tickers
    """
    try:
        # Build URL with parameters
        url = '/v2/snapshot/locale/us/markets/stocks/tickers'
        params = {}

        if tickers:
            # Join tickers with comma for API
            params['tickers'] = ','.join(tickers)

        # Add OTC parameter
        params['include_otc'] = str(include_otc).lower()

        # Make API request
        result = make_polygon_request(url, params)

        if 'error' in result:
            return result

        # Extract response data
        snapshot_data = {
            "status": result.get("status"),
            "count": result.get("count", 0),
            "request_id": result.get("requestId"),
            "tickers": result.get("tickers", [])
        }

        # Add summary statistics
        tickers_list = snapshot_data["tickers"]
        if tickers_list:
            total_volume = sum(ticker.get("day", {}).get("v", 0) for ticker in tickers_list)
            advancers = sum(1 for ticker in tickers_list if ticker.get("todaysChange", 0) > 0)
            decliners = sum(1 for ticker in tickers_list if ticker.get("todaysChange", 0) < 0)
            unchanged = sum(1 for ticker in tickers_list if ticker.get("todaysChange", 0) == 0)

            snapshot_data["summary"] = {
                "total_volume": total_volume,
                "advancers": advancers,
                "decliners": decliners,
                "unchanged": unchanged,
                "total_processed": len(tickers_list)
            }

        return snapshot_data

    except Exception as e:
        return {"error": f"Failed to get full market snapshot: {str(e)}"}

def get_unified_snapshot(
    ticker: Optional[str] = None,
    type: Optional[str] = None,
    ticker_gte: Optional[str] = None,
    ticker_gt: Optional[str] = None,
    ticker_lte: Optional[str] = None,
    ticker_lt: Optional[str] = None,
    ticker_any_of: Optional[str] = None,
    order: Optional[str] = None,
    limit: Optional[int] = None,
    sort: Optional[str] = None
) -> Dict:
    """
    Retrieve unified snapshots of market data for multiple asset classes including
    stocks, options, forex, and cryptocurrencies in a single request

    Args:
        ticker: Search a range of tickers lexicographically
        type: Query by the type of asset (stocks, options, fx, crypto, indices)
        ticker_gte: Range by ticker (greater than or equal)
        ticker_gt: Range by ticker (greater than)
        ticker_lte: Range by ticker (less than or equal)
        ticker_lt: Range by ticker (less than)
        ticker_any_of: Comma separated list of tickers, up to a maximum of 250
        order: Order results based on the sort field
        limit: Limit the number of results returned (default: 10, max: 250)
        sort: Sort field used for ordering

    Returns:
        Dictionary containing unified snapshot data for multiple asset classes
    """
    try:
        # Build parameters dictionary, only include non-None values
        params = {}

        if ticker is not None:
            params['ticker'] = ticker
        if type is not None:
            params['type'] = type
        if ticker_gte is not None:
            params['ticker.gte'] = ticker_gte
        if ticker_gt is not None:
            params['ticker.gt'] = ticker_gt
        if ticker_lte is not None:
            params['ticker.lte'] = ticker_lte
        if ticker_lt is not None:
            params['ticker.lt'] = ticker_lt
        if ticker_any_of is not None:
            params['ticker.any_of'] = ticker_any_of
        if order is not None:
            params['order'] = order
        if limit is not None:
            params['limit'] = min(limit, 250)  # Enforce max limit of 250
        if sort is not None:
            params['sort'] = sort

        # Make API request to get unified snapshot (Note: this uses v3 API)
        result = make_polygon_request('/v3/snapshot', params)

        if 'error' in result:
            return result

        # Extract and format unified snapshot data
        snapshots = []
        for snapshot_data in result.get('results', []):
            # Build comprehensive snapshot object for different asset types
            snapshot_info = {
                'ticker': snapshot_data.get('ticker'),
                'name': snapshot_data.get('name'),
                'type': snapshot_data.get('type'),
                'market_status': snapshot_data.get('market_status'),
                'timeframe': snapshot_data.get('timeframe'),
                'last_updated': snapshot_data.get('last_updated')
            }

            # Add error information if present
            if snapshot_data.get('error'):
                snapshot_info['error'] = {
                    'code': snapshot_data.get('error'),
                    'message': snapshot_data.get('message')
                }

            # Fair Market Value (Business plans only)
            if snapshot_data.get('fmv') is not None:
                snapshot_info['fair_market_value'] = {
                    'value': snapshot_data.get('fmv'),
                    'last_updated': snapshot_data.get('fmv_last_updated')
                }

            # Session data (trading session metrics)
            session_data = snapshot_data.get('session')
            if session_data:
                snapshot_info['session'] = {
                    'open': session_data.get('open'),
                    'high': session_data.get('high'),
                    'low': session_data.get('low'),
                    'close': session_data.get('close'),
                    'volume': session_data.get('volume'),
                    'previous_close': session_data.get('previous_close'),
                    'change': session_data.get('change'),
                    'change_percent': session_data.get('change_percent'),
                    'early_trading_change': session_data.get('early_trading_change'),
                    'early_trading_change_percent': session_data.get('early_trading_change_percent'),
                    'regular_trading_change': session_data.get('regular_trading_change'),
                    'regular_trading_change_percent': session_data.get('regular_trading_change_percent'),
                    'late_trading_change': session_data.get('late_trading_change'),
                    'late_trading_change_percent': session_data.get('late_trading_change_percent')
                }

            # Last quote data
            last_quote = snapshot_data.get('last_quote')
            if last_quote:
                snapshot_info['last_quote'] = {
                    'bid': last_quote.get('bid'),
                    'bid_size': last_quote.get('bid_size'),
                    'bid_exchange': last_quote.get('bid_exchange'),
                    'ask': last_quote.get('ask'),
                    'ask_size': last_quote.get('ask_size'),
                    'ask_exchange': last_quote.get('ask_exchange'),
                    'midpoint': last_quote.get('midpoint'),
                    'last_updated': last_quote.get('last_updated'),
                    'timeframe': last_quote.get('timeframe')
                }

            # Last trade data
            last_trade = snapshot_data.get('last_trade')
            if last_trade:
                snapshot_info['last_trade'] = {
                    'price': last_trade.get('price'),
                    'size': last_trade.get('size'),
                    'exchange': last_trade.get('exchange'),
                    'conditions': last_trade.get('conditions'),
                    'last_updated': last_trade.get('last_updated'),
                    'sip_timestamp': last_trade.get('sip_timestamp'),
                    'timeframe': last_trade.get('timeframe')
                }

            # Asset type specific data
            asset_type = snapshot_data.get('type')

            if asset_type == 'stocks':
                # Last minute aggregate for stocks
                last_minute = snapshot_data.get('last_minute')
                if last_minute:
                    snapshot_info['last_minute'] = {
                        'open': last_minute.get('open'),
                        'high': last_minute.get('high'),
                        'low': last_minute.get('low'),
                        'close': last_minute.get('close'),
                        'volume': last_minute.get('volume'),
                        'transactions': last_minute.get('transactions'),
                        'vwap': last_minute.get('vwap')
                    }

            elif asset_type == 'options':
                # Options-specific data
                details = snapshot_data.get('details', {})
                if details:
                    snapshot_info['option_details'] = {
                        'contract_type': details.get('contract_type'),
                        'exercise_style': details.get('exercise_style'),
                        'expiration_date': details.get('expiration_date'),
                        'strike_price': details.get('strike_price'),
                        'shares_per_contract': details.get('shares_per_contract'),
                        'underlying_ticker': details.get('underlying_ticker')
                    }

                # Greeks data
                greeks = snapshot_data.get('greeks', {})
                if greeks:
                    snapshot_info['greeks'] = {
                        'delta': greeks.get('delta'),
                        'gamma': greeks.get('gamma'),
                        'theta': greeks.get('theta'),
                        'vega': greeks.get('vega')
                    }

                # Options-specific fields
                snapshot_info['break_even_price'] = snapshot_data.get('break_even_price')
                snapshot_info['implied_volatility'] = snapshot_data.get('implied_volatility')
                snapshot_info['open_interest'] = snapshot_data.get('open_interest')

                # Underlying asset information
                underlying = snapshot_data.get('underlying_asset', {})
                if underlying:
                    snapshot_info['underlying_asset'] = {
                        'ticker': underlying.get('ticker'),
                        'price': underlying.get('price'),
                        'change_to_break_even': underlying.get('change_to_break_even'),
                        'last_updated': underlying.get('last_updated'),
                        'timeframe': underlying.get('timeframe')
                    }

            elif asset_type == 'indices':
                # Index-specific data
                snapshot_info['value'] = snapshot_data.get('value')

            snapshots.append(snapshot_info)

        # Add summary statistics
        asset_type_counts = {}
        successful_requests = 0
        failed_requests = 0

        for snapshot in snapshots:
            asset_type = snapshot.get('type', 'unknown')
            asset_type_counts[asset_type] = asset_type_counts.get(asset_type, 0) + 1

            if 'error' in snapshot:
                failed_requests += 1
            else:
                successful_requests += 1

        return {
            'success': True,
            'count': len(snapshots),
            'snapshots': snapshots,
            'summary': {
                'asset_types': asset_type_counts,
                'successful_requests': successful_requests,
                'failed_requests': failed_requests,
                'total_processed': len(snapshots)
            },
            'next_url': result.get('next_url'),
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": f"Failed to get unified snapshot: {str(e)}"}

def get_top_market_movers(direction: str, include_otc: bool = False) -> Dict:
    """
    Retrieve snapshot data highlighting the top 20 gainers or losers in the U.S. stock market

    Args:
        direction: The direction of market movers to return. Must be either 'gainers' or 'losers'
        include_otc: Include OTC securities in response

    Returns:
        Dictionary containing top market movers data
    """
    try:
        # Validate direction parameter
        if direction.lower() not in ['gainers', 'losers']:
            return {"error": "Direction must be either 'gainers' or 'losers'"}

        # Build URL with parameters
        url = f'/v2/snapshot/locale/us/markets/stocks/{direction.lower()}'
        params = {}

        # Add OTC parameter
        params['include_otc'] = str(include_otc).lower()

        # Make API request
        result = make_polygon_request(url, params)

        if 'error' in result:
            return result

        # Extract response data
        tickers_list = result.get("tickers", [])

        # Enhance ticker data with additional metrics
        enhanced_tickers = []
        for ticker_data in tickers_list:
            # Build comprehensive ticker object
            enhanced_ticker = {
                'ticker': ticker_data.get('ticker'),
                'updated': ticker_data.get('updated'),
                'todays_change': ticker_data.get('todaysChange'),
                'todays_change_perc': ticker_data.get('todaysChangePerc'),

                # Day-level aggregated data
                'day': {
                    'open': ticker_data.get('day', {}).get('o'),
                    'high': ticker_data.get('day', {}).get('h'),
                    'low': ticker_data.get('day', {}).get('l'),
                    'close': ticker_data.get('day', {}).get('c'),
                    'volume': ticker_data.get('day', {}).get('v'),
                    'vwap': ticker_data.get('day', {}).get('vw')
                },

                # Previous day data
                'prev_day': {
                    'open': ticker_data.get('prevDay', {}).get('o'),
                    'high': ticker_data.get('prevDay', {}).get('h'),
                    'low': ticker_data.get('prevDay', {}).get('l'),
                    'close': ticker_data.get('prevDay', {}).get('c'),
                    'volume': ticker_data.get('prevDay', {}).get('v'),
                    'vwap': ticker_data.get('prevDay', {}).get('vw')
                },

                # Most recent trade
                'last_trade': {
                    'price': ticker_data.get('lastTrade', {}).get('p'),
                    'size': ticker_data.get('lastTrade', {}).get('s'),
                    'timestamp': ticker_data.get('lastTrade', {}).get('t'),
                    'exchange': ticker_data.get('lastTrade', {}).get('x'),
                    'conditions': ticker_data.get('lastTrade', {}).get('c')
                },

                # Most recent quote
                'last_quote': {
                    'bid_price': ticker_data.get('lastQuote', {}).get('P'),
                    'bid_size': ticker_data.get('lastQuote', {}).get('S'),
                    'ask_price': ticker_data.get('lastQuote', {}).get('p'),
                    'ask_size': ticker_data.get('lastQuote', {}).get('s'),
                    'timestamp': ticker_data.get('lastQuote', {}).get('t'),
                    'conditions': ticker_data.get('lastQuote', {}).get('c')
                },

                # Minute-level aggregated data
                'minute': {
                    'open': ticker_data.get('min', {}).get('o'),
                    'high': ticker_data.get('min', {}).get('h'),
                    'low': ticker_data.get('min', {}).get('l'),
                    'close': ticker_data.get('min', {}).get('c'),
                    'volume': ticker_data.get('min', {}).get('v'),
                    'vwap': ticker_data.get('min', {}).get('vw'),
                    'num_trades': ticker_data.get('min', {}).get('n'),
                    'timestamp': ticker_data.get('min', {}).get('t')
                }
            }

            # Calculate additional metrics for market movers
            day_volume = ticker_data.get('day', {}).get('v', 0)
            prev_close = ticker_data.get('prevDay', {}).get('c', 0)
            current_price = ticker_data.get('day', {}).get('c', 0)

            # Add calculated fields
            enhanced_ticker['metrics'] = {
                'day_volume': day_volume,
                'previous_close': prev_close,
                'current_price': current_price,
                'price_range': ticker_data.get('day', {}).get('h', 0) - ticker_data.get('day', {}).get('l', 0),
                'spread': ticker_data.get('lastQuote', {}).get('p', 0) - ticker_data.get('lastQuote', {}).get('P', 0)
            }

            # Add ranking information
            enhanced_ticker['rank'] = len(enhanced_tickers) + 1  # 1-based ranking
            enhanced_ticker['direction'] = direction.lower()

            enhanced_tickers.append(enhanced_ticker)

        # Calculate summary statistics
        if enhanced_tickers:
            total_volume = sum(ticker['metrics']['day_volume'] for ticker in enhanced_tickers)
            avg_change = sum(ticker['todays_change_perc'] for ticker in enhanced_tickers) / len(enhanced_tickers)
            biggest_mover = max(enhanced_tickers, key=lambda x: abs(x['todays_change_perc']))

            # Price distribution analysis
            price_ranges = {
                'under_10': len([t for t in enhanced_tickers if t['metrics']['current_price'] < 10]),
                '10_to_50': len([t for t in enhanced_tickers if 10 <= t['metrics']['current_price'] < 50]),
                '50_to_100': len([t for t in enhanced_tickers if 50 <= t['metrics']['current_price'] < 100]),
                'over_100': len([t for t in enhanced_tickers if t['metrics']['current_price'] >= 100])
            }

            summary = {
                'direction': direction.lower(),
                'total_volume': total_volume,
                'average_change_percent': round(avg_change, 2),
                'biggest_mover': {
                    'ticker': biggest_mover['ticker'],
                    'change_percent': biggest_mover['todays_change_perc']
                },
                'price_distribution': price_ranges,
                'movers_count': len(enhanced_tickers),
                'include_otc': include_otc
            }
        else:
            summary = {
                'direction': direction.lower(),
                'movers_count': 0,
                'include_otc': include_otc,
                'message': f"No top {direction.lower()} found at this time"
            }

        return {
            'success': True,
            'direction': direction.lower(),
            'status': result.get('status'),
            'tickers': enhanced_tickers,
            'summary': summary,
            'request_id': result.get('request_id')
        }

    except Exception as e:
        return {"error": f"Failed to get top market movers: {str(e)}"}

def get_trades(
    stock_ticker: str,
    timestamp: Optional[str] = None,
    timestamp_gte: Optional[str] = None,
    timestamp_gt: Optional[str] = None,
    timestamp_lte: Optional[str] = None,
    timestamp_lt: Optional[str] = None,
    order: Optional[str] = None,
    limit: Optional[int] = None,
    sort: Optional[str] = None
) -> Dict:
    """
    Retrieve comprehensive, tick-level trade data for a specified stock ticker within a defined time range

    Args:
        stock_ticker: Case-sensitive ticker symbol (e.g., AAPL)
        timestamp: Query by trade timestamp. Either a date with the format YYYY-MM-DD or a nanosecond timestamp
        timestamp_gte: Range by timestamp (greater than or equal)
        timestamp_gt: Range by timestamp (greater than)
        timestamp_lte: Range by timestamp (less than or equal)
        timestamp_lt: Range by timestamp (less than)
        order: Order results based on the sort field
        limit: Limit the number of results returned (default: 1000, max: 50000)
        sort: Sort field used for ordering

    Returns:
        Dictionary containing tick-level trade data
    """
    try:
        if not stock_ticker:
            return {"error": "Stock ticker is required"}

        # Build parameters dictionary, only include non-None values
        params = {}

        if timestamp is not None:
            params['timestamp'] = timestamp
        if timestamp_gte is not None:
            params['timestamp.gte'] = timestamp_gte
        if timestamp_gt is not None:
            params['timestamp.gt'] = timestamp_gt
        if timestamp_lte is not None:
            params['timestamp.lte'] = timestamp_lte
        if timestamp_lt is not None:
            params['timestamp.lt'] = timestamp_lt
        if order is not None:
            params['order'] = order
        if limit is not None:
            params['limit'] = min(limit, 50000)  # Enforce max limit of 50000
        if sort is not None:
            params['sort'] = sort

        # Make API request to get trades (Note: this uses v3 API)
        result = make_polygon_request(f'/v3/trades/{stock_ticker}', params)

        if 'error' in result:
            return result

        # Extract and format trade data
        trades = []
        for trade_data in result.get('results', []):
            # Build comprehensive trade object
            trade_info = {
                'trade_id': trade_data.get('id'),
                'ticker': stock_ticker,
                'price': trade_data.get('price'),
                'size': trade_data.get('size'),
                'exchange': trade_data.get('exchange'),
                'tape': trade_data.get('tape'),
                'sequence_number': trade_data.get('sequence_number'),
                'conditions': trade_data.get('conditions', []),
                'correction': trade_data.get('correction'),
                'trf_id': trade_data.get('trf_id'),

                # Various timestamps with different precision levels
                'participant_timestamp': trade_data.get('participant_timestamp'),
                'sip_timestamp': trade_data.get('sip_timestamp'),
                'trf_timestamp': trade_data.get('trf_timestamp')
            }

            # Calculate additional metrics
            trade_volume = trade_data.get('size', 0)
            trade_price = trade_data.get('price', 0)
            trade_value = trade_volume * trade_price

            # Add calculated fields
            trade_info['metrics'] = {
                'trade_volume': trade_volume,
                'trade_value': trade_value,
                'trade_conditions_count': len(trade_data.get('conditions', [])),
                'exchange_id': trade_data.get('exchange'),
                'tape_id': trade_data.get('tape')
            }

            # Add human-readable tape information
            tape_map = {1: 'A (NYSE)', 2: 'B (NYSE ARCA)', 3: 'C (NASDAQ)'}
            tape_id = trade_data.get('tape')
            if tape_id in tape_map:
                trade_info['tape_description'] = tape_map[tape_id]

            trades.append(trade_info)

        # Calculate summary statistics
        if trades:
            total_volume = sum(trade['metrics']['trade_volume'] for trade in trades)
            total_value = sum(trade['metrics']['trade_value'] for trade in trades)
            avg_price = sum(trade['price'] for trade in trades) / len(trades)
            avg_size = total_volume / len(trades)

            # Price range analysis
            prices = [trade['price'] for trade in trades]
            min_price = min(prices)
            max_price = max(prices)
            price_range = max_price - min_price

            # Exchange distribution
            exchange_counts = {}
            tape_counts = {}
            for trade in trades:
                exchange = trade['metrics']['exchange_id']
                tape = trade['metrics']['tape_id']
                exchange_counts[exchange] = exchange_counts.get(exchange, 0) + 1
                tape_counts[tape] = tape_counts.get(tape, 0) + 1

            # Time range analysis
            timestamps = [trade['sip_timestamp'] for trade in trades if trade.get('sip_timestamp')]
            if timestamps:
                start_time = min(timestamps)
                end_time = max(timestamps)
                time_range_ns = end_time - start_time
                time_range_seconds = time_range_ns / 1_000_000_000 if time_range_ns > 0 else 0
            else:
                start_time = None
                end_time = None
                time_range_seconds = 0

            summary = {
                'ticker': stock_ticker,
                'trade_count': len(trades),
                'total_volume': total_volume,
                'total_value': total_value,
                'average_price': round(avg_price, 4),
                'average_size': round(avg_size, 2),
                'min_price': min_price,
                'max_price': max_price,
                'price_range': round(price_range, 4),
                'time_range_seconds': round(time_range_seconds, 3),
                'start_time': start_time,
                'end_time': end_time,
                'exchange_distribution': exchange_counts,
                'tape_distribution': tape_counts,
                'most_active_exchange': max(exchange_counts.items(), key=lambda x: x[1])[0] if exchange_counts else None
            }
        else:
            summary = {
                'ticker': stock_ticker,
                'trade_count': 0,
                'message': f"No trades found for {stock_ticker} with specified criteria"
            }

        return {
            'success': True,
            'ticker': stock_ticker,
            'trades': trades,
            'summary': summary,
            'next_url': result.get('next_url'),
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": f"Failed to get trades for {stock_ticker}: {str(e)}"}

def get_last_trade(stocks_ticker: str) -> Dict:
    """
    Retrieve the latest available trade for a specified stock ticker, including details such as price, size, exchange, and timestamp

    Args:
        stocks_ticker: Case-sensitive ticker symbol (e.g., AAPL)

    Returns:
        Dictionary containing the most recent trade data
    """
    try:
        if not stocks_ticker:
            return {"error": "Stock ticker is required"}

        # Make API request to get last trade (Note: this uses v2 API)
        result = make_polygon_request(f'/v2/last/trade/{stocks_ticker}', {})

        if 'error' in result:
            return result

        # Extract trade data
        trade_data = result.get('results')

        if not trade_data:
            return {"error": f"No last trade data found for {stocks_ticker}"}

        # Build comprehensive last trade object
        last_trade = {
            'ticker': trade_data.get('T'),
            'trade_id': trade_data.get('i'),
            'price': trade_data.get('p'),
            'size': trade_data.get('s'),
            'exchange_id': trade_data.get('x'),
            'tape_id': trade_data.get('z'),
            'sequence_number': trade_data.get('q'),
            'conditions': trade_data.get('c', []),
            'correction_indicator': trade_data.get('e'),
            'trf_id': trade_data.get('r'),

            # Various timestamps with different precision levels
            'sip_timestamp': trade_data.get('t'),
            'trf_timestamp': trade_data.get('f'),
            'participant_timestamp': trade_data.get('y')
        }

        # Add human-readable tape information
        tape_map = {1: 'A (NYSE)', 2: 'B (NYSE ARCA)', 3: 'C (NASDAQ)'}
        tape_id = trade_data.get('z')
        if tape_id in tape_map:
            last_trade['tape_description'] = tape_map[tape_id]

        # Calculate trade value
        trade_price = trade_data.get('p', 0)
        trade_size = trade_data.get('s', 0)
        trade_value = trade_price * trade_size

        # Add calculated fields
        last_trade['metrics'] = {
            'trade_value': trade_value,
            'trade_conditions_count': len(trade_data.get('c', [])),
            'exchange_id': trade_data.get('x'),
            'tape_id': trade_data.get('z'),
            'trf_id': trade_data.get('r')
        }

        # Add timestamp analysis
        sip_timestamp = trade_data.get('t')
        participant_timestamp = trade_data.get('y')
        trf_timestamp = trade_data.get('f')

        # Calculate time differences (if all timestamps are available)
        timestamp_analysis = {}
        if sip_timestamp and participant_timestamp:
            latency_sip_participant = sip_timestamp - participant_timestamp
            timestamp_analysis['sip_to_participant_latency_ns'] = latency_sip_participant
            timestamp_analysis['sip_to_participant_latency_ms'] = latency_sip_participant / 1_000_000

        if trf_timestamp and sip_timestamp:
            latency_trf_sip = sip_timestamp - trf_timestamp
            timestamp_analysis['trf_to_sip_latency_ns'] = latency_trf_sip
            timestamp_analysis['trf_to_sip_latency_ms'] = latency_trf_sip / 1_000_000

        if participant_timestamp and trf_timestamp:
            latency_participant_trf = participant_timestamp - trf_timestamp
            timestamp_analysis['participant_to_trf_latency_ns'] = latency_participant_trf
            timestamp_analysis['participant_to_trf_latency_ms'] = latency_participant_trf / 1_000_000

        last_trade['timestamp_analysis'] = timestamp_analysis

        # Add condition codes interpretation
        conditions = trade_data.get('c', [])
        if conditions:
            last_trade['conditions_info'] = {
                'condition_codes': conditions,
                'conditions_count': len(conditions),
                'has_conditions': True
            }
        else:
            last_trade['conditions_info'] = {
                'condition_codes': [],
                'conditions_count': 0,
                'has_conditions': False
            }

        # Add exchange information placeholder
        last_trade['exchange_info'] = {
            'exchange_id': trade_data.get('x'),
            'is_regular_hours': True,  # Could be enhanced with market hours logic
            'tape_id': tape_id,
            'tape_description': last_trade.get('tape_description')
        }

        # Add market status context (based on timestamp)
        if sip_timestamp:
            # Convert nanosecond timestamp to datetime for analysis
            try:
                import datetime
                trade_time = datetime.datetime.fromtimestamp(sip_timestamp / 1_000_000_000)
                last_trade['trade_time_formatted'] = trade_time.strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]  # Millisecond precision
                last_trade['trade_date'] = trade_time.strftime('%Y-%m-%d')
                last_trade['trade_time'] = trade_time.strftime('%H:%M:%S')
            except (ValueError, OSError):
                # Handle timestamp conversion errors
                pass

        return {
            'success': True,
            'ticker': stocks_ticker,
            'last_trade': last_trade,
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": f"Failed to get last trade for {stocks_ticker}: {str(e)}"}

def get_quotes(
    stock_ticker: str,
    timestamp: Optional[str] = None,
    timestamp_gte: Optional[str] = None,
    timestamp_gt: Optional[str] = None,
    timestamp_lte: Optional[str] = None,
    timestamp_lt: Optional[str] = None,
    order: Optional[str] = None,
    limit: Optional[int] = None,
    sort: Optional[str] = None
) -> Dict:
    """
    Retrieve National Best Bid and Offer (NBBO) quotes for a specified stock ticker over a defined time range

    Args:
        stock_ticker: Case-sensitive ticker symbol (e.g., AAPL)
        timestamp: Query by timestamp. Either a date with the format YYYY-MM-DD or a nanosecond timestamp
        timestamp_gte: Range by timestamp (greater than or equal)
        timestamp_gt: Range by timestamp (greater than)
        timestamp_lte: Range by timestamp (less than or equal)
        timestamp_lt: Range by timestamp (less than)
        order: Order results based on the sort field
        limit: Limit the number of results returned (default: 1000, max: 50000)
        sort: Sort field used for ordering

    Returns:
        Dictionary containing NBBO quote data
    """
    try:
        if not stock_ticker:
            return {"error": "Stock ticker is required"}

        # Build parameters dictionary, only include non-None values
        params = {}

        if timestamp is not None:
            params['timestamp'] = timestamp
        if timestamp_gte is not None:
            params['timestamp.gte'] = timestamp_gte
        if timestamp_gt is not None:
            params['timestamp.gt'] = timestamp_gt
        if timestamp_lte is not None:
            params['timestamp.lte'] = timestamp_lte
        if timestamp_lt is not None:
            params['timestamp.lt'] = timestamp_lt
        if order is not None:
            params['order'] = order
        if limit is not None:
            params['limit'] = min(limit, 50000)  # Enforce max limit of 50000
        if sort is not None:
            params['sort'] = sort

        # Make API request to get quotes (Note: this uses v3 API)
        result = make_polygon_request(f'/v3/quotes/{stock_ticker}', params)

        if 'error' in result:
            return result

        # Extract and format quote data
        quotes = []
        for quote_data in result.get('results', []):
            # Build comprehensive quote object
            quote_info = {
                'ticker': stock_ticker,
                'bid_price': quote_data.get('bid_price'),
                'bid_size': quote_data.get('bid_size'),
                'bid_exchange': quote_data.get('bid_exchange'),
                'ask_price': quote_data.get('ask_price'),
                'ask_size': quote_data.get('ask_size'),
                'ask_exchange': quote_data.get('ask_exchange'),
                'tape': quote_data.get('tape'),
                'sequence_number': quote_data.get('sequence_number'),
                'conditions': quote_data.get('conditions', []),
                'indicators': quote_data.get('indicators', []),

                # Various timestamps with different precision levels
                'participant_timestamp': quote_data.get('participant_timestamp'),
                'sip_timestamp': quote_data.get('sip_timestamp'),
                'trf_timestamp': quote_data.get('trf_timestamp')
            }

            # Calculate NBBO metrics
            bid_price = quote_data.get('bid_price', 0)
            ask_price = quote_data.get('ask_price', 0)
            bid_size = quote_data.get('bid_size', 0)
            ask_size = quote_data.get('ask_size', 0)

            # Calculate spread and liquidity metrics
            if bid_price > 0 and ask_price > 0:
                spread = ask_price - bid_price
                spread_percentage = (spread / bid_price) * 100
                mid_price = (bid_price + ask_price) / 2
            else:
                spread = 0
                spread_percentage = 0
                mid_price = 0

            # Convert round lot sizes to actual share quantities (1 round lot = 100 shares)
            actual_bid_size = bid_size * 100
            actual_ask_size = ask_size * 100

            # Add calculated fields
            quote_info['nbbo_metrics'] = {
                'spread': spread,
                'spread_percentage': round(spread_percentage, 4),
                'mid_price': mid_price,
                'bid_price': bid_price,
                'ask_price': ask_price,
                'bid_size_round_lots': bid_size,
                'ask_size_round_lots': ask_size,
                'bid_size_shares': actual_bid_size,
                'ask_size_shares': actual_ask_size,
                'total_liquidity_shares': actual_bid_size + actual_ask_size,
                'has_bid': bid_price > 0,
                'has_ask': ask_price > 0,
                'has_nbbo': bid_price > 0 and ask_price > 0
            }

            # Add exchange information
            quote_info['exchange_info'] = {
                'bid_exchange_id': quote_data.get('bid_exchange'),
                'ask_exchange_id': quote_data.get('ask_exchange'),
                'tape_id': quote_data.get('tape'),
                'same_exchange': quote_data.get('bid_exchange') == quote_data.get('ask_exchange')
            }

            # Add human-readable tape information
            tape_map = {1: 'A (NYSE)', 2: 'B (NYSE ARCA)', 3: 'C (NASDAQ)'}
            tape_id = quote_data.get('tape')
            if tape_id in tape_map:
                quote_info['tape_description'] = tape_map[tape_id]

            # Add condition and indicator analysis
            conditions = quote_data.get('conditions', [])
            indicators = quote_data.get('indicators', [])

            quote_info['quote_conditions'] = {
                'condition_codes': conditions,
                'indicators': indicators,
                'conditions_count': len(conditions),
                'indicators_count': len(indicators),
                'has_conditions': len(conditions) > 0,
                'has_indicators': len(indicators) > 0
            }

            quotes.append(quote_info)

        # Calculate summary statistics
        if quotes:
            # Spread analysis
            spreads = [q['nbbo_metrics']['spread'] for q in quotes if q['nbbo_metrics']['has_nbbo']]
            avg_spread = sum(spreads) / len(spreads) if spreads else 0
            min_spread = min(spreads) if spreads else 0
            max_spread = max(spreads) if spreads else 0

            # Liquidity analysis
            total_liquidity = sum(q['nbbo_metrics']['total_liquidity_shares'] for q in quotes)
            avg_liquidity = total_liquidity / len(quotes)

            # Price analysis
            mid_prices = [q['nbbo_metrics']['mid_price'] for q in quotes if q['nbbo_metrics']['mid_price'] > 0]
            avg_mid_price = sum(mid_prices) / len(mid_prices) if mid_prices else 0

            # Exchange distribution
            bid_exchanges = {}
            ask_exchanges = {}
            for quote in quotes:
                bid_ex = quote['exchange_info']['bid_exchange_id']
                ask_ex = quote['exchange_info']['ask_exchange_id']
                if bid_ex:
                    bid_exchanges[bid_ex] = bid_exchanges.get(bid_ex, 0) + 1
                if ask_ex:
                    ask_exchanges[ask_ex] = ask_exchanges.get(ask_ex, 0) + 1

            # NBBO completeness
            nbbo_complete = sum(1 for q in quotes if q['nbbo_metrics']['has_nbbo'])
            bid_only = sum(1 for q in quotes if q['nbbo_metrics']['has_bid'] and not q['nbbo_metrics']['has_ask'])
            ask_only = sum(1 for q in quotes if q['nbbo_metrics']['has_ask'] and not q['nbbo_metrics']['has_bid'])

            # Time range analysis
            timestamps = [quote['sip_timestamp'] for quote in quotes if quote.get('sip_timestamp')]
            if timestamps:
                start_time = min(timestamps)
                end_time = max(timestamps)
                time_range_ns = end_time - start_time
                time_range_seconds = time_range_ns / 1_000_000_000 if time_range_ns > 0 else 0
            else:
                start_time = None
                end_time = None
                time_range_seconds = 0

            summary = {
                'ticker': stock_ticker,
                'quote_count': len(quotes),
                'time_range_seconds': round(time_range_seconds, 3),
                'start_time': start_time,
                'end_time': end_time,
                'spread_analysis': {
                    'average_spread': round(avg_spread, 4),
                    'min_spread': min_spread,
                    'max_spread': max_spread
                },
                'liquidity_analysis': {
                    'total_liquidity_shares': total_liquidity,
                    'average_liquidity_shares': round(avg_liquidity, 2)
                },
                'price_analysis': {
                    'average_mid_price': round(avg_mid_price, 4)
                },
                'nbbo_completeness': {
                    'complete_nbbo': nbbo_complete,
                    'bid_only': bid_only,
                    'ask_only': ask_only,
                    'nbbo_completion_rate': round((nbbo_complete / len(quotes)) * 100, 2)
                },
                'exchange_distribution': {
                    'bid_exchanges': bid_exchanges,
                    'ask_exchanges': ask_exchanges,
                    'most_active_bid_exchange': max(bid_exchanges.items(), key=lambda x: x[1])[0] if bid_exchanges else None,
                    'most_active_ask_exchange': max(ask_exchanges.items(), key=lambda x: x[1])[0] if ask_exchanges else None
                }
            }
        else:
            summary = {
                'ticker': stock_ticker,
                'quote_count': 0,
                'message': f"No quotes found for {stock_ticker} with specified criteria"
            }

        return {
            'success': True,
            'ticker': stock_ticker,
            'quotes': quotes,
            'summary': summary,
            'next_url': result.get('next_url'),
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": f"Failed to get quotes for {stock_ticker}: {str(e)}"}

def get_last_quote(stocks_ticker: str) -> Dict:
    """
    Retrieve the most recent National Best Bid and Offer (NBBO) quote for a specified stock ticker

    Args:
        stocks_ticker: Case-sensitive ticker symbol (e.g., AAPL)

    Returns:
        Dictionary containing the most recent NBBO quote data
    """
    try:
        if not stocks_ticker:
            return {"error": "Stock ticker is required"}

        # Make API request to get last quote (Note: this uses v2 API)
        result = make_polygon_request(f'/v2/last/nbbo/{stocks_ticker}', {})

        if 'error' in result:
            return result

        # Extract quote data
        quote_data = result.get('results')

        if not quote_data:
            return {"error": f"No last quote data found for {stocks_ticker}"}

        # Extract quote details from the compact format
        bid_price = quote_data.get('p', 0)
        ask_price = quote_data.get('P', 0)
        bid_size = quote_data.get('s', 0)
        ask_size = quote_data.get('S', 0)

        # Build comprehensive last quote object
        last_quote = {
            'ticker': quote_data.get('T'),
            'bid_price': bid_price,
            'ask_price': ask_price,
            'bid_size': bid_size,
            'ask_size': ask_size,
            'bid_exchange_id': quote_data.get('x'),
            'ask_exchange_id': quote_data.get('X'),
            'tape_id': quote_data.get('z'),
            'sequence_number': quote_data.get('q'),
            'conditions': quote_data.get('c', []),
            'indicators': quote_data.get('i', []),

            # Various timestamps with different precision levels
            'sip_timestamp': quote_data.get('t'),
            'trf_timestamp': quote_data.get('f'),
            'participant_timestamp': quote_data.get('y')
        }

        # Add human-readable tape information
        tape_map = {1: 'A (NYSE)', 2: 'B (NYSE ARCA)', 3: 'C (NASDAQ)'}
        tape_id = quote_data.get('z')
        if tape_id in tape_map:
            last_quote['tape_description'] = tape_map[tape_id]

        # Calculate NBBO metrics
        if bid_price > 0 and ask_price > 0:
            spread = ask_price - bid_price
            spread_percentage = (spread / bid_price) * 100
            mid_price = (bid_price + ask_price) / 2
        else:
            spread = 0
            spread_percentage = 0
            mid_price = 0

        # Convert round lot sizes to actual share quantities (1 round lot = 100 shares)
        actual_bid_size = bid_size * 100
        actual_ask_size = ask_size * 100

        # Add calculated fields
        last_quote['nbbo_metrics'] = {
            'spread': spread,
            'spread_percentage': round(spread_percentage, 4),
            'mid_price': mid_price,
            'bid_price': bid_price,
            'ask_price': ask_price,
            'bid_size_round_lots': bid_size,
            'ask_size_round_lots': ask_size,
            'bid_size_shares': actual_bid_size,
            'ask_size_shares': actual_ask_size,
            'total_liquidity_shares': actual_bid_size + actual_ask_size,
            'has_bid': bid_price > 0,
            'has_ask': ask_price > 0,
            'has_nbbo': bid_price > 0 and ask_price > 0,
            'spread_bps': round(spread_percentage * 100, 2)  # Spread in basis points
        }

        # Add exchange information
        last_quote['exchange_info'] = {
            'bid_exchange_id': quote_data.get('x'),
            'ask_exchange_id': quote_data.get('X'),
            'tape_id': tape_id,
            'same_exchange': quote_data.get('x') == quote_data.get('X'),
            'cross_exchange': quote_data.get('x') != quote_data.get('X')
        }

        # Add timestamp analysis
        sip_timestamp = quote_data.get('t')
        participant_timestamp = quote_data.get('y')
        trf_timestamp = quote_data.get('f')

        # Calculate time differences (if all timestamps are available)
        timestamp_analysis = {}
        if sip_timestamp and participant_timestamp:
            latency_sip_participant = sip_timestamp - participant_timestamp
            timestamp_analysis['sip_to_participant_latency_ns'] = latency_sip_participant
            timestamp_analysis['sip_to_participant_latency_ms'] = latency_sip_participant / 1_000_000

        if trf_timestamp and sip_timestamp:
            latency_trf_sip = sip_timestamp - trf_timestamp
            timestamp_analysis['trf_to_sip_latency_ns'] = latency_trf_sip
            timestamp_analysis['trf_to_sip_latency_ms'] = latency_trf_sip / 1_000_000

        if participant_timestamp and trf_timestamp:
            latency_participant_trf = participant_timestamp - trf_timestamp
            timestamp_analysis['participant_to_trf_latency_ns'] = latency_participant_trf
            timestamp_analysis['participant_to_trf_latency_ms'] = latency_participant_trf / 1_000_000

        last_quote['timestamp_analysis'] = timestamp_analysis

        # Add condition and indicator analysis
        conditions = quote_data.get('c', [])
        indicators = quote_data.get('i', [])

        last_quote['quote_conditions'] = {
            'condition_codes': conditions,
            'indicators': indicators,
            'conditions_count': len(conditions),
            'indicators_count': len(indicators),
            'has_conditions': len(conditions) > 0,
            'has_indicators': len(indicators) > 0
        }

        # Add market status context (based on timestamp)
        if sip_timestamp:
            # Convert nanosecond timestamp to datetime for analysis
            try:
                import datetime
                quote_time = datetime.datetime.fromtimestamp(sip_timestamp / 1_000_000_000)
                last_quote['quote_time_formatted'] = quote_time.strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]  # Millisecond precision
                last_quote['quote_date'] = quote_time.strftime('%Y-%m-%d')
                last_quote['quote_time'] = quote_time.strftime('%H:%M:%S')
            except (ValueError, OSError):
                # Handle timestamp conversion errors
                pass

        # Add liquidity analysis
        if last_quote['nbbo_metrics']['has_nbbo']:
            # Liquidity quality assessment based on spread and size
            spread_quality = 'Tight' if spread_percentage < 0.1 else ('Normal' if spread_percentage < 0.5 else 'Wide')
            size_quality = 'High' if actual_bid_size + actual_ask_size >= 1000 else ('Normal' if actual_bid_size + actual_ask_size >= 100 else 'Low')

            last_quote['liquidity_analysis'] = {
                'spread_quality': spread_quality,
                'size_quality': size_quality,
                'overall_liquidity': 'Excellent' if spread_quality == 'Tight' and size_quality == 'High' else
                                  'Good' if spread_quality in ['Tight', 'Normal'] and size_quality in ['High', 'Normal'] else 'Fair'
            }

        return {
            'success': True,
            'ticker': stocks_ticker,
            'last_quote': last_quote,
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": f"Failed to get last quote for {stocks_ticker}: {str(e)}"}

def get_sma(
    stock_ticker: str,
    timestamp: Optional[str] = None,
    timespan: Optional[str] = None,
    adjusted: Optional[bool] = None,
    window: Optional[int] = None,
    series_type: Optional[str] = None,
    expand_underlying: Optional[bool] = None,
    order: Optional[str] = None,
    limit: Optional[int] = None,
    timestamp_gte: Optional[str] = None,
    timestamp_gt: Optional[str] = None,
    timestamp_lte: Optional[str] = None,
    timestamp_lt: Optional[str] = None
) -> Dict:
    """
    Retrieve the Simple Moving Average (SMA) for a specified ticker over a defined time range

    Args:
        stock_ticker: Case-sensitive ticker symbol (e.g., AAPL)
        timestamp: Query by timestamp. Either a date with the format YYYY-MM-DD or a millisecond timestamp
        timespan: The size of the aggregate time window (e.g., day, hour, minute)
        adjusted: Whether aggregates are adjusted for splits (default: true)
        window: The window size used to calculate SMA (e.g., 10 for 10-day SMA)
        series_type: The price type used for calculation (e.g., close, open, high, low)
        expand_underlying: Whether to include underlying aggregates in response
        order: Order results by timestamp (asc, desc)
        limit: Limit number of results (default: 10, max: 5000)
        timestamp_gte: Range by timestamp (greater than or equal)
        timestamp_gt: Range by timestamp (greater than)
        timestamp_lte: Range by timestamp (less than or equal)
        timestamp_lt: Range by timestamp (less than)

    Returns:
        Dictionary containing SMA indicator data
    """
    try:
        if not stock_ticker:
            return {"error": "Stock ticker is required"}

        # Build parameters dictionary, only include non-None values
        params = {}

        if timestamp is not None:
            params['timestamp'] = timestamp
        if timespan is not None:
            params['timespan'] = timespan
        if adjusted is not None:
            params['adjusted'] = str(adjusted).lower()
        if window is not None:
            params['window'] = window
        if series_type is not None:
            params['series_type'] = series_type
        if expand_underlying is not None:
            params['expand_underlying'] = str(expand_underlying).lower()
        if order is not None:
            params['order'] = order
        if limit is not None:
            params['limit'] = min(limit, 5000)  # Enforce max limit of 5000
        if timestamp_gte is not None:
            params['timestamp.gte'] = timestamp_gte
        if timestamp_gt is not None:
            params['timestamp.gt'] = timestamp_gt
        if timestamp_lte is not None:
            params['timestamp.lte'] = timestamp_lte
        if timestamp_lt is not None:
            params['timestamp.lt'] = timestamp_lt

        # Make API request to get SMA (Note: this uses v1 API)
        result = make_polygon_request(f'/v1/indicators/sma/{stock_ticker}', params)

        if 'error' in result:
            return result

        # Extract SMA results
        results_data = result.get('results', {})
        if not results_data:
            return {"error": f"No SMA data found for {stock_ticker}"}

        # Extract SMA values
        sma_values = results_data.get('values', [])
        underlying_data = results_data.get('underlying', {})

        # Process SMA values
        processed_sma = []
        for sma_point in sma_values:
            point_info = {
                'timestamp': sma_point.get('timestamp'),
                'sma_value': sma_point.get('value'),
                'date': None
            }

            # Convert timestamp to readable date if possible
            timestamp = sma_point.get('timestamp')
            if timestamp:
                try:
                    import datetime
                    # Handle both millisecond and nanosecond timestamps
                    if timestamp > 1e15:  # Nanosecond timestamp
                        point_time = datetime.datetime.fromtimestamp(timestamp / 1_000_000_000)
                    else:  # Millisecond timestamp
                        point_time = datetime.datetime.fromtimestamp(timestamp / 1000)
                    point_info['date'] = point_time.strftime('%Y-%m-%d')
                    point_info['datetime'] = point_time.strftime('%Y-%m-%d %H:%M:%S')
                except (ValueError, OSError):
                    pass

            processed_sma.append(point_info)

        # Process underlying aggregates if available
        underlying_aggregates = []
        if underlying_data and expand_underlying:
            aggregates = underlying_data.get('aggregates', [])
            for agg in aggregates:
                agg_info = {
                    'timestamp': agg.get('t'),
                    'open': agg.get('o'),
                    'high': agg.get('h'),
                    'low': agg.get('l'),
                    'close': agg.get('c'),
                    'volume': agg.get('v'),
                    'vwap': agg.get('vw'),
                    'transactions': agg.get('n'),
                    'date': None
                }

                # Convert timestamp to readable date
                timestamp = agg.get('t')
                if timestamp:
                    try:
                        import datetime
                        if timestamp > 1e15:  # Nanosecond timestamp
                            agg_time = datetime.datetime.fromtimestamp(timestamp / 1_000_000_000)
                        else:  # Millisecond timestamp
                            agg_time = datetime.datetime.fromtimestamp(timestamp / 1000)
                        agg_info['date'] = agg_time.strftime('%Y-%m-%d')
                        agg_info['datetime'] = agg_time.strftime('%Y-%m-%d %H:%M:%S')
                    except (ValueError, OSError):
                        pass

                underlying_aggregates.append(agg_info)

        # Calculate SMA statistics
        if processed_sma:
            sma_values_list = [point['sma_value'] for point in processed_sma if point['sma_value'] is not None]
            if sma_values_list:
                sma_stats = {
                    'count': len(sma_values_list),
                    'min_value': min(sma_values_list),
                    'max_value': max(sma_values_list),
                    'avg_value': sum(sma_values_list) / len(sma_values_list),
                    'latest_value': sma_values_list[0] if sma_values_list else None,
                    'first_value': sma_values_list[-1] if sma_values_list else None,
                    'total_change': sma_values_list[0] - sma_values_list[-1] if len(sma_values_list) > 1 else 0,
                    'percent_change': ((sma_values_list[0] - sma_values_list[-1]) / sma_values_list[-1] * 100) if len(sma_values_list) > 1 and sma_values_list[-1] != 0 else 0
                }
            else:
                sma_stats = {'count': 0, 'message': 'No valid SMA values found'}
        else:
            sma_stats = {'count': 0, 'message': 'No SMA data found'}

        # Build comprehensive response
        sma_response = {
            'ticker': stock_ticker,
            'indicator_type': 'SMA',
            'parameters': {
                'window': window,
                'series_type': series_type,
                'timespan': timespan,
                'adjusted': adjusted
            },
            'sma_values': processed_sma,
            'statistics': sma_stats,
            'underlying_url': underlying_data.get('url') if underlying_data else None
        }

        # Include underlying aggregates if requested
        if expand_underlying and underlying_aggregates:
            sma_response['underlying_aggregates'] = underlying_aggregates

        return {
            'success': True,
            'sma_data': sma_response,
            'next_url': result.get('next_url'),
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": f"Failed to get SMA for {stock_ticker}: {str(e)}"}

def get_ema(
    stock_ticker: str,
    timestamp: Optional[str] = None,
    timespan: Optional[str] = None,
    adjusted: Optional[bool] = None,
    window: Optional[int] = None,
    series_type: Optional[str] = None,
    expand_underlying: Optional[bool] = None,
    order: Optional[str] = None,
    limit: Optional[int] = None,
    timestamp_gte: Optional[str] = None,
    timestamp_gt: Optional[str] = None,
    timestamp_lte: Optional[str] = None,
    timestamp_lt: Optional[str] = None
) -> Dict:
    """
    Retrieve the Exponential Moving Average (EMA) for a specified ticker over a defined time range

    Args:
        stock_ticker: Case-sensitive ticker symbol (e.g., AAPL)
        timestamp: Query by timestamp. Either a date with the format YYYY-MM-DD or a millisecond timestamp
        timespan: The size of the aggregate time window (e.g., day, hour, minute)
        adjusted: Whether aggregates are adjusted for splits (default: true)
        window: The window size used to calculate EMA (e.g., 10 for 10-day EMA)
        series_type: The price type used for calculation (e.g., close, open, high, low)
        expand_underlying: Whether to include underlying aggregates in response
        order: Order results by timestamp (asc, desc)
        limit: Limit number of results (default: 10, max: 5000)
        timestamp_gte: Range by timestamp (greater than or equal)
        timestamp_gt: Range by timestamp (greater than)
        timestamp_lte: Range by timestamp (less than or equal)
        timestamp_lt: Range by timestamp (less than)

    Returns:
        Dictionary containing EMA indicator data
    """
    try:
        if not stock_ticker:
            return {"error": "Stock ticker is required"}

        # Build parameters dictionary, only include non-None values
        params = {}

        if timestamp is not None:
            params['timestamp'] = timestamp
        if timespan is not None:
            params['timespan'] = timespan
        if adjusted is not None:
            params['adjusted'] = str(adjusted).lower()
        if window is not None:
            params['window'] = window
        if series_type is not None:
            params['series_type'] = series_type
        if expand_underlying is not None:
            params['expand_underlying'] = str(expand_underlying).lower()
        if order is not None:
            params['order'] = order
        if limit is not None:
            params['limit'] = min(limit, 5000)  # Enforce max limit of 5000
        if timestamp_gte is not None:
            params['timestamp.gte'] = timestamp_gte
        if timestamp_gt is not None:
            params['timestamp.gt'] = timestamp_gt
        if timestamp_lte is not None:
            params['timestamp.lte'] = timestamp_lte
        if timestamp_lt is not None:
            params['timestamp.lt'] = timestamp_lt

        # Make API request to get EMA (Note: this uses v1 API)
        result = make_polygon_request(f'/v1/indicators/ema/{stock_ticker}', params)

        if 'error' in result:
            return result

        # Extract EMA results
        results_data = result.get('results', {})
        if not results_data:
            return {"error": f"No EMA data found for {stock_ticker}"}

        # Extract EMA values
        ema_values = results_data.get('values', [])
        underlying_data = results_data.get('underlying', {})

        # Process EMA values
        processed_ema = []
        for ema_point in ema_values:
            point_info = {
                'timestamp': ema_point.get('timestamp'),
                'ema_value': ema_point.get('value'),
                'date': None
            }

            # Convert timestamp to readable date if possible
            timestamp = ema_point.get('timestamp')
            if timestamp:
                try:
                    import datetime
                    # Handle both millisecond and nanosecond timestamps
                    if timestamp > 1e15:  # Nanosecond timestamp
                        point_time = datetime.datetime.fromtimestamp(timestamp / 1_000_000_000)
                    else:  # Millisecond timestamp
                        point_time = datetime.datetime.fromtimestamp(timestamp / 1000)
                    point_info['date'] = point_time.strftime('%Y-%m-%d')
                    point_info['datetime'] = point_time.strftime('%Y-%m-%d %H:%M:%S')
                except (ValueError, OSError):
                    pass

            processed_ema.append(point_info)

        # Process underlying aggregates if available
        underlying_aggregates = []
        if underlying_data and expand_underlying:
            aggregates = underlying_data.get('aggregates', [])
            for agg in aggregates:
                agg_info = {
                    'timestamp': agg.get('t'),
                    'open': agg.get('o'),
                    'high': agg.get('h'),
                    'low': agg.get('l'),
                    'close': agg.get('c'),
                    'volume': agg.get('v'),
                    'vwap': agg.get('vw'),
                    'transactions': agg.get('n'),
                    'date': None
                }

                # Convert timestamp to readable date
                timestamp = agg.get('t')
                if timestamp:
                    try:
                        import datetime
                        if timestamp > 1e15:  # Nanosecond timestamp
                            agg_time = datetime.datetime.fromtimestamp(timestamp / 1_000_000_000)
                        else:  # Millisecond timestamp
                            agg_time = datetime.datetime.fromtimestamp(timestamp / 1000)
                        agg_info['date'] = agg_time.strftime('%Y-%m-%d')
                        agg_info['datetime'] = agg_time.strftime('%Y-%m-%d %H:%M:%S')
                    except (ValueError, OSError):
                        pass

                underlying_aggregates.append(agg_info)

        # Calculate EMA statistics
        if processed_ema:
            ema_values_list = [point['ema_value'] for point in processed_ema if point['ema_value'] is not None]
            if ema_values_list:
                ema_stats = {
                    'count': len(ema_values_list),
                    'min_value': min(ema_values_list),
                    'max_value': max(ema_values_list),
                    'avg_value': sum(ema_values_list) / len(ema_values_list),
                    'latest_value': ema_values_list[0] if ema_values_list else None,
                    'first_value': ema_values_list[-1] if ema_values_list else None,
                    'total_change': ema_values_list[0] - ema_values_list[-1] if len(ema_values_list) > 1 else 0,
                    'percent_change': ((ema_values_list[0] - ema_values_list[-1]) / ema_values_list[-1] * 100) if len(ema_values_list) > 1 and ema_values_list[-1] != 0 else 0
                }
            else:
                ema_stats = {'count': 0, 'message': 'No valid EMA values found'}
        else:
            ema_stats = {'count': 0, 'message': 'No EMA data found'}

        # Build comprehensive response
        ema_response = {
            'ticker': stock_ticker,
            'indicator_type': 'EMA',
            'parameters': {
                'window': window,
                'series_type': series_type,
                'timespan': timespan,
                'adjusted': adjusted
            },
            'ema_values': processed_ema,
            'statistics': ema_stats,
            'underlying_url': underlying_data.get('url') if underlying_data else None
        }

        # Include underlying aggregates if requested
        if expand_underlying and underlying_aggregates:
            ema_response['underlying_aggregates'] = underlying_aggregates

        return {
            'success': True,
            'ema_data': ema_response,
            'next_url': result.get('next_url'),
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": f"Failed to get EMA for {stock_ticker}: {str(e)}"}

def get_macd(
    stock_ticker: str,
    timestamp: Optional[str] = None,
    timespan: Optional[str] = None,
    adjusted: Optional[bool] = None,
    short_window: Optional[int] = None,
    long_window: Optional[int] = None,
    signal_window: Optional[int] = None,
    series_type: Optional[str] = None,
    expand_underlying: Optional[bool] = None,
    order: Optional[str] = None,
    limit: Optional[int] = None,
    timestamp_gte: Optional[str] = None,
    timestamp_gt: Optional[str] = None,
    timestamp_lte: Optional[str] = None,
    timestamp_lt: Optional[str] = None
) -> Dict:
    """
    Retrieve the Moving Average Convergence/Divergence (MACD) for a specified ticker over a defined time range

    Args:
        stock_ticker: Case-sensitive ticker symbol (e.g., AAPL)
        timestamp: Query by timestamp. Either a date with the format YYYY-MM-DD or a millisecond timestamp
        timespan: The size of the aggregate time window (e.g., day, hour, minute)
        adjusted: Whether aggregates are adjusted for splits (default: true)
        short_window: Short window size for MACD calculation (e.g., 12 for 12-day EMA)
        long_window: Long window size for MACD calculation (e.g., 26 for 26-day EMA)
        signal_window: Signal line window size (e.g., 9 for 9-day EMA)
        series_type: Price type used for calculation (e.g., close, open, high, low)
        expand_underlying: Whether to include underlying aggregates in response
        order: Order results by timestamp (asc, desc)
        limit: Limit number of results (default: 10, max: 5000)
        timestamp_gte: Range by timestamp (greater than or equal)
        timestamp_gt: Range by timestamp (greater than)
        timestamp_lte: Range by timestamp (less than or equal)
        timestamp_lt: Range by timestamp (less than)

    Returns:
        Dictionary containing MACD indicator data
    """
    try:
        if not stock_ticker:
            return {"error": "Stock ticker is required"}

        # Build parameters dictionary, only include non-None values
        params = {}

        if timestamp is not None:
            params['timestamp'] = timestamp
        if timespan is not None:
            params['timespan'] = timespan
        if adjusted is not None:
            params['adjusted'] = str(adjusted).lower()
        if short_window is not None:
            params['short_window'] = short_window
        if long_window is not None:
            params['long_window'] = long_window
        if signal_window is not None:
            params['signal_window'] = signal_window
        if series_type is not None:
            params['series_type'] = series_type
        if expand_underlying is not None:
            params['expand_underlying'] = str(expand_underlying).lower()
        if order is not None:
            params['order'] = order
        if limit is not None:
            params['limit'] = min(limit, 5000)  # Enforce max limit of 5000
        if timestamp_gte is not None:
            params['timestamp.gte'] = timestamp_gte
        if timestamp_gt is not None:
            params['timestamp.gt'] = timestamp_gt
        if timestamp_lte is not None:
            params['timestamp.lte'] = timestamp_lte
        if timestamp_lt is not None:
            params['timestamp.lt'] = timestamp_lt

        # Make API request to get MACD (Note: this uses v1 API)
        result = make_polygon_request(f'/v1/indicators/macd/{stock_ticker}', params)

        if 'error' in result:
            return result

        # Extract MACD results
        results_data = result.get('results', {})
        if not results_data:
            return {"error": f"No MACD data found for {stock_ticker}"}

        # Extract MACD values
        macd_values = results_data.get('values', [])
        underlying_data = results_data.get('underlying', {})

        # Process MACD values
        processed_macd = []
        for macd_point in macd_values:
            point_info = {
                'timestamp': macd_point.get('timestamp'),
                'macd_value': macd_point.get('value'),
                'signal_line': macd_point.get('signal'),
                'histogram': macd_point.get('histogram'),
                'date': None
            }

            # Convert timestamp to readable date if possible
            timestamp = macd_point.get('timestamp')
            if timestamp:
                try:
                    import datetime
                    # Handle both millisecond and nanosecond timestamps
                    if timestamp > 1e15:  # Nanosecond timestamp
                        point_time = datetime.datetime.fromtimestamp(timestamp / 1_000_000_000)
                    else:  # Millisecond timestamp
                        point_time = datetime.datetime.fromtimestamp(timestamp / 1000)
                    point_info['date'] = point_time.strftime('%Y-%m-%d')
                    point_info['datetime'] = point_time.strftime('%Y-%m-%d %H:%M:%S')
                except (ValueError, OSError):
                    pass

            # Calculate MACD analysis metrics
            macd_val = macd_point.get('value', 0)
            signal_val = macd_point.get('signal', 0)
            histogram_val = macd_point.get('histogram', 0)

            # MACD crossovers and signals
            if macd_val and signal_val:
                point_info['crossover_analysis'] = {
                    'macd_above_signal': macd_val > signal_val,
                    'macd_below_signal': macd_val < signal_val,
                    'signal_crossover': abs(macd_val - signal_val) < 0.001,  # Close to zero indicates crossover
                    'macd_signal_diff': macd_val - signal_val
                }
            else:
                point_info['crossover_analysis'] = {
                    'macd_above_signal': None,
                    'macd_below_signal': None,
                    'signal_crossover': False,
                    'macd_signal_diff': None
                }

            # Zero line analysis (center line)
            point_info['zero_line_analysis'] = {
                'macd_above_zero': macd_val > 0,
                'macd_below_zero': macd_val < 0,
                'macd_near_zero': abs(macd_val) < 0.01
            }

            # Signal strength
            point_info['signal_strength'] = {
                'histogram_positive': histogram_val > 0,
                'histogram_negative': histogram_val < 0,
                'histogram_strength': abs(histogram_val)
            }

            processed_macd.append(point_info)

        # Process underlying aggregates if available
        underlying_aggregates = []
        if underlying_data and expand_underlying:
            aggregates = underlying_data.get('aggregates', [])
            for agg in aggregates:
                agg_info = {
                    'timestamp': agg.get('t'),
                    'open': agg.get('o'),
                    'high': agg.get('h'),
                    'low': agg.get('l'),
                    'close': agg.get('c'),
                    'volume': agg.get('v'),
                    'vwap': agg.get('vw'),
                    'transactions': agg.get('n'),
                    'date': None
                }

                # Convert timestamp to readable date
                timestamp = agg.get('t')
                if timestamp:
                    try:
                        import datetime
                        if timestamp > 1e15:  # Nanosecond timestamp
                            agg_time = datetime.datetime.fromtimestamp(timestamp / 1_000_000_000)
                        else:  # Millisecond timestamp
                            agg_time = datetime.datetime.fromtimestamp(timestamp / 1000)
                        agg_info['date'] = agg_time.strftime('%Y-%m-%d')
                        agg_info['datetime'] = agg_time.strftime('%Y-%m-%d %H:%M:%S')
                    except (ValueError, OSError):
                        pass

                underlying_aggregates.append(agg_info)

        # Calculate MACD statistics
        if processed_macd:
            macd_values_list = [point['macd_value'] for point in processed_macd if point['macd_value'] is not None]
            signal_values_list = [point['signal_line'] for point in processed_macd if point['signal_line'] is not None]
            histogram_values_list = [point['histogram'] for point in processed_macd if point['histogram'] is not None]

            if macd_values_list:
                macd_stats = {
                    'count': len(macd_values_list),
                    'min_value': min(macd_values_list),
                    'max_value': max(macd_values_list),
                    'avg_value': sum(macd_values_list) / len(macd_values_list),
                    'latest_value': macd_values_list[0] if macd_values_list else None,
                    'first_value': macd_values_list[-1] if macd_values_list else None,
                    'total_change': macd_values_list[0] - macd_values_list[-1] if len(macd_values_list) > 1 else 0,
                    'percent_change': ((macd_values_list[0] - macd_values_list[-1]) / macd_values_list[-1] * 100) if len(macd_values_list) > 1 and macd_values_list[-1] != 0 else 0
                }
            else:
                    macd_stats = {'count': 0, 'message': 'No valid MACD values found'}

            # Signal line statistics
            if signal_values_list:
                signal_stats = {
                    'min_value': min(signal_values_list),
                    'max_value': max(signal_values_list),
                    'avg_value': sum(signal_values_list) / len(signal_values_list),
                    'latest_value': signal_values_list[0] if signal_values_list else None,
                    'first_value': signal_values_list[-1] if signal_values_list else None
                }
            else:
                signal_stats = {'message': 'No signal line data found'}

            # Histogram statistics (momentum analysis)
            if histogram_values_list:
                histogram_stats = {
                    'min_value': min(histogram_values_list),
                    'max_value': max(histogram_values_list),
                    'avg_value': sum(histogram_values_list) / len(histogram_values_list),
                    'latest_value': histogram_values_list[0] if histogram_values_list else None,
                    'positive_periods': sum(1 for h in histogram_values_list if h > 0),
                    'negative_periods': sum(1 for h in histogram_values_list if h < 0),
                    'zero_periods': sum(1 for h in histogram_values_list if abs(h) < 0.001)
                }
            else:
                histogram_stats = {'message': 'No histogram data found'}

            # Crossover analysis
            crossovers = [point for point in processed_macd if point['crossover_analysis']['signal_crossover']]
            macd_above_signal = sum(1 for point in processed_macd if point['crossover_analysis']['macd_above_signal'])
            macd_below_signal = sum(1 for point in processed_macd if point['crossover_analysis']['macd_below_signal'])

            crossover_stats = {
                'total_crossovers': len(crossovers),
                'macd_above_signal_periods': macd_above_signal,
                'macd_below_signal_periods': macd_below_signal,
                'dominant_position': 'bullish' if macd_above_signal > macd_below_signal else 'bearish' if macd_below_signal > macd_above_signal else 'neutral'
            }
        else:
            macd_stats = {'count': 0, 'message': 'No MACD data found'}
            signal_stats = {'message': 'No signal line data found'}
            histogram_stats = {'message': 'No histogram data found'}
            crossover_stats = {'message': 'No crossover data found'}

        # Build comprehensive response
        macd_response = {
            'ticker': stock_ticker,
            'indicator_type': 'MACD',
            'parameters': {
                'short_window': short_window,
                'long_window': long_window,
                'signal_window': signal_window,
                'series_type': series_type,
                'timespan': timespan,
                'adjusted': adjusted
            },
            'macd_values': processed_macd,
            'statistics': {
                'macd_stats': macd_stats,
                'signal_stats': signal_stats,
                'histogram_stats': histogram_stats,
                'crossover_stats': crossover_stats
            },
            'underlying_url': underlying_data.get('url') if underlying_data else None
        }

        # Include underlying aggregates if requested
        if expand_underlying and underlying_aggregates:
            macd_response['underlying_aggregates'] = underlying_aggregates

        return {
            'success': True,
            'macd_data': macd_response,
            'next_url': result.get('next_url'),
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": f"Failed to get MACD for {stock_ticker}: {str(e)}"}

def get_rsi(
    stock_ticker: str,
    timestamp: Optional[str] = None,
    timespan: Optional[str] = None,
    adjusted: Optional[bool] = None,
    window: Optional[int] = None,
    series_type: Optional[str] = None,
    expand_underlying: Optional[bool] = None,
    order: Optional[str] = None,
    limit: Optional[int] = None,
    timestamp_gte: Optional[str] = None,
    timestamp_gt: Optional[str] = None,
    timestamp_lte: Optional[str] = None,
    timestamp_lt: Optional[str] = None
) -> Dict:
    """
    Retrieve the Relative Strength Index (RSI) for a specified ticker over a defined time range

    Args:
        stock_ticker: Case-sensitive ticker symbol (e.g., AAPL)
        timestamp: Query by timestamp. Either a date with the format YYYY-MM-DD or a millisecond timestamp
        timespan: The size of the aggregate time window (e.g., day, hour, minute)
        adjusted: Whether aggregates are adjusted for splits (default: true)
        window: The window size used to calculate RSI (e.g., 14 for 14-day RSI)
        series_type: Price type used for calculation (e.g., close, open, high, low)
        expand_underlying: Whether to include underlying aggregates in response
        order: Order results by timestamp (asc, desc)
        limit: Limit number of results (default: 10, max: 5000)
        timestamp_gte: Range by timestamp (greater than or equal)
        timestamp_gt: Range by timestamp (greater than)
        timestamp_lte: Range by timestamp (less than or equal)
        timestamp_lt: Range by timestamp (less than)

    Returns:
        Dictionary containing RSI indicator data
    """
    try:
        if not stock_ticker:
            return {"error": "Stock ticker is required"}

        # Build parameters dictionary, only include non-None values
        params = {}

        if timestamp is not None:
            params['timestamp'] = timestamp
        if timespan is not None:
            params['timespan'] = timespan
        if adjusted is not None:
            params['adjusted'] = str(adjusted).lower()
        if window is not None:
            params['window'] = window
        if series_type is not None:
            params['series_type'] = series_type
        if expand_underlying is not None:
            params['expand_underlying'] = str(expand_underlying).lower()
        if order is not None:
            params['order'] = order
        if limit is not None:
            params['limit'] = min(limit, 5000)  # Enforce max limit of 5000
        if timestamp_gte is not None:
            params['timestamp.gte'] = timestamp_gte
        if timestamp_gt is not None:
            params['timestamp.gt'] = timestamp_gt
        if timestamp_lte is not None:
            params['timestamp.lte'] = timestamp_lte
        if timestamp_lt is not None:
            params['timestamp.lt'] = timestamp_lt

        # Make API request to get RSI (Note: this uses v1 API)
        result = make_polygon_request(f'/v1/indicators/rsi/{stock_ticker}', params)

        if 'error' in result:
            return result

        # Extract RSI results
        results_data = result.get('results', {})
        if not results_data:
            return {"error": f"No RSI data found for {stock_ticker}"}

        # Extract RSI values
        rsi_values = results_data.get('values', [])
        underlying_data = results_data.get('underlying', {})

        # Process RSI values
        processed_rsi = []
        for rsi_point in rsi_values:
            point_info = {
                'timestamp': rsi_point.get('timestamp'),
                'rsi_value': rsi_point.get('value'),
                'date': None
            }

            # Convert timestamp to readable date if possible
            timestamp = rsi_point.get('timestamp')
            if timestamp:
                try:
                    import datetime
                    # Handle both millisecond and nanosecond timestamps
                    if timestamp > 1e15:  # Nanosecond timestamp
                        point_time = datetime.datetime.fromtimestamp(timestamp / 1_000_000_000)
                    else:  # Millisecond timestamp
                        point_time = datetime.datetime.fromtimestamp(timestamp / 1000)
                    point_info['date'] = point_time.strftime('%Y-%m-%d')
                    point_info['datetime'] = point_time.strftime('%Y-%m-%d %H:%M:%S')
                except (ValueError, OSError):
                    pass

            # RSI level analysis (0-100 scale)
            rsi_val = rsi_point.get('value', 0)
            if rsi_val is not None:
                point_info['rsi_analysis'] = {
                    'rsi_level': rsi_val,
                    'overbought': rsi_val > 70,
                    'oversold': rsi_val < 30,
                    'neutral': 30 <= rsi_val <= 70,
                    'extreme_overbought': rsi_val > 80,
                    'extreme_oversold': rsi_val < 20,
                    'distance_to_overbought': max(0, 70 - rsi_val) if rsi_val < 70 else 0,
                    'distance_to_oversold': max(0, rsi_val - 30) if rsi_val > 30 else 0
                }

                # RSI momentum classification
                if rsi_val >= 80:
                    momentum = 'Strong Overbought'
                elif rsi_val >= 70:
                    momentum = 'Overbought'
                elif rsi_val >= 30:
                    momentum = 'Neutral'
                elif rsi_val >= 20:
                    momentum = 'Oversold'
                else:
                    momentum = 'Strong Oversold'

                point_info['rsi_momentum'] = momentum

                # Divergence potential analysis (would need price data for full analysis)
                point_info['divergence_potential'] = {
                    'rsi_extreme': rsi_val > 80 or rsi_val < 20,
                    'reversal_probability': 'High' if rsi_val > 80 or rsi_val < 20 else 'Medium' if rsi_val >= 70 or rsi_val <= 30 else 'Low'
                }

            processed_rsi.append(point_info)

        # Process underlying aggregates if available
        underlying_aggregates = []
        if underlying_data and expand_underlying:
            aggregates = underlying_data.get('aggregates', [])
            for agg in aggregates:
                agg_info = {
                    'timestamp': agg.get('t'),
                    'open': agg.get('o'),
                    'high': agg.get('h'),
                    'low': agg.get('l'),
                    'close': agg.get('c'),
                    'volume': agg.get('v'),
                    'vwap': agg.get('vw'),
                    'transactions': agg.get('n'),
                    'date': None
                }

                # Convert timestamp to readable date
                timestamp = agg.get('t')
                if timestamp:
                    try:
                        import datetime
                        if timestamp > 1e15:  # Nanosecond timestamp
                            agg_time = datetime.datetime.fromtimestamp(timestamp / 1_000_000_000)
                        else:  # Millisecond timestamp
                            agg_time = datetime.datetime.fromtimestamp(timestamp / 1000)
                        agg_info['date'] = agg_time.strftime('%Y-%m-%d')
                        agg_info['datetime'] = agg_time.strftime('%Y-%m-%d %H:%M:%S')
                    except (ValueError, OSError):
                        pass

                underlying_aggregates.append(agg_info)

        # Calculate RSI statistics
        if processed_rsi:
            rsi_values_list = [point['rsi_value'] for point in processed_rsi if point['rsi_value'] is not None]
            if rsi_values_list:
                rsi_stats = {
                    'count': len(rsi_values_list),
                    'min_value': min(rsi_values_list),
                    'max_value': max(rsi_values_list),
                    'avg_value': sum(rsi_values_list) / len(rsi_values_list),
                    'latest_value': rsi_values_list[0] if rsi_values_list else None,
                    'first_value': rsi_values_list[-1] if rsi_values_list else None,
                    'total_change': rsi_values_list[0] - rsi_values_list[-1] if len(rsi_values_list) > 1 else 0,
                    'percent_change': ((rsi_values_list[0] - rsi_values_list[-1]) / rsi_values_list[-1] * 100) if len(rsi_values_list) > 1 and rsi_values_list[-1] != 0 else 0
                }
            else:
                    rsi_stats = {'count': 0, 'message': 'No valid RSI values found'}

            # RSI level analysis
            overbought_periods = sum(1 for point in processed_rsi if point.get('rsi_analysis', {}).get('overbought', False))
            oversold_periods = sum(1 for point in processed_rsi if point.get('rsi_analysis', {}).get('oversold', False))
            neutral_periods = sum(1 for point in processed_rsi if point.get('rsi_analysis', {}).get('neutral', False))
            extreme_overbought = sum(1 for point in processed_rsi if point.get('rsi_analysis', {}).get('extreme_overbought', False))
            extreme_oversold = sum(1 for point in processed_rsi if point.get('rsi_analysis', {}).get('extreme_oversold', False))

            level_distribution = {
                'overbought_periods': overbought_periods,
                'oversold_periods': oversold_periods,
                'neutral_periods': neutral_periods,
                'extreme_overbought': extreme_overbought,
                'extreme_oversold': extreme_oversold,
                'dominant_level': 'overbought' if overbought_periods > oversold_periods else 'oversold' if oversold_periods > neutral_periods else 'neutral',
                'extreme_conditions': extreme_overbought + extreme_oversold
            }

            # Momentum analysis
            momentum_periods = {}
            for point in processed_rsi:
                momentum = point.get('rsi_momentum', 'Unknown')
                momentum_periods[momentum] = momentum_periods.get(momentum, 0) + 1

            momentum_stats = {
                'distribution': momentum_periods,
                'dominant_momentum': max(momentum_periods.items(), key=lambda x: x[1])[0] if momentum_periods else 'neutral',
                'momentum_consistency': 'High' if len(momentum_periods) == 1 else 'Medium' if len(momentum_periods) <= 3 else 'Low'
            }

            # Extreme level detection
            latest_rsi = processed_rsi[-1].get('rsi_value', 50) if processed_rsi else 50
            if latest_rsi > 80:
                extreme_status = 'Critical Overbought'
                action_signal = 'Consider Selling'
            elif latest_rsi < 20:
                extreme_status = 'Critical Oversold'
                action_signal = 'Consider Buying'
            elif latest_rsi > 70:
                extreme_status = 'Overbought Warning'
                action_signal = 'Caution - Potential Reversal'
            elif latest_rsi < 30:
                extreme_status = 'Oversold Warning'
                action_signal = 'Caution - Potential Reversal'
            else:
                extreme_status = 'Normal Range'
                action_signal = 'Hold'

            extreme_analysis = {
                'current_status': extreme_status,
                'suggested_action': action_signal,
                'rsi_level': latest_rsi,
                'level_distance': {
                    'from_overbought': max(0, 70 - latest_rsi) if latest_rsi < 70 else 0,
                    'from_oversold': max(0, latest_rsi - 30) if latest_rsi > 30 else 0
                }
            }
        else:
            rsi_stats = {'count': 0, 'message': 'No RSI data found'}
            level_distribution = {'message': 'No RSI data found'}
            momentum_stats = {'message': 'No RSI data found'}
            extreme_analysis = {'message': 'No RSI data found'}

        # Build comprehensive response
        rsi_response = {
            'ticker': stock_ticker,
            'indicator_type': 'RSI',
            'parameters': {
                'window': window,
                'series_type': series_type,
                'timespan': timespan,
                'adjusted': adjusted
            },
            'rsi_values': processed_rsi,
            'statistics': {
                'rsi_stats': rsi_stats,
                'level_distribution': level_distribution,
                'momentum_stats': momentum_stats,
                'extreme_analysis': extreme_analysis
            },
            'underlying_url': underlying_data.get('url') if underlying_data else None
        }

        # Include underlying aggregates if requested
        if expand_underlying and underlying_aggregates:
            rsi_response['underlying_aggregates'] = underlying_aggregates

        return {
            'success': True,
            'rsi_data': rsi_response,
            'next_url': result.get('next_url'),
            'request_id': result.get('request_id'),
            'status': result.get('status')
        }

    except Exception as e:
        return {"error": f"Failed to get RSI for {stock_ticker}: {str(e)}"}

def get_balance_sheets(
    cik: Optional[str] = None,
    cik_any_of: Optional[str] = None,
    cik_gt: Optional[str] = None,
    cik_gte: Optional[str] = None,
    cik_lt: Optional[str] = None,
    cik_lte: Optional[str] = None,
    tickers: Optional[str] = None,
    tickers_all_of: Optional[str] = None,
    tickers_any_of: Optional[str] = None,
    period_end: Optional[str] = None,
    period_end_gt: Optional[str] = None,
    period_end_gte: Optional[str] = None,
    period_end_lt: Optional[str] = None,
    period_end_lte: Optional[str] = None,
    filing_date: Optional[str] = None,
    filing_date_gt: Optional[str] = None,
    filing_date_gte: Optional[str] = None,
    filing_date_lt: Optional[str] = None,
    filing_date_lte: Optional[str] = None,
    fiscal_year: Optional[float] = None,
    fiscal_year_gt: Optional[float] = None,
    fiscal_year_gte: Optional[float] = None,
    fiscal_year_lt: Optional[float] = None,
    fiscal_year_lte: Optional[float] = None,
    fiscal_quarter: Optional[float] = None,
    fiscal_quarter_gt: Optional[float] = None,
    fiscal_quarter_gte: Optional[float] = None,
    fiscal_quarter_lt: Optional[float] = None,
    fiscal_quarter_lte: Optional[float] = None,
    timeframe: Optional[str] = None,
    timeframe_any_of: Optional[str] = None,
    timeframe_gt: Optional[str] = None,
    timeframe_gte: Optional[str] = None,
    timeframe_lt: Optional[str] = None,
    timeframe_lte: Optional[str] = None,
    limit: Optional[int] = None,
    sort: Optional[str] = None
) -> Dict:
    """
    Retrieve comprehensive balance sheet data for public companies from Polygon.io API.

    Args:
        cik: The company's Central Index Key (CIK)
        cik_any_of: Filter equal to any of the CIK values (comma separated)
        cik_gt/cik_gte/cik_lt/cik_lte: CIK range filters
        tickers: Filter for arrays that contain the ticker value
        tickers_all_of: Filter for arrays that contain all ticker values (comma separated)
        tickers_any_of: Filter for arrays that contain any ticker values (comma separated)
        period_end: The last date of the reporting period (yyyy-mm-dd)
        period_end_gt/period_end_gte/period_end_lt/period_end_lte: Period end range filters
        filing_date: The date when the financial statement was filed (yyyy-mm-dd)
        filing_date_gt/filing_date_gte/filing_date_lt/filing_date_lte: Filing date range filters
        fiscal_year/fiscal_year_gt/fiscal_year_gte/fiscal_year_lt/fiscal_year_lte: Fiscal year filters
        fiscal_quarter/fiscal_quarter_gt/fiscal_quarter_gte/fiscal_quarter_lt/fiscal_quarter_lte: Fiscal quarter filters
        timeframe: The reporting period type (quarterly, annual)
        timeframe_any_of: Filter equal to any timeframe values (comma separated)
        timeframe_gt/timeframe_gte/timeframe_lt/timeframe_lte: Timeframe range filters
        limit: Limit the maximum number of results (max 50000)
        sort: Sort columns and direction (e.g., 'period_end.desc')

    Returns:
        Dict containing balance sheet data with enhanced financial analytics
    """
    try:
        # Build API endpoint
        endpoint = "/stocks/financials/v1/balance-sheets"

        # Build query parameters
        params = {}

        # CIK parameters
        if cik:
            params['cik'] = cik
        if cik_any_of:
            params['cik.any_of'] = cik_any_of
        if cik_gt:
            params['cik.gt'] = cik_gt
        if cik_gte:
            params['cik.gte'] = cik_gte
        if cik_lt:
            params['cik.lt'] = cik_lt
        if cik_lte:
            params['cik.lte'] = cik_lte

        # Ticker parameters
        if tickers:
            params['tickers'] = tickers
        if tickers_all_of:
            params['tickers.all_of'] = tickers_all_of
        if tickers_any_of:
            params['tickers.any_of'] = tickers_any_of

        # Period end parameters
        if period_end:
            params['period_end'] = period_end
        if period_end_gt:
            params['period_end.gt'] = period_end_gt
        if period_end_gte:
            params['period_end.gte'] = period_end_gte
        if period_end_lt:
            params['period_end.lt'] = period_end_lt
        if period_end_lte:
            params['period_end.lte'] = period_end_lte

        # Filing date parameters
        if filing_date:
            params['filing_date'] = filing_date
        if filing_date_gt:
            params['filing_date.gt'] = filing_date_gt
        if filing_date_gte:
            params['filing_date.gte'] = filing_date_gte
        if filing_date_lt:
            params['filing_date.lt'] = filing_date_lt
        if filing_date_lte:
            params['filing_date.lte'] = filing_date_lte

        # Fiscal year parameters
        if fiscal_year is not None:
            params['fiscal_year'] = fiscal_year
        if fiscal_year_gt is not None:
            params['fiscal_year.gt'] = fiscal_year_gt
        if fiscal_year_gte is not None:
            params['fiscal_year.gte'] = fiscal_year_gte
        if fiscal_year_lt is not None:
            params['fiscal_year.lt'] = fiscal_year_lt
        if fiscal_year_lte is not None:
            params['fiscal_year.lte'] = fiscal_year_lte

        # Fiscal quarter parameters
        if fiscal_quarter is not None:
            params['fiscal_quarter'] = fiscal_quarter
        if fiscal_quarter_gt is not None:
            params['fiscal_quarter.gt'] = fiscal_quarter_gt
        if fiscal_quarter_gte is not None:
            params['fiscal_quarter.gte'] = fiscal_quarter_gte
        if fiscal_quarter_lt is not None:
            params['fiscal_quarter.lt'] = fiscal_quarter_lt
        if fiscal_quarter_lte is not None:
            params['fiscal_quarter.lte'] = fiscal_quarter_lte

        # Timeframe parameters
        if timeframe:
            params['timeframe'] = timeframe
        if timeframe_any_of:
            params['timeframe.any_of'] = timeframe_any_of
        if timeframe_gt:
            params['timeframe.gt'] = timeframe_gt
        if timeframe_gte:
            params['timeframe.gte'] = timeframe_gte
        if timeframe_lt:
            params['timeframe.lt'] = timeframe_lt
        if timeframe_lte:
            params['timeframe.lte'] = timeframe_lte

        # Other parameters
        if limit:
            params['limit'] = limit
        if sort:
            params['sort'] = sort

        # Make API request
        result = make_polygon_request(endpoint, params)

        if 'error' in result:
            return result

        # Enhanced analytics for balance sheet data
        if result.get('results'):
            for balance_sheet in result['results']:
                # Extract basic financial data
                total_assets = balance_sheet.get('total_assets', 0)
                total_current_assets = balance_sheet.get('total_current_assets', 0)
                total_liabilities = balance_sheet.get('total_liabilities', 0)
                total_current_liabilities = balance_sheet.get('total_current_liabilities', 0)
                total_equity = balance_sheet.get('total_equity', 0)
                total_equity_attributable_to_parent = balance_sheet.get('total_equity_attributable_to_parent', 0)
                cash_and_equivalents = balance_sheet.get('cash_and_equivalents', 0)
                short_term_investments = balance_sheet.get('short_term_investments', 0)
                receivables = balance_sheet.get('receivables', 0)
                inventories = balance_sheet.get('inventories', 0)
                property_plant_equipment_net = balance_sheet.get('property_plant_equipment_net', 0)
                goodwill = balance_sheet.get('goodwill', 0)
                intangible_assets_net = balance_sheet.get('intangible_assets_net', 0)
                long_term_debt = balance_sheet.get('long_term_debt_and_capital_lease_obligations', 0)
                debt_current = balance_sheet.get('debt_current', 0)
                accounts_payable = balance_sheet.get('accounts_payable', 0)

                # Calculate key financial ratios and metrics
                working_capital = total_current_assets - total_current_liabilities
                current_ratio = total_current_assets / total_current_liabilities if total_current_liabilities > 0 else None
                quick_ratio = (total_current_assets - inventories) / total_current_liabilities if total_current_liabilities > 0 else None
                cash_ratio = (cash_and_equivalents + short_term_investments) / total_current_liabilities if total_current_liabilities > 0 else None

                # Leverage ratios
                debt_to_equity = total_liabilities / total_equity if total_equity > 0 else None
                debt_to_assets = total_liabilities / total_assets if total_assets > 0 else None
                long_term_debt_to_equity = long_term_debt / total_equity if total_equity > 0 else None

                # Liquidity metrics
                total_liquid_assets = cash_and_equivalents + short_term_investments + receivables
                liquid_ratio = total_liquid_assets / total_current_liabilities if total_current_liabilities > 0 else None

                # Asset composition
                current_assets_ratio = total_current_assets / total_assets if total_assets > 0 else None
                ppe_ratio = property_plant_equipment_net / total_assets if total_assets > 0 else None
                intangible_ratio = (goodwill + intangible_assets_net) / total_assets if total_assets > 0 else None
                cash_ratio_to_assets = (cash_and_equivalents + short_term_investments) / total_assets if total_assets > 0 else None

                # Equity analysis
                equity_multiplier = total_assets / total_equity if total_equity > 0 else None
                book_value_per_share = total_equity_attributable_to_parent  # Would need shares outstanding for proper calculation
                retained_earnings_ratio = balance_sheet.get('retained_earnings_deficit', 0) / total_equity if total_equity > 0 else None

                # Create enhanced financial analytics
                balance_sheet['financial_ratios'] = {
                    'liquidity_ratios': {
                        'current_ratio': round(current_ratio, 4) if current_ratio else None,
                        'quick_ratio': round(quick_ratio, 4) if quick_ratio else None,
                        'cash_ratio': round(cash_ratio, 4) if cash_ratio else None,
                        'liquid_ratio': round(liquid_ratio, 4) if liquid_ratio else None,
                        'working_capital': working_capital
                    },
                    'leverage_ratios': {
                        'debt_to_equity': round(debt_to_equity, 4) if debt_to_equity else None,
                        'debt_to_assets': round(debt_to_assets, 4) if debt_to_assets else None,
                        'long_term_debt_to_equity': round(long_term_debt_to_equity, 4) if long_term_debt_to_equity else None,
                        'equity_multiplier': round(equity_multiplier, 4) if equity_multiplier else None
                    },
                    'asset_composition': {
                        'current_assets_ratio': round(current_assets_ratio, 4) if current_assets_ratio else None,
                        'ppe_ratio': round(ppe_ratio, 4) if ppe_ratio else None,
                        'intangible_ratio': round(intangible_ratio, 4) if intangible_ratio else None,
                        'cash_ratio_to_assets': round(cash_ratio_to_assets, 4) if cash_ratio_to_assets else None
                    }
                }

                # Financial health assessment
                health_score = 0
                health_factors = []

                # Current ratio assessment (ideal: 1.5-3.0)
                if current_ratio and current_ratio >= 1.5:
                    health_score += 20
                    health_factors.append("Strong current ratio")
                elif current_ratio and current_ratio >= 1.0:
                    health_score += 10
                    health_factors.append("Adequate current ratio")
                elif current_ratio:
                    health_factors.append("Low current ratio - potential liquidity concern")

                # Debt-to-equity assessment (ideal: < 0.5 for conservative, < 2.0 acceptable)
                if debt_to_equity and debt_to_equity < 0.5:
                    health_score += 20
                    health_factors.append("Very low debt levels")
                elif debt_to_equity and debt_to_equity < 1.0:
                    health_score += 15
                    health_factors.append("Moderate debt levels")
                elif debt_to_equity and debt_to_equity < 2.0:
                    health_score += 10
                    health_factors.append("Acceptable debt levels")
                elif debt_to_equity:
                    health_factors.append("High debt levels - financial risk concern")

                # Cash position assessment
                if cash_and_equivalents and total_current_liabilities:
                    cash_coverage = (cash_and_equivalents + short_term_investments) / total_current_liabilities
                    if cash_coverage >= 0.5:
                        health_score += 15
                        health_factors.append("Strong cash position")
                    elif cash_coverage >= 0.2:
                        health_score += 10
                        health_factors.append("Adequate cash position")
                    else:
                        health_factors.append("Low cash coverage - liquidity risk")

                # Working capital assessment
                if working_capital > 0:
                    health_score += 15
                    health_factors.append("Positive working capital")
                else:
                    health_factors.append("Negative working capital - financial distress indicator")

                # Equity base assessment
                if total_equity > 0:
                    health_score += 10
                    health_factors.append("Positive equity base")
                else:
                    health_factors.append("Negative equity - serious financial concern")

                # Asset quality assessment
                if intangible_ratio and intangible_ratio < 0.3:
                    health_score += 10
                    health_factors.append("High-quality asset base")
                elif intangible_ratio and intangible_ratio < 0.5:
                    health_score += 5
                    health_factors.append("Moderate intangible assets")
                else:
                    health_factors.append("High intangible assets - quality concern")

                # Overall financial health rating
                if health_score >= 80:
                    health_rating = "Excellent"
                elif health_score >= 60:
                    health_rating = "Good"
                elif health_score >= 40:
                    health_rating = "Fair"
                elif health_score >= 20:
                    health_rating = "Poor"
                else:
                    health_rating = "Very Poor"

                balance_sheet['financial_health'] = {
                    'health_score': health_score,
                    'health_rating': health_rating,
                    'health_factors': health_factors,
                    'assessment_date': datetime.now().isoformat()
                }

                # Add formatted currency values for better readability
                balance_sheet['formatted_values'] = {
                    'total_assets': f"${total_assets:,.0f}" if total_assets else "$0",
                    'total_current_assets': f"${total_current_assets:,.0f}" if total_current_assets else "$0",
                    'total_liabilities': f"${total_liabilities:,.0f}" if total_liabilities else "$0",
                    'total_current_liabilities': f"${total_current_liabilities:,.0f}" if total_current_liabilities else "$0",
                    'total_equity': f"${total_equity:,.0f}" if total_equity else "$0",
                    'cash_and_equivalents': f"${cash_and_equivalents:,.0f}" if cash_and_equivalents else "$0",
                    'working_capital': f"${working_capital:,.0f}" if working_capital else "$0",
                    'property_plant_equipment_net': f"${property_plant_equipment_net:,.0f}" if property_plant_equipment_net else "$0",
                    'long_term_debt': f"${long_term_debt:,.0f}" if long_term_debt else "$0"
                }

                # Add period analysis
                if balance_sheet.get('period_end') and balance_sheet.get('fiscal_year'):
                    period_info = {
                        'period_type': balance_sheet.get('timeframe', 'unknown'),
                        'fiscal_year': balance_sheet.get('fiscal_year'),
                        'fiscal_quarter': balance_sheet.get('fiscal_quarter'),
                        'period_end_date': balance_sheet.get('period_end'),
                        'filing_date': balance_sheet.get('filing_date'),
                        'days_to_file': None
                    }

                    # Calculate days between period end and filing
                    if balance_sheet.get('period_end') and balance_sheet.get('filing_date'):
                        try:
                            period_end = datetime.strptime(balance_sheet['period_end'], '%Y-%m-%d')
                            filing_date = datetime.strptime(balance_sheet['filing_date'], '%Y-%m-%d')
                            days_to_file = (filing_date - period_end).days
                            period_info['days_to_file'] = days_to_file

                            if days_to_file <= 30:
                                period_info['filing_timeliness'] = 'Excellent'
                            elif days_to_file <= 45:
                                period_info['filing_timeliness'] = 'Good'
                            elif days_to_file <= 60:
                                period_info['filing_timeliness'] = 'Average'
                            else:
                                period_info['filing_timeliness'] = 'Delayed'
                        except ValueError:
                            period_info['filing_timeliness'] = 'Unknown'

                    balance_sheet['period_analysis'] = period_info

        # Add summary statistics
        if result.get('results'):
            balance_sheets = result['results']

            # Calculate aggregate metrics across all periods
            total_companies = len(set(sheet.get('cik') for sheet in balance_sheets if sheet.get('cik')))
            unique_tickers = set()
            for sheet in balance_sheets:
                if sheet.get('tickers'):
                    unique_tickers.update(sheet['tickers'])

            timeframes = {}
            fiscal_years = []
            total_assets_sum = 0
            total_equity_sum = 0

            for sheet in balance_sheets:
                timeframe = sheet.get('timeframe', 'unknown')
                timeframes[timeframe] = timeframes.get(timeframe, 0) + 1

                fiscal_year = sheet.get('fiscal_year')
                if fiscal_year:
                    fiscal_years.append(fiscal_year)

                total_assets_sum += sheet.get('total_assets', 0)
                total_equity_sum += sheet.get('total_equity', 0)

            summary_stats = {
                'total_balance_sheets': len(balance_sheets),
                'unique_companies': total_companies,
                'unique_tickers': list(unique_tickers),
                'ticker_count': len(unique_tickers),
                'timeframe_distribution': timeframes,
                'fiscal_year_range': {
                    'earliest': min(fiscal_years) if fiscal_years else None,
                    'latest': max(fiscal_years) if fiscal_years else None,
                    'count': len(set(fiscal_years)) if fiscal_years else 0
                },
                'aggregate_metrics': {
                    'total_assets_all_periods': total_assets_sum,
                    'total_equity_all_periods': total_equity_sum,
                    'average_assets_per_sheet': total_assets_sum / len(balance_sheets) if balance_sheets else 0,
                    'average_equity_per_sheet': total_equity_sum / len(balance_sheets) if balance_sheets else 0
                },
                'query_summary': {
                    'parameters_used': {
                        'cik': cik,
                        'tickers': tickers,
                        'timeframe': timeframe,
                        'fiscal_year': fiscal_year,
                        'limit': limit,
                        'sort': sort
                    },
                    'result_count': len(balance_sheets),
                    'has_next_page': bool(result.get('next_url'))
                }
            }

            result['summary_statistics'] = summary_stats

        return {
            'success': True,
            'balance_sheet_data': result,
            'query_parameters': {
                'cik': cik,
                'tickers': tickers,
                'timeframe': timeframe,
                'fiscal_year': fiscal_year,
                'fiscal_quarter': fiscal_quarter,
                'limit': limit,
                'sort': sort
            }
        }

    except Exception as e:
        return {"error": f"Failed to get balance sheets: {str(e)}"}

def get_cash_flow_statements(
    cik: Optional[str] = None,
    cik_any_of: Optional[str] = None,
    cik_gt: Optional[str] = None,
    cik_gte: Optional[str] = None,
    cik_lt: Optional[str] = None,
    cik_lte: Optional[str] = None,
    period_end: Optional[str] = None,
    period_end_gt: Optional[str] = None,
    period_end_gte: Optional[str] = None,
    period_end_lt: Optional[str] = None,
    period_end_lte: Optional[str] = None,
    filing_date: Optional[str] = None,
    filing_date_gt: Optional[str] = None,
    filing_date_gte: Optional[str] = None,
    filing_date_lt: Optional[str] = None,
    filing_date_lte: Optional[str] = None,
    tickers: Optional[str] = None,
    tickers_all_of: Optional[str] = None,
    tickers_any_of: Optional[str] = None,
    fiscal_year: Optional[float] = None,
    fiscal_year_gt: Optional[float] = None,
    fiscal_year_gte: Optional[float] = None,
    fiscal_year_lt: Optional[float] = None,
    fiscal_year_lte: Optional[float] = None,
    fiscal_quarter: Optional[float] = None,
    fiscal_quarter_gt: Optional[float] = None,
    fiscal_quarter_gte: Optional[float] = None,
    fiscal_quarter_lt: Optional[float] = None,
    fiscal_quarter_lte: Optional[float] = None,
    timeframe: Optional[str] = None,
    timeframe_any_of: Optional[str] = None,
    timeframe_gt: Optional[str] = None,
    timeframe_gte: Optional[str] = None,
    timeframe_lt: Optional[str] = None,
    timeframe_lte: Optional[str] = None,
    limit: Optional[int] = None,
    sort: Optional[str] = None
) -> Dict:
    """
    Retrieve comprehensive cash flow statement data for public companies from Polygon.io API.

    Args:
        cik: The company's Central Index Key (CIK)
        cik_any_of: Filter equal to any of the CIK values (comma separated)
        cik_gt/cik_gte/cik_lt/cik_lte: CIK range filters
        period_end: The last date of the reporting period (yyyy-mm-dd)
        period_end_gt/period_end_gte/period_end_lt/period_end_lte: Period end range filters
        filing_date: The date when the financial statement was filed (yyyy-mm-dd)
        filing_date_gt/filing_date_gte/filing_date_lt/filing_date_lte: Filing date range filters
        tickers: Filter for arrays that contain the ticker value
        tickers_all_of: Filter for arrays that contain all ticker values (comma separated)
        tickers_any_of: Filter for arrays that contain any ticker values (comma separated)
        fiscal_year/fiscal_year_gt/fiscal_year_gte/fiscal_year_lt/fiscal_year_lte: Fiscal year filters
        fiscal_quarter/fiscal_quarter_gt/fiscal_quarter_gte/fiscal_quarter_lt/fiscal_quarter_lte: Fiscal quarter filters
        timeframe: The reporting period type (quarterly, annual, trailing_twelve_months)
        timeframe_any_of: Filter equal to any timeframe values (comma separated)
        timeframe_gt/timeframe_gte/timeframe_lt/timeframe_lte: Timeframe range filters
        limit: Limit the maximum number of results (max 50000)
        sort: Sort columns and direction (e.g., 'period_end.desc')

    Returns:
        Dict containing cash flow statement data with enhanced cash flow analytics
    """
    try:
        # Build API endpoint
        endpoint = "/stocks/financials/v1/cash-flow-statements"

        # Build query parameters
        params = {}

        # CIK parameters
        if cik:
            params['cik'] = cik
        if cik_any_of:
            params['cik.any_of'] = cik_any_of
        if cik_gt:
            params['cik.gt'] = cik_gt
        if cik_gte:
            params['cik.gte'] = cik_gte
        if cik_lt:
            params['cik.lt'] = cik_lt
        if cik_lte:
            params['cik.lte'] = cik_lte

        # Period end parameters
        if period_end:
            params['period_end'] = period_end
        if period_end_gt:
            params['period_end.gt'] = period_end_gt
        if period_end_gte:
            params['period_end.gte'] = period_end_gte
        if period_end_lt:
            params['period_end.lt'] = period_end_lt
        if period_end_lte:
            params['period_end.lte'] = period_end_lte

        # Filing date parameters
        if filing_date:
            params['filing_date'] = filing_date
        if filing_date_gt:
            params['filing_date.gt'] = filing_date_gt
        if filing_date_gte:
            params['filing_date.gte'] = filing_date_gte
        if filing_date_lt:
            params['filing_date.lt'] = filing_date_lt
        if filing_date_lte:
            params['filing_date.lte'] = filing_date_lte

        # Ticker parameters
        if tickers:
            params['tickers'] = tickers
        if tickers_all_of:
            params['tickers.all_of'] = tickers_all_of
        if tickers_any_of:
            params['tickers.any_of'] = tickers_any_of

        # Fiscal year parameters
        if fiscal_year is not None:
            params['fiscal_year'] = fiscal_year
        if fiscal_year_gt is not None:
            params['fiscal_year.gt'] = fiscal_year_gt
        if fiscal_year_gte is not None:
            params['fiscal_year.gte'] = fiscal_year_gte
        if fiscal_year_lt is not None:
            params['fiscal_year.lt'] = fiscal_year_lt
        if fiscal_year_lte is not None:
            params['fiscal_year.lte'] = fiscal_year_lte

        # Fiscal quarter parameters
        if fiscal_quarter is not None:
            params['fiscal_quarter'] = fiscal_quarter
        if fiscal_quarter_gt is not None:
            params['fiscal_quarter.gt'] = fiscal_quarter_gt
        if fiscal_quarter_gte is not None:
            params['fiscal_quarter.gte'] = fiscal_quarter_gte
        if fiscal_quarter_lt is not None:
            params['fiscal_quarter.lt'] = fiscal_quarter_lt
        if fiscal_quarter_lte is not None:
            params['fiscal_quarter.lte'] = fiscal_quarter_lte

        # Timeframe parameters
        if timeframe:
            params['timeframe'] = timeframe
        if timeframe_any_of:
            params['timeframe.any_of'] = timeframe_any_of
        if timeframe_gt:
            params['timeframe.gt'] = timeframe_gt
        if timeframe_gte:
            params['timeframe.gte'] = timeframe_gte
        if timeframe_lt:
            params['timeframe.lt'] = timeframe_lt
        if timeframe_lte:
            params['timeframe.lte'] = timeframe_lte

        # Other parameters
        if limit:
            params['limit'] = limit
        if sort:
            params['sort'] = sort

        # Make API request
        result = make_polygon_request(endpoint, params)

        if 'error' in result:
            return result

        # Enhanced analytics for cash flow statement data
        if result.get('results'):
            for cash_flow in result['results']:
                # Extract basic cash flow data
                net_income = cash_flow.get('net_income', 0)
                net_cash_from_operating_activities = cash_flow.get('net_cash_from_operating_activities', 0)
                net_cash_from_investing_activities = cash_flow.get('net_cash_from_investing_activities', 0)
                net_cash_from_financing_activities = cash_flow.get('net_cash_from_financing_activities', 0)
                change_in_cash_and_equivalents = cash_flow.get('change_in_cash_and_equivalents', 0)

                # Operating activities details
                cash_from_operating_activities_continuing = cash_flow.get('cash_from_operating_activities_continuing_operations', 0)
                depreciation_depletion_and_amortization = cash_flow.get('depreciation_depletion_and_amortization', 0)
                other_operating_activities = cash_flow.get('other_operating_activities', 0)
                change_in_other_operating_assets_and_liabilities_net = cash_flow.get('change_in_other_operating_assets_and_liabilities_net', 0)

                # Investing activities details
                purchase_of_property_plant_and_equipment = cash_flow.get('purchase_of_property_plant_and_equipment', 0)
                sale_of_property_plant_and_equipment = cash_flow.get('sale_of_property_plant_and_equipment', 0)
                other_investing_activities = cash_flow.get('other_investing_activities', 0)

                # Financing activities details
                dividends = cash_flow.get('dividends', 0)
                long_term_debt_issuances_repayments = cash_flow.get('long_term_debt_issuances_repayments', 0)
                short_term_debt_issuances_repayments = cash_flow.get('short_term_debt_issuances_repayments', 0)
                other_financing_activities = cash_flow.get('other_financing_activities', 0)

                # Additional cash flow items
                effect_of_currency_exchange_rate = cash_flow.get('effect_of_currency_exchange_rate', 0)
                income_loss_from_discontinued_operations = cash_flow.get('income_loss_from_discontinued_operations', 0)
                noncontrolling_interests = cash_flow.get('noncontrolling_interests', 0)
                other_cash_adjustments = cash_flow.get('other_cash_adjustments', 0)

                # Calculate key cash flow ratios and metrics
                operating_cash_flow_margin = (net_cash_from_operating_activities / abs(net_income)) if net_income != 0 else None
                free_cash_flow = net_cash_from_operating_activities + purchase_of_property_plant_and_equipment  # CapEx is negative
                cash_flow_to_debt_ratio = (net_cash_from_operating_activities / abs(long_term_debt_issuances_repayments)) if long_term_debt_issuances_repayments != 0 else None
                capex_ratio = (abs(purchase_of_property_plant_and_equipment) / net_cash_from_operating_activities) if net_cash_from_operating_activities != 0 else None
                dividend_payout_ratio = (abs(dividends) / net_cash_from_operating_activities) if net_cash_from_operating_activities > 0 else None
                debt_repayment_capacity = (net_cash_from_operating_activities / abs(long_term_debt_issuances_repayments)) if long_term_debt_issuances_repayments < 0 else None

                # Cash flow quality metrics
                cash_conversion_quality = (depreciation_depletion_and_amortization / abs(net_cash_from_operating_activities)) if net_cash_from_operating_activities != 0 else None
                working_capital_efficiency = (change_in_other_operating_assets_and_liabilities_net / net_cash_from_operating_activities) if net_cash_from_operating_activities != 0 else None

                # Investment and financing analysis
                net_investment = purchase_of_property_plant_and_equipment + sale_of_property_plant_and_equipment + other_investing_activities
                net_financing = long_term_debt_issuances_repayments + short_term_debt_issuances_repayments + dividends + other_financing_activities
                self_financing_ratio = (net_cash_from_operating_activities / abs(purchase_of_property_plant_and_equipment)) if purchase_of_property_plant_and_equipment != 0 else None

                # Create enhanced cash flow analytics
                cash_flow['cash_flow_ratios'] = {
                    'profitability_metrics': {
                        'operating_cash_flow_margin': round(operating_cash_flow_margin, 4) if operating_cash_flow_margin else None,
                        'free_cash_flow': free_cash_flow,
                        'cash_flow_to_debt_ratio': round(cash_flow_to_debt_ratio, 4) if cash_flow_to_debt_ratio else None,
                        'self_financing_ratio': round(self_financing_ratio, 4) if self_financing_ratio else None
                    },
                    'investment_metrics': {
                        'capex_ratio': round(capex_ratio, 4) if capex_ratio else None,
                        'capital_expenditure': purchase_of_property_plant_and_equipment,
                        'net_investment': net_investment,
                        'investment_to_operating_cf_ratio': round(net_investment / abs(net_cash_from_operating_activities), 4) if net_cash_from_operating_activities != 0 else None
                    },
                    'financing_metrics': {
                        'dividend_payout_ratio': round(dividend_payout_ratio, 4) if dividend_payout_ratio else None,
                        'debt_repayment_capacity': round(debt_repayment_capacity, 4) if debt_repayment_capacity else None,
                        'net_financing': net_financing,
                        'dividend_yield_to_cf': round(abs(dividends) / net_cash_from_operating_activities, 4) if net_cash_from_operating_activities > 0 else None
                    },
                    'quality_metrics': {
                        'cash_conversion_quality': round(cash_conversion_quality, 4) if cash_conversion_quality else None,
                        'working_capital_efficiency': round(working_capital_efficiency, 4) if working_capital_efficiency else None,
                        'non_cash_items_ratio': round(depreciation_depletion_and_amortization / abs(net_income), 4) if net_income != 0 else None
                    }
                }

                # Cash flow health assessment
                health_score = 0
                health_factors = []

                # Operating cash flow assessment (positive is healthy)
                if net_cash_from_operating_activities > 0:
                    health_score += 25
                    health_factors.append("Positive operating cash flow")
                    if operating_cash_flow_margin and operating_cash_flow_margin > 1.0:
                        health_score += 10
                        health_factors.append("Strong operating cash flow margin")
                    elif operating_cash_flow_margin and operating_cash_flow_margin > 0.8:
                        health_score += 5
                        health_factors.append("Good operating cash flow margin")
                else:
                    health_factors.append("Negative operating cash flow - concern")

                # Free cash flow assessment
                if free_cash_flow > 0:
                    health_score += 20
                    health_factors.append("Positive free cash flow")
                    if free_cash_flow > net_income * 0.8:
                        health_score += 10
                        health_factors.append("Strong free cash flow generation")
                else:
                    health_factors.append("Negative free cash flow - investment concern")

                # Debt management assessment
                if long_term_debt_issuances_repayments < 0:  # Debt repayment
                    if debt_repayment_capacity and debt_repayment_capacity > 1.5:
                        health_score += 15
                        health_factors.append("Strong debt repayment capacity")
                    elif debt_repayment_capacity and debt_repayment_capacity > 1.0:
                        health_score += 10
                        health_factors.append("Adequate debt repayment capacity")
                    else:
                        health_factors.append("Limited debt repayment capacity")
                elif long_term_debt_issuances_repayments > 0:
                    health_score += 5
                    health_factors.append("Debt financing for growth")

                # Dividend sustainability assessment
                if dividends < 0:  # Paying dividends
                    if dividend_payout_ratio and dividend_payout_ratio < 0.4:
                        health_score += 10
                        health_factors.append("Conservative dividend payout")
                    elif dividend_payout_ratio and dividend_payout_ratio < 0.6:
                        health_score += 5
                        health_factors.append("Moderate dividend payout")
                    else:
                        health_factors.append("High dividend payout - sustainability concern")

                # Investment assessment
                if purchase_of_property_plant_and_equipment < 0:  # Making investments
                    if capex_ratio and capex_ratio < 0.5:
                        health_score += 10
                        health_factors.append("Moderate capital expenditure")
                    elif capex_ratio and capex_ratio < 0.8:
                        health_score += 5
                        health_factors.append("Reasonable capital expenditure")
                    else:
                        health_factors.append("High capital expenditure - cash pressure")

                # Cash change assessment
                if change_in_cash_and_equivalents > 0:
                    health_score += 10
                    health_factors.append("Positive cash change")
                else:
                    health_factors.append("Negative cash change - liquidity concern")

                # Overall cash flow health rating
                if health_score >= 80:
                    health_rating = "Excellent"
                elif health_score >= 60:
                    health_rating = "Good"
                elif health_score >= 40:
                    health_rating = "Fair"
                elif health_score >= 20:
                    health_rating = "Poor"
                else:
                    health_rating = "Very Poor"

                cash_flow['cash_flow_health'] = {
                    'health_score': health_score,
                    'health_rating': health_rating,
                    'health_factors': health_factors,
                    'assessment_date': datetime.now().isoformat()
                }

                # Cash flow classification
                cf_pattern = "Unknown"
                if net_cash_from_operating_activities > 0 and net_cash_from_investing_activities < 0 and net_cash_from_financing_activities < 0:
                    cf_pattern = "Mature/Growth"  # Strong operations, investing, paying debt/dividends
                elif net_cash_from_operating_activities > 0 and net_cash_from_investing_activities < 0 and net_cash_from_financing_activities > 0:
                    cf_pattern = "Growth/Expansion"  # Strong operations, investing, raising capital
                elif net_cash_from_operating_activities > 0 and net_cash_from_investing_activities > 0 and net_cash_from_financing_activities > 0:
                    cf_pattern = "Financing/Restructuring"  # Selling assets, raising capital
                elif net_cash_from_operating_activities < 0 and net_cash_from_investing_activities > 0 and net_cash_from_financing_activities > 0:
                    cf_pattern = "Distress/Turnaround"  # Weak operations, selling assets, raising capital

                cash_flow['cash_flow_pattern'] = {
                    'pattern': cf_pattern,
                    'operating_cash_flow': net_cash_from_operating_activities,
                    'investing_cash_flow': net_cash_from_investing_activities,
                    'financing_cash_flow': net_cash_from_financing_activities,
                    'net_cash_change': change_in_cash_and_equivalents
                }

                # Add formatted currency values for better readability
                cash_flow['formatted_values'] = {
                    'net_income': f"${net_income:,.0f}" if net_income else "$0",
                    'operating_cash_flow': f"${net_cash_from_operating_activities:,.0f}" if net_cash_from_operating_activities else "$0",
                    'investing_cash_flow': f"${net_cash_from_investing_activities:,.0f}" if net_cash_from_investing_activities else "$0",
                    'financing_cash_flow': f"${net_cash_from_financing_activities:,.0f}" if net_cash_from_financing_activities else "$0",
                    'free_cash_flow': f"${free_cash_flow:,.0f}" if free_cash_flow else "$0",
                    'change_in_cash': f"${change_in_cash_and_equivalents:,.0f}" if change_in_cash_and_equivalents else "$0",
                    'capital_expenditure': f"${purchase_of_property_plant_and_equipment:,.0f}" if purchase_of_property_plant_and_equipment else "$0",
                    'dividends': f"${dividends:,.0f}" if dividends else "$0"
                }

                # Add period analysis
                if cash_flow.get('period_end') and cash_flow.get('fiscal_year'):
                    period_info = {
                        'period_type': cash_flow.get('timeframe', 'unknown'),
                        'fiscal_year': cash_flow.get('fiscal_year'),
                        'fiscal_quarter': cash_flow.get('fiscal_quarter'),
                        'period_end_date': cash_flow.get('period_end'),
                        'filing_date': cash_flow.get('filing_date'),
                        'days_to_file': None
                    }

                    # Calculate days between period end and filing
                    if cash_flow.get('period_end') and cash_flow.get('filing_date'):
                        try:
                            period_end = datetime.strptime(cash_flow['period_end'], '%Y-%m-%d')
                            filing_date = datetime.strptime(cash_flow['filing_date'], '%Y-%m-%d')
                            days_to_file = (filing_date - period_end).days
                            period_info['days_to_file'] = days_to_file

                            if days_to_file <= 30:
                                period_info['filing_timeliness'] = 'Excellent'
                            elif days_to_file <= 45:
                                period_info['filing_timeliness'] = 'Good'
                            elif days_to_file <= 60:
                                period_info['filing_timeliness'] = 'Average'
                            else:
                                period_info['filing_timeliness'] = 'Delayed'
                        except ValueError:
                            period_info['filing_timeliness'] = 'Unknown'

                    cash_flow['period_analysis'] = period_info

        # Add summary statistics
        if result.get('results'):
            cash_flows = result['results']

            # Calculate aggregate metrics across all periods
            total_companies = len(set(cf.get('cik') for cf in cash_flows if cf.get('cik')))
            unique_tickers = set()
            for cf in cash_flows:
                if cf.get('tickers'):
                    unique_tickers.update(cf['tickers'])

            timeframes = {}
            fiscal_years = []
            total_operating_cf = 0
            total_investing_cf = 0
            total_financing_cf = 0
            total_free_cf = 0
            positive_ocf_count = 0
            positive_fcf_count = 0

            for cf in cash_flows:
                timeframe = cf.get('timeframe', 'unknown')
                timeframes[timeframe] = timeframes.get(timeframe, 0) + 1

                fiscal_year = cf.get('fiscal_year')
                if fiscal_year:
                    fiscal_years.append(fiscal_year)

                total_operating_cf += cf.get('net_cash_from_operating_activities', 0)
                total_investing_cf += cf.get('net_cash_from_investing_activities', 0)
                total_financing_cf += cf.get('net_cash_from_financing_activities', 0)

                # Calculate free cash flow
                free_cf = cf.get('net_cash_from_operating_activities', 0) + cf.get('purchase_of_property_plant_and_equipment', 0)
                total_free_cf += free_cf

                if cf.get('net_cash_from_operating_activities', 0) > 0:
                    positive_ocf_count += 1
                if free_cf > 0:
                    positive_fcf_count += 1

            summary_stats = {
                'total_cash_flow_statements': len(cash_flows),
                'unique_companies': total_companies,
                'unique_tickers': list(unique_tickers),
                'ticker_count': len(unique_tickers),
                'timeframe_distribution': timeframes,
                'fiscal_year_range': {
                    'earliest': min(fiscal_years) if fiscal_years else None,
                    'latest': max(fiscal_years) if fiscal_years else None,
                    'count': len(set(fiscal_years)) if fiscal_years else 0
                },
                'aggregate_cash_flows': {
                    'total_operating_cash_flow': total_operating_cf,
                    'total_investing_cash_flow': total_investing_cf,
                    'total_financing_cash_flow': total_financing_cf,
                    'total_free_cash_flow': total_free_cf,
                    'average_operating_cash_flow': total_operating_cf / len(cash_flows) if cash_flows else 0,
                    'average_free_cash_flow': total_free_cf / len(cash_flows) if cash_flows else 0
                },
                'cash_flow_quality_metrics': {
                    'positive_operating_cash_flow_ratio': positive_ocf_count / len(cash_flows) if cash_flows else 0,
                    'positive_free_cash_flow_ratio': positive_fcf_count / len(cash_flows) if cash_flows else 0,
                    'average_cash_flow_health_score': sum(cf.get('cash_flow_health', {}).get('health_score', 0) for cf in cash_flows) / len(cash_flows) if cash_flows else 0
                },
                'query_summary': {
                    'parameters_used': {
                        'cik': cik,
                        'tickers': tickers,
                        'timeframe': timeframe,
                        'fiscal_year': fiscal_year,
                        'limit': limit,
                        'sort': sort
                    },
                    'result_count': len(cash_flows),
                    'has_next_page': bool(result.get('next_url'))
                }
            }

            result['summary_statistics'] = summary_stats

        return {
            'success': True,
            'cash_flow_data': result,
            'query_parameters': {
                'cik': cik,
                'tickers': tickers,
                'timeframe': timeframe,
                'fiscal_year': fiscal_year,
                'fiscal_quarter': fiscal_quarter,
                'limit': limit,
                'sort': sort
            }
        }

    except Exception as e:
        return {"error": f"Failed to get cash flow statements: {str(e)}"}

def get_income_statements(
    cik: Optional[str] = None,
    cik_any_of: Optional[str] = None,
    cik_gt: Optional[str] = None,
    cik_gte: Optional[str] = None,
    cik_lt: Optional[str] = None,
    cik_lte: Optional[str] = None,
    tickers: Optional[str] = None,
    tickers_all_of: Optional[str] = None,
    tickers_any_of: Optional[str] = None,
    period_end: Optional[str] = None,
    period_end_gt: Optional[str] = None,
    period_end_gte: Optional[str] = None,
    period_end_lt: Optional[str] = None,
    period_end_lte: Optional[str] = None,
    filing_date: Optional[str] = None,
    filing_date_gt: Optional[str] = None,
    filing_date_gte: Optional[str] = None,
    filing_date_lt: Optional[str] = None,
    filing_date_lte: Optional[str] = None,
    fiscal_year: Optional[float] = None,
    fiscal_year_gt: Optional[float] = None,
    fiscal_year_gte: Optional[float] = None,
    fiscal_year_lt: Optional[float] = None,
    fiscal_year_lte: Optional[float] = None,
    fiscal_quarter: Optional[float] = None,
    fiscal_quarter_gt: Optional[float] = None,
    fiscal_quarter_gte: Optional[float] = None,
    fiscal_quarter_lt: Optional[float] = None,
    fiscal_quarter_lte: Optional[float] = None,
    timeframe: Optional[str] = None,
    timeframe_any_of: Optional[str] = None,
    timeframe_gt: Optional[str] = None,
    timeframe_gte: Optional[str] = None,
    timeframe_lt: Optional[str] = None,
    timeframe_lte: Optional[str] = None,
    limit: Optional[int] = None,
    sort: Optional[str] = None
) -> Dict:
    """
    Retrieve comprehensive income statement data for public companies from Polygon.io API.

    Args:
        cik: The company's Central Index Key (CIK)
        cik_any_of: Filter equal to any of the CIK values (comma separated)
        cik_gt/cik_gte/cik_lt/cik_lte: CIK range filters
        tickers: Filter for arrays that contain the ticker value
        tickers_all_of: Filter for arrays that contain all ticker values (comma separated)
        tickers_any_of: Filter for arrays that contain any ticker values (comma separated)
        period_end: The last date of the reporting period (yyyy-mm-dd)
        period_end_gt/period_end_gte/period_end_lt/period_end_lte: Period end range filters
        filing_date: The date when the financial statement was filed (yyyy-mm-dd)
        filing_date_gt/filing_date_gte/filing_date_lt/filing_date_lte: Filing date range filters
        fiscal_year/fiscal_year_gt/fiscal_year_gte/fiscal_year_lt/fiscal_year_lte: Fiscal year filters
        fiscal_quarter/fiscal_quarter_gt/fiscal_quarter_gte/fiscal_quarter_lt/fiscal_quarter_lte: Fiscal quarter filters
        timeframe: The reporting period type (quarterly, annual, trailing_twelve_months)
        timeframe_any_of: Filter equal to any timeframe values (comma separated)
        timeframe_gt/timeframe_gte/timeframe_lt/timeframe_lte: Timeframe range filters
        limit: Limit the maximum number of results (max 50000)
        sort: Sort columns and direction (e.g., 'period_end.desc')

    Returns:
        Dict containing income statement data with enhanced profitability analytics
    """
    try:
        # Build API endpoint
        endpoint = "/stocks/financials/v1/income-statements"

        # Build query parameters
        params = {}

        # CIK parameters
        if cik:
            params['cik'] = cik
        if cik_any_of:
            params['cik.any_of'] = cik_any_of
        if cik_gt:
            params['cik.gt'] = cik_gt
        if cik_gte:
            params['cik.gte'] = cik_gte
        if cik_lt:
            params['cik.lt'] = cik_lt
        if cik_lte:
            params['cik.lte'] = cik_lte

        # Ticker parameters
        if tickers:
            params['tickers'] = tickers
        if tickers_all_of:
            params['tickers.all_of'] = tickers_all_of
        if tickers_any_of:
            params['tickers.any_of'] = tickers_any_of

        # Period end parameters
        if period_end:
            params['period_end'] = period_end
        if period_end_gt:
            params['period_end.gt'] = period_end_gt
        if period_end_gte:
            params['period_end.gte'] = period_end_gte
        if period_end_lt:
            params['period_end.lt'] = period_end_lt
        if period_end_lte:
            params['period_end.lte'] = period_end_lte

        # Filing date parameters
        if filing_date:
            params['filing_date'] = filing_date
        if filing_date_gt:
            params['filing_date.gt'] = filing_date_gt
        if filing_date_gte:
            params['filing_date.gte'] = filing_date_gte
        if filing_date_lt:
            params['filing_date.lt'] = filing_date_lt
        if filing_date_lte:
            params['filing_date.lte'] = filing_date_lte

        # Fiscal year parameters
        if fiscal_year is not None:
            params['fiscal_year'] = fiscal_year
        if fiscal_year_gt is not None:
            params['fiscal_year.gt'] = fiscal_year_gt
        if fiscal_year_gte is not None:
            params['fiscal_year.gte'] = fiscal_year_gte
        if fiscal_year_lt is not None:
            params['fiscal_year.lt'] = fiscal_year_lt
        if fiscal_year_lte is not None:
            params['fiscal_year.lte'] = fiscal_year_lte

        # Fiscal quarter parameters
        if fiscal_quarter is not None:
            params['fiscal_quarter'] = fiscal_quarter
        if fiscal_quarter_gt is not None:
            params['fiscal_quarter.gt'] = fiscal_quarter_gt
        if fiscal_quarter_gte is not None:
            params['fiscal_quarter.gte'] = fiscal_quarter_gte
        if fiscal_quarter_lt is not None:
            params['fiscal_quarter.lt'] = fiscal_quarter_lt
        if fiscal_quarter_lte is not None:
            params['fiscal_quarter.lte'] = fiscal_quarter_lte

        # Timeframe parameters
        if timeframe:
            params['timeframe'] = timeframe
        if timeframe_any_of:
            params['timeframe.any_of'] = timeframe_any_of
        if timeframe_gt:
            params['timeframe.gt'] = timeframe_gt
        if timeframe_gte:
            params['timeframe.gte'] = timeframe_gte
        if timeframe_lt:
            params['timeframe.lt'] = timeframe_lt
        if timeframe_lte:
            params['timeframe.lte'] = timeframe_lte

        # Other parameters
        if limit:
            params['limit'] = limit
        if sort:
            params['sort'] = sort

        # Make API request
        result = make_polygon_request(endpoint, params)

        if 'error' in result:
            return result

        # Enhanced analytics for income statement data
        if result.get('results'):
            for income_statement in result['results']:
                # Extract basic income statement data
                revenue = income_statement.get('revenue', 0)
                cost_of_revenue = income_statement.get('cost_of_revenue', 0)
                gross_profit = income_statement.get('gross_profit', 0)
                operating_income = income_statement.get('operating_income', 0)
                income_before_income_taxes = income_statement.get('income_before_income_taxes', 0)
                income_taxes = income_statement.get('income_taxes', 0)
                net_income_loss_attributable_common_shareholders = income_statement.get('net_income_loss_attributable_common_shareholders', 0)
                consolidated_net_income_loss = income_statement.get('consolidated_net_income_loss', 0)

                # Operating expense details
                selling_general_administrative = income_statement.get('selling_general_administrative', 0)
                research_development = income_statement.get('research_development', 0)
                depreciation_depletion_amortization = income_statement.get('depreciation_depletion_amortization', 0)
                other_operating_expenses = income_statement.get('other_operating_expenses', 0)
                total_operating_expenses = income_statement.get('total_operating_expenses', 0)

                # Other income/expense items
                interest_income = income_statement.get('interest_income', 0)
                interest_expense = income_statement.get('interest_expense', 0)
                other_income_expense = income_statement.get('other_income_expense', 0)
                total_other_income_expense = income_statement.get('total_other_income_expense', 0)

                # Additional profitability metrics
                ebitda = income_statement.get('ebitda', 0)
                equity_in_affiliates = income_statement.get('equity_in_affiliates', 0)
                discontinued_operations = income_statement.get('discontinued_operations', 0)
                extraordinary_items = income_statement.get('extraordinary_items', 0)
                noncontrolling_interest = income_statement.get('noncontrolling_interest', 0)
                preferred_stock_dividends_declared = income_statement.get('preferred_stock_dividends_declared', 0)

                # EPS data
                basic_earnings_per_share = income_statement.get('basic_earnings_per_share', 0)
                diluted_earnings_per_share = income_statement.get('diluted_earnings_per_share', 0)
                basic_shares_outstanding = income_statement.get('basic_shares_outstanding', 0)
                diluted_shares_outstanding = income_statement.get('diluted_shares_outstanding', 0)

                # Calculate key profitability ratios and metrics
                gross_profit_margin = (gross_profit / revenue) if revenue != 0 else None
                operating_profit_margin = (operating_income / revenue) if revenue != 0 else None
                net_profit_margin = (net_income_loss_attributable_common_shareholders / revenue) if revenue != 0 else None
                ebitda_margin = (ebitda / revenue) if revenue != 0 else None

                # Operating efficiency ratios
                cost_of_revenue_ratio = (cost_of_revenue / revenue) if revenue != 0 else None
                sga_ratio = (selling_general_administrative / revenue) if revenue != 0 else None
                rd_ratio = (research_development / revenue) if revenue != 0 else None
                depreciation_ratio = (depreciation_depletion_amortization / revenue) if revenue != 0 else None

                # Tax efficiency
                effective_tax_rate = (income_taxes / income_before_income_taxes) if income_before_income_taxes != 0 else None

                # Interest coverage
                interest_coverage_ratio = (operating_income / abs(interest_expense)) if interest_expense != 0 else None

                # EPS quality metrics
                eps_quality = (net_income_loss_attributable_common_shareholders / consolidated_net_income_loss) if consolidated_net_income_loss != 0 else None
                share_dilution_impact = (basic_earnings_per_share - diluted_earnings_per_share) / basic_earnings_per_share if basic_earnings_per_share != 0 else None

                # Revenue quality metrics
                operating_revenue_quality = (operating_income / gross_profit) if gross_profit != 0 else None
                net_income_quality = (net_income_loss_attributable_common_shareholders / operating_income) if operating_income != 0 else None

                # Per-share calculations
                revenue_per_share = (revenue / diluted_shares_outstanding) if diluted_shares_outstanding != 0 else None
                operating_income_per_share = (operating_income / diluted_shares_outstanding) if diluted_shares_outstanding != 0 else None
                gross_profit_per_share = (gross_profit / diluted_shares_outstanding) if diluted_shares_outstanding != 0 else None

                # Create enhanced profitability analytics
                income_statement['profitability_ratios'] = {
                    'margin_analysis': {
                        'gross_profit_margin': round(gross_profit_margin, 4) if gross_profit_margin else None,
                        'operating_profit_margin': round(operating_profit_margin, 4) if operating_profit_margin else None,
                        'net_profit_margin': round(net_profit_margin, 4) if net_profit_margin else None,
                        'ebitda_margin': round(ebitda_margin, 4) if ebitda_margin else None
                    },
                    'efficiency_ratios': {
                        'cost_of_revenue_ratio': round(cost_of_revenue_ratio, 4) if cost_of_revenue_ratio else None,
                        'sga_ratio': round(sga_ratio, 4) if sga_ratio else None,
                        'rd_ratio': round(rd_ratio, 4) if rd_ratio else None,
                        'depreciation_ratio': round(depreciation_ratio, 4) if depreciation_ratio else None
                    },
                    'financial_metrics': {
                        'effective_tax_rate': round(effective_tax_rate, 4) if effective_tax_rate else None,
                        'interest_coverage_ratio': round(interest_coverage_ratio, 4) if interest_coverage_ratio else None,
                        'eps_quality': round(eps_quality, 4) if eps_quality else None,
                        'share_dilution_impact': round(share_dilution_impact, 4) if share_dilution_impact else None
                    },
                    'quality_metrics': {
                        'operating_revenue_quality': round(operating_revenue_quality, 4) if operating_revenue_quality else None,
                        'net_income_quality': round(net_income_quality, 4) if net_income_quality else None,
                        'ebitda_to_operating_income': round(ebitda / operating_income, 4) if operating_income != 0 else None
                    },
                    'per_share_metrics': {
                        'revenue_per_share': round(revenue_per_share, 4) if revenue_per_share else None,
                        'operating_income_per_share': round(operating_income_per_share, 4) if operating_income_per_share else None,
                        'gross_profit_per_share': round(gross_profit_per_share, 4) if gross_profit_per_share else None,
                        'basic_earnings_per_share': round(basic_earnings_per_share, 4) if basic_earnings_per_share else None,
                        'diluted_earnings_per_share': round(diluted_earnings_per_share, 4) if diluted_earnings_per_share else None
                    }
                }

                # Profitability health assessment
                health_score = 0
                health_factors = []

                # Gross profit margin assessment
                if gross_profit_margin and gross_profit_margin > 0.5:
                    health_score += 15
                    health_factors.append("Excellent gross margin")
                elif gross_profit_margin and gross_profit_margin > 0.3:
                    health_score += 10
                    health_factors.append("Good gross margin")
                elif gross_profit_margin and gross_profit_margin > 0.15:
                    health_score += 5
                    health_factors.append("Adequate gross margin")
                elif gross_profit_margin:
                    health_factors.append("Low gross margin - pricing pressure concern")

                # Operating profit margin assessment
                if operating_profit_margin and operating_profit_margin > 0.2:
                    health_score += 15
                    health_factors.append("Excellent operating margin")
                elif operating_profit_margin and operating_profit_margin > 0.1:
                    health_score += 10
                    health_factors.append("Good operating margin")
                elif operating_profit_margin and operating_profit_margin > 0.05:
                    health_score += 5
                    health_factors.append("Adequate operating margin")
                elif operating_profit_margin:
                    health_factors.append("Low operating margin - efficiency concern")

                # Net profit margin assessment
                if net_profit_margin and net_profit_margin > 0.15:
                    health_score += 15
                    health_factors.append("Excellent net margin")
                elif net_profit_margin and net_profit_margin > 0.08:
                    health_score += 10
                    health_factors.append("Good net margin")
                elif net_profit_margin and net_profit_margin > 0.03:
                    health_score += 5
                    health_factors.append("Adequate net margin")
                elif net_profit_margin:
                    health_factors.append("Low net margin - profitability concern")

                # Revenue growth assessment (if we had historical data)
                if revenue > 0:
                    health_score += 10
                    health_factors.append("Positive revenue")

                # EPS quality assessment
                if basic_earnings_per_share > 0:
                    health_score += 10
                    health_factors.append("Positive EPS")
                    if share_dilution_impact and share_dilution_impact < 0.05:
                        health_score += 5
                        health_factors.append("Minimal share dilution")
                    elif share_dilution_impact and share_dilution_impact > 0.15:
                        health_factors.append("High share dilution - EPS quality concern")

                # Interest coverage assessment
                if interest_coverage_ratio and interest_coverage_ratio > 5:
                    health_score += 10
                    health_factors.append("Strong interest coverage")
                elif interest_coverage_ratio and interest_coverage_ratio > 2:
                    health_score += 5
                    health_factors.append("Adequate interest coverage")
                elif interest_expense < 0:
                    health_score += 10
                    health_factors.append("No interest expense")
                elif interest_coverage_ratio:
                    health_factors.append("Low interest coverage - financial risk")

                # R&D investment assessment (for tech/growth companies)
                if rd_ratio and rd_ratio > 0.15:
                    health_score += 5
                    health_factors.append("High R&D investment - growth focus")
                elif rd_ratio and rd_ratio > 0.05:
                    health_score += 3
                    health_factors.append("Moderate R&D investment")

                # Overall profitability health rating
                if health_score >= 80:
                    health_rating = "Excellent"
                elif health_score >= 60:
                    health_rating = "Good"
                elif health_score >= 40:
                    health_rating = "Fair"
                elif health_score >= 20:
                    health_rating = "Poor"
                else:
                    health_rating = "Very Poor"

                income_statement['profitability_health'] = {
                    'health_score': health_score,
                    'health_rating': health_rating,
                    'health_factors': health_factors,
                    'assessment_date': datetime.now().isoformat()
                }

                # Business model classification based on margins
                business_model = "Unknown"
                if gross_profit_margin and operating_profit_margin:
                    if gross_profit_margin > 0.6 and operating_profit_margin > 0.25:
                        business_model = "Premium/High-Margin"
                    elif gross_profit_margin > 0.4 and operating_profit_margin > 0.15:
                        business_model = "Quality/Mature"
                    elif gross_profit_margin > 0.3 and operating_profit_margin > 0.08:
                        business_model = "Competitive/Growth"
                    elif gross_profit_margin > 0.2:
                        business_model = "Volume/Low-Margin"
                    else:
                        business_model = "Cost-Conscious"

                income_statement['business_model_analysis'] = {
                    'model_type': business_model,
                    'gross_profit_margin': gross_profit_margin,
                    'operating_profit_margin': operating_profit_margin,
                    'characteristics': _get_business_model_characteristics(business_model)
                }

                # Add formatted currency values for better readability
                income_statement['formatted_values'] = {
                    'revenue': f"${revenue:,.0f}" if revenue else "$0",
                    'gross_profit': f"${gross_profit:,.0f}" if gross_profit else "$0",
                    'operating_income': f"${operating_income:,.0f}" if operating_income else "$0",
                    'net_income': f"${net_income_loss_attributable_common_shareholders:,.0f}" if net_income_loss_attributable_common_shareholders else "$0",
                    'ebitda': f"${ebitda:,.0f}" if ebitda else "$0",
                    'cost_of_revenue': f"${cost_of_revenue:,.0f}" if cost_of_revenue else "$0",
                    'sga_expenses': f"${selling_general_administrative:,.0f}" if selling_general_administrative else "$0",
                    'rd_expenses': f"${research_development:,.0f}" if research_development else "$0"
                }

                # Add period analysis
                if income_statement.get('period_end') and income_statement.get('fiscal_year'):
                    period_info = {
                        'period_type': income_statement.get('timeframe', 'unknown'),
                        'fiscal_year': income_statement.get('fiscal_year'),
                        'fiscal_quarter': income_statement.get('fiscal_quarter'),
                        'period_end_date': income_statement.get('period_end'),
                        'filing_date': income_statement.get('filing_date'),
                        'days_to_file': None
                    }

                    # Calculate days between period end and filing
                    if income_statement.get('period_end') and income_statement.get('filing_date'):
                        try:
                            period_end = datetime.strptime(income_statement['period_end'], '%Y-%m-%d')
                            filing_date = datetime.strptime(income_statement['filing_date'], '%Y-%m-%d')
                            days_to_file = (filing_date - period_end).days
                            period_info['days_to_file'] = days_to_file

                            if days_to_file <= 30:
                                period_info['filing_timeliness'] = 'Excellent'
                            elif days_to_file <= 45:
                                period_info['filing_timeliness'] = 'Good'
                            elif days_to_file <= 60:
                                period_info['filing_timeliness'] = 'Average'
                            else:
                                period_info['filing_timeliness'] = 'Delayed'
                        except ValueError:
                            period_info['filing_timeliness'] = 'Unknown'

                    income_statement['period_analysis'] = period_info

        # Add summary statistics
        if result.get('results'):
            income_statements = result['results']

            # Calculate aggregate metrics across all periods
            total_companies = len(set(is_.get('cik') for is_ in income_statements if is_.get('cik')))
            unique_tickers = set()
            for is_ in income_statements:
                if is_.get('tickers'):
                    unique_tickers.update(is_['tickers'])

            timeframes = {}
            fiscal_years = []
            total_revenue = 0
            total_net_income = 0
            total_operating_income = 0
            positive_net_income_count = 0
            positive_operating_income_count = 0

            for is_ in income_statements:
                timeframe = is_.get('timeframe', 'unknown')
                timeframes[timeframe] = timeframes.get(timeframe, 0) + 1

                fiscal_year = is_.get('fiscal_year')
                if fiscal_year:
                    fiscal_years.append(fiscal_year)

                total_revenue += is_.get('revenue', 0)
                total_net_income += is_.get('net_income_loss_attributable_common_shareholders', 0)
                total_operating_income += is_.get('operating_income', 0)

                if is_.get('net_income_loss_attributable_common_shareholders', 0) > 0:
                    positive_net_income_count += 1
                if is_.get('operating_income', 0) > 0:
                    positive_operating_income_count += 1

            # Calculate average margins
            avg_gross_margin = sum(is_.get('gross_profit', 0) / is_.get('revenue', 1) for is_ in income_statements if is_.get('revenue', 0) > 0) / len(income_statements) if income_statements else 0
            avg_operating_margin = total_operating_income / total_revenue if total_revenue > 0 else 0
            avg_net_margin = total_net_income / total_revenue if total_revenue > 0 else 0

            summary_stats = {
                'total_income_statements': len(income_statements),
                'unique_companies': total_companies,
                'unique_tickers': list(unique_tickers),
                'ticker_count': len(unique_tickers),
                'timeframe_distribution': timeframes,
                'fiscal_year_range': {
                    'earliest': min(fiscal_years) if fiscal_years else None,
                    'latest': max(fiscal_years) if fiscal_years else None,
                    'count': len(set(fiscal_years)) if fiscal_years else 0
                },
                'aggregate_performance': {
                    'total_revenue': total_revenue,
                    'total_net_income': total_net_income,
                    'total_operating_income': total_operating_income,
                    'average_revenue_per_statement': total_revenue / len(income_statements) if income_statements else 0,
                    'average_net_income_per_statement': total_net_income / len(income_statements) if income_statements else 0
                },
                'profitability_metrics': {
                    'average_gross_margin': round(avg_gross_margin, 4),
                    'average_operating_margin': round(avg_operating_margin, 4),
                    'average_net_margin': round(avg_net_margin, 4),
                    'positive_net_income_ratio': positive_net_income_count / len(income_statements) if income_statements else 0,
                    'positive_operating_income_ratio': positive_operating_income_count / len(income_statements) if income_statements else 0
                },
                'quality_metrics': {
                    'average_profitability_health_score': sum(is_.get('profitability_health', {}).get('health_score', 0) for is_ in income_statements) / len(income_statements) if income_statements else 0
                },
                'query_summary': {
                    'parameters_used': {
                        'cik': cik,
                        'tickers': tickers,
                        'timeframe': timeframe,
                        'fiscal_year': fiscal_year,
                        'limit': limit,
                        'sort': sort
                    },
                    'result_count': len(income_statements),
                    'has_next_page': bool(result.get('next_url'))
                }
            }

            result['summary_statistics'] = summary_stats

        return {
            'success': True,
            'income_statement_data': result,
            'query_parameters': {
                'cik': cik,
                'tickers': tickers,
                'timeframe': timeframe,
                'fiscal_year': fiscal_year,
                'fiscal_quarter': fiscal_quarter,
                'limit': limit,
                'sort': sort
            }
        }

    except Exception as e:
        return {"error": f"Failed to get income statements: {str(e)}"}

def get_financial_ratios(
    ticker: Optional[str] = None,
    ticker_any_of: Optional[str] = None,
    ticker_gt: Optional[str] = None,
    ticker_gte: Optional[str] = None,
    ticker_lt: Optional[str] = None,
    ticker_lte: Optional[str] = None,
    cik: Optional[str] = None,
    cik_any_of: Optional[str] = None,
    cik_gt: Optional[str] = None,
    cik_gte: Optional[str] = None,
    cik_lt: Optional[str] = None,
    cik_lte: Optional[str] = None,
    price: Optional[float] = None,
    price_gt: Optional[float] = None,
    price_gte: Optional[float] = None,
    price_lt: Optional[float] = None,
    price_lte: Optional[float] = None,
    average_volume: Optional[float] = None,
    average_volume_gt: Optional[float] = None,
    average_volume_gte: Optional[float] = None,
    average_volume_lt: Optional[float] = None,
    average_volume_lte: Optional[float] = None,
    market_cap: Optional[float] = None,
    market_cap_gt: Optional[float] = None,
    market_cap_gte: Optional[float] = None,
    market_cap_lt: Optional[float] = None,
    market_cap_lte: Optional[float] = None,
    earnings_per_share: Optional[float] = None,
    earnings_per_share_gt: Optional[float] = None,
    earnings_per_share_gte: Optional[float] = None,
    earnings_per_share_lt: Optional[float] = None,
    earnings_per_share_lte: Optional[float] = None,
    price_to_earnings: Optional[float] = None,
    price_to_earnings_gt: Optional[float] = None,
    price_to_earnings_gte: Optional[float] = None,
    price_to_earnings_lt: Optional[float] = None,
    price_to_earnings_lte: Optional[float] = None,
    price_to_book: Optional[float] = None,
    price_to_book_gt: Optional[float] = None,
    price_to_book_gte: Optional[float] = None,
    price_to_book_lt: Optional[float] = None,
    price_to_book_lte: Optional[float] = None,
    price_to_sales: Optional[float] = None,
    price_to_sales_gt: Optional[float] = None,
    price_to_sales_gte: Optional[float] = None,
    price_to_sales_lt: Optional[float] = None,
    price_to_sales_lte: Optional[float] = None,
    price_to_cash_flow: Optional[float] = None,
    price_to_cash_flow_gt: Optional[float] = None,
    price_to_cash_flow_gte: Optional[float] = None,
    price_to_cash_flow_lt: Optional[float] = None,
    price_to_cash_flow_lte: Optional[float] = None,
    price_to_free_cash_flow: Optional[float] = None,
    price_to_free_cash_flow_gt: Optional[float] = None,
    price_to_free_cash_flow_gte: Optional[float] = None,
    price_to_free_cash_flow_lt: Optional[float] = None,
    price_to_free_cash_flow_lte: Optional[float] = None,
    dividend_yield: Optional[float] = None,
    dividend_yield_gt: Optional[float] = None,
    dividend_yield_gte: Optional[float] = None,
    dividend_yield_lt: Optional[float] = None,
    dividend_yield_lte: Optional[float] = None,
    return_on_assets: Optional[float] = None,
    return_on_assets_gt: Optional[float] = None,
    return_on_assets_gte: Optional[float] = None,
    return_on_assets_lt: Optional[float] = None,
    return_on_assets_lte: Optional[float] = None,
    return_on_equity: Optional[float] = None,
    return_on_equity_gt: Optional[float] = None,
    return_on_equity_gte: Optional[float] = None,
    return_on_equity_lt: Optional[float] = None,
    return_on_equity_lte: Optional[float] = None,
    debt_to_equity: Optional[float] = None,
    debt_to_equity_gt: Optional[float] = None,
    debt_to_equity_gte: Optional[float] = None,
    debt_to_equity_lt: Optional[float] = None,
    debt_to_equity_lte: Optional[float] = None,
    current: Optional[float] = None,
    current_gt: Optional[float] = None,
    current_gte: Optional[float] = None,
    current_lt: Optional[float] = None,
    current_lte: Optional[float] = None,
    quick: Optional[float] = None,
    quick_gt: Optional[float] = None,
    quick_gte: Optional[float] = None,
    quick_lt: Optional[float] = None,
    quick_lte: Optional[float] = None,
    cash: Optional[float] = None,
    cash_gt: Optional[float] = None,
    cash_gte: Optional[float] = None,
    cash_lt: Optional[float] = None,
    cash_lte: Optional[float] = None,
    ev_to_sales: Optional[float] = None,
    ev_to_sales_gt: Optional[float] = None,
    ev_to_sales_gte: Optional[float] = None,
    ev_to_sales_lt: Optional[float] = None,
    ev_to_sales_lte: Optional[float] = None,
    ev_to_ebitda: Optional[float] = None,
    ev_to_ebitda_gt: Optional[float] = None,
    ev_to_ebitda_gte: Optional[float] = None,
    ev_to_ebitda_lt: Optional[float] = None,
    ev_to_ebitda_lte: Optional[float] = None,
    enterprise_value: Optional[float] = None,
    enterprise_value_gt: Optional[float] = None,
    enterprise_value_gte: Optional[float] = None,
    enterprise_value_lt: Optional[float] = None,
    enterprise_value_lte: Optional[float] = None,
    free_cash_flow: Optional[float] = None,
    free_cash_flow_gt: Optional[float] = None,
    free_cash_flow_gte: Optional[float] = None,
    free_cash_flow_lt: Optional[float] = None,
    free_cash_flow_lte: Optional[float] = None,
    limit: Optional[int] = None,
    sort: Optional[str] = None
) -> Dict:
    """
    Retrieve comprehensive financial ratios data for public companies from Polygon.io API.

    Args:
        ticker: Stock ticker symbol for the company
        ticker_any_of: Filter equal to any of the ticker values (comma separated)
        ticker_gt/ticker_gte/ticker_lt/ticker_lte: Ticker range filters
        cik: Central Index Key (CIK) number assigned by the SEC
        cik_any_of: Filter equal to any of the CIK values (comma separated)
        cik_gt/cik_gte/cik_lt/cik_lte: CIK range filters
        price/price_gt/price_gte/price_lt/price_lte: Stock price filters
        average_volume/average_volume_gt/average_volume_gte/average_volume_lt/average_volume_lte: Trading volume filters
        market_cap/market_cap_gt/market_cap_gte/market_cap_lt/market_cap_lte: Market cap filters
        earnings_per_share/earnings_per_share_gt/earnings_per_share_gte/earnings_per_share_lt/earnings_per_share_lte: EPS filters
        price_to_earnings/price_to_earnings_gt/price_to_earnings_gte/price_to_earnings_lt/price_to_earnings_lte: P/E ratio filters
        price_to_book/price_to_book_gt/price_to_book_gte/price_to_book_lt/price_to_book_lte: P/B ratio filters
        price_to_sales/price_to_sales_gt/price_to_sales_gte/price_to_sales_lt/price_to_sales_lte: P/S ratio filters
        price_to_cash_flow/price_to_cash_flow_gt/price_to_cash_flow_gte/price_to_cash_flow_lt/price_to_cash_flow_lte: P/CF ratio filters
        price_to_free_cash_flow/price_to_free_cash_flow_gt/price_to_free_cash_flow_gte/price_to_free_cash_flow_lt/price_to_free_cash_flow_lte: P/FCF ratio filters
        dividend_yield/dividend_yield_gt/dividend_yield_gte/dividend_yield_lt/dividend_yield_lte: Dividend yield filters
        return_on_assets/return_on_assets_gt/return_on_assets_gte/return_on_assets_lt/return_on_assets_lte: ROA filters
        return_on_equity/return_on_equity_gt/return_on_equity_gte/return_on_equity_lt/return_on_equity_lte: ROE filters
        debt_to_equity/debt_to_equity_gt/debt_to_equity_gte/debt_to_equity_lt/debt_to_equity_lte: D/E ratio filters
        current/current_gt/current_gte/current_lt/current_lte: Current ratio filters
        quick/quick_gt/quick_gte/quick_lt/quick_lte: Quick ratio filters
        cash/cash_gt/cash_gte/cash_lt/cash_lte: Cash ratio filters
        ev_to_sales/ev_to_sales_gt/ev_to_sales_gte/ev_to_sales_lt/ev_to_sales_lte: EV/Sales ratio filters
        ev_to_ebitda/ev_to_ebitda_gt/ev_to_ebitda_gte/ev_to_ebitda_lt/ev_to_ebitda_lte: EV/EBITDA ratio filters
        enterprise_value/enterprise_value_gt/enterprise_value_gte/enterprise_value_lt/enterprise_value_lte: Enterprise value filters
        free_cash_flow/free_cash_flow_gt/free_cash_flow_gte/free_cash_flow_lt/free_cash_flow_lte: Free cash flow filters
        limit: Limit the maximum number of results (max 50000)
        sort: Sort columns and direction (e.g., 'ticker.desc', 'price_to_earnings.asc')

    Returns:
        Dict containing financial ratios data with enhanced investment analysis
    """
    try:
        # Build API endpoint
        endpoint = "/stocks/financials/v1/ratios"

        # Build query parameters
        params = {}

        # Ticker parameters
        if ticker:
            params['ticker'] = ticker
        if ticker_any_of:
            params['ticker.any_of'] = ticker_any_of
        if ticker_gt:
            params['ticker.gt'] = ticker_gt
        if ticker_gte:
            params['ticker.gte'] = ticker_gte
        if ticker_lt:
            params['ticker.lt'] = ticker_lt
        if ticker_lte:
            params['ticker.lte'] = ticker_lte

        # CIK parameters
        if cik:
            params['cik'] = cik
        if cik_any_of:
            params['cik.any_of'] = cik_any_of
        if cik_gt:
            params['cik.gt'] = cik_gt
        if cik_gte:
            params['cik.gte'] = cik_gte
        if cik_lt:
            params['cik.lt'] = cik_lt
        if cik_lte:
            params['cik.lte'] = cik_lte

        # Price parameters
        if price is not None:
            params['price'] = price
        if price_gt is not None:
            params['price.gt'] = price_gt
        if price_gte is not None:
            params['price.gte'] = price_gte
        if price_lt is not None:
            params['price.lt'] = price_lt
        if price_lte is not None:
            params['price.lte'] = price_lte

        # Volume parameters
        if average_volume is not None:
            params['average_volume'] = average_volume
        if average_volume_gt is not None:
            params['average_volume.gt'] = average_volume_gt
        if average_volume_gte is not None:
            params['average_volume.gte'] = average_volume_gte
        if average_volume_lt is not None:
            params['average_volume.lt'] = average_volume_lt
        if average_volume_lte is not None:
            params['average_volume.lte'] = average_volume_lte

        # Market cap parameters
        if market_cap is not None:
            params['market_cap'] = market_cap
        if market_cap_gt is not None:
            params['market_cap.gt'] = market_cap_gt
        if market_cap_gte is not None:
            params['market_cap.gte'] = market_cap_gte
        if market_cap_lt is not None:
            params['market_cap.lt'] = market_cap_lt
        if market_cap_lte is not None:
            params['market_cap.lte'] = market_cap_lte

        # EPS parameters
        if earnings_per_share is not None:
            params['earnings_per_share'] = earnings_per_share
        if earnings_per_share_gt is not None:
            params['earnings_per_share.gt'] = earnings_per_share_gt
        if earnings_per_share_gte is not None:
            params['earnings_per_share.gte'] = earnings_per_share_gte
        if earnings_per_share_lt is not None:
            params['earnings_per_share.lt'] = earnings_per_share_lt
        if earnings_per_share_lte is not None:
            params['earnings_per_share.lte'] = earnings_per_share_lte

        # P/E ratio parameters
        if price_to_earnings is not None:
            params['price_to_earnings'] = price_to_earnings
        if price_to_earnings_gt is not None:
            params['price_to_earnings.gt'] = price_to_earnings_gt
        if price_to_earnings_gte is not None:
            params['price_to_earnings.gte'] = price_to_earnings_gte
        if price_to_earnings_lt is not None:
            params['price_to_earnings.lt'] = price_to_earnings_lt
        if price_to_earnings_lte is not None:
            params['price_to_earnings.lte'] = price_to_earnings_lte

        # P/B ratio parameters
        if price_to_book is not None:
            params['price_to_book'] = price_to_book
        if price_to_book_gt is not None:
            params['price_to_book.gt'] = price_to_book_gt
        if price_to_book_gte is not None:
            params['price_to_book.gte'] = price_to_book_gte
        if price_to_book_lt is not None:
            params['price_to_book.lt'] = price_to_book_lt
        if price_to_book_lte is not None:
            params['price_to_book.lte'] = price_to_book_lte

        # P/S ratio parameters
        if price_to_sales is not None:
            params['price_to_sales'] = price_to_sales
        if price_to_sales_gt is not None:
            params['price_to_sales.gt'] = price_to_sales_gt
        if price_to_sales_gte is not None:
            params['price_to_sales.gte'] = price_to_sales_gte
        if price_to_sales_lt is not None:
            params['price_to_sales.lt'] = price_to_sales_lt
        if price_to_sales_lte is not None:
            params['price_to_sales.lte'] = price_to_sales_lte

        # P/CF ratio parameters
        if price_to_cash_flow is not None:
            params['price_to_cash_flow'] = price_to_cash_flow
        if price_to_cash_flow_gt is not None:
            params['price_to_cash_flow.gt'] = price_to_cash_flow_gt
        if price_to_cash_flow_gte is not None:
            params['price_to_cash_flow.gte'] = price_to_cash_flow_gte
        if price_to_cash_flow_lt is not None:
            params['price_to_cash_flow.lt'] = price_to_cash_flow_lt
        if price_to_cash_flow_lte is not None:
            params['price_to_cash_flow.lte'] = price_to_cash_flow_lte

        # P/FCF ratio parameters
        if price_to_free_cash_flow is not None:
            params['price_to_free_cash_flow'] = price_to_free_cash_flow
        if price_to_free_cash_flow_gt is not None:
            params['price_to_free_cash_flow.gt'] = price_to_free_cash_flow_gt
        if price_to_free_cash_flow_gte is not None:
            params['price_to_free_cash_flow.gte'] = price_to_free_cash_flow_gte
        if price_to_free_cash_flow_lt is not None:
            params['price_to_free_cash_flow.lt'] = price_to_free_cash_flow_lt
        if price_to_free_cash_flow_lte is not None:
            params['price_to_free_cash_flow.lte'] = price_to_free_cash_flow_lte

        # Dividend yield parameters
        if dividend_yield is not None:
            params['dividend_yield'] = dividend_yield
        if dividend_yield_gt is not None:
            params['dividend_yield.gt'] = dividend_yield_gt
        if dividend_yield_gte is not None:
            params['dividend_yield.gte'] = dividend_yield_gte
        if dividend_yield_lt is not None:
            params['dividend_yield.lt'] = dividend_yield_lt
        if dividend_yield_lte is not None:
            params['dividend_yield.lte'] = dividend_yield_lte

        # ROA parameters
        if return_on_assets is not None:
            params['return_on_assets'] = return_on_assets
        if return_on_assets_gt is not None:
            params['return_on_assets.gt'] = return_on_assets_gt
        if return_on_assets_gte is not None:
            params['return_on_assets.gte'] = return_on_assets_gte
        if return_on_assets_lt is not None:
            params['return_on_assets.lt'] = return_on_assets_lt
        if return_on_assets_lte is not None:
            params['return_on_assets.lte'] = return_on_assets_lte

        # ROE parameters
        if return_on_equity is not None:
            params['return_on_equity'] = return_on_equity
        if return_on_equity_gt is not None:
            params['return_on_equity.gt'] = return_on_equity_gt
        if return_on_equity_gte is not None:
            params['return_on_equity.gte'] = return_on_equity_gte
        if return_on_equity_lt is not None:
            params['return_on_equity.lt'] = return_on_equity_lt
        if return_on_equity_lte is not None:
            params['return_on_equity.lte'] = return_on_equity_lte

        # D/E ratio parameters
        if debt_to_equity is not None:
            params['debt_to_equity'] = debt_to_equity
        if debt_to_equity_gt is not None:
            params['debt_to_equity.gt'] = debt_to_equity_gt
        if debt_to_equity_gte is not None:
            params['debt_to_equity.gte'] = debt_to_equity_gte
        if debt_to_equity_lt is not None:
            params['debt_to_equity.lt'] = debt_to_equity_lt
        if debt_to_equity_lte is not None:
            params['debt_to_equity.lte'] = debt_to_equity_lte

        # Current ratio parameters
        if current is not None:
            params['current'] = current
        if current_gt is not None:
            params['current.gt'] = current_gt
        if current_gte is not None:
            params['current.gte'] = current_gte
        if current_lt is not None:
            params['current.lt'] = current_lt
        if current_lte is not None:
            params['current.lte'] = current_lte

        # Quick ratio parameters
        if quick is not None:
            params['quick'] = quick
        if quick_gt is not None:
            params['quick.gt'] = quick_gt
        if quick_gte is not None:
            params['quick.gte'] = quick_gte
        if quick_lt is not None:
            params['quick.lt'] = quick_lt
        if quick_lte is not None:
            params['quick.lte'] = quick_lte

        # Cash ratio parameters
        if cash is not None:
            params['cash'] = cash
        if cash_gt is not None:
            params['cash.gt'] = cash_gt
        if cash_gte is not None:
            params['cash.gte'] = cash_gte
        if cash_lt is not None:
            params['cash.lt'] = cash_lt
        if cash_lte is not None:
            params['cash.lte'] = cash_lte

        # EV/Sales ratio parameters
        if ev_to_sales is not None:
            params['ev_to_sales'] = ev_to_sales
        if ev_to_sales_gt is not None:
            params['ev_to_sales.gt'] = ev_to_sales_gt
        if ev_to_sales_gte is not None:
            params['ev_to_sales.gte'] = ev_to_sales_gte
        if ev_to_sales_lt is not None:
            params['ev_to_sales.lt'] = ev_to_sales_lt
        if ev_to_sales_lte is not None:
            params['ev_to_sales.lte'] = ev_to_sales_lte

        # EV/EBITDA ratio parameters
        if ev_to_ebitda is not None:
            params['ev_to_ebitda'] = ev_to_ebitda
        if ev_to_ebitda_gt is not None:
            params['ev_to_ebitda.gt'] = ev_to_ebitda_gt
        if ev_to_ebitda_gte is not None:
            params['ev_to_ebitda.gte'] = ev_to_ebitda_gte
        if ev_to_ebitda_lt is not None:
            params['ev_to_ebitda.lt'] = ev_to_ebitda_lt
        if ev_to_ebitda_lte is not None:
            params['ev_to_ebitda.lte'] = ev_to_ebitda_lte

        # Enterprise value parameters
        if enterprise_value is not None:
            params['enterprise_value'] = enterprise_value
        if enterprise_value_gt is not None:
            params['enterprise_value.gt'] = enterprise_value_gt
        if enterprise_value_gte is not None:
            params['enterprise_value.gte'] = enterprise_value_gte
        if enterprise_value_lt is not None:
            params['enterprise_value.lt'] = enterprise_value_lt
        if enterprise_value_lte is not None:
            params['enterprise_value.lte'] = enterprise_value_lte

        # Free cash flow parameters
        if free_cash_flow is not None:
            params['free_cash_flow'] = free_cash_flow
        if free_cash_flow_gt is not None:
            params['free_cash_flow.gt'] = free_cash_flow_gt
        if free_cash_flow_gte is not None:
            params['free_cash_flow.gte'] = free_cash_flow_gte
        if free_cash_flow_lt is not None:
            params['free_cash_flow.lt'] = free_cash_flow_lt
        if free_cash_flow_lte is not None:
            params['free_cash_flow.lte'] = free_cash_flow_lte

        # Other parameters
        if limit:
            params['limit'] = limit
        if sort:
            params['sort'] = sort

        # Make API request
        result = make_polygon_request(endpoint, params)

        if 'error' in result:
            return result

        # Enhanced analytics for financial ratios data
        if result.get('results'):
            for ratio_data in result['results']:
                # Extract basic ratio data
                ticker = ratio_data.get('ticker')
                price = ratio_data.get('price', 0)
                market_cap = ratio_data.get('market_cap', 0)
                enterprise_value = ratio_data.get('enterprise_value', 0)
                average_volume = ratio_data.get('average_volume', 0)

                # Valuation ratios
                pe_ratio = ratio_data.get('price_to_earnings')
                pb_ratio = ratio_data.get('price_to_book')
                ps_ratio = ratio_data.get('price_to_sales')
                pcf_ratio = ratio_data.get('price_to_cash_flow')
                pfcf_ratio = ratio_data.get('price_to_free_cash_flow')
                ev_sales_ratio = ratio_data.get('ev_to_sales')
                ev_ebitda_ratio = ratio_data.get('ev_to_ebitda')

                # Profitability ratios
                roa = ratio_data.get('return_on_assets')
                roe = ratio_data.get('return_on_equity')
                dividend_yield = ratio_data.get('dividend_yield')

                # Liquidity ratios
                current_ratio = ratio_data.get('current')
                quick_ratio = ratio_data.get('quick')
                cash_ratio = ratio_data.get('cash')

                # Leverage ratio
                debt_to_equity_ratio = ratio_data.get('debt_to_equity')

                # Cash flow data
                free_cash_flow_amount = ratio_data.get('free_cash_flow', 0)
                earnings_per_share_amount = ratio_data.get('earnings_per_share', 0)

                # Investment screening and analysis
                investment_score = 0
                investment_factors = []
                risk_factors = []

                # Valuation analysis
                if pe_ratio and pe_ratio > 0:
                    if pe_ratio < 15:
                        investment_score += 15
                        investment_factors.append("Attractive P/E ratio")
                    elif pe_ratio < 25:
                        investment_score += 10
                        investment_factors.append("Reasonable P/E ratio")
                    elif pe_ratio > 50:
                        risk_factors.append("High P/E ratio - valuation concern")
                    elif pe_ratio > 0:
                        investment_score += 5
                        investment_factors.append("Fair P/E ratio")
                    else:
                        risk_factors.append("Negative earnings - P/E undefined")

                # P/B ratio analysis
                if pb_ratio and pb_ratio > 0:
                    if pb_ratio < 1.5:
                        investment_score += 10
                        investment_factors.append("Attractive P/B ratio")
                    elif pb_ratio < 3:
                        investment_score += 5
                        investment_factors.append("Reasonable P/B ratio")
                    elif pb_ratio > 10:
                        risk_factors.append("High P/B ratio - overvalued")

                # P/S ratio analysis
                if ps_ratio and ps_ratio > 0:
                    if ps_ratio < 2:
                        investment_score += 8
                        investment_factors.append("Attractive P/S ratio")
                    elif ps_ratio < 5:
                        investment_score += 4
                        investment_factors.append("Reasonable P/S ratio")
                    elif ps_ratio > 15:
                        risk_factors.append("High P/S ratio - expensive")

                # Dividend yield analysis
                if dividend_yield and dividend_yield > 0:
                    if dividend_yield > 0.04:
                        investment_score += 12
                        investment_factors.append("High dividend yield")
                    elif dividend_yield > 0.02:
                        investment_score += 8
                        investment_factors.append("Good dividend yield")
                    elif dividend_yield > 0.01:
                        investment_score += 4
                        investment_factors.append("Modest dividend yield")

                # Profitability analysis
                if roe and roe > 0:
                    if roe > 0.20:
                        investment_score += 15
                        investment_factors.append("Excellent ROE")
                    elif roe > 0.15:
                        investment_score += 10
                        investment_factors.append("Good ROE")
                    elif roe > 0.10:
                        investment_score += 5
                        investment_factors.append("Adequate ROE")
                    elif roe < 0.05:
                        risk_factors.append("Low ROE - profitability concern")

                if roa and roa > 0:
                    if roa > 0.10:
                        investment_score += 10
                        investment_factors.append("Excellent ROA")
                    elif roa > 0.05:
                        investment_score += 5
                        investment_factors.append("Good ROA")
                    elif roa < 0.02:
                        risk_factors.append("Low ROA - efficiency concern")

                # Liquidity analysis
                if current_ratio and current_ratio > 0:
                    if current_ratio > 2.0:
                        investment_score += 8
                        investment_factors.append("Strong current ratio")
                    elif current_ratio > 1.5:
                        investment_score += 4
                        investment_factors.append("Adequate current ratio")
                    elif current_ratio < 1.0:
                        risk_factors.append("Low current ratio - liquidity risk")

                if quick_ratio and quick_ratio > 0:
                    if quick_ratio > 1.5:
                        investment_score += 6
                        investment_factors.append("Strong quick ratio")
                    elif quick_ratio < 0.8:
                        risk_factors.append("Low quick ratio - liquidity concern")

                # Leverage analysis
                if debt_to_equity_ratio and debt_to_equity_ratio > 0:
                    if debt_to_equity_ratio < 0.5:
                        investment_score += 8
                        investment_factors.append("Low debt levels")
                    elif debt_to_equity_ratio < 1.0:
                        investment_score += 4
                        investment_factors.append("Moderate debt levels")
                    elif debt_to_equity_ratio > 2.0:
                        risk_factors.append("High debt levels - financial risk")

                # EV/EBITDA analysis
                if ev_ebitda_ratio and ev_ebitda_ratio > 0:
                    if ev_ebitda_ratio < 10:
                        investment_score += 8
                        investment_factors.append("Attractive EV/EBITDA")
                    elif ev_ebitda_ratio < 15:
                        investment_score += 4
                        investment_factors.append("Reasonable EV/EBITDA")
                    elif ev_ebitda_ratio > 25:
                        risk_factors.append("High EV/EBITDA - expensive")

                # Free cash flow analysis
                if free_cash_flow_amount > 0:
                    investment_score += 10
                    investment_factors.append("Positive free cash flow")
                    if pfcf_ratio and pfcf_ratio > 0:
                        if pfcf_ratio < 15:
                            investment_score += 5
                            investment_factors.append("Attractive P/FCF ratio")
                        elif pfcf_ratio > 30:
                            risk_factors.append("High P/FCF ratio - expensive")

                # Size and liquidity analysis
                if market_cap > 0:
                    if market_cap > 10000000000:  # >$10B
                        investment_score += 5
                        investment_factors.append("Large cap stability")
                    elif market_cap > 2000000000:  # >$2B
                        investment_score += 3
                        investment_factors.append("Mid cap")
                    elif market_cap < 300000000:  # <$300M
                        risk_factors.append("Small cap - higher risk")

                if average_volume > 0:
                    if average_volume > 1000000:  # >1M shares/day
                        investment_score += 3
                        investment_factors.append("High liquidity")
                    elif average_volume < 100000:  # <100K shares/day
                        risk_factors.append("Low liquidity - trading risk")

                # Calculate overall investment rating
                if investment_score >= 70:
                    investment_rating = "Strong Buy"
                elif investment_score >= 55:
                    investment_rating = "Buy"
                elif investment_score >= 40:
                    investment_rating = "Hold"
                elif investment_score >= 25:
                    investment_rating = "Sell"
                else:
                    investment_rating = "Strong Sell"

                # Calculate risk level
                risk_level = "Low"
                if len(risk_factors) >= 5:
                    risk_level = "Very High"
                elif len(risk_factors) >= 3:
                    risk_level = "High"
                elif len(risk_factors) >= 1:
                    risk_level = "Medium"

                # Investment style classification
                investment_style = "Unknown"
                if pe_ratio and dividend_yield:
                    if pe_ratio < 15 and dividend_yield > 0.02:
                        investment_style = "Value"
                    elif pe_ratio > 25 and dividend_yield < 0.01:
                        investment_style = "Growth"
                    elif pe_ratio < 20 and dividend_yield > 0.01:
                        investment_style = "GARP (Growth at Reasonable Price)"
                    elif dividend_yield > 0.03:
                        investment_style = "Income"

                # Create enhanced investment analysis
                ratio_data['investment_analysis'] = {
                    'investment_score': investment_score,
                    'investment_rating': investment_rating,
                    'risk_level': risk_level,
                    'investment_style': investment_style,
                    'investment_factors': investment_factors,
                    'risk_factors': risk_factors,
                    'analysis_date': datetime.now().isoformat()
                }

                # Valuation analysis
                ratio_data['valuation_assessment'] = {
                    'pe_analysis': {
                        'ratio': pe_ratio,
                        'assessment': _get_pe_assessment(pe_ratio),
                        'category': _get_pe_category(pe_ratio)
                    },
                    'pb_analysis': {
                        'ratio': pb_ratio,
                        'assessment': _get_pb_assessment(pb_ratio),
                        'category': _get_pb_category(pb_ratio)
                    },
                    'ps_analysis': {
                        'ratio': ps_ratio,
                        'assessment': _get_ps_assessment(ps_ratio),
                        'category': _get_ps_category(ps_ratio)
                    },
                    'ev_analysis': {
                        'ev_to_ebitda': ev_ebitda_ratio,
                        'ev_to_sales': ev_sales_ratio,
                        'assessment': _get_ev_assessment(ev_ebitda_ratio, ev_sales_ratio)
                    }
                }

                # Financial health assessment
                ratio_data['financial_health'] = {
                    'profitability': {
                        'roe': roe,
                        'roa': roa,
                        'assessment': _get_profitability_assessment(roe, roa)
                    },
                    'liquidity': {
                        'current_ratio': current_ratio,
                        'quick_ratio': quick_ratio,
                        'cash_ratio': cash_ratio,
                        'assessment': _get_liquidity_assessment(current_ratio, quick_ratio, cash_ratio)
                    },
                    'leverage': {
                        'debt_to_equity': debt_to_equity_ratio,
                        'assessment': _get_leverage_assessment(debt_to_equity_ratio)
                    }
                }

                # Market metrics analysis
                ratio_data['market_metrics'] = {
                    'market_cap_category': _get_market_cap_category(market_cap),
                    'liquidity_assessment': _get_liquidity_assessment_by_volume(average_volume),
                    'trading_activity': {
                        'average_volume': average_volume,
                        'volume_category': _get_volume_category(average_volume)
                    }
                }

                # Add formatted values for better readability
                ratio_data['formatted_values'] = {
                    'price': f"${price:.2f}" if price else "$0.00",
                    'market_cap': f"${market_cap/1000000:,.0f}M" if market_cap else "$0M",
                    'enterprise_value': f"${enterprise_value/1000000:,.0f}M" if enterprise_value else "$0M",
                    'free_cash_flow': f"${free_cash_flow_amount/1000000:,.0f}M" if free_cash_flow_amount else "$0M",
                    'earnings_per_share': f"${earnings_per_share_amount:.2f}" if earnings_per_share_amount else "$0.00",
                    'dividend_yield': f"{dividend_yield*100:.2f}%" if dividend_yield else "0.00%"
                }

        # Add summary statistics
        if result.get('results'):
            ratios = result['results']

            # Calculate aggregate metrics across all companies
            total_companies = len(ratios)
            tickers = [r.get('ticker') for r in ratios if r.get('ticker')]

            # Calculate averages for key ratios
            pe_ratios = [r.get('price_to_earnings') for r in ratios if r.get('price_to_earnings') and r.get('price_to_earnings') > 0]
            pb_ratios = [r.get('price_to_book') for r in ratios if r.get('price_to_book') and r.get('price_to_book') > 0]
            roe_values = [r.get('return_on_equity') for r in ratios if r.get('return_on_equity') and r.get('return_on_equity') > 0]
            debt_ratios = [r.get('debt_to_equity') for r in ratios if r.get('debt_to_equity') and r.get('debt_to_equity') > 0]
            dividend_yields = [r.get('dividend_yield') for r in ratios if r.get('dividend_yield') and r.get('dividend_yield') > 0]

            # Investment rating distribution
            investment_ratings = {}
            for r in ratios:
                rating = r.get('investment_analysis', {}).get('investment_rating', 'Unknown')
                investment_ratings[rating] = investment_ratings.get(rating, 0) + 1

            # Market cap distribution
            large_caps = sum(1 for r in ratios if r.get('market_cap', 0) > 10000000000)
            mid_caps = sum(1 for r in ratios if 2000000000 < r.get('market_cap', 0) <= 10000000000)
            small_caps = sum(1 for r in ratios if r.get('market_cap', 0) <= 2000000000)

            summary_stats = {
                'total_companies': total_companies,
                'tickers': tickers,
                'key_ratio_averages': {
                    'average_pe_ratio': round(sum(pe_ratios) / len(pe_ratios), 2) if pe_ratios else None,
                    'average_pb_ratio': round(sum(pb_ratios) / len(pb_ratios), 2) if pb_ratios else None,
                    'average_roe': round(sum(roe_values) / len(roe_values), 4) if roe_values else None,
                    'average_debt_to_equity': round(sum(debt_ratios) / len(debt_ratios), 2) if debt_ratios else None,
                    'average_dividend_yield': round(sum(dividend_yields) / len(dividend_yields), 4) if dividend_yields else None
                },
                'investment_rating_distribution': investment_ratings,
                'market_cap_distribution': {
                    'large_cap': large_caps,
                    'mid_cap': mid_caps,
                    'small_cap': small_caps
                },
                'dividend_paying_companies': len(dividend_yields),
                'high_roe_companies': len([r for r in roe_values if r > 0.15]),
                'low_debt_companies': len([r for r in debt_ratios if r < 0.5]),
                'query_summary': {
                    'parameters_used': {
                        'ticker': ticker,
                        'cik': cik,
                        'limit': limit,
                        'sort': sort
                    },
                    'result_count': len(ratios),
                    'has_next_page': bool(result.get('next_url'))
                }
            }

            result['summary_statistics'] = summary_stats

        return {
            'success': True,
            'ratios_data': result,
            'query_parameters': {
                'ticker': ticker,
                'cik': cik,
                'limit': limit,
                'sort': sort
            }
        }

    except Exception as e:
        return {"error": f"Failed to get financial ratios: {str(e)}"}

def _get_pe_assessment(pe_ratio: Optional[float]) -> str:
    """Get P/E ratio assessment"""
    if not pe_ratio or pe_ratio <= 0:
        return "Negative earnings - P/E undefined"
    if pe_ratio < 10:
        return "Very attractive - potentially undervalued"
    elif pe_ratio < 15:
        return "Attractive - good value"
    elif pe_ratio < 20:
        return "Reasonable - fair value"
    elif pe_ratio < 30:
        return "Expensive - growth premium"
    else:
        return "Very expensive - speculation"

def _get_pe_category(pe_ratio: Optional[float]) -> str:
    """Get P/E ratio category"""
    if not pe_ratio or pe_ratio <= 0:
        return "No Earnings"
    if pe_ratio < 12:
        return "Value"
    elif pe_ratio < 25:
        return "Balanced"
    else:
        return "Growth"

def _get_pb_assessment(pb_ratio: Optional[float]) -> str:
    """Get P/B ratio assessment"""
    if not pb_ratio or pb_ratio <= 0:
        return "No book value data"
    if pb_ratio < 1:
        return "Very attractive - below book value"
    elif pb_ratio < 1.5:
        return "Attractive - reasonable premium"
    elif pb_ratio < 3:
        return "Reasonable - normal premium"
    elif pb_ratio < 5:
        return "Expensive - high premium"
    else:
        return "Very expensive - speculation"

def _get_pb_category(pb_ratio: Optional[float]) -> str:
    """Get P/B ratio category"""
    if not pb_ratio or pb_ratio <= 0:
        return "No Book Value"
    if pb_ratio < 1.5:
        return "Value"
    elif pb_ratio < 3:
        return "Balanced"
    else:
        return "Growth"

def _get_ps_assessment(ps_ratio: Optional[float]) -> str:
    """Get P/S ratio assessment"""
    if not ps_ratio or ps_ratio <= 0:
        return "No sales data"
    if ps_ratio < 1:
        return "Very attractive - low sales multiple"
    elif ps_ratio < 2:
        return "Attractive - reasonable multiple"
    elif ps_ratio < 5:
        return "Reasonable - normal multiple"
    elif ps_ratio < 10:
        return "Expensive - high multiple"
    else:
        return "Very expensive - speculation"

def _get_ps_category(ps_ratio: Optional[float]) -> str:
    """Get P/S ratio category"""
    if not ps_ratio or ps_ratio <= 0:
        return "No Sales"
    if ps_ratio < 2:
        return "Value"
    elif ps_ratio < 5:
        return "Balanced"
    else:
        return "Growth"

def _get_ev_assessment(ev_ebitda: Optional[float], ev_sales: Optional[float]) -> str:
    """Get EV ratio assessment"""
    if not ev_ebitda and not ev_sales:
        return "No EV data"

    assessments = []
    if ev_ebitda:
        if ev_ebitda < 8:
            assessments.append("EV/EBITDA attractive")
        elif ev_ebitda > 20:
            assessments.append("EV/EBITDA expensive")

    if ev_sales:
        if ev_sales < 2:
            assessments.append("EV/Sales attractive")
        elif ev_sales > 10:
            assessments.append("EV/Sales expensive")

    return "; ".join(assessments) if assessments else "EV ratios normal"

def _get_profitability_assessment(roe: Optional[float], roa: Optional[float]) -> str:
    """Get profitability assessment"""
    if not roe and not roa:
        return "No profitability data"

    assessments = []
    if roe:
        if roe > 0.20:
            assessments.append("Excellent ROE")
        elif roe > 0.15:
            assessments.append("Good ROE")
        elif roe < 0.05:
            assessments.append("Poor ROE")

    if roa:
        if roa > 0.10:
            assessments.append("Excellent ROA")
        elif roa > 0.05:
            assessments.append("Good ROA")
        elif roa < 0.02:
            assessments.append("Poor ROA")

    return "; ".join(assessments) if assessments else "Profitability normal"

def _get_liquidity_assessment(current: Optional[float], quick: Optional[float], cash: Optional[float]) -> str:
    """Get liquidity assessment"""
    if not current and not quick and not cash:
        return "No liquidity data"

    assessments = []
    if current:
        if current > 2.0:
            assessments.append("Strong current ratio")
        elif current < 1.0:
            assessments.append("Weak current ratio")

    if quick:
        if quick > 1.5:
            assessments.append("Strong quick ratio")
        elif quick < 0.8:
            assessments.append("Weak quick ratio")

    if cash:
        if cash > 0.5:
            assessments.append("Strong cash position")
        elif cash < 0.2:
            assessments.append("Weak cash position")

    return "; ".join(assessments) if assessments else "Liquidity normal"

def _get_leverage_assessment(debt_to_equity: Optional[float]) -> str:
    """Get leverage assessment"""
    if not debt_to_equity or debt_to_equity <= 0:
        return "No debt data"
    if debt_to_equity < 0.3:
        return "Very low leverage"
    elif debt_to_equity < 0.5:
        return "Low leverage"
    elif debt_to_equity < 1.0:
        return "Moderate leverage"
    elif debt_to_equity < 2.0:
        return "High leverage"
    else:
        return "Very high leverage"

def _get_market_cap_category(market_cap: Optional[float]) -> str:
    """Get market cap category"""
    if not market_cap or market_cap <= 0:
        return "Unknown"
    if market_cap > 20000000000:  # >$20B
        return "Mega Cap"
    elif market_cap > 10000000000:  # >$10B
        return "Large Cap"
    elif market_cap > 2000000000:  # >$2B
        return "Mid Cap"
    elif market_cap > 300000000:  # >$300M
        return "Small Cap"
    else:
        return "Micro Cap"

def _get_liquidity_assessment_by_volume(volume: Optional[float]) -> str:
    """Get liquidity assessment by volume"""
    if not volume or volume <= 0:
        return "Unknown"
    if volume > 5000000:  # >5M shares/day
        return "Very High"
    elif volume > 1000000:  # >1M shares/day
        return "High"
    elif volume > 500000:  # >500K shares/day
        return "Medium"
    elif volume > 100000:  # >100K shares/day
        return "Low"
    else:
        return "Very Low"

def _get_volume_category(volume: Optional[float]) -> str:
    """Get volume category"""
    if not volume or volume <= 0:
        return "Unknown"
    if volume > 2000000:
        return "Highly Liquid"
    elif volume > 500000:
        return "Liquid"
    elif volume > 100000:
        return "Moderately Liquid"
    else:
        return "Illiquid"

def _get_business_model_characteristics(business_model: str) -> List[str]:
    """Get characteristics of different business models based on margin analysis"""
    characteristics_map = {
        "Premium/High-Margin": [
            "Strong pricing power",
            "Brand differentiation",
            "Low competitive pressure",
            "High customer loyalty"
        ],
        "Quality/Mature": [
            "Established market position",
            "Good operational efficiency",
            "Balanced growth strategy",
            "Stable profitability"
        ],
        "Competitive/Growth": [
            "Focus on market share",
            "Moderate pricing power",
            "Investment in growth",
            "Competitive positioning"
        ],
        "Volume/Low-Margin": [
            "High volume strategy",
            "Price-sensitive market",
            "Operational efficiency focus",
            "Cost leadership approach"
        ],
        "Cost-Conscious": [
            "Margin pressure",
            "Competitive challenges",
            "Need for operational improvement",
            "Pricing strategy review needed"
        ]
    }
    return characteristics_map.get(business_model, ["Unknown business model"])

def _get_frequency_description(frequency: Optional[int]) -> str:
    """Get human-readable description of dividend frequency"""
    frequency_map = {
        0: "One-time",
        1: "Annually",
        2: "Bi-annually",
        4: "Quarterly",
        12: "Monthly",
        24: "Bi-monthly",
        52: "Weekly"
    }
    return frequency_map.get(frequency, "Unknown")

def _get_dividend_type_description(dividend_type: Optional[str]) -> str:
    """Get human-readable description of dividend type"""
    type_map = {
        "CD": "Cash Dividend (Regular)",
        "SC": "Special Cash Dividend",
        "LT": "Long-Term Capital Gain Distribution",
        "ST": "Short-Term Capital Gain Distribution"
    }
    return type_map.get(dividend_type, "Unknown")

def get_short_interest(
    ticker: Optional[str] = None,
    ticker_any_of: Optional[str] = None,
    ticker_gt: Optional[str] = None,
    ticker_gte: Optional[str] = None,
    ticker_lt: Optional[str] = None,
    ticker_lte: Optional[str] = None,
    days_to_cover: Optional[float] = None,
    days_to_cover_any_of: Optional[str] = None,
    days_to_cover_gt: Optional[float] = None,
    days_to_cover_gte: Optional[float] = None,
    days_to_cover_lt: Optional[float] = None,
    days_to_cover_lte: Optional[float] = None,
    settlement_date: Optional[str] = None,
    settlement_date_any_of: Optional[str] = None,
    settlement_date_gt: Optional[str] = None,
    settlement_date_gte: Optional[str] = None,
    settlement_date_lt: Optional[str] = None,
    settlement_date_lte: Optional[str] = None,
    avg_daily_volume: Optional[int] = None,
    avg_daily_volume_any_of: Optional[str] = None,
    avg_daily_volume_gt: Optional[int] = None,
    avg_daily_volume_gte: Optional[int] = None,
    avg_daily_volume_lt: Optional[int] = None,
    avg_daily_volume_lte: Optional[int] = None,
    limit: Optional[int] = None,
    sort: Optional[str] = None
) -> Dict:
    """
    Retrieve bi-monthly aggregated short interest data reported to FINRA by broker-dealers.

    Args:
        ticker: The primary ticker symbol for the stock
        ticker_any_of: Filter equal to any of the values (comma separated)
        ticker_gt: Filter greater than the value
        ticker_gte: Filter greater than or equal to the value
        ticker_lt: Filter less than the value
        ticker_lte: Filter less than or equal to the value
        days_to_cover: Days to cover ratio (short_interest / avg_daily_volume)
        days_to_cover_any_of: Filter days to cover equal to any values (comma separated)
        days_to_cover_gt: Filter days to cover greater than value
        days_to_cover_gte: Filter days to cover greater than or equal to value
        days_to_cover_lt: Filter days to cover less than value
        days_to_cover_lte: Filter days to cover less than or equal to value
        settlement_date: Settlement date (YYYY-MM-DD)
        settlement_date_any_of: Filter settlement date equal to any values (comma separated)
        settlement_date_gt: Filter settlement date greater than value
        settlement_date_gte: Filter settlement date greater than or equal to value
        settlement_date_lt: Filter settlement date less than value
        settlement_date_lte: Filter settlement date less than or equal to value
        avg_daily_volume: Average daily trading volume
        avg_daily_volume_any_of: Filter avg daily volume equal to any values (comma separated)
        avg_daily_volume_gt: Filter avg daily volume greater than value
        avg_daily_volume_gte: Filter avg daily volume greater than or equal to value
        avg_daily_volume_lt: Filter avg daily volume less than value
        avg_daily_volume_lte: Filter avg daily volume less than or equal to value
        limit: Limit maximum number of results (default 10, max 50000)
        sort: Sort columns (e.g., 'settlement_date.desc', 'ticker.asc')

    Returns:
        Dictionary containing short interest data with analytics
    """
    try:
        # Build query parameters
        params = {}

        # Ticker parameters
        if ticker:
            params['ticker'] = ticker
        if ticker_any_of:
            params['ticker.any_of'] = ticker_any_of
        if ticker_gt:
            params['ticker.gt'] = ticker_gt
        if ticker_gte:
            params['ticker.gte'] = ticker_gte
        if ticker_lt:
            params['ticker.lt'] = ticker_lt
        if ticker_lte:
            params['ticker.lte'] = ticker_lte

        # Days to cover parameters
        if days_to_cover is not None:
            params['days_to_cover'] = days_to_cover
        if days_to_cover_any_of:
            params['days_to_cover.any_of'] = days_to_cover_any_of
        if days_to_cover_gt is not None:
            params['days_to_cover.gt'] = days_to_cover_gt
        if days_to_cover_gte is not None:
            params['days_to_cover.gte'] = days_to_cover_gte
        if days_to_cover_lt is not None:
            params['days_to_cover.lt'] = days_to_cover_lt
        if days_to_cover_lte is not None:
            params['days_to_cover.lte'] = days_to_cover_lte

        # Settlement date parameters
        if settlement_date:
            params['settlement_date'] = settlement_date
        if settlement_date_any_of:
            params['settlement_date.any_of'] = settlement_date_any_of
        if settlement_date_gt:
            params['settlement_date.gt'] = settlement_date_gt
        if settlement_date_gte:
            params['settlement_date.gte'] = settlement_date_gte
        if settlement_date_lt:
            params['settlement_date.lt'] = settlement_date_lt
        if settlement_date_lte:
            params['settlement_date.lte'] = settlement_date_lte

        # Average daily volume parameters
        if avg_daily_volume is not None:
            params['avg_daily_volume'] = avg_daily_volume
        if avg_daily_volume_any_of:
            params['avg_daily_volume.any_of'] = avg_daily_volume_any_of
        if avg_daily_volume_gt is not None:
            params['avg_daily_volume.gt'] = avg_daily_volume_gt
        if avg_daily_volume_gte is not None:
            params['avg_daily_volume.gte'] = avg_daily_volume_gte
        if avg_daily_volume_lt is not None:
            params['avg_daily_volume.lt'] = avg_daily_volume_lt
        if avg_daily_volume_lte is not None:
            params['avg_daily_volume.lte'] = avg_daily_volume_lte

        # Pagination and sorting
        if limit:
            params['limit'] = min(limit, 50000)  # Enforce maximum limit
        if sort:
            params['sort'] = sort

        # Make API request
        endpoint = "/stocks/v1/short-interest"
        response = make_polygon_request(endpoint, params)

        if "error" in response:
            return response

        # Enhanced analytics for short interest data
        results = response.get("results", [])
        enhanced_results = []

        for result in results:
            enhanced_result = result.copy()

            # Extract key metrics
            short_interest = result.get("short_interest", 0)
            avg_daily_volume = result.get("avg_daily_volume", 0)
            days_to_cover = result.get("days_to_cover", 0)
            ticker = result.get("ticker", "")
            settlement_date = result.get("settlement_date", "")

            # Short interest analysis
            short_interest_analysis = _analyze_short_interest_level(short_interest, avg_daily_volume)
            enhanced_result["short_interest_analysis"] = short_interest_analysis

            # Days to cover analysis
            days_to_cover_analysis = _analyze_days_to_cover(days_to_cover)
            enhanced_result["days_to_cover_analysis"] = days_to_cover_analysis

            # Short squeeze risk assessment
            squeeze_risk = _assess_short_squeeze_risk(days_to_cover, short_interest, avg_daily_volume)
            enhanced_result["short_squeeze_risk"] = squeeze_risk

            # Market sentiment indicator
            sentiment = _determine_short_sentiment(days_to_cover, short_interest_analysis["level"])
            enhanced_result["market_sentiment"] = sentiment

            # Liquidity impact assessment
            liquidity_impact = _assess_liquidity_impact(short_interest, avg_daily_volume)
            enhanced_result["liquidity_impact"] = liquidity_impact

            # Trading recommendation
            trading_recommendation = _generate_short_interest_trading_recommendation(
                days_to_cover, squeeze_risk, sentiment, liquidity_impact
            )
            enhanced_result["trading_recommendation"] = trading_recommendation

            # Risk level (1-5 scale)
            risk_level = _calculate_short_interest_risk_level(days_to_cover, squeeze_risk)
            enhanced_result["risk_level"] = risk_level

            # Format large numbers for readability
            if short_interest > 1000000:
                enhanced_result["short_interest_formatted"] = f"{short_interest / 1000000:.2f}M"
            elif short_interest > 1000:
                enhanced_result["short_interest_formatted"] = f"{short_interest / 1000:.2f}K"
            else:
                enhanced_result["short_interest_formatted"] = str(short_interest)

            # Format average daily volume
            if avg_daily_volume > 1000000:
                enhanced_result["avg_daily_volume_formatted"] = f"{avg_daily_volume / 1000000:.2f}M"
            elif avg_daily_volume > 1000:
                enhanced_result["avg_daily_volume_formatted"] = f"{avg_daily_volume / 1000:.2f}K"
            else:
                enhanced_result["avg_daily_volume_formatted"] = str(avg_daily_volume)

            enhanced_results.append(enhanced_result)

        # Summary statistics
        summary = _calculate_short_interest_summary(enhanced_results)

        return {
            "status": response.get("status", "OK"),
            "request_id": response.get("request_id"),
            "count": len(enhanced_results),
            "results": enhanced_results,
            "summary": summary,
            "next_url": response.get("next_url")
        }

    except Exception as e:
        return {"error": f"Failed to fetch short interest data: {str(e)}"}

def get_short_volume(
    ticker: Optional[str] = None,
    ticker_any_of: Optional[str] = None,
    ticker_gt: Optional[str] = None,
    ticker_gte: Optional[str] = None,
    ticker_lt: Optional[str] = None,
    ticker_lte: Optional[str] = None,
    date: Optional[str] = None,
    date_any_of: Optional[str] = None,
    date_gt: Optional[str] = None,
    date_gte: Optional[str] = None,
    date_lt: Optional[str] = None,
    date_lte: Optional[str] = None,
    short_volume_ratio: Optional[float] = None,
    short_volume_ratio_any_of: Optional[str] = None,
    short_volume_ratio_gt: Optional[float] = None,
    short_volume_ratio_gte: Optional[float] = None,
    short_volume_ratio_lt: Optional[float] = None,
    short_volume_ratio_lte: Optional[float] = None,
    total_volume: Optional[int] = None,
    total_volume_any_of: Optional[str] = None,
    total_volume_gt: Optional[int] = None,
    total_volume_gte: Optional[int] = None,
    total_volume_lt: Optional[int] = None,
    total_volume_lte: Optional[int] = None,
    limit: Optional[int] = None,
    sort: Optional[str] = None
) -> Dict:
    """
    Retrieve daily aggregated short sale volume data reported to FINRA from off-exchange trading venues.

    Args:
        ticker: The primary ticker symbol for the stock
        ticker_any_of: Filter equal to any of the values (comma separated)
        ticker_gt: Filter greater than the value
        ticker_gte: Filter greater than or equal to the value
        ticker_lt: Filter less than the value
        ticker_lte: Filter less than or equal to the value
        date: Date of trade activity (YYYY-MM-DD)
        date_any_of: Filter date equal to any values (comma separated)
        date_gt: Filter date greater than value
        date_gte: Filter date greater than or equal to value
        date_lt: Filter date less than value
        date_lte: Filter date less than or equal to value
        short_volume_ratio: Percentage of total volume that was sold short
        short_volume_ratio_any_of: Filter ratio equal to any values (comma separated)
        short_volume_ratio_gt: Filter ratio greater than value
        short_volume_ratio_gte: Filter ratio greater than or equal to value
        short_volume_ratio_lt: Filter ratio less than value
        short_volume_ratio_lte: Filter ratio less than or equal to value
        total_volume: Total reported volume across all venues
        total_volume_any_of: Filter total volume equal to any values (comma separated)
        total_volume_gt: Filter total volume greater than value
        total_volume_gte: Filter total volume greater than or equal to value
        total_volume_lt: Filter total volume less than value
        total_volume_lte: Filter total volume less than or equal to value
        limit: Limit maximum number of results (default 10, max 50000)
        sort: Sort columns (e.g., 'date.desc', 'ticker.asc')

    Returns:
        Dictionary containing short volume data with analytics
    """
    try:
        # Build query parameters
        params = {}

        # Ticker parameters
        if ticker:
            params['ticker'] = ticker
        if ticker_any_of:
            params['ticker.any_of'] = ticker_any_of
        if ticker_gt:
            params['ticker.gt'] = ticker_gt
        if ticker_gte:
            params['ticker.gte'] = ticker_gte
        if ticker_lt:
            params['ticker.lt'] = ticker_lt
        if ticker_lte:
            params['ticker.lte'] = ticker_lte

        # Date parameters
        if date:
            params['date'] = date
        if date_any_of:
            params['date.any_of'] = date_any_of
        if date_gt:
            params['date.gt'] = date_gt
        if date_gte:
            params['date.gte'] = date_gte
        if date_lt:
            params['date.lt'] = date_lt
        if date_lte:
            params['date.lte'] = date_lte

        # Short volume ratio parameters
        if short_volume_ratio is not None:
            params['short_volume_ratio'] = short_volume_ratio
        if short_volume_ratio_any_of:
            params['short_volume_ratio.any_of'] = short_volume_ratio_any_of
        if short_volume_ratio_gt is not None:
            params['short_volume_ratio.gt'] = short_volume_ratio_gt
        if short_volume_ratio_gte is not None:
            params['short_volume_ratio.gte'] = short_volume_ratio_gte
        if short_volume_ratio_lt is not None:
            params['short_volume_ratio.lt'] = short_volume_ratio_lt
        if short_volume_ratio_lte is not None:
            params['short_volume_ratio.lte'] = short_volume_ratio_lte

        # Total volume parameters
        if total_volume is not None:
            params['total_volume'] = total_volume
        if total_volume_any_of:
            params['total_volume.any_of'] = total_volume_any_of
        if total_volume_gt is not None:
            params['total_volume.gt'] = total_volume_gt
        if total_volume_gte is not None:
            params['total_volume.gte'] = total_volume_gte
        if total_volume_lt is not None:
            params['total_volume.lt'] = total_volume_lt
        if total_volume_lte is not None:
            params['total_volume.lte'] = total_volume_lte

        # Pagination and sorting
        if limit:
            params['limit'] = min(limit, 50000)  # Enforce maximum limit
        if sort:
            params['sort'] = sort

        # Make API request
        endpoint = "/stocks/v1/short-volume"
        response = make_polygon_request(endpoint, params)

        if "error" in response:
            return response

        # Enhanced analytics for short volume data
        results = response.get("results", [])
        enhanced_results = []

        for result in results:
            enhanced_result = result.copy()

            # Extract key metrics
            short_volume = result.get("short_volume", 0)
            total_volume = result.get("total_volume", 0)
            short_volume_ratio = result.get("short_volume_ratio", 0)
            ticker = result.get("ticker", "")
            date = result.get("date", "")

            # Venue breakdown analysis
            venue_breakdown = _analyze_venue_breakdown(result)
            enhanced_result["venue_breakdown_analysis"] = venue_breakdown

            # Short volume ratio analysis
            ratio_analysis = _analyze_short_volume_ratio(short_volume_ratio)
            enhanced_result["ratio_analysis"] = ratio_analysis

            # Exempt volume analysis
            exempt_analysis = _analyze_exempt_volume(result)
            enhanced_result["exempt_volume_analysis"] = exempt_analysis

            # Market pressure indicator
            pressure = _assess_short_selling_pressure(short_volume_ratio, venue_breakdown)
            enhanced_result["selling_pressure"] = pressure

            # Trading pattern analysis
            pattern = _analyze_short_selling_pattern(short_volume, total_volume, short_volume_ratio)
            enhanced_result["trading_pattern"] = pattern

            # Volume distribution assessment
            distribution = _assess_volume_distribution(result)
            enhanced_result["volume_distribution"] = distribution

            # Market sentiment from short volume
            volume_sentiment = _determine_volume_sentiment(short_volume_ratio, pressure, pattern)
            enhanced_result["volume_sentiment"] = volume_sentiment

            # Trading recommendation
            volume_trading_recommendation = _generate_short_volume_trading_recommendation(
                short_volume_ratio, pressure, volume_sentiment, distribution
            )
            enhanced_result["volume_trading_recommendation"] = volume_trading_recommendation

            # Risk level (1-5 scale)
            volume_risk_level = _calculate_short_volume_risk_level(short_volume_ratio, pressure)
            enhanced_result["volume_risk_level"] = volume_risk_level

            # Format large numbers for readability
            if short_volume > 1000000:
                enhanced_result["short_volume_formatted"] = f"{short_volume / 1000000:.2f}M"
            elif short_volume > 1000:
                enhanced_result["short_volume_formatted"] = f"{short_volume / 1000:.2f}K"
            else:
                enhanced_result["short_volume_formatted"] = str(short_volume)

            if total_volume > 1000000:
                enhanced_result["total_volume_formatted"] = f"{total_volume / 1000000:.2f}M"
            elif total_volume > 1000:
                enhanced_result["total_volume_formatted"] = f"{total_volume / 1000:.2f}K"
            else:
                enhanced_result["total_volume_formatted"] = str(total_volume)

            enhanced_results.append(enhanced_result)

        # Summary statistics
        summary = _calculate_short_volume_summary(enhanced_results)

        return {
            "status": response.get("status", "OK"),
            "request_id": response.get("request_id"),
            "count": len(enhanced_results),
            "results": enhanced_results,
            "summary": summary,
            "next_url": response.get("next_url")
        }

    except Exception as e:
        return {"error": f"Failed to fetch short volume data: {str(e)}"}

# Helper functions for short interest analysis
def _analyze_short_interest_level(short_interest: int, avg_daily_volume: int) -> Dict:
    """Analyze short interest level"""
    if avg_daily_volume == 0:
        return {
            "level": "Unknown",
            "assessment": "Cannot assess without volume data",
            "severity": "Low"
        }

    short_interest_ratio = short_interest / avg_daily_volume

    if short_interest_ratio > 0.10:  # >10% of daily volume
        level = "Very High"
        assessment = "Extremely high short interest - potential for significant volatility"
        severity = "High"
    elif short_interest_ratio > 0.05:  # >5% of daily volume
        level = "High"
        assessment = "High short interest - notable bearish sentiment"
        severity = "Medium-High"
    elif short_interest_ratio > 0.02:  # >2% of daily volume
        level = "Moderate"
        assessment = "Moderate short interest - some bearish sentiment"
        severity = "Medium"
    elif short_interest_ratio > 0.01:  # >1% of daily volume
        level = "Low"
        assessment = "Low short interest - mild bearish sentiment"
        severity = "Low-Medium"
    else:
        level = "Very Low"
        assessment = "Very low short interest - minimal bearish sentiment"
        severity = "Low"

    return {
        "level": level,
        "assessment": assessment,
        "severity": severity,
        "short_interest_ratio": round(short_interest_ratio, 4)
    }

def _analyze_days_to_cover(days_to_cover: float) -> Dict:
    """Analyze days to cover ratio"""
    if days_to_cover > 10:
        level = "Very High"
        assessment = "Very high days to cover - extended time needed to cover shorts"
        implication = "Potential for prolonged short squeeze if price rises"
    elif days_to_cover > 5:
        level = "High"
        assessment = "High days to cover - significant time to cover shorts"
        implication = "Vulnerable to short squeeze on positive news"
    elif days_to_cover > 2:
        level = "Moderate"
        assessment = "Moderate days to cover - reasonable time to cover shorts"
        implication = "Some squeeze potential but manageable"
    elif days_to_cover > 1:
        level = "Low"
        assessment = "Low days to cover - shorts can be covered quickly"
        implication = "Limited squeeze risk"
    else:
        level = "Very Low"
        assessment = "Very low days to cover - shorts can be covered very quickly"
        implication = "Minimal squeeze risk"

    return {
        "level": level,
        "assessment": assessment,
        "implication": implication,
        "days_to_cover": round(days_to_cover, 2)
    }

def _assess_short_squeeze_risk(days_to_cover: float, short_interest: int, avg_daily_volume: int) -> Dict:
    """Assess short squeeze risk"""
    risk_score = 0

    # Days to cover factor (40% weight)
    if days_to_cover > 10:
        risk_score += 40
    elif days_to_cover > 5:
        risk_score += 30
    elif days_to_cover > 2:
        risk_score += 20
    elif days_to_cover > 1:
        risk_score += 10

    # Short interest relative to volume factor (30% weight)
    if avg_daily_volume > 0:
        short_interest_ratio = short_interest / avg_daily_volume
        if short_interest_ratio > 0.10:
            risk_score += 30
        elif short_interest_ratio > 0.05:
            risk_score += 20
        elif short_interest_ratio > 0.02:
            risk_score += 10

    # Absolute short interest factor (20% weight)
    if short_interest > 10000000:  # >10M shares
        risk_score += 20
    elif short_interest > 5000000:  # >5M shares
        risk_score += 15
    elif short_interest > 1000000:  # >1M shares
        risk_score += 10
    elif short_interest > 500000:   # >500K shares
        risk_score += 5

    # Volume stability factor (10% weight) - proxy using avg_daily_volume
    if avg_daily_volume < 100000:  # <100K shares/day
        risk_score += 10
    elif avg_daily_volume < 500000:  # <500K shares/day
        risk_score += 5

    # Determine risk level
    if risk_score >= 80:
        risk_level = "Extreme"
        probability = "Very High"
    elif risk_score >= 60:
        risk_level = "High"
        probability = "High"
    elif risk_score >= 40:
        risk_level = "Moderate"
        probability = "Medium"
    elif risk_score >= 20:
        risk_level = "Low"
        probability = "Low"
    else:
        risk_level = "Very Low"
        probability = "Very Low"

    return {
        "risk_level": risk_level,
        "risk_score": risk_score,
        "squeeze_probability": probability,
        "factors": {
            "days_to_cover_contribution": min(40, risk_score),
            "short_interest_contribution": min(30, risk_score),
            "absolute_interest_contribution": min(20, risk_score),
            "volume_stability_contribution": min(10, risk_score)
        }
    }

def _determine_short_sentiment(days_to_cover: float, short_interest_level: str) -> Dict:
    """Determine market sentiment from short data"""
    sentiment_score = 0

    # Days to cover scoring
    if days_to_cover > 10:
        sentiment_score += 40
    elif days_to_cover > 5:
        sentiment_score += 30
    elif days_to_cover > 2:
        sentiment_score += 20
    elif days_to_cover > 1:
        sentiment_score += 10

    # Short interest level scoring
    if short_interest_level == "Very High":
        sentiment_score += 30
    elif short_interest_level == "High":
        sentiment_score += 25
    elif short_interest_level == "Moderate":
        sentiment_score += 15
    elif short_interest_level == "Low":
        sentiment_score += 10

    # Determine sentiment
    if sentiment_score >= 60:
        sentiment = "Very Bearish"
        confidence = "High"
        implication = "Strong negative sentiment, potential for decline"
    elif sentiment_score >= 45:
        sentiment = "Bearish"
        confidence = "Medium-High"
        implication = "Negative sentiment, likely downward pressure"
    elif sentiment_score >= 30:
        sentiment = "Neutral-Bearish"
        confidence = "Medium"
        implication = "Slightly negative sentiment, modest pressure"
    elif sentiment_score >= 15:
        sentiment = "Neutral"
        confidence = "Low-Medium"
        implication = "Balanced sentiment, minimal pressure"
    else:
        sentiment = "Bullish-Neutral"
        confidence = "Low"
        implication = "Slightly positive sentiment, minimal short pressure"

    return {
        "sentiment": sentiment,
        "confidence": confidence,
        "implication": implication,
        "sentiment_score": sentiment_score
    }

def _assess_liquidity_impact(short_interest: int, avg_daily_volume: int) -> Dict:
    """Assess liquidity impact of short interest"""
    if avg_daily_volume == 0:
        return {
            "impact": "Unknown",
            "assessment": "Cannot assess without volume data"
        }

    short_interest_ratio = short_interest / avg_daily_volume

    if short_interest_ratio > 0.20:  # >20% of daily volume
        impact = "High"
        assessment = "Short interest represents significant portion of daily volume"
        liquidity_risk = "High liquidity impact expected during covering"
    elif short_interest_ratio > 0.10:  # >10% of daily volume
        impact = "Moderate-High"
        assessment = "Short interest could impact liquidity during covering"
        liquidity_risk = "Moderate to high liquidity impact"
    elif short_interest_ratio > 0.05:  # >5% of daily volume
        impact = "Moderate"
        assessment = "Short interest may affect liquidity during covering"
        liquidity_risk = "Moderate liquidity impact"
    elif short_interest_ratio > 0.02:  # >2% of daily volume
        impact = "Low-Moderate"
        assessment = "Short interest could have minor liquidity impact"
        liquidity_risk = "Low to moderate liquidity impact"
    else:
        impact = "Low"
        assessment = "Short interest unlikely to significantly impact liquidity"
        liquidity_risk = "Low liquidity impact"

    return {
        "impact": impact,
        "assessment": assessment,
        "liquidity_risk": liquidity_risk,
        "short_interest_ratio": round(short_interest_ratio, 4)
    }

def _generate_short_interest_trading_recommendation(
    days_to_cover: float, squeeze_risk: Dict, sentiment: Dict, liquidity_impact: Dict
) -> Dict:
    """Generate trading recommendation based on short interest analysis"""
    recommendation_score = 0
    factors = []

    # Factor 1: Short squeeze potential (40% weight)
    squeeze_score = {
        "Extreme": 40, "High": 30, "Moderate": 20, "Low": 10, "Very Low": 5
    }.get(squeeze_risk["risk_level"], 0)
    recommendation_score += squeeze_score
    factors.append(f"Squeeze risk: {squeeze_score}/40")

    # Factor 2: Market sentiment (30% weight)
    if sentiment["sentiment"] == "Very Bearish":
        sentiment_score = 30
    elif sentiment["sentiment"] == "Bearish":
        sentiment_score = 25
    elif sentiment["sentiment"] == "Neutral-Bearish":
        sentiment_score = 15
    elif sentiment["sentiment"] == "Neutral":
        sentiment_score = 10
    else:
        sentiment_score = 5
    recommendation_score += sentiment_score
    factors.append(f"Sentiment score: {sentiment_score}/30")

    # Factor 3: Liquidity impact (20% weight)
    liquidity_score = {
        "High": 20, "Moderate-High": 15, "Moderate": 10, "Low-Moderate": 5, "Low": 2
    }.get(liquidity_impact["impact"], 0)
    recommendation_score += liquidity_score
    factors.append(f"Liquidity impact: {liquidity_score}/20")

    # Factor 4: Days to cover (10% weight)
    if days_to_cover > 10:
        days_score = 10
    elif days_to_cover > 5:
        days_score = 8
    elif days_to_cover > 2:
        days_score = 5
    elif days_to_cover > 1:
        days_score = 3
    else:
        days_score = 1
    recommendation_score += days_score
    factors.append(f"Days to cover: {days_score}/10")

    # Generate recommendation
    if recommendation_score >= 70:
        action = "STRONG CAUTION"
        reasoning = "High short squeeze risk with bearish sentiment - avoid long positions"
        risk_level = "Very High"
    elif recommendation_score >= 50:
        action = "CAUTION"
        reasoning = "Moderate to high squeeze risk with negative sentiment - be cautious"
        risk_level = "High"
    elif recommendation_score >= 30:
        action = "MONITOR"
        reasoning = "Moderate risk - monitor closely for squeeze potential"
        risk_level = "Medium"
    elif recommendation_score >= 15:
        action = "CONSIDER"
        reasoning = "Low to moderate risk - consider with proper risk management"
        risk_level = "Low-Medium"
    else:
        action = "OPPORTUNITY"
        reasoning = "Low squeeze risk with minimal bearish sentiment - potential opportunity"
        risk_level = "Low"

    return {
        "action": action,
        "reasoning": reasoning,
        "risk_level": risk_level,
        "recommendation_score": recommendation_score,
        "factors": factors
    }

def _calculate_short_interest_risk_level(days_to_cover: float, squeeze_risk: Dict) -> int:
    """Calculate overall risk level (1-5 scale)"""
    risk_score = 0

    # Days to cover risk
    if days_to_cover > 10:
        risk_score += 3
    elif days_to_cover > 5:
        risk_score += 2
    elif days_to_cover > 2:
        risk_score += 1

    # Squeeze risk
    squeeze_levels = {"Extreme": 3, "High": 2, "Moderate": 1, "Low": 0, "Very Low": 0}
    risk_score += squeeze_levels.get(squeeze_risk["risk_level"], 0)

    # Cap at 5
    return min(5, risk_score)

def _calculate_short_interest_summary(results: List[Dict]) -> Dict:
    """Calculate summary statistics for short interest data"""
    if not results:
        return {}

    # Extract metrics
    days_to_cover_values = [r.get("days_to_cover", 0) for r in results]
    short_interest_values = [r.get("short_interest", 0) for r in results]
    volume_values = [r.get("avg_daily_volume", 0) for r in results]

    # Calculate statistics
    avg_days_to_cover = sum(days_to_cover_values) / len(days_to_cover_values) if days_to_cover_values else 0
    total_short_interest = sum(short_interest_values)
    total_volume = sum(volume_values)

    # Risk distribution
    risk_levels = [r.get("short_squeeze_risk", {}).get("risk_level", "Unknown") for r in results]
    risk_distribution = {}
    for level in risk_levels:
        risk_distribution[level] = risk_distribution.get(level, 0) + 1

    # Sentiment distribution
    sentiments = [r.get("market_sentiment", {}).get("sentiment", "Unknown") for r in results]
    sentiment_distribution = {}
    for sentiment in sentiments:
        sentiment_distribution[sentiment] = sentiment_distribution.get(sentiment, 0) + 1

    return {
        "total_records": len(results),
        "average_days_to_cover": round(avg_days_to_cover, 2),
        "total_short_interest": total_short_interest,
        "total_average_volume": total_volume,
        "risk_distribution": risk_distribution,
        "sentiment_distribution": sentiment_distribution,
        "high_risk_count": risk_distribution.get("High", 0) + risk_distribution.get("Extreme", 0),
        "bearish_sentiment_count": sentiment_distribution.get("Bearish", 0) + sentiment_distribution.get("Very Bearish", 0)
    }

# Helper functions for short volume analysis
def _analyze_venue_breakdown(result: Dict) -> Dict:
    """Analyze short volume by venue"""
    venues = {
        "NYSE": result.get("nyse_short_volume", 0),
        "Nasdaq Carteret": result.get("nasdaq_carteret_short_volume", 0),
        "Nasdaq Chicago": result.get("nasdaq_chicago_short_volume", 0),
        "ADF": result.get("adf_short_volume", 0)
    }

    total_venue_volume = sum(venues.values())
    breakdown = {}

    for venue, volume in venues.items():
        if total_venue_volume > 0:
            percentage = (volume / total_venue_volume) * 100
            breakdown[venue] = {
                "volume": volume,
                "percentage": round(percentage, 2),
                "dominance": "High" if percentage > 40 else "Medium" if percentage > 20 else "Low"
            }
        else:
            breakdown[venue] = {
                "volume": volume,
                "percentage": 0,
                "dominance": "None"
            }

    # Find dominant venue
    dominant_venue = max(venues.items(), key=lambda x: x[1]) if venues else ("Unknown", 0)

    return {
        "breakdown": breakdown,
        "dominant_venue": dominant_venue[0],
        "total_venue_volume": total_venue_volume,
        "venue_concentration": "High" if dominant_venue[1] / total_venue_volume > 0.6 else "Medium" if dominant_venue[1] / total_venue_volume > 0.4 else "Low" if total_venue_volume > 0 else "None"
    }

def _analyze_short_volume_ratio(short_volume_ratio: float) -> Dict:
    """Analyze short volume ratio"""
    if short_volume_ratio > 50:
        level = "Very High"
        assessment = "Extremely high short selling activity - strong bearish sentiment"
        implication = "Significant downward pressure expected"
    elif short_volume_ratio > 40:
        level = "High"
        assessment = "High short selling activity - notable bearish sentiment"
        implication = "Strong downward pressure"
    elif short_volume_ratio > 30:
        level = "Moderate-High"
        assessment = "Moderately high short selling activity"
        implication = "Moderate downward pressure"
    elif short_volume_ratio > 20:
        level = "Moderate"
        assessment = "Moderate short selling activity"
        implication = "Some downward pressure"
    elif short_volume_ratio > 10:
        level = "Low-Moderate"
        assessment = "Low to moderate short selling activity"
        implication = "Mild downward pressure"
    else:
        level = "Low"
        assessment = "Low short selling activity - minimal bearish sentiment"
        implication = "Minimal downward pressure"

    return {
        "level": level,
        "assessment": assessment,
        "implication": implication,
        "short_volume_ratio": round(short_volume_ratio, 2)
    }

def _analyze_exempt_volume(result: Dict) -> Dict:
    """Analyze exempt volume patterns"""
    exempt_volume = result.get("exempt_volume", 0)
    total_short_volume = result.get("short_volume", 0)

    if total_short_volume == 0:
        return {
            "exempt_percentage": 0,
            "level": "None",
            "assessment": "No short volume to analyze"
        }

    exempt_percentage = (exempt_volume / total_short_volume) * 100

    if exempt_percentage > 20:
        level = "High"
        assessment = "High proportion of exempt short sales - market making activity"
    elif exempt_percentage > 10:
        level = "Moderate"
        assessment = "Moderate exempt short sales - some market making"
    elif exempt_percentage > 5:
        level = "Low"
        assessment = "Low exempt short sales - minimal market making"
    else:
        level = "Very Low"
        assessment = "Very low exempt short sales - mostly speculative"

    return {
        "exempt_percentage": round(exempt_percentage, 2),
        "level": level,
        "assessment": assessment,
        "exempt_volume": exempt_volume,
        "total_short_volume": total_short_volume
    }

def _assess_short_selling_pressure(short_volume_ratio: float, venue_breakdown: Dict) -> Dict:
    """Assess short selling pressure"""
    pressure_score = 0

    # Ratio based pressure (70% weight)
    if short_volume_ratio > 50:
        pressure_score += 70
    elif short_volume_ratio > 40:
        pressure_score += 60
    elif short_volume_ratio > 30:
        pressure_score += 50
    elif short_volume_ratio > 20:
        pressure_score += 35
    elif short_volume_ratio > 10:
        pressure_score += 20
    else:
        pressure_score += 10

    # Venue concentration pressure (30% weight)
    concentration = venue_breakdown.get("venue_concentration", "Low")
    if concentration == "High":
        pressure_score += 30
    elif concentration == "Medium":
        pressure_score += 20
    elif concentration == "Low":
        pressure_score += 10

    # Determine pressure level
    if pressure_score >= 80:
        pressure_level = "Extreme"
        description = "Intense short selling pressure across multiple venues"
    elif pressure_score >= 60:
        pressure_level = "High"
        description = "Strong short selling pressure"
    elif pressure_score >= 40:
        pressure_level = "Moderate"
        description = "Moderate short selling pressure"
    elif pressure_score >= 20:
        pressure_level = "Low"
        description = "Low short selling pressure"
    else:
        pressure_level = "Very Low"
        description = "Minimal short selling pressure"

    return {
        "pressure_level": pressure_level,
        "pressure_score": pressure_score,
        "description": description
    }

def _analyze_short_selling_pattern(short_volume: int, total_volume: int, short_volume_ratio: float) -> Dict:
    """Analyze short selling pattern"""
    if total_volume == 0:
        return {
            "pattern": "Unknown",
            "assessment": "Cannot analyze without volume data"
        }

    # Pattern classification based on ratio and volume
    if short_volume_ratio > 50 and total_volume > 1000000:
        pattern = "High Volume - High Shorting"
        assessment = "High trading volume with intense short selling activity"
        implication = "Strong bearish sentiment with high liquidity"
    elif short_volume_ratio > 50:
        pattern = "Low Volume - High Shorting"
        assessment = "Low trading volume with intense short selling activity"
        implication = "Strong bearish sentiment with liquidity concerns"
    elif short_volume_ratio > 30 and total_volume > 1000000:
        pattern = "High Volume - Moderate Shorting"
        assessment = "High trading volume with moderate short selling activity"
        implication = "Moderate bearish sentiment with good liquidity"
    elif short_volume_ratio > 30:
        pattern = "Low Volume - Moderate Shorting"
        assessment = "Low trading volume with moderate short selling activity"
        implication = "Moderate bearish sentiment with liquidity concerns"
    elif short_volume_ratio > 15:
        pattern = "Balanced Shorting"
        assessment = "Moderate short selling activity relative to volume"
        implication = "Balanced sentiment with normal liquidity"
    else:
        pattern = "Low Shorting"
        assessment = "Low short selling activity relative to volume"
        implication = "Bullish to neutral sentiment"

    return {
        "pattern": pattern,
        "assessment": assessment,
        "implication": implication,
        "short_volume": short_volume,
        "total_volume": total_volume,
        "short_volume_ratio": round(short_volume_ratio, 2)
    }

def _assess_volume_distribution(result: Dict) -> Dict:
    """Assess volume distribution across venues"""
    venues = [
        ("NYSE", result.get("nyse_short_volume", 0)),
        ("Nasdaq Carteret", result.get("nasdaq_carteret_short_volume", 0)),
        ("Nasdaq Chicago", result.get("nasdaq_chicago_short_volume", 0)),
        ("ADF", result.get("adf_short_volume", 0))
    ]

    total_short = result.get("short_volume", 0)
    if total_short == 0:
        return {
            "distribution": "None",
            "assessment": "No short volume to analyze"
        }

    # Calculate distribution metrics
    active_venues = sum(1 for _, volume in venues if volume > 0)
    max_venue_volume = max(volume for _, volume in venues)
    max_venue_percentage = (max_venue_volume / total_short) * 100

    if active_venues >= 3 and max_venue_percentage < 50:
        distribution = "Well Distributed"
        assessment = "Short selling spread across multiple venues"
    elif active_venues >= 2 and max_venue_percentage < 70:
        distribution = "Moderately Distributed"
        assessment = "Short selling across multiple venues with some concentration"
    elif active_venues == 1:
        distribution = "Single Venue"
        assessment = "Short selling concentrated in single venue"
    else:
        distribution = "Concentrated"
        assessment = "Short selling heavily concentrated in dominant venue"

    return {
        "distribution": distribution,
        "assessment": assessment,
        "active_venues": active_venues,
        "max_venue_percentage": round(max_venue_percentage, 2),
        "venue_breakdown": {venue: volume for venue, volume in venues if volume > 0}
    }

def _determine_volume_sentiment(short_volume_ratio: float, pressure: Dict, pattern: Dict) -> Dict:
    """Determine market sentiment from short volume data"""
    sentiment_score = 0

    # Short volume ratio scoring (50% weight)
    if short_volume_ratio > 50:
        sentiment_score += 50
    elif short_volume_ratio > 40:
        sentiment_score += 40
    elif short_volume_ratio > 30:
        sentiment_score += 30
    elif short_volume_ratio > 20:
        sentiment_score += 20
    elif short_volume_ratio > 10:
        sentiment_score += 10

    # Pressure scoring (30% weight)
    pressure_levels = {"Extreme": 30, "High": 25, "Moderate": 15, "Low": 10, "Very Low": 5}
    sentiment_score += pressure_levels.get(pressure.get("pressure_level", "Low"), 0)

    # Pattern scoring (20% weight)
    if "High Shorting" in pattern.get("pattern", ""):
        sentiment_score += 20
    elif "Moderate Shorting" in pattern.get("pattern", ""):
        sentiment_score += 15
    elif "Balanced" in pattern.get("pattern", ""):
        sentiment_score += 10
    else:
        sentiment_score += 5

    # Determine sentiment
    if sentiment_score >= 75:
        sentiment = "Very Bearish"
        confidence = "High"
        implication = "Strong bearish sentiment indicated by intense short selling"
    elif sentiment_score >= 60:
        sentiment = "Bearish"
        confidence = "Medium-High"
        implication = "Bearish sentiment with significant short selling activity"
    elif sentiment_score >= 45:
        sentiment = "Neutral-Bearish"
        confidence = "Medium"
        implication = "Leaning bearish with moderate short selling"
    elif sentiment_score >= 30:
        sentiment = "Neutral"
        confidence = "Low-Medium"
        implication = "Balanced sentiment with normal short selling levels"
    else:
        sentiment = "Bullish-Neutral"
        confidence = "Low"
        implication = "Slightly bullish sentiment with low short selling"

    return {
        "sentiment": sentiment,
        "confidence": confidence,
        "implication": implication,
        "sentiment_score": sentiment_score
    }

def _generate_short_volume_trading_recommendation(
    short_volume_ratio: float, pressure: Dict, sentiment: Dict, distribution: Dict
) -> Dict:
    """Generate trading recommendation based on short volume analysis"""
    recommendation_score = 0
    factors = []

    # Factor 1: Short volume ratio (40% weight)
    if short_volume_ratio > 50:
        ratio_score = 40
    elif short_volume_ratio > 40:
        ratio_score = 35
    elif short_volume_ratio > 30:
        ratio_score = 25
    elif short_volume_ratio > 20:
        ratio_score = 15
    else:
        ratio_score = 5
    recommendation_score += ratio_score
    factors.append(f"Short volume ratio: {ratio_score}/40")

    # Factor 2: Selling pressure (25% weight)
    pressure_levels = {"Extreme": 25, "High": 20, "Moderate": 15, "Low": 10, "Very Low": 5}
    pressure_score = pressure_levels.get(pressure.get("pressure_level", "Low"), 0)
    recommendation_score += pressure_score
    factors.append(f"Selling pressure: {pressure_score}/25")

    # Factor 3: Sentiment (20% weight)
    if sentiment["sentiment"] == "Very Bearish":
        sentiment_score = 20
    elif sentiment["sentiment"] == "Bearish":
        sentiment_score = 17
    elif sentiment["sentiment"] == "Neutral-Bearish":
        sentiment_score = 12
    elif sentiment["sentiment"] == "Neutral":
        sentiment_score = 8
    else:
        sentiment_score = 5
    recommendation_score += sentiment_score
    factors.append(f"Sentiment: {sentiment_score}/20")

    # Factor 4: Distribution (15% weight)
    if distribution["distribution"] == "Single Venue":
        dist_score = 15
    elif distribution["distribution"] == "Concentrated":
        dist_score = 12
    elif distribution["distribution"] == "Moderately Distributed":
        dist_score = 8
    else:
        dist_score = 5
    recommendation_score += dist_score
    factors.append(f"Volume distribution: {dist_score}/15")

    # Generate recommendation
    if recommendation_score >= 70:
        action = "STRONG SELL"
        reasoning = "Intense short selling pressure with bearish sentiment - strong sell signal"
        risk_level = "Very High"
    elif recommendation_score >= 55:
        action = "SELL"
        reasoning = "High short selling pressure with bearish sentiment - sell signal"
        risk_level = "High"
    elif recommendation_score >= 40:
        action = "WEAK SELL"
        reasoning = "Moderate short selling pressure with bearish sentiment - weak sell signal"
        risk_level = "Medium-High"
    elif recommendation_score >= 25:
        action = "HOLD"
        reasoning = "Balanced short selling activity - hold position"
        risk_level = "Medium"
    elif recommendation_score >= 10:
        action = "WEAK BUY"
        reasoning = "Low short selling pressure with neutral sentiment - weak buy signal"
        risk_level = "Low-Medium"
    else:
        action = "BUY"
        reasoning = "Very low short selling pressure with bullish sentiment - buy signal"
        risk_level = "Low"

    return {
        "action": action,
        "reasoning": reasoning,
        "risk_level": risk_level,
        "recommendation_score": recommendation_score,
        "factors": factors
    }

def _calculate_short_volume_risk_level(short_volume_ratio: float, pressure: Dict) -> int:
    """Calculate overall risk level (1-5 scale) for short volume"""
    risk_score = 0

    # Ratio based risk
    if short_volume_ratio > 50:
        risk_score += 2
    elif short_volume_ratio > 30:
        risk_score += 1

    # Pressure based risk
    pressure_levels = {"Extreme": 2, "High": 2, "Moderate": 1, "Low": 0, "Very Low": 0}
    risk_score += pressure_levels.get(pressure.get("pressure_level", "Low"), 0)

    # Cap at 5
    return min(5, risk_score)

def _calculate_short_volume_summary(results: List[Dict]) -> Dict:
    """Calculate summary statistics for short volume data"""
    if not results:
        return {}

    # Extract metrics
    short_volume_ratios = [r.get("short_volume_ratio", 0) for r in results]
    short_volumes = [r.get("short_volume", 0) for r in results]
    total_volumes = [r.get("total_volume", 0) for r in results]

    # Calculate statistics
    avg_short_volume_ratio = sum(short_volume_ratios) / len(short_volume_ratios) if short_volume_ratios else 0
    total_short_volume = sum(short_volumes)
    total_volume = sum(total_volumes)

    # Pressure distribution
    pressure_levels = [r.get("selling_pressure", {}).get("pressure_level", "Unknown") for r in results]
    pressure_distribution = {}
    for level in pressure_levels:
        pressure_distribution[level] = pressure_distribution.get(level, 0) + 1

    # Sentiment distribution
    sentiments = [r.get("volume_sentiment", {}).get("sentiment", "Unknown") for r in results]
    sentiment_distribution = {}
    for sentiment in sentiments:
        sentiment_distribution[sentiment] = sentiment_distribution.get(sentiment, 0) + 1

    # Venue analysis
    venue_usage = {}
    for result in results:
        for venue in ["NYSE", "Nasdaq Carteret", "Nasdaq Chicago", "ADF"]:
            volume = result.get(f"{venue.lower().replace(' ', '_')}_short_volume", 0)
            if volume > 0:
                venue_usage[venue] = venue_usage.get(venue, 0) + 1

    return {
        "total_records": len(results),
        "average_short_volume_ratio": round(avg_short_volume_ratio, 2),
        "total_short_volume": total_short_volume,
        "total_volume": total_volume,
        "overall_short_ratio": round((total_short_volume / total_volume * 100) if total_volume > 0 else 0, 2),
        "pressure_distribution": pressure_distribution,
        "sentiment_distribution": sentiment_distribution,
        "venue_usage": venue_usage,
        "high_pressure_count": pressure_distribution.get("High", 0) + pressure_distribution.get("Extreme", 0),
        "bearish_sentiment_count": sentiment_distribution.get("Bearish", 0) + sentiment_distribution.get("Very Bearish", 0)
    }

def main():
    """Main CLI entry point"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python polygon_data.py <command> <args>",
            "commands": [
                "tickers --ticker=SYMBOL --type=TYPE --market=MARKET --exchange=EXCHANGE --cusip=CUSIP --cik=CIK --date=DATE --search=SEARCH --active=true/false --ticker-gte=SYMBOL --ticker-gt=SYMBOL --ticker-lte=SYMBOL --ticker-lt=SYMBOL --order=ORDER --limit=NUMBER --sort=SORT",
                "ticker-details --ticker=SYMBOL --date=DATE",
                "ticker-types --asset-class=ASSET_CLASS --locale=LOCALE",
                "related-tickers --ticker=SYMBOL",
                "news --ticker=SYMBOL --published-utc=DATE --limit=NUMBER --sort=SORT --order=ORDER",
                "ipos --ticker=SYMBOL --ipo-status=STATUS --listing-date=DATE --limit=NUMBER --sort=SORT --order=ORDER",
                "splits --ticker=SYMBOL --execution-date=DATE --reverse-split=true/false --limit=NUMBER --sort=SORT --order=ORDER",
                "dividends --ticker=SYMBOL --ex-dividend-date=DATE --frequency=NUMBER --dividend-type=TYPE --cash-amount=AMOUNT --limit=NUMBER --sort=SORT --order=ORDER",
                "ticker-events --identifier=IDENTIFIER --types=TYPES",
                "exchanges --asset-class=ASSET_CLASS --locale=LOCALE",
                "market-holidays",
                "market-status",
                "condition-codes --asset-class=ASSET_CLASS --data-type=TYPE --id=ID --sip=SIP --limit=NUMBER --sort=SORT --order=ORDER",
                "ticker-snapshot --ticker=SYMBOL",
                "market-snapshot --tickers=SYMBOL1,SYMBOL2 --include-otc=true/false",
                "unified-snapshot --ticker=SYMBOL --type=TYPE --ticker-any-of=SYMBOL1,SYMBOL2 --limit=NUMBER --sort=SORT --order=ORDER",
                "top-movers --direction=gainers/losers --include-otc=true/false",
                "trades --ticker=SYMBOL --timestamp=TIMESTAMP --timestamp-gte=TIMESTAMP --timestamp-lte=TIMESTAMP --limit=NUMBER --sort=SORT --order=ORDER",
                "last-trade --ticker=SYMBOL",
                "quotes --ticker=SYMBOL --timestamp=TIMESTAMP --timestamp-gte=TIMESTAMP --timestamp-lte=TIMESTAMP --limit=NUMBER --sort=SORT --order=ORDER",
                "last-quote --ticker=SYMBOL",
                "sma --ticker=SYMBOL --window=NUMBER --timespan=TIMESPAN --series-type=TYPE --adjusted=true/false --expand-underlying=true/false --limit=NUMBER",
                "ema --ticker=SYMBOL --window=NUMBER --timespan=TIMESPAN --series-type=TYPE --adjusted=true/false --expand-underlying=true/false --limit=NUMBER",
                "macd --ticker=SYMBOL --short-window=NUMBER --long-window=NUMBER --signal-window=NUMBER --timespan=TIMESPAN --series-type=TYPE --adjusted=true/false --expand-underlying=true/false --limit=NUMBER",
                "rsi --ticker=SYMBOL --window=NUMBER --timespan=TIMESPAN --series-type=TYPE --adjusted=true/false --expand-underlying=true/false --limit=NUMBER",
                "balance-sheets --cik=CIK --tickers=SYMBOL --period-end=DATE --filing-date=DATE --fiscal-year=YEAR --fiscal-quarter=QUARTER --timeframe=TIMEFRAME --limit=NUMBER --sort=SORT",
                "cash-flow-statements --cik=CIK --tickers=SYMBOL --period-end=DATE --filing-date=DATE --fiscal-year=YEAR --fiscal-quarter=QUARTER --timeframe=TIMEFRAME --limit=NUMBER --sort=SORT",
                "income-statements --cik=CIK --tickers=SYMBOL --period-end=DATE --filing-date=DATE --fiscal-year=YEAR --fiscal-quarter=QUARTER --timeframe=TIMEFRAME --limit=NUMBER --sort=SORT",
                "ratios --ticker=SYMBOL --timeframe=TIMEFRAME --period-type=TYPE --period=PERIOD --fy-of=YEAR --q-of=QUARTER --as-of-date=DATE --include-sources=true/false --period-of-report-date=DATE --date-field=FIELD --splits=true/false --field-format=FORMAT --precision=NUMBER --locale=LOCALE --sic=CODE --cik=CIK --unit-multiplier=MULTIPLIER --period-of-report-label=LABEL --include-qt-fact=true/false --search-by-column-values=VALUES --period-of-report-day=DAY --quarterly-report-day=DAY --payout-ratio-gte=VALUE --payout-ratio-lte=VALUE --payout-ratio-gt=VALUE --payout-ratio-lt=VALUE --payout-ratio-eq=VALUE --dividend-yield-gte=VALUE --dividend-yield-lte=VALUE --dividend-yield-gt=VALUE --dividend-yield-lt=VALUE --dividend-yield-eq=VALUE --dividend-per-share-gte=VALUE --dividend-per-share-lte=VALUE --dividend-per-share-gt=VALUE --dividend-per-share-lt=VALUE --dividend-per-share-eq=VALUE --dividend-yield-ttm-gte=VALUE --dividend-yield-ttm-lte=VALUE --dividend-yield-ttm-gt=VALUE --dividend-yield-ttm-lt=VALUE --dividend-yield-ttm-eq=VALUE --book-value-per-share-gte=VALUE --book-value-per-share-lte=VALUE --book-value-per-share-gt=VALUE --book-value-per-share-lt=VALUE --book-value-per-share-eq=VALUE --book-value-per-share-ttm-gte=VALUE --book-value-per-share-ttm-lte=VALUE --book-value-per-share-ttm-gt=VALUE --book-value-per-share-ttm-lt=VALUE --book-value-per-share-ttm-eq=VALUE --book-value-per-share-growth-ttm-pct-gte=VALUE --book-value-per-share-growth-ttm-pct-lte=VALUE --book-value-per-share-growth-ttm-pct-gt=VALUE --book-value-per-share-growth-ttm-pct-lt=VALUE --book-value-per-share-growth-ttm-pct-eq=VALUE --diluted-eps-growth-ttm-pct-gte=VALUE --diluted-eps-growth-ttm-pct-lte=VALUE --diluted-eps-growth-ttm-pct-gt=VALUE --diluted-eps-growth-ttm-pct-lt=VALUE --diluted-eps-growth-ttm-pct-eq=VALUE --basic-earnings-per-share-gte=VALUE --basic-earnings-per-share-lte=VALUE --basic-earnings-per-share-gt=VALUE --basic-earnings-per-share-lt=VALUE --basic-earnings-per-share-eq=VALUE --basic-eps-ttm-gte=VALUE --basic-eps-ttm-lte=VALUE --basic-eps-ttm-gt=VALUE --basic-eps-ttm-lt=VALUE --basic-eps-ttm-eq=VALUE --basic-average-shares-gte=VALUE --basic-average-shares-lte=VALUE --basic-average-shares-gt=VALUE --basic-average-shares-lt=VALUE --basic-average-shares-eq=VALUE --diluted-earnings-per-share-gte=VALUE --diluted-earnings-per-share-lte=VALUE --diluted-earnings-per-share-gt=VALUE --diluted-earnings-per-share-lt=VALUE --diluted-earnings-per-share-eq=VALUE --diluted-eps-ttm-gte=VALUE --diluted-eps-ttm-lte=VALUE --diluted-eps-ttm-gt=VALUE --diluted-eps-ttm-lt=VALUE --diluted-eps-ttm-eq=VALUE --diluted-average-shares-gte=VALUE --diluted-average-shares-lte=VALUE --diluted-average-shares-gt=VALUE --diluted-average-shares-lt=VALUE --diluted-average-shares-eq=VALUE --weighted-average-shares-gte=VALUE --weighted-average-shares-lte=VALUE --weighted-average-shares-gt=VALUE --weighted-average-shares-lt=VALUE --weighted-average-shares-eq=VALUE --market-capitalization-gte=VALUE --market-capitalization-lte=VALUE --market-capitalization-gt=VALUE --market-capitalization-lt=VALUE --market-capitalization-eq=VALUE --ev-gte=VALUE --ev-lte=VALUE --ev-gt=VALUE --ev-lt=VALUE --ev-eq=VALUE --pe-basic-gte=VALUE --pe-basic-lte=VALUE --pe-basic-gt=VALUE --pe-basic-lt=VALUE --pe-basic-eq=VALUE --pe-basic-ttm-gte=VALUE --pe-basic-ttm-lte=VALUE --pe-basic-ttm-gt=VALUE --pe-basic-ttm-lt=VALUE --pe-basic-ttm-eq=VALUE --pe-diluted-gte=VALUE --pe-diluted-lte=VALUE --pe-diluted-gt=VALUE --pe-diluted-lt=VALUE --pe-diluted-eq=VALUE --pe-diluted-ttm-gte=VALUE --pe-diluted-ttm-lte=VALUE --pe-diluted-ttm-gt=VALUE --pe-diluted-ttm-lt=VALUE --pe-diluted-ttm-eq=VALUE --pb-ttm-gte=VALUE --pb-ttm-lte=VALUE --pb-ttm-gt=VALUE --pb-ttm-lt=VALUE --pb-ttm-eq=VALUE --roe-ttm-gte=VALUE --roe-ttm-lte=VALUE --roe-ttm-gt=VALUE --roe-ttm-lt=VALUE --roe-ttm-eq=VALUE --roa-ttm-gte=VALUE --roa-ttm-lte=VALUE --roa-ttm-gt=VALUE --roa-ttm-lt=VALUE --roa-ttm-eq=VALUE --roic-ttm-gte=VALUE --roic-ttm-lte=VALUE --roic-ttm-gt=VALUE --roic-ttm-lt=VALUE --roic-ttm-eq=VALUE --profit-margin-ttm-gte=VALUE --profit-margin-ttm-lte=VALUE --profit-margin-ttm-gt=VALUE --profit-margin-ttm-lt=VALUE --profit-margin-ttm-eq=VALUE --gross-margin-ttm-gte=VALUE --gross-margin-ttm-lte=VALUE --gross-margin-ttm-gt=VALUE --gross-margin-ttm-lt=VALUE --gross-margin-ttm-eq=VALUE --sga-to-revenue-ttm-gte=VALUE --sga-to-revenue-ttm-lte=VALUE --sga-to-revenue-ttm-gt=VALUE --sga-to-revenue-ttm-lt=VALUE --sga-to-revenue-ttm-eq=VALUE --rd-to-revenue-ttm-gte=VALUE --rd-to-revenue-ttm-lte=VALUE --rd-to-revenue-ttm-gt=VALUE --rd-to-revenue-ttm-lt=VALUE --rd-to-revenue-ttm-eq=VALUE --r-and-d-to-revenue-ttm-gte=VALUE --r-and-d-to-revenue-ttm-lte=VALUE --r-and-d-to-revenue-ttm-gt=VALUE --r-and-d-to-revenue-ttm-lt=VALUE --r-and-d-to-revenue-ttm-eq=VALUE --effective-tax-rate-ttm-gte=VALUE --effective-tax-rate-ttm-lte=VALUE --effective-tax-rate-ttm-gt=VALUE --effective-tax-rate-ttm-lt=VALUE --effective-tax-rate-ttm-eq=VALUE --return-on-tangible-assets-ttm-gte=VALUE --return-on-tangible-assets-ttm-lte=VALUE --return-on-tangible-assets-ttm-gt=VALUE --return-on-tangible-assets-ttm-lt=VALUE --return-on-tangible-assets-ttm-eq=VALUE --interest-coverage-ttm-gte=VALUE --interest-coverage-ttm-lte=VALUE --interest-coverage-ttm-gt=VALUE --interest-coverage-ttm-lt=VALUE --interest-coverage-ttm-eq=VALUE --current-ratio-gte=VALUE --current-ratio-lte=VALUE --current-ratio-gt=VALUE --current-ratio-lt=VALUE --current-ratio-eq=VALUE --quick-ratio-gte=VALUE --quick-ratio-lte=VALUE --quick-ratio-gt=VALUE --quick-ratio-lt=VALUE --quick-ratio-eq=VALUE --cash-ratio-gte=VALUE --cash-ratio-lte=VALUE --cash-ratio-gt=VALUE --cash-ratio-lt=VALUE --cash-ratio-eq=VALUE --days-of-sales-outstanding-gte=VALUE --days-of-sales-outstanding-lte=VALUE --days-of-sales-outstanding-gt=VALUE --days-of-sales-outstanding-lt=VALUE --days-of-sales-outstanding-eq=VALUE --days-of-inventory-on-hand-gte=VALUE --days-of-inventory-on-hand-lte=VALUE --days-of-inventory-on-hand-gt=VALUE --days-of-inventory-on-hand-lt=VALUE --days-of-inventory-on-hand-eq=VALUE --ebitda-margin-ttm-gte=VALUE --ebitda-margin-ttm-lte=VALUE --ebitda-margin-ttm-gt=VALUE --ebitda-margin-ttm-lt=VALUE --ebitda-margin-ttm-eq=VALUE --ebitda-to-interest-coverage-ttm-gte=VALUE --ebitda-to-interest-coverage-ttm-lte=VALUE --ebitda-to-interest-coverage-ttm-gt=VALUE --ebitda-to-interest-coverage-ttm-lt=VALUE --ebitda-to-interest-coverage-ttm-eq=VALUE --ebitda-to-revenue-ttm-gte=VALUE --ebitda-to-revenue-ttm-lte=VALUE --ebitda-to-revenue-ttm-gt=VALUE --ebitda-to-revenue-ttm-lt=VALUE --ebitda-to-revenue-ttm-eq=VALUE --ev-to-ebitda-ttm-gte=VALUE --ev-to-ebitda-ttm-lte=VALUE --ev-to-ebitda-ttm-gt=VALUE --ev-to-ebitda-ttm-lt=VALUE --ev-to-ebitda-ttm-eq=VALUE --ev-to-operating-cash-flow-ttm-gte=VALUE --ev-to-operating-cash-flow-ttm-lte=VALUE --ev-to-operating-cash-flow-ttm-gt=VALUE --ev-to-operating-cash-flow-ttm-lt=VALUE --ev-to-operating-cash-flow-ttm-eq=VALUE --ev-to-sales-ttm-gte=VALUE --ev-to-sales-ttm-lte=VALUE --ev-to-sales-ttm-gt=VALUE --ev-to-sales-ttm-lt=VALUE --ev-to-sales-ttm-eq=VALUE --ps-ttm-gte=VALUE --ps-ttm-lte=VALUE --ps-ttm-gt=VALUE --ps-ttm-lt=VALUE --ps-ttm-eq=VALUE --price-to-book-ttm-gte=VALUE --price-to-book-ttm-lte=VALUE --price-to-book-ttm-gt=VALUE --price-to-book-ttm-lt=VALUE --price-to-book-ttm-eq=VALUE --price-to-tangible-book-ttm-gte=VALUE --price-to-tangible-book-ttm-lte=VALUE --price-to-tangible-book-ttm-gt=VALUE --price-to-tangible-book-ttm-lt=VALUE --price-to-tangible-book-ttm-eq=VALUE --price-to-sales-ttm-gte=VALUE --price-to-sales-ttm-lte=VALUE --price-to-sales-ttm-gt=VALUE --price-to-sales-ttm-lt=VALUE --price-to-sales-ttm-eq=VALUE --fcfe-yield-ttm-gte=VALUE --fcfe-yield-ttm-lte=VALUE --fcfe-yield-ttm-gt=VALUE --fcfe-yield-ttm-lt=VALUE --fcfe-yield-ttm-eq=VALUE --fcff-yield-ttm-gte=VALUE --fcff-yield-ttm-lte=VALUE --fcff-yield-ttm-gt=VALUE --fcff-yield-ttm-lt=VALUE --fcff-yield-ttm-eq=VALUE --dividend-yield-basic-ttm-gte=VALUE --dividend-yield-basic-ttm-lte=VALUE --dividend-yield-basic-ttm-gt=VALUE --dividend-yield-basic-ttm-lt=VALUE --dividend-yield-basic-ttm-eq=VALUE --dividend-yield-ttm-gte=VALUE --dividend-yield-ttm-lte=VALUE --dividend-yield-ttm-gt=VALUE --dividend-yield-ttm-lt=VALUE --dividend-yield-ttm-eq=VALUE --total-debt-to-capitalization-gte=VALUE --total-debt-to-capitalization-lte=VALUE --total-debt-to-capitalization-gt=VALUE --total-debt-to-capitalization-lt=VALUE --total-debt-to-capitalization-eq=VALUE --total-debt-to-equity-gte=VALUE --total-debt-to-equity-lte=VALUE --total-debt-to-equity-gt=VALUE --total-debt-to-equity-lt=VALUE --total-debt-to-equity-eq=VALUE --long-term-debt-to-equity-gte=VALUE --long-term-debt-to-equity-lte=VALUE --long-term-debt-to-equity-gt=VALUE --long-term-debt-to-equity-lt=VALUE --long-term-debt-to-equity-eq=VALUE --short-term-debt-to-equity-gte=VALUE --short-term-debt-to-equity-lte=VALUE --short-term-debt-to-equity-gt=VALUE --short-term-debt-to-equity-lt=VALUE --short-term-debt-to-equity-eq=VALUE --long-term-debt-to-total-assets-gte=VALUE --long-term-debt-to-total-assets-lte=VALUE --long-term-debt-to-total-assets-gt=VALUE --long-term-debt-to-total-assets-lt=VALUE --long-term-debt-to-total-assets-eq=VALUE --total-assets-to-total-equity-gte=VALUE --total-assets-to-total-equity-lte=VALUE --total-assets-to-total-equity-gt=VALUE --total-assets-to-total-equity-lt=VALUE --total-assets-to-total-equity-eq=VALUE --debt-to-assets-gte=VALUE --debt-to-assets-lte=VALUE --debt-to-assets-gt=VALUE --debt-to-assets-lt=VALUE --debt-to-assets-eq=VALUE --book-yield-ttm-gte=VALUE --book-yield-ttm-lte=VALUE --book-yield-ttm-gt=VALUE --book-yield-ttm-lt=VALUE --book-yield-ttm-eq=VALUE --dividend-payout-ratio-ttm-gte=VALUE --dividend-payout-ratio-ttm-lte=VALUE --dividend-payout-ratio-ttm-gt=VALUE --dividend-payout-ratio-ttm-lt=VALUE --dividend-payout-ratio-ttm-eq=VALUE --free-cash-flow-yield-ttm-gte=VALUE --free-cash-flow-yield-ttm-lte=VALUE --free-cash-flow-yield-ttm-gt=VALUE --free-cash-flow-yield-ttm-lt=VALUE --free-cash-flow-yield-ttm-eq=VALUE --graham-number-ttm-gte=VALUE --graham-number-ttm-lte=VALUE --graham-number-ttm-gt=VALUE --graham-number-ttm-lt=VALUE --graham-number-ttm-eq=VALUE --graham-number-ttm-to-net-current-asset-value-ttm-gte=VALUE --graham-number-ttm-to-net-current-asset-value-ttm-lte=VALUE --graham-number-ttm-to-net-current-asset-value-ttm-gt=VALUE --graham-number-ttm-to-net-current-asset-value-ttm-lt=VALUE --graham-number-ttm-to-net-current-asset-value-ttm-eq=VALUE",
                "short-interest --ticker=SYMBOL --ticker-any-of=SYMBOLS --ticker-gt=SYMBOL --ticker-gte=SYMBOL --ticker-lt=SYMBOL --ticker-lte=SYMBOL --days-to-cover=NUMBER --days-to-cover-any-of=VALUES --days-to-cover-gt=NUMBER --days-to-cover-gte=NUMBER --days-to-cover-lt=NUMBER --days-to-cover-lte=NUMBER --settlement-date=DATE --settlement-date-any-of=DATES --settlement-date-gt=DATE --settlement-date-gte=DATE --settlement-date-lt=DATE --settlement-date-lte=DATE --avg-daily-volume=NUMBER --avg-daily-volume-any-of=VALUES --avg-daily-volume-gt=NUMBER --avg-daily-volume-gte=NUMBER --avg-daily-volume-lt=NUMBER --avg-daily-volume-lte=NUMBER --limit=NUMBER --sort=SORT",
                "short-volume --ticker=SYMBOL --ticker-any-of=SYMBOLS --ticker-gt=SYMBOL --ticker-gte=SYMBOL --ticker-lt=SYMBOL --ticker-lte=SYMBOL --date=DATE --date-any-of=DATES --date-gt=DATE --date-gte=DATE --date-lt=DATE --date-lte=DATE --short-volume-ratio=NUMBER --short-volume-ratio-any-of=VALUES --short-volume-ratio-gt=NUMBER --short-volume-ratio-gte=NUMBER --short-volume-ratio-lt=NUMBER --short-volume-ratio-lte=NUMBER --total-volume=NUMBER --total-volume-any-of=VALUES --total-volume-gt=NUMBER --total-volume-gte=NUMBER --total-volume-lt=NUMBER --total-volume-lte=NUMBER --limit=NUMBER --sort=SORT",
                "Examples:",
                "  python polygon_data.py tickers --ticker=AAPL --type=CS --market=stocks --exchange=XNYS --limit=100 --active=true",
                "  python polygon_data.py ticker-details --ticker=AAPL --date=2021-04-25",
                "  python polygon_data.py ticker-types --asset-class=stocks --locale=us",
                "  python polygon_data.py related-tickers --ticker=AAPL",
                "  python polygon_data.py news --ticker=AAPL --limit=10 --sort=published_utc --order=descending",
                "  python polygon_data.py ipos --ipo-status=pending --limit=20 --sort=listing_date",
                "  python polygon_data.py splits --ticker=AAPL --limit=10 --sort=execution_date",
                "  python polygon_data.py dividends --ticker=AAPL --frequency=4 --limit=20 --sort=pay_date",
                "  python polygon_data.py ticker-events --identifier=META --types=ticker_change",
                "  python polygon_data.py exchanges --asset-class=stocks --locale=us",
                "  python polygon_data.py market-holidays",
                "  python polygon_data.py market-status",
                "  python polygon_data.py condition-codes --asset-class=stocks --data-type=trade --limit=50",
                "  python polygon_data.py ticker-snapshot --ticker=AAPL",
                "  python polygon_data.py market-snapshot --tickers=AAPL,TSLA,GOOG --include-otc=false",
                "  python polygon_data.py unified-snapshot --type=stocks --limit=50",
                "  python polygon_data.py unified-snapshot --ticker-any-of=AAPL,TSLA,BTC-USD --limit=10",
                "  python polygon_data.py top-movers --direction=gainers --include-otc=false",
                "  python polygon_data.py top-movers --direction=losers --include-otc=true",
                "  python polygon_data.py trades --ticker=AAPL --timestamp-gte=2021-04-25 --timestamp-lte=2021-04-25 --limit=1000",
                "  python polygon_data.py trades --ticker=AAPL --timestamp=1619337600000000000 --limit=500",
                "  python polygon_data.py last-trade --ticker=AAPL",
                "  python polygon_data.py quotes --ticker=AAPL --timestamp-gte=2021-04-25 --timestamp-lte=2021-04-25 --limit=1000",
                "  python polygon_data.py last-quote --ticker=AAPL",
                "  python polygon_data.py sma --ticker=AAPL --window=20 --timespan=day --series-type=close --limit=50",
                "  python polygon_data.py ema --ticker=AAPL --window=12 --timespan=day --series-type=close --expand-underlying=true --limit=30",
                "  python polygon_data.py macd --ticker=AAPL --short-window=12 --long-window=26 --signal-window=9 --timespan=day --series-type=close --expand-underlying=true --limit=50",
                "  python polygon_data.py rsi --ticker=AAPL --window=14 --timespan=day --series-type=close --expand-underlying=true --limit=50",
                "  python polygon_data.py balance-sheets --tickers=AAPL --timeframe=quarterly --limit=10 --sort=period_end.desc",
                "  python polygon_data.py balance-sheets --cik=0000320193 --fiscal-year=2024 --timeframe=annual --limit=5",
                "  python polygon_data.py balance-sheets --tickers=AAPL,MSFT,GOOGL --timeframe=quarterly --fiscal-year=2024 --limit=30",
                "  python polygon_data.py cash-flow-statements --tickers=AAPL --timeframe=quarterly --limit=10 --sort=period_end.desc",
                "  python polygon_data.py cash-flow-statements --cik=0000320193 --fiscal-year=2024 --timeframe=trailing_twelve_months --limit=5",
                "  python polygon_data.py cash-flow-statements --tickers=AAPL,MSFT,GOOGL --timeframe=annual --fiscal-year-gte=2023 --limit=15",
                "  python polygon_data.py income-statements --tickers=AAPL --timeframe=quarterly --limit=10 --sort=period_end.desc",
                "  python polygon_data.py income-statements --cik=0000320193 --fiscal-year=2024 --timeframe=trailing_twelve_months --limit=5",
                "  python polygon_data.py income-statements --tickers=AAPL,MSFT,GOOGL --timeframe=annual --fiscal-year-gte=2023 --limit=15",
                "  python polygon_data.py ratios --ticker=AAPL --timeframe=quarterly --limit=10 --sort=period_end.desc",
                "  python polygon_data.py ratios --ticker=AAPL --timeframe=trailing_twelve_months --pe-basic-ttm-gte=10 --roe-ttm-gte=0.15 --current-ratio-gte=1.5",
                "  python polygon_data.py ratios --ticker=AAPL --timeframe=quarterly --pb-ttm-lte=3 --ps-ttm-lte=5 --debt-to-equity-lte=1.0 --roe-ttm-gte=0.10",
                "  python polygon_data.py ratios --ticker=AAPL --timeframe=annual --market-capitalization-gte=1000000000 --ev-to-ebitda-ttm-lte=15 --roic-ttm-gte=0.12",
                "  python polygon_data.py ratios --ticker=AAPL --timeframe=quarterly --dividend-yield-ttm-gte=0.02 --profit-margin-ttm-gte=0.10 --quick-ratio-gte=1.0",
                "  python polygon_data.py ratios --ticker=AAPL --timeframe=trailing_twelve_months --pe-basic-ttm-gte=5 --pe-basic-ttm-lte=20 --roe-ttm-gte=0.15 --roa-ttm-gte=0.05",
                "  python polygon_data.py short-interest --ticker=AAPL --limit=10 --sort=settlement_date.desc",
                "  python polygon_data.py short-interest --ticker=AAPL --days-to-cover-gte=5 --settlement-date-gte=2024-01-01 --limit=20",
                "  python polygon_data.py short-interest --ticker-any-of=AAPL,TSLA,MSFT --days-to-cover-gt=2 --avg-daily-volume-gte=1000000 --sort=settlement_date.desc",
                "  python polygon_data.py short-interest --ticker=AAPL --settlement-date=2024-03-14 --days-to-cover-lte=10 --avg-daily-volume-gte=500000",
                "  python polygon_data.py short-volume --ticker=AAPL --limit=10 --sort=date.desc",
                "  python polygon_data.py short-volume --ticker=AAPL --short-volume-ratio-gte=30 --date-gte=2024-01-01 --limit=50",
                "  python polygon_data.py short-volume --ticker=AAPL --date=2024-03-25 --short-volume-ratio-lte=50 --total-volume-gte=100000",
                "  python polygon_data.py short-volume --ticker-any-of=AAPL,TSLA,MSFT --short-volume-ratio-gt=25 --date-gte=2024-03-01 --sort=date.desc"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]

    if command == "tickers":
        # Parse optional arguments using key-value pairs
        kwargs = {}

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                # Convert specific parameter types
                if key == 'active':
                    kwargs[key] = value.lower() in ('true', '1', 'yes')
                elif key == 'limit':
                    try:
                        kwargs[key] = int(value)
                    except ValueError:
                        pass
                elif key in ['ticker_gte', 'ticker_gt', 'ticker_lte', 'ticker_lt']:
                    # Map CLI parameter names to function parameter names
                    if key == 'ticker_gte':
                        kwargs['ticker_gte'] = value
                    elif key == 'ticker_gt':
                        kwargs['ticker_gt'] = value
                    elif key == 'ticker_lte':
                        kwargs['ticker_lte'] = value
                    elif key == 'ticker_lt':
                        kwargs['ticker_lt'] = value
                else:
                    kwargs[key] = value

        result = get_all_tickers(**kwargs)
        print(json.dumps(result, indent=2))

    elif command == "ticker-details":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: python polygon_data.py ticker-details --ticker=SYMBOL [--date=DATE]"}))
            sys.exit(1)

        # Parse arguments for ticker details
        kwargs = {}
        ticker = None

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                if key == 'ticker':
                    ticker = value
                else:
                    kwargs[key] = value

        if not ticker:
            print(json.dumps({"error": "Ticker symbol is required"}))
            sys.exit(1)

        result = get_ticker_details(ticker, **kwargs)
        print(json.dumps(result, indent=2))

    elif command == "ticker-types":
        # Parse optional arguments for ticker types
        kwargs = {}

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python
                kwargs[key] = value

        result = get_ticker_types(**kwargs)
        print(json.dumps(result, indent=2))

    elif command == "related-tickers":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: python polygon_data.py related-tickers --ticker=SYMBOL"}))
            sys.exit(1)

        # Parse arguments for related tickers
        ticker = None

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                if key == 'ticker':
                    ticker = value

        if not ticker:
            print(json.dumps({"error": "Ticker symbol is required"}))
            sys.exit(1)

        result = get_related_tickers(ticker)
        print(json.dumps(result, indent=2))

    elif command == "news":
        # Parse optional arguments for news
        kwargs = {}

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                # Convert specific parameter types
                if key == 'limit':
                    try:
                        kwargs[key] = int(value)
                    except ValueError:
                        pass
                elif key in ['ticker_gte', 'ticker_gt', 'ticker_lte', 'ticker_lt',
                           'published_utc_gte', 'published_utc_gt', 'published_utc_lte', 'published_utc_lt']:
                    # Map CLI parameter names to function parameter names
                    kwargs[key] = value
                else:
                    kwargs[key] = value

        result = get_news(**kwargs)
        print(json.dumps(result, indent=2))

    elif command == "ipos":
        # Parse optional arguments for IPOs
        kwargs = {}

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                # Convert specific parameter types
                if key == 'limit':
                    try:
                        kwargs[key] = int(value)
                    except ValueError:
                        pass
                elif key in ['listing_date_gte', 'listing_date_gt', 'listing_date_lte', 'listing_date_lt']:
                    # Map CLI parameter names to function parameter names
                    kwargs[key] = value
                else:
                    kwargs[key] = value

        result = get_ipos(**kwargs)
        print(json.dumps(result, indent=2))

    elif command == "splits":
        # Parse optional arguments for stock splits
        kwargs = {}

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                # Convert specific parameter types
                if key == 'limit':
                    try:
                        kwargs[key] = int(value)
                    except ValueError:
                        pass
                elif key == 'reverse_split':
                    kwargs[key] = value.lower() in ('true', '1', 'yes')
                elif key in ['ticker_gte', 'ticker_gt', 'ticker_lte', 'ticker_lt',
                           'execution_date_gte', 'execution_date_gt', 'execution_date_lte', 'execution_date_lt']:
                    # Map CLI parameter names to function parameter names
                    kwargs[key] = value
                else:
                    kwargs[key] = value

        result = get_splits(**kwargs)
        print(json.dumps(result, indent=2))

    elif command == "dividends":
        # Parse optional arguments for dividends
        kwargs = {}

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                # Convert specific parameter types
                if key in ['limit', 'frequency']:
                    try:
                        kwargs[key] = int(value)
                    except ValueError:
                        pass
                elif key == 'cash_amount':
                    try:
                        kwargs[key] = float(value)
                    except ValueError:
                        pass
                elif key in ['ticker_gte', 'ticker_gt', 'ticker_lte', 'ticker_lt',
                           'ex_dividend_date_gte', 'ex_dividend_date_gt', 'ex_dividend_date_lte', 'ex_dividend_date_lt',
                           'record_date_gte', 'record_date_gt', 'record_date_lte', 'record_date_lt',
                           'declaration_date_gte', 'declaration_date_gt', 'declaration_date_lte', 'declaration_date_lt',
                           'pay_date_gte', 'pay_date_gt', 'pay_date_lte', 'pay_date_lt',
                           'cash_amount_gte', 'cash_amount_gt', 'cash_amount_lte', 'cash_amount_lt']:
                    # Map CLI parameter names to function parameter names
                    kwargs[key] = value
                else:
                    kwargs[key] = value

        result = get_dividends(**kwargs)
        print(json.dumps(result, indent=2))

    elif command == "ticker-events":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: python polygon_data.py ticker-events --identifier=IDENTIFIER [--types=TYPES]"}))
            sys.exit(1)

        # Parse arguments for ticker events
        kwargs = {}
        identifier = None

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                if key == 'identifier':
                    identifier = value
                else:
                    kwargs[key] = value

        if not identifier:
            print(json.dumps({"error": "Identifier (ticker, CUSIP, or Composite FIGI) is required"}))
            sys.exit(1)

        result = get_ticker_events(identifier, **kwargs)
        print(json.dumps(result, indent=2))

    elif command == "exchanges":
        # Parse optional arguments for exchanges
        kwargs = {}

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python
                kwargs[key] = value

        result = get_exchanges(**kwargs)
        print(json.dumps(result, indent=2))

    elif command == "market-holidays":
        result = get_market_holidays()
        print(json.dumps(result, indent=2))

    elif command == "market-status":
        result = get_market_status()
        print(json.dumps(result, indent=2))

    elif command == "condition-codes":
        # Parse optional arguments for condition codes
        kwargs = {}

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                # Convert specific parameter types
                if key == 'id' or key == 'limit':
                    try:
                        kwargs[key] = int(value)
                    except ValueError:
                        pass
                else:
                    kwargs[key] = value

        result = get_condition_codes(**kwargs)
        print(json.dumps(result, indent=2))

    elif command == "ticker-snapshot":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: python polygon_data.py ticker-snapshot --ticker=SYMBOL"}))
            sys.exit(1)

        # Parse arguments for ticker snapshot
        ticker = None

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                if key == 'ticker':
                    ticker = value

        if not ticker:
            print(json.dumps({"error": "Ticker symbol is required"}))
            sys.exit(1)

        result = get_single_ticker_snapshot(ticker)
        print(json.dumps(result, indent=2))

    elif command == "market-snapshot":
        # Parse arguments for market snapshot
        kwargs = {}

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                # Convert specific parameter types
                if key == 'include_otc':
                    kwargs[key] = value.lower() in ('true', '1', 'yes')
                elif key == 'tickers':
                    # Split comma-separated tickers into list
                    if value:
                        kwargs[key] = [ticker.strip() for ticker in value.split(',')]
                    else:
                        kwargs[key] = None

        result = get_full_market_snapshot(**kwargs)
        print(json.dumps(result, indent=2))

    elif command == "unified-snapshot":
        # Parse arguments for unified snapshot
        kwargs = {}

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                # Convert specific parameter types
                if key == 'limit':
                    try:
                        kwargs[key] = int(value)
                    except ValueError:
                        pass
                elif key in ['ticker_gte', 'ticker_gt', 'ticker_lte', 'ticker_lt']:
                    # Map CLI parameter names to function parameter names
                    kwargs[key] = value
                else:
                    kwargs[key] = value

        result = get_unified_snapshot(**kwargs)
        print(json.dumps(result, indent=2))

    elif command == "top-movers":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: python polygon_data.py top-movers --direction=gainers/losers [--include-otc=true/false]"}))
            sys.exit(1)

        # Parse arguments for top market movers
        kwargs = {}
        direction = None

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                if key == 'direction':
                    direction = value
                elif key == 'include_otc':
                    kwargs['include_otc'] = value.lower() in ('true', '1', 'yes')

        if not direction:
            print(json.dumps({"error": "Direction parameter is required and must be either 'gainers' or 'losers'"}))
            sys.exit(1)

        result = get_top_market_movers(direction, **kwargs)
        print(json.dumps(result, indent=2))

    elif command == "trades":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: python polygon_data.py trades --ticker=SYMBOL [--timestamp=TIMESTAMP] [--timestamp-gte=TIMESTAMP] [--timestamp-lte=TIMESTAMP] [--limit=NUMBER] [--sort=SORT] [--order=ORDER]"}))
            sys.exit(1)

        # Parse arguments for trades
        kwargs = {}
        stock_ticker = None

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                if key == 'ticker':
                    stock_ticker = value
                elif key == 'limit':
                    try:
                        kwargs[key] = int(value)
                    except ValueError:
                        pass
                elif key in ['timestamp_gte', 'timestamp_gt', 'timestamp_lte', 'timestamp_lt']:
                    # Map CLI parameter names to function parameter names
                    kwargs[key] = value
                else:
                    kwargs[key] = value

        if not stock_ticker:
            print(json.dumps({"error": "Ticker symbol is required"}))
            sys.exit(1)

        result = get_trades(stock_ticker, **kwargs)
        print(json.dumps(result, indent=2))

    elif command == "last-trade":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: python polygon_data.py last-trade --ticker=SYMBOL"}))
            sys.exit(1)

        # Parse arguments for last trade
        stocks_ticker = None

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                if key == 'ticker':
                    stocks_ticker = value

        if not stocks_ticker:
            print(json.dumps({"error": "Ticker symbol is required"}))
            sys.exit(1)

        result = get_last_trade(stocks_ticker)
        print(json.dumps(result, indent=2))

    elif command == "quotes":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: python polygon_data.py quotes --ticker=SYMBOL [--timestamp=TIMESTAMP] [--timestamp-gte=TIMESTAMP] [--timestamp-lte=TIMESTAMP] [--limit=NUMBER] [--sort=SORT] [--order=ORDER]"}))
            sys.exit(1)

        # Parse arguments for quotes
        kwargs = {}
        stock_ticker = None

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                if key == 'ticker':
                    stock_ticker = value
                elif key == 'limit':
                    try:
                        kwargs[key] = int(value)
                    except ValueError:
                        pass
                elif key in ['timestamp_gte', 'timestamp_gt', 'timestamp_lte', 'timestamp_lt']:
                    # Map CLI parameter names to function parameter names
                    kwargs[key] = value
                else:
                    kwargs[key] = value

        if not stock_ticker:
            print(json.dumps({"error": "Ticker symbol is required"}))
            sys.exit(1)

        result = get_quotes(stock_ticker, **kwargs)
        print(json.dumps(result, indent=2))

    elif command == "last-quote":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: python polygon_data.py last-quote --ticker=SYMBOL"}))
            sys.exit(1)

        # Parse arguments for last quote
        stocks_ticker = None

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                if key == 'ticker':
                    stocks_ticker = value

        if not stocks_ticker:
            print(json.dumps({"error": "Ticker symbol is required"}))
            sys.exit(1)

        result = get_last_quote(stocks_ticker)
        print(json.dumps(result, indent=2))

    elif command == "sma":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: python polygon_data.py sma --ticker=SYMBOL --window=NUMBER --timespan=TIMESPAN --series-type=TYPE --adjusted=true/false --expand-underlying=true/false --limit=NUMBER"}))
            sys.exit(1)

        # Parse arguments for SMA
        kwargs = {}
        stock_ticker = None

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                if key == 'ticker':
                    stock_ticker = value
                elif key in ['limit', 'window']:
                    try:
                        kwargs[key] = int(value)
                    except ValueError:
                        pass
                elif key in ['adjusted', 'expand_underlying']:
                    kwargs[key] = value.lower() in ('true', '1', 'yes')
                elif key in ['timestamp_gte', 'timestamp_gt', 'timestamp_lte', 'timestamp_lt']:
                    # Map CLI parameter names to function parameter names
                    kwargs[key] = value
                else:
                    kwargs[key] = value

        if not stock_ticker:
            print(json.dumps({"error": "Ticker symbol is required"}))
            sys.exit(1)

        result = get_sma(stock_ticker, **kwargs)
        print(json.dumps(result, indent=2))

    elif command == "ema":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: python polygon_data.py ema --ticker=SYMBOL --window=NUMBER --timespan=TIMESPAN --series-type=TYPE --adjusted=true/false --expand-underlying=true/false --limit=NUMBER"}))
            sys.exit(1)

        # Parse arguments for EMA
        kwargs = {}
        stock_ticker = None

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                if key == 'ticker':
                    stock_ticker = value
                elif key in ['limit', 'window']:
                    try:
                        kwargs[key] = int(value)
                    except ValueError:
                        pass
                elif key in ['adjusted', 'expand_underlying']:
                    kwargs[key] = value.lower() in ('true', '1', 'yes')
                elif key in ['timestamp_gte', 'timestamp_gt', 'timestamp_lte', 'timestamp_lt']:
                    # Map CLI parameter names to function parameter names
                    kwargs[key] = value
                else:
                    kwargs[key] = value

        if not stock_ticker:
            print(json.dumps({"error": "Ticker symbol is required"}))
            sys.exit(1)

        result = get_ema(stock_ticker, **kwargs)
        print(json.dumps(result, indent=2))

    elif command == "macd":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: python polygon_data.py macd --ticker=SYMBOL --short-window=NUMBER --long-window=NUMBER --signal-window=NUMBER --timespan=TIMESPAN --series-type=TYPE --adjusted=true/false --expand-underlying=true/false --limit=NUMBER"}))
            sys.exit(1)

        # Parse arguments for MACD
        kwargs = {}
        stock_ticker = None

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                if key == 'ticker':
                    stock_ticker = value
                elif key in ['limit', 'short_window', 'long_window', 'signal_window']:
                    try:
                        kwargs[key] = int(value)
                    except ValueError:
                        pass
                elif key in ['adjusted', 'expand_underlying']:
                    kwargs[key] = value.lower() in ('true', '1', 'yes')
                elif key in ['timestamp_gte', 'timestamp_gt', 'timestamp_lte', 'timestamp_lt']:
                    # Map CLI parameter names to function parameter names
                    kwargs[key] = value
                else:
                    kwargs[key] = value

        if not stock_ticker:
            print(json.dumps({"error": "Ticker symbol is required"}))
            sys.exit(1)

        result = get_macd(stock_ticker, **kwargs)
        print(json.dumps(result, indent=2))

    elif command == "rsi":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: python polygon_data.py rsi --ticker=SYMBOL --window=NUMBER --timespan=TIMESPAN --series-type=TYPE --adjusted=true/false --expand-underlying=true/false --limit=NUMBER"}))
            sys.exit(1)

        # Parse arguments for RSI
        kwargs = {}
        stock_ticker = None

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                if key == 'ticker':
                    stock_ticker = value
                elif key in ['limit', 'window']:
                    try:
                        kwargs[key] = int(value)
                    except ValueError:
                        pass
                elif key in ['adjusted', 'expand_underlying']:
                    kwargs[key] = value.lower() in ('true', '1', 'yes')
                elif key in ['timestamp_gte', 'timestamp_gt', 'timestamp_lte', 'timestamp_lt']:
                    # Map CLI parameter names to function parameter names
                    kwargs[key] = value
                else:
                    kwargs[key] = value

        if not stock_ticker:
            print(json.dumps({"error": "Ticker symbol is required"}))
            sys.exit(1)

        result = get_rsi(stock_ticker, **kwargs)
        print(json.dumps(result, indent=2))

    elif command == "balance-sheets":
        # Parse arguments for balance sheets
        kwargs = {}

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                # Map CLI parameter names to function parameter names
                if key == 'cik':
                    kwargs['cik'] = value
                elif key == 'cik_any_of':
                    kwargs['cik_any_of'] = value
                elif key == 'cik_gt':
                    kwargs['cik_gt'] = value
                elif key == 'cik_gte':
                    kwargs['cik_gte'] = value
                elif key == 'cik_lt':
                    kwargs['cik_lt'] = value
                elif key == 'cik_lte':
                    kwargs['cik_lte'] = value
                elif key == 'tickers':
                    kwargs['tickers'] = value
                elif key == 'tickers_all_of':
                    kwargs['tickers_all_of'] = value
                elif key == 'tickers_any_of':
                    kwargs['tickers_any_of'] = value
                elif key == 'period_end':
                    kwargs['period_end'] = value
                elif key == 'period_end_gt':
                    kwargs['period_end_gt'] = value
                elif key == 'period_end_gte':
                    kwargs['period_end_gte'] = value
                elif key == 'period_end_lt':
                    kwargs['period_end_lt'] = value
                elif key == 'period_end_lte':
                    kwargs['period_end_lte'] = value
                elif key == 'filing_date':
                    kwargs['filing_date'] = value
                elif key == 'filing_date_gt':
                    kwargs['filing_date_gt'] = value
                elif key == 'filing_date_gte':
                    kwargs['filing_date_gte'] = value
                elif key == 'filing_date_lt':
                    kwargs['filing_date_lt'] = value
                elif key == 'filing_date_lte':
                    kwargs['filing_date_lte'] = value
                elif key == 'fiscal_year':
                    try:
                        kwargs['fiscal_year'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_year_gt':
                    try:
                        kwargs['fiscal_year_gt'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_year_gte':
                    try:
                        kwargs['fiscal_year_gte'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_year_lt':
                    try:
                        kwargs['fiscal_year_lt'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_year_lte':
                    try:
                        kwargs['fiscal_year_lte'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_quarter':
                    try:
                        kwargs['fiscal_quarter'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_quarter_gt':
                    try:
                        kwargs['fiscal_quarter_gt'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_quarter_gte':
                    try:
                        kwargs['fiscal_quarter_gte'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_quarter_lt':
                    try:
                        kwargs['fiscal_quarter_lt'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_quarter_lte':
                    try:
                        kwargs['fiscal_quarter_lte'] = float(value)
                    except ValueError:
                        pass
                elif key == 'timeframe':
                    kwargs['timeframe'] = value
                elif key == 'timeframe_any_of':
                    kwargs['timeframe_any_of'] = value
                elif key == 'timeframe_gt':
                    kwargs['timeframe_gt'] = value
                elif key == 'timeframe_gte':
                    kwargs['timeframe_gte'] = value
                elif key == 'timeframe_lt':
                    kwargs['timeframe_lt'] = value
                elif key == 'timeframe_lte':
                    kwargs['timeframe_lte'] = value
                elif key == 'limit':
                    try:
                        kwargs['limit'] = int(value)
                    except ValueError:
                        pass
                elif key == 'sort':
                    kwargs['sort'] = value

        result = get_balance_sheets(**kwargs)
        print(json.dumps(result, indent=2))

    elif command == "cash-flow-statements":
        # Parse arguments for cash flow statements
        kwargs = {}

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                # Map CLI parameter names to function parameter names
                if key == 'cik':
                    kwargs['cik'] = value
                elif key == 'cik_any_of':
                    kwargs['cik_any_of'] = value
                elif key == 'cik_gt':
                    kwargs['cik_gt'] = value
                elif key == 'cik_gte':
                    kwargs['cik_gte'] = value
                elif key == 'cik_lt':
                    kwargs['cik_lt'] = value
                elif key == 'cik_lte':
                    kwargs['cik_lte'] = value
                elif key == 'period_end':
                    kwargs['period_end'] = value
                elif key == 'period_end_gt':
                    kwargs['period_end_gt'] = value
                elif key == 'period_end_gte':
                    kwargs['period_end_gte'] = value
                elif key == 'period_end_lt':
                    kwargs['period_end_lt'] = value
                elif key == 'period_end_lte':
                    kwargs['period_end_lte'] = value
                elif key == 'filing_date':
                    kwargs['filing_date'] = value
                elif key == 'filing_date_gt':
                    kwargs['filing_date_gt'] = value
                elif key == 'filing_date_gte':
                    kwargs['filing_date_gte'] = value
                elif key == 'filing_date_lt':
                    kwargs['filing_date_lt'] = value
                elif key == 'filing_date_lte':
                    kwargs['filing_date_lte'] = value
                elif key == 'tickers':
                    kwargs['tickers'] = value
                elif key == 'tickers_all_of':
                    kwargs['tickers_all_of'] = value
                elif key == 'tickers_any_of':
                    kwargs['tickers_any_of'] = value
                elif key == 'fiscal_year':
                    try:
                        kwargs['fiscal_year'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_year_gt':
                    try:
                        kwargs['fiscal_year_gt'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_year_gte':
                    try:
                        kwargs['fiscal_year_gte'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_year_lt':
                    try:
                        kwargs['fiscal_year_lt'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_year_lte':
                    try:
                        kwargs['fiscal_year_lte'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_quarter':
                    try:
                        kwargs['fiscal_quarter'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_quarter_gt':
                    try:
                        kwargs['fiscal_quarter_gt'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_quarter_gte':
                    try:
                        kwargs['fiscal_quarter_gte'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_quarter_lt':
                    try:
                        kwargs['fiscal_quarter_lt'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_quarter_lte':
                    try:
                        kwargs['fiscal_quarter_lte'] = float(value)
                    except ValueError:
                        pass
                elif key == 'timeframe':
                    kwargs['timeframe'] = value
                elif key == 'timeframe_any_of':
                    kwargs['timeframe_any_of'] = value
                elif key == 'timeframe_gt':
                    kwargs['timeframe_gt'] = value
                elif key == 'timeframe_gte':
                    kwargs['timeframe_gte'] = value
                elif key == 'timeframe_lt':
                    kwargs['timeframe_lt'] = value
                elif key == 'timeframe_lte':
                    kwargs['timeframe_lte'] = value
                elif key == 'limit':
                    try:
                        kwargs['limit'] = int(value)
                    except ValueError:
                        pass
                elif key == 'sort':
                    kwargs['sort'] = value

        result = get_cash_flow_statements(**kwargs)
        print(json.dumps(result, indent=2))

    elif command == "income-statements":
        # Parse arguments for income statements
        kwargs = {}

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                # Map CLI parameter names to function parameter names
                if key == 'cik':
                    kwargs['cik'] = value
                elif key == 'cik_any_of':
                    kwargs['cik_any_of'] = value
                elif key == 'cik_gt':
                    kwargs['cik_gt'] = value
                elif key == 'cik_gte':
                    kwargs['cik_gte'] = value
                elif key == 'cik_lt':
                    kwargs['cik_lt'] = value
                elif key == 'cik_lte':
                    kwargs['cik_lte'] = value
                elif key == 'tickers':
                    kwargs['tickers'] = value
                elif key == 'tickers_all_of':
                    kwargs['tickers_all_of'] = value
                elif key == 'tickers_any_of':
                    kwargs['tickers_any_of'] = value
                elif key == 'period_end':
                    kwargs['period_end'] = value
                elif key == 'period_end_gt':
                    kwargs['period_end_gt'] = value
                elif key == 'period_end_gte':
                    kwargs['period_end_gte'] = value
                elif key == 'period_end_lt':
                    kwargs['period_end_lt'] = value
                elif key == 'period_end_lte':
                    kwargs['period_end_lte'] = value
                elif key == 'filing_date':
                    kwargs['filing_date'] = value
                elif key == 'filing_date_gt':
                    kwargs['filing_date_gt'] = value
                elif key == 'filing_date_gte':
                    kwargs['filing_date_gte'] = value
                elif key == 'filing_date_lt':
                    kwargs['filing_date_lt'] = value
                elif key == 'filing_date_lte':
                    kwargs['filing_date_lte'] = value
                elif key == 'fiscal_year':
                    try:
                        kwargs['fiscal_year'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_year_gt':
                    try:
                        kwargs['fiscal_year_gt'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_year_gte':
                    try:
                        kwargs['fiscal_year_gte'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_year_lt':
                    try:
                        kwargs['fiscal_year_lt'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_year_lte':
                    try:
                        kwargs['fiscal_year_lte'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_quarter':
                    try:
                        kwargs['fiscal_quarter'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_quarter_gt':
                    try:
                        kwargs['fiscal_quarter_gt'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_quarter_gte':
                    try:
                        kwargs['fiscal_quarter_gte'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_quarter_lt':
                    try:
                        kwargs['fiscal_quarter_lt'] = float(value)
                    except ValueError:
                        pass
                elif key == 'fiscal_quarter_lte':
                    try:
                        kwargs['fiscal_quarter_lte'] = float(value)
                    except ValueError:
                        pass
                elif key == 'timeframe':
                    kwargs['timeframe'] = value
                elif key == 'timeframe_any_of':
                    kwargs['timeframe_any_of'] = value
                elif key == 'timeframe_gt':
                    kwargs['timeframe_gt'] = value
                elif key == 'timeframe_gte':
                    kwargs['timeframe_gte'] = value
                elif key == 'timeframe_lt':
                    kwargs['timeframe_lt'] = value
                elif key == 'timeframe_lte':
                    kwargs['timeframe_lte'] = value
                elif key == 'limit':
                    try:
                        kwargs['limit'] = int(value)
                    except ValueError:
                        pass
                elif key == 'sort':
                    kwargs['sort'] = value

        result = get_income_statements(**kwargs)
        print(json.dumps(result, indent=2))

    elif command == "ratios":
        # Parse optional arguments using key-value pairs
        kwargs = {}

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                # Handle .any_of parameters
                if key.endswith('_any_of'):
                    kwargs[key] = value
                # Handle range parameters (.gt, .gte, .lt, .lte)
                elif key.endswith(('_gt', '_gte', '_lt', '_lte')):
                    kwargs[key] = value
                # Convert numeric parameters
                elif key == 'limit':
                    try:
                        kwargs[key] = int(value)
                    except ValueError:
                        kwargs[key] = value
                elif key in ['price', 'price_gt', 'price_gte', 'price_lt', 'price_lte',
                           'dividend_yield_gte', 'dividend_yield_lte', 'dividend_yield_gt', 'dividend_yield_lt', 'dividend_yield_eq',
                           'dividend_per_share_gte', 'dividend_per_share_lte', 'dividend_per_share_gt', 'dividend_per_share_lt', 'dividend_per_share_eq',
                           'dividend_yield_ttm_gte', 'dividend_yield_ttm_lte', 'dividend_yield_ttm_gt', 'dividend_yield_ttm_lt', 'dividend_yield_ttm_eq',
                           'book_value_per_share_gte', 'book_value_per_share_lte', 'book_value_per_share_gt', 'book_value_per_share_lt', 'book_value_per_share_eq',
                           'book_value_per_share_ttm_gte', 'book_value_per_share_ttm_lte', 'book_value_per_share_ttm_gt', 'book_value_per_share_ttm_lt', 'book_value_per_share_ttm_eq',
                           'book_value_per_share_growth_ttm_pct_gte', 'book_value_per_share_growth_ttm_pct_lte', 'book_value_per_share_growth_ttm_pct_gt', 'book_value_per_share_growth_ttm_pct_lt', 'book_value_per_share_growth_ttm_pct_eq',
                           'diluted_eps_growth_ttm_pct_gte', 'diluted_eps_growth_ttm_pct_lte', 'diluted_eps_growth_ttm_pct_gt', 'diluted_eps_growth_ttm_pct_lt', 'diluted_eps_growth_ttm_pct_eq',
                           'basic_earnings_per_share_gte', 'basic_earnings_per_share_lte', 'basic_earnings_per_share_gt', 'basic_earnings_per_share_lt', 'basic_earnings_per_share_eq',
                           'basic_eps_ttm_gte', 'basic_eps_ttm_lte', 'basic_eps_ttm_gt', 'basic_eps_ttm_lt', 'basic_eps_ttm_eq',
                           'basic_average_shares_gte', 'basic_average_shares_lte', 'basic_average_shares_gt', 'basic_average_shares_lt', 'basic_average_shares_eq',
                           'diluted_earnings_per_share_gte', 'diluted_earnings_per_share_lte', 'diluted_earnings_per_share_gt', 'diluted_earnings_per_share_lt', 'diluted_earnings_per_share_eq',
                           'diluted_eps_ttm_gte', 'diluted_eps_ttm_lte', 'diluted_eps_ttm_gt', 'diluted_eps_ttm_lt', 'diluted_eps_ttm_eq',
                           'diluted_average_shares_gte', 'diluted_average_shares_lte', 'diluted_average_shares_gt', 'diluted_average_shares_lt', 'diluted_average_shares_eq',
                           'weighted_average_shares_gte', 'weighted_average_shares_lte', 'weighted_average_shares_gt', 'weighted_average_shares_lt', 'weighted_average_shares_eq',
                           'market_capitalization_gte', 'market_capitalization_lte', 'market_capitalization_gt', 'market_capitalization_lt', 'market_capitalization_eq',
                           'ev_gte', 'ev_lte', 'ev_gt', 'ev_lt', 'ev_eq',
                           'pe_basic_gte', 'pe_basic_lte', 'pe_basic_gt', 'pe_basic_lt', 'pe_basic_eq',
                           'pe_basic_ttm_gte', 'pe_basic_ttm_lte', 'pe_basic_ttm_gt', 'pe_basic_ttm_lt', 'pe_basic_ttm_eq',
                           'pe_diluted_gte', 'pe_diluted_lte', 'pe_diluted_gt', 'pe_diluted_lt', 'pe_diluted_eq',
                           'pe_diluted_ttm_gte', 'pe_diluted_ttm_lte', 'pe_diluted_ttm_gt', 'pe_diluted_ttm_lt', 'pe_diluted_ttm_eq',
                           'pb_ttm_gte', 'pb_ttm_lte', 'pb_ttm_gt', 'pb_ttm_lt', 'pb_ttm_eq',
                           'roe_ttm_gte', 'roe_ttm_lte', 'roe_ttm_gt', 'roe_ttm_lt', 'roe_ttm_eq',
                           'roa_ttm_gte', 'roa_ttm_lte', 'roa_ttm_gt', 'roa_ttm_lt', 'roa_ttm_eq',
                           'roic_ttm_gte', 'roic_ttm_lte', 'roic_ttm_gt', 'roic_ttm_lt', 'roic_ttm_eq',
                           'profit_margin_ttm_gte', 'profit_margin_ttm_lte', 'profit_margin_ttm_gt', 'profit_margin_ttm_lt', 'profit_margin_ttm_eq',
                           'gross_margin_ttm_gte', 'gross_margin_ttm_lte', 'gross_margin_ttm_gt', 'gross_margin_ttm_lt', 'gross_margin_ttm_eq',
                           'sga_to_revenue_ttm_gte', 'sga_to_revenue_ttm_lte', 'sga_to_revenue_ttm_gt', 'sga_to_revenue_ttm_lt', 'sga_to_revenue_ttm_eq',
                           'rd_to_revenue_ttm_gte', 'rd_to_revenue_ttm_lte', 'rd_to_revenue_ttm_gt', 'rd_to_revenue_ttm_lt', 'rd_to_revenue_ttm_eq',
                           'r_and_d_to_revenue_ttm_gte', 'r_and_d_to_revenue_ttm_lte', 'r_and_d_to_revenue_ttm_gt', 'r_and_d_to_revenue_ttm_lt', 'r_and_d_to_revenue_ttm_eq',
                           'effective_tax_rate_ttm_gte', 'effective_tax_rate_ttm_lte', 'effective_tax_rate_ttm_gt', 'effective_tax_rate_ttm_lt', 'effective_tax_rate_ttm_eq',
                           'return_on_tangible_assets_ttm_gte', 'return_on_tangible_assets_ttm_lte', 'return_on_tangible_assets_ttm_gt', 'return_on_tangible_assets_ttm_lt', 'return_on_tangible_assets_ttm_eq',
                           'interest_coverage_ttm_gte', 'interest_coverage_ttm_lte', 'interest_coverage_ttm_gt', 'interest_coverage_ttm_lt', 'interest_coverage_ttm_eq',
                           'current_ratio_gte', 'current_ratio_lte', 'current_ratio_gt', 'current_ratio_lt', 'current_ratio_eq',
                           'quick_ratio_gte', 'quick_ratio_lte', 'quick_ratio_gt', 'quick_ratio_lt', 'quick_ratio_eq',
                           'cash_ratio_gte', 'cash_ratio_lte', 'cash_ratio_gt', 'cash_ratio_lt', 'cash_ratio_eq',
                           'days_of_sales_outstanding_gte', 'days_of_sales_outstanding_lte', 'days_of_sales_outstanding_gt', 'days_of_sales_outstanding_lt', 'days_of_sales_outstanding_eq',
                           'days_of_inventory_on_hand_gte', 'days_of_inventory_on_hand_lte', 'days_of_inventory_on_hand_gt', 'days_of_inventory_on_hand_lt', 'days_of_inventory_on_hand_eq',
                           'ebitda_margin_ttm_gte', 'ebitda_margin_ttm_lte', 'ebitda_margin_ttm_gt', 'ebitda_margin_ttm_lt', 'ebitda_margin_ttm_eq',
                           'ebitda_to_interest_coverage_ttm_gte', 'ebitda_to_interest_coverage_ttm_lte', 'ebitda_to_interest_coverage_ttm_gt', 'ebitda_to_interest_coverage_ttm_lt', 'ebitda_to_interest_coverage_ttm_eq',
                           'ebitda_to_revenue_ttm_gte', 'ebitda_to_revenue_ttm_lte', 'ebitda_to_revenue_ttm_gt', 'ebitda_to_revenue_ttm_lt', 'ebitda_to_revenue_ttm_eq',
                           'ev_to_ebitda_ttm_gte', 'ev_to_ebitda_ttm_lte', 'ev_to_ebitda_ttm_gt', 'ev_to_ebitda_ttm_lt', 'ev_to_ebitda_ttm_eq',
                           'ev_to_operating_cash_flow_ttm_gte', 'ev_to_operating_cash_flow_ttm_lte', 'ev_to_operating_cash_flow_ttm_gt', 'ev_to_operating_cash_flow_ttm_lt', 'ev_to_operating_cash_flow_ttm_eq',
                           'ev_to_sales_ttm_gte', 'ev_to_sales_ttm_lte', 'ev_to_sales_ttm_gt', 'ev_to_sales_ttm_lt', 'ev_to_sales_ttm_eq',
                           'ps_ttm_gte', 'ps_ttm_lte', 'ps_ttm_gt', 'ps_ttm_lt', 'ps_ttm_eq',
                           'price_to_book_ttm_gte', 'price_to_book_ttm_lte', 'price_to_book_ttm_gt', 'price_to_book_ttm_lt', 'price_to_book_ttm_eq',
                           'price_to_tangible_book_ttm_gte', 'price_to_tangible_book_ttm_lte', 'price_to_tangible_book_ttm_gt', 'price_to_tangible_book_ttm_lt', 'price_to_tangible_book_ttm_eq',
                           'price_to_sales_ttm_gte', 'price_to_sales_ttm_lte', 'price_to_sales_ttm_gt', 'price_to_sales_ttm_lt', 'price_to_sales_ttm_eq',
                           'fcfe_yield_ttm_gte', 'fcfe_yield_ttm_lte', 'fcfe_yield_ttm_gt', 'fcfe_yield_ttm_lt', 'fcfe_yield_ttm_eq',
                           'fcff_yield_ttm_gte', 'fcff_yield_ttm_lte', 'fcff_yield_ttm_gt', 'fcff_yield_ttm_lt', 'fcff_yield_ttm_eq',
                           'dividend_yield_basic_ttm_gte', 'dividend_yield_basic_ttm_lte', 'dividend_yield_basic_ttm_gt', 'dividend_yield_basic_ttm_lt', 'dividend_yield_basic_ttm_eq',
                           'dividend_yield_ttm_gte', 'dividend_yield_ttm_lte', 'dividend_yield_ttm_gt', 'dividend_yield_ttm_lt', 'dividend_yield_ttm_eq',
                           'total_debt_to_capitalization_gte', 'total_debt_to_capitalization_lte', 'total_debt_to_capitalization_gt', 'total_debt_to_capitalization_lt', 'total_debt_to_capitalization_eq',
                           'total_debt_to_equity_gte', 'total_debt_to_equity_lte', 'total_debt_to_equity_gt', 'total_debt_to_equity_lt', 'total_debt_to_equity_eq',
                           'long_term_debt_to_equity_gte', 'long_term_debt_to_equity_lte', 'long_term_debt_to_equity_gt', 'long_term_debt_to_equity_lt', 'long_term_debt_to_equity_eq',
                           'short_term_debt_to_equity_gte', 'short_term_debt_to_equity_lte', 'short_term_debt_to_equity_gt', 'short_term_debt_to_equity_lt', 'short_term_debt_to_equity_eq',
                           'long_term_debt_to_total_assets_gte', 'long_term_debt_to_total_assets_lte', 'long_term_debt_to_total_assets_gt', 'long_term_debt_to_total_assets_lt', 'long_term_debt_to_total_assets_eq',
                           'total_assets_to_total_equity_gte', 'total_assets_to_total_equity_lte', 'total_assets_to_total_equity_gt', 'total_assets_to_total_equity_lt', 'total_assets_to_total_equity_eq',
                           'debt_to_assets_gte', 'debt_to_assets_lte', 'debt_to_assets_gt', 'debt_to_assets_lt', 'debt_to_assets_eq',
                           'book_yield_ttm_gte', 'book_yield_ttm_lte', 'book_yield_ttm_gt', 'book_yield_ttm_lt', 'book_yield_ttm_eq',
                           'dividend_payout_ratio_ttm_gte', 'dividend_payout_ratio_ttm_lte', 'dividend_payout_ratio_ttm_gt', 'dividend_payout_ratio_ttm_lt', 'dividend_payout_ratio_ttm_eq',
                           'free_cash_flow_yield_ttm_gte', 'free_cash_flow_yield_ttm_lte', 'free_cash_flow_yield_ttm_gt', 'free_cash_flow_yield_ttm_lt', 'free_cash_flow_yield_ttm_eq',
                           'graham_number_ttm_gte', 'graham_number_ttm_lte', 'graham_number_ttm_gt', 'graham_number_ttm_lt', 'graham_number_ttm_eq',
                           'graham_number_ttm_to_net_current_asset_value_ttm_gte', 'graham_number_ttm_to_net_current_asset_value_ttm_lte', 'graham_number_ttm_to_net_current_asset_value_ttm_gt', 'graham_number_ttm_to_net_current_asset_value_ttm_lt', 'graham_number_ttm_to_net_current_asset_value_ttm_eq']:
                    try:
                        kwargs[key] = float(value)
                    except ValueError:
                        kwargs[key] = value
                else:
                    kwargs[key] = value

        result = get_financial_ratios(**kwargs)
        print(json.dumps(result, indent=2))

    elif command == "short-interest":
        # Parse optional arguments using key-value pairs
        kwargs = {}

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                # Convert specific parameter types
                if key in ['limit']:
                    try:
                        kwargs[key] = int(value)
                    except ValueError:
                        kwargs[key] = value
                elif key in ['days_to_cover', 'days_to_cover_any_of', 'days_to_cover_gt', 'days_to_cover_gte', 'days_to_cover_lt', 'days_to_cover_lte',
                           'avg_daily_volume', 'avg_daily_volume_any_of', 'avg_daily_volume_gt', 'avg_daily_volume_gte', 'avg_daily_volume_lt', 'avg_daily_volume_lte']:
                    try:
                        kwargs[key] = float(value) if key.startswith('days_to_cover') else int(value)
                    except ValueError:
                        kwargs[key] = value
                else:
                    kwargs[key] = value

        result = get_short_interest(**kwargs)
        print(json.dumps(result, indent=2))

    elif command == "short-volume":
        # Parse optional arguments using key-value pairs
        kwargs = {}

        for arg in sys.argv[2:]:
            if '=' in arg:
                key, value = arg.split('=', 1)
                key = key.lstrip('-')  # Remove leading dashes
                key = key.replace('-', '_')  # Replace dashes with underscores for Python

                # Convert specific parameter types
                if key in ['limit']:
                    try:
                        kwargs[key] = int(value)
                    except ValueError:
                        kwargs[key] = value
                elif key in ['short_volume_ratio', 'short_volume_ratio_any_of', 'short_volume_ratio_gt', 'short_volume_ratio_gte', 'short_volume_ratio_lt', 'short_volume_ratio_lte',
                           'total_volume', 'total_volume_any_of', 'total_volume_gt', 'total_volume_gte', 'total_volume_lt', 'total_volume_lte']:
                    try:
                        kwargs[key] = float(value) if key.startswith('short_volume_ratio') else int(value)
                    except ValueError:
                        kwargs[key] = value
                else:
                    kwargs[key] = value

        result = get_short_volume(**kwargs)
        print(json.dumps(result, indent=2))

    else:
        print(json.dumps({"error": f"Unknown command: {command}"}))
        sys.exit(1)

if __name__ == "__main__":
    main()