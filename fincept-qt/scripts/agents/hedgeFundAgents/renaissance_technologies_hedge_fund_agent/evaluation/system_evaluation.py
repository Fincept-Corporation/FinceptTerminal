"""
System Evaluation

Evaluates overall system health and performance:
- System uptime and reliability
- Resource utilization
- Component health
- End-to-end latency
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


class SystemEvaluator(Evaluator):
    """
    Evaluates overall system health and performance.

    Key metrics:
    - System availability
    - End-to-end latency
    - Throughput
    - Error rates
    - Resource utilization
    """

    def __init__(self):
        super().__init__(
            name="system_evaluator",
            description="Evaluates overall system health and performance",
            level=EvaluationLevel.SYSTEM,
        )

    def evaluate(self, context: Dict[str, Any]) -> EvaluationResult:
        """
        Evaluate system health.

        Context should contain:
        - uptime_pct: System uptime percentage
        - requests: List of request/response data
        - errors: List of errors
        - resources: Resource utilization data
        - components: Component health data
        """
        result = EvaluationResult(
            evaluation_id=f"system_eval_{uuid.uuid4().hex[:8]}",
            level=self.level,
            subject="System Health",
        )

        # Evaluate different aspects
        self._evaluate_availability(result, context)
        self._evaluate_latency(result, context)
        self._evaluate_throughput(result, context)
        self._evaluate_errors(result, context)
        self._evaluate_resources(result, context)
        self._evaluate_components(result, context)

        # Calculate overall score
        result.calculate_overall_score()

        # Generate summary
        result.summary = self._generate_summary(result)
        result.findings = self._generate_findings(result)
        result.recommendations = self._generate_recommendations(result)

        self._history.append(result)
        return result

    def _evaluate_availability(
        self,
        result: EvaluationResult,
        context: Dict[str, Any],
    ):
        """Evaluate system availability"""
        uptime_pct = context.get("uptime_pct", 100)

        result.add_metric(EvaluationMetric(
            name="system_uptime",
            value=uptime_pct,
            metric_type=MetricType.ACCURACY,
            unit="%",
            target=99.9,  # 3 nines
            warning_threshold=99.5,
            critical_threshold=99.0,
        ))

        # Calculate downtime
        downtime_minutes = (100 - uptime_pct) / 100 * 24 * 60  # Per day
        result.add_metric(EvaluationMetric(
            name="estimated_daily_downtime_minutes",
            value=downtime_minutes,
            metric_type=MetricType.LATENCY,
            unit="minutes",
        ))

    def _evaluate_latency(
        self,
        result: EvaluationResult,
        context: Dict[str, Any],
    ):
        """Evaluate system latency"""
        requests = context.get("requests", [])

        latencies = [
            r.get("latency_ms", 0)
            for r in requests
            if r.get("latency_ms") is not None
        ]

        if not latencies:
            return

        # Average latency
        avg_latency = statistics.mean(latencies)
        result.add_metric(EvaluationMetric(
            name="avg_latency_ms",
            value=avg_latency,
            metric_type=MetricType.LATENCY,
            unit="ms",
            target=100,  # 100ms target
            warning_threshold=500,
            critical_threshold=1000,
        ))

        # P95 latency
        sorted_latencies = sorted(latencies)
        p95_idx = int(len(sorted_latencies) * 0.95)
        p95_latency = sorted_latencies[p95_idx] if sorted_latencies else 0

        result.add_metric(EvaluationMetric(
            name="p95_latency_ms",
            value=p95_latency,
            metric_type=MetricType.LATENCY,
            unit="ms",
            target=500,
            warning_threshold=1000,
        ))

        # P99 latency
        p99_idx = int(len(sorted_latencies) * 0.99)
        p99_latency = sorted_latencies[p99_idx] if sorted_latencies else 0

        result.add_metric(EvaluationMetric(
            name="p99_latency_ms",
            value=p99_latency,
            metric_type=MetricType.LATENCY,
            unit="ms",
        ))

    def _evaluate_throughput(
        self,
        result: EvaluationResult,
        context: Dict[str, Any],
    ):
        """Evaluate system throughput"""
        requests = context.get("requests", [])
        time_window_seconds = context.get("time_window_seconds", 3600)  # Default 1 hour

        if requests and time_window_seconds > 0:
            throughput = len(requests) / time_window_seconds * 60  # Per minute
            result.add_metric(EvaluationMetric(
                name="requests_per_minute",
                value=throughput,
                metric_type=MetricType.THROUGHPUT,
                unit="req/min",
            ))

        # Signals processed
        signals_processed = context.get("signals_processed", 0)
        if signals_processed:
            result.add_metric(EvaluationMetric(
                name="signals_processed",
                value=signals_processed,
                metric_type=MetricType.THROUGHPUT,
            ))

        # Trades executed
        trades_executed = context.get("trades_executed", 0)
        if trades_executed:
            result.add_metric(EvaluationMetric(
                name="trades_executed",
                value=trades_executed,
                metric_type=MetricType.THROUGHPUT,
            ))

    def _evaluate_errors(
        self,
        result: EvaluationResult,
        context: Dict[str, Any],
    ):
        """Evaluate error rates"""
        errors = context.get("errors", [])
        requests = context.get("requests", [])

        total_requests = len(requests)
        total_errors = len(errors)

        if total_requests > 0:
            error_rate = total_errors / total_requests
            result.add_metric(EvaluationMetric(
                name="error_rate",
                value=error_rate,
                metric_type=MetricType.ACCURACY,
                unit="%",
                target=0.001,  # 0.1% target
                warning_threshold=0.01,
                critical_threshold=0.05,
            ))

        # Error breakdown by type
        if errors:
            error_types = {}
            for e in errors:
                e_type = e.get("type", "unknown")
                error_types[e_type] = error_types.get(e_type, 0) + 1

            for e_type, count in error_types.items():
                result.add_metric(EvaluationMetric(
                    name=f"errors_{e_type}",
                    value=count,
                    metric_type=MetricType.QUALITY,
                ))

    def _evaluate_resources(
        self,
        result: EvaluationResult,
        context: Dict[str, Any],
    ):
        """Evaluate resource utilization"""
        resources = context.get("resources", {})

        # CPU utilization
        cpu_pct = resources.get("cpu_percent", 0)
        if cpu_pct:
            result.add_metric(EvaluationMetric(
                name="cpu_utilization",
                value=cpu_pct,
                metric_type=MetricType.QUALITY,
                unit="%",
                target=70,  # Want to stay under 70%
                warning_threshold=80,
                critical_threshold=90,
            ))

        # Memory utilization
        memory_pct = resources.get("memory_percent", 0)
        if memory_pct:
            result.add_metric(EvaluationMetric(
                name="memory_utilization",
                value=memory_pct,
                metric_type=MetricType.QUALITY,
                unit="%",
                target=70,
                warning_threshold=80,
                critical_threshold=90,
            ))

        # Disk utilization
        disk_pct = resources.get("disk_percent", 0)
        if disk_pct:
            result.add_metric(EvaluationMetric(
                name="disk_utilization",
                value=disk_pct,
                metric_type=MetricType.QUALITY,
                unit="%",
                target=70,
                warning_threshold=80,
                critical_threshold=90,
            ))

    def _evaluate_components(
        self,
        result: EvaluationResult,
        context: Dict[str, Any],
    ):
        """Evaluate component health"""
        components = context.get("components", {})

        healthy = 0
        unhealthy = 0

        for name, status in components.items():
            is_healthy = status.get("healthy", True)
            if is_healthy:
                healthy += 1
            else:
                unhealthy += 1
                result.add_metric(EvaluationMetric(
                    name=f"component_{name}_status",
                    value=0,
                    metric_type=MetricType.QUALITY,
                ))

        total = healthy + unhealthy
        if total > 0:
            health_rate = healthy / total
            result.add_metric(EvaluationMetric(
                name="component_health_rate",
                value=health_rate,
                metric_type=MetricType.ACCURACY,
                unit="%",
                target=1.0,
                warning_threshold=0.9,
                critical_threshold=0.8,
            ))

    def _generate_summary(self, result: EvaluationResult) -> str:
        """Generate system health summary"""
        summary = f"""
System Health Evaluation
========================
Overall Score: {result.overall_score:.0f}/100 (Grade: {result.grade})
Status: {result.status.upper()}

""".strip()

        # Key metrics
        key_metrics = ["system_uptime", "avg_latency_ms", "error_rate", "component_health_rate"]
        for m in result.metrics:
            if m.name in key_metrics:
                status = "OK" if m.meets_target else "ISSUE"
                summary += f"\n{m.name}: {m.value:.2f}{m.unit} ({status})"

        return summary

    def _generate_findings(self, result: EvaluationResult) -> List[str]:
        """Generate key findings"""
        findings = []

        for m in result.metrics:
            if m.status == "critical":
                findings.append(f"CRITICAL: {m.name} = {m.value:.2f}{m.unit}")
            elif m.status == "warning":
                findings.append(f"WARNING: {m.name} = {m.value:.2f}{m.unit}")

        return findings

    def _generate_recommendations(self, result: EvaluationResult) -> List[str]:
        """Generate recommendations"""
        recommendations = []

        for m in result.metrics:
            if not m.meets_target:
                if "uptime" in m.name:
                    recommendations.append("Investigate availability issues and add redundancy")
                elif "latency" in m.name:
                    recommendations.append("Optimize slow endpoints and add caching")
                elif "error" in m.name:
                    recommendations.append("Review error logs and fix recurring issues")
                elif "cpu" in m.name or "memory" in m.name:
                    recommendations.append("Scale resources or optimize resource usage")

        return list(set(recommendations))


# Global instance
_system_evaluator: Optional[SystemEvaluator] = None


def get_system_evaluator() -> SystemEvaluator:
    """Get the global system evaluator"""
    global _system_evaluator
    if _system_evaluator is None:
        _system_evaluator = SystemEvaluator()
    return _system_evaluator
