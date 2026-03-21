
"""Equity Investment Calculations Module
======================================

Financial calculations and utility functions

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



import numpy as np
import pandas as pd
from typing import List, Dict, Any, Optional, Tuple, Union
import math
from scipy import stats
from scipy.optimize import fsolve
import warnings

from .base_models import ValidationError


class FinancialCalculations:
    """Common financial calculation utilities"""

    @staticmethod
    def time_value_of_money(principal: float, rate: float, periods: int,
                            compounding: str = "annual") -> Dict[str, float]:
        """Comprehensive time value of money calculations"""

        # Compounding frequency mapping
        compounding_freq = {
            "annual": 1,
            "semi-annual": 2,
            "quarterly": 4,
            "monthly": 12,
            "daily": 365,
            "continuous": float('inf')
        }

        freq = compounding_freq.get(compounding.lower(), 1)

        if freq == float('inf'):  # Continuous compounding
            future_value = principal * math.exp(rate * periods)
            effective_rate = math.exp(rate) - 1
        else:
            future_value = principal * (1 + rate / freq) ** (freq * periods)
            effective_rate = (1 + rate / freq) ** freq - 1

        present_value = future_value / ((1 + effective_rate) ** periods)

        return {
            'principal': principal,
            'future_value': future_value,
            'present_value_of_fv': present_value,
            'effective_annual_rate': effective_rate,
            'total_interest': future_value - principal,
            'compounding_frequency': freq
        }

    @staticmethod
    def annuity_calculations(payment: float, rate: float, periods: int,
                             annuity_type: str = "ordinary") -> Dict[str, float]:
        """Calculate present and future value of annuities"""

        if rate <= 0:
            # Handle zero interest rate case
            pv_annuity = payment * periods
            fv_annuity = payment * periods
        else:
            # Ordinary annuity (payments at end of period)
            pv_ordinary = payment * ((1 - (1 + rate) ** -periods) / rate)
            fv_ordinary = payment * (((1 + rate) ** periods - 1) / rate)

            if annuity_type.lower() == "due":
                # Annuity due (payments at beginning of period)
                pv_annuity = pv_ordinary * (1 + rate)
                fv_annuity = fv_ordinary * (1 + rate)
            else:
                pv_annuity = pv_ordinary
                fv_annuity = fv_ordinary

        return {
            'payment_amount': payment,
            'present_value': pv_annuity,
            'future_value': fv_annuity,
            'total_payments': payment * periods,
            'total_interest': fv_annuity - (payment * periods),
            'annuity_type': annuity_type
        }

    @staticmethod
    def perpetuity_value(payment: float, discount_rate: float,
                         growth_rate: float = 0) -> Dict[str, float]:
        """Calculate present value of perpetuity"""

        if discount_rate <= growth_rate:
            raise ValidationError("Discount rate must be greater than growth rate")

        if growth_rate == 0:
            # Simple perpetuity
            pv = payment / discount_rate
        else:
            # Growing perpetuity
            pv = payment / (discount_rate - growth_rate)

        return {
            'payment': payment,
            'discount_rate': discount_rate,
            'growth_rate': growth_rate,
            'present_value': pv,
            'perpetuity_type': 'Growing' if growth_rate > 0 else 'Simple'
        }

    @staticmethod
    def loan_calculations(principal: float, annual_rate: float, years: int,
                          payment_frequency: int = 12) -> Dict[str, Any]:
        """Calculate loan payments and amortization"""

        monthly_rate = annual_rate / payment_frequency
        total_payments = years * payment_frequency

        if annual_rate == 0:
            payment = principal / total_payments
        else:
            payment = principal * (monthly_rate * (1 + monthly_rate) ** total_payments) / \
                      ((1 + monthly_rate) ** total_payments - 1)

        # Create amortization schedule
        balance = principal
        schedule = []
        total_interest = 0

        for i in range(1, int(total_payments) + 1):
            interest_payment = balance * monthly_rate
            principal_payment = payment - interest_payment
            balance -= principal_payment
            total_interest += interest_payment

            schedule.append({
                'payment_number': i,
                'payment': payment,
                'principal': principal_payment,
                'interest': interest_payment,
                'balance': max(0, balance)  # Avoid negative balance due to rounding
            })

        return {
            'loan_amount': principal,
            'monthly_payment': payment,
            'total_payments': total_payments,
            'total_interest': total_interest,
            'total_cost': principal + total_interest,
            'amortization_schedule': schedule[:12],  # First year only
            'full_schedule_available': True
        }

    @staticmethod
    def bond_calculations(face_value: float, coupon_rate: float, market_rate: float,
                          years_to_maturity: float, frequency: int = 2) -> Dict[str, float]:
        """Calculate bond price, yield, and duration"""

        periods = years_to_maturity * frequency
        coupon_payment = (face_value * coupon_rate) / frequency
        period_rate = market_rate / frequency

        # Bond price calculation
        if market_rate == 0:
            bond_price = face_value + (coupon_payment * periods)
        else:
            # Present value of coupon payments
            pv_coupons = coupon_payment * ((1 - (1 + period_rate) ** -periods) / period_rate)
            # Present value of face value
            pv_face = face_value / ((1 + period_rate) ** periods)
            bond_price = pv_coupons + pv_face

        # Current yield
        current_yield = (coupon_payment * frequency) / bond_price

        # Macaulay Duration
        cash_flows = [coupon_payment] * int(periods)
        cash_flows[-1] += face_value  # Add face value to last payment

        weighted_time = 0
        total_pv = 0

        for t, cf in enumerate(cash_flows, 1):
            pv_cf = cf / ((1 + period_rate) ** t)
            weighted_time += (t / frequency) * pv_cf
            total_pv += pv_cf

        macaulay_duration = weighted_time / total_pv
        modified_duration = macaulay_duration / (1 + market_rate / frequency)

        return {
            'bond_price': bond_price,
            'face_value': face_value,
            'coupon_rate': coupon_rate,
            'market_rate': market_rate,
            'current_yield': current_yield,
            'macaulay_duration': macaulay_duration,
            'modified_duration': modified_duration,
            'price_sensitivity': modified_duration * bond_price * 0.01,  # Price change for 1% rate change
            'premium_discount': 'Premium' if bond_price > face_value else 'Discount' if bond_price < face_value else 'Par'
        }


class StatisticalCalculations:
    """Statistical analysis utilities for finance"""

    @staticmethod
    def descriptive_statistics(data: Union[List[float], pd.Series]) -> Dict[str, float]:
        """Calculate comprehensive descriptive statistics"""

        if isinstance(data, list):
            data = pd.Series(data)

        return {
            'count': len(data),
            'mean': data.mean(),
            'median': data.median(),
            'mode': data.mode().iloc[0] if not data.mode().empty else np.nan,
            'std_dev': data.std(),
            'variance': data.var(),
            'skewness': data.skew(),
            'kurtosis': data.kurtosis(),
            'min': data.min(),
            'max': data.max(),
            'range': data.max() - data.min(),
            'q25': data.quantile(0.25),
            'q75': data.quantile(0.75),
            'iqr': data.quantile(0.75) - data.quantile(0.25),
            'cv': data.std() / data.mean() if data.mean() != 0 else np.nan
        }

    @staticmethod
    def correlation_analysis(x: Union[List[float], pd.Series],
                             y: Union[List[float], pd.Series]) -> Dict[str, float]:
        """Calculate correlation and regression statistics"""

        if isinstance(x, list):
            x = pd.Series(x)
        if isinstance(y, list):
            y = pd.Series(y)

        # Remove missing values
        valid_data = pd.DataFrame({'x': x, 'y': y}).dropna()
        x_clean = valid_data['x']
        y_clean = valid_data['y']

        if len(x_clean) < 2:
            return {'error': 'Insufficient data for correlation analysis'}

        # Correlation
        pearson_corr = x_clean.corr(y_clean)
        spearman_corr = x_clean.corr(y_clean, method='spearman')

        # Linear regression
        slope, intercept, r_value, p_value, std_err = stats.linregress(x_clean, y_clean)

        return {
            'pearson_correlation': pearson_corr,
            'spearman_correlation': spearman_corr,
            'r_squared': r_value ** 2,
            'regression_slope': slope,
            'regression_intercept': intercept,
            'p_value': p_value,
            'standard_error': std_err,
            'sample_size': len(x_clean)
        }

    @staticmethod
    def hypothesis_testing(sample_data: Union[List[float], pd.Series],
                           null_hypothesis: float, alternative: str = "two-sided",
                           alpha: float = 0.05) -> Dict[str, Any]:
        """Perform one-sample t-test"""

        if isinstance(sample_data, list):
            sample_data = pd.Series(sample_data)

        # Remove missing values
        clean_data = sample_data.dropna()

        if len(clean_data) < 2:
            return {'error': 'Insufficient data for hypothesis testing'}

        # One-sample t-test
        t_stat, p_value = stats.ttest_1samp(clean_data, null_hypothesis)

        # Determine if we reject null hypothesis
        if alternative == "two-sided":
            reject_null = p_value < alpha
        elif alternative == "greater":
            reject_null = (t_stat > 0) and (p_value / 2 < alpha)
        elif alternative == "less":
            reject_null = (t_stat < 0) and (p_value / 2 < alpha)
        else:
            reject_null = p_value < alpha

        # Confidence interval
        confidence_level = 1 - alpha
        margin_error = stats.t.ppf((1 + confidence_level) / 2, len(clean_data) - 1) * \
                       (clean_data.std() / math.sqrt(len(clean_data)))

        ci_lower = clean_data.mean() - margin_error
        ci_upper = clean_data.mean() + margin_error

        return {
            'sample_mean': clean_data.mean(),
            'null_hypothesis': null_hypothesis,
            't_statistic': t_stat,
            'p_value': p_value,
            'alpha': alpha,
            'reject_null': reject_null,
            'confidence_interval': (ci_lower, ci_upper),
            'confidence_level': confidence_level,
            'sample_size': len(clean_data),
            'degrees_freedom': len(clean_data) - 1
        }


class RiskMetrics:
    """Risk and return calculation utilities"""

    @staticmethod
    def portfolio_metrics(returns: Union[List[float], pd.Series],
                          risk_free_rate: float = 0.02) -> Dict[str, float]:
        """Calculate comprehensive portfolio risk metrics"""

        if isinstance(returns, list):
            returns = pd.Series(returns)

        # Remove missing values
        returns = returns.dropna()

        if len(returns) == 0:
            return {'error': 'No valid return data'}

        # Basic metrics
        mean_return = returns.mean()
        volatility = returns.std()

        # Risk-adjusted metrics
        sharpe_ratio = (mean_return - risk_free_rate / 252) / volatility if volatility > 0 else 0

        # Downside metrics
        downside_returns = returns[returns < 0]
        downside_deviation = downside_returns.std() if len(downside_returns) > 0 else 0
        sortino_ratio = (mean_return - risk_free_rate / 252) / downside_deviation if downside_deviation > 0 else 0

        # Value at Risk (parametric)
        var_95 = np.percentile(returns, 5)
        var_99 = np.percentile(returns, 1)

        # Expected Shortfall (Conditional VaR)
        es_95 = returns[returns <= var_95].mean() if len(returns[returns <= var_95]) > 0 else var_95
        es_99 = returns[returns <= var_99].mean() if len(returns[returns <= var_99]) > 0 else var_99

        # Maximum Drawdown
        cumulative_returns = (1 + returns).cumprod()
        running_max = cumulative_returns.expanding().max()
        drawdown = (cumulative_returns - running_max) / running_max
        max_drawdown = drawdown.min()

        return {
            'mean_return': mean_return,
            'annualized_return': mean_return * 252,
            'volatility': volatility,
            'annualized_volatility': volatility * math.sqrt(252),
            'sharpe_ratio': sharpe_ratio,
            'sortino_ratio': sortino_ratio,
            'var_95': var_95,
            'var_99': var_99,
            'expected_shortfall_95': es_95,
            'expected_shortfall_99': es_99,
            'max_drawdown': max_drawdown,
            'downside_deviation': downside_deviation,
            'positive_periods': (returns > 0).sum(),
            'negative_periods': (returns < 0).sum(),
            'hit_ratio': (returns > 0).sum() / len(returns)
        }

    @staticmethod
    def beta_calculation(asset_returns: Union[List[float], pd.Series],
                         market_returns: Union[List[float], pd.Series]) -> Dict[str, float]:
        """Calculate beta and related risk metrics"""

        if isinstance(asset_returns, list):
            asset_returns = pd.Series(asset_returns)
        if isinstance(market_returns, list):
            market_returns = pd.Series(market_returns)

        # Align series and remove missing values
        combined = pd.DataFrame({'asset': asset_returns, 'market': market_returns}).dropna()

        if len(combined) < 10:
            return {'error': 'Insufficient data for beta calculation'}

        asset_clean = combined['asset']
        market_clean = combined['market']

        # Beta calculation
        covariance = asset_clean.cov(market_clean)
        market_variance = market_clean.var()
        beta = covariance / market_variance if market_variance > 0 else 0

        # Alpha calculation (using CAPM)
        risk_free_rate = 0.02 / 252  # Daily risk-free rate
        alpha = asset_clean.mean() - (risk_free_rate + beta * (market_clean.mean() - risk_free_rate))

        # R-squared
        correlation = asset_clean.corr(market_clean)
        r_squared = correlation ** 2

        # Tracking error
        excess_returns = asset_clean - market_clean
        tracking_error = excess_returns.std()

        # Information ratio
        information_ratio = excess_returns.mean() / tracking_error if tracking_error > 0 else 0

        return {
            'beta': beta,
            'alpha': alpha,
            'correlation': correlation,
            'r_squared': r_squared,
            'tracking_error': tracking_error,
            'information_ratio': information_ratio,
            'sample_size': len(combined),
            'annualized_alpha': alpha * 252,
            'annualized_tracking_error': tracking_error * math.sqrt(252)
        }


class OptionCalculations:
    """Option pricing and Greeks calculations"""

    @staticmethod
    def black_scholes(spot_price: float, strike_price: float, time_to_expiry: float,
                      risk_free_rate: float, volatility: float,
                      option_type: str = "call") -> Dict[str, float]:
        """Calculate Black-Scholes option price and Greeks"""

        # Black-Scholes formula components
        d1 = (math.log(spot_price / strike_price) +
              (risk_free_rate + 0.5 * volatility ** 2) * time_to_expiry) / \
             (volatility * math.sqrt(time_to_expiry))

        d2 = d1 - volatility * math.sqrt(time_to_expiry)

        # Standard normal CDF
        N_d1 = stats.norm.cdf(d1)
        N_d2 = stats.norm.cdf(d2)
        N_neg_d1 = stats.norm.cdf(-d1)
        N_neg_d2 = stats.norm.cdf(-d2)

        # Standard normal PDF
        n_d1 = stats.norm.pdf(d1)

        if option_type.lower() == "call":
            # Call option price
            option_price = spot_price * N_d1 - strike_price * math.exp(-risk_free_rate * time_to_expiry) * N_d2

            # Call Greeks
            delta = N_d1
            gamma = n_d1 / (spot_price * volatility * math.sqrt(time_to_expiry))
            theta = (-spot_price * n_d1 * volatility / (2 * math.sqrt(time_to_expiry)) -
                     risk_free_rate * strike_price * math.exp(-risk_free_rate * time_to_expiry) * N_d2) / 365

        else:  # Put option
            # Put option price
            option_price = strike_price * math.exp(-risk_free_rate * time_to_expiry) * N_neg_d2 - spot_price * N_neg_d1

            # Put Greeks
            delta = N_d1 - 1
            gamma = n_d1 / (spot_price * volatility * math.sqrt(time_to_expiry))
            theta = (-spot_price * n_d1 * volatility / (2 * math.sqrt(time_to_expiry)) +
                     risk_free_rate * strike_price * math.exp(-risk_free_rate * time_to_expiry) * N_neg_d2) / 365

        # Greeks common to both calls and puts
        vega = spot_price * n_d1 * math.sqrt(time_to_expiry) / 100  # Per 1% change in volatility
        rho = (strike_price * time_to_expiry * math.exp(-risk_free_rate * time_to_expiry) *
               (N_d2 if option_type.lower() == "call" else N_neg_d2)) / 100  # Per 1% change in rate

        return {
            'option_price': option_price,
            'delta': delta,
            'gamma': gamma,
            'theta': theta,
            'vega': vega,
            'rho': rho,
            'd1': d1,
            'd2': d2,
            'intrinsic_value': max(0, spot_price - strike_price) if option_type.lower() == "call"
            else max(0, strike_price - spot_price),
            'time_value': option_price - max(0, spot_price - strike_price if option_type.lower() == "call"
            else strike_price - spot_price)
        }

    @staticmethod
    def implied_volatility(option_price: float, spot_price: float, strike_price: float,
                           time_to_expiry: float, risk_free_rate: float,
                           option_type: str = "call") -> float:
        """Calculate implied volatility using Newton-Raphson method"""

        def bs_price_diff(vol):
            bs_result = OptionCalculations.black_scholes(
                spot_price, strike_price, time_to_expiry, risk_free_rate, vol, option_type
            )
            return bs_result['option_price'] - option_price

        try:
            # Initial guess
            initial_vol = 0.2
            implied_vol = fsolve(bs_price_diff, initial_vol)[0]

            # Validate result
            if implied_vol < 0 or implied_vol > 5:  # Unrealistic volatility
                return np.nan

            return implied_vol

        except:
            return np.nan


class TechnicalIndicators:
    """Technical analysis calculation utilities"""

    @staticmethod
    def moving_averages(prices: Union[List[float], pd.Series],
                        periods: List[int]) -> Dict[str, pd.Series]:
        """Calculate multiple moving averages"""

        if isinstance(prices, list):
            prices = pd.Series(prices)

        moving_averages = {}

        for period in periods:
            ma_name = f'MA_{period}'
            moving_averages[ma_name] = prices.rolling(window=period).mean()

        return moving_averages

    @staticmethod
    def bollinger_bands(prices: Union[List[float], pd.Series],
                        period: int = 20, std_dev: float = 2) -> Dict[str, pd.Series]:
        """Calculate Bollinger Bands"""

        if isinstance(prices, list):
            prices = pd.Series(prices)

        ma = prices.rolling(window=period).mean()
        std = prices.rolling(window=period).std()

        upper_band = ma + (std * std_dev)
        lower_band = ma - (std * std_dev)

        return {
            'middle_band': ma,
            'upper_band': upper_band,
            'lower_band': lower_band,
            'bandwidth': (upper_band - lower_band) / ma,
            'percent_b': (prices - lower_band) / (upper_band - lower_band)
        }

    @staticmethod
    def rsi(prices: Union[List[float], pd.Series], period: int = 14) -> pd.Series:
        """Calculate Relative Strength Index"""

        if isinstance(prices, list):
            prices = pd.Series(prices)

        delta = prices.diff()
        gain = delta.where(delta > 0, 0)
        loss = -delta.where(delta < 0, 0)

        avg_gain = gain.rolling(window=period).mean()
        avg_loss = loss.rolling(window=period).mean()

        rs = avg_gain / avg_loss
        rsi = 100 - (100 / (1 + rs))

        return rsi

    @staticmethod
    def macd(prices: Union[List[float], pd.Series],
             fast_period: int = 12, slow_period: int = 26, signal_period: int = 9) -> Dict[str, pd.Series]:
        """Calculate MACD indicator"""

        if isinstance(prices, list):
            prices = pd.Series(prices)

        ema_fast = prices.ewm(span=fast_period).mean()
        ema_slow = prices.ewm(span=slow_period).mean()

        macd_line = ema_fast - ema_slow
        signal_line = macd_line.ewm(span=signal_period).mean()
        histogram = macd_line - signal_line

        return {
            'macd_line': macd_line,
            'signal_line': signal_line,
            'histogram': histogram
        }


# Utility functions for common calculations
def quick_return_calculation(start_price: float, end_price: float,
                             dividends: float = 0) -> Dict[str, float]:
    """Quick return calculation"""
    price_return = (end_price - start_price) / start_price
    total_return = (end_price - start_price + dividends) / start_price

    return {
        'price_return': price_return,
        'total_return': total_return,
        'dividend_yield': dividends / start_price,
        'price_appreciation': price_return
    }


def compound_annual_growth_rate(beginning_value: float, ending_value: float,
                                years: float) -> float:
    """Calculate CAGR"""
    if beginning_value <= 0 or ending_value <= 0 or years <= 0:
        raise ValidationError("All values must be positive for CAGR calculation")

    return (ending_value / beginning_value) ** (1 / years) - 1


def rule_of_72(interest_rate: float) -> float:
    """Calculate doubling time using Rule of 72"""
    if interest_rate <= 0:
        raise ValidationError("Interest rate must be positive")

    return 72 / (interest_rate * 100)


def effective_annual_rate(nominal_rate: float, compounding_periods: int) -> float:
    """Calculate effective annual rate"""
    return (1 + nominal_rate / compounding_periods) ** compounding_periods - 1


def present_value_growing_annuity(payment: float, growth_rate: float,
                                  discount_rate: float, periods: int) -> float:
    """Calculate PV of growing annuity"""
    if discount_rate == growth_rate:
        return payment * periods / (1 + discount_rate)

    factor = (1 - ((1 + growth_rate) / (1 + discount_rate)) ** periods)
    return payment * factor / (discount_rate - growth_rate)