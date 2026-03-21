"""
Agent Evaluation

Evaluates individual agent performance:
- Decision accuracy
- Response quality
- Task completion
- Learning effectiveness
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


class AgentEvaluator(Evaluator):
    """
    Evaluates individual agent performance.

    Key metrics:
    - Decision accuracy
    - Response quality
    - Task completion rate
    - Learning from feedback
    - Consistency
    """

    def __init__(self):
        super().__init__(
            name="agent_evaluator",
            description="Evaluates individual agent performance",
            level=EvaluationLevel.AGENT,
        )

    def evaluate(self, context: Dict[str, Any]) -> EvaluationResult:
        """
        Evaluate agent performance.

        Context should contain:
        - agent_id: ID of the agent to evaluate
        - decisions: List of decisions made by the agent
        - tasks: List of tasks completed
        - feedback: Feedback received
        """
        agent_id = context.get("agent_id", "unknown")
        decisions = context.get("decisions", [])
        tasks = context.get("tasks", [])
        feedback = context.get("feedback", [])

        result = EvaluationResult(
            evaluation_id=f"agent_eval_{uuid.uuid4().hex[:8]}",
            level=self.level,
            subject=f"Agent: {agent_id}",
        )

        # Evaluate different aspects
        self._evaluate_decision_accuracy(result, decisions)
        self._evaluate_task_completion(result, tasks)
        self._evaluate_response_quality(result, decisions, tasks)
        self._evaluate_learning(result, decisions, feedback)
        self._evaluate_consistency(result, decisions)

        # Calculate overall score
        result.calculate_overall_score()

        # Generate summary
        result.summary = self._generate_summary(result, agent_id)
        result.findings = self._generate_findings(result)
        result.recommendations = self._generate_recommendations(result)

        self._history.append(result)
        return result

    def _evaluate_decision_accuracy(
        self,
        result: EvaluationResult,
        decisions: List[Dict[str, Any]],
    ):
        """Evaluate decision accuracy"""
        if not decisions:
            return

        # Overall accuracy
        correct = len([d for d in decisions if d.get("outcome") == "correct"])
        incorrect = len([d for d in decisions if d.get("outcome") == "incorrect"])
        total = correct + incorrect

        if total > 0:
            accuracy = correct / total
            result.add_metric(EvaluationMetric(
                name="decision_accuracy",
                value=accuracy,
                metric_type=MetricType.ACCURACY,
                unit="%",
                target=0.70,
                warning_threshold=0.60,
                critical_threshold=0.50,
            ))

        # Accuracy by decision type
        by_type = {}
        for d in decisions:
            d_type = d.get("decision_type", "unknown")
            if d_type not in by_type:
                by_type[d_type] = {"correct": 0, "total": 0}
            by_type[d_type]["total"] += 1
            if d.get("outcome") == "correct":
                by_type[d_type]["correct"] += 1

        for d_type, counts in by_type.items():
            if counts["total"] > 0:
                accuracy = counts["correct"] / counts["total"]
                result.add_metric(EvaluationMetric(
                    name=f"{d_type}_accuracy",
                    value=accuracy,
                    metric_type=MetricType.ACCURACY,
                    unit="%",
                ))

        # P&L impact of decisions
        pnl_impacts = [
            d.get("pnl_impact_bps", 0)
            for d in decisions
            if d.get("pnl_impact_bps") is not None
        ]

        if pnl_impacts:
            total_pnl = sum(pnl_impacts)
            result.add_metric(EvaluationMetric(
                name="total_pnl_impact_bps",
                value=total_pnl,
                metric_type=MetricType.PROFIT,
                unit="bps",
            ))

    def _evaluate_task_completion(
        self,
        result: EvaluationResult,
        tasks: List[Dict[str, Any]],
    ):
        """Evaluate task completion"""
        if not tasks:
            return

        completed = len([t for t in tasks if t.get("status") == "completed"])
        failed = len([t for t in tasks if t.get("status") == "failed"])
        total = len(tasks)

        # Completion rate
        completion_rate = completed / total if total > 0 else 0
        result.add_metric(EvaluationMetric(
            name="task_completion_rate",
            value=completion_rate,
            metric_type=MetricType.ACCURACY,
            unit="%",
            target=0.95,
            warning_threshold=0.90,
            critical_threshold=0.80,
        ))

        # Average completion time
        completion_times = [
            t.get("completion_time_seconds", 0)
            for t in tasks
            if t.get("status") == "completed" and t.get("completion_time_seconds")
        ]

        if completion_times:
            avg_time = statistics.mean(completion_times)
            result.add_metric(EvaluationMetric(
                name="avg_task_completion_time_seconds",
                value=avg_time,
                metric_type=MetricType.LATENCY,
                unit="seconds",
            ))

    def _evaluate_response_quality(
        self,
        result: EvaluationResult,
        decisions: List[Dict[str, Any]],
        tasks: List[Dict[str, Any]],
    ):
        """Evaluate response quality"""
        quality_scores = []

        # Get quality ratings from decisions
        for d in decisions:
            if d.get("quality_score"):
                quality_scores.append(d["quality_score"])

        # Get quality ratings from tasks
        for t in tasks:
            if t.get("quality_score"):
                quality_scores.append(t["quality_score"])

        if quality_scores:
            avg_quality = statistics.mean(quality_scores)
            result.add_metric(EvaluationMetric(
                name="avg_response_quality",
                value=avg_quality,
                metric_type=MetricType.QUALITY,
                target=0.8,
                warning_threshold=0.7,
            ))

    def _evaluate_learning(
        self,
        result: EvaluationResult,
        decisions: List[Dict[str, Any]],
        feedback: List[Dict[str, Any]],
    ):
        """Evaluate learning from feedback"""
        if len(decisions) < 10:
            return  # Need enough data

        # Split into first half and second half
        mid = len(decisions) // 2
        first_half = decisions[:mid]
        second_half = decisions[mid:]

        # Calculate accuracy improvement
        first_accuracy = len([d for d in first_half if d.get("outcome") == "correct"]) / len(first_half)
        second_accuracy = len([d for d in second_half if d.get("outcome") == "correct"]) / len(second_half)

        improvement = second_accuracy - first_accuracy

        result.add_metric(EvaluationMetric(
            name="learning_improvement",
            value=improvement,
            metric_type=MetricType.QUALITY,
            target=0.05,  # 5% improvement target
        ))

        # Feedback incorporation rate
        if feedback:
            applied = len([f for f in feedback if f.get("applied", False)])
            incorporation_rate = applied / len(feedback)
            result.add_metric(EvaluationMetric(
                name="feedback_incorporation_rate",
                value=incorporation_rate,
                metric_type=MetricType.ACCURACY,
                unit="%",
                target=0.8,
            ))

    def _evaluate_consistency(
        self,
        result: EvaluationResult,
        decisions: List[Dict[str, Any]],
    ):
        """Evaluate decision consistency"""
        if len(decisions) < 5:
            return

        # Check for contradictory decisions
        # (Same situation but different decision)
        contradictions = 0
        # This would need more sophisticated logic in production

        # Evaluate confidence calibration
        confidences = [d.get("confidence", 0.5) for d in decisions]
        outcomes = [1 if d.get("outcome") == "correct" else 0 for d in decisions]

        if confidences and outcomes:
            # Simple calibration: high confidence should correlate with correct
            high_conf = [(c, o) for c, o in zip(confidences, outcomes) if c > 0.7]
            low_conf = [(c, o) for c, o in zip(confidences, outcomes) if c < 0.5]

            if high_conf:
                high_conf_accuracy = sum(o for _, o in high_conf) / len(high_conf)
                result.add_metric(EvaluationMetric(
                    name="high_confidence_accuracy",
                    value=high_conf_accuracy,
                    metric_type=MetricType.ACCURACY,
                    unit="%",
                    target=0.75,
                ))

    def _generate_summary(
        self,
        result: EvaluationResult,
        agent_id: str,
    ) -> str:
        """Generate evaluation summary"""
        summary = f"""
Agent Performance Evaluation: {agent_id}
=========================================
Overall Score: {result.overall_score:.0f}/100 (Grade: {result.grade})
Status: {result.status.upper()}

""".strip()

        # Add key metrics
        key_metrics = ["decision_accuracy", "task_completion_rate", "avg_response_quality"]
        for m in result.metrics:
            if m.name in key_metrics:
                status = "PASS" if m.meets_target else "NEEDS IMPROVEMENT"
                summary += f"\n{m.name}: {m.value:.1%} ({status})"

        return summary

    def _generate_findings(self, result: EvaluationResult) -> List[str]:
        """Generate key findings"""
        findings = []

        for m in result.metrics:
            if m.status == "critical":
                findings.append(f"CRITICAL: {m.name} below threshold")
            elif m.status == "warning":
                findings.append(f"WARNING: {m.name} needs improvement")

        # Add positive findings
        for m in result.metrics:
            if m.meets_target and m.name in ["decision_accuracy", "task_completion_rate"]:
                findings.append(f"GOOD: {m.name} meets target")

        return findings

    def _generate_recommendations(self, result: EvaluationResult) -> List[str]:
        """Generate recommendations"""
        recommendations = []

        for m in result.metrics:
            if not m.meets_target:
                if "accuracy" in m.name:
                    recommendations.append(f"Review decision criteria for {m.name}")
                elif "completion" in m.name:
                    recommendations.append("Investigate task failures and bottlenecks")
                elif "quality" in m.name:
                    recommendations.append("Improve response generation with more context")
                elif "learning" in m.name:
                    recommendations.append("Enhance feedback loop and memory utilization")

        return list(set(recommendations))


# Global instance
_agent_evaluator: Optional[AgentEvaluator] = None


def get_agent_evaluator() -> AgentEvaluator:
    """Get the global agent evaluator"""
    global _agent_evaluator
    if _agent_evaluator is None:
        _agent_evaluator = AgentEvaluator()
    return _agent_evaluator
