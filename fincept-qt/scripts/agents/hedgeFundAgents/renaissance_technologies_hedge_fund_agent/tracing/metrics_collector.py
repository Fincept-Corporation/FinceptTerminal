"""
Metrics Collector

Collects and aggregates performance metrics:
- Counter metrics (counts, totals)
- Gauge metrics (current values)
- Histogram metrics (distributions)
- Trading-specific metrics
"""

from typing import Optional, List, Dict, Any, Callable
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from enum import Enum
from collections import defaultdict
import threading
import statistics
import math

from ..config import get_config


class MetricType(str, Enum):
    """Types of metrics"""
    COUNTER = "counter"      # Monotonically increasing (requests, trades)
    GAUGE = "gauge"          # Current value (price, position size)
    HISTOGRAM = "histogram"  # Distribution (latency, returns)
    SUMMARY = "summary"      # Pre-calculated percentiles


@dataclass
class MetricValue:
    """A single metric value with timestamp"""
    value: float
    timestamp: datetime
    labels: Dict[str, str] = field(default_factory=dict)


@dataclass
class MetricDefinition:
    """Definition of a metric"""
    name: str
    metric_type: MetricType
    description: str
    unit: str = ""
    labels: List[str] = field(default_factory=list)


@dataclass
class HistogramBuckets:
    """Histogram bucket configuration"""
    boundaries: List[float]
    counts: List[int] = field(default_factory=list)
    sum_value: float = 0.0
    count: int = 0

    def __post_init__(self):
        if not self.counts:
            # +1 for the +Inf bucket
            self.counts = [0] * (len(self.boundaries) + 1)

    def observe(self, value: float):
        """Record a value in the histogram"""
        self.sum_value += value
        self.count += 1

        # Find the bucket
        for i, boundary in enumerate(self.boundaries):
            if value <= boundary:
                self.counts[i] += 1
                return
        # +Inf bucket
        self.counts[-1] += 1

    def get_percentile(self, percentile: float) -> float:
        """Estimate percentile from histogram"""
        if self.count == 0:
            return 0.0

        target_count = self.count * percentile / 100

        cumulative = 0
        for i, count in enumerate(self.counts):
            cumulative += count
            if cumulative >= target_count:
                if i == 0:
                    return self.boundaries[0] / 2
                elif i < len(self.boundaries):
                    # Linear interpolation
                    prev_boundary = self.boundaries[i - 1] if i > 0 else 0
                    return prev_boundary + (self.boundaries[i] - prev_boundary) * 0.5
                else:
                    # +Inf bucket - use last boundary * 2
                    return self.boundaries[-1] * 2 if self.boundaries else 0

        return self.boundaries[-1] if self.boundaries else 0


class MetricsCollector:
    """
    Collects and aggregates metrics for the trading system.

    Provides:
    1. Counter metrics for counts/totals
    2. Gauge metrics for current values
    3. Histogram metrics for distributions
    4. Trading-specific metric aggregations
    """

    # Default histogram buckets for common use cases
    LATENCY_BUCKETS = [5, 10, 25, 50, 100, 250, 500, 1000, 2500, 5000, 10000]  # ms
    RETURN_BUCKETS = [-500, -200, -100, -50, -20, -10, 0, 10, 20, 50, 100, 200, 500]  # bps
    SIZE_BUCKETS = [1000, 10000, 100000, 1000000, 10000000, 100000000]  # dollars

    def __init__(self):
        self.config = get_config()

        # Metric storage
        self._counters: Dict[str, float] = defaultdict(float)
        self._gauges: Dict[str, MetricValue] = {}
        self._histograms: Dict[str, HistogramBuckets] = {}
        self._time_series: Dict[str, List[MetricValue]] = defaultdict(list)

        # Metric definitions
        self._definitions: Dict[str, MetricDefinition] = {}

        # Threading
        self._lock = threading.Lock()

        # Register standard metrics
        self._register_standard_metrics()

    def _register_standard_metrics(self):
        """Register standard trading system metrics"""
        # Signal metrics
        self.register_metric(MetricDefinition(
            name="signals_generated",
            metric_type=MetricType.COUNTER,
            description="Total number of signals generated",
            labels=["signal_type", "agent_id"],
        ))

        self.register_metric(MetricDefinition(
            name="signals_approved",
            metric_type=MetricType.COUNTER,
            description="Total number of signals approved by IC",
            labels=["signal_type"],
        ))

        self.register_metric(MetricDefinition(
            name="signals_rejected",
            metric_type=MetricType.COUNTER,
            description="Total number of signals rejected",
            labels=["signal_type", "reason"],
        ))

        # Trade metrics
        self.register_metric(MetricDefinition(
            name="trades_executed",
            metric_type=MetricType.COUNTER,
            description="Total number of trades executed",
            labels=["direction", "execution_algo"],
        ))

        self.register_metric(MetricDefinition(
            name="trade_pnl_bps",
            metric_type=MetricType.HISTOGRAM,
            description="Trade P&L distribution",
            unit="bps",
        ))

        self.register_metric(MetricDefinition(
            name="trade_slippage_bps",
            metric_type=MetricType.HISTOGRAM,
            description="Trade slippage distribution",
            unit="bps",
        ))

        # Agent metrics
        self.register_metric(MetricDefinition(
            name="agent_requests",
            metric_type=MetricType.COUNTER,
            description="Total agent requests",
            labels=["agent_id"],
        ))

        self.register_metric(MetricDefinition(
            name="agent_latency_ms",
            metric_type=MetricType.HISTOGRAM,
            description="Agent response latency",
            unit="ms",
            labels=["agent_id"],
        ))

        self.register_metric(MetricDefinition(
            name="agent_errors",
            metric_type=MetricType.COUNTER,
            description="Agent errors",
            labels=["agent_id", "error_type"],
        ))

        # Workflow metrics
        self.register_metric(MetricDefinition(
            name="workflow_executions",
            metric_type=MetricType.COUNTER,
            description="Total workflow executions",
            labels=["workflow_name"],
        ))

        self.register_metric(MetricDefinition(
            name="workflow_duration_ms",
            metric_type=MetricType.HISTOGRAM,
            description="Workflow execution duration",
            unit="ms",
            labels=["workflow_name"],
        ))

        # Risk metrics
        self.register_metric(MetricDefinition(
            name="portfolio_var",
            metric_type=MetricType.GAUGE,
            description="Current portfolio VaR",
            unit="%",
        ))

        self.register_metric(MetricDefinition(
            name="portfolio_drawdown",
            metric_type=MetricType.GAUGE,
            description="Current portfolio drawdown",
            unit="%",
        ))

        self.register_metric(MetricDefinition(
            name="guardrail_violations",
            metric_type=MetricType.COUNTER,
            description="Guardrail violations",
            labels=["guardrail_name", "severity"],
        ))

        # System metrics
        self.register_metric(MetricDefinition(
            name="system_uptime_seconds",
            metric_type=MetricType.COUNTER,
            description="System uptime in seconds",
        ))

        self.register_metric(MetricDefinition(
            name="memory_usage_bytes",
            metric_type=MetricType.GAUGE,
            description="Memory usage",
            unit="bytes",
        ))

        self.register_metric(MetricDefinition(
            name="active_positions",
            metric_type=MetricType.GAUGE,
            description="Number of active positions",
        ))

    def register_metric(self, definition: MetricDefinition):
        """Register a metric definition"""
        with self._lock:
            self._definitions[definition.name] = definition

            # Initialize histogram buckets if needed
            if definition.metric_type == MetricType.HISTOGRAM:
                buckets = self._get_default_buckets(definition.name)
                self._histograms[definition.name] = HistogramBuckets(boundaries=buckets)

    def _get_default_buckets(self, metric_name: str) -> List[float]:
        """Get default buckets for a histogram metric"""
        if "latency" in metric_name or "duration" in metric_name:
            return self.LATENCY_BUCKETS
        elif "pnl" in metric_name or "return" in metric_name or "slippage" in metric_name:
            return self.RETURN_BUCKETS
        elif "size" in metric_name or "value" in metric_name:
            return self.SIZE_BUCKETS
        else:
            # Default buckets
            return [1, 5, 10, 25, 50, 75, 100, 250, 500, 1000]

    def _make_key(self, name: str, labels: Optional[Dict[str, str]] = None) -> str:
        """Create a unique key for a metric with labels"""
        if not labels:
            return name
        label_str = ",".join(f"{k}={v}" for k, v in sorted(labels.items()))
        return f"{name}{{{label_str}}}"

    # Counter methods
    def increment(
        self,
        name: str,
        value: float = 1.0,
        labels: Optional[Dict[str, str]] = None,
    ):
        """Increment a counter"""
        key = self._make_key(name, labels)
        with self._lock:
            self._counters[key] += value

    def get_counter(
        self,
        name: str,
        labels: Optional[Dict[str, str]] = None,
    ) -> float:
        """Get counter value"""
        key = self._make_key(name, labels)
        with self._lock:
            return self._counters.get(key, 0.0)

    # Gauge methods
    def set_gauge(
        self,
        name: str,
        value: float,
        labels: Optional[Dict[str, str]] = None,
    ):
        """Set a gauge value"""
        key = self._make_key(name, labels)
        with self._lock:
            self._gauges[key] = MetricValue(
                value=value,
                timestamp=datetime.utcnow(),
                labels=labels or {},
            )

            # Also add to time series for historical tracking
            self._time_series[key].append(MetricValue(
                value=value,
                timestamp=datetime.utcnow(),
                labels=labels or {},
            ))

            # Keep only last 1000 values
            if len(self._time_series[key]) > 1000:
                self._time_series[key] = self._time_series[key][-1000:]

    def get_gauge(
        self,
        name: str,
        labels: Optional[Dict[str, str]] = None,
    ) -> Optional[float]:
        """Get current gauge value"""
        key = self._make_key(name, labels)
        with self._lock:
            metric = self._gauges.get(key)
            return metric.value if metric else None

    # Histogram methods
    def observe(
        self,
        name: str,
        value: float,
        labels: Optional[Dict[str, str]] = None,
    ):
        """Record a histogram observation"""
        key = self._make_key(name, labels)
        with self._lock:
            if key not in self._histograms:
                buckets = self._get_default_buckets(name)
                self._histograms[key] = HistogramBuckets(boundaries=buckets)
            self._histograms[key].observe(value)

            # Also track in time series
            self._time_series[key].append(MetricValue(
                value=value,
                timestamp=datetime.utcnow(),
                labels=labels or {},
            ))

            if len(self._time_series[key]) > 10000:
                self._time_series[key] = self._time_series[key][-10000:]

    def get_histogram_stats(
        self,
        name: str,
        labels: Optional[Dict[str, str]] = None,
    ) -> Dict[str, float]:
        """Get histogram statistics"""
        key = self._make_key(name, labels)
        with self._lock:
            hist = self._histograms.get(key)
            if not hist or hist.count == 0:
                return {}

            return {
                "count": hist.count,
                "sum": hist.sum_value,
                "mean": hist.sum_value / hist.count,
                "p50": hist.get_percentile(50),
                "p90": hist.get_percentile(90),
                "p95": hist.get_percentile(95),
                "p99": hist.get_percentile(99),
            }

    # Trading-specific methods
    def record_signal(
        self,
        signal_type: str,
        agent_id: str,
        approved: bool,
        rejection_reason: Optional[str] = None,
    ):
        """Record a signal generation/decision"""
        self.increment("signals_generated", labels={
            "signal_type": signal_type,
            "agent_id": agent_id,
        })

        if approved:
            self.increment("signals_approved", labels={"signal_type": signal_type})
        else:
            self.increment("signals_rejected", labels={
                "signal_type": signal_type,
                "reason": rejection_reason or "unknown",
            })

    def record_trade(
        self,
        direction: str,
        execution_algo: str,
        pnl_bps: float,
        slippage_bps: float,
        market_impact_bps: float,
    ):
        """Record a trade execution"""
        self.increment("trades_executed", labels={
            "direction": direction,
            "execution_algo": execution_algo,
        })

        self.observe("trade_pnl_bps", pnl_bps)
        self.observe("trade_slippage_bps", slippage_bps)
        self.observe("trade_market_impact_bps", market_impact_bps)

    def record_agent_request(
        self,
        agent_id: str,
        latency_ms: float,
        success: bool,
        error_type: Optional[str] = None,
    ):
        """Record an agent request"""
        self.increment("agent_requests", labels={"agent_id": agent_id})
        self.observe("agent_latency_ms", latency_ms, labels={"agent_id": agent_id})

        if not success and error_type:
            self.increment("agent_errors", labels={
                "agent_id": agent_id,
                "error_type": error_type,
            })

    def record_workflow_execution(
        self,
        workflow_name: str,
        duration_ms: float,
        success: bool,
    ):
        """Record a workflow execution"""
        self.increment("workflow_executions", labels={
            "workflow_name": workflow_name,
        })

        self.observe("workflow_duration_ms", duration_ms, labels={
            "workflow_name": workflow_name,
        })

    def record_guardrail_check(
        self,
        guardrail_name: str,
        passed: bool,
        severity: str = "warning",
    ):
        """Record a guardrail check result"""
        if not passed:
            self.increment("guardrail_violations", labels={
                "guardrail_name": guardrail_name,
                "severity": severity,
            })

    def update_portfolio_metrics(
        self,
        var_pct: float,
        drawdown_pct: float,
        active_positions: int,
    ):
        """Update portfolio-level metrics"""
        self.set_gauge("portfolio_var", var_pct)
        self.set_gauge("portfolio_drawdown", drawdown_pct)
        self.set_gauge("active_positions", active_positions)

    # Aggregation and reporting
    def get_summary(self) -> Dict[str, Any]:
        """Get a summary of all metrics"""
        with self._lock:
            summary = {
                "timestamp": datetime.utcnow().isoformat(),
                "counters": {},
                "gauges": {},
                "histograms": {},
            }

            # Counters
            for key, value in self._counters.items():
                summary["counters"][key] = value

            # Gauges
            for key, metric in self._gauges.items():
                summary["gauges"][key] = {
                    "value": metric.value,
                    "timestamp": metric.timestamp.isoformat(),
                }

            # Histograms
            for key, hist in self._histograms.items():
                if hist.count > 0:
                    summary["histograms"][key] = {
                        "count": hist.count,
                        "sum": hist.sum_value,
                        "mean": hist.sum_value / hist.count,
                        "p50": hist.get_percentile(50),
                        "p95": hist.get_percentile(95),
                        "p99": hist.get_percentile(99),
                    }

            return summary

    def get_trading_dashboard(self) -> Dict[str, Any]:
        """Get trading-specific dashboard metrics"""
        signals_generated = self.get_counter("signals_generated")
        signals_approved = self.get_counter("signals_approved")
        trades_executed = self.get_counter("trades_executed")

        pnl_stats = self.get_histogram_stats("trade_pnl_bps")
        slippage_stats = self.get_histogram_stats("trade_slippage_bps")

        return {
            "timestamp": datetime.utcnow().isoformat(),
            "signals": {
                "total_generated": signals_generated,
                "total_approved": signals_approved,
                "approval_rate": signals_approved / signals_generated if signals_generated > 0 else 0,
            },
            "trades": {
                "total_executed": trades_executed,
                "pnl": {
                    "mean_bps": pnl_stats.get("mean", 0),
                    "p50_bps": pnl_stats.get("p50", 0),
                    "p95_bps": pnl_stats.get("p95", 0),
                },
                "slippage": {
                    "mean_bps": slippage_stats.get("mean", 0),
                    "p95_bps": slippage_stats.get("p95", 0),
                },
            },
            "portfolio": {
                "var_pct": self.get_gauge("portfolio_var"),
                "drawdown_pct": self.get_gauge("portfolio_drawdown"),
                "active_positions": self.get_gauge("active_positions"),
            },
            "guardrails": {
                "total_violations": self.get_counter("guardrail_violations"),
            },
        }

    def get_agent_metrics(self, agent_id: str) -> Dict[str, Any]:
        """Get metrics for a specific agent"""
        requests = self.get_counter("agent_requests", labels={"agent_id": agent_id})
        errors = self.get_counter("agent_errors", labels={"agent_id": agent_id})
        latency_stats = self.get_histogram_stats("agent_latency_ms", labels={"agent_id": agent_id})

        return {
            "agent_id": agent_id,
            "total_requests": requests,
            "total_errors": errors,
            "error_rate": errors / requests if requests > 0 else 0,
            "latency": {
                "mean_ms": latency_stats.get("mean", 0),
                "p50_ms": latency_stats.get("p50", 0),
                "p95_ms": latency_stats.get("p95", 0),
                "p99_ms": latency_stats.get("p99", 0),
            },
        }

    def get_time_series(
        self,
        name: str,
        labels: Optional[Dict[str, str]] = None,
        since: Optional[datetime] = None,
    ) -> List[Dict[str, Any]]:
        """Get time series data for a metric"""
        key = self._make_key(name, labels)
        with self._lock:
            series = self._time_series.get(key, [])

            if since:
                series = [m for m in series if m.timestamp >= since]

            return [
                {"value": m.value, "timestamp": m.timestamp.isoformat()}
                for m in series
            ]

    def reset(self):
        """Reset all metrics (for testing)"""
        with self._lock:
            self._counters.clear()
            self._gauges.clear()
            self._histograms.clear()
            self._time_series.clear()

            # Re-initialize histograms
            for name, definition in self._definitions.items():
                if definition.metric_type == MetricType.HISTOGRAM:
                    buckets = self._get_default_buckets(name)
                    self._histograms[name] = HistogramBuckets(boundaries=buckets)


# Global metrics collector instance
_metrics_collector: Optional[MetricsCollector] = None


def get_metrics_collector() -> MetricsCollector:
    """Get the global metrics collector"""
    global _metrics_collector
    if _metrics_collector is None:
        _metrics_collector = MetricsCollector()
    return _metrics_collector


def reset_metrics_collector():
    """Reset the metrics collector (for testing)"""
    global _metrics_collector
    _metrics_collector = None
