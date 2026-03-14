"""
Trade Evaluation

Evaluates trade execution and outcomes:
- Execution quality
- Slippage analysis
- Market impact assessment
- P&L attribution
"""

from typing import Optional, List, Dict, Any
from datetime import datetime
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


class TradeEvaluator(Evaluator):
    """
    Evaluates trade execution quality and outcomes.

    Key metrics:
    - Execution quality (vs arrival, vs VWAP)
    - Slippage
    - Market impact
    - Fill rate
    - P&L analysis
    """

    def __init__(self):
        super().__init__(
            name="trade_evaluator",
            description="Evaluates trade execution and outcomes",
            level=EvaluationLevel.TRADE,
        )

    def evaluate(self, context: Dict[str, Any]) -> EvaluationResult:
        """
        Evaluate trade execution.

        Context should contain:
        - trades: List of trade objects with execution details
        - period: Evaluation period
        """
        trades = context.get("trades", [])
        period = context.get("period", "unknown")

        result = EvaluationResult(
            evaluation_id=f"trade_eval_{uuid.uuid4().hex[:8]}",
            level=self.level,
            subject=f"Trade Execution ({period})",
        )

        if not trades:
            result.summary = "No trades to evaluate"
            result.overall_score = 0
            return result

        # Evaluate different aspects
        self._evaluate_execution_quality(result, trades)
        self._evaluate_slippage(result, trades)
        self._evaluate_market_impact(result, trades)
        self._evaluate_fill_quality(result, trades)
        self._evaluate_pnl(result, trades)
        self._evaluate_by_algorithm(result, trades)

        # Calculate overall score
        result.calculate_overall_score()

        # Generate summary
        result.summary = self._generate_summary(result, trades)
        result.findings = self._generate_findings(result)
        result.recommendations = self._generate_recommendations(result)

        self._history.append(result)
        return result

    def _evaluate_execution_quality(
        self,
        result: EvaluationResult,
        trades: List[Dict[str, Any]],
    ):
        """Evaluate execution quality benchmarks"""
        # vs Arrival Price
        vs_arrival = [
            t.get("vs_arrival_bps", 0)
            for t in trades
            if t.get("vs_arrival_bps") is not None
        ]

        if vs_arrival:
            avg_vs_arrival = statistics.mean(vs_arrival)
            result.add_metric(EvaluationMetric(
                name="avg_vs_arrival_bps",
                value=avg_vs_arrival,
                metric_type=MetricType.QUALITY,
                unit="bps",
                target=-1,  # Want to beat arrival by 1 bps
                warning_threshold=2,  # Warning if 2 bps worse
                critical_threshold=5,  # Critical if 5 bps worse
            ))

        # vs VWAP
        vs_vwap = [
            t.get("vs_vwap_bps", 0)
            for t in trades
            if t.get("vs_vwap_bps") is not None
        ]

        if vs_vwap:
            avg_vs_vwap = statistics.mean(vs_vwap)
            result.add_metric(EvaluationMetric(
                name="avg_vs_vwap_bps",
                value=avg_vs_vwap,
                metric_type=MetricType.QUALITY,
                unit="bps",
                target=0,  # Match VWAP
                warning_threshold=3,
            ))

        # Implementation shortfall
        shortfall = [
            t.get("implementation_shortfall_bps", 0)
            for t in trades
            if t.get("implementation_shortfall_bps") is not None
        ]

        if shortfall:
            avg_shortfall = statistics.mean(shortfall)
            result.add_metric(EvaluationMetric(
                name="avg_implementation_shortfall_bps",
                value=avg_shortfall,
                metric_type=MetricType.QUALITY,
                unit="bps",
                target=5,
                warning_threshold=10,
                critical_threshold=20,
            ))

    def _evaluate_slippage(
        self,
        result: EvaluationResult,
        trades: List[Dict[str, Any]],
    ):
        """Evaluate slippage"""
        slippage = [
            t.get("slippage_bps", 0)
            for t in trades
            if t.get("slippage_bps") is not None
        ]

        if slippage:
            avg_slippage = statistics.mean(slippage)
            result.add_metric(EvaluationMetric(
                name="avg_slippage_bps",
                value=avg_slippage,
                metric_type=MetricType.QUALITY,
                unit="bps",
                target=2,
                warning_threshold=5,
                critical_threshold=10,
            ))

            # Slippage volatility
            if len(slippage) > 1:
                slippage_vol = statistics.stdev(slippage)
                result.add_metric(EvaluationMetric(
                    name="slippage_volatility_bps",
                    value=slippage_vol,
                    metric_type=MetricType.RISK,
                    unit="bps",
                ))

    def _evaluate_market_impact(
        self,
        result: EvaluationResult,
        trades: List[Dict[str, Any]],
    ):
        """Evaluate market impact"""
        impact = [
            t.get("market_impact_bps", 0)
            for t in trades
            if t.get("market_impact_bps") is not None
        ]

        if impact:
            avg_impact = statistics.mean(impact)
            result.add_metric(EvaluationMetric(
                name="avg_market_impact_bps",
                value=avg_impact,
                metric_type=MetricType.RISK,
                unit="bps",
                target=5,
                warning_threshold=10,
                critical_threshold=20,
            ))

            # Max impact
            max_impact = max(impact)
            result.add_metric(EvaluationMetric(
                name="max_market_impact_bps",
                value=max_impact,
                metric_type=MetricType.RISK,
                unit="bps",
            ))

    def _evaluate_fill_quality(
        self,
        result: EvaluationResult,
        trades: List[Dict[str, Any]],
    ):
        """Evaluate fill quality"""
        fill_rates = [
            t.get("fill_rate", 1.0)
            for t in trades
            if t.get("fill_rate") is not None
        ]

        if fill_rates:
            avg_fill_rate = statistics.mean(fill_rates)
            result.add_metric(EvaluationMetric(
                name="avg_fill_rate",
                value=avg_fill_rate,
                metric_type=MetricType.ACCURACY,
                unit="%",
                target=0.98,
                warning_threshold=0.95,
                critical_threshold=0.90,
            ))

        # Execution time
        exec_times = [
            t.get("execution_time_minutes", 0)
            for t in trades
            if t.get("execution_time_minutes") is not None
        ]

        if exec_times:
            avg_exec_time = statistics.mean(exec_times)
            result.add_metric(EvaluationMetric(
                name="avg_execution_time_minutes",
                value=avg_exec_time,
                metric_type=MetricType.LATENCY,
                unit="minutes",
            ))

    def _evaluate_pnl(
        self,
        result: EvaluationResult,
        trades: List[Dict[str, Any]],
    ):
        """Evaluate P&L"""
        pnl_bps = [
            t.get("pnl_bps", 0)
            for t in trades
            if t.get("pnl_bps") is not None
        ]

        if pnl_bps:
            # Total P&L
            total_pnl = sum(pnl_bps)
            result.add_metric(EvaluationMetric(
                name="total_pnl_bps",
                value=total_pnl,
                metric_type=MetricType.PROFIT,
                unit="bps",
            ))

            # Average P&L per trade
            avg_pnl = statistics.mean(pnl_bps)
            result.add_metric(EvaluationMetric(
                name="avg_pnl_per_trade_bps",
                value=avg_pnl,
                metric_type=MetricType.PROFIT,
                unit="bps",
                target=5,
                warning_threshold=0,
            ))

            # Win rate
            winners = len([p for p in pnl_bps if p > 0])
            win_rate = winners / len(pnl_bps)
            result.add_metric(EvaluationMetric(
                name="trade_win_rate",
                value=win_rate,
                metric_type=MetricType.ACCURACY,
                unit="%",
                target=0.5075,
            ))

    def _evaluate_by_algorithm(
        self,
        result: EvaluationResult,
        trades: List[Dict[str, Any]],
    ):
        """Evaluate by execution algorithm"""
        by_algo = {}
        for t in trades:
            algo = t.get("algorithm", "unknown")
            if algo not in by_algo:
                by_algo[algo] = []
            by_algo[algo].append(t)

        for algo, algo_trades in by_algo.items():
            vs_arrival = [
                t.get("vs_arrival_bps", 0)
                for t in algo_trades
                if t.get("vs_arrival_bps") is not None
            ]

            if vs_arrival:
                result.add_metric(EvaluationMetric(
                    name=f"{algo}_avg_vs_arrival_bps",
                    value=statistics.mean(vs_arrival),
                    metric_type=MetricType.QUALITY,
                    unit="bps",
                ))

    def _generate_summary(
        self,
        result: EvaluationResult,
        trades: List[Dict[str, Any]],
    ) -> str:
        """Generate evaluation summary"""
        total_trades = len(trades)
        total_notional = sum(t.get("notional", 0) for t in trades)

        summary = f"""
Trade Execution Evaluation
==========================
Total Trades: {total_trades:,}
Total Notional: ${total_notional:,.0f}
Overall Score: {result.overall_score:.0f}/100 (Grade: {result.grade})

""".strip()

        # Add key metrics
        for m in result.metrics:
            if m.name in ["avg_vs_arrival_bps", "avg_slippage_bps", "avg_market_impact_bps"]:
                status = "PASS" if m.meets_target else "FAIL"
                summary += f"\n{m.name}: {m.value:.2f} bps ({status})"

        return summary

    def _generate_findings(self, result: EvaluationResult) -> List[str]:
        """Generate key findings"""
        findings = []

        for m in result.metrics:
            if m.status == "critical":
                findings.append(f"CRITICAL: {m.name} = {m.value:.2f} {m.unit}")
            elif m.status == "warning":
                findings.append(f"WARNING: {m.name} = {m.value:.2f} {m.unit}")

        return findings

    def _generate_recommendations(self, result: EvaluationResult) -> List[str]:
        """Generate recommendations"""
        recommendations = []

        for m in result.metrics:
            if not m.meets_target:
                if "slippage" in m.name:
                    recommendations.append("Consider more passive execution algorithms")
                elif "impact" in m.name:
                    recommendations.append("Reduce participation rate or extend execution window")
                elif "fill_rate" in m.name:
                    recommendations.append("Review order pricing and urgency settings")

        return list(set(recommendations))


# Global instance
_trade_evaluator: Optional[TradeEvaluator] = None


def get_trade_evaluator() -> TradeEvaluator:
    """Get the global trade evaluator"""
    global _trade_evaluator
    if _trade_evaluator is None:
        _trade_evaluator = TradeEvaluator()
    return _trade_evaluator
