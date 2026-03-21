
"""Equity Investment Market Efficiency Module
======================================

Market efficiency and anomaly analysis

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
from typing import List, Dict, Any, Optional, Tuple
from dataclasses import dataclass
import scipy.stats as stats
from sklearn.linear_model import LinearRegression
import warnings

warnings.filterwarnings('ignore')

from .base_models import (
    BaseMarketAnalysisModel, MarketEfficiencyForm, ValidationError,
    FinceptAnalyticsError
)


@dataclass
class EfficiencyTestResult:
    """Result structure for efficiency tests"""
    test_name: str
    efficiency_form: MarketEfficiencyForm
    test_statistic: float
    p_value: float
    is_efficient: bool
    confidence_level: float
    interpretation: str


class MarketEfficiencyTester(BaseMarketAnalysisModel):
    """Market efficiency testing framework"""

    def __init__(self):
        super().__init__("Market Efficiency Tester", "Tests for various forms of market efficiency")
        self.significance_level = 0.05

    def validate_inputs(self, **kwargs) -> bool:
        """Validate inputs for efficiency tests"""
        price_data = kwargs.get('price_data')
        if price_data is None or price_data.empty:
            raise ValidationError("Price data is required for efficiency tests")

        if 'Close' not in price_data.columns:
            raise ValidationError("Price data must contain 'Close' column")

        return True

    def analyze_market_data(self, market_data: pd.DataFrame) -> Dict[str, Any]:
        """Comprehensive market efficiency analysis"""

        results = {
            'weak_form_tests': self.test_weak_form_efficiency(market_data),
            'semi_strong_tests': self.test_semi_strong_efficiency(market_data),
            'strong_form_tests': self.test_strong_form_efficiency(market_data),
            'anomalies': self.detect_market_anomalies(market_data),
            'overall_assessment': {}
        }

        # Overall efficiency assessment
        weak_efficient = results['weak_form_tests']['is_efficient']
        semi_strong_efficient = results['semi_strong_tests']['is_efficient']
        strong_efficient = results['strong_form_tests']['is_efficient']

        if strong_efficient:
            efficiency_level = "Strong-form efficient"
        elif semi_strong_efficient:
            efficiency_level = "Semi-strong-form efficient"
        elif weak_efficient:
            efficiency_level = "Weak-form efficient"
        else:
            efficiency_level = "Inefficient"

        results['overall_assessment'] = {
            'efficiency_level': efficiency_level,
            'weak_form_efficient': weak_efficient,
            'semi_strong_form_efficient': semi_strong_efficient,
            'strong_form_efficient': strong_efficient,
            'anomalies_detected': len(results['anomalies']) > 0
        }

        return results


class WeakFormEfficiencyTester:
    """Tests for weak-form market efficiency"""

    def __init__(self, significance_level: float = 0.05):
        self.significance_level = significance_level

    def runs_test(self, returns: pd.Series) -> EfficiencyTestResult:
        """Runs test for randomness in return series"""

        # Convert returns to binary sequence (above/below median)
        median_return = returns.median()
        binary_sequence = (returns > median_return).astype(int)

        # Count runs
        runs = 1
        for i in range(1, len(binary_sequence)):
            if binary_sequence.iloc[i] != binary_sequence.iloc[i - 1]:
                runs += 1

        # Calculate expected runs and variance
        n1 = sum(binary_sequence)  # Number of 1s
        n2 = len(binary_sequence) - n1  # Number of 0s
        n = len(binary_sequence)

        expected_runs = (2 * n1 * n2) / n + 1
        variance_runs = (2 * n1 * n2 * (2 * n1 * n2 - n)) / (n ** 2 * (n - 1))

        # Test statistic
        if variance_runs > 0:
            z_stat = (runs - expected_runs) / np.sqrt(variance_runs)
            p_value = 2 * (1 - stats.norm.cdf(abs(z_stat)))
        else:
            z_stat = 0
            p_value = 1

        is_efficient = p_value > self.significance_level

        return EfficiencyTestResult(
            test_name="Runs Test",
            efficiency_form=MarketEfficiencyForm.WEAK,
            test_statistic=z_stat,
            p_value=p_value,
            is_efficient=is_efficient,
            confidence_level=1 - self.significance_level,
            interpretation=f"Returns appear {'random' if is_efficient else 'non-random'} (p-value: {p_value:.4f})"
        )

    def autocorrelation_test(self, returns: pd.Series, lags: List[int] = None) -> Dict[str, EfficiencyTestResult]:
        """Test for autocorrelation in returns at various lags"""

        if lags is None:
            lags = [1, 5, 10, 20]

        results = {}

        for lag in lags:
            if len(returns) <= lag:
                continue

            # Calculate autocorrelation
            autocorr = returns.autocorr(lag=lag)

            # Test statistic (Box-Pierce test approximation)
            n = len(returns)
            test_stat = n * (autocorr ** 2)
            p_value = 1 - stats.chi2.cdf(test_stat, df=1)

            is_efficient = p_value > self.significance_level

            results[f'lag_{lag}'] = EfficiencyTestResult(
                test_name=f"Autocorrelation Test (Lag {lag})",
                efficiency_form=MarketEfficiencyForm.WEAK,
                test_statistic=autocorr,
                p_value=p_value,
                is_efficient=is_efficient,
                confidence_level=1 - self.significance_level,
                interpretation=f"Lag {lag} autocorrelation: {autocorr:.4f} ({'not significant' if is_efficient else 'significant'})"
            )

        return results

    def variance_ratio_test(self, returns: pd.Series, periods: List[int] = None) -> Dict[str, EfficiencyTestResult]:
        """Variance ratio test for random walk"""

        if periods is None:
            periods = [2, 4, 8, 16]

        results = {}

        for k in periods:
            if len(returns) < k * 10:  # Need sufficient observations
                continue

            # Calculate variance ratios
            var_1 = returns.var()

            # k-period returns
            k_returns = returns.rolling(k).sum().dropna()
            var_k = k_returns.var()

            # Variance ratio
            variance_ratio = var_k / (k * var_1)

            # Test statistic under random walk hypothesis
            n = len(returns)
            test_stat = np.sqrt(n) * (variance_ratio - 1) / np.sqrt(2 * (k - 1))
            p_value = 2 * (1 - stats.norm.cdf(abs(test_stat)))

            is_efficient = p_value > self.significance_level

            results[f'period_{k}'] = EfficiencyTestResult(
                test_name=f"Variance Ratio Test (Period {k})",
                efficiency_form=MarketEfficiencyForm.WEAK,
                test_statistic=variance_ratio,
                p_value=p_value,
                is_efficient=is_efficient,
                confidence_level=1 - self.significance_level,
                interpretation=f"VR({k}) = {variance_ratio:.4f} ({'consistent with' if is_efficient else 'inconsistent with'} random walk)"
            )

        return results


class SemiStrongFormTester:
    """Tests for semi-strong form market efficiency"""

    def __init__(self, significance_level: float = 0.05):
        self.significance_level = significance_level

    def event_study_analysis(self, price_data: pd.DataFrame, event_dates: List[str],
                             estimation_window: int = 250, event_window: int = 21) -> Dict[str, Any]:
        """Event study analysis to test market reaction to new information"""

        if 'Close' not in price_data.columns:
            raise ValidationError("Price data must contain 'Close' column")

        # Calculate returns
        returns = price_data['Close'].pct_change().dropna()

        results = []

        for event_date in event_dates:
            try:
                event_idx = returns.index.get_loc(pd.to_datetime(event_date))
            except KeyError:
                continue

            # Define windows
            est_start = max(0, event_idx - estimation_window - event_window // 2)
            est_end = event_idx - event_window // 2
            event_start = event_idx - event_window // 2
            event_end = event_idx + event_window // 2

            if est_start >= est_end or event_start >= len(returns):
                continue

            # Estimation period returns
            estimation_returns = returns.iloc[est_start:est_end]

            # Event period returns
            event_returns = returns.iloc[event_start:event_end + 1]

            # Calculate normal performance (market model)
            # Simplified: use mean return during estimation period
            expected_return = estimation_returns.mean()

            # Calculate abnormal returns
            abnormal_returns = event_returns - expected_return

            # Cumulative abnormal returns
            cumulative_ar = abnormal_returns.cumsum()

            # Test statistics
            ar_std = estimation_returns.std()
            t_stat = abnormal_returns.mean() / (ar_std / np.sqrt(len(abnormal_returns)))
            p_value = 2 * (1 - stats.t.cdf(abs(t_stat), df=len(estimation_returns) - 1))

            results.append({
                'event_date': event_date,
                'abnormal_returns': abnormal_returns.tolist(),
                'cumulative_abnormal_return': cumulative_ar.iloc[-1],
                'average_abnormal_return': abnormal_returns.mean(),
                't_statistic': t_stat,
                'p_value': p_value,
                'is_significant': p_value < self.significance_level
            })

        # Overall assessment
        significant_events = sum(1 for r in results if r['is_significant'])
        efficiency_assessment = significant_events / len(results) if results else 0

        return {
            'individual_events': results,
            'total_events': len(results),
            'significant_events': significant_events,
            'efficiency_ratio': 1 - efficiency_assessment,  # Higher ratio = more efficient
            'is_efficient': efficiency_assessment < 0.1,  # Less than 10% significant events
            'interpretation': f"Market shows {'strong' if efficiency_assessment < 0.05 else 'moderate' if efficiency_assessment < 0.15 else 'weak'} semi-strong efficiency"
        }

    def post_announcement_drift_test(self, price_data: pd.DataFrame,
                                     earnings_announcement_dates: List[str]) -> Dict[str, Any]:
        """Test for post-earnings announcement drift"""

        returns = price_data['Close'].pct_change().dropna()

        drift_periods = [5, 10, 20, 60]  # Days after announcement
        results = {}

        for period in drift_periods:
            cumulative_returns = []

            for announcement_date in earnings_announcement_dates:
                try:
                    announcement_idx = returns.index.get_loc(pd.to_datetime(announcement_date))

                    if announcement_idx + period < len(returns):
                        post_returns = returns.iloc[announcement_idx + 1:announcement_idx + period + 1]
                        cumulative_return = (1 + post_returns).prod() - 1
                        cumulative_returns.append(cumulative_return)

                except (KeyError, IndexError):
                    continue

            if cumulative_returns:
                avg_drift = np.mean(cumulative_returns)
                drift_std = np.std(cumulative_returns)
                t_stat = avg_drift / (drift_std / np.sqrt(len(cumulative_returns)))
                p_value = 2 * (1 - stats.t.cdf(abs(t_stat), df=len(cumulative_returns) - 1))

                results[f'{period}_day'] = {
                    'average_drift': avg_drift,
                    'drift_std': drift_std,
                    't_statistic': t_stat,
                    'p_value': p_value,
                    'is_significant': p_value < self.significance_level,
                    'sample_size': len(cumulative_returns)
                }

        return results


class StrongFormTester:
    """Tests for strong-form market efficiency"""

    def __init__(self, significance_level: float = 0.05):
        self.significance_level = significance_level

    def insider_trading_analysis(self, price_data: pd.DataFrame,
                                 insider_transaction_dates: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Analyze abnormal returns around insider trading dates"""

        returns = price_data['Close'].pct_change().dropna()

        buy_returns = []
        sell_returns = []

        for transaction in insider_transaction_dates:
            transaction_date = transaction.get('date')
            transaction_type = transaction.get('type')  # 'buy' or 'sell'

            try:
                transaction_idx = returns.index.get_loc(pd.to_datetime(transaction_date))

                # Look at returns in the following 30 days
                if transaction_idx + 30 < len(returns):
                    future_returns = returns.iloc[transaction_idx + 1:transaction_idx + 31]
                    cumulative_return = (1 + future_returns).prod() - 1

                    if transaction_type.lower() == 'buy':
                        buy_returns.append(cumulative_return)
                    elif transaction_type.lower() == 'sell':
                        sell_returns.append(cumulative_return)

            except (KeyError, IndexError):
                continue

        results = {}

        # Analyze buy transactions
        if buy_returns:
            avg_buy_return = np.mean(buy_returns)
            buy_std = np.std(buy_returns)
            buy_t_stat = avg_buy_return / (buy_std / np.sqrt(len(buy_returns)))
            buy_p_value = stats.t.sf(buy_t_stat, df=len(buy_returns) - 1)  # One-tailed test

            results['insider_buys'] = {
                'average_return': avg_buy_return,
                'sample_size': len(buy_returns),
                't_statistic': buy_t_stat,
                'p_value': buy_p_value,
                'outperforms_market': buy_p_value < self.significance_level and avg_buy_return > 0
            }

        # Analyze sell transactions
        if sell_returns:
            avg_sell_return = np.mean(sell_returns)
            sell_std = np.std(sell_returns)
            sell_t_stat = avg_sell_return / (sell_std / np.sqrt(len(sell_returns)))
            sell_p_value = stats.t.cdf(sell_t_stat, df=len(sell_returns) - 1)  # One-tailed test (negative)

            results['insider_sells'] = {
                'average_return': avg_sell_return,
                'sample_size': len(sell_returns),
                't_statistic': sell_t_stat,
                'p_value': sell_p_value,
                'underperforms_market': sell_p_value < self.significance_level and avg_sell_return < 0
            }

        # Overall strong-form efficiency assessment
        insider_advantage = False

        if results.get('insider_buys', {}).get('outperforms_market', False):
            insider_advantage = True
        if results.get('insider_sells', {}).get('underperforms_market', False):
            insider_advantage = True

        results['strong_form_efficient'] = not insider_advantage

        return results

    def mutual_fund_performance_analysis(self, fund_returns: pd.DataFrame,
                                         market_returns: pd.Series) -> Dict[str, Any]:
        """Analyze mutual fund performance for strong-form efficiency"""

        results = {}

        for fund_name in fund_returns.columns:
            fund_return_series = fund_returns[fund_name].dropna()

            # Align dates
            common_dates = fund_return_series.index.intersection(market_returns.index)
            fund_aligned = fund_return_series.loc[common_dates]
            market_aligned = market_returns.loc[common_dates]

            if len(fund_aligned) < 50:  # Need sufficient observations
                continue

            # Calculate alpha (Jensen's alpha)
            # Simplified CAPM: Fund Return = Alpha + Beta * Market Return + Error
            X = market_aligned.values.reshape(-1, 1)
            y = fund_aligned.values

            model = LinearRegression()
            model.fit(X, y)

            alpha = model.intercept_
            beta = model.coef_[0]

            # Calculate t-statistic for alpha
            y_pred = model.predict(X)
            residuals = y - y_pred
            mse = np.mean(residuals ** 2)

            # Standard error of alpha
            x_mean = np.mean(market_aligned)
            alpha_se = np.sqrt(mse * (1 / len(fund_aligned) + x_mean ** 2 / np.sum((market_aligned - x_mean) ** 2)))

            alpha_t_stat = alpha / alpha_se if alpha_se > 0 else 0
            alpha_p_value = 2 * (1 - stats.t.cdf(abs(alpha_t_stat), df=len(fund_aligned) - 2))

            results[fund_name] = {
                'alpha': alpha,
                'beta': beta,
                'alpha_t_statistic': alpha_t_stat,
                'alpha_p_value': alpha_p_value,
                'significant_alpha': alpha_p_value < self.significance_level,
                'excess_return': alpha * 252,  # Annualized alpha
                'sample_size': len(fund_aligned)
            }

        # Overall assessment
        funds_with_significant_alpha = sum(1 for f in results.values() if f['significant_alpha'])
        total_funds = len(results)

        strong_form_efficient = (funds_with_significant_alpha / total_funds) < 0.05 if total_funds > 0 else True

        return {
            'individual_funds': results,
            'total_funds_analyzed': total_funds,
            'funds_with_significant_alpha': funds_with_significant_alpha,
            'percentage_significant': (funds_with_significant_alpha / total_funds * 100) if total_funds > 0 else 0,
            'strong_form_efficient': strong_form_efficient
        }


class MarketAnomalyDetector:
    """Detect various market anomalies"""

    def detect_calendar_anomalies(self, price_data: pd.DataFrame) -> Dict[str, Any]:
        """Detect calendar-based anomalies"""

        returns = price_data['Close'].pct_change().dropna()
        returns.index = pd.to_datetime(returns.index)

        anomalies = {}

        # Day-of-the-week effect
        returns_by_weekday = returns.groupby(returns.index.dayofweek)
        weekday_means = returns_by_weekday.mean()
        weekday_stats = {}

        for day in range(5):  # Monday=0 to Friday=4
            day_returns = returns_by_weekday.get_group(day) if day in returns_by_weekday.groups else pd.Series([])
            if len(day_returns) > 10:
                t_stat, p_value = stats.ttest_1samp(day_returns, 0)
                weekday_stats[day] = {
                    'mean_return': day_returns.mean(),
                    't_statistic': t_stat,
                    'p_value': p_value,
                    'sample_size': len(day_returns)
                }

        anomalies['day_of_week'] = weekday_stats

        # Month-of-the-year effect
        returns_by_month = returns.groupby(returns.index.month)
        monthly_stats = {}

        for month in range(1, 13):
            month_returns = returns_by_month.get_group(month) if month in returns_by_month.groups else pd.Series([])
            if len(month_returns) > 5:
                t_stat, p_value = stats.ttest_1samp(month_returns, 0)
                monthly_stats[month] = {
                    'mean_return': month_returns.mean(),
                    't_statistic': t_stat,
                    'p_value': p_value,
                    'sample_size': len(month_returns)
                }

        anomalies['month_of_year'] = monthly_stats

        # January effect
        january_returns = returns[returns.index.month == 1]
        other_returns = returns[returns.index.month != 1]

        if len(january_returns) > 5 and len(other_returns) > 5:
            january_mean = january_returns.mean()
            other_mean = other_returns.mean()
            t_stat, p_value = stats.ttest_ind(january_returns, other_returns)

            anomalies['january_effect'] = {
                'january_mean': january_mean,
                'other_months_mean': other_mean,
                'difference': january_mean - other_mean,
                't_statistic': t_stat,
                'p_value': p_value,
                'january_outperforms': january_mean > other_mean and p_value < 0.05
            }

        return anomalies

    def detect_size_anomaly(self, price_data: pd.DataFrame, market_cap_data: pd.Series) -> Dict[str, Any]:
        """Detect size anomaly (small-cap outperformance)"""

        returns = price_data['Close'].pct_change().dropna()

        # Align market cap data with returns
        aligned_cap = market_cap_data.reindex(returns.index).fillna(method='ffill')

        # Create size deciles
        deciles = pd.qcut(aligned_cap, 10, labels=False)

        decile_returns = {}
        for decile in range(10):
            decile_mask = deciles == decile
            decile_return_series = returns[decile_mask]

            if len(decile_return_series) > 20:
                decile_returns[decile + 1] = {
                    'mean_return': decile_return_series.mean(),
                    'volatility': decile_return_series.std(),
                    'sharpe_ratio': decile_return_series.mean() / decile_return_series.std() if decile_return_series.std() > 0 else 0,
                    'sample_size': len(decile_return_series)
                }

        # Test if smallest decile outperforms largest
        if 1 in decile_returns and 10 in decile_returns:
            small_returns = returns[deciles == 0]
            large_returns = returns[deciles == 9]

            t_stat, p_value = stats.ttest_ind(small_returns, large_returns)

            size_anomaly = {
                'decile_performance': decile_returns,
                'small_vs_large_test': {
                    'small_cap_mean': small_returns.mean(),
                    'large_cap_mean': large_returns.mean(),
                    't_statistic': t_stat,
                    'p_value': p_value,
                    'small_outperforms': small_returns.mean() > large_returns.mean() and p_value < 0.05
                }
            }
        else:
            size_anomaly = {'decile_performance': decile_returns}

        return size_anomaly

    def detect_momentum_anomaly(self, price_data: pd.DataFrame, lookback_period: int = 252) -> Dict[str, Any]:
        """Detect momentum anomaly"""

        returns = price_data['Close'].pct_change().dropna()

        # Calculate momentum (past year return excluding last month)
        momentum_returns = returns.rolling(lookback_period).apply(lambda x: (1 + x[:-21]).prod() - 1, raw=False)

        # Next month returns
        future_returns = returns.shift(-21).rolling(21).apply(lambda x: (1 + x).prod() - 1, raw=False)

        # Combine data
        momentum_data = pd.DataFrame({
            'momentum': momentum_returns,
            'future_return': future_returns
        }).dropna()

        if len(momentum_data) < 50:
            return {'error': 'Insufficient data for momentum analysis'}

        # Create momentum deciles
        momentum_deciles = pd.qcut(momentum_data['momentum'], 10, labels=False)

        decile_performance = {}
        for decile in range(10):
            decile_mask = momentum_deciles == decile
            decile_future_returns = momentum_data['future_return'][decile_mask]

            if len(decile_future_returns) > 5:
                decile_performance[decile + 1] = {
                    'mean_future_return': decile_future_returns.mean(),
                    'std_future_return': decile_future_returns.std(),
                    'sample_size': len(decile_future_returns)
                }

        # Test winner vs loser portfolio
        if 1 in decile_performance and 10 in decile_performance:
            loser_returns = momentum_data['future_return'][momentum_deciles == 0]
            winner_returns = momentum_data['future_return'][momentum_deciles == 9]

            t_stat, p_value = stats.ttest_ind(winner_returns, loser_returns)

            momentum_test = {
                'winner_mean': winner_returns.mean(),
                'loser_mean': loser_returns.mean(),
                'winner_minus_loser': winner_returns.mean() - loser_returns.mean(),
                't_statistic': t_stat,
                'p_value': p_value,
                'momentum_exists': winner_returns.mean() > loser_returns.mean() and p_value < 0.05
            }
        else:
            momentum_test = {}

        return {
            'decile_performance': decile_performance,
            'momentum_test': momentum_test
        }


def comprehensive_efficiency_analysis(price_data: pd.DataFrame,
                                      event_dates: List[str] = None,
                                      insider_transactions: List[Dict[str, Any]] = None) -> Dict[str, Any]:
    """Perform comprehensive market efficiency analysis"""

    # Initialize testers
    weak_tester = WeakFormEfficiencyTester()
    semi_strong_tester = SemiStrongFormTester()
    strong_tester = StrongFormTester()
    anomaly_detector = MarketAnomalyDetector()

    returns = price_data['Close'].pct_change().dropna()

    results = {
        'data_summary': {
            'start_date': returns.index[0].strftime('%Y-%m-%d'),
            'end_date': returns.index[-1].strftime('%Y-%m-%d'),
            'total_observations': len(returns),
            'mean_return': returns.mean(),
            'volatility': returns.std(),
            'skewness': returns.skew(),
            'kurtosis': returns.kurtosis()
        }
    }

    # Weak-form tests
    results['weak_form'] = {
        'runs_test': weak_tester.runs_test(returns),
        'autocorrelation_tests': weak_tester.autocorrelation_test(returns),
        'variance_ratio_tests': weak_tester.variance_ratio_test(returns)
    }

    # Semi-strong form tests
    if event_dates:
        results['semi_strong'] = {
            'event_study': semi_strong_tester.event_study_analysis(price_data, event_dates)
        }

    # Strong-form tests
    if insider_transactions:
        results['strong_form'] = {
            'insider_trading': strong_tester.insider_trading_analysis(price_data, insider_transactions)
        }

    # Anomaly detection
    results['anomalies'] = {
        'calendar_anomalies': anomaly_detector.detect_calendar_anomalies(price_data),
        'momentum_anomaly': anomaly_detector.detect_momentum_anomaly(price_data)
    }

    return results