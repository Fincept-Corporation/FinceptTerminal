"""
Alternative Investments Base Analytics Module
============================================

Core financial mathematics foundation and abstract base classes for alternative
investment analytics. Provides essential calculations including IRR, NPV, Sharpe
ratios, VaR, and performance metrics compliant with CFA Institute standards
for alternative investment analysis and portfolio management.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Cash flow series for IRR/NPV calculations
  - Historical return data for performance metrics
  - Market data for benchmarking and risk calculations
  - Asset parameters for valuation models
  - Time series data with datetime indexing

OUTPUT:
  - Financial metrics (IRR, NPV, ROI, MOIC, DPI)
  - Risk-adjusted performance measures (Sharpe, Sortino, Calmar ratios)
  - Value-at-Risk and Conditional VaR calculations
  - Performance attribution and benchmark comparisons
  - Portfolio level analytics and aggregations

PARAMETERS:
  - risk_free_rate: Risk-free rate for calculations (default: 0.02)
  - confidence_level: VaR confidence level (default: 0.95)
  - benchmark_period: Benchmark data period in years (default: 3)
  - compounding_frequency: Compounding frequency (default: 'annual')
  - decimal_precision: Decimal precision for calculations (default: 8)
  - currency: Base currency for calculations (default: 'USD')
"""

import numpy as np
import pandas as pd
from decimal import Decimal, getcontext
from typing import List, Optional, Dict, Any, Tuple, Union
from datetime import datetime, timedelta
from abc import ABC, abstractmethod
import logging

from config import (
    MarketData, CashFlow, Performance, AssetParameters, AssetClass,
    Constants, Config, ValidationRules
)

logger = logging.getLogger(__name__)


class FinancialMath:
    """Core financial mathematics functions following CFA standards"""

    @staticmethod
    def irr(cash_flows: List[CashFlow], guess: Decimal = Decimal('0.10')) -> Optional[Decimal]:
        """
        Calculate Internal Rate of Return using Newton-Raphson method
        CFA Standard: IRR is the discount rate that makes NPV = 0

        Args:
            cash_flows: List of CashFlow objects
            guess: Initial guess for IRR

        Returns:
            IRR as decimal (e.g., 0.15 for 15%)
        """
        if not cash_flows:
            return None

        # Sort cash flows by date
        sorted_cfs = sorted(cash_flows, key=lambda x: x.date)

        # Convert to numpy arrays for calculation
        dates = [datetime.strptime(cf.date, '%Y-%m-%d') for cf in sorted_cfs]
        amounts = [float(cf.amount) for cf in sorted_cfs]

        # Calculate days from first cash flow
        base_date = dates[0]
        days = [(d - base_date).days for d in dates]

        def npv(rate):
            return sum(amount / (1 + rate) ** (day / 365.25) for amount, day in zip(amounts, days))

        def npv_derivative(rate):
            return sum(-amount * (day / 365.25) / (1 + rate) ** ((day / 365.25) + 1)
                       for amount, day in zip(amounts, days))

        rate = float(guess)
        for _ in range(Config.PE_IRR_MAX_ITERATIONS):
            npv_val = npv(rate)
            if abs(npv_val) < float(Config.PE_IRR_TOLERANCE):
                return Decimal(str(rate))

            npv_deriv = npv_derivative(rate)
            if abs(npv_deriv) < 1e-12:
                break

            rate = rate - npv_val / npv_deriv

        return None  # Convergence failed

    @staticmethod
    def npv(cash_flows: List[CashFlow], discount_rate: Decimal) -> Decimal:
        """
        Calculate Net Present Value
        CFA Standard: NPV = Σ(CF_t / (1+r)^t)

        Args:
            cash_flows: List of CashFlow objects
            discount_rate: Discount rate as decimal

        Returns:
            NPV value
        """
        if not cash_flows:
            return Decimal('0')

        sorted_cfs = sorted(cash_flows, key=lambda x: x.date)
        base_date = datetime.strptime(sorted_cfs[0].date, '%Y-%m-%d')

        npv_value = Decimal('0')
        for cf in sorted_cfs:
            cf_date = datetime.strptime(cf.date, '%Y-%m-%d')
            years = Decimal(str((cf_date - base_date).days)) / Constants.DAYS_IN_YEAR
            present_value = cf.amount / ((Decimal('1') + discount_rate) ** years)
            npv_value += present_value

        return npv_value

    @staticmethod
    def moic(cash_flows: List[CashFlow]) -> Optional[Decimal]:
        """
        Calculate Multiple of Invested Capital
        CFA Standard: MOIC = Total Distributions / Total Contributions

        Args:
            cash_flows: List of CashFlow objects

        Returns:
            MOIC as decimal multiple
        """
        total_invested = Decimal('0')
        total_distributed = Decimal('0')

        for cf in cash_flows:
            if cf.amount < 0:  # Investment/contribution
                total_invested += abs(cf.amount)
            elif cf.amount > 0:  # Distribution
                total_distributed += cf.amount

        if total_invested == 0:
            return None

        return total_distributed / total_invested

    @staticmethod
    def dpi(cash_flows: List[CashFlow]) -> Decimal:
        """
        Calculate Distributions to Paid-In Capital
        CFA Standard: DPI = Cumulative Distributions / Paid-In Capital

        Args:
            cash_flows: List of CashFlow objects

        Returns:
            DPI ratio
        """
        total_paid_in = Decimal('0')
        total_distributions = Decimal('0')

        for cf in cash_flows:
            if cf.cf_type in ['capital_call', 'investment'] or cf.amount < 0:
                total_paid_in += abs(cf.amount)
            elif cf.cf_type == 'distribution' or cf.amount > 0:
                total_distributions += cf.amount

        if total_paid_in == 0:
            return Decimal('0')

        return total_distributions / total_paid_in

    @staticmethod
    def rvpi(cash_flows: List[CashFlow], current_nav: Decimal) -> Decimal:
        """
        Calculate Residual Value to Paid-In Capital
        CFA Standard: RVPI = Net Asset Value / Paid-In Capital

        Args:
            cash_flows: List of CashFlow objects
            current_nav: Current Net Asset Value

        Returns:
            RVPI ratio
        """
        total_paid_in = Decimal('0')

        for cf in cash_flows:
            if cf.cf_type in ['capital_call', 'investment'] or cf.amount < 0:
                total_paid_in += abs(cf.amount)

        if total_paid_in == 0:
            return Decimal('0')

        return current_nav / total_paid_in

    @staticmethod
    def sharpe_ratio(returns: List[Decimal], risk_free_rate: Decimal = None) -> Decimal:
        """
        Calculate Sharpe Ratio
        CFA Standard: (Portfolio Return - Risk-Free Rate) / Portfolio Standard Deviation

        Args:
            returns: List of period returns
            risk_free_rate: Risk-free rate for the period

        Returns:
            Sharpe ratio
        """
        if len(returns) < 2:
            return Decimal('0')

        if risk_free_rate is None:
            risk_free_rate = Config.RISK_FREE_RATE / Constants.MONTHS_IN_YEAR  # Monthly rate

        excess_returns = [r - risk_free_rate for r in returns]
        mean_excess = sum(excess_returns) / len(excess_returns)

        if len(excess_returns) == 1:
            return Decimal('0')

        variance = sum((r - mean_excess) ** 2 for r in excess_returns) / (len(excess_returns) - 1)
        std_dev = variance.sqrt()

        if std_dev == 0:
            return Decimal('0')

        return mean_excess / std_dev

    @staticmethod
    def sortino_ratio(returns: List[Decimal], target_return: Decimal = Decimal('0')) -> Decimal:
        """
        Calculate Sortino Ratio
        CFA Standard: (Portfolio Return - Target Return) / Downside Deviation

        Args:
            returns: List of period returns
            target_return: Target or minimum acceptable return

        Returns:
            Sortino ratio
        """
        if len(returns) < 2:
            return Decimal('0')

        excess_returns = [r - target_return for r in returns]
        mean_excess = sum(excess_returns) / len(excess_returns)

        # Calculate downside deviation (only negative excess returns)
        downside_returns = [r for r in excess_returns if r < 0]

        if not downside_returns:
            return Decimal('999')  # No downside risk

        downside_variance = sum(r ** 2 for r in downside_returns) / len(returns)
        downside_deviation = downside_variance.sqrt()

        if downside_deviation == 0:
            return Decimal('0')

        return mean_excess / downside_deviation

    @staticmethod
    def maximum_drawdown(prices: List[Decimal]) -> Tuple[Decimal, int, int]:
        """
        Calculate Maximum Drawdown
        CFA Standard: Maximum peak-to-trough decline

        Args:
            prices: List of price values

        Returns:
            Tuple of (max_drawdown, peak_index, trough_index)
        """
        if len(prices) < 2:
            return Decimal('0'), 0, 0

        max_dd = Decimal('0')
        peak_idx = 0
        trough_idx = 0
        current_peak = prices[0]
        current_peak_idx = 0

        for i, price in enumerate(prices):
            if price > current_peak:
                current_peak = price
                current_peak_idx = i

            drawdown = (current_peak - price) / current_peak
            if drawdown > max_dd:
                max_dd = drawdown
                peak_idx = current_peak_idx
                trough_idx = i

        return max_dd, peak_idx, trough_idx

    @staticmethod
    def var_historical(returns: List[Decimal], confidence_level: Decimal = Decimal('0.05')) -> Decimal:
        """
        Calculate Historical Value at Risk
        CFA Standard: Historical simulation method

        Args:
            returns: List of historical returns
            confidence_level: Confidence level (e.g., 0.05 for 95% VaR)

        Returns:
            VaR value (positive number representing loss)
        """
        if not returns:
            return Decimal('0')

        sorted_returns = sorted(returns)
        index = int(len(sorted_returns) * confidence_level)

        if index >= len(sorted_returns):
            index = len(sorted_returns) - 1

        return abs(sorted_returns[index])

    @staticmethod
    def calmar_ratio(annual_return: Decimal, max_drawdown: Decimal) -> Decimal:
        """
        Calculate Calmar Ratio
        CFA Standard: Annual Return / Maximum Drawdown

        Args:
            annual_return: Annualized return
            max_drawdown: Maximum drawdown

        Returns:
            Calmar ratio
        """
        if max_drawdown == 0:
            return Decimal('999')  # No drawdown

        return annual_return / max_drawdown


class AlternativeInvestmentBase(ABC):
    """
    Abstract base class for all alternative investment types
    Defines common interface and shared functionality
    """

    def __init__(self, parameters: AssetParameters):
        self.parameters = parameters
        self.market_data: List[MarketData] = []
        self.cash_flows: List[CashFlow] = []
        self.performance_history: List[Performance] = []
        self.config = Config()
        self.math = FinancialMath()
        self._validate_parameters()

    def _validate_parameters(self) -> None:
        """Validate asset parameters"""
        if self.parameters.management_fee:
            if not ValidationRules.validate_management_fee(
                    self.parameters.management_fee,
                    self.parameters.asset_class
            ):
                raise ValueError(f"Invalid management fee: {self.parameters.management_fee}")

        if self.parameters.performance_fee:
            if not ValidationRules.validate_performance_fee(
                    self.parameters.performance_fee,
                    self.parameters.asset_class
            ):
                raise ValueError(f"Invalid performance fee: {self.parameters.performance_fee}")

    def add_market_data(self, data: List[MarketData]) -> None:
        """Add market data to the investment"""
        self.market_data.extend(data)
        self.market_data.sort(key=lambda x: x.timestamp)

    def add_cash_flows(self, cash_flows: List[CashFlow]) -> None:
        """Add cash flows to the investment"""
        self.cash_flows.extend(cash_flows)
        self.cash_flows.sort(key=lambda x: x.date)

    def get_latest_price(self) -> Optional[Decimal]:
        """Get the most recent price"""
        if not self.market_data:
            return None
        return self.market_data[-1].price

    def get_price_history(self, start_date: str = None, end_date: str = None) -> List[MarketData]:
        """Get price history for specified date range"""
        filtered_data = self.market_data

        if start_date:
            filtered_data = [d for d in filtered_data if d.timestamp >= start_date]

        if end_date:
            filtered_data = [d for d in filtered_data if d.timestamp <= end_date]

        return filtered_data

    def calculate_simple_returns(self) -> List[Decimal]:
        """Calculate simple returns from price data"""
        if len(self.market_data) < 2:
            return []

        returns = []
        for i in range(1, len(self.market_data)):
            prev_price = self.market_data[i - 1].price
            curr_price = self.market_data[i].price
            ret = (curr_price - prev_price) / prev_price
            returns.append(ret)

        return returns

    def calculate_log_returns(self) -> List[Decimal]:
        """Calculate logarithmic returns from price data"""
        if len(self.market_data) < 2:
            return []

        returns = []
        for i in range(1, len(self.market_data)):
            prev_price = self.market_data[i - 1].price
            curr_price = self.market_data[i].price
            ret = (curr_price / prev_price).ln()
            returns.append(ret)

        return returns

    def calculate_volatility(self, returns: List[Decimal] = None, annualized: bool = True) -> Decimal:
        """Calculate volatility (standard deviation of returns)"""
        if returns is None:
            returns = self.calculate_simple_returns()

        if len(returns) < 2:
            return Decimal('0')

        mean_return = sum(returns) / len(returns)
        variance = sum((r - mean_return) ** 2 for r in returns) / (len(returns) - 1)
        volatility = variance.sqrt()

        if annualized:
            # Annualize based on frequency (assume daily data)
            volatility *= Constants.BUSINESS_DAYS_IN_YEAR.sqrt()

        return volatility

    def calculate_total_return(self, start_date: str = None, end_date: str = None) -> Decimal:
        """Calculate total return including distributions"""
        price_data = self.get_price_history(start_date, end_date)

        if len(price_data) < 2:
            return Decimal('0')

        # Price appreciation
        start_price = price_data[0].price
        end_price = price_data[-1].price
        price_return = (end_price - start_price) / start_price

        # Add distributions/cash flows
        relevant_cfs = self.cash_flows
        if start_date:
            relevant_cfs = [cf for cf in relevant_cfs if cf.date >= start_date]
        if end_date:
            relevant_cfs = [cf for cf in relevant_cfs if cf.date <= end_date]

        distributions = sum(cf.amount for cf in relevant_cfs if cf.amount > 0)
        distribution_return = distributions / start_price

        return price_return + distribution_return

    def calculate_fees(self, nav: Decimal, period_days: int = 365) -> Dict[str, Decimal]:
        """Calculate management and performance fees"""
        fees = {}

        # Management fee
        if self.parameters.management_fee:
            mgmt_fee = nav * self.parameters.management_fee * (Decimal(str(period_days)) / Constants.DAYS_IN_YEAR)
            fees['management_fee'] = mgmt_fee

        # Performance fee (simplified - would need high water mark tracking)
        if self.parameters.performance_fee:
            # This is a simplified calculation
            returns = self.calculate_simple_returns()
            if returns:
                excess_return = sum(returns) - (self.parameters.hurdle_rate or Decimal('0'))
                if excess_return > 0:
                    perf_fee = nav * excess_return * self.parameters.performance_fee
                    fees['performance_fee'] = perf_fee

        return fees

    @abstractmethod
    def calculate_nav(self) -> Decimal:
        """Calculate Net Asset Value - must be implemented by subclasses"""
        pass

    @abstractmethod
    def calculate_key_metrics(self) -> Dict[str, Any]:
        """Calculate key performance metrics - must be implemented by subclasses"""
        pass

    @abstractmethod
    def valuation_summary(self) -> Dict[str, Any]:
        """Provide valuation summary - must be implemented by subclasses"""
        pass

    def get_performance_summary(self) -> Dict[str, Any]:
        """Get comprehensive performance summary"""
        returns = self.calculate_simple_returns()

        if not returns:
            return {"error": "Insufficient data for performance calculation"}

        volatility = self.calculate_volatility(returns)
        sharpe = self.math.sharpe_ratio(returns)
        sortino = self.math.sortino_ratio(returns)

        prices = [md.price for md in self.market_data]
        max_dd, peak_idx, trough_idx = self.math.maximum_drawdown(prices)

        var_95 = self.math.var_historical(returns, Decimal('0.05'))

        total_return = self.calculate_total_return()

        return {
            'total_return': float(total_return),
            'annualized_return': float(total_return * Constants.DAYS_IN_YEAR / len(self.market_data)),
            'volatility': float(volatility),
            'sharpe_ratio': float(sharpe),
            'sortino_ratio': float(sortino),
            'maximum_drawdown': float(max_dd),
            'var_95': float(var_95),
            'number_of_observations': len(returns),
            'latest_price': float(self.get_latest_price() or 0)
        }


# Export main components
__all__ = ['FinancialMath', 'AlternativeInvestmentBase']