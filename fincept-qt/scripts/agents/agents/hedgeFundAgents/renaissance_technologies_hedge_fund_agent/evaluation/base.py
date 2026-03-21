"""
Base Evaluation Infrastructure

Core evaluation framework for Renaissance Technologies multi-agent system.
Implements metrics tracking, performance assessment, and continuous improvement.
"""

from typing import Optional, List, Dict, Any, Callable
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from enum import Enum
from abc import ABC, abstractmethod
import statistics

from ..config import get_config


class EvaluationLevel(str, Enum):
    """Levels at which evaluation is performed"""
    SIGNAL = "signal"           # Individual signal quality
    TRADE = "trade"             # Trade outcome
    AGENT = "agent"             # Agent performance
    TEAM = "team"               # Team effectiveness
    SYSTEM = "system"           # Overall system health
    WORKFLOW = "workflow"       # Workflow performance


class MetricType(str, Enum):
    """Types of evaluation metrics"""
    ACCURACY = "accuracy"       # Correctness rate
    PRECISION = "precision"     # True positive rate
    RECALL = "recall"           # Coverage rate
    F1_SCORE = "f1_score"       # Harmonic mean of precision/recall
    LATENCY = "latency"         # Response time
    THROUGHPUT = "throughput"   # Volume processed
    QUALITY = "quality"         # Output quality score
    PROFIT = "profit"           # Financial performance
    RISK = "risk"               # Risk metrics


@dataclass
class EvaluationMetric:
    """A single evaluation metric"""
    name: str
    value: float
    metric_type: MetricType
    unit: str = ""
    timestamp: str = field(default_factory=lambda: datetime.utcnow().isoformat())

    # Comparison thresholds
    target: Optional[float] = None
    warning_threshold: Optional[float] = None
    critical_threshold: Optional[float] = None

    # Historical context
    previous_value: Optional[float] = None
    historical_avg: Optional[float] = None
    percentile: Optional[float] = None

    @property
    def meets_target(self) -> bool:
        """Check if metric meets target"""
        if self.target is None:
            return True
        return self.value >= self.target

    @property
    def status(self) -> str:
        """Get metric status"""
        if self.critical_threshold and self.value <= self.critical_threshold:
            return "critical"
        if self.warning_threshold and self.value <= self.warning_threshold:
            return "warning"
        if self.meets_target:
            return "good"
        return "below_target"

    def to_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "value": self.value,
            "metric_type": self.metric_type.value,
            "unit": self.unit,
            "timestamp": self.timestamp,
            "target": self.target,
            "meets_target": self.meets_target,
            "status": self.status,
        }


@dataclass
class EvaluationResult:
    """Result of an evaluation"""
    evaluation_id: str
    level: EvaluationLevel
    subject: str  # What was evaluated

    # Metrics
    metrics: List[EvaluationMetric] = field(default_factory=list)

    # Overall assessment
    overall_score: float = 0.0
    grade: str = "N/A"  # A, B, C, D, F
    status: str = "unknown"  # excellent, good, acceptable, poor, critical

    # Details
    summary: str = ""
    findings: List[str] = field(default_factory=list)
    recommendations: List[str] = field(default_factory=list)

    # Metadata
    evaluated_at: str = field(default_factory=lambda: datetime.utcnow().isoformat())
    evaluated_by: str = "system"

    def add_metric(self, metric: EvaluationMetric):
        """Add a metric to the result"""
        self.metrics.append(metric)

    def calculate_overall_score(self):
        """Calculate overall score from metrics"""
        if not self.metrics:
            self.overall_score = 0
            return

        scores = []
        for metric in self.metrics:
            if metric.target and metric.target > 0:
                score = min(100, (metric.value / metric.target) * 100)
                scores.append(score)
            elif metric.value > 0:
                scores.append(min(100, metric.value * 100))

        self.overall_score = statistics.mean(scores) if scores else 0
        self._assign_grade()

    def _assign_grade(self):
        """Assign letter grade based on score"""
        if self.overall_score >= 90:
            self.grade = "A"
            self.status = "excellent"
        elif self.overall_score >= 80:
            self.grade = "B"
            self.status = "good"
        elif self.overall_score >= 70:
            self.grade = "C"
            self.status = "acceptable"
        elif self.overall_score >= 60:
            self.grade = "D"
            self.status = "poor"
        else:
            self.grade = "F"
            self.status = "critical"

    def to_dict(self) -> Dict[str, Any]:
        return {
            "evaluation_id": self.evaluation_id,
            "level": self.level.value,
            "subject": self.subject,
            "metrics": [m.to_dict() for m in self.metrics],
            "overall_score": self.overall_score,
            "grade": self.grade,
            "status": self.status,
            "summary": self.summary,
            "findings": self.findings,
            "recommendations": self.recommendations,
            "evaluated_at": self.evaluated_at,
        }


class Evaluator(ABC):
    """
    Base class for all evaluators.

    Evaluators assess performance at different levels
    of the system.
    """

    def __init__(
        self,
        name: str,
        description: str,
        level: EvaluationLevel,
    ):
        self.name = name
        self.description = description
        self.level = level
        self.config = get_config()

        # Historical results
        self._history: List[EvaluationResult] = []

    @abstractmethod
    def evaluate(self, context: Dict[str, Any]) -> EvaluationResult:
        """
        Perform evaluation.

        Args:
            context: Data needed for evaluation

        Returns:
            EvaluationResult with metrics and assessment
        """
        pass

    def get_history(
        self,
        limit: int = 100,
        since: Optional[datetime] = None,
    ) -> List[EvaluationResult]:
        """Get historical evaluation results"""
        results = self._history

        if since:
            results = [
                r for r in results
                if datetime.fromisoformat(r.evaluated_at) >= since
            ]

        return results[-limit:]

    def get_trend(
        self,
        metric_name: str,
        periods: int = 10,
    ) -> Dict[str, Any]:
        """Get trend for a specific metric"""
        history = self.get_history(limit=periods)

        values = []
        for result in history:
            for metric in result.metrics:
                if metric.name == metric_name:
                    values.append(metric.value)
                    break

        if len(values) < 2:
            return {"trend": "insufficient_data", "values": values}

        # Calculate trend
        avg_first_half = statistics.mean(values[:len(values)//2])
        avg_second_half = statistics.mean(values[len(values)//2:])

        if avg_second_half > avg_first_half * 1.05:
            trend = "improving"
        elif avg_second_half < avg_first_half * 0.95:
            trend = "declining"
        else:
            trend = "stable"

        return {
            "trend": trend,
            "values": values,
            "avg_first_half": avg_first_half,
            "avg_second_half": avg_second_half,
            "change_pct": (avg_second_half - avg_first_half) / avg_first_half * 100 if avg_first_half else 0,
        }


class EvaluationRegistry:
    """
    Registry for all evaluators in the system.
    """

    def __init__(self):
        self._evaluators: Dict[str, Evaluator] = {}

    def register(self, evaluator: Evaluator):
        """Register an evaluator"""
        self._evaluators[evaluator.name] = evaluator

    def get(self, name: str) -> Optional[Evaluator]:
        """Get an evaluator by name"""
        return self._evaluators.get(name)

    def get_by_level(self, level: EvaluationLevel) -> List[Evaluator]:
        """Get all evaluators for a level"""
        return [e for e in self._evaluators.values() if e.level == level]

    def evaluate_all(
        self,
        level: Optional[EvaluationLevel] = None,
        context: Optional[Dict[str, Any]] = None,
    ) -> List[EvaluationResult]:
        """Run all evaluators (or for specific level)"""
        results = []
        context = context or {}

        evaluators = (
            self.get_by_level(level) if level
            else list(self._evaluators.values())
        )

        for evaluator in evaluators:
            try:
                result = evaluator.evaluate(context)
                results.append(result)
            except Exception as e:
                # Create error result
                results.append(EvaluationResult(
                    evaluation_id=f"error_{evaluator.name}",
                    level=evaluator.level,
                    subject=evaluator.name,
                    overall_score=0,
                    status="error",
                    summary=f"Evaluation failed: {str(e)}",
                ))

        return results

    def get_system_health(self) -> Dict[str, Any]:
        """Get overall system health from all evaluators"""
        results = self.evaluate_all()

        scores = [r.overall_score for r in results if r.overall_score > 0]
        statuses = [r.status for r in results]

        return {
            "overall_health": statistics.mean(scores) if scores else 0,
            "evaluations_run": len(results),
            "status_distribution": {
                status: statuses.count(status)
                for status in set(statuses)
            },
            "critical_issues": [
                r.subject for r in results if r.status == "critical"
            ],
        }


# Global registry instance
_evaluation_registry: Optional[EvaluationRegistry] = None


def get_evaluation_registry() -> EvaluationRegistry:
    """Get the global evaluation registry"""
    global _evaluation_registry
    if _evaluation_registry is None:
        _evaluation_registry = EvaluationRegistry()
        _register_default_evaluators(_evaluation_registry)
    return _evaluation_registry


def _register_default_evaluators(registry: EvaluationRegistry):
    """Register default evaluators"""
    from .signal_evaluation import SignalEvaluator
    from .trade_evaluation import TradeEvaluator
    from .agent_evaluation import AgentEvaluator
    from .system_evaluation import SystemEvaluator

    registry.register(SignalEvaluator())
    registry.register(TradeEvaluator())
    registry.register(AgentEvaluator())
    registry.register(SystemEvaluator())
