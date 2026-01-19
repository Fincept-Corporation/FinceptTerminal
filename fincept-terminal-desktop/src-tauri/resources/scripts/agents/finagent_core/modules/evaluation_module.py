"""
Evaluation Module - Agent quality assessment for financial analysis

Provides:
- Accuracy evaluation for financial predictions
- Performance metrics (latency, token usage)
- Reliability assessment
- Agent-as-judge evaluation
"""

from typing import Dict, Any, Optional, List, Callable
from dataclasses import dataclass
from datetime import datetime
import logging
import json

logger = logging.getLogger(__name__)


@dataclass
class EvaluationResult:
    """Result of an evaluation."""
    passed: bool
    score: float
    metric: str
    details: Dict[str, Any]
    timestamp: str


class AccuracyEvaluator:
    """
    Evaluate accuracy of financial predictions.

    Compares agent predictions against actual outcomes.
    """

    def __init__(
        self,
        tolerance: float = 0.05,
        metrics: List[str] = None
    ):
        """
        Initialize accuracy evaluator.

        Args:
            tolerance: Acceptable error margin (default 5%)
            metrics: Metrics to evaluate
        """
        self.tolerance = tolerance
        self.metrics = metrics or ["price_prediction", "direction", "signal"]

    def evaluate(
        self,
        prediction: Dict[str, Any],
        actual: Dict[str, Any]
    ) -> EvaluationResult:
        """
        Evaluate prediction accuracy.

        Args:
            prediction: Agent's prediction
            actual: Actual outcome

        Returns:
            EvaluationResult
        """
        scores = {}
        total_score = 0.0
        count = 0

        # Price prediction accuracy
        if "price" in prediction and "price" in actual:
            pred_price = prediction["price"]
            actual_price = actual["price"]
            if actual_price > 0:
                error = abs(pred_price - actual_price) / actual_price
                price_score = max(0, 1 - error / self.tolerance)
                scores["price_accuracy"] = price_score
                total_score += price_score
                count += 1

        # Direction accuracy (up/down)
        if "direction" in prediction and "direction" in actual:
            pred_dir = prediction["direction"].lower()
            actual_dir = actual["direction"].lower()
            dir_score = 1.0 if pred_dir == actual_dir else 0.0
            scores["direction_accuracy"] = dir_score
            total_score += dir_score
            count += 1

        # Signal accuracy (buy/sell/hold)
        if "signal" in prediction and "signal" in actual:
            pred_signal = prediction["signal"].lower()
            actual_signal = actual["signal"].lower()
            signal_score = 1.0 if pred_signal == actual_signal else 0.0
            scores["signal_accuracy"] = signal_score
            total_score += signal_score
            count += 1

        # Confidence calibration
        if "confidence" in prediction and "correct" in actual:
            conf = prediction["confidence"]
            correct = actual["correct"]
            # Good calibration: high confidence when correct, low when wrong
            if correct:
                scores["calibration"] = conf
            else:
                scores["calibration"] = 1 - conf
            total_score += scores["calibration"]
            count += 1

        avg_score = total_score / count if count > 0 else 0.0
        passed = avg_score >= 0.7  # 70% threshold

        return EvaluationResult(
            passed=passed,
            score=avg_score,
            metric="accuracy",
            details=scores,
            timestamp=datetime.now().isoformat()
        )

    def to_agno_eval(self):
        """Convert to Agno evaluation format."""
        try:
            from agno.eval import AccuracyEval
            return AccuracyEval()
        except ImportError:
            return None


class PerformanceEvaluator:
    """
    Evaluate agent performance metrics.

    Measures:
    - Response latency
    - Token usage
    - Tool call efficiency
    """

    def __init__(
        self,
        max_latency_ms: int = 5000,
        max_tokens: int = 4000,
        max_tool_calls: int = 10
    ):
        """
        Initialize performance evaluator.

        Args:
            max_latency_ms: Maximum acceptable latency
            max_tokens: Maximum token budget
            max_tool_calls: Maximum tool calls
        """
        self.max_latency_ms = max_latency_ms
        self.max_tokens = max_tokens
        self.max_tool_calls = max_tool_calls

    def evaluate(self, run_metrics: Dict[str, Any]) -> EvaluationResult:
        """
        Evaluate run performance.

        Args:
            run_metrics: Metrics from agent run

        Returns:
            EvaluationResult
        """
        scores = {}

        # Latency score
        latency = run_metrics.get("latency_ms", 0)
        latency_score = max(0, 1 - latency / self.max_latency_ms)
        scores["latency"] = latency_score

        # Token efficiency
        tokens_used = run_metrics.get("total_tokens", 0)
        token_score = max(0, 1 - tokens_used / self.max_tokens)
        scores["token_efficiency"] = token_score

        # Tool call efficiency
        tool_calls = run_metrics.get("tool_calls", 0)
        tool_score = max(0, 1 - tool_calls / self.max_tool_calls)
        scores["tool_efficiency"] = tool_score

        # Success rate
        success = run_metrics.get("success", True)
        scores["success"] = 1.0 if success else 0.0

        avg_score = sum(scores.values()) / len(scores)
        passed = avg_score >= 0.6

        return EvaluationResult(
            passed=passed,
            score=avg_score,
            metric="performance",
            details={
                "scores": scores,
                "raw_metrics": run_metrics
            },
            timestamp=datetime.now().isoformat()
        )


class ReliabilityEvaluator:
    """
    Evaluate agent reliability.

    Measures:
    - Consistency across runs
    - Error rate
    - Recovery from failures
    """

    def __init__(
        self,
        consistency_threshold: float = 0.8,
        max_error_rate: float = 0.05
    ):
        """
        Initialize reliability evaluator.

        Args:
            consistency_threshold: Minimum consistency score
            max_error_rate: Maximum acceptable error rate
        """
        self.consistency_threshold = consistency_threshold
        self.max_error_rate = max_error_rate
        self._run_history: List[Dict] = []

    def record_run(self, result: Dict[str, Any]) -> None:
        """Record a run result for reliability tracking."""
        self._run_history.append({
            "result": result,
            "timestamp": datetime.now().isoformat(),
            "success": result.get("success", True)
        })

    def evaluate(self) -> EvaluationResult:
        """
        Evaluate reliability from run history.

        Returns:
            EvaluationResult
        """
        if len(self._run_history) < 2:
            return EvaluationResult(
                passed=True,
                score=1.0,
                metric="reliability",
                details={"message": "Not enough runs for evaluation"},
                timestamp=datetime.now().isoformat()
            )

        # Calculate error rate
        error_count = sum(1 for r in self._run_history if not r["success"])
        error_rate = error_count / len(self._run_history)
        error_score = max(0, 1 - error_rate / self.max_error_rate)

        # Calculate consistency (compare similar inputs)
        consistency_score = self._calculate_consistency()

        scores = {
            "error_rate": error_score,
            "consistency": consistency_score,
            "total_runs": len(self._run_history),
            "error_count": error_count
        }

        avg_score = (error_score + consistency_score) / 2
        passed = avg_score >= self.consistency_threshold

        return EvaluationResult(
            passed=passed,
            score=avg_score,
            metric="reliability",
            details=scores,
            timestamp=datetime.now().isoformat()
        )

    def _calculate_consistency(self) -> float:
        """Calculate consistency across runs."""
        # Simple heuristic: successful runs are consistent
        success_count = sum(1 for r in self._run_history if r["success"])
        return success_count / len(self._run_history) if self._run_history else 1.0


class AgentJudgeEvaluator:
    """
    Use an AI agent to judge another agent's output.

    Provides qualitative assessment of:
    - Analysis quality
    - Reasoning soundness
    - Recommendation appropriateness
    """

    def __init__(
        self,
        judge_model: str = "gpt-4-turbo",
        criteria: List[str] = None
    ):
        """
        Initialize agent-as-judge evaluator.

        Args:
            judge_model: Model to use for judging
            criteria: Evaluation criteria
        """
        self.judge_model = judge_model
        self.criteria = criteria or [
            "accuracy",
            "reasoning_quality",
            "completeness",
            "actionability",
            "risk_awareness"
        ]

    def evaluate(
        self,
        query: str,
        response: str,
        context: Dict[str, Any] = None
    ) -> EvaluationResult:
        """
        Have judge agent evaluate a response.

        Args:
            query: Original query
            response: Agent's response
            context: Additional context

        Returns:
            EvaluationResult
        """
        try:
            from agno.eval import AgentAsJudgeEval

            judge_eval = AgentAsJudgeEval(
                model=self.judge_model,
                criteria=self.criteria
            )

            result = judge_eval.evaluate(
                input=query,
                output=response,
                context=context
            )

            return EvaluationResult(
                passed=result.passed,
                score=result.score,
                metric="agent_judge",
                details={
                    "criteria_scores": result.criteria_scores,
                    "feedback": result.feedback
                },
                timestamp=datetime.now().isoformat()
            )

        except ImportError:
            # Fallback: simple heuristic evaluation
            return self._heuristic_evaluate(query, response)

    def _heuristic_evaluate(self, query: str, response: str) -> EvaluationResult:
        """Fallback heuristic evaluation."""
        scores = {}

        # Check response length (not too short, not too long)
        length = len(response)
        if 100 < length < 5000:
            scores["length_appropriate"] = 1.0
        else:
            scores["length_appropriate"] = 0.5

        # Check for key financial terms
        financial_terms = ["price", "market", "risk", "return", "analysis", "recommend"]
        term_count = sum(1 for term in financial_terms if term.lower() in response.lower())
        scores["financial_relevance"] = min(1.0, term_count / 3)

        # Check for reasoning indicators
        reasoning_terms = ["because", "therefore", "however", "analysis shows", "based on"]
        reasoning_count = sum(1 for term in reasoning_terms if term.lower() in response.lower())
        scores["reasoning_present"] = min(1.0, reasoning_count / 2)

        avg_score = sum(scores.values()) / len(scores)

        return EvaluationResult(
            passed=avg_score >= 0.6,
            score=avg_score,
            metric="agent_judge_heuristic",
            details=scores,
            timestamp=datetime.now().isoformat()
        )


class EvaluationModule:
    """
    Combined evaluation manager for financial agents.
    """

    def __init__(self):
        """Initialize evaluation module."""
        self.accuracy_eval = AccuracyEvaluator()
        self.performance_eval = PerformanceEvaluator()
        self.reliability_eval = ReliabilityEvaluator()
        self.judge_eval: Optional[AgentJudgeEvaluator] = None
        self._results: List[EvaluationResult] = []

    def enable_agent_judge(self, model: str = "gpt-4-turbo") -> "EvaluationModule":
        """Enable agent-as-judge evaluation."""
        self.judge_eval = AgentJudgeEvaluator(judge_model=model)
        return self

    def evaluate_prediction(
        self,
        prediction: Dict[str, Any],
        actual: Dict[str, Any]
    ) -> EvaluationResult:
        """Evaluate prediction accuracy."""
        result = self.accuracy_eval.evaluate(prediction, actual)
        self._results.append(result)
        return result

    def evaluate_performance(self, run_metrics: Dict[str, Any]) -> EvaluationResult:
        """Evaluate run performance."""
        result = self.performance_eval.evaluate(run_metrics)
        self._results.append(result)
        self.reliability_eval.record_run({"success": result.passed, **run_metrics})
        return result

    def evaluate_reliability(self) -> EvaluationResult:
        """Evaluate overall reliability."""
        result = self.reliability_eval.evaluate()
        self._results.append(result)
        return result

    def evaluate_with_judge(
        self,
        query: str,
        response: str,
        context: Dict[str, Any] = None
    ) -> EvaluationResult:
        """Have judge agent evaluate response."""
        if not self.judge_eval:
            self.enable_agent_judge()
        result = self.judge_eval.evaluate(query, response, context)
        self._results.append(result)
        return result

    def get_summary(self) -> Dict[str, Any]:
        """Get evaluation summary."""
        if not self._results:
            return {"message": "No evaluations recorded"}

        passed_count = sum(1 for r in self._results if r.passed)
        avg_score = sum(r.score for r in self._results) / len(self._results)

        by_metric = {}
        for r in self._results:
            if r.metric not in by_metric:
                by_metric[r.metric] = []
            by_metric[r.metric].append(r.score)

        metric_averages = {
            metric: sum(scores) / len(scores)
            for metric, scores in by_metric.items()
        }

        return {
            "total_evaluations": len(self._results),
            "passed": passed_count,
            "failed": len(self._results) - passed_count,
            "pass_rate": passed_count / len(self._results),
            "average_score": avg_score,
            "by_metric": metric_averages
        }

    def clear_history(self) -> None:
        """Clear evaluation history."""
        self._results = []
        self.reliability_eval._run_history = []

    @classmethod
    def from_config(cls, config: Dict[str, Any]) -> "EvaluationModule":
        """Create from configuration."""
        module = cls()

        if config.get("accuracy"):
            acc_config = config["accuracy"]
            module.accuracy_eval = AccuracyEvaluator(
                tolerance=acc_config.get("tolerance", 0.05)
            )

        if config.get("performance"):
            perf_config = config["performance"]
            module.performance_eval = PerformanceEvaluator(
                max_latency_ms=perf_config.get("max_latency_ms", 5000),
                max_tokens=perf_config.get("max_tokens", 4000)
            )

        if config.get("agent_judge"):
            module.enable_agent_judge(
                model=config["agent_judge"].get("model", "gpt-4-turbo")
            )

        return module
