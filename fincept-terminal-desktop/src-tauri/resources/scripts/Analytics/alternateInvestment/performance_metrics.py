"""
Alternative Investment Performance Metrics Module
===============================================

Comprehensive performance measurement and analysis framework for alternative investments. Implements CFA Institute standard methodologies including time-weighted returns, money-weighted returns, risk-adjusted metrics, performance attribution, and fee impact analysis.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Historical market data and prices for investments
  - Cash flow data including contributions and distributions
  - Benchmark return series for comparison
  - Factor returns for multi-factor analysis
  - Fee structures and terms
  - Portfolio holdings and weights

OUTPUT:
  - Time-weighted and money-weighted return calculations
  - Risk-adjusted performance metrics (Sharpe, Sortino, Treynor, etc.)
  - Performance attribution analysis
  - Benchmark comparison and relative performance
  - Factor exposure analysis
  - Fee impact assessment and net-of-fee performance

PARAMETERS:
  - risk_free_rate: Risk-free rate for calculations - default: Config.RISK_FREE_RATE
  - benchmark_returns: Benchmark return series for comparison
  - factor_returns: Dictionary of factor return series
  - management_fee: Annual management fee rate for fee analysis
  - performance_fee: Performance fee rate
  - hurdle_rate: Hurdle rate for performance fees
  - high_water_mark: Whether high water mark applies
  - window_months: Rolling window size for performance analysis - default: 12
"""

import numpy as np
import pandas as pd
from decimal import Decimal, getcontext
from typing import List, Dict, Optional, Any, Tuple
from datetime import datetime, timedelta
import logging

from config import (
    MarketData, CashFlow, Performance, AssetClass, Constants, Config
)
from base_analytics import FinancialMath

logger = logging.getLogger(__name__)

class PerformanceAnalyzer:
    """
    Comprehensive performance analysis for alternative investments
    Implements CFA Institute standard performance measurement methodologies
    """

    def __init__(self):
        self.math = FinancialMath()
        self.config = Config()

    def calculate_time_weighted_return(self, prices: List[MarketData],
                                     cash_flows: List[CashFlow] = None) -> Dict[str, Decimal]:
        """
        Calculate Time-Weighted Return (TWR)
        CFA Standard: Geometric mean of sub-period returns
        Eliminates the effect of cash flow timing

        Args:
            prices: List of MarketData objects
            cash_flows: Optional cash flows for adjustment

        Returns:
            Dictionary with TWR metrics
        """
        if len(prices) < 2:
            return {"error": "Insufficient price data"}

        # Sort prices by timestamp
        sorted_prices = sorted(prices, key=lambda x: x.timestamp)

        # Calculate sub-period returns
        returns = []
        for i in range(1, len(sorted_prices)):
            prev_price = sorted_prices[i-1].price
            curr_price = sorted_prices[i].price
            period_return = (curr_price - prev_price) / prev_price
            returns.append(period_return)

        # Calculate geometric mean (TWR)
        if not returns:
            return {"twr": Decimal('0')}

        cumulative_return = Decimal('1')
        for ret in returns:
            cumulative_return *= (Decimal('1') + ret)

        twr = cumulative_return - Decimal('1')

        # Annualize if we have enough data
        total_days = (datetime.strptime(sorted_prices[-1].timestamp[:10], '%Y-%m-%d') -
                     datetime.strptime(sorted_prices[0].timestamp[:10], '%Y-%m-%d')).days

        if total_days > 0:
            years = Decimal(str(total_days)) / Constants.DAYS_IN_YEAR
            annualized_twr = (cumulative_return ** (Decimal('1') / years)) - Decimal('1')
        else:
            annualized_twr = twr

        return {
            "twr": twr,
            "annualized_twr": annualized_twr,
            "cumulative_return": cumulative_return - Decimal('1'),
            "number_of_periods": len(returns),
            "total_days": total_days
        }

    def calculate_money_weighted_return(self, cash_flows: List[CashFlow]) -> Dict[str, Decimal]:
        """
        Calculate Money-Weighted Return (MWR) using IRR
        CFA Standard: IRR of all cash flows including ending value
        Reflects the effect of cash flow timing

        Args:
            cash_flows: List of CashFlow objects

        Returns:
            Dictionary with MWR metrics
        """
        if not cash_flows:
            return {"error": "No cash flows provided"}

        irr = self.math.irr(cash_flows)

        if irr is None:
            return {"error": "Could not calculate IRR"}

        # Calculate other metrics
        moic = self.math.moic(cash_flows)
        dpi = self.math.dpi(cash_flows)

        return {
            "mwr_irr": irr,
            "moic": moic,
            "dpi": dpi,
            "total_cash_flows": len(cash_flows)
        }

    def calculate_risk_adjusted_returns(self, returns: List[Decimal],
                                      benchmark_returns: List[Decimal] = None) -> Dict[str, Decimal]:
        """
        Calculate comprehensive risk-adjusted return metrics
        CFA Standards: Sharpe, Treynor, Information, Sortino ratios

        Args:
            returns: Portfolio returns
            benchmark_returns: Benchmark returns for comparison

        Returns:
            Dictionary of risk-adjusted metrics
        """
        if len(returns) < 2:
            return {"error": "Insufficient return data"}

        metrics = {}

        # Basic statistics
        mean_return = sum(returns) / len(returns)
        volatility = self._calculate_volatility(returns)

        # Sharpe Ratio
        rf_rate = self.config.RISK_FREE_RATE / Constants.MONTHS_IN_YEAR  # Monthly risk-free rate
        sharpe = self.math.sharpe_ratio(returns, rf_rate)
        metrics["sharpe_ratio"] = sharpe

        # Sortino Ratio
        sortino = self.math.sortino_ratio(returns)
        metrics["sortino_ratio"] = sortino

        # Calmar Ratio (need price series for max drawdown)
        if len(returns) > 1:
            # Approximate price series from returns
            prices = [Decimal('100')]  # Start with base 100
            for ret in returns:
                prices.append(prices[-1] * (Decimal('1') + ret))

            max_dd, _, _ = self.math.maximum_drawdown(prices)
            annualized_return = mean_return * Constants.MONTHS_IN_YEAR  # Assuming monthly returns
            calmar = self.math.calmar_ratio(annualized_return, max_dd)
            metrics["calmar_ratio"] = calmar
            metrics["maximum_drawdown"] = max_dd

        # Benchmark-relative metrics
        if benchmark_returns and len(benchmark_returns) == len(returns):
            # Tracking Error
            active_returns = [r - b for r, b in zip(returns, benchmark_returns)]
            tracking_error = self._calculate_volatility(active_returns)
            metrics["tracking_error"] = tracking_error

            # Information Ratio
            mean_active_return = sum(active_returns) / len(active_returns)
            if tracking_error > 0:
                information_ratio = mean_active_return / tracking_error
                metrics["information_ratio"] = information_ratio

            # Beta calculation
            if len(returns) > 1:
                beta = self._calculate_beta(returns, benchmark_returns)
                metrics["beta"] = beta

                # Treynor Ratio
                if beta != 0:
                    treynor = (mean_return - rf_rate) / beta
                    metrics["treynor_ratio"] = treynor

        # Value at Risk
        var_95 = self.math.var_historical(returns, Decimal('0.05'))
        var_99 = self.math.var_historical(returns, Decimal('0.01'))
        metrics["var_95"] = var_95
        metrics["var_99"] = var_99

        # Conditional VaR (Expected Shortfall)
        cvar_95 = self._calculate_cvar(returns, Decimal('0.05'))
        metrics["cvar_95"] = cvar_95

        return metrics

    def performance_attribution(self, portfolio_returns: List[Decimal],
                              benchmark_returns: List[Decimal],
                              sector_weights: Dict[str, Decimal] = None) -> Dict[str, Any]:
        """
        Perform return-based performance attribution analysis
        CFA Standard: Decompose excess returns into allocation and selection effects

        Args:
            portfolio_returns: Portfolio period returns
            benchmark_returns: Benchmark period returns
            sector_weights: Optional sector weight information

        Returns:
            Attribution analysis results
        """
        if len(portfolio_returns) != len(benchmark_returns):
            return {"error": "Portfolio and benchmark return lengths must match"}

        attribution = {}

        # Calculate basic attribution metrics
        active_returns = [p - b for p, b in zip(portfolio_returns, benchmark_returns)]

        portfolio_mean = sum(portfolio_returns) / len(portfolio_returns)
        benchmark_mean = sum(benchmark_returns) / len(benchmark_returns)
        active_mean = portfolio_mean - benchmark_mean

        attribution["total_active_return"] = active_mean
        attribution["portfolio_return"] = portfolio_mean
        attribution["benchmark_return"] = benchmark_mean

        # Hit rate (percentage of periods with positive active returns)
        positive_periods = sum(1 for ar in active_returns if ar > 0)
        hit_rate = Decimal(str(positive_periods)) / Decimal(str(len(active_returns)))
        attribution["hit_rate"] = hit_rate

        # Consistency metrics
        active_volatility = self._calculate_volatility(active_returns)
        attribution["active_volatility"] = active_volatility

        if active_volatility > 0:
            information_ratio = active_mean / active_volatility
            attribution["information_ratio"] = information_ratio

        return attribution

    def calculate_downside_metrics(self, returns: List[Decimal],
                                 target_return: Decimal = Decimal('0')) -> Dict[str, Decimal]:
        """
        Calculate comprehensive downside risk metrics
        CFA Standards: Downside deviation, downside beta, etc.

        Args:
            returns: Period returns
            target_return: Target or MAR (Minimum Acceptable Return)

        Returns:
            Dictionary of downside metrics
        """
        downside_metrics = {}

        # Downside deviation
        downside_returns = [min(r - target_return, Decimal('0')) for r in returns]
        downside_variance = sum(dr ** 2 for dr in downside_returns) / len(returns)
        downside_deviation = downside_variance.sqrt()
        downside_metrics["downside_deviation"] = downside_deviation

        # Sortino ratio
        mean_return = sum(returns) / len(returns)
        if downside_deviation > 0:
            sortino = (mean_return - target_return) / downside_deviation
            downside_metrics["sortino_ratio"] = sortino

        # Downside frequency
        negative_periods = sum(1 for r in returns if r < target_return)
        downside_frequency = Decimal(str(negative_periods)) / Decimal(str(len(returns)))
        downside_metrics["downside_frequency"] = downside_frequency

        # Average downside return
        negative_returns = [r for r in returns if r < target_return]
        if negative_returns:
            avg_downside = sum(negative_returns) / len(negative_returns)
            downside_metrics["average_downside_return"] = avg_downside

        return downside_metrics

    def rolling_performance(self, prices: List[MarketData], window_months: int = 12) -> List[Dict]:
        """
        Calculate rolling performance metrics

        Args:
            prices: List of MarketData objects
            window_months: Rolling window size in months

        Returns:
            List of rolling performance dictionaries
        """
        if len(prices) < window_months:
            return []

        rolling_results = []
        sorted_prices = sorted(prices, key=lambda x: x.timestamp)

        for i in range(window_months, len(sorted_prices)):
            window_prices = sorted_prices[i-window_months:i+1]

            # Calculate returns for the window
            window_returns = []
            for j in range(1, len(window_prices)):
                ret = (window_prices[j].price - window_prices[j-1].price) / window_prices[j-1].price
                window_returns.append(ret)

            if window_returns:
                period_return = (window_prices[-1].price - window_prices[0].price) / window_prices[0].price
                volatility = self._calculate_volatility(window_returns)
                sharpe = self.math.sharpe_ratio(window_returns)

                rolling_results.append({
                    "end_date": window_prices[-1].timestamp,
                    "period_return": float(period_return),
                    "annualized_return": float(period_return * Constants.MONTHS_IN_YEAR / window_months),
                    "volatility": float(volatility),
                    "sharpe_ratio": float(sharpe)
                })

        return rolling_results

    def calculate_factor_exposures(self, portfolio_returns: List[Decimal],
                                 factor_returns: Dict[str, List[Decimal]]) -> Dict[str, Decimal]:
        """
        Calculate factor exposures using multiple regression
        CFA Standard: Multi-factor model analysis

        Args:
            portfolio_returns: Portfolio returns
            factor_returns: Dictionary of factor name -> factor returns

        Returns:
            Dictionary of factor loadings (betas)
        """
        if not factor_returns or len(portfolio_returns) < 10:
            return {}

        # Ensure all factor return series have same length as portfolio
        valid_factors = {}
        for factor_name, returns in factor_returns.items():
            if len(returns) == len(portfolio_returns):
                valid_factors[factor_name] = returns

        if not valid_factors:
            return {}

        try:
            # Convert to numpy arrays for regression
            y = np.array([float(r) for r in portfolio_returns])
            X = np.array([[float(valid_factors[factor][i]) for factor in valid_factors.keys()]
                         for i in range(len(portfolio_returns))])

            # Add intercept term
            X = np.column_stack([np.ones(len(y)), X])

            # Ordinary least squares regression
            beta = np.linalg.lstsq(X, y, rcond=None)[0]

            exposures = {"alpha": Decimal(str(beta[0]))}
            for i, factor_name in enumerate(valid_factors.keys()):
                exposures[f"{factor_name}_beta"] = Decimal(str(beta[i+1]))

            # Calculate R-squared
            y_pred = X @ beta
            ss_res = np.sum((y - y_pred) ** 2)
            ss_tot = np.sum((y - np.mean(y)) ** 2)
            r_squared = 1 - (ss_res / ss_tot) if ss_tot != 0 else 0
            exposures["r_squared"] = Decimal(str(r_squared))

            return exposures

        except Exception as e:
            logger.error(f"Error calculating factor exposures: {str(e)}")
            return {}

    def benchmark_analysis(self, portfolio_returns: List[Decimal],
                          benchmark_returns: List[Decimal]) -> Dict[str, Any]:
        """
        Comprehensive benchmark analysis
        CFA Standards: Up/down capture, batting average, etc.

        Args:
            portfolio_returns: Portfolio returns
            benchmark_returns: Benchmark returns

        Returns:
            Dictionary of benchmark analysis metrics
        """
        if len(portfolio_returns) != len(benchmark_returns):
            return {"error": "Return series length mismatch"}

        analysis = {}

        # Up/Down Capture Ratios
        up_periods = [(p, b) for p, b in zip(portfolio_returns, benchmark_returns) if b > 0]
        down_periods = [(p, b) for p, b in zip(portfolio_returns, benchmark_returns) if b < 0]

        if up_periods:
            up_portfolio = sum(p for p, b in up_periods) / len(up_periods)
            up_benchmark = sum(b for p, b in up_periods) / len(up_periods)
            up_capture = up_portfolio / up_benchmark if up_benchmark != 0 else Decimal('0')
            analysis["up_capture_ratio"] = up_capture

        if down_periods:
            down_portfolio = sum(p for p, b in down_periods) / len(down_periods)
            down_benchmark = sum(b for p, b in down_periods) / len(down_periods)
            down_capture = down_portfolio / down_benchmark if down_benchmark != 0 else Decimal('0')
            analysis["down_capture_ratio"] = down_capture

        # Batting Average (percentage of periods outperforming benchmark)
        outperformance_periods = sum(1 for p, b in zip(portfolio_returns, benchmark_returns) if p > b)
        batting_average = Decimal(str(outperformance_periods)) / Decimal(str(len(portfolio_returns)))
        analysis["batting_average"] = batting_average

        # Beta and correlation
        beta = self._calculate_beta(portfolio_returns, benchmark_returns)
        correlation = self._calculate_correlation(portfolio_returns, benchmark_returns)
        analysis["beta"] = beta
        analysis["correlation"] = correlation

        return analysis

    def performance_persistence(self, returns_by_period: List[List[Decimal]]) -> Dict[str, Any]:
        """
        Analyze performance persistence across periods

        Args:
            returns_by_period: List of return lists for different periods

        Returns:
            Persistence analysis metrics
        """
        if len(returns_by_period) < 2:
            return {"error": "Need at least 2 periods for persistence analysis"}

        persistence = {}

        # Calculate rankings for each period
        period_rankings = []
        for period_returns in returns_by_period:
            if not period_returns:
                continue
            sorted_returns = sorted(period_returns, reverse=True)
            rankings = []
            for ret in period_returns:
                rank = sorted_returns.index(ret) + 1
                rankings.append(rank)
            period_rankings.append(rankings)

        if len(period_rankings) < 2:
            return {"error": "Insufficient valid periods"}

        # Calculate rank correlation between consecutive periods
        correlations = []
        for i in range(len(period_rankings) - 1):
            corr = self._calculate_rank_correlation(period_rankings[i], period_rankings[i+1])
            correlations.append(corr)

        persistence["rank_correlations"] = correlations
        persistence["average_rank_correlation"] = sum(correlations) / len(correlations) if correlations else Decimal('0')

        return persistence

    def _calculate_volatility(self, returns: List[Decimal]) -> Decimal:
        """Calculate standard deviation of returns"""
        if len(returns) < 2:
            return Decimal('0')

        mean_return = sum(returns) / len(returns)
        variance = sum((r - mean_return) ** 2 for r in returns) / (len(returns) - 1)
        return variance.sqrt()

    def _calculate_beta(self, portfolio_returns: List[Decimal],
                       benchmark_returns: List[Decimal]) -> Decimal:
        """Calculate beta (systematic risk measure)"""
        if len(portfolio_returns) != len(benchmark_returns) or len(portfolio_returns) < 2:
            return Decimal('1')

        # Calculate covariance and benchmark variance
        port_mean = sum(portfolio_returns) / len(portfolio_returns)
        bench_mean = sum(benchmark_returns) / len(benchmark_returns)

        covariance = sum((p - port_mean) * (b - bench_mean)
                        for p, b in zip(portfolio_returns, benchmark_returns)) / (len(portfolio_returns) - 1)

        bench_variance = sum((b - bench_mean) ** 2 for b in benchmark_returns) / (len(benchmark_returns) - 1)

        if bench_variance == 0:
            return Decimal('1')

        return Decimal(str(covariance)) / Decimal(str(bench_variance))

    def _calculate_correlation(self, x: List[Decimal], y: List[Decimal]) -> Decimal:
        """Calculate correlation coefficient"""
        if len(x) != len(y) or len(x) < 2:
            return Decimal('0')

        x_mean = sum(x) / len(x)
        y_mean = sum(y) / len(y)

        numerator = sum((xi - x_mean) * (yi - y_mean) for xi, yi in zip(x, y))
        x_sq_sum = sum((xi - x_mean) ** 2 for xi in x)
        y_sq_sum = sum((yi - y_mean) ** 2 for yi in y)

        denominator = (x_sq_sum * y_sq_sum).sqrt()

        if denominator == 0:
            return Decimal('0')

        return numerator / denominator

    def _calculate_rank_correlation(self, ranks1: List[int], ranks2: List[int]) -> Decimal:
        """Calculate Spearman rank correlation"""
        if len(ranks1) != len(ranks2) or len(ranks1) < 2:
            return Decimal('0')

        n = len(ranks1)
        d_squared_sum = sum((r1 - r2) ** 2 for r1, r2 in zip(ranks1, ranks2))

        correlation = Decimal('1') - (Decimal('6') * Decimal(str(d_squared_sum))) / (Decimal(str(n)) * (Decimal(str(n))**2 - Decimal('1')))

        return correlation

    def _calculate_cvar(self, returns: List[Decimal], confidence_level: Decimal) -> Decimal:
        """Calculate Conditional Value at Risk (Expected Shortfall)"""
        if not returns:
            return Decimal('0')

        sorted_returns = sorted(returns)
        var_index = int(len(sorted_returns) * confidence_level)

        if var_index >= len(sorted_returns):
            var_index = len(sorted_returns) - 1

        # Average of returns at or below VaR level
        tail_returns = sorted_returns[:var_index + 1]
        if not tail_returns:
            return Decimal('0')

        cvar = sum(tail_returns) / len(tail_returns)
        return abs(cvar)

class FeeAnalyzer:
    """
    Analyze fee structures and their impact on performance
    CFA Standards: Fee transparency and impact analysis
    """

    def __init__(self):
        self.config = Config()

    def calculate_fee_impact(self, gross_returns: List[Decimal],
                           management_fee: Decimal,
                           performance_fee: Decimal = None,
                           hurdle_rate: Decimal = None,
                           high_water_mark: bool = True) -> Dict[str, Any]:
        """
        Calculate the impact of fees on investment returns

        Args:
            gross_returns: Gross returns before fees
            management_fee: Annual management fee rate
            performance_fee: Performance fee rate (if applicable)
            hurdle_rate: Hurdle rate for performance fees
            high_water_mark: Whether high water mark applies

        Returns:
            Fee impact analysis
        """
        if not gross_returns:
            return {"error": "No returns provided"}

        net_returns = []
        cumulative_nav = Decimal('100')  # Start with 100 base value
        high_water_mark_value = cumulative_nav if high_water_mark else None

        periods_per_year = 12  # Assume monthly returns
        monthly_mgmt_fee = management_fee / periods_per_year

        total_mgmt_fees = Decimal('0')
        total_perf_fees = Decimal('0')

        for gross_return in gross_returns:
            # Apply gross return
            period_nav = cumulative_nav * (Decimal('1') + gross_return)

            # Calculate management fee
            mgmt_fee_amount = cumulative_nav * monthly_mgmt_fee
            total_mgmt_fees += mgmt_fee_amount

            # Calculate performance fee
            perf_fee_amount = Decimal('0')
            if performance_fee and performance_fee > 0:
                if hurdle_rate:
                    monthly_hurdle = hurdle_rate / periods_per_year
                    hurdle_return = cumulative_nav * monthly_hurdle
                else:
                    hurdle_return = Decimal('0')

                excess_return = max(Decimal('0'), (period_nav - cumulative_nav) - hurdle_return)

                if high_water_mark and high_water_mark_value:
                    if period_nav > high_water_mark_value:
                        perf_fee_amount = excess_return * performance_fee
                        high_water_mark_value = period_nav
                else:
                    perf_fee_amount = excess_return * performance_fee

                total_perf_fees += perf_fee_amount

            # Calculate net return after fees
            net_nav = period_nav - mgmt_fee_amount - perf_fee_amount
            net_return = (net_nav - cumulative_nav) / cumulative_nav
            net_returns.append(net_return)

            cumulative_nav = net_nav

        # Calculate summary statistics
        gross_cumulative = Decimal('1')
        net_cumulative = Decimal('1')

        for gross_ret, net_ret in zip(gross_returns, net_returns):
            gross_cumulative *= (Decimal('1') + gross_ret)
            net_cumulative *= (Decimal('1') + net_ret)

        fee_drag = (gross_cumulative - net_cumulative) / gross_cumulative

        return {
            "gross_cumulative_return": gross_cumulative - Decimal('1'),
            "net_cumulative_return": net_cumulative - Decimal('1'),
            "total_fee_drag": fee_drag,
            "total_management_fees": total_mgmt_fees,
            "total_performance_fees": total_perf_fees,
            "total_fees": total_mgmt_fees + total_perf_fees,
            "net_returns": net_returns,
            "fee_ratio": (total_mgmt_fees + total_perf_fees) / (cumulative_nav * len(gross_returns))
        }

# Export main components
__all__ = ['PerformanceAnalyzer', 'FeeAnalyzer']