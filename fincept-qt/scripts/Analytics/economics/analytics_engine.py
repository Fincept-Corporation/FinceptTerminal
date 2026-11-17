"""
Analytics Engine Module
=======================

Advanced statistical analysis, forecasting, and scenario analysis for economic data.
Provides sophisticated analytical capabilities including time series forecasting,
statistical hypothesis testing, Monte Carlo simulation, and scenario analysis.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Pandas DataFrame/Series with economic/financial data
  - Time series data with datetime index (for time series analysis)
  - Scenario parameters (dictionaries for scenario analysis)
  - Volatility, drift, base values (for Monte Carlo simulation)

OUTPUT:
  - Descriptive statistics (mean, median, variance, percentiles)
  - Correlation matrices with p-values
  - Hypothesis test results (t-tests, normality tests)
  - Time series forecasts (ARIMA, exponential smoothing, linear trend)
  - Monte Carlo simulation results with risk metrics
  - Scenario analysis comparisons and sensitivity results

PARAMETERS:
  - precision: Decimal precision (default: 8)
  - base_currency: Currency for calculations (default: 'USD')
  - confidence_level: Statistical confidence (default: 0.95)
  - forecast_periods: Number of periods to forecast (default: 12)
  - num_simulations: Monte Carlo simulations (default: 1000)
  - distribution: Probability distribution (default: 'normal')
"""

import numpy as np
import pandas as pd
from decimal import Decimal
from typing import Dict, List, Tuple, Optional, Any, Union, Callable
from datetime import datetime, timedelta
import math
import warnings
from scipy import stats
import logging
from .core import EconomicsBase, ValidationError, CalculationError, DataError

logger = logging.getLogger(__name__)

# No process-wide warnings.filterwarnings('ignore') here: a library must not silence warnings
# for its importer. The ARIMA order search silences its own fitting warnings locally.
# (scipy.optimize.minimize and the sklearn imports that used to sit here were never used.)


def _finite(value):
    """float(value) when finite, else None (NaN/inf statistics of degenerate samples)."""
    try:
        v = float(value)
    except (TypeError, ValueError):
        return None
    return v if math.isfinite(v) else None


class StatisticalAnalyzer(EconomicsBase):
    """Advanced statistical analysis for economic data"""

    def __init__(self, precision: int = 8, base_currency: str = 'USD'):
        super().__init__(precision, base_currency)

    def descriptive_statistics(self, data: pd.Series,
                               confidence_level: float = 0.95) -> Dict[str, Any]:
        """Calculate comprehensive descriptive statistics"""

        if data.empty:
            raise ValidationError("Empty data series provided")

        # Remove NaN values
        clean_data = data.dropna()
        if clean_data.empty:
            raise ValidationError("No valid data points after removing NaN values")

        n = len(clean_data)
        if n < 3:
            raise ValidationError("At least 3 observations are needed for descriptive statistics")
        mean = clean_data.mean()
        std = clean_data.std()
        constant = not std > 0

        # Basic statistics
        basic_stats = {
            'count': n,
            'mean': self.to_decimal(mean),
            'median': self.to_decimal(clean_data.median()),
            'mode': self.to_decimal(clean_data.mode().iloc[0]) if not clean_data.mode().empty else None,
            'standard_deviation': self.to_decimal(std),
            'variance': self.to_decimal(clean_data.var()),
            'minimum': self.to_decimal(clean_data.min()),
            'maximum': self.to_decimal(clean_data.max()),
            'range': self.to_decimal(clean_data.max() - clean_data.min())
        }

        # Percentiles
        percentiles = {}
        for p in [1, 5, 10, 25, 50, 75, 90, 95, 99]:
            percentiles[f'p{p}'] = self.to_decimal(clean_data.quantile(p / 100))

        # Shape statistics. pandas Series.kurtosis() is already EXCESS kurtosis (normal = 0);
        # Pearson kurtosis is that + 3. Undefined (None) for a constant series or n < 4.
        skew = None if constant else _finite(clean_data.skew())
        excess_kurt = None if constant else _finite(clean_data.kurtosis())
        shape_stats = {
            'skewness': self.to_decimal(skew) if skew is not None else None,
            'kurtosis': self.to_decimal(excess_kurt + 3) if excess_kurt is not None else None,
            'excess_kurtosis': self.to_decimal(excess_kurt) if excess_kurt is not None else None
        }

        # Confidence intervals
        alpha = 1 - confidence_level
        t_critical = stats.t.ppf(1 - alpha / 2, n - 1)
        margin_of_error = t_critical * (std / np.sqrt(n))

        confidence_intervals = {
            'mean_ci_lower': self.to_decimal(mean - margin_of_error),
            'mean_ci_upper': self.to_decimal(mean + margin_of_error),
            'confidence_level': self.to_decimal(confidence_level)
        }

        # Normality tests (undefined for a constant series)
        normality_tests = (self._test_normality(clean_data) if not constant
                           else {'error': 'constant series: normality tests are undefined'})

        return {
            'basic_statistics': basic_stats,
            'percentiles': percentiles,
            'shape_statistics': shape_stats,
            'confidence_intervals': confidence_intervals,
            'normality_tests': normality_tests,
            'outlier_analysis': self._analyze_outliers(clean_data)
        }

    def _test_normality(self, data: pd.Series) -> Dict[str, Any]:
        """Test for normality using multiple tests"""

        results = {}

        # Shapiro-Wilk test (best for small samples)
        if len(data) <= 5000:
            shapiro_stat, shapiro_p = stats.shapiro(data)
            results['shapiro_wilk'] = {
                'statistic': self.to_decimal(shapiro_stat),
                'p_value': self.to_decimal(shapiro_p),
                'is_normal': shapiro_p > 0.05
            }

        # Kolmogorov-Smirnov test
        ks_stat, ks_p = stats.kstest(data, 'norm', args=(data.mean(), data.std()))
        results['kolmogorov_smirnov'] = {
            'statistic': self.to_decimal(ks_stat),
            'p_value': self.to_decimal(ks_p),
            'is_normal': ks_p > 0.05
        }

        # Anderson-Darling test
        ad_stat, ad_critical, ad_significance = stats.anderson(data, dist='norm')
        results['anderson_darling'] = {
            'statistic': self.to_decimal(ad_stat),
            'critical_values': [self.to_decimal(cv) for cv in ad_critical],
            'significance_levels': [self.to_decimal(sl) for sl in ad_significance],
            'is_normal': ad_stat < ad_critical[2]  # 5% significance level
        }

        return results

    def _analyze_outliers(self, data: pd.Series) -> Dict[str, Any]:
        """Analyze outliers using multiple methods"""

        # IQR method
        Q1 = data.quantile(0.25)
        Q3 = data.quantile(0.75)
        IQR = Q3 - Q1
        lower_bound = Q1 - 1.5 * IQR
        upper_bound = Q3 + 1.5 * IQR
        iqr_outliers = ((data < lower_bound) | (data > upper_bound)).sum()

        # Z-score method (no outliers by definition when the series is constant)
        sd = data.std()
        if sd > 0:
            z_scores = np.abs((data - data.mean()) / sd)
            zscore_outliers = (z_scores > 3).sum()
        else:
            zscore_outliers = 0

        # Modified Z-score method (Iglewicz-Hoaglin); undefined when the MAD is 0
        median = data.median()
        mad = np.median(np.abs(data - median))
        if mad > 0:
            modified_z_scores = 0.6745 * (data - median) / mad
            modified_zscore_outliers = (np.abs(modified_z_scores) > 3.5).sum()
        else:
            modified_zscore_outliers = 0

        return {
            'iqr_method': {
                'outlier_count': int(iqr_outliers),
                'outlier_percentage': self.to_decimal((iqr_outliers / len(data)) * 100),
                'lower_bound': self.to_decimal(lower_bound),
                'upper_bound': self.to_decimal(upper_bound)
            },
            'zscore_method': {
                'outlier_count': int(zscore_outliers),
                'outlier_percentage': self.to_decimal((zscore_outliers / len(data)) * 100),
                'threshold': self.to_decimal(3)
            },
            'modified_zscore_method': {
                'outlier_count': int(modified_zscore_outliers),
                'outlier_percentage': self.to_decimal((modified_zscore_outliers / len(data)) * 100),
                'threshold': self.to_decimal(3.5)
            }
        }

    def correlation_analysis(self, data: pd.DataFrame,
                             method: str = 'pearson') -> Dict[str, Any]:
        """Comprehensive correlation analysis"""

        if data.empty:
            raise ValidationError("Empty dataframe provided")

        # Select only numeric columns
        numeric_data = data.select_dtypes(include=[np.number])
        if numeric_data.empty:
            raise ValidationError("No numeric columns found in data")

        # Calculate correlation matrix
        if method.lower() == 'pearson':
            corr_matrix = numeric_data.corr(method='pearson')
        elif method.lower() == 'spearman':
            corr_matrix = numeric_data.corr(method='spearman')
        elif method.lower() == 'kendall':
            corr_matrix = numeric_data.corr(method='kendall')
        else:
            raise ValidationError(f"Unknown correlation method: {method}")

        # Calculate p-values for correlations
        p_values = self._calculate_correlation_pvalues(numeric_data, method)

        # Find significant correlations
        significant_correlations = self._find_significant_correlations(
            corr_matrix, p_values, alpha=0.05
        )

        # Identify highest correlations
        highest_correlations = self._find_highest_correlations(corr_matrix, top_n=10)

        return {
            'correlation_matrix': corr_matrix.round(4).to_dict(),
            'p_values': p_values,
            'method': method,
            'significant_correlations': significant_correlations,
            'highest_correlations': highest_correlations,
            'summary_statistics': {
                'mean_correlation': self.to_decimal(
                    corr_matrix.values[np.triu_indices_from(corr_matrix.values, k=1)].mean()),
                'max_correlation': self.to_decimal(
                    corr_matrix.values[np.triu_indices_from(corr_matrix.values, k=1)].max()),
                'min_correlation': self.to_decimal(
                    corr_matrix.values[np.triu_indices_from(corr_matrix.values, k=1)].min())
            }
        }

    def _calculate_correlation_pvalues(self, data: pd.DataFrame, method: str) -> Dict[str, Dict[str, float]]:
        """Calculate p-values for correlation matrix"""

        columns = data.columns
        p_values = {}

        for i, col1 in enumerate(columns):
            p_values[col1] = {}
            for j, col2 in enumerate(columns):
                if i == j:
                    p_values[col1][col2] = 0.0
                else:
                    if method.lower() == 'pearson':
                        _, p_val = stats.pearsonr(data[col1].dropna(), data[col2].dropna())
                    elif method.lower() == 'spearman':
                        _, p_val = stats.spearmanr(data[col1].dropna(), data[col2].dropna())
                    elif method.lower() == 'kendall':
                        _, p_val = stats.kendalltau(data[col1].dropna(), data[col2].dropna())

                    p_values[col1][col2] = float(p_val)

        return p_values

    def _find_significant_correlations(self, corr_matrix: pd.DataFrame,
                                       p_values: Dict[str, Dict[str, float]],
                                       alpha: float = 0.05) -> List[Dict[str, Any]]:
        """Find statistically significant correlations"""

        significant = []
        columns = corr_matrix.columns

        for i, col1 in enumerate(columns):
            for j, col2 in enumerate(columns):
                if i < j:  # Avoid duplicates
                    corr = corr_matrix.loc[col1, col2]
                    p_val = p_values[col1][col2]

                    if p_val < alpha:
                        significant.append({
                            'variable_1': col1,
                            'variable_2': col2,
                            'correlation': self.to_decimal(corr),
                            'p_value': self.to_decimal(p_val),
                            'significance_level': alpha
                        })

        # Sort by absolute correlation
        significant.sort(key=lambda x: abs(x['correlation']), reverse=True)

        return significant

    def _find_highest_correlations(self, corr_matrix: pd.DataFrame,
                                   top_n: int = 10) -> List[Dict[str, Any]]:
        """Find highest absolute correlations"""

        correlations = []
        columns = corr_matrix.columns

        for i, col1 in enumerate(columns):
            for j, col2 in enumerate(columns):
                if i < j:  # Avoid duplicates
                    corr = corr_matrix.loc[col1, col2]
                    correlations.append({
                        'variable_1': col1,
                        'variable_2': col2,
                        'correlation': self.to_decimal(corr),
                        'absolute_correlation': self.to_decimal(abs(corr))
                    })

        # Sort by absolute correlation and return top N
        correlations.sort(key=lambda x: x['absolute_correlation'], reverse=True)

        return correlations[:top_n]

    def hypothesis_testing(self, data1: pd.Series, data2: Optional[pd.Series] = None,
                           test_type: str = 'one_sample_t',
                           alternative: str = 'two-sided',
                           alpha: float = 0.05,
                           null_value: float = 0) -> Dict[str, Any]:
        """Comprehensive hypothesis testing"""

        if data1.empty:
            raise ValidationError("Empty data series provided")

        # Clean data
        clean_data1 = data1.dropna()
        if clean_data1.empty:
            raise ValidationError("No valid data points in first series")

        test_results = {}

        if test_type == 'one_sample_t':
            # One-sample t-test
            statistic, p_value = stats.ttest_1samp(clean_data1, null_value)

            test_results = {
                'test_type': 'One-sample t-test',
                'null_hypothesis': f'Population mean equals {null_value}',
                'alternative_hypothesis': self._format_alternative_hypothesis('mean', null_value, alternative),
                'test_statistic': self.to_decimal(statistic),
                'p_value': self.to_decimal(p_value),
                'degrees_of_freedom': len(clean_data1) - 1,
                'sample_mean': self.to_decimal(clean_data1.mean()),
                'sample_size': len(clean_data1)
            }

        elif test_type == 'two_sample_t':
            if data2 is None:
                raise ValidationError("Second data series required for two-sample t-test")

            clean_data2 = data2.dropna()
            if clean_data2.empty:
                raise ValidationError("No valid data points in second series")

            # Two-sample t-test (assuming unequal variances)
            statistic, p_value = stats.ttest_ind(clean_data1, clean_data2, equal_var=False)

            test_results = {
                'test_type': 'Two-sample t-test (Welch)',
                'null_hypothesis': 'Population means are equal',
                'alternative_hypothesis': self._format_alternative_hypothesis('means', 0, alternative),
                'test_statistic': self.to_decimal(statistic),
                'p_value': self.to_decimal(p_value),
                'sample_1_mean': self.to_decimal(clean_data1.mean()),
                'sample_2_mean': self.to_decimal(clean_data2.mean()),
                'sample_1_size': len(clean_data1),
                'sample_2_size': len(clean_data2)
            }

        elif test_type == 'paired_t':
            if data2 is None:
                raise ValidationError("Second data series required for paired t-test")

            # Align the series and remove NaN pairs
            aligned_data = pd.DataFrame({'data1': data1, 'data2': data2}).dropna()
            if aligned_data.empty:
                raise ValidationError("No valid paired observations")

            statistic, p_value = stats.ttest_rel(aligned_data['data1'], aligned_data['data2'])

            test_results = {
                'test_type': 'Paired t-test',
                'null_hypothesis': 'Mean difference equals zero',
                'alternative_hypothesis': self._format_alternative_hypothesis('difference', 0, alternative),
                'test_statistic': self.to_decimal(statistic),
                'p_value': self.to_decimal(p_value),
                'degrees_of_freedom': len(aligned_data) - 1,
                'mean_difference': self.to_decimal((aligned_data['data1'] - aligned_data['data2']).mean()),
                'sample_size': len(aligned_data)
            }

        elif test_type == 'z_test':
            # One-sample z-test (requires known population standard deviation)
            pop_std = null_value  # Reuse null_value parameter for population std
            if pop_std <= 0:
                raise ValidationError("Population standard deviation must be positive for z-test")

            sample_mean = clean_data1.mean()
            n = len(clean_data1)
            z_statistic = (sample_mean - null_value) / (pop_std / np.sqrt(n))

            if alternative == 'two-sided':
                p_value = 2 * (1 - stats.norm.cdf(abs(z_statistic)))
            elif alternative == 'greater':
                p_value = 1 - stats.norm.cdf(z_statistic)
            else:  # 'less'
                p_value = stats.norm.cdf(z_statistic)

            test_results = {
                'test_type': 'One-sample z-test',
                'null_hypothesis': f'Population mean equals {null_value}',
                'alternative_hypothesis': self._format_alternative_hypothesis('mean', null_value, alternative),
                'test_statistic': self.to_decimal(z_statistic),
                'p_value': self.to_decimal(p_value),
                'sample_mean': self.to_decimal(sample_mean),
                'population_std': self.to_decimal(pop_std),
                'sample_size': n
            }

        else:
            raise ValidationError(f"Unknown test type: {test_type}")

        # Add common results
        test_results.update({
            'alpha': self.to_decimal(alpha),
            'alternative': alternative,
            'reject_null': test_results['p_value'] < alpha,
            'conclusion': self._generate_test_conclusion(test_results['p_value'], alpha,
                                                         test_results['null_hypothesis'])
        })

        return test_results

    def _format_alternative_hypothesis(self, parameter: str, null_value: float, alternative: str) -> str:
        """Format alternative hypothesis string"""

        if alternative == 'two-sided':
            return f'Population {parameter} does not equal {null_value}'
        elif alternative == 'greater':
            return f'Population {parameter} is greater than {null_value}'
        elif alternative == 'less':
            return f'Population {parameter} is less than {null_value}'
        else:
            return f'Alternative hypothesis with {alternative} direction'

    def _generate_test_conclusion(self, p_value: Decimal, alpha: float, null_hypothesis: str) -> str:
        """Generate conclusion from hypothesis test"""

        if p_value < alpha:
            return f"Reject null hypothesis: {null_hypothesis} (p-value = {p_value:.6f} < α = {alpha})"
        else:
            return f"Fail to reject null hypothesis: {null_hypothesis} (p-value = {p_value:.6f} ≥ α = {alpha})"

    def time_series_analysis(self, data: pd.Series) -> Dict[str, Any]:
        """Basic time series analysis"""

        if not isinstance(data.index, pd.DatetimeIndex):
            raise ValidationError("Data must have datetime index for time series analysis")

        if data.empty:
            raise ValidationError("Empty time series provided")

        # Clean data
        clean_data = data.dropna()
        if len(clean_data) < 10:
            raise ValidationError("Insufficient data points for time series analysis")

        results = {
            'basic_properties': {
                'start_date': clean_data.index.min(),
                'end_date': clean_data.index.max(),
                'frequency': pd.infer_freq(clean_data.index),
                'total_observations': len(clean_data),
                'missing_observations': len(data) - len(clean_data)
            },
            'stationarity_tests': self._test_stationarity(clean_data),
            'autocorrelation': self._calculate_autocorrelation(clean_data),
            'trend_analysis': self._analyze_trend(clean_data),
            'seasonality_analysis': self._analyze_seasonality(clean_data)
        }

        return results

    def _test_stationarity(self, data: pd.Series) -> Dict[str, Any]:
        """Test for stationarity using Augmented Dickey-Fuller test"""

        from statsmodels.tsa.stattools import adfuller

        try:
            result = adfuller(data.values, autolag='AIC')

            return {
                'adf_statistic': self.to_decimal(result[0]),
                'p_value': self.to_decimal(result[1]),
                'critical_values': {
                    '1%': self.to_decimal(result[4]['1%']),
                    '5%': self.to_decimal(result[4]['5%']),
                    '10%': self.to_decimal(result[4]['10%'])
                },
                'is_stationary': result[1] < 0.05,
                'interpretation': 'Stationary' if result[1] < 0.05 else 'Non-stationary'
            }
        except Exception as e:
            logger.warning(f"Could not perform ADF test: {e}")
            return {'error': 'Could not perform stationarity test'}

    def _calculate_autocorrelation(self, data: pd.Series, max_lags: int = 20) -> Dict[str, Any]:
        """Calculate autocorrelation function"""

        try:
            from statsmodels.tsa.stattools import acf

            # Limit max_lags to reasonable value
            max_lags = min(max_lags, len(data) // 4)

            autocorr = acf(data.values, nlags=max_lags, alpha=0.05)

            return {
                'autocorrelations': [self.to_decimal(x) for x in autocorr[0]],
                'confidence_intervals': {
                    'lower': [self.to_decimal(x) for x in autocorr[1][:, 0]],
                    'upper': [self.to_decimal(x) for x in autocorr[1][:, 1]]
                },
                'significant_lags': [i for i, ac in enumerate(autocorr[0])
                                     if abs(ac) > 2 / np.sqrt(len(data)) and i > 0]
            }
        except Exception as e:
            logger.warning(f"Could not calculate autocorrelation: {e}")
            return {'error': 'Could not calculate autocorrelation'}

    def _analyze_trend(self, data: pd.Series) -> Dict[str, Any]:
        """Analyze trend in time series"""

        # Linear trend using OLS
        x = np.arange(len(data))
        y = data.values

        slope, intercept, r_value, p_value, std_err = stats.linregress(x, y)

        # Calculate trend strength
        trend_strength = abs(r_value)

        return {
            'linear_trend': {
                'slope': self.to_decimal(slope),
                'intercept': self.to_decimal(intercept),
                'r_squared': self.to_decimal(r_value ** 2),
                'p_value': self.to_decimal(p_value),
                'standard_error': self.to_decimal(std_err)
            },
            'trend_direction': 'Increasing' if slope > 0 else 'Decreasing' if slope < 0 else 'Flat',
            'trend_strength': self.to_decimal(trend_strength),
            'trend_significance': p_value < 0.05
        }

    def _analyze_seasonality(self, data: pd.Series) -> Dict[str, Any]:
        """Analyze seasonality in time series"""

        if len(data) < 24:  # Need at least 2 years of monthly data
            return {'error': 'Insufficient data for seasonality analysis'}

        try:
            from statsmodels.tsa.seasonal import seasonal_decompose

            # Infer frequency for decomposition
            freq = pd.infer_freq(data.index)
            if freq is None:
                # Try to infer period based on data length
                if len(data) >= 365:
                    period = 365  # Daily data, yearly seasonality
                elif len(data) >= 52:
                    period = 52  # Weekly data, yearly seasonality
                elif len(data) >= 12:
                    period = 12  # Monthly data, yearly seasonality
                else:
                    period = 4  # Quarterly data
            else:
                period = None

            decomposition = seasonal_decompose(data, model='additive', period=period)

            # Calculate seasonality strength
            seasonal_var = np.var(decomposition.seasonal.dropna())
            residual_var = np.var(decomposition.resid.dropna())
            seasonality_strength = seasonal_var / (seasonal_var + residual_var)

            return {
                'seasonality_detected': seasonality_strength > 0.1,
                'seasonality_strength': self.to_decimal(seasonality_strength),
                'seasonal_period': period,
                'components_variance': {
                    'trend': self.to_decimal(np.var(decomposition.trend.dropna())),
                    'seasonal': self.to_decimal(seasonal_var),
                    'residual': self.to_decimal(residual_var)
                }
            }
        except Exception as e:
            logger.warning(f"Could not perform seasonality analysis: {e}")
            return {'error': 'Could not perform seasonality analysis'}

    def calculate(self, analysis_type: str, **kwargs) -> Dict[str, Any]:
        """Main statistical analysis dispatcher"""

        analyses = {
            'descriptive_statistics': lambda: self.descriptive_statistics(
                kwargs['data'], kwargs.get('confidence_level', 0.95)
            ),
            'correlation_analysis': lambda: self.correlation_analysis(
                kwargs['data'], kwargs.get('method', 'pearson')
            ),
            'hypothesis_testing': lambda: self.hypothesis_testing(
                kwargs['data1'], kwargs.get('data2'), kwargs.get('test_type', 'one_sample_t'),
                kwargs.get('alternative', 'two-sided'), kwargs.get('alpha', 0.05),
                kwargs.get('null_value', 0)
            ),
            'time_series_analysis': lambda: self.time_series_analysis(kwargs['data'])
        }

        if analysis_type not in analyses:
            raise ValidationError(f"Unknown analysis type: {analysis_type}")

        result = analyses[analysis_type]()
        result['metadata'] = self.get_metadata()
        result['analysis_type'] = analysis_type

        return result


class ScenarioAnalyzer(EconomicsBase):
    """Scenario analysis and Monte Carlo simulation"""

    def __init__(self, precision: int = 8, base_currency: str = 'USD'):
        super().__init__(precision, base_currency)

    def monte_carlo_simulation(self, base_value: float,
                               volatility: float,
                               drift: float = 0.0,
                               time_periods: int = 252,
                               num_simulations: int = 1000,
                               distribution: str = 'normal',
                               horizon_years: float = 1.0,
                               seed: Optional[int] = 42) -> Dict[str, Any]:
        """Monte Carlo simulation of a geometric Brownian motion.
        drift and volatility are annual; the horizon is horizon_years split into time_periods
        steps of dt = horizon_years / time_periods. Exact log-normal step
        S_{t+dt} = S_t exp((mu - sigma^2/2) dt + sigma sqrt(dt) z), so prices stay positive.
        Student-t shocks (df 5) are scaled to unit variance. VaR / expected shortfall are LOSSES
        (base_value minus the tail level / tail mean), positive numbers."""

        if num_simulations < 100:
            raise ValidationError("Number of simulations must be at least 100")

        if time_periods < 1:
            raise ValidationError("Time periods must be positive")
        if base_value <= 0:
            raise ValidationError("base_value must be positive for a geometric Brownian motion")
        if volatility < 0:
            raise ValidationError("volatility must be non-negative")
        if horizon_years <= 0:
            raise ValidationError("horizon_years must be positive")

        rng = np.random.default_rng(seed)  # local generator: does not reseed numpy's global RNG

        # Generate random shocks (unit variance)
        if distribution.lower() == 'normal':
            random_shocks = rng.standard_normal((num_simulations, time_periods))
        elif distribution.lower() == 'student_t':
            df = 5  # degrees of freedom for t-distribution
            random_shocks = rng.standard_t(df, (num_simulations, time_periods)) * np.sqrt((df - 2) / df)
        else:
            raise ValidationError(f"Unknown distribution: {distribution}")

        dt = horizon_years / time_periods
        log_steps = (drift - 0.5 * volatility ** 2) * dt + volatility * np.sqrt(dt) * random_shocks
        simulations = np.empty((num_simulations, time_periods + 1))
        simulations[:, 0] = base_value
        simulations[:, 1:] = base_value * np.exp(np.cumsum(log_steps, axis=1))

        # Calculate statistics
        final_values = simulations[:, -1]

        percentiles = {}
        for p in [1, 5, 10, 25, 50, 75, 90, 95, 99]:
            percentiles[f'p{p}'] = self.to_decimal(np.percentile(final_values, p))

        # Tail levels of the final value, and the corresponding losses
        level_95 = np.percentile(final_values, 5)
        level_99 = np.percentile(final_values, 1)
        tail_95 = np.mean(final_values[final_values <= level_95])
        tail_99 = np.mean(final_values[final_values <= level_99])

        # Path statistics: drawdown from the running peak
        max_values = np.max(simulations, axis=1)
        min_values = np.min(simulations, axis=1)
        drawdowns = 1.0 - simulations / np.maximum.accumulate(simulations, axis=1)
        max_drawdowns = np.max(drawdowns, axis=1)

        return {
            'simulation_parameters': {
                'base_value': self.to_decimal(base_value),
                'volatility': self.to_decimal(volatility),
                'drift': self.to_decimal(drift),
                'time_periods': time_periods,
                'horizon_years': self.to_decimal(horizon_years),
                'num_simulations': num_simulations,
                'distribution': distribution,
                'seed': seed
            },
            'final_value_statistics': {
                'mean': self.to_decimal(np.mean(final_values)),
                'median': self.to_decimal(np.median(final_values)),
                'std': self.to_decimal(np.std(final_values)),
                'min': self.to_decimal(np.min(final_values)),
                'max': self.to_decimal(np.max(final_values)),
                'percentiles': percentiles
            },
            'risk_metrics': {
                'var_95': self.to_decimal(base_value - level_95),
                'var_99': self.to_decimal(base_value - level_99),
                'expected_shortfall_95': self.to_decimal(base_value - tail_95),
                'expected_shortfall_99': self.to_decimal(base_value - tail_99),
                'final_value_p5': self.to_decimal(level_95),
                'final_value_p1': self.to_decimal(level_99),
                'risk_metric_basis': 'losses vs base_value over the horizon (positive = loss)',
                'probability_of_loss': self.to_decimal(np.mean(final_values < base_value) * 100)
            },
            'path_statistics': {
                'max_value_mean': self.to_decimal(np.mean(max_values)),
                'min_value_mean': self.to_decimal(np.mean(min_values)),
                'max_drawdown_mean': self.to_decimal(np.mean(max_drawdowns) * 100),
                'max_drawdown_unit': '% from the running peak'
            }
        }

    def scenario_analysis(self, base_case: Dict[str, float],
                          scenarios: Dict[str, Dict[str, float]],
                          model_function: Callable,
                          sensitivity_vars: List[str] = None) -> Dict[str, Any]:
        """Comprehensive scenario analysis"""

        if not scenarios:
            raise ValidationError("At least one scenario must be provided")

        # Calculate base case
        try:
            base_result = model_function(**base_case)
        except Exception as e:
            raise CalculationError(f"Error calculating base case: {e}")

        # Calculate scenarios
        scenario_results = {}
        for scenario_name, scenario_params in scenarios.items():
            try:
                # Merge base case with scenario parameters
                scenario_inputs = {**base_case, **scenario_params}
                scenario_result = model_function(**scenario_inputs)
                scenario_results[scenario_name] = scenario_result
            except Exception as e:
                logger.error(f"Error calculating scenario {scenario_name}: {e}")
                scenario_results[scenario_name] = {'error': str(e)}

        # Sensitivity analysis if requested
        sensitivity_results = None
        if sensitivity_vars:
            sensitivity_results = self._sensitivity_analysis(
                base_case, model_function, sensitivity_vars
            )

        return {
            'base_case': {
                'inputs': base_case,
                'result': base_result
            },
            'scenarios': {
                name: {
                    'inputs': {**base_case, **scenarios[name]},
                    'result': result
                }
                for name, result in scenario_results.items()
            },
            'sensitivity_analysis': sensitivity_results,
            'scenario_comparison': self._compare_scenarios(base_result, scenario_results)
        }

    def _sensitivity_analysis(self, base_case: Dict[str, float],
                              model_function: Callable,
                              variables: List[str],
                              shock_size: float = 0.1) -> Dict[str, Any]:
        """Perform sensitivity analysis"""

        sensitivity_results = {}

        for var in variables:
            if var not in base_case:
                logger.warning(f"Variable {var} not found in base case")
                continue

            base_value = base_case[var]

            # Positive shock
            shocked_inputs_pos = base_case.copy()
            shocked_inputs_pos[var] = base_value * (1 + shock_size)

            # Negative shock
            shocked_inputs_neg = base_case.copy()
            shocked_inputs_neg[var] = base_value * (1 - shock_size)

            try:
                base_result = model_function(**base_case)
                result_pos = model_function(**shocked_inputs_pos)
                result_neg = model_function(**shocked_inputs_neg)

                # Calculate elasticity (percentage change in result / percentage change in input)
                if isinstance(base_result, dict):
                    # If result is a dictionary, calculate sensitivity for each output
                    var_sensitivity = {}
                    for output_var, base_output in base_result.items():
                        if isinstance(base_output, (int, float)):
                            elasticity_pos = ((result_pos[output_var] - base_output) / base_output) / shock_size
                            elasticity_neg = ((result_neg[output_var] - base_output) / base_output) / (-shock_size)
                            avg_elasticity = (elasticity_pos + elasticity_neg) / 2

                            var_sensitivity[output_var] = {
                                'elasticity': self.to_decimal(avg_elasticity),
                                'positive_shock_result': self.to_decimal(result_pos[output_var]),
                                'negative_shock_result': self.to_decimal(result_neg[output_var])
                            }
                else:
                    # Single output value
                    elasticity_pos = ((result_pos - base_result) / base_result) / shock_size
                    elasticity_neg = ((result_neg - base_result) / base_result) / (-shock_size)
                    avg_elasticity = (elasticity_pos + elasticity_neg) / 2

                    var_sensitivity = {
                        'elasticity': self.to_decimal(avg_elasticity),
                        'positive_shock_result': self.to_decimal(result_pos),
                        'negative_shock_result': self.to_decimal(result_neg)
                    }

                sensitivity_results[var] = var_sensitivity

            except Exception as e:
                logger.error(f"Error in sensitivity analysis for {var}: {e}")
                sensitivity_results[var] = {'error': str(e)}

        return sensitivity_results

    def _compare_scenarios(self, base_result: Any, scenario_results: Dict[str, Any]) -> Dict[str, Any]:
        """Compare scenario results to base case"""

        comparisons = {}

        for scenario_name, scenario_result in scenario_results.items():
            if 'error' in str(scenario_result):
                comparisons[scenario_name] = {'error': 'Could not compare due to calculation error'}
                continue

            try:
                if isinstance(base_result, dict) and isinstance(scenario_result, dict):
                    # Compare dictionary results
                    scenario_comparison = {}
                    for key in base_result.keys():
                        if key in scenario_result and isinstance(base_result[key], (int, float)):
                            base_val = base_result[key]
                            scenario_val = scenario_result[key]

                            absolute_change = scenario_val - base_val
                            percentage_change = (absolute_change / base_val * 100) if base_val != 0 else 0

                            scenario_comparison[key] = {
                                'base_value': self.to_decimal(base_val),
                                'scenario_value': self.to_decimal(scenario_val),
                                'absolute_change': self.to_decimal(absolute_change),
                                'percentage_change': self.to_decimal(percentage_change)
                            }

                    comparisons[scenario_name] = scenario_comparison

                elif isinstance(base_result, (int, float)) and isinstance(scenario_result, (int, float)):
                    # Compare single values
                    absolute_change = scenario_result - base_result
                    percentage_change = (absolute_change / base_result * 100) if base_result != 0 else 0

                    comparisons[scenario_name] = {
                        'base_value': self.to_decimal(base_result),
                        'scenario_value': self.to_decimal(scenario_result),
                        'absolute_change': self.to_decimal(absolute_change),
                        'percentage_change': self.to_decimal(percentage_change)
                    }

            except Exception as e:
                logger.error(f"Error comparing scenario {scenario_name}: {e}")
                comparisons[scenario_name] = {'error': str(e)}

        return comparisons

    def stress_testing(self, base_parameters: Dict[str, float],
                       stress_scenarios: Dict[str, Dict[str, float]],
                       model_function: Callable,
                       risk_thresholds: Dict[str, float] = None) -> Dict[str, Any]:
        """Comprehensive stress testing"""

        # Standard stress scenarios if none provided
        if not stress_scenarios:
            stress_scenarios = {
                'mild_stress': {var: val * 0.9 for var, val in base_parameters.items()},
                'moderate_stress': {var: val * 0.8 for var, val in base_parameters.items()},
                'severe_stress': {var: val * 0.7 for var, val in base_parameters.items()},
                'extreme_stress': {var: val * 0.5 for var, val in base_parameters.items()}
            }

        # Run stress scenarios
        stress_results = {}
        base_result = model_function(**base_parameters)

        for stress_name, stress_params in stress_scenarios.items():
            try:
                stressed_inputs = {**base_parameters, **stress_params}
                stress_result = model_function(**stressed_inputs)
                stress_results[stress_name] = stress_result
            except Exception as e:
                logger.error(f"Error in stress scenario {stress_name}: {e}")
                stress_results[stress_name] = {'error': str(e)}

        # Assess risk threshold breaches
        threshold_analysis = None
        if risk_thresholds:
            threshold_analysis = self._analyze_threshold_breaches(
                base_result, stress_results, risk_thresholds
            )

        return {
            'base_case': base_result,
            'stress_scenarios': stress_results,
            'threshold_analysis': threshold_analysis,
            'stress_summary': self._summarize_stress_results(base_result, stress_results)
        }

    def _analyze_threshold_breaches(self, base_result: Any,
                                    stress_results: Dict[str, Any],
                                    thresholds: Dict[str, float]) -> Dict[str, Any]:
        """Analyze threshold breaches in stress scenarios"""

        breaches = {}

        for scenario_name, scenario_result in stress_results.items():
            if 'error' in str(scenario_result):
                continue

            scenario_breaches = {}

            if isinstance(scenario_result, dict):
                for var, threshold in thresholds.items():
                    if var in scenario_result:
                        value = scenario_result[var]
                        breach = value < threshold
                        scenario_breaches[var] = {
                            'threshold': self.to_decimal(threshold),
                            'actual_value': self.to_decimal(value),
                            'breach': breach,
                            'distance_from_threshold': self.to_decimal(value - threshold)
                        }

            breaches[scenario_name] = scenario_breaches

        return breaches

    def _summarize_stress_results(self, base_result: Any,
                                  stress_results: Dict[str, Any]) -> Dict[str, Any]:
        """Summarize stress testing results"""

        if isinstance(base_result, dict):
            # Multi-variable case
            summary = {}
            for var in base_result.keys():
                if isinstance(base_result[var], (int, float)):
                    var_results = []
                    for scenario_result in stress_results.values():
                        if isinstance(scenario_result, dict) and var in scenario_result:
                            var_results.append(scenario_result[var])

                    if var_results:
                        summary[var] = {
                            'base_value': self.to_decimal(base_result[var]),
                            'worst_case': self.to_decimal(min(var_results)),
                            'best_case': self.to_decimal(max(var_results)),
                            'average_stress': self.to_decimal(np.mean(var_results)),
                            'max_decline': self.to_decimal(base_result[var] - min(var_results))
                        }
            return summary
        else:
            # Single variable case
            valid_results = [r for r in stress_results.values() if 'error' not in str(r)]

            if valid_results:
                return {
                    'base_value': self.to_decimal(base_result),
                    'worst_case': self.to_decimal(min(valid_results)),
                    'best_case': self.to_decimal(max(valid_results)),
                    'average_stress': self.to_decimal(np.mean(valid_results)),
                    'max_decline': self.to_decimal(base_result - min(valid_results))
                }

            return {'error': 'No valid stress results to summarize'}

    def calculate(self, analysis_type: str, **kwargs) -> Dict[str, Any]:
        """Main scenario analysis dispatcher"""

        analyses = {
            'monte_carlo': lambda: self.monte_carlo_simulation(
                kwargs['base_value'], kwargs['volatility'], kwargs.get('drift', 0.0),
                kwargs.get('time_periods', 252), kwargs.get('num_simulations', 1000),
                kwargs.get('distribution', 'normal'), kwargs.get('horizon_years', 1.0),
                kwargs.get('seed', 42)
            ),
            'scenario_analysis': lambda: self.scenario_analysis(
                kwargs['base_case'], kwargs['scenarios'], kwargs['model_function'],
                kwargs.get('sensitivity_vars')
            ),
            'stress_testing': lambda: self.stress_testing(
                kwargs['base_parameters'], kwargs.get('stress_scenarios', {}),
                kwargs['model_function'], kwargs.get('risk_thresholds')
            )
        }

        if analysis_type not in analyses:
            raise ValidationError(f"Unknown analysis type: {analysis_type}")

        result = analyses[analysis_type]()
        result['metadata'] = self.get_metadata()
        result['analysis_type'] = analysis_type

        return result


class ForecastingEngine(EconomicsBase):
    """Economic forecasting using various methods"""

    def __init__(self, precision: int = 8, base_currency: str = 'USD'):
        super().__init__(precision, base_currency)

    def simple_forecasting_methods(self, data: pd.Series,
                                   forecast_periods: int = 12,
                                   methods: List[str] = None) -> Dict[str, Any]:
        """Simple forecasting methods for economic time series"""

        if data.empty:
            raise ValidationError("Empty time series provided")

        # Clean data
        clean_data = data.dropna()
        if len(clean_data) < 5:
            raise ValidationError("Insufficient data for forecasting")

        if methods is None:
            methods = ['naive', 'mean', 'linear_trend', 'exponential_smoothing']

        forecasts = {}

        for method in methods:
            try:
                if method == 'naive':
                    forecasts[method] = self._naive_forecast(clean_data, forecast_periods)
                elif method == 'mean':
                    forecasts[method] = self._mean_forecast(clean_data, forecast_periods)
                elif method == 'linear_trend':
                    forecasts[method] = self._linear_trend_forecast(clean_data, forecast_periods)
                elif method == 'exponential_smoothing':
                    forecasts[method] = self._exponential_smoothing_forecast(clean_data, forecast_periods)
                elif method == 'moving_average':
                    forecasts[method] = self._moving_average_forecast(clean_data, forecast_periods)
                else:
                    logger.warning(f"Unknown forecasting method: {method}")

            except Exception as e:
                logger.error(f"Error in {method} forecasting: {e}")
                forecasts[method] = {'error': str(e)}

        # Generate forecast evaluation if we have enough data
        evaluation = None
        if len(clean_data) > forecast_periods * 2:
            evaluation = self._evaluate_forecasting_methods(clean_data, forecast_periods, methods)

        return {
            'forecasts': forecasts,
            'forecast_periods': forecast_periods,
            'data_length': len(clean_data),
            'methods_used': methods,
            'evaluation': evaluation
        }

    def _naive_forecast(self, data: pd.Series, periods: int) -> Dict[str, Any]:
        """Naive forecast - last value carried forward"""

        last_value = data.iloc[-1]
        forecast_values = [last_value] * periods

        return {
            'method': 'Naive (Random Walk)',
            'forecast_values': [self.to_decimal(x) for x in forecast_values],
            'description': 'Last observed value carried forward'
        }

    def _mean_forecast(self, data: pd.Series, periods: int) -> Dict[str, Any]:
        """Mean forecast - historical mean"""

        mean_value = data.mean()
        forecast_values = [mean_value] * periods

        return {
            'method': 'Historical Mean',
            'forecast_values': [self.to_decimal(x) for x in forecast_values],
            'description': 'Historical average carried forward'
        }

    def _linear_trend_forecast(self, data: pd.Series, periods: int) -> Dict[str, Any]:
        """Linear trend extrapolation"""

        # Linear trend using OLS
        x = np.arange(len(data))
        y = data.values

        # Fit linear regression
        slope, intercept, r_value, p_value, std_err = stats.linregress(x, y)

        # Generate forecasts
        forecast_x = np.arange(len(data), len(data) + periods)
        forecast_values = slope * forecast_x + intercept

        return {
            'method': 'Linear Trend',
            'forecast_values': [self.to_decimal(x) for x in forecast_values],
            'slope': self.to_decimal(slope),
            'intercept': self.to_decimal(intercept),
            'r_squared': self.to_decimal(r_value ** 2),
            'description': 'Linear trend extrapolation'
        }

    def _exponential_smoothing_forecast(self, data: pd.Series, periods: int) -> Dict[str, Any]:
        """Simple exponential smoothing"""

        try:
            from statsmodels.tsa.holtwinters import ExponentialSmoothing

            # Fit exponential smoothing model
            model = ExponentialSmoothing(data.values, trend=None, seasonal=None)
            fitted_model = model.fit()

            # Generate forecasts
            forecast_values = fitted_model.forecast(periods)

            return {
                'method': 'Exponential Smoothing',
                'forecast_values': [self.to_decimal(x) for x in forecast_values],
                'alpha': self.to_decimal(fitted_model.params['smoothing_level']),
                'description': 'Simple exponential smoothing'
            }
        except Exception as e:
            # Fallback to manual exponential smoothing
            alpha = 0.3  # Default smoothing parameter

            # Initialize with first observation
            smoothed_values = [data.iloc[0]]

            # Calculate smoothed values
            for i in range(1, len(data)):
                smoothed_value = alpha * data.iloc[i] + (1 - alpha) * smoothed_values[-1]
                smoothed_values.append(smoothed_value)

            # Forecast using last smoothed value
            last_smoothed = smoothed_values[-1]
            forecast_values = [last_smoothed] * periods

            return {
                'method': 'Exponential Smoothing (Manual)',
                'forecast_values': [self.to_decimal(x) for x in forecast_values],
                'alpha': self.to_decimal(alpha),
                'description': 'Manual exponential smoothing implementation'
            }

    def _moving_average_forecast(self, data: pd.Series, periods: int, window: int = None) -> Dict[str, Any]:
        """Moving average forecast"""

        if window is None:
            window = min(12, len(data) // 4)  # Default to 1/4 of data length, max 12

        window = max(1, min(window, len(data)))  # Ensure valid window size

        # Calculate moving average
        ma_values = data.rolling(window=window).mean()
        last_ma = ma_values.iloc[-1]

        # Forecast using last moving average
        forecast_values = [last_ma] * periods

        return {
            'method': f'Moving Average ({window} periods)',
            'forecast_values': [self.to_decimal(x) for x in forecast_values],
            'window_size': window,
            'description': f'{window}-period moving average'
        }

    def _evaluate_forecasting_methods(self, data: pd.Series, forecast_periods: int,
                                      methods: List[str]) -> Dict[str, Any]:
        """Evaluate forecasting methods using holdout sample"""

        # Split data for evaluation
        train_size = len(data) - forecast_periods
        train_data = data.iloc[:train_size]
        test_data = data.iloc[train_size:]

        evaluation_results = {}

        for method in methods:
            try:
                # Generate forecast on training data
                if method == 'naive':
                    forecast = self._naive_forecast(train_data, forecast_periods)
                elif method == 'mean':
                    forecast = self._mean_forecast(train_data, forecast_periods)
                elif method == 'linear_trend':
                    forecast = self._linear_trend_forecast(train_data, forecast_periods)
                elif method == 'exponential_smoothing':
                    forecast = self._exponential_smoothing_forecast(train_data, forecast_periods)
                elif method == 'moving_average':
                    forecast = self._moving_average_forecast(train_data, forecast_periods)
                else:
                    continue

                if 'error' in forecast:
                    continue

                # Calculate forecast errors
                forecast_values = [float(x) for x in forecast['forecast_values']]
                actual_values = test_data.values

                # Ensure same length
                min_length = min(len(forecast_values), len(actual_values))
                forecast_values = forecast_values[:min_length]
                actual_values = actual_values[:min_length]

                if min_length == 0:
                    continue

                # Calculate error metrics
                mae = np.mean(np.abs(np.array(actual_values) - np.array(forecast_values)))
                mse = np.mean((np.array(actual_values) - np.array(forecast_values)) ** 2)
                rmse = np.sqrt(mse)

                # Mean Absolute Percentage Error (undefined when an actual value is 0)
                actual_arr = np.array(actual_values, dtype=float)
                mape = (np.mean(np.abs((actual_arr - np.array(forecast_values)) / actual_arr)) * 100
                        if np.all(actual_arr != 0) else None)

                evaluation_results[method] = {
                    'mae': self.to_decimal(mae),
                    'mse': self.to_decimal(mse),
                    'rmse': self.to_decimal(rmse),
                    'mape': self.to_decimal(mape) if mape is not None else None
                }

            except Exception as e:
                logger.error(f"Error evaluating {method}: {e}")
                evaluation_results[method] = {'error': str(e)}

        # Find best method based on RMSE
        if evaluation_results:
            best_method = min(evaluation_results.keys(),
                              key=lambda k: evaluation_results[k].get('rmse', float('inf'))
                              if 'error' not in evaluation_results[k] else float('inf'))

            return {
                'method_performance': evaluation_results,
                'best_method': best_method,
                'evaluation_period': forecast_periods
            }

        return {'error': 'Could not evaluate any methods'}

    def arima_forecast(self, data: pd.Series, forecast_periods: int = 12,
                       order: Tuple[int, int, int] = None,
                       auto_arima: bool = True) -> Dict[str, Any]:
        """ARIMA forecasting"""

        if data.empty:
            raise ValidationError("Empty time series provided")

        clean_data = data.dropna()
        if len(clean_data) < 20:
            raise ValidationError("Insufficient data for ARIMA modeling (need at least 20 observations)")

        try:
            from statsmodels.tsa.arima.model import ARIMA

            if auto_arima and order is None:
                # Simple auto-ARIMA. AIC is only comparable between models fitted to the same
                # (equally differenced) series, so d is chosen first by an ADF unit-root test
                # (p < 0.05 -> d = 0, else d = 1) and only p, q are searched by AIC.
                from statsmodels.tsa.stattools import adfuller
                try:
                    d = 0 if adfuller(clean_data.values, autolag='AIC')[1] < 0.05 else 1
                except Exception:
                    d = 1
                best_aic = float('inf')
                best_order = (1, d, 1)

                with warnings.catch_warnings():
                    warnings.simplefilter('ignore')  # convergence chatter of the search fits only
                    for p in range(0, 4):
                        for q in range(0, 4):
                            try:
                                fitted_model = ARIMA(clean_data, order=(p, d, q)).fit()
                                if fitted_model.aic < best_aic:
                                    best_aic = fitted_model.aic
                                    best_order = (p, d, q)
                            except Exception:
                                continue

                order = best_order
            elif order is None:
                order = (1, 1, 1)  # Default ARIMA(1,1,1)

            # Fit ARIMA model
            model = ARIMA(clean_data, order=order)
            fitted_model = model.fit()

            # Generate forecasts
            forecast_result = fitted_model.forecast(steps=forecast_periods)
            confidence_intervals = fitted_model.get_forecast(steps=forecast_periods).conf_int()

            return {
                'method': f'ARIMA{order}',
                'forecast_values': [self.to_decimal(x) for x in forecast_result],
                'confidence_intervals': {
                    'lower': [self.to_decimal(x) for x in confidence_intervals.iloc[:, 0]],
                    'upper': [self.to_decimal(x) for x in confidence_intervals.iloc[:, 1]]
                },
                'model_order': order,
                'aic': self.to_decimal(fitted_model.aic),
                'bic': self.to_decimal(fitted_model.bic),
                'log_likelihood': self.to_decimal(fitted_model.llf),
                'description': f'ARIMA({order[0]},{order[1]},{order[2]}) model'
            }

        except ImportError:
            raise DataError("statsmodels package required for ARIMA forecasting")
        except Exception as e:
            raise CalculationError(f"ARIMA forecasting failed: {e}")

    def calculate(self, forecast_type: str, **kwargs) -> Dict[str, Any]:
        """Main forecasting dispatcher"""

        forecasts = {
            'simple_methods': lambda: self.simple_forecasting_methods(
                kwargs['data'], kwargs.get('forecast_periods', 12),
                kwargs.get('methods')
            ),
            'arima': lambda: self.arima_forecast(
                kwargs['data'], kwargs.get('forecast_periods', 12),
                kwargs.get('order'), kwargs.get('auto_arima', True)
            )
        }

        if forecast_type not in forecasts:
            raise ValidationError(f"Unknown forecast type: {forecast_type}")

        result = forecasts[forecast_type]()
        result['metadata'] = self.get_metadata()
        result['forecast_type'] = forecast_type

        return result
