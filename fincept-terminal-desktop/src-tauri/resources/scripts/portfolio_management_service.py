#!/usr/bin/env python
"""
Comprehensive Portfolio Management Service
Integrates risk management, behavioral finance, ETF analytics, and portfolio planning
"""

import sys
import json
import numpy as np
import pandas as pd
from typing import Dict, List, Optional

# Add Analytics path
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'Analytics'))

try:
    from portfolioManagement.risk_management import (
        calculate_var, calculate_cvar, calculate_portfolio_beta,
        calculate_tracking_error, calculate_downside_deviation
    )
    from portfolioManagement.portfolio_planning import (
        asset_allocation_by_age, calculate_required_savings,
        retirement_income_planning, monte_carlo_retirement
    )
    IMPORTS_AVAILABLE = True
except ImportError as e:
    IMPORTS_AVAILABLE = False
    IMPORT_ERROR = str(e)


def calculate_risk_metrics(returns_data: Dict[str, List[float]],
                           weights: Optional[List[float]] = None,
                           confidence_levels: List[float] = [0.95, 0.99]) -> Dict:
    """Calculate comprehensive risk metrics"""

    df = pd.DataFrame(returns_data)
    returns = df.values

    if weights is None:
        weights = np.ones(len(df.columns)) / len(df.columns)
    else:
        weights = np.array(weights)

    portfolio_returns = np.dot(returns, weights)

    # VaR calculations
    var_metrics = {}
    for conf in confidence_levels:
        var = np.percentile(portfolio_returns, (1 - conf) * 100)
        cvar = portfolio_returns[portfolio_returns <= var].mean()
        var_metrics[f'var_{int(conf*100)}'] = float(var)
        var_metrics[f'cvar_{int(conf*100)}'] = float(cvar)

    # Volatility metrics
    volatility = np.std(portfolio_returns) * np.sqrt(252)
    downside_returns = portfolio_returns[portfolio_returns < 0]
    downside_vol = np.std(downside_returns) * np.sqrt(252) if len(downside_returns) > 0 else 0

    # Drawdown
    cumulative = np.cumprod(1 + portfolio_returns)
    running_max = np.maximum.accumulate(cumulative)
    drawdown = (cumulative - running_max) / running_max
    max_drawdown = np.min(drawdown)

    return {
        **var_metrics,
        'volatility': float(volatility),
        'downside_volatility': float(downside_vol),
        'max_drawdown': float(max_drawdown),
        'skewness': float(pd.Series(portfolio_returns).skew()),
        'kurtosis': float(pd.Series(portfolio_returns).kurtosis())
    }


def asset_allocation_plan(age: int, risk_tolerance: str, years_to_retirement: int) -> Dict:
    """Generate strategic asset allocation"""

    # Age-based allocation
    equity_pct = max(10, 110 - age)

    # Risk tolerance adjustment
    risk_adjustments = {
        'conservative': -20,
        'moderate': 0,
        'aggressive': 15
    }
    equity_pct += risk_adjustments.get(risk_tolerance.lower(), 0)
    equity_pct = max(10, min(90, equity_pct))

    # Allocation breakdown
    allocation = {
        'equities': {
            'domestic': equity_pct * 0.6,
            'international': equity_pct * 0.3,
            'emerging_markets': equity_pct * 0.1
        },
        'fixed_income': {
            'bonds': (100 - equity_pct) * 0.7,
            'tips': (100 - equity_pct) * 0.2,
            'cash': (100 - equity_pct) * 0.1
        },
        'alternatives': 0 if years_to_retirement < 5 else 10,
        'summary': {
            'total_equity': equity_pct,
            'total_fixed_income': 100 - equity_pct,
            'risk_level': risk_tolerance
        }
    }

    return allocation


def retirement_planning(current_age: int, retirement_age: int, current_savings: float,
                       annual_contribution: float, expected_return: float = 0.07,
                       inflation: float = 0.03) -> Dict:
    """Comprehensive retirement planning"""

    years_to_retirement = retirement_age - current_age

    # Future value calculation
    fv = current_savings * ((1 + expected_return) ** years_to_retirement)

    # Annual contribution growth
    if expected_return != 0:
        contrib_fv = annual_contribution * (((1 + expected_return) ** years_to_retirement - 1) / expected_return)
    else:
        contrib_fv = annual_contribution * years_to_retirement

    total_savings = fv + contrib_fv

    # Inflation-adjusted
    real_value = total_savings / ((1 + inflation) ** years_to_retirement)

    # Withdrawal calculations (4% rule)
    safe_withdrawal = total_savings * 0.04

    return {
        'years_to_retirement': years_to_retirement,
        'projected_savings': float(total_savings),
        'inflation_adjusted_value': float(real_value),
        'annual_income_4pct_rule': float(safe_withdrawal),
        'monthly_income': float(safe_withdrawal / 12),
        'assumptions': {
            'expected_return': expected_return,
            'inflation_rate': inflation,
            'withdrawal_rate': 0.04
        }
    }


def behavioral_bias_check(portfolio_data: Dict) -> Dict:
    """Detect behavioral finance biases"""

    biases_detected = []

    # Concentration risk (overconfidence)
    if 'weights' in portfolio_data:
        weights = np.array(portfolio_data['weights'])
        max_weight = np.max(weights)
        if max_weight > 0.3:
            biases_detected.append({
                'bias': 'Overconcentration',
                'severity': 'High' if max_weight > 0.5 else 'Medium',
                'description': f'Single position represents {max_weight*100:.1f}% of portfolio'
            })

    # Home bias check (if symbols provided)
    if 'symbols' in portfolio_data:
        us_count = sum(1 for s in portfolio_data['symbols'] if not any(x in s for x in ['.L', '.T', '.HK']))
        if us_count / len(portfolio_data['symbols']) > 0.8:
            biases_detected.append({
                'bias': 'Home Country Bias',
                'severity': 'Medium',
                'description': 'Portfolio heavily concentrated in domestic market'
            })

    # Disposition effect (if returns provided)
    if 'returns' in portfolio_data:
        returns = np.array(portfolio_data['returns'])
        if np.mean(returns) < 0:
            biases_detected.append({
                'bias': 'Disposition Effect',
                'severity': 'Medium',
                'description': 'Holding onto losing positions'
            })

    return {
        'biases_detected': biases_detected,
        'bias_count': len(biases_detected),
        'overall_score': 'Good' if len(biases_detected) == 0 else 'Needs Attention'
    }


def etf_analysis(symbols: List[str], expense_ratios: List[float]) -> Dict:
    """Analyze ETF costs and efficiency"""

    avg_expense_ratio = np.mean(expense_ratios)

    # Cost impact over time
    cost_impact_10y = (1 - (1 - avg_expense_ratio) ** 10) * 100

    # Categorize
    category = 'Low Cost' if avg_expense_ratio < 0.002 else 'Medium Cost' if avg_expense_ratio < 0.005 else 'High Cost'

    return {
        'average_expense_ratio': float(avg_expense_ratio),
        'total_annual_cost_pct': float(avg_expense_ratio * 100),
        'cost_impact_10_years': float(cost_impact_10y),
        'cost_category': category,
        'etfs_analyzed': len(symbols),
        'lowest_expense_ratio': float(np.min(expense_ratios)),
        'highest_expense_ratio': float(np.max(expense_ratios))
    }


def main():
    """CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({"error": "No command specified"}))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "risk_metrics":
            data = json.loads(sys.argv[2])
            weights = json.loads(sys.argv[3]) if len(sys.argv) > 3 else None
            result = calculate_risk_metrics(data, weights)
            print(json.dumps(result))

        elif command == "asset_allocation":
            age = int(sys.argv[2])
            risk_tolerance = sys.argv[3]
            years_to_retirement = int(sys.argv[4])
            result = asset_allocation_plan(age, risk_tolerance, years_to_retirement)
            print(json.dumps(result))

        elif command == "retirement_planning":
            current_age = int(sys.argv[2])
            retirement_age = int(sys.argv[3])
            current_savings = float(sys.argv[4])
            annual_contribution = float(sys.argv[5])
            result = retirement_planning(current_age, retirement_age, current_savings, annual_contribution)
            print(json.dumps(result))

        elif command == "behavioral_analysis":
            data = json.loads(sys.argv[2])
            result = behavioral_bias_check(data)
            print(json.dumps(result))

        elif command == "etf_analysis":
            symbols = json.loads(sys.argv[2])
            expense_ratios = json.loads(sys.argv[3])
            result = etf_analysis(symbols, expense_ratios)
            print(json.dumps(result))

        else:
            print(json.dumps({"error": f"Unknown command: {command}"}))
            sys.exit(1)

    except Exception as e:
        print(json.dumps({"error": str(e)}))
        sys.exit(1)


if __name__ == "__main__":
    main()
