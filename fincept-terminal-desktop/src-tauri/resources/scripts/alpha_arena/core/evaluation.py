"""
Evaluation Module

Tracks and evaluates agent prediction accuracy and performance.
"""

from datetime import datetime
from typing import Dict, List, Optional, Any
from pydantic import BaseModel, Field
import math

from alpha_arena.types.models import ModelDecision, TradeAction, TradeResult
from alpha_arena.utils.logging import get_logger

logger = get_logger("evaluation")


class PredictionRecord(BaseModel):
    """Record of a prediction for evaluation."""

    decision_id: str
    model_name: str
    action: str
    symbol: str
    confidence: float
    price_at_decision: float
    timestamp: datetime = Field(default_factory=datetime.now)

    # Outcome data (filled in later)
    price_after: Optional[float] = None
    actual_return: Optional[float] = None
    was_correct: Optional[bool] = None
    pnl: Optional[float] = None


class ModelMetrics(BaseModel):
    """Performance metrics for a model."""

    model_name: str
    total_decisions: int = 0
    correct_decisions: int = 0
    total_pnl: float = 0.0
    win_rate: float = 0.0
    avg_confidence: float = 0.0
    avg_return: float = 0.0

    # Calibration metrics
    confidence_accuracy_correlation: float = 0.0
    overconfidence_ratio: float = 0.0

    # Risk metrics
    sharpe_ratio: float = 0.0
    max_drawdown: float = 0.0
    profit_factor: float = 0.0

    # Trade statistics
    avg_trade_pnl: float = 0.0
    best_trade: float = 0.0
    worst_trade: float = 0.0
    consecutive_wins: int = 0
    consecutive_losses: int = 0


class EvaluationResult(BaseModel):
    """Result of an evaluation."""

    passed: bool = True
    score: float = 0.0
    details: Dict[str, Any] = Field(default_factory=dict)


class EvaluationModule:
    """
    Evaluates agent prediction accuracy and trading performance.
    """

    def __init__(self, lookback_periods: int = 1):
        self.lookback_periods = lookback_periods
        self._predictions: Dict[str, List[PredictionRecord]] = {}
        self._returns: Dict[str, List[float]] = {}
        self._pnls: Dict[str, List[float]] = {}

    def record_decision(
        self,
        decision: ModelDecision,
        trade_result: Optional[TradeResult] = None,
    ):
        """
        Record a decision for later evaluation.

        Args:
            decision: The trading decision made
            trade_result: Optional trade execution result
        """
        model_name = decision.model_name

        if model_name not in self._predictions:
            self._predictions[model_name] = []
            self._returns[model_name] = []
            self._pnls[model_name] = []

        record = PredictionRecord(
            decision_id=f"{decision.competition_id}_{decision.model_name}_{decision.cycle_number}",
            model_name=model_name,
            action=decision.action.value if hasattr(decision.action, 'value') else str(decision.action),
            symbol=decision.symbol,
            confidence=decision.confidence,
            price_at_decision=decision.price_at_decision or 0,
        )

        if trade_result and trade_result.pnl is not None:
            record.pnl = trade_result.pnl
            self._pnls[model_name].append(trade_result.pnl)

        self._predictions[model_name].append(record)

    def update_outcome(
        self,
        model_name: str,
        current_price: float,
        lookback: int = 1,
    ):
        """
        Update prediction outcomes with current price.

        Args:
            model_name: Model to update
            current_price: Current market price
            lookback: How many previous predictions to update
        """
        if model_name not in self._predictions:
            return

        predictions = self._predictions[model_name]

        for i in range(max(0, len(predictions) - lookback), len(predictions)):
            pred = predictions[i]
            if pred.price_after is not None:
                continue

            pred.price_after = current_price
            price_change = (current_price - pred.price_at_decision) / pred.price_at_decision

            pred.actual_return = price_change

            # Determine if prediction was correct
            if pred.action == "hold":
                pred.was_correct = True  # Hold is neutral
            elif pred.action in ["buy", "long"]:
                pred.was_correct = price_change > 0
            elif pred.action in ["sell", "short"]:
                pred.was_correct = price_change < 0
            else:
                pred.was_correct = None

            self._returns[model_name].append(price_change)

    def get_model_metrics(self, model_name: str) -> ModelMetrics:
        """
        Calculate comprehensive metrics for a model.

        Args:
            model_name: Model to evaluate

        Returns:
            ModelMetrics with all performance metrics
        """
        if model_name not in self._predictions:
            return ModelMetrics(model_name=model_name)

        predictions = self._predictions[model_name]
        if not predictions:
            return ModelMetrics(model_name=model_name)

        # Basic counts
        total = len(predictions)
        evaluated = [p for p in predictions if p.was_correct is not None]
        correct = sum(1 for p in evaluated if p.was_correct)

        # Win rate
        win_rate = correct / len(evaluated) if evaluated else 0

        # Average confidence
        avg_confidence = sum(p.confidence for p in predictions) / total

        # Returns and P&L
        returns = self._returns.get(model_name, [])
        pnls = self._pnls.get(model_name, [])

        avg_return = sum(returns) / len(returns) if returns else 0
        total_pnl = sum(pnls)

        # Trade statistics
        if pnls:
            avg_trade_pnl = total_pnl / len(pnls)
            best_trade = max(pnls)
            worst_trade = min(pnls)
        else:
            avg_trade_pnl = 0
            best_trade = 0
            worst_trade = 0

        # Sharpe ratio (simplified - assumes risk-free rate of 0)
        if returns and len(returns) > 1:
            mean_return = sum(returns) / len(returns)
            variance = sum((r - mean_return) ** 2 for r in returns) / len(returns)
            std_dev = math.sqrt(variance) if variance > 0 else 0
            sharpe_ratio = mean_return / std_dev if std_dev > 0 else 0
        else:
            sharpe_ratio = 0

        # Maximum drawdown
        max_drawdown = self._calculate_max_drawdown(pnls)

        # Profit factor
        gains = sum(p for p in pnls if p > 0)
        losses = abs(sum(p for p in pnls if p < 0))
        profit_factor = gains / losses if losses > 0 else float('inf') if gains > 0 else 0

        # Consecutive wins/losses
        consecutive_wins, consecutive_losses = self._calculate_streaks(pnls)

        # Calibration metrics
        confidence_accuracy = self._calculate_calibration(evaluated)

        return ModelMetrics(
            model_name=model_name,
            total_decisions=total,
            correct_decisions=correct,
            total_pnl=total_pnl,
            win_rate=win_rate,
            avg_confidence=avg_confidence,
            avg_return=avg_return,
            confidence_accuracy_correlation=confidence_accuracy,
            sharpe_ratio=sharpe_ratio,
            max_drawdown=max_drawdown,
            profit_factor=profit_factor,
            avg_trade_pnl=avg_trade_pnl,
            best_trade=best_trade,
            worst_trade=worst_trade,
            consecutive_wins=consecutive_wins,
            consecutive_losses=consecutive_losses,
        )

    def _calculate_max_drawdown(self, pnls: List[float]) -> float:
        """Calculate maximum drawdown from P&L series."""
        if not pnls:
            return 0

        cumulative = 0
        peak = 0
        max_dd = 0

        for pnl in pnls:
            cumulative += pnl
            peak = max(peak, cumulative)
            drawdown = peak - cumulative
            max_dd = max(max_dd, drawdown)

        return max_dd

    def _calculate_streaks(self, pnls: List[float]) -> tuple:
        """Calculate consecutive win/loss streaks."""
        if not pnls:
            return 0, 0

        max_wins = 0
        max_losses = 0
        current_wins = 0
        current_losses = 0

        for pnl in pnls:
            if pnl > 0:
                current_wins += 1
                current_losses = 0
                max_wins = max(max_wins, current_wins)
            elif pnl < 0:
                current_losses += 1
                current_wins = 0
                max_losses = max(max_losses, current_losses)
            else:
                current_wins = 0
                current_losses = 0

        return max_wins, max_losses

    def _calculate_calibration(self, predictions: List[PredictionRecord]) -> float:
        """
        Calculate calibration score.

        A well-calibrated model has confidence scores that match actual accuracy.
        """
        if len(predictions) < 5:
            return 0

        # Bin predictions by confidence
        bins = [(0, 0.25), (0.25, 0.5), (0.5, 0.75), (0.75, 1.0)]
        calibration_errors = []

        for low, high in bins:
            bin_preds = [p for p in predictions if low <= p.confidence < high]
            if bin_preds:
                expected_accuracy = (low + high) / 2
                actual_accuracy = sum(1 for p in bin_preds if p.was_correct) / len(bin_preds)
                calibration_errors.append(abs(expected_accuracy - actual_accuracy))

        return 1 - (sum(calibration_errors) / len(calibration_errors)) if calibration_errors else 0

    def evaluate_prediction(
        self,
        prediction: Dict[str, Any],
        actual: Dict[str, Any],
    ) -> EvaluationResult:
        """
        Evaluate a single prediction against actual outcome.

        Args:
            prediction: Predicted values
            actual: Actual observed values

        Returns:
            EvaluationResult with score and details
        """
        score = 0.0
        details = {}

        predicted_action = prediction.get("action", "hold").lower()
        predicted_price = prediction.get("price")
        actual_price = actual.get("price")

        if predicted_price and actual_price:
            price_diff = actual_price - predicted_price
            price_diff_pct = (price_diff / predicted_price) * 100

            # Check if direction was correct
            if predicted_action == "buy" and price_diff > 0:
                score = 1.0
                details["direction_correct"] = True
            elif predicted_action in ["sell", "short"] and price_diff < 0:
                score = 1.0
                details["direction_correct"] = True
            elif predicted_action == "hold":
                score = 0.5  # Neutral
                details["direction_correct"] = None
            else:
                score = 0.0
                details["direction_correct"] = False

            details["price_change_pct"] = price_diff_pct

        return EvaluationResult(
            passed=score >= 0.5,
            score=score,
            details=details,
        )

    def get_comparison_report(self) -> Dict[str, ModelMetrics]:
        """
        Get comparison report of all models.

        Returns:
            Dict of model name to metrics
        """
        return {
            name: self.get_model_metrics(name)
            for name in self._predictions.keys()
        }

    def clear_history(self, model_name: Optional[str] = None):
        """Clear prediction history."""
        if model_name:
            self._predictions.pop(model_name, None)
            self._returns.pop(model_name, None)
            self._pnls.pop(model_name, None)
        else:
            self._predictions.clear()
            self._returns.clear()
            self._pnls.clear()
