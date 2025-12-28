import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Tuple, Union
import json
from finquant.efficient_frontier import EfficientFrontier

def compute_efficient_frontier(
    mean_returns: Union[pd.Series, Dict],
    cov_matrix: Union[pd.DataFrame, Dict],
    targets: Optional[List[float]] = None
) -> Dict:
    if isinstance(mean_returns, dict):
        mean_returns = pd.Series(mean_returns)
    if isinstance(cov_matrix, dict):
        cov_matrix = pd.DataFrame(cov_matrix)

    ef = EfficientFrontier(mean_returns, cov_matrix)
    frontier = ef.efficient_frontier(targets=targets)

    return {
        'frontier_points': frontier.tolist(),
        'num_points': len(frontier),
        'assets': mean_returns.index.tolist()
    }

def find_minimum_volatility_portfolio(
    mean_returns: Union[pd.Series, Dict],
    cov_matrix: Union[pd.DataFrame, Dict],
    save_weights: bool = True
) -> Dict:
    if isinstance(mean_returns, dict):
        mean_returns = pd.Series(mean_returns)
    if isinstance(cov_matrix, dict):
        cov_matrix = pd.DataFrame(cov_matrix)

    ef = EfficientFrontier(mean_returns, cov_matrix)
    result = ef.minimum_volatility(save_weights=save_weights)

    if save_weights and isinstance(result, pd.DataFrame):
        weights_dict = {asset: float(result.loc[asset, 'Allocation'])
                       for asset in result.index.tolist()}
        exp_ret, vol, sharpe = ef.properties(verbose=False)
        return {
            'expected_return': float(exp_ret),
            'volatility': float(vol),
            'sharpe_ratio': float(sharpe),
            'weights': weights_dict
        }
    else:
        return {
            'weights': result.tolist() if isinstance(result, np.ndarray) else result
        }

def find_maximum_sharpe_portfolio(
    mean_returns: Union[pd.Series, Dict],
    cov_matrix: Union[pd.DataFrame, Dict],
    save_weights: bool = True
) -> Dict:
    if isinstance(mean_returns, dict):
        mean_returns = pd.Series(mean_returns)
    if isinstance(cov_matrix, dict):
        cov_matrix = pd.DataFrame(cov_matrix)

    ef = EfficientFrontier(mean_returns, cov_matrix)
    result = ef.maximum_sharpe_ratio(save_weights=save_weights)

    if save_weights and isinstance(result, pd.DataFrame):
        weights_dict = {asset: float(result.loc[asset, 'Allocation'])
                       for asset in result.index.tolist()}
        exp_ret, vol, sharpe = ef.properties(verbose=False)
        return {
            'expected_return': float(exp_ret),
            'volatility': float(vol),
            'sharpe_ratio': float(sharpe),
            'weights': weights_dict
        }
    else:
        return {
            'weights': result.tolist() if isinstance(result, np.ndarray) else result
        }

def find_efficient_return_portfolio(
    mean_returns: Union[pd.Series, Dict],
    cov_matrix: Union[pd.DataFrame, Dict],
    target_return: float,
    save_weights: bool = True
) -> Dict:
    if isinstance(mean_returns, dict):
        mean_returns = pd.Series(mean_returns)
    if isinstance(cov_matrix, dict):
        cov_matrix = pd.DataFrame(cov_matrix)

    ef = EfficientFrontier(mean_returns, cov_matrix)
    result = ef.efficient_return(target=target_return, save_weights=save_weights)

    if save_weights and isinstance(result, pd.DataFrame):
        weights_dict = {asset: float(result.loc[asset, 'Allocation'])
                       for asset in result.index.tolist()}
        exp_ret, vol, sharpe = ef.properties(verbose=False)
        return {
            'expected_return': float(exp_ret),
            'volatility': float(vol),
            'sharpe_ratio': float(sharpe),
            'weights': weights_dict
        }
    else:
        return {
            'weights': result.tolist() if isinstance(result, np.ndarray) else result
        }

def find_efficient_volatility_portfolio(
    mean_returns: Union[pd.Series, Dict],
    cov_matrix: Union[pd.DataFrame, Dict],
    target_volatility: float
) -> Dict:
    if isinstance(mean_returns, dict):
        mean_returns = pd.Series(mean_returns)
    if isinstance(cov_matrix, dict):
        cov_matrix = pd.DataFrame(cov_matrix)

    ef = EfficientFrontier(mean_returns, cov_matrix)
    result = ef.efficient_volatility(target=target_volatility)

    weights_dict = {asset: float(result.loc[asset, 'Allocation'])
                   for asset in result.index.tolist()}
    exp_ret, vol, sharpe = ef.properties(verbose=False)
    return {
        'expected_return': float(exp_ret),
        'volatility': float(vol),
        'sharpe_ratio': float(sharpe),
        'weights': weights_dict
    }

def plot_efficient_frontier(
    mean_returns: Union[pd.Series, Dict],
    cov_matrix: Union[pd.DataFrame, Dict],
    targets: Optional[List[float]] = None
) -> Dict:
    if isinstance(mean_returns, dict):
        mean_returns = pd.Series(mean_returns)
    if isinstance(cov_matrix, dict):
        cov_matrix = pd.DataFrame(cov_matrix)

    ef = EfficientFrontier(mean_returns, cov_matrix)
    ef.plot_efrontier()

    return {
        'success': True,
        'message': 'Efficient frontier plot generated'
    }

def main():
    print("Testing FinQuant Optimization Wrapper")

    mean_returns = pd.Series({
        'GOOG': 0.25,
        'AMZN': 0.30,
        'AAPL': 0.20
    })

    cov_matrix = pd.DataFrame({
        'GOOG': [0.04, 0.02, 0.01],
        'AMZN': [0.02, 0.05, 0.015],
        'AAPL': [0.01, 0.015, 0.03]
    }, index=['GOOG', 'AMZN', 'AAPL'])

    print("\n1. Testing find_minimum_volatility_portfolio...")
    try:
        result = find_minimum_volatility_portfolio(mean_returns, cov_matrix)
        print("Volatility: {:.4f}".format(result['volatility']))
        print("Sharpe Ratio: {:.4f}".format(result['sharpe_ratio']))
        print("Weights: {}".format(result['weights']))
        print("Test 1: PASSED")
    except Exception as e:
        print("Test 1: FAILED - {}".format(str(e)))
        return

    print("\n2. Testing find_maximum_sharpe_portfolio...")
    try:
        result = find_maximum_sharpe_portfolio(mean_returns, cov_matrix)
        print("Sharpe Ratio: {:.4f}".format(result['sharpe_ratio']))
        print("Weights: {}".format(result['weights']))
        print("Test 2: PASSED")
    except Exception as e:
        print("Test 2: FAILED - {}".format(str(e)))
        return

    print("\n3. Testing find_efficient_return_portfolio...")
    try:
        result = find_efficient_return_portfolio(mean_returns, cov_matrix, target_return=0.25)
        print("Expected Return: {:.4f}".format(result['expected_return']))
        print("Volatility: {:.4f}".format(result['volatility']))
        print("Test 3: PASSED")
    except Exception as e:
        print("Test 3: FAILED - {}".format(str(e)))
        return

    print("\nAll tests: PASSED")

if __name__ == "__main__":
    main()
