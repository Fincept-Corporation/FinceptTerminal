"""
Signal Evaluation

Evaluates the quality and performance of trading signals:
- Statistical validity
- Predictive accuracy
- Risk-adjusted returns
- Signal decay analysis
"""

from typing import Optional, List, Dict, Any
from datetime import datetime, timedelta
import statistics
import uuid

from .base import (
    Evaluator,
    EvaluationResult,
    EvaluationMetric,
    EvaluationLevel,
    MetricType,
)
from ..config import get_config


class SignalEvaluator(Evaluator):
    """
    Evaluates trading signal quality and performance.

    Key metrics:
    - Win rate (signals that were profitable)
    - Average return per signal
    - Information coefficient (IC)
    - Hit rate (signals > threshold)
    - False positive rate
    - Signal-to-noise ratio
    """

    def __init__(self):
        super().__init__(
            name="signal_evaluator",
            description="Evaluates trading signal quality and performance",
            level=EvaluationLevel.SIGNAL,
        )

        # Targets based on RenTech standards
        self.target_win_rate = 0.5075  # "Right 50.75% of the time"
        self.target_ic = 0.05  # Information coefficient
        self.target_sharpe = 2.0  # High Sharpe target
        self.min_p_value = 0.01

    def evaluate(self, context: Dict[str, Any]) -> EvaluationResult:
        """
        Evaluate signal quality.

        Context should contain:
        - signals: List of signal objects with outcomes
        - period: Evaluation period
        """
        signals = context.get("signals", [])
        period = context.get("period", "unknown")

        result = EvaluationResult(
            evaluation_id=f"signal_eval_{uuid.uuid4().hex[:8]}",
            level=self.level,
            subject=f"Signal Quality ({period})",
        )

        if not signals:
            result.summary = "No signals to evaluate"
            result.overall_score = 0
            return result

        # Calculate metrics
        self._evaluate_win_rate(result, signals)
        self._evaluate_returns(result, signals)
        self._evaluate_statistical_quality(result, signals)
        self._evaluate_signal_timing(result, signals)
        self._evaluate_by_type(result, signals)

        # Calculate overall score
        result.calculate_overall_score()

        # Generate summary
        result.summary = self._generate_summary(result)
        result.findings = self._generate_findings(result, signals)
        result.recommendations = self._generate_recommendations(result, signals)

        # Store in history
        self._history.append(result)

        return result

    def _evaluate_win_rate(
        self,
        result: EvaluationResult,
        signals: List[Dict[str, Any]],
    ):
        """Evaluate signal win rate"""
        winning = [s for s in signals if s.get("pnl_bps", 0) > 0]
        losing = [s for s in signals if s.get("pnl_bps", 0) < 0]
        breakeven = [s for s in signals if s.get("pnl_bps", 0) == 0]

        total = len(signals)
        win_rate = len(winning) / total if total > 0 else 0

        result.add_metric(EvaluationMetric(
            name="win_rate",
            value=win_rate,
            metric_type=MetricType.ACCURACY,
            unit="%",
            target=self.target_win_rate,
            warning_threshold=0.50,
            critical_threshold=0.48,
        ))

        # Profit factor
        gross_profit = sum(s.get("pnl_bps", 0) for s in winning)
        gross_loss = abs(sum(s.get("pnl_bps", 0) for s in losing))
        profit_factor = gross_profit / gross_loss if gross_loss > 0 else float('inf')

        result.add_metric(EvaluationMetric(
            name="profit_factor",
            value=min(profit_factor, 10),  # Cap for display
            metric_type=MetricType.PROFIT,
            target=1.5,
            warning_threshold=1.0,
        ))

    def _evaluate_returns(
        self,
        result: EvaluationResult,
        signals: List[Dict[str, Any]],
    ):
        """Evaluate return metrics"""
        returns = [s.get("pnl_bps", 0) for s in signals]

        if not returns:
            return

        # Average return
        avg_return = statistics.mean(returns)
        result.add_metric(EvaluationMetric(
            name="avg_return_bps",
            value=avg_return,
            metric_type=MetricType.PROFIT,
            unit="bps",
            target=10,
            warning_threshold=0,
        ))

        # Return volatility
        if len(returns) > 1:
            return_vol = statistics.stdev(returns)
            result.add_metric(EvaluationMetric(
                name="return_volatility_bps",
                value=return_vol,
                metric_type=MetricType.RISK,
                unit="bps",
            ))

            # Sharpe-like ratio
            sharpe = avg_return / return_vol if return_vol > 0 else 0
            result.add_metric(EvaluationMetric(
                name="signal_sharpe",
                value=sharpe,
                metric_type=MetricType.QUALITY,
                target=self.target_sharpe,
                warning_threshold=1.0,
            ))

        # Max return and max loss
        result.add_metric(EvaluationMetric(
            name="max_return_bps",
            value=max(returns),
            metric_type=MetricType.PROFIT,
            unit="bps",
        ))

        result.add_metric(EvaluationMetric(
            name="max_loss_bps",
            value=min(returns),
            metric_type=MetricType.RISK,
            unit="bps",
        ))

    def _evaluate_statistical_quality(
        self,
        result: EvaluationResult,
        signals: List[Dict[str, Any]],
    ):
        """Evaluate statistical quality of signals"""
        p_values = [s.get("p_value") for s in signals if s.get("p_value") is not None]

        if not p_values:
            return

        # Average p-value
        avg_p_value = statistics.mean(p_values)
        result.add_metric(EvaluationMetric(
            name="avg_p_value",
            value=avg_p_value,
            metric_type=MetricType.QUALITY,
            target=self.min_p_value,
        ))

        # Percentage meeting p-value threshold
        passing = len([p for p in p_values if p < self.min_p_value])
        pass_rate = passing / len(p_values)
        result.add_metric(EvaluationMetric(
            name="p_value_pass_rate",
            value=pass_rate,
            metric_type=MetricType.ACCURACY,
            unit="%",
            target=0.95,
            warning_threshold=0.80,
        ))

        # Information coefficient (correlation between signal and outcome)
        confidences = [s.get("confidence", 0.5) for s in signals]
        returns = [s.get("pnl_bps", 0) for s in signals]

        if len(confidences) > 2:
            try:
                # Simplified IC calculation
                ic = self._calculate_ic(confidences, returns)
                result.add_metric(EvaluationMetric(
                    name="information_coefficient",
                    value=ic,
                    metric_type=MetricType.QUALITY,
                    target=self.target_ic,
                    warning_threshold=0.02,
                ))
            except:
                pass

    def _calculate_ic(self, predictions: List[float], outcomes: List[float]) -> float:
        """Calculate Information Coefficient (Spearman correlation)"""
        n = len(predictions)
        if n < 3:
            return 0

        # Rank both series
        pred_ranks = self._rank(predictions)
        outcome_ranks = self._rank(outcomes)

        # Spearman correlation
        d_squared = sum((p - o) ** 2 for p, o in zip(pred_ranks, outcome_ranks))
        ic = 1 - (6 * d_squared) / (n * (n**2 - 1))

        return ic

    def _rank(self, values: List[float]) -> List[float]:
        """Calculate ranks for a list of values"""
        sorted_idx = sorted(range(len(values)), key=lambda i: values[i])
        ranks = [0] * len(values)
        for rank, idx in enumerate(sorted_idx):
            ranks[idx] = rank + 1
        return ranks

    def _evaluate_signal_timing(
        self,
        result: EvaluationResult,
        signals: List[Dict[str, Any]],
    ):
        """Evaluate signal timing and decay"""
        # Average holding period
        holding_periods = [
            s.get("holding_period_days", 0)
            for s in signals
            if s.get("holding_period_days") is not None
        ]

        if holding_periods:
            avg_holding = statistics.mean(holding_periods)
            result.add_metric(EvaluationMetric(
                name="avg_holding_period_days",
                value=avg_holding,
                metric_type=MetricType.LATENCY,
                unit="days",
            ))

        # Signal age at execution
        ages = [
            s.get("execution_delay_minutes", 0)
            for s in signals
            if s.get("execution_delay_minutes") is not None
        ]

        if ages:
            avg_age = statistics.mean(ages)
            result.add_metric(EvaluationMetric(
                name="avg_execution_delay_minutes",
                value=avg_age,
                metric_type=MetricType.LATENCY,
                unit="minutes",
                target=30,  # Execute within 30 minutes
                warning_threshold=60,
            ))

    def _evaluate_by_type(
        self,
        result: EvaluationResult,
        signals: List[Dict[str, Any]],
    ):
        """Evaluate by signal type"""
        by_type = {}
        for s in signals:
            signal_type = s.get("signal_type", "unknown")
            if signal_type not in by_type:
                by_type[signal_type] = []
            by_type[signal_type].append(s)

        for signal_type, type_signals in by_type.items():
            returns = [s.get("pnl_bps", 0) for s in type_signals]
            winning = len([r for r in returns if r > 0])

            result.add_metric(EvaluationMetric(
                name=f"{signal_type}_win_rate",
                value=winning / len(returns) if returns else 0,
                metric_type=MetricType.ACCURACY,
                unit="%",
            ))

            result.add_metric(EvaluationMetric(
                name=f"{signal_type}_avg_return",
                value=statistics.mean(returns) if returns else 0,
                metric_type=MetricType.PROFIT,
                unit="bps",
            ))

    def _generate_summary(self, result: EvaluationResult) -> str:
        """Generate evaluation summary"""
        win_rate = None
        sharpe = None
        ic = None

        for m in result.metrics:
            if m.name == "win_rate":
                win_rate = m.value
            elif m.name == "signal_sharpe":
                sharpe = m.value
            elif m.name == "information_coefficient":
                ic = m.value

        summary = f"Signal Quality Score: {result.overall_score:.0f}/100 (Grade: {result.grade})\n"

        if win_rate:
            summary += f"Win Rate: {win_rate:.1%} "
            summary += "(PASS)" if win_rate >= self.target_win_rate else "(BELOW TARGET)"
            summary += "\n"

        if sharpe:
            summary += f"Signal Sharpe: {sharpe:.2f}\n"

        if ic:
            summary += f"Information Coefficient: {ic:.3f}\n"

        return summary

    def _generate_findings(
        self,
        result: EvaluationResult,
        signals: List[Dict[str, Any]],
    ) -> List[str]:
        """Generate key findings"""
        findings = []

        for metric in result.metrics:
            if metric.status == "critical":
                findings.append(f"CRITICAL: {metric.name} at {metric.value:.4f} (threshold: {metric.critical_threshold})")
            elif metric.status == "warning":
                findings.append(f"WARNING: {metric.name} at {metric.value:.4f} (threshold: {metric.warning_threshold})")

        return findings

    def _generate_recommendations(
        self,
        result: EvaluationResult,
        signals: List[Dict[str, Any]],
    ) -> List[str]:
        """Generate recommendations"""
        recommendations = []

        for metric in result.metrics:
            if not metric.meets_target and metric.target:
                if "win_rate" in metric.name:
                    recommendations.append("Review signal generation parameters to improve win rate")
                elif "p_value" in metric.name:
                    recommendations.append("Increase statistical significance threshold for signal acceptance")
                elif "sharpe" in metric.name:
                    recommendations.append("Consider reducing position sizes or improving signal quality")

        return recommendations


# Global instance
_signal_evaluator: Optional[SignalEvaluator] = None


def get_signal_evaluator() -> SignalEvaluator:
    """Get the global signal evaluator"""
    global _signal_evaluator
    if _signal_evaluator is None:
        _signal_evaluator = SignalEvaluator()
    return _signal_evaluator
