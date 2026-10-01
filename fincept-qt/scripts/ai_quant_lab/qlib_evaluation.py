"""
AI Quant Lab - Qlib Evaluation Module
Complete implementation of factor and model evaluation including:
- Information Coefficient (IC) Analysis
- Alpha Factor Evaluation
- Risk Metrics and Attribution
- Performance Analysis
- Factor Combination and Selection
"""

import json
import sys
from typing import Dict, List, Any, Optional, Union, Tuple
import warnings
warnings.filterwarnings('ignore')

try:
    import pandas as pd
    import numpy as np
    from scipy import stats
    from scipy.stats import spearmanr, pearsonr
    PANDAS_AVAILABLE = True
except ImportError:
    PANDAS_AVAILABLE = False
    pd = None
    np = None
    stats = None

# Qlib imports
QLIB_AVAILABLE = False
try:
    import qlib
    from qlib.data import D
    QLIB_AVAILABLE = True
except ImportError:
    QLIB_AVAILABLE = False


class EvaluationService:
    """
    Comprehensive evaluation service for factors and models.

    Features:
    - IC/IR/ICIR analysis
    - Risk-adjusted returns
    - Factor combination
    - Performance attribution
    - Statistical significance testing
    """

    def __init__(self):
        self.evaluation_results = {}

    def calculate_ic_metrics(self,
                             predictions: pd.DataFrame,
                             returns: pd.DataFrame,
                             method: str = "pearson") -> Dict[str, Any]:
        """
        Calculate Information Coefficient metrics.

        IC measures the correlation between predictions and actual returns.

        Args:
            predictions: DataFrame with predictions (datetime x instruments)
            returns: DataFrame with actual returns (datetime x instruments)
            method: Correlation method ('pearson' or 'spearman')

        Returns:
            IC metrics including IC, IC_std, ICIR, IC_series
        """
        if not PANDAS_AVAILABLE:
            return {"success": False, "error": "Pandas not available"}

        if isinstance(predictions, pd.Series):
            # The CLI/panel supplies flat lists, not a datetime x instrument frame.
            return self._ic_metrics_1d(predictions, returns, method)

        try:
            # Align data
            common_dates = predictions.index.intersection(returns.index)
            common_instruments = predictions.columns.intersection(returns.columns)

            pred_aligned = predictions.loc[common_dates, common_instruments]
            ret_aligned = returns.loc[common_dates, common_instruments]

            # Calculate IC for each time period
            ic_series = []
            for date in common_dates:
                pred_values = pred_aligned.loc[date].dropna()
                ret_values = ret_aligned.loc[date].dropna()

                # Align instruments
                common_inst = pred_values.index.intersection(ret_values.index)
                if len(common_inst) < 10:  # Need minimum observations
                    continue

                pred_vals = pred_values[common_inst].values
                ret_vals = ret_values[common_inst].values

                if method == "pearson":
                    ic, _ = pearsonr(pred_vals, ret_vals)
                else:  # spearman (rank IC)
                    ic, _ = spearmanr(pred_vals, ret_vals)

                ic_series.append({"date": str(date), "ic": float(ic)})

            if not ic_series:
                return {"success": False, "error": "No valid IC calculations"}

            # Calculate statistics
            ic_values = [x["ic"] for x in ic_series]
            ic_mean = np.mean(ic_values)
            ic_std = np.std(ic_values)
            icir = ic_mean / ic_std if ic_std > 0 else 0

            # Additional metrics
            ic_positive_rate = sum(1 for ic in ic_values if ic > 0) / len(ic_values)
            ic_significance = self._test_ic_significance(ic_values)

            # Rolling statistics
            ic_arr = np.array(ic_values)
            rolling_ic_mean = pd.Series(ic_arr).rolling(window=20).mean().dropna()

            return {
                "success": True,
                "IC_mean": float(ic_mean),
                "IC_std": float(ic_std),
                "ICIR": float(icir),
                "IC_positive_rate": float(ic_positive_rate),
                "IC_max": float(np.max(ic_values)),
                "IC_min": float(np.min(ic_values)),
                "IC_median": float(np.median(ic_values)),
                "IC_skewness": float(stats.skew(ic_values)),
                "IC_kurtosis": float(stats.kurtosis(ic_values)),
                "p_value": ic_significance["p_value"],
                "is_significant": ic_significance["is_significant"],
                "ic_series": ic_series,
                "rolling_ic_mean": rolling_ic_mean.to_dict(),
                "observations": len(ic_values),
                "method": method
            }

        except Exception as e:
            return {"success": False, "error": f"IC calculation failed: {str(e)}"}

    def calculate_rank_ic(self,
                         predictions: pd.DataFrame,
                         returns: pd.DataFrame) -> Dict[str, Any]:
        """
        Calculate Rank IC (Spearman correlation).

        More robust to outliers than Pearson IC.

        Args:
            predictions: Predicted values
            returns: Actual returns

        Returns:
            Rank IC metrics
        """
        return self.calculate_ic_metrics(predictions, returns, method="spearman")

    # ------------------------------------------------------------------
    # Flat-vector (1-D) evaluation -- what the C++ panel actually sends
    # ------------------------------------------------------------------
    # The DataFrame methods above need a datetime x instrument panel. The panel
    # (and the MCP tools) send two flat lists, which used to crash every call with
    # "'Series' object has no attribute 'columns'" and score every factor 0 / "Very
    # Poor". These helpers evaluate one prediction vector against one return vector.

    @staticmethod
    def _paired(predictions, returns) -> Tuple[np.ndarray, np.ndarray]:
        pair = pd.concat([pd.Series(predictions).reset_index(drop=True),
                          pd.Series(returns).reset_index(drop=True)], axis=1).dropna()
        return pair.iloc[:, 0].astype(float).values, pair.iloc[:, 1].astype(float).values

    def _ic_metrics_1d(self, predictions, returns, method: str = "pearson") -> Dict[str, Any]:
        """IC of one prediction vector against realised returns.

        ``IC_mean`` is the full-sample correlation. Dispersion statistics (std, ICIR,
        positive rate, ...) come from a rolling-window IC series and are only
        meaningful with >= 2 windows, so ``rolling_ic_available`` says whether they
        were computed rather than reporting a made-up zero.
        """
        try:
            x, y = self._paired(predictions, returns)
            n = len(x)
            if n < 3:
                return {"success": False,
                        "error": f"IC needs at least 3 paired observations (got {n})"}
            if np.std(x) == 0 or np.std(y) == 0:
                return {"success": False,
                        "error": "IC is undefined: predictions or returns are constant"}

            corr = spearmanr if method == "spearman" else pearsonr
            ic, p_value = corr(x, y)

            window = max(10, n // 5)
            rolling = []
            if n >= 2 * window:
                for end in range(window, n + 1):
                    xs, ys = x[end - window:end], y[end - window:end]
                    if np.std(xs) == 0 or np.std(ys) == 0:
                        continue
                    rolling.append((end, float(corr(xs, ys)[0])))

            result = {
                "success": True,
                "IC_mean": float(ic),
                "p_value": float(p_value),
                "is_significant": bool(p_value < 0.05),
                "observations": int(n),
                "method": method,
                "rolling_window": int(window),
                "rolling_ic_available": len(rolling) >= 2,
            }
            if len(rolling) >= 2:
                vals = np.array([r[1] for r in rolling])
                std = float(np.std(vals))
                result.update({
                    "IC_std": std,
                    "ICIR": float(np.mean(vals) / std) if std > 0 else 0.0,
                    "IC_positive_rate": float(np.mean(vals > 0)),
                    "IC_max": float(np.max(vals)),
                    "IC_min": float(np.min(vals)),
                    "IC_median": float(np.median(vals)),
                    "IC_skewness": float(stats.skew(vals)),
                    "IC_kurtosis": float(stats.kurtosis(vals)),
                    "ic_series": [{"date": f"obs {end}", "ic": v} for end, v in rolling],
                })
            else:
                result.update({
                    "IC_std": 0.0, "ICIR": 0.0, "IC_positive_rate": 0.0,
                    "IC_max": float(ic), "IC_min": float(ic), "IC_median": float(ic),
                    "IC_skewness": 0.0, "IC_kurtosis": 0.0, "ic_series": [],
                    "note": (f"Only {n} observations: rolling-IC statistics (std, ICIR, positive "
                             f"rate) need at least {2 * window}. IC shown is the full-sample correlation."),
                })
            return result
        except Exception as e:
            return {"success": False, "error": f"IC calculation failed: {str(e)}"}

    def _quantiles_1d(self, factor_values, returns, quantiles: int = 5) -> Dict[str, Any]:
        """Bucket observations by factor value (Q1 = lowest) and compare realised returns.

        Orientation: a predictive factor has returns that INCREASE from Q1 to Q<n>,
        so the long-short leg is top bucket minus bottom bucket.
        """
        try:
            x, y = self._paired(factor_values, returns)
            n = len(x)
            quantiles = int(quantiles)
            if n < quantiles * 4:
                return {"success": False,
                        "error": f"Quantile analysis needs at least {quantiles * 4} observations "
                                 f"for {quantiles} buckets (got {n})"}
            labels = pd.qcut(pd.Series(x).rank(method="first"), q=quantiles, labels=False)
            stats_by_q = {}
            means = []
            for q in range(quantiles):
                vals = y[(labels == q).values]
                if len(vals) == 0:
                    continue
                sd = float(np.std(vals))
                stats_by_q[f"Q{q + 1}"] = {
                    "mean_return": float(np.mean(vals)),
                    "std_return": sd,
                    "sharpe": float(np.mean(vals) / sd * np.sqrt(252)) if sd > 0 else 0.0,
                    "win_rate": float(np.mean(vals > 0)),
                    "observations": int(len(vals)),
                }
                means.append(float(np.mean(vals)))
            top = y[(labels == quantiles - 1).values]
            bottom = y[(labels == 0).values]
            spread = float(np.mean(top) - np.mean(bottom))
            pooled = float(np.sqrt((np.var(top) + np.var(bottom)) / 2))
            steps = len(means) - 1
            return {
                "success": True,
                "quantile_stats": stats_by_q,
                "long_short_mean_return": spread,
                "long_short_sharpe": float(spread / pooled * np.sqrt(252)) if pooled > 0 else 0.0,
                "monotonicity": float(sum(1 for i in range(steps) if means[i + 1] > means[i]) / steps)
                                if steps > 0 else 0.0,
                "spread": spread,
            }
        except Exception as e:
            return {"success": False, "error": f"Factor returns analysis failed: {str(e)}"}

    def analyze_factor_returns(self,
                               factor_values: pd.DataFrame,
                               returns: pd.DataFrame,
                               quantiles: int = 5) -> Dict[str, Any]:
        """
        Analyze returns by factor quantiles.

        Divides universe into quantiles based on factor values and analyzes performance.

        Args:
            factor_values: Factor values (datetime x instruments)
            returns: Forward returns (datetime x instruments)
            quantiles: Number of quantiles to create

        Returns:
            Quantile performance analysis
        """
        if not PANDAS_AVAILABLE:
            return {"success": False, "error": "Pandas not available"}

        if isinstance(factor_values, pd.Series):
            return self._quantiles_1d(factor_values, returns, quantiles)

        try:
            common_dates = factor_values.index.intersection(returns.index)

            quantile_returns = {f"Q{i+1}": [] for i in range(quantiles)}

            for date in common_dates:
                factors = factor_values.loc[date].dropna()
                rets = returns.loc[date].dropna()

                common_inst = factors.index.intersection(rets.index)
                if len(common_inst) < quantiles * 5:
                    continue

                factor_vals = factors[common_inst]
                return_vals = rets[common_inst]

                # Create quantiles
                quantile_labels = pd.qcut(factor_vals, q=quantiles, labels=False, duplicates='drop')

                for q in range(quantiles):
                    q_mask = (quantile_labels == q)
                    q_returns = return_vals[q_mask]
                    if len(q_returns) > 0:
                        quantile_returns[f"Q{q+1}"].append(q_returns.mean())

            # Calculate statistics for each quantile
            quantile_stats = {}
            for q_name, q_rets in quantile_returns.items():
                if len(q_rets) > 0:
                    quantile_stats[q_name] = {
                        "mean_return": float(np.mean(q_rets)),
                        "std_return": float(np.std(q_rets)),
                        "sharpe": float(np.mean(q_rets) / np.std(q_rets) * np.sqrt(252)) if np.std(q_rets) > 0 else 0,
                        "win_rate": float(sum(1 for r in q_rets if r > 0) / len(q_rets)),
                        "observations": len(q_rets)
                    }

            # Long-short strategy (Q1 - Q5)
            if len(quantile_returns["Q1"]) > 0 and len(quantile_returns[f"Q{quantiles}"]) > 0:
                long_short_returns = [
                    q1 - q5 for q1, q5 in zip(quantile_returns["Q1"], quantile_returns[f"Q{quantiles}"])
                ]
                long_short_sharpe = np.mean(long_short_returns) / np.std(long_short_returns) * np.sqrt(252) if np.std(long_short_returns) > 0 else 0
            else:
                long_short_sharpe = 0
                long_short_returns = []

            return {
                "success": True,
                "quantile_stats": quantile_stats,
                "long_short_sharpe": float(long_short_sharpe),
                "long_short_mean_return": float(np.mean(long_short_returns)) if long_short_returns else 0,
                "monotonicity": self._check_monotonicity(quantile_stats),
                "spread": float(quantile_stats["Q1"]["mean_return"] - quantile_stats[f"Q{quantiles}"]["mean_return"]) if quantile_stats else 0
            }

        except Exception as e:
            return {"success": False, "error": f"Factor returns analysis failed: {str(e)}"}

    def calculate_factor_turnover(self,
                                  factor_values: pd.DataFrame,
                                  top_n: int = 50) -> Dict[str, Any]:
        """
        Calculate factor turnover (portfolio stability).

        Low turnover = stable factor rankings.

        Args:
            factor_values: Factor values over time
            top_n: Number of top stocks to track

        Returns:
            Turnover metrics
        """
        if not PANDAS_AVAILABLE:
            return {"success": False, "error": "Pandas not available"}

        if isinstance(factor_values, pd.Series):
            return {"success": False,
                    "error": "Turnover needs a datetime x instrument factor panel, not a flat vector"}

        try:
            dates = sorted(factor_values.index)
            turnovers = []

            for i in range(1, len(dates)):
                prev_date = dates[i-1]
                curr_date = dates[i]

                prev_top = set(factor_values.loc[prev_date].nlargest(top_n).index)
                curr_top = set(factor_values.loc[curr_date].nlargest(top_n).index)

                # Turnover = % of portfolio that changed
                turnover = len(prev_top.symmetric_difference(curr_top)) / top_n
                turnovers.append(turnover)

            return {
                "success": True,
                "mean_turnover": float(np.mean(turnovers)),
                "median_turnover": float(np.median(turnovers)),
                "std_turnover": float(np.std(turnovers)),
                "max_turnover": float(np.max(turnovers)),
                "min_turnover": float(np.min(turnovers)),
                "turnover_series": turnovers,
                "observations": len(turnovers)
            }

        except Exception as e:
            return {"success": False, "error": f"Turnover calculation failed: {str(e)}"}

    def calculate_risk_metrics(self,
                               returns: pd.Series,
                               benchmark_returns: Optional[pd.Series] = None,
                               confidence_level: float = 0.95) -> Dict[str, Any]:
        """
        Calculate comprehensive risk metrics.

        Args:
            returns: Return series
            benchmark_returns: Benchmark returns (optional)
            confidence_level: Confidence level for VaR/CVaR

        Returns:
            Risk metrics dict
        """
        if not PANDAS_AVAILABLE:
            return {"success": False, "error": "Pandas not available"}

        try:
            # Volatility
            volatility = returns.std() * np.sqrt(252)

            # Downside deviation
            downside_returns = returns[returns < 0]
            downside_deviation = downside_returns.std() * np.sqrt(252)

            # Value at Risk (VaR)
            var = returns.quantile(1 - confidence_level)

            # Conditional VaR (CVaR / Expected Shortfall)
            cvar = returns[returns <= var].mean()

            # Maximum drawdown
            cumulative = (1 + returns).cumprod()
            running_max = cumulative.expanding().max()
            drawdown = (cumulative - running_max) / running_max
            max_drawdown = drawdown.min()

            # Drawdown duration
            in_drawdown = (drawdown < 0).astype(int)
            drawdown_periods = []
            current_period = 0
            for val in in_drawdown:
                if val == 1:
                    current_period += 1
                else:
                    if current_period > 0:
                        drawdown_periods.append(current_period)
                    current_period = 0
            max_drawdown_duration = max(drawdown_periods) if drawdown_periods else 0

            metrics = {
                "success": True,
                "volatility": float(volatility),
                "downside_deviation": float(downside_deviation),
                "var_95": float(var),
                "cvar_95": float(cvar),
                "max_drawdown": float(max_drawdown),
                "max_drawdown_duration": int(max_drawdown_duration),
                "skewness": float(stats.skew(returns)),
                "kurtosis": float(stats.kurtosis(returns))
            }

            # Benchmark-relative risk metrics
            if benchmark_returns is not None:
                excess_returns = returns - benchmark_returns
                tracking_error = excess_returns.std() * np.sqrt(252)

                # Beta
                covariance = returns.cov(benchmark_returns)
                benchmark_var = benchmark_returns.var()
                beta = covariance / benchmark_var if benchmark_var > 0 else 0

                # Up/Down capture
                up_market = benchmark_returns > 0
                down_market = benchmark_returns < 0

                up_capture = (returns[up_market].mean() / benchmark_returns[up_market].mean()) if benchmark_returns[up_market].mean() != 0 else 0
                down_capture = (returns[down_market].mean() / benchmark_returns[down_market].mean()) if benchmark_returns[down_market].mean() != 0 else 0

                metrics.update({
                    "tracking_error": float(tracking_error),
                    "beta": float(beta),
                    "up_capture": float(up_capture),
                    "down_capture": float(down_capture)
                })

            return metrics

        except Exception as e:
            return {"success": False, "error": f"Risk metrics calculation failed: {str(e)}"}

    def combine_factors(self,
                       factors: Dict[str, pd.DataFrame],
                       weights: Optional[Dict[str, float]] = None,
                       method: str = "equal_weight") -> Dict[str, Any]:
        """
        Combine multiple factors into a composite signal.

        Args:
            factors: Dict of factor name -> factor values DataFrame
            weights: Optional weights for each factor
            method: Combination method ('equal_weight', 'ic_weight', 'optimal')

        Returns:
            Combined factor DataFrame and metadata
        """
        if not PANDAS_AVAILABLE:
            return {"success": False, "error": "Pandas not available"}

        try:
            factor_dfs = list(factors.values())

            if method == "equal_weight":
                # Simple average
                combined = sum(factor_dfs) / len(factor_dfs)
                weights_used = {name: 1.0/len(factors) for name in factors.keys()}

            elif method == "ic_weight" and weights:
                # Weight by IC or provided weights
                combined = sum(df * weights.get(name, 1.0/len(factors))
                             for name, df in factors.items())
                weights_used = weights

            elif method == "optimal":
                # Optimal weights to maximize IC (simplified)
                combined = sum(factor_dfs) / len(factor_dfs)  # Placeholder
                weights_used = {name: 1.0/len(factors) for name in factors.keys()}

            else:
                combined = sum(factor_dfs) / len(factor_dfs)
                weights_used = {name: 1.0/len(factors) for name in factors.keys()}

            return {
                "success": True,
                "combined_factor": combined.to_dict(),
                "weights": weights_used,
                "method": method,
                "num_factors": len(factors)
            }

        except Exception as e:
            return {"success": False, "error": f"Factor combination failed: {str(e)}"}

    def _test_ic_significance(self, ic_series: List[float], alpha: float = 0.05) -> Dict[str, Any]:
        """Test statistical significance of IC"""
        if len(ic_series) < 10:
            return {"is_significant": False, "p_value": 1.0}

        # T-test: H0: mean IC = 0
        t_stat, p_value = stats.ttest_1samp(ic_series, 0)

        return {
            "is_significant": bool(p_value < alpha),
            "p_value": float(p_value),
            "t_statistic": float(t_stat),
            "alpha": alpha
        }

    def _check_monotonicity(self, quantile_stats: Dict[str, Dict]) -> float:
        """Check if returns are monotonic across quantiles"""
        if not quantile_stats:
            return 0.0

        returns = [stats["mean_return"] for stats in quantile_stats.values()]

        # Count monotonic pairs
        monotonic_count = 0
        total_pairs = len(returns) - 1

        for i in range(len(returns) - 1):
            if returns[i] > returns[i+1]:
                monotonic_count += 1

        return monotonic_count / total_pairs if total_pairs > 0 else 0.0

    def generate_evaluation_report(self,
                                   factor_name: str,
                                   predictions: pd.DataFrame,
                                   returns: pd.DataFrame) -> Dict[str, Any]:
        """
        Generate comprehensive evaluation report for a factor.

        Args:
            factor_name: Name of the factor
            predictions: Factor predictions
            returns: Actual returns

        Returns:
            Complete evaluation report
        """
        try:
            report = {
                "factor_name": factor_name,
                "evaluation_date": pd.Timestamp.now().isoformat(),
                "data_range": {
                    "start": str(predictions.index[0]),
                    "end": str(predictions.index[-1]),
                    "periods": len(predictions)
                }
            }

            if isinstance(predictions, pd.Series):
                return self._report_1d(report, predictions, returns)

            # IC Analysis
            ic_metrics = self.calculate_ic_metrics(predictions, returns)
            report["ic_analysis"] = ic_metrics

            # Rank IC
            rank_ic = self.calculate_rank_ic(predictions, returns)
            report["rank_ic_analysis"] = rank_ic

            # Quantile Analysis
            quantile_analysis = self.analyze_factor_returns(predictions, returns)
            report["quantile_analysis"] = quantile_analysis

            # Turnover
            turnover = self.calculate_factor_turnover(predictions)
            report["turnover_analysis"] = turnover

            # Overall score (0-100)
            score = self._calculate_factor_score(ic_metrics, quantile_analysis, turnover)
            report["overall_score"] = score
            report["rating"] = self._get_factor_rating(score)

            return {
                "success": True,
                "report": report
            }

        except Exception as e:
            return {"success": False, "error": f"Report generation failed: {str(e)}"}

    def _report_1d(self, report: Dict[str, Any], predictions, returns) -> Dict[str, Any]:
        """Full evaluation report for flat prediction / return vectors."""
        ic_metrics = self._ic_metrics_1d(predictions, returns, "pearson")
        rank_ic = self._ic_metrics_1d(predictions, returns, "spearman")
        quantile_analysis = self._quantiles_1d(predictions, returns)
        report["ic_analysis"] = ic_metrics
        report["rank_ic_analysis"] = rank_ic
        report["quantile_analysis"] = quantile_analysis
        report["turnover_analysis"] = {}   # undefined without a time x instrument panel

        if not ic_metrics.get("success"):
            return {"success": False, "error": ic_metrics.get("error", "IC calculation failed")}

        # Same point scheme as _calculate_factor_score, but re-based on the components
        # that exist for a flat vector (no turnover; ICIR only with rolling windows),
        # so a good factor is not capped at 60-80 by terms that cannot be computed.
        earned, possible = 0.0, 20.0
        earned += min(abs(ic_metrics.get("IC_mean", 0)) * 400, 20)
        if ic_metrics.get("rolling_ic_available"):
            possible += 20.0
            earned += min(abs(ic_metrics.get("ICIR", 0)) * 5, 20)
        if quantile_analysis.get("success"):
            possible += 40.0
            earned += min(abs(quantile_analysis.get("long_short_sharpe", 0)) * 10, 20)
            earned += quantile_analysis.get("monotonicity", 0) * 20
        score = min(earned / possible * 100.0, 100.0)
        report["overall_score"] = score
        report["rating"] = self._get_factor_rating(score)
        return {"success": True, "report": report}

    def _calculate_factor_score(self,
                                ic_metrics: Dict,
                                quantile_metrics: Dict,
                                turnover_metrics: Dict) -> float:
        """Calculate overall factor quality score (0-100)"""
        score = 0.0

        # IC contribution (40 points)
        if ic_metrics.get("success"):
            ic_mean = ic_metrics.get("IC_mean", 0)
            icir = ic_metrics.get("ICIR", 0)
            score += min(abs(ic_mean) * 400, 20)  # Max 20 for IC
            score += min(abs(icir) * 5, 20)       # Max 20 for ICIR

        # Quantile analysis (40 points)
        if quantile_metrics.get("success"):
            sharpe = quantile_metrics.get("long_short_sharpe", 0)
            monotonicity = quantile_metrics.get("monotonicity", 0)
            score += min(abs(sharpe) * 10, 20)    # Max 20 for Sharpe
            score += monotonicity * 20             # Max 20 for monotonicity

        # Turnover (20 points) - lower is better
        if turnover_metrics.get("success"):
            turnover = turnover_metrics.get("mean_turnover", 1.0)
            score += max(20 - turnover * 40, 0)   # Max 20, decreases with turnover

        return min(score, 100.0)

    def _get_factor_rating(self, score: float) -> str:
        """Convert score to rating"""
        if score >= 80:
            return "Excellent"
        elif score >= 60:
            return "Good"
        elif score >= 40:
            return "Fair"
        elif score >= 20:
            return "Poor"
        else:
            return "Very Poor"


def _json_safe(obj):
    """Replace NaN / +-Infinity with None before json.dumps.

    Python emits them as bare ``NaN`` / ``Infinity`` tokens, which are not JSON: the
    terminal's parser rejects the WHOLE payload ("malformed JSON") over one empty cell.
    """
    if isinstance(obj, float):
        return obj if obj == obj and obj not in (float('inf'), float('-inf')) else None
    if isinstance(obj, dict):
        return {k: _json_safe(v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple)):
        return [_json_safe(v) for v in obj]
    return obj


def main():
    """CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "No command specified"}))
        sys.exit(1)

    command = sys.argv[1]
    service = EvaluationService()

    try:
        params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}

        if command == "check_status":
            result = {
                "success": True,
                "pandas_available": PANDAS_AVAILABLE,
                "qlib_available": QLIB_AVAILABLE
            }

        elif command == "calculate_ic":
            predictions = pd.Series(params.get("predictions", []))
            returns = pd.Series(params.get("returns", []))
            method = params.get("method", "pearson")
            result = service.calculate_ic_metrics(predictions, returns, method)

        elif command == "calculate_rank_ic":
            predictions = pd.Series(params.get("predictions", []))
            returns = pd.Series(params.get("returns", []))
            result = service.calculate_rank_ic(predictions, returns)

        elif command == "analyze_factor_returns":
            factor_values = pd.Series(params.get("factor_values", []))
            returns = pd.Series(params.get("returns", []))
            quantiles = params.get("quantiles", 5)
            result = service.analyze_factor_returns(factor_values, returns, quantiles)

        elif command == "calculate_factor_turnover":
            factor_values = pd.Series(params.get("factor_values", []))
            top_n = params.get("top_n", 50)
            result = service.calculate_factor_turnover(factor_values, top_n)

        elif command == "calculate_risk_metrics":
            returns = pd.Series(params.get("returns", []))
            benchmark_returns = pd.Series(params.get("benchmark_returns")) if params.get("benchmark_returns") else None
            confidence_level = params.get("confidence_level", 0.95)
            result = service.calculate_risk_metrics(returns, benchmark_returns, confidence_level)

        elif command == "combine_factors":
            factors = {k: pd.Series(v) for k, v in params.get("factors", {}).items()}
            weights = params.get("weights")
            method = params.get("method", "equal_weight")
            result = service.combine_factors(factors, weights, method)

        elif command == "generate_evaluation_report":
            factor_name = params.get("factor_name", "factor")
            predictions = pd.Series(params.get("predictions", []))
            returns = pd.Series(params.get("returns", []))
            result = service.generate_evaluation_report(factor_name, predictions, returns)

        else:
            result = {"success": False, "error": f"Unknown command: {command}"}

        print(json.dumps(_json_safe(result)))

    except Exception as e:
        print(json.dumps({"success": False, "error": str(e)}))
        sys.exit(1)


if __name__ == "__main__":
    main()
