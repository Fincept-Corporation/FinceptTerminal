import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any, Tuple
from datetime import date, datetime
import json
import pypme

def calculate_pme(
    cashflows: Union[List[float], np.ndarray],
    prices: Union[List[float], np.ndarray],
    pme_prices: Union[List[float], np.ndarray]
) -> Dict[str, Any]:
    """Calculate Public Market Equivalent (PME)"""
    cashflows = list(cashflows) if isinstance(cashflows, np.ndarray) else cashflows
    prices = list(prices) if isinstance(prices, np.ndarray) else prices
    pme_prices = list(pme_prices) if isinstance(pme_prices, np.ndarray) else pme_prices

    result = pypme.pme(cashflows, prices, pme_prices)

    return {
        'pme': float(result)
    }

def calculate_verbose_pme(
    cashflows: Union[List[float], np.ndarray],
    prices: Union[List[float], np.ndarray],
    pme_prices: Union[List[float], np.ndarray]
) -> Dict[str, Any]:
    """Calculate PME with detailed output"""
    cashflows = list(cashflows) if isinstance(cashflows, np.ndarray) else cashflows
    prices = list(prices) if isinstance(prices, np.ndarray) else prices
    pme_prices = list(pme_prices) if isinstance(pme_prices, np.ndarray) else pme_prices

    pme, nav_pme, df = pypme.verbose_pme(cashflows, prices, pme_prices)

    return {
        'pme': float(pme),
        'nav_pme': float(nav_pme),
        'details': df.to_dict(orient='records')
    }

def calculate_xpme(
    dates: Union[List[date], List[str]],
    cashflows: Union[List[float], np.ndarray],
    prices: Union[List[float], np.ndarray],
    pme_prices: Union[List[float], np.ndarray]
) -> Dict[str, Any]:
    """Calculate Extended PME (xPME)"""
    if isinstance(dates[0], str):
        dates = [datetime.strptime(d, '%Y-%m-%d').date() for d in dates]

    cashflows = list(cashflows) if isinstance(cashflows, np.ndarray) else cashflows
    prices = list(prices) if isinstance(prices, np.ndarray) else prices
    pme_prices = list(pme_prices) if isinstance(pme_prices, np.ndarray) else pme_prices

    result = pypme.xpme(dates, cashflows, prices, pme_prices)

    return {
        'xpme': float(result)
    }

def calculate_verbose_xpme(
    dates: Union[List[date], List[str]],
    cashflows: Union[List[float], np.ndarray],
    prices: Union[List[float], np.ndarray],
    pme_prices: Union[List[float], np.ndarray]
) -> Dict[str, Any]:
    """Calculate xPME with detailed output"""
    if isinstance(dates[0], str):
        dates = [datetime.strptime(d, '%Y-%m-%d').date() for d in dates]

    cashflows = list(cashflows) if isinstance(cashflows, np.ndarray) else cashflows
    prices = list(prices) if isinstance(prices, np.ndarray) else prices
    pme_prices = list(pme_prices) if isinstance(pme_prices, np.ndarray) else pme_prices

    xpme, nav_pme, df = pypme.verbose_xpme(dates, cashflows, prices, pme_prices)

    return {
        'xpme': float(xpme),
        'nav_pme': float(nav_pme),
        'details': df.to_dict(orient='records')
    }

def calculate_tessa_xpme(
    dates: Union[List[date], List[str]],
    cashflows: Union[List[float], np.ndarray],
    prices: Union[List[float], np.ndarray],
    pme_ticker: str,
    pme_source: str = 'yahoo'
) -> Dict[str, Any]:
    """Calculate xPME using Tessa for market data"""
    if isinstance(dates[0], str):
        dates = [datetime.strptime(d, '%Y-%m-%d').date() for d in dates]

    cashflows = list(cashflows) if isinstance(cashflows, np.ndarray) else cashflows
    prices = list(prices) if isinstance(prices, np.ndarray) else prices

    result = pypme.tessa_xpme(dates, cashflows, prices, pme_ticker, pme_source)

    return {
        'xpme': float(result)
    }

def calculate_tessa_verbose_xpme(
    dates: Union[List[date], List[str]],
    cashflows: Union[List[float], np.ndarray],
    prices: Union[List[float], np.ndarray],
    pme_ticker: str,
    pme_source: str = 'yahoo'
) -> Dict[str, Any]:
    """Calculate xPME with Tessa and detailed output"""
    if isinstance(dates[0], str):
        dates = [datetime.strptime(d, '%Y-%m-%d').date() for d in dates]

    cashflows = list(cashflows) if isinstance(cashflows, np.ndarray) else cashflows
    prices = list(prices) if isinstance(prices, np.ndarray) else prices

    xpme, nav_pme, df = pypme.tessa_verbose_xpme(dates, cashflows, prices, pme_ticker, pme_source)

    return {
        'xpme': float(xpme),
        'nav_pme': float(nav_pme),
        'details': df.to_dict(orient='records')
    }

def pick_prices_from_dataframe(
    dates: Union[List[date], List[str]],
    pricedf: pd.DataFrame,
    column: str
) -> Dict[str, Any]:
    """Extract prices from DataFrame for given dates"""
    if isinstance(dates[0], str):
        dates = [datetime.strptime(d, '%Y-%m-%d').date() for d in dates]

    prices = pypme.pick_prices_from_dataframe(dates, pricedf, column)

    return {
        'prices': prices
    }

def main():
    print("Testing pypme wrapper")

    dates = [date(2020, 1, 1), date(2021, 1, 1), date(2022, 1, 1)]
    cashflows = [-1000, -500]
    prices = [100, 110, 120]
    pme_prices = [100, 105, 115]

    pme_result = calculate_pme(cashflows, prices, pme_prices)
    print("PME: {:.4f}".format(pme_result['pme']))

    verbose_result = calculate_verbose_pme(cashflows, prices, pme_prices)
    print("Verbose PME: {:.4f}, NAV PME: {:.4f}".format(verbose_result['pme'], verbose_result['nav_pme']))

    xpme_result = calculate_xpme(dates, cashflows, prices, pme_prices)
    print("xPME: {:.4f}".format(xpme_result['xpme']))

    verbose_xpme_result = calculate_verbose_xpme(dates, cashflows, prices, pme_prices)
    print("Verbose xPME: {:.4f}".format(verbose_xpme_result['xpme']))

    pricedf = pd.DataFrame({
        'date': dates,
        'price': pme_prices
    }).set_index('date')

    picked_prices = pick_prices_from_dataframe(dates, pricedf, 'price')
    print("Picked prices count: {}".format(len(picked_prices['prices'])))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
