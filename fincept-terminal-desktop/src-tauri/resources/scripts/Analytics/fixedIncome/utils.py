
Fixed Income Utils Module
=================================

Fixed income utility functions

===== DATA SOURCES REQUIRED =====
INPUT:
  - Bond prospectuses and issue documentation
  - Market price data and yield curve information
  - Credit ratings and default recovery rates
  - Interest rate benchmarks and swap curves
  - Macroeconomic data affecting fixed income markets

OUTPUT:
  - Bond valuation models and price calculations
  - Yield curve analysis and forward rate calculations
  - Credit risk assessment and spread analysis
  - Duration and convexity risk metrics
  - Portfolio optimization and immunization strategies

PARAMETERS:
  - yield_curve_method: Yield curve modeling method (default: 'nelson_siegel')
  - credit_spread: Credit spread assumption (default: 0.02)
  - recovery_rate: Default recovery rate (default: 0.40)
  - convexity_adjustment: Convexity adjustment factor (default: 1.0)
  - benchmark_curve: Benchmark yield curve (default: 'treasury')


import math
from typing import Callable, Optional, List, Tuple, Union
from decimal import Decimal, getcontext
from datetime import date, datetime, timedelta
from config import (
    DayCountConvention, CompoundingFrequency, BusinessDayConvention,
    PRECISION_CONFIG, ERROR_TOLERANCE, ValidationError
)

# Set high precision for calculations
getcontext().prec = 28


class MathUtils:
    """Mathematical utility functions for fixed income calculations"""

    @staticmethod
    def newton_raphson(func: Callable[[Decimal], Decimal],
                       derivative: Callable[[Decimal], Decimal],
                       initial_guess: Decimal,
                       tolerance: Decimal = ERROR_TOLERANCE['medium'],
                       max_iterations: int = 100) -> Decimal:
        """Newton-Raphson method for root finding"""
        x = initial_guess

        for i in range(max_iterations):
            fx = func(x)

            if abs(fx) < tolerance:
                return x

            dfx = derivative(x)
            if abs(dfx) < tolerance:
                raise ValueError("Derivative too small, convergence failed")

            x_new = x - fx / dfx

            if abs(x_new - x) < tolerance:
                return x_new

            x = x_new

        raise ValueError(f"Newton-Raphson failed to converge after {max_iterations} iterations")

    @staticmethod
    def bisection_method(func: Callable[[Decimal], Decimal],
                         lower_bound: Decimal,
                         upper_bound: Decimal,
                         tolerance: Decimal = ERROR_TOLERANCE['medium'],
                         max_iterations: int = 100) -> Decimal:
        """Bisection method for root finding"""
        if func(lower_bound) * func(upper_bound) > 0:
            raise ValueError("Function must have opposite signs at bounds")

        for i in range(max_iterations):
            midpoint = (lower_bound + upper_bound) / Decimal('2')
            f_mid = func(midpoint)

            if abs(f_mid) < tolerance or (upper_bound - lower_bound) / Decimal('2') < tolerance:
                return midpoint

            if func(lower_bound) * f_mid < 0:
                upper_bound = midpoint
            else:
                lower_bound = midpoint

        raise ValueError(f"Bisection method failed to converge after {max_iterations} iterations")

    @staticmethod
    def brent_method(func: Callable[[Decimal], Decimal],
                     lower_bound: Decimal,
                     upper_bound: Decimal,
                     tolerance: Decimal = ERROR_TOLERANCE['medium'],
                     max_iterations: int = 100) -> Decimal:
        """Brent's method for root finding (combines bisection and secant methods)"""
        a, b = lower_bound, upper_bound
        fa, fb = func(a), func(b)

        if fa * fb > 0:
            raise ValueError("Function must have opposite signs at bounds")

        if abs(fa) < abs(fb):
            a, b = b, a
            fa, fb = fb, fa

        c = a
        fc = fa
        mflag = True

        for i in range(max_iterations):
            if abs(fb) < tolerance:
                return b

            if fa != fc and fb != fc:
                # Inverse quadratic interpolation
                s = (a * fb * fc) / ((fa - fb) * (fa - fc)) + \
                    (b * fa * fc) / ((fb - fa) * (fb - fc)) + \
                    (c * fa * fb) / ((fc - fa) * (fc - fb))
            else:
                # Secant method
                s = b - fb * (b - a) / (fb - fa)

            # Check conditions for bisection
            condition1 = not ((Decimal('3') * a + b) / Decimal('4') <= s <= b)
            condition2 = mflag and abs(s - b) >= abs(b - c) / Decimal('2')
            condition3 = not mflag and abs(s - b) >= abs(c - d) / Decimal('2')
            condition4 = mflag and abs(b - c) < tolerance
            condition5 = not mflag and abs(c - d) < tolerance

            if condition1 or condition2 or condition3 or condition4 or condition5:
                s = (a + b) / Decimal('2')
                mflag = True
            else:
                mflag = False

            fs = func(s)
            d = c
            c = b
            fc = fb

            if fa * fs < 0:
                b = s
                fb = fs
            else:
                a = s
                fa = fs

            if abs(fa) < abs(fb):
                a, b = b, a
                fa, fb = fb, fa

        raise ValueError(f"Brent's method failed to converge after {max_iterations} iterations")

    @staticmethod
    def present_value(cash_flow: Decimal, discount_rate: Decimal, time_period: Decimal) -> Decimal:
        """Calculate present value of a cash flow"""
        if discount_rate < -1:
            raise ValueError("Discount rate cannot be less than -100%")

        return cash_flow / ((Decimal('1') + discount_rate) ** time_period)

    @staticmethod
    def future_value(present_value: Decimal, rate: Decimal, time_period: Decimal) -> Decimal:
        """Calculate future value"""
        return present_value * ((Decimal('1') + rate) ** time_period)

    @staticmethod
    def compound_frequency_conversion(rate: Decimal,
                                      from_freq: CompoundingFrequency,
                                      to_freq: CompoundingFrequency) -> Decimal:
        """Convert rate between different compounding frequencies"""
        if from_freq == to_freq:
            return rate

        # Convert to continuous compounding first
        if from_freq == CompoundingFrequency.CONTINUOUS:
            continuous_rate = rate
        else:
            m = Decimal(str(from_freq.value))
            continuous_rate = m * Decimal(str(math.log(float(1 + rate / m))))

        # Convert from continuous to target frequency
        if to_freq == CompoundingFrequency.CONTINUOUS:
            return continuous_rate
        else:
            n = Decimal(str(to_freq.value))
            return n * (Decimal(str(math.exp(float(continuous_rate / n)))) - Decimal('1'))

    @staticmethod
    def linear_interpolation(x: Decimal, x1: Decimal, y1: Decimal, x2: Decimal, y2: Decimal) -> Decimal:
        """Linear interpolation between two points"""
        if x1 == x2:
            return y1

        return y1 + (y2 - y1) * (x - x1) / (x2 - x1)

    @staticmethod
    def cubic_spline_interpolation(x: Decimal, points: List[Tuple[Decimal, Decimal]]) -> Decimal:
        """Simplified cubic spline interpolation (linear for now)"""
        # Find the two points to interpolate between
        points = sorted(points, key=lambda p: p[0])

        if x <= points[0][0]:
            return points[0][1]
        if x >= points[-1][0]:
            return points[-1][1]

        for i in range(len(points) - 1):
            if points[i][0] <= x <= points[i + 1][0]:
                return MathUtils.linear_interpolation(
                    x, points[i][0], points[i][1], points[i + 1][0], points[i + 1][1]
                )

        return points[0][1]  # Fallback


class DateUtils:
    """Date utility functions for fixed income calculations"""

    @staticmethod
    def add_business_days(start_date: date, business_days: int,
                          convention: BusinessDayConvention = BusinessDayConvention.FOLLOWING) -> date:
        """Add business days to a date"""
        current_date = start_date
        days_added = 0

        while days_added < business_days:
            current_date += timedelta(days=1)
            if DateUtils.is_business_day(current_date):
                days_added += 1

        return DateUtils.adjust_for_business_day(current_date, convention)

    @staticmethod
    def is_business_day(check_date: date) -> bool:
        """Check if date is a business day (Monday-Friday, no holidays)"""
        # Simple implementation - excludes weekends only
        # In practice, would need holiday calendar
        return check_date.weekday() < 5

    @staticmethod
    def adjust_for_business_day(check_date: date,
                                convention: BusinessDayConvention) -> date:
        """Adjust date according to business day convention"""
        if DateUtils.is_business_day(check_date):
            return check_date

        if convention == BusinessDayConvention.FOLLOWING:
            while not DateUtils.is_business_day(check_date):
                check_date += timedelta(days=1)
        elif convention == BusinessDayConvention.PRECEDING:
            while not DateUtils.is_business_day(check_date):
                check_date -= timedelta(days=1)
        elif convention == BusinessDayConvention.MODIFIED_FOLLOWING:
            original_month = check_date.month
            while not DateUtils.is_business_day(check_date):
                check_date += timedelta(days=1)
            # If we moved to next month, use preceding instead
            if check_date.month != original_month:
                check_date = DateUtils.adjust_for_business_day(
                    check_date.replace(day=1) - timedelta(days=1),
                    BusinessDayConvention.PRECEDING
                )
        elif convention == BusinessDayConvention.MODIFIED_PRECEDING:
            original_month = check_date.month
            while not DateUtils.is_business_day(check_date):
                check_date -= timedelta(days=1)
            # If we moved to previous month, use following instead
            if check_date.month != original_month:
                check_date = DateUtils.adjust_for_business_day(
                    check_date.replace(day=1),
                    BusinessDayConvention.FOLLOWING
                )
        # UNADJUSTED returns the original date

        return check_date

    @staticmethod
    def calculate_day_count_fraction(start_date: date, end_date: date,
                                     convention: DayCountConvention) -> Decimal:
        """Calculate day count fraction between two dates"""
        if start_date >= end_date:
            return Decimal('0')

        if convention == DayCountConvention.ACTUAL_360:
            days = (end_date - start_date).days
            return Decimal(days) / Decimal('360')

        elif convention == DayCountConvention.ACTUAL_365:
            days = (end_date - start_date).days
            return Decimal(days) / Decimal('365')

        elif convention == DayCountConvention.ACTUAL_365_FIXED:
            days = (end_date - start_date).days
            return Decimal(days) / Decimal('365')

        elif convention == DayCountConvention.ACTUAL_ACTUAL:
            # ISDA Actual/Actual
            days = (end_date - start_date).days
            year_start = date(start_date.year, 1, 1)
            year_end = date(start_date.year + 1, 1, 1)
            days_in_year = (year_end - year_start).days
            return Decimal(days) / Decimal(days_in_year)

        elif convention == DayCountConvention.THIRTY_360:
            return DateUtils._thirty_360_fraction(start_date, end_date)

        elif convention == DayCountConvention.THIRTY_360_EUROPEAN:
            return DateUtils._thirty_360_european_fraction(start_date, end_date)

        else:
            # Default to Actual/365
            days = (end_date - start_date).days
            return Decimal(days) / Decimal('365')

    @staticmethod
    def _thirty_360_fraction(start_date: date, end_date: date) -> Decimal:
        """Calculate 30/360 day count fraction (US/NASD convention)"""
        d1 = start_date.day
        d2 = end_date.day
        m1 = start_date.month
        m2 = end_date.month
        y1 = start_date.year
        y2 = end_date.year

        # Adjust for end-of-month
        if d1 == 31:
            d1 = 30
        if d1 == 30 and d2 == 31:
            d2 = 30

        days = 360 * (y2 - y1) + 30 * (m2 - m1) + (d2 - d1)
        return Decimal(days) / Decimal('360')

    @staticmethod
    def _thirty_360_european_fraction(start_date: date, end_date: date) -> Decimal:
        """Calculate 30E/360 day count fraction (European convention)"""
        d1 = min(start_date.day, 30)
        d2 = min(end_date.day, 30)
        m1 = start_date.month
        m2 = end_date.month
        y1 = start_date.year
        y2 = end_date.year

        days = 360 * (y2 - y1) + 30 * (m2 - m1) + (d2 - d1)
        return Decimal(days) / Decimal('360')

    @staticmethod
    def is_leap_year(year: int) -> bool:
        """Check if year is a leap year"""
        return year % 4 == 0 and (year % 100 != 0 or year % 400 == 0)

    @staticmethod
    def days_in_year(year: int) -> int:
        """Get number of days in a year"""
        return 366 if DateUtils.is_leap_year(year) else 365

    @staticmethod
    def end_of_month(input_date: date) -> date:
        """Get last day of month for given date"""
        if input_date.month == 12:
            next_month = date(input_date.year + 1, 1, 1)
        else:
            next_month = date(input_date.year, input_date.month + 1, 1)
        return next_month - timedelta(days=1)

    @staticmethod
    def generate_schedule(start_date: date, end_date: date,
                          frequency: CompoundingFrequency,
                          convention: BusinessDayConvention = BusinessDayConvention.MODIFIED_FOLLOWING) -> List[date]:
        """Generate payment schedule between two dates"""
        if frequency == CompoundingFrequency.CONTINUOUS:
            return [end_date]  # Only final payment for continuous

        schedule = []
        freq_value = frequency.value
        months_between = 12 // freq_value

        current_date = end_date
        while current_date > start_date:
            schedule.append(DateUtils.adjust_for_business_day(current_date, convention))

            # Move back by payment frequency
            if current_date.month <= months_between:
                new_month = 12 + current_date.month - months_between
                new_year = current_date.year - 1
            else:
                new_month = current_date.month - months_between
                new_year = current_date.year

            try:
                current_date = current_date.replace(year=new_year, month=new_month)
            except ValueError:
                # Handle end-of-month adjustments
                current_date = DateUtils.end_of_month(date(new_year, new_month, 1))

        schedule.reverse()
        return schedule


class ValidationUtils:
    """Input validation utilities"""

    @staticmethod
    def validate_positive(value: Decimal, name: str) -> Decimal:
        """Validate that value is positive"""
        if value <= 0:
            raise ValidationError(f"{name} must be positive, got {value}")
        return value

    @staticmethod
    def validate_non_negative(value: Decimal, name: str) -> Decimal:
        """Validate that value is non-negative"""
        if value < 0:
            raise ValidationError(f"{name} cannot be negative, got {value}")
        return value

    @staticmethod
    def validate_percentage(value: Decimal, name: str, allow_negative: bool = False) -> Decimal:
        """Validate percentage value (0-100% or -100% to 100%)"""
        min_val = Decimal('-1') if allow_negative else Decimal('0')
        max_val = Decimal('1')

        if not (min_val <= value <= max_val):
            raise ValidationError(f"{name} must be between {min_val * 100}% and {max_val * 100}%, got {value * 100}%")
        return value

    @staticmethod
    def validate_yield(value: Decimal, name: str = "Yield") -> Decimal:
        """Validate yield value"""
        from config import VALIDATION_RULES
        min_yield = VALIDATION_RULES['min_yield']
        max_yield = VALIDATION_RULES['max_yield']

        if not (min_yield <= value <= max_yield):
            raise ValidationError(
                f"{name} must be between {min_yield * 100}% and {max_yield * 100}%, got {value * 100}%")
        return value

    @staticmethod
    def validate_price(value: Decimal, name: str = "Price") -> Decimal:
        """Validate bond price"""
        from config import VALIDATION_RULES
        min_price = VALIDATION_RULES['min_price']
        max_price = VALIDATION_RULES['max_price']

        if not (min_price <= value <= max_price):
            raise ValidationError(f"{name} must be between {min_price} and {max_price}, got {value}")
        return value

    @staticmethod
    def validate_date_order(start_date: date, end_date: date,
                            start_name: str = "Start date", end_name: str = "End date"):
        """Validate that start date is before end date"""
        if start_date >= end_date:
            raise ValidationError(f"{start_name} ({start_date}) must be before {end_name} ({end_date})")

    @staticmethod
    def validate_maturity_range(issue_date: date, maturity_date: date):
        """Validate maturity is within reasonable range"""
        from config import VALIDATION_RULES

        days_to_maturity = (maturity_date - issue_date).days
        min_days = VALIDATION_RULES['min_maturity_days']
        max_days = VALIDATION_RULES['max_maturity_years'] * 365

        if days_to_maturity < min_days:
            raise ValidationError(f"Maturity too short: {days_to_maturity} days (minimum: {min_days})")

        if days_to_maturity > max_days:
            raise ValidationError(f"Maturity too long: {days_to_maturity} days (maximum: {max_days})")


class FormattingUtils:
    """Output formatting utilities"""

    @staticmethod
    def format_percentage(value: Decimal, decimal_places: int = 2) -> str:
        """Format decimal as percentage"""
        percentage = value * Decimal('100')
        return f"{percentage:.{decimal_places}f}%"

    @staticmethod
    def format_currency(value: Decimal, currency_symbol: str = "$", decimal_places: int = 2) -> str:
        """Format decimal as currency"""
        return f"{currency_symbol}{value:,.{decimal_places}f}"

    @staticmethod
    def format_basis_points(value: Decimal) -> str:
        """Format decimal as basis points"""
        bps = value * Decimal('10000')
        return f"{bps:.0f} bps"

    @staticmethod
    def format_yield(value: Decimal, decimal_places: int = 3) -> str:
        """Format yield with appropriate precision"""
        return FormattingUtils.format_percentage(value, decimal_places)

    @staticmethod
    def format_duration(value: Decimal, decimal_places: int = 2) -> str:
        """Format duration"""
        return f"{value:.{decimal_places}f} years"

    @staticmethod
    def format_price(value: Decimal, decimal_places: int = 4) -> str:
        """Format bond price"""
        return f"{value:.{decimal_places}f}"


class CachingUtils:
    """Simple caching utilities for expensive calculations"""

    def __init__(self, max_size: int = 1000):
        self.cache = {}
        self.max_size = max_size
        self.access_order = []

    def get(self, key: str):
        """Get cached value"""
        if key in self.cache:
            # Move to end (most recently used)
            self.access_order.remove(key)
            self.access_order.append(key)
            return self.cache[key]
        return None

    def put(self, key: str, value):
        """Put value in cache"""
        if key in self.cache:
            # Update existing
            self.cache[key] = value
            self.access_order.remove(key)
            self.access_order.append(key)
        else:
            # Add new
            if len(self.cache) >= self.max_size:
                # Remove least recently used
                oldest_key = self.access_order.pop(0)
                del self.cache[oldest_key]

            self.cache[key] = value
            self.access_order.append(key)

    def clear(self):
        """Clear cache"""
        self.cache.clear()
        self.access_order.clear()


# Global cache instance
calculation_cache = CachingUtils(max_size=5000)


def cache_calculation(func):
    """Decorator for caching calculation results"""

    def wrapper(*args, **kwargs):
        # Create cache key from function name and arguments
        cache_key = f"{func.__name__}_{hash(str(args) + str(sorted(kwargs.items())))}"

        # Try to get from cache
        result = calculation_cache.get(cache_key)
        if result is not None:
            return result

        # Calculate and cache result
        result = func(*args, **kwargs)
        calculation_cache.put(cache_key, result)
        return result

    return wrapper


# Commonly used mathematical constants as Decimals
PI = Decimal('3.1415926535897932384626433832795')
E = Decimal('2.7182818284590452353602874713527')
SQRT_2 = Decimal('1.4142135623730950488016887242097')
SQRT_2PI = Decimal('2.5066282746310005024157652848110')


# Standard normal distribution approximation
def standard_normal_cdf(x: Decimal) -> Decimal:
    """Approximate standard normal cumulative distribution function"""
    # Abramowitz and Stegun approximation
    x_float = float(x)

    if x_float < 0:
        return Decimal('1') - standard_normal_cdf(-x)

    # Constants for approximation
    a1 = Decimal('0.254829592')
    a2 = Decimal('-0.284496736')
    a3 = Decimal('1.421413741')
    a4 = Decimal('-1.453152027')
    a5 = Decimal('1.061405429')
    p = Decimal('0.3275911')

    t = Decimal('1') / (Decimal('1') + p * x)
    y = Decimal('1') - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * \
        Decimal(str(math.exp(-float(x * x / Decimal('2')))))

    return y


def black_scholes_call_price(S: Decimal, K: Decimal, T: Decimal, r: Decimal, sigma: Decimal) -> Decimal:
    """Black-Scholes call option price (simplified implementation)"""
    if T <= 0 or sigma <= 0:
        return max(S - K, Decimal('0'))

    d1 = (Decimal(str(math.log(float(S / K)))) + (r + sigma * sigma / Decimal('2')) * T) / (
                sigma * Decimal(str(math.sqrt(float(T)))))
    d2 = d1 - sigma * Decimal(str(math.sqrt(float(T))))

    call_price = S * standard_normal_cdf(d1) - K * Decimal(str(math.exp(-float(r * T)))) * standard_normal_cdf(d2)

    return call_price