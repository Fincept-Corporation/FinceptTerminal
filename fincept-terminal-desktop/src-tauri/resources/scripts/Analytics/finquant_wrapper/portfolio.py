import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union
import json
from finquant.portfolio import build_portfolio, Portfolio

def build_portfolio_from_tickers(
    tickers: List[str],
    start_date: str,
    end_date: Optional[str] = None,
    weights: Optional[List[float]] = None,
    market_index: Optional[str] = None,
    data_api: str = 'yfinance'
) -> Dict:
    names_dict = {ticker: weights[i] if weights else 1.0/len(tickers)
                  for i, ticker in enumerate(tickers)}

    pf_params = {
        'names': names_dict,
        'start_date': start_date,
        'data_api': data_api,
    }
    if end_date:
        pf_params['end_date'] = end_date
    if market_index:
        pf_params['market_index'] = market_index

    pf = build_portfolio(**pf_params)

    return {
        'success': True,
        'portfolio_id': id(pf),
        'num_stocks': len(tickers),
        'tickers': tickers,
        'weights': weights if weights else [1.0/len(tickers)] * len(tickers)
    }

def build_portfolio_from_data(
    data: Union[pd.DataFrame, Dict],
    weights: Optional[List[float]] = None,
    market_index_data: Optional[Union[pd.DataFrame, Dict]] = None
) -> Dict:
    if isinstance(data, dict):
        data = pd.DataFrame(data)

    if market_index_data and isinstance(market_index_data, dict):
        market_index_data = pd.DataFrame(market_index_data)

    names = data.columns.tolist()
    names_dict = {name: weights[i] if weights else 1.0/len(names)
                  for i, name in enumerate(names)}

    pf_params = {'names': names_dict, 'data': data}
    if market_index_data is not None:
        pf_params['market_index'] = market_index_data

    pf = build_portfolio(**pf_params)

    return {
        'success': True,
        'portfolio_id': id(pf),
        'num_stocks': len(names),
        'tickers': names,
        'weights': weights if weights else [1.0/len(names)] * len(names)
    }

def get_portfolio_properties(
    tickers: List[str],
    start_date: str,
    end_date: Optional[str] = None,
    weights: Optional[List[float]] = None,
    data_api: str = 'yfinance'
) -> Dict:
    result = build_portfolio_from_tickers(tickers, start_date, end_date, weights, data_api=data_api)
    if not result['success']:
        return result

    names_dict = {ticker: weights[i] if weights else 1.0/len(tickers)
                  for i, ticker in enumerate(tickers)}
    pf_params = {'names': names_dict, 'start_date': start_date, 'data_api': data_api}
    if end_date:
        pf_params['end_date'] = end_date

    pf = build_portfolio(**pf_params)

    return {
        'expected_return': float(pf.comp_expected_return()),
        'volatility': float(pf.comp_volatility()),
        'sharpe_ratio': float(pf.comp_sharpe()),
        'sortino_ratio': float(pf.comp_sortino()),
        'var': float(pf.comp_var()),
        'downside_risk': float(pf.comp_downside_risk()),
        'weights': pf.comp_weights().to_dict(),
        'tickers': tickers
    }

def calculate_portfolio_metrics(
    tickers: List[str],
    start_date: str,
    end_date: Optional[str] = None,
    weights: Optional[List[float]] = None,
    freq: int = 252,
    market_index: Optional[str] = None,
    data_api: str = 'yfinance'
) -> Dict:
    names_dict = {ticker: weights[i] if weights else 1.0/len(tickers)
                  for i, ticker in enumerate(tickers)}
    pf_params = {'names': names_dict, 'start_date': start_date, 'data_api': data_api}
    if end_date:
        pf_params['end_date'] = end_date
    if market_index:
        pf_params['market_index'] = market_index

    pf = build_portfolio(**pf_params)

    metrics = {
        'expected_return': float(pf.comp_expected_return(freq=freq)),
        'volatility': float(pf.comp_volatility(freq=freq)),
        'sharpe_ratio': float(pf.comp_sharpe()),
        'sortino_ratio': float(pf.comp_sortino()),
        'var': float(pf.comp_var()),
        'downside_risk': float(pf.comp_downside_risk(freq=freq)),
        'weights': pf.comp_weights().to_dict(),
    }

    beta = pf.comp_beta()
    if beta is not None:
        metrics['beta'] = float(beta)
        treynor = pf.comp_treynor()
        if treynor is not None:
            metrics['treynor_ratio'] = float(treynor)
        rsquared = pf.comp_rsquared()
        if rsquared is not None:
            metrics['r_squared'] = float(rsquared)

    mean_returns = pf.comp_mean_returns(freq=freq)
    metrics['mean_returns'] = mean_returns.to_dict()

    stock_volatility = pf.comp_stock_volatility(freq=freq)
    metrics['stock_volatility'] = stock_volatility.to_dict()

    return metrics

def optimize_portfolio_min_volatility(
    tickers: List[str],
    start_date: str,
    end_date: Optional[str] = None,
    verbose: bool = False,
    data_api: str = 'yfinance'
) -> Dict:
    names_dict = {ticker: 1.0/len(tickers) for ticker in tickers}
    pf_params = {'names': names_dict, 'start_date': start_date, 'data_api': data_api}
    if end_date:
        pf_params['end_date'] = end_date

    pf = build_portfolio(**pf_params)
    result_df = pf.ef_minimum_volatility(verbose=verbose)

    return {
        'expected_return': float(result_df.loc['Expected Return'].values[0]),
        'volatility': float(result_df.loc['Volatility'].values[0]),
        'sharpe_ratio': float(result_df.loc['Sharpe Ratio'].values[0]),
        'weights': {ticker: float(result_df.loc[ticker].values[0])
                   for ticker in tickers}
    }

def optimize_portfolio_max_sharpe(
    tickers: List[str],
    start_date: str,
    end_date: Optional[str] = None,
    verbose: bool = False
,
    data_api: str = 'yfinance'
) -> Dict:
    names_dict = {ticker: 1.0/len(tickers) for ticker in tickers}
    pf_params = {'names': names_dict, 'start_date': start_date, 'data_api': data_api}
    if end_date:
        pf_params['end_date'] = end_date

    pf = build_portfolio(**pf_params)
    result_df = pf.ef_maximum_sharpe_ratio(verbose=verbose)

    return {
        'expected_return': float(result_df.loc['Expected Return'].values[0]),
        'volatility': float(result_df.loc['Volatility'].values[0]),
        'sharpe_ratio': float(result_df.loc['Sharpe Ratio'].values[0]),
        'weights': {ticker: float(result_df.loc[ticker].values[0])
                   for ticker in tickers}
    }

def optimize_portfolio_efficient_return(
    tickers: List[str],
    target_return: float,
    start_date: str,
    end_date: Optional[str] = None,
    verbose: bool = False
,
    data_api: str = 'yfinance'
) -> Dict:
    names_dict = {ticker: 1.0/len(tickers) for ticker in tickers}
    pf_params = {'names': names_dict, 'start_date': start_date, 'data_api': data_api}
    if end_date:
        pf_params['end_date'] = end_date

    pf = build_portfolio(**pf_params)
    result_df = pf.ef_efficient_return(target=target_return, verbose=verbose)

    return {
        'expected_return': float(result_df.loc['Expected Return'].values[0]),
        'volatility': float(result_df.loc['Volatility'].values[0]),
        'sharpe_ratio': float(result_df.loc['Sharpe Ratio'].values[0]),
        'weights': {ticker: float(result_df.loc[ticker].values[0])
                   for ticker in tickers}
    }

def optimize_portfolio_efficient_volatility(
    tickers: List[str],
    target_volatility: float,
    start_date: str,
    end_date: Optional[str] = None,
    verbose: bool = False
,
    data_api: str = 'yfinance'
) -> Dict:
    names_dict = {ticker: 1.0/len(tickers) for ticker in tickers}
    pf_params = {'names': names_dict, 'start_date': start_date, 'data_api': data_api}
    if end_date:
        pf_params['end_date'] = end_date

    pf = build_portfolio(**pf_params)
    result_df = pf.ef_efficient_volatility(target=target_volatility, verbose=verbose)

    return {
        'expected_return': float(result_df.loc['Expected Return'].values[0]),
        'volatility': float(result_df.loc['Volatility'].values[0]),
        'sharpe_ratio': float(result_df.loc['Sharpe Ratio'].values[0]),
        'weights': {ticker: float(result_df.loc[ticker].values[0])
                   for ticker in tickers}
    }

def monte_carlo_optimization(
    tickers: List[str],
    start_date: str,
    end_date: Optional[str] = None,
    num_trials: int = 5000
,
    data_api: str = 'yfinance'
) -> Dict:
    names_dict = {ticker: 1.0/len(tickers) for ticker in tickers}
    pf_params = {'names': names_dict, 'start_date': start_date, 'data_api': data_api}
    if end_date:
        pf_params['end_date'] = end_date

    pf = build_portfolio(**pf_params)
    opt_weights, opt_results = pf.mc_optimisation(num_trials=num_trials)

    return {
        'optimal_weights': opt_weights.to_dict(),
        'min_volatility': {
            'expected_return': float(opt_results.loc['Minimum Volatility', 'Expected Return']),
            'volatility': float(opt_results.loc['Minimum Volatility', 'Volatility']),
            'sharpe_ratio': float(opt_results.loc['Minimum Volatility', 'Sharpe Ratio'])
        },
        'max_sharpe': {
            'expected_return': float(opt_results.loc['Maximum Sharpe Ratio', 'Expected Return']),
            'volatility': float(opt_results.loc['Maximum Sharpe Ratio', 'Volatility']),
            'sharpe_ratio': float(opt_results.loc['Maximum Sharpe Ratio', 'Sharpe Ratio'])
        },
        'num_trials': num_trials
    }

def main():
    print("Testing FinQuant Portfolio Wrapper")

    tickers = ['GOOG', 'AMZN', 'AAPL']
    start_date = '2020-01-01'
    end_date = '2023-12-31'

    print("\n1. Testing get_portfolio_properties...")
    try:
        result = get_portfolio_properties(tickers, start_date, end_date)
        print("Expected Return: {:.4f}".format(result['expected_return']))
        print("Volatility: {:.4f}".format(result['volatility']))
        print("Sharpe Ratio: {:.4f}".format(result['sharpe_ratio']))
        print("Test 1: PASSED")
    except Exception as e:
        print("Test 1: FAILED - {}".format(str(e)))
        return

    print("\n2. Testing optimize_portfolio_max_sharpe...")
    try:
        result = optimize_portfolio_max_sharpe(tickers, start_date, end_date)
        print("Optimal Sharpe Ratio: {:.4f}".format(result['sharpe_ratio']))
        print("Weights: {}".format(result['weights']))
        print("Test 2: PASSED")
    except Exception as e:
        print("Test 2: FAILED - {}".format(str(e)))
        return

    print("\nAll tests: PASSED")

if __name__ == "__main__":
    main()
