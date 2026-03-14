
"""Derivatives Utils Module
=================================

Fixed income utility functions

===== DATA SOURCES REQUIRED =====
INPUT:
  - Underlying asset price data and market information
  - Option chain data with strikes, expirations, and premiums
  - Interest rate curves and volatility surfaces
  - Market quotes and bid-ask spreads
  - Counterparty information and collateral data

OUTPUT:
  - Derivatives pricing models and Greeks calculations
  - Portfolio risk metrics and exposure analysis
  - Arbitrage opportunities and trading signals
  - Scenario analysis and stress test results
  - Hedging effectiveness and optimization recommendations

PARAMETERS:
  - risk_free_rate: Risk-free rate for option pricing (default: 0.02)
  - volatility_method: Volatility estimation method (default: 'historical')
  - dividend_yield: Dividend yield for underlying assets (default: 0.0)
  - time_steps: Number of time steps for simulation (default: 100)
  - confidence_level: Confidence level for risk metrics (default: 0.95)
"""



import numpy as np
import pandas as pd
from typing import List, Tuple, Optional, Callable, Union, Dict, Any
from datetime import datetime, date, timedelta
from scipy import optimize, interpolate, stats
from scipy.special import erf, erfc
import calendar
import logging
from enum import Enum
from dataclasses import dataclass

from .core import DayCountConvention, ValidationError, ModelValidator

logger = logging.getLogger(__name__)


class InterpolationMethod(Enum):
    """Interpolation methods for yield curves and surfaces"""
    LINEAR = "linear"
    CUBIC_SPLINE = "cubic"
    NATURAL_SPLINE = "natural"
    HERMITE = "hermite"
    AKIMA = "akima"


class OptimizationMethod(Enum):
    """Optimization methods for numerical procedures"""
    NEWTON_RAPHSON = "newton_raphson"
    BISECTION = "bisection"
    BRENT = "brent"
    SECANT = "secant"
    LEVENBERG_MARQUARDT = "lm"


@dataclass
class Holiday:
    """Holiday definition for business day calculations"""
    name: str
    date: datetime
    country: str = "US"


class BusinessDayCalculator:
    """Business day calculations with holiday support"""

    def __init__(self, country: str = "US"):
        self.country = country
        self.holidays = self._load_holidays()

    def _load_holidays(self) -> List[Holiday]:
        """Load holidays for specified country"""
        # US Federal Holidays (simplified)
        current_year = datetime.now().year
        holidays = []

        for year in range(current_year - 1, current_year + 5):
            # New Year's Day
            holidays.append(Holiday("New Year's Day", datetime(year, 1, 1)))

            # Independence Day
            holidays.append(Holiday("Independence Day", datetime(year, 7, 4)))

            # Christmas Day
            holidays.append(Holiday("Christmas Day", datetime(year, 12, 25)))

            # Martin Luther King Jr. Day (3rd Monday in January)
            jan_1 = datetime(year, 1, 1)
            days_to_monday = (7 - jan_1.weekday()) % 7
            first_monday = jan_1 + timedelta(days=days_to_monday)
            mlk_day = first_monday + timedelta(days=14)  # 3rd Monday
            holidays.append(Holiday("MLK Day", mlk_day))

            # Presidents Day (3rd Monday in February)
            feb_1 = datetime(year, 2, 1)
            days_to_monday = (7 - feb_1.weekday()) % 7
            first_monday = feb_1 + timedelta(days=days_to_monday)
            presidents_day = first_monday + timedelta(days=14)
            holidays.append(Holiday("Presidents Day", presidents_day))

            # Labor Day (1st Monday in September)
            sep_1 = datetime(year, 9, 1)
            days_to_monday = (7 - sep_1.weekday()) % 7
            labor_day = sep_1 + timedelta(days=days_to_monday)
            holidays.append(Holiday("Labor Day", labor_day))

            # Thanksgiving (4th Thursday in November)
            nov_1 = datetime(year, 11, 1)
            days_to_thursday = (3 - nov_1.weekday()) % 7
            first_thursday = nov_1 + timedelta(days=days_to_thursday)
            thanksgiving = first_thursday + timedelta(days=21)  # 4th Thursday
            holidays.append(Holiday("Thanksgiving", thanksgiving))

        return holidays

    def is_business_day(self, date_input: Union[datetime, date]) -> bool:
        """Check if date is a business day"""
        if isinstance(date_input, date):
            date_input = datetime.combine(date_input, datetime.min.time())

        # Check if weekend
        if date_input.weekday() >= 5:  # Saturday = 5, Sunday = 6
            return False

        # Check if holiday
        for holiday in self.holidays:
            if (date_input.date() == holiday.date.date() and
                    holiday.country == self.country):
                return False

        return True

    def add_business_days(self, start_date: Union[datetime, date],
                          days: int) -> datetime:
        """Add business days to a date"""
        if isinstance(start_date, date):
            start_date = datetime.combine(start_date, datetime.min.time())

        current_date = start_date
        days_added = 0

        while days_added < days:
            current_date += timedelta(days=1)
            if self.is_business_day(current_date):
                days_added += 1

        return current_date

    def business_days_between(self, start_date: Union[datetime, date],
                              end_date: Union[datetime, date]) -> int:
        """Count business days between two dates"""
        if isinstance(start_date, date):
            start_date = datetime.combine(start_date, datetime.min.time())
        if isinstance(end_date, date):
            end_date = datetime.combine(end_date, datetime.min.time())

        if start_date >= end_date:
            return 0

        business_days = 0
        current_date = start_date

        while current_date < end_date:
            current_date += timedelta(days=1)
            if self.is_business_day(current_date):
                business_days += 1

        return business_days


class MathUtils:
    """Mathematical utility functions for derivatives pricing"""

    @staticmethod
    def normal_cdf(x: float) -> float:
        """Cumulative distribution function of standard normal"""
        return 0.5 * (1 + erf(x / np.sqrt(2)))

    @staticmethod
    def normal_pdf(x: float) -> float:
        """Probability density function of standard normal"""
        return np.exp(-0.5 * x ** 2) / np.sqrt(2 * np.pi)

    @staticmethod
    def inverse_normal_cdf(p: float) -> float:
        """Inverse cumulative distribution function (quantile)"""
        if not 0 < p < 1:
            raise ValueError("Probability must be between 0 and 1")
        return stats.norm.ppf(p)

    @staticmethod
    def bivariate_normal_cdf(x: float, y: float, rho: float) -> float:
        """Bivariate normal cumulative distribution function"""
        return stats.multivariate_normal.cdf([x, y], cov=[[1, rho], [rho, 1]])

    @staticmethod
    def black_scholes_call_delta(S: float, K: float, T: float, r: float,
                                 sigma: float, q: float = 0) -> float:
        """Black-Scholes call delta (analytical)"""
        if T <= 0:
            return 1.0 if S > K else 0.0

        d1 = (np.log(S / K) + (r - q + 0.5 * sigma ** 2) * T) / (sigma * np.sqrt(T))
        return np.exp(-q * T) * MathUtils.normal_cdf(d1)

    @staticmethod
    def black_scholes_gamma(S: float, K: float, T: float, r: float,
                            sigma: float, q: float = 0) -> float:
        """Black-Scholes gamma (analytical)"""
        if T <= 0:
            return 0.0

        d1 = (np.log(S / K) + (r - q + 0.5 * sigma ** 2) * T) / (sigma * np.sqrt(T))
        return (np.exp(-q * T) * MathUtils.normal_pdf(d1)) / (S * sigma * np.sqrt(T))

    @staticmethod
    def compound_interest(principal: float, rate: float, time: float,
                          frequency: int = 1) -> float:
        """Calculate compound interest"""
        return principal * (1 + rate / frequency) ** (frequency * time)

    @staticmethod
    def continuous_compounding(principal: float, rate: float, time: float) -> float:
        """Calculate continuous compounding"""
        return principal * np.exp(rate * time)

    @staticmethod
    def present_value(future_value: float, rate: float, time: float) -> float:
        """Calculate present value with continuous compounding"""
        return future_value * np.exp(-rate * time)

    @staticmethod
    def annuity_pv(payment: float, rate: float, periods: int) -> float:
        """Present value of ordinary annuity"""
        if rate == 0:
            return payment * periods
        return payment * (1 - (1 + rate) ** -periods) / rate

    @staticmethod
    def perpetuity_pv(payment: float, rate: float) -> float:
        """Present value of perpetuity"""
        if rate <= 0:
            raise ValueError("Rate must be positive for perpetuity")
        return payment / rate


class InterpolationEngine:
    """Advanced interpolation methods for financial data"""

    @staticmethod
    def linear_interpolation(x_points: np.ndarray, y_points: np.ndarray,
                             x_new: Union[float, np.ndarray]) -> Union[float, np.ndarray]:
        """Linear interpolation"""
        if len(x_points) != len(y_points):
            raise ValueError("x_points and y_points must have same length")

        return np.interp(x_new, x_points, y_points)

    @staticmethod
    def cubic_spline_interpolation(x_points: np.ndarray, y_points: np.ndarray,
                                   x_new: Union[float, np.ndarray]) -> Union[float, np.ndarray]:
        """Cubic spline interpolation"""
        if len(x_points) < 4:
            return InterpolationEngine.linear_interpolation(x_points, y_points, x_new)

        spline = interpolate.CubicSpline(x_points, y_points)
        return spline(x_new)

    @staticmethod
    def natural_spline_interpolation(x_points: np.ndarray, y_points: np.ndarray,
                                     x_new: Union[float, np.ndarray]) -> Union[float, np.ndarray]:
        """Natural cubic spline interpolation"""
        if len(x_points) < 4:
            return InterpolationEngine.linear_interpolation(x_points, y_points, x_new)

        spline = interpolate.CubicSpline(x_points, y_points, bc_type='natural')
        return spline(x_new)

    @staticmethod
    def akima_interpolation(x_points: np.ndarray, y_points: np.ndarray,
                            x_new: Union[float, np.ndarray]) -> Union[float, np.ndarray]:
        """Akima interpolation (good for yield curves)"""
        if len(x_points) < 5:
            return InterpolationEngine.cubic_spline_interpolation(x_points, y_points, x_new)

        akima = interpolate.Akima1DInterpolator(x_points, y_points)
        return akima(x_new)

    @staticmethod
    def interpolate_yield_curve(maturities: np.ndarray, rates: np.ndarray,
                                target_maturity: float,
                                method: InterpolationMethod = InterpolationMethod.CUBIC_SPLINE) -> float:
        """Interpolate yield curve for specific maturity"""
        if method == InterpolationMethod.LINEAR:
            return InterpolationEngine.linear_interpolation(maturities, rates, target_maturity)
        elif method == InterpolationMethod.CUBIC_SPLINE:
            return InterpolationEngine.cubic_spline_interpolation(maturities, rates, target_maturity)
        elif method == InterpolationMethod.NATURAL_SPLINE:
            return InterpolationEngine.natural_spline_interpolation(maturities, rates, target_maturity)
        elif method == InterpolationMethod.AKIMA:
            return InterpolationEngine.akima_interpolation(maturities, rates, target_maturity)
        else:
            return InterpolationEngine.linear_interpolation(maturities, rates, target_maturity)


class OptimizationSolver:
    """Numerical optimization methods for derivatives pricing"""

    @staticmethod
    def newton_raphson(func: Callable, derivative: Callable, initial_guess: float,
                       tolerance: float = 1e-8, max_iterations: int = 100) -> Dict[str, Any]:
        """Newton-Raphson root finding method"""
        x = initial_guess

        for i in range(max_iterations):
            try:
                f_x = func(x)
                f_prime_x = derivative(x)

                if abs(f_prime_x) < tolerance:
                    return {"success": False, "message": "Derivative too small", "iterations": i}

                x_new = x - f_x / f_prime_x

                if abs(x_new - x) < tolerance:
                    return {
                        "success": True,
                        "root": x_new,
                        "function_value": func(x_new),
                        "iterations": i + 1
                    }

                x = x_new

            except Exception as e:
                return {"success": False, "message": str(e), "iterations": i}

        return {"success": False, "message": "Max iterations reached", "iterations": max_iterations}

    @staticmethod
    def bisection_method(func: Callable, a: float, b: float,
                         tolerance: float = 1e-8, max_iterations: int = 100) -> Dict[str, Any]:
        """Bisection root finding method"""
        if func(a) * func(b) >= 0:
            return {"success": False, "message": "Function must have opposite signs at endpoints"}

        for i in range(max_iterations):
            c = (a + b) / 2
            f_c = func(c)

            if abs(f_c) < tolerance or (b - a) / 2 < tolerance:
                return {
                    "success": True,
                    "root": c,
                    "function_value": f_c,
                    "iterations": i + 1
                }

            if func(a) * f_c < 0:
                b = c
            else:
                a = c

        return {"success": False, "message": "Max iterations reached", "iterations": max_iterations}

    @staticmethod
    def brent_method(func: Callable, a: float, b: float,
                     tolerance: float = 1e-8) -> Dict[str, Any]:
        """Brent's method for root finding (most robust)"""
        try:
            result = optimize.brentq(func, a, b, xtol=tolerance, full_output=True)
            root, result_info = result

            return {
                "success": True,
                "root": root,
                "function_value": func(root),
                "iterations": result_info.iterations,
                "function_calls": result_info.function_calls
            }
        except Exception as e:
            return {"success": False, "message": str(e)}

    @staticmethod
    def golden_section_search(func: Callable, a: float, b: float,
                              tolerance: float = 1e-8, maximize: bool = False) -> Dict[str, Any]:
        """Golden section search for optimization"""
        golden_ratio = (1 + np.sqrt(5)) / 2
        resphi = 2 - golden_ratio

        tol = tolerance
        x1 = a + resphi * (b - a)
        x2 = b - resphi * (b - a)
        f1 = func(x1)
        f2 = func(x2)

        if maximize:
            f1, f2 = -f1, -f2

        iterations = 0
        while abs(b - a) > tol:
            iterations += 1
            if f1 < f2:
                b = x2
                x2 = x1
                f2 = f1
                x1 = a + resphi * (b - a)
                f1 = func(x1)
                if maximize:
                    f1 = -f1
            else:
                a = x1
                x1 = x2
                f1 = f2
                x2 = b - resphi * (b - a)
                f2 = func(x2)
                if maximize:
                    f2 = -f2

        optimal_x = (a + b) / 2
        optimal_value = func(optimal_x)

        return {
            "success": True,
            "optimal_x": optimal_x,
            "optimal_value": optimal_value if not maximize else -optimal_value,
            "iterations": iterations
        }


class StatisticalUtils:
    """Statistical utility functions"""

    @staticmethod
    def calculate_returns(prices: np.ndarray, method: str = "log") -> np.ndarray:
        """Calculate returns from price series"""
        if len(prices) < 2:
            return np.array([])

        if method == "log":
            return np.diff(np.log(prices))
        elif method == "simple":
            return np.diff(prices) / prices[:-1]
        else:
            raise ValueError("Method must be 'log' or 'simple'")

    @staticmethod
    def rolling_volatility(returns: np.ndarray, window: int,
                           annualize: bool = True) -> np.ndarray:
        """Calculate rolling volatility"""
        if len(returns) < window:
            return np.array([])

        rolling_vol = []
        for i in range(window - 1, len(returns)):
            window_returns = returns[i - window + 1:i + 1]
            vol = np.std(window_returns)
            if annualize:
                vol *= np.sqrt(252)  # Assuming daily returns
            rolling_vol.append(vol)

        return np.array(rolling_vol)

    @staticmethod
    def ewma_volatility(returns: np.ndarray, lambda_param: float = 0.94,
                        annualize: bool = True) -> np.ndarray:
        """Exponentially weighted moving average volatility"""
        if len(returns) == 0:
            return np.array([])

        ewma_var = np.zeros(len(returns))
        ewma_var[0] = returns[0] ** 2

        for i in range(1, len(returns)):
            ewma_var[i] = lambda_param * ewma_var[i - 1] + (1 - lambda_param) * returns[i] ** 2

        ewma_vol = np.sqrt(ewma_var)
        if annualize:
            ewma_vol *= np.sqrt(252)

        return ewma_vol

    @staticmethod
    def garch_volatility(returns: np.ndarray, p: int = 1, q: int = 1) -> Dict[str, Any]:
        """GARCH(p,q) volatility estimation (simplified)"""
        # This is a simplified implementation
        # In practice, use specialized libraries like arch

        n = len(returns)
        if n < max(p, q) + 10:
            return {"success": False, "message": "Insufficient data for GARCH"}

        # Simple GARCH(1,1) implementation
        if p == 1 and q == 1:
            omega = 0.000001  # Long-term variance
            alpha = 0.1  # ARCH parameter
            beta = 0.85  # GARCH parameter

            conditional_var = np.zeros(n)
            conditional_var[0] = np.var(returns)

            for i in range(1, n):
                conditional_var[i] = (omega +
                                      alpha * returns[i - 1] ** 2 +
                                      beta * conditional_var[i - 1])

            conditional_vol = np.sqrt(conditional_var)

            return {
                "success": True,
                "conditional_volatility": conditional_vol,
                "parameters": {"omega": omega, "alpha": alpha, "beta": beta}
            }

        return {"success": False, "message": "Only GARCH(1,1) implemented"}

    @staticmethod
    def correlation_matrix(returns_matrix: np.ndarray, method: str = "pearson") -> np.ndarray:
        """Calculate correlation matrix"""
        if method == "pearson":
            return np.corrcoef(returns_matrix, rowvar=False)
        elif method == "spearman":
            return stats.spearmanr(returns_matrix).correlation
        elif method == "kendall":
            # Simplified Kendall's tau for multiple variables
            n_vars = returns_matrix.shape[1]
            corr_matrix = np.eye(n_vars)
            for i in range(n_vars):
                for j in range(i + 1, n_vars):
                    tau, _ = stats.kendalltau(returns_matrix[:, i], returns_matrix[:, j])
                    corr_matrix[i, j] = tau
                    corr_matrix[j, i] = tau
            return corr_matrix
        else:
            raise ValueError("Method must be 'pearson', 'spearman', or 'kendall'")

    @staticmethod
    def jarque_bera_test(data: np.ndarray) -> Dict[str, float]:
        """Jarque-Bera test for normality"""
        n = len(data)
        if n < 4:
            return {"statistic": np.nan, "p_value": np.nan}

        # Calculate skewness and kurtosis
        mean_data = np.mean(data)
        std_data = np.std(data, ddof=1)

        skewness = np.mean(((data - mean_data) / std_data) ** 3)
        kurtosis = np.mean(((data - mean_data) / std_data) ** 4)

        # Jarque-Bera statistic
        jb_statistic = n * (skewness ** 2 / 6 + (kurtosis - 3) ** 2 / 24)

        # p-value (chi-squared distribution with 2 degrees of freedom)
        p_value = 1 - stats.chi2.cdf(jb_statistic, 2)

        return {
            "statistic": jb_statistic,
            "p_value": p_value,
            "skewness": skewness,
            "kurtosis": kurtosis,
            "is_normal": p_value > 0.05  # 5% significance level
        }


class DateUtils:
    """Date utility functions for financial calculations"""

    @staticmethod
    def year_fraction(start_date: Union[datetime, date],
                      end_date: Union[datetime, date],
                      day_count: DayCountConvention = DayCountConvention.ACT_365) -> float:
        """Calculate year fraction between dates"""
        if isinstance(start_date, date):
            start_date = datetime.combine(start_date, datetime.min.time())
        if isinstance(end_date, date):
            end_date = datetime.combine(end_date, datetime.min.time())

        if end_date <= start_date:
            return 0.0

        if day_count == DayCountConvention.ACT_365:
            return (end_date - start_date).days / 365.0
        elif day_count == DayCountConvention.ACT_360:
            return (end_date - start_date).days / 360.0
        elif day_count == DayCountConvention.THIRTY_360:
            return DateUtils._thirty_360_fraction(start_date, end_date)
        elif day_count == DayCountConvention.ACT_ACT:
            return DateUtils._act_act_fraction(start_date, end_date)
        else:
            return (end_date - start_date).days / 365.0

    @staticmethod
    def _thirty_360_fraction(start_date: datetime, end_date: datetime) -> float:
        """30/360 day count calculation"""
        start_day = min(start_date.day, 30)
        end_day = end_date.day

        if start_day == 30 and end_day == 31:
            end_day = 30

        days = (360 * (end_date.year - start_date.year) +
                30 * (end_date.month - start_date.month) +
                (end_day - start_day))

        return days / 360.0

    @staticmethod
    def _act_act_fraction(start_date: datetime, end_date: datetime) -> float:
        """ACT/ACT day count calculation"""
        total_days = 0
        current_year = start_date.year
        current_date = start_date

        while current_date < end_date:
            year_end = datetime(current_year, 12, 31)
            next_year_start = datetime(current_year + 1, 1, 1)

            if end_date <= year_end:
                # All remaining days in current year
                days_in_year = (end_date - current_date).days
                year_length = 366 if calendar.isleap(current_year) else 365
                total_days += days_in_year / year_length
                break
            else:
                # Days remaining in current year
                days_in_year = (next_year_start - current_date).days
                year_length = 366 if calendar.isleap(current_year) else 365
                total_days += days_in_year / year_length

                current_year += 1
                current_date = next_year_start

        return total_days

    @staticmethod
    def add_tenor(start_date: Union[datetime, date], tenor: str) -> datetime:
        """Add tenor to date (e.g., '3M', '1Y', '6W')"""
        if isinstance(start_date, date):
            start_date = datetime.combine(start_date, datetime.min.time())

        tenor = tenor.upper()

        if tenor.endswith('D'):
            days = int(tenor[:-1])
            return start_date + timedelta(days=days)
        elif tenor.endswith('W'):
            weeks = int(tenor[:-1])
            return start_date + timedelta(weeks=weeks)
        elif tenor.endswith('M'):
            months = int(tenor[:-1])
            new_month = start_date.month + months
            new_year = start_date.year + (new_month - 1) // 12
            new_month = ((new_month - 1) % 12) + 1

            # Handle end-of-month dates
            try:
                return start_date.replace(year=new_year, month=new_month)
            except ValueError:
                # Handle cases like Jan 31 + 1M = Feb 28/29
                last_day = calendar.monthrange(new_year, new_month)[1]
                return start_date.replace(year=new_year, month=new_month, day=last_day)
        elif tenor.endswith('Y'):
            years = int(tenor[:-1])
            try:
                return start_date.replace(year=start_date.year + years)
            except ValueError:
                # Handle leap year edge case (Feb 29)
                return start_date.replace(year=start_date.year + years, month=2, day=28)
        else:
            raise ValueError(f"Invalid tenor format: {tenor}")

    @staticmethod
    def generate_schedule(start_date: Union[datetime, date],
                          end_date: Union[datetime, date],
                          frequency: str = "3M",
                          business_day_convention: str = "modified_following") -> List[datetime]:
        """Generate payment schedule between dates"""
        if isinstance(start_date, date):
            start_date = datetime.combine(start_date, datetime.min.time())
        if isinstance(end_date, date):
            end_date = datetime.combine(end_date, datetime.min.time())

        schedule = []
        current_date = start_date

        bdc = BusinessDayCalculator()

        while current_date < end_date:
            next_date = DateUtils.add_tenor(current_date, frequency)

            if next_date > end_date:
                next_date = end_date

            # Apply business day convention
            if business_day_convention == "following":
                while not bdc.is_business_day(next_date):
                    next_date += timedelta(days=1)
            elif business_day_convention == "preceding":
                while not bdc.is_business_day(next_date):
                    next_date -= timedelta(days=1)
            elif business_day_convention == "modified_following":
                original_month = next_date.month
                while not bdc.is_business_day(next_date):
                    next_date += timedelta(days=1)
                    if next_date.month != original_month:
                        # Rolled into next month, use preceding instead
                        next_date = DateUtils.add_tenor(current_date, frequency)
                        while not bdc.is_business_day(next_date):
                            next_date -= timedelta(days=1)
                        break

            schedule.append(next_date)
            current_date = next_date

        return schedule


# Export main classes and functions
__all__ = [
    'InterpolationMethod', 'OptimizationMethod', 'Holiday', 'BusinessDayCalculator',
    'MathUtils', 'InterpolationEngine', 'OptimizationSolver', 'StatisticalUtils', 'DateUtils'
]