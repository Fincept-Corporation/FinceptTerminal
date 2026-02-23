"""
Triple Barrier Method - Industry-standard financial labeling
Based on: Lopez de Prado, M. (2018). Advances in Financial Machine Learning.

Labels:
  +1: Price hit upper barrier (take-profit) first -> Bullish
   0: Timed out without hitting either barrier -> Neutral
  -1: Price hit lower barrier (stop-loss) first -> Bearish
"""

import pandas as pd
import numpy as np
from typing import Tuple, Dict, List


class TripleBarrierLabeler:
    """
    Generate triple-barrier labels for a price series.

    Usage:
        labeler = TripleBarrierLabeler(upper=0.05, lower=0.03, max_hold=20)
        labels = labeler.generate_labels(price_series)
    """

    def __init__(self, upper_barrier: float = 0.05, lower_barrier: float = 0.03, max_holding_period: int = 20):
        self.upper = upper_barrier
        self.lower = lower_barrier  # stored positive, negated when checking
        self.max_hold = max_holding_period

    def generate_labels(self, prices: pd.Series, return_details: bool = False):
        if len(prices) < self.max_hold + 1:
            raise ValueError(f"Price series too short ({len(prices)}), need at least {self.max_hold + 1}")

        labels = pd.Series(index=prices.index, dtype=float)
        labels[:] = np.nan
        details = [] if return_details else None

        for i in range(len(prices) - self.max_hold):
            entry_price = prices.iloc[i]
            entry_date = prices.index[i]
            future_prices = prices.iloc[i + 1:i + 1 + self.max_hold]
            future_returns = (future_prices - entry_price) / entry_price
            label, hit_day, hit_type = self._check_barriers(future_returns)
            labels.iloc[i] = label

            if return_details:
                details.append({
                    "date": entry_date,
                    "entry_price": entry_price,
                    "label": label,
                    "hit_day": hit_day,
                    "hit_type": hit_type,
                    "max_return": future_returns.max(),
                    "min_return": future_returns.min(),
                    "final_return": future_returns.iloc[-1] if len(future_returns) > 0 else 0,
                })

        if return_details:
            return labels, pd.DataFrame(details)
        return labels

    def _check_barriers(self, returns: pd.Series) -> Tuple[int, int, str]:
        upper_touch = returns >= self.upper
        lower_touch = returns <= -self.lower

        upper_idx = np.where(upper_touch)[0]
        first_upper = upper_idx[0] if len(upper_idx) > 0 else np.inf

        lower_idx = np.where(lower_touch)[0]
        first_lower = lower_idx[0] if len(lower_idx) > 0 else np.inf

        if first_upper == np.inf and first_lower == np.inf:
            return 0, self.max_hold, "timeout"
        elif first_upper <= first_lower:
            return 1, int(first_upper) + 1, "upper"
        else:
            return -1, int(first_lower) + 1, "lower"

    def get_statistics(self, labels: pd.Series) -> Dict:
        valid = labels.dropna()
        total = len(valid)
        if total == 0:
            return {"total": 0, "bullish": 0, "neutral": 0, "bearish": 0}

        bullish = int((valid == 1).sum())
        neutral = int((valid == 0).sum())
        bearish = int((valid == -1).sum())

        return {
            "total": total,
            "bullish": bullish,
            "bullish_pct": round(bullish / total * 100, 2),
            "neutral": neutral,
            "neutral_pct": round(neutral / total * 100, 2),
            "bearish": bearish,
            "bearish_pct": round(bearish / total * 100, 2),
            "bull_bear_ratio": round(bullish / bearish, 2) if bearish > 0 else float("inf"),
        }


class TripleBarrierPredictor:
    """Predict triple-barrier outcomes from historical pattern matches."""

    def __init__(self, labeler: TripleBarrierLabeler = None):
        self.labeler = labeler or TripleBarrierLabeler()

    def predict_from_matches(self, matches: List[Dict], min_matches: int = 5) -> Dict:
        """
        Predict outcome from Top-K similar pattern matches.
        Each match dict must have: symbol, date, score, future_prices (pd.Series of Close).
        """
        results = []

        for match in matches:
            try:
                entry_price = match["entry_price"]
                future_prices = match["future_prices"]
                score = match.get("score", 0.5)

                if future_prices is None or len(future_prices) < 2:
                    continue

                returns = (future_prices - entry_price) / entry_price
                label, hit_day, hit_type = self.labeler._check_barriers(returns)

                results.append({
                    "symbol": match.get("symbol", ""),
                    "date": match.get("date", ""),
                    "score": score,
                    "label": label,
                    "hit_day": hit_day,
                    "hit_type": hit_type,
                    "max_return": float(returns.max()) * 100,
                    "min_return": float(returns.min()) * 100,
                    "final_return": float(returns.iloc[-1]) * 100 if len(returns) > 0 else 0,
                })
            except Exception:
                continue

        if len(results) < min_matches:
            return {
                "valid": False,
                "message": f"Insufficient matches ({len(results)}/{min_matches})",
                "matches_count": len(results),
            }

        df_r = pd.DataFrame(results)
        total = len(df_r)
        bullish = int((df_r["label"] == 1).sum())
        neutral = int((df_r["label"] == 0).sum())
        bearish = int((df_r["label"] == -1).sum())

        weights = df_r["score"].values
        weights = weights / (weights.sum() + 1e-8)

        weighted_label = float((df_r["label"] * weights).sum())
        expected_return = float((df_r["final_return"] * weights).sum())

        if bullish > bearish and bullish >= total * 0.4:
            prediction = "BULLISH"
            confidence = bullish / total
        elif bearish > bullish and bearish >= total * 0.4:
            prediction = "BEARISH"
            confidence = bearish / total
        else:
            prediction = "NEUTRAL"
            confidence = neutral / total

        return {
            "valid": True,
            "prediction": prediction,
            "confidence": round(confidence * 100, 1),
            "weighted_label": round(weighted_label, 3),
            "bullish_count": bullish,
            "bullish_pct": round(bullish / total * 100, 1),
            "neutral_count": neutral,
            "neutral_pct": round(neutral / total * 100, 1),
            "bearish_count": bearish,
            "bearish_pct": round(bearish / total * 100, 1),
            "avg_max_return": round(df_r["max_return"].mean(), 2),
            "avg_min_return": round(df_r["min_return"].mean(), 2),
            "avg_final_return": round(df_r["final_return"].mean(), 2),
            "expected_return": round(expected_return, 2),
            "avg_hit_day": round(df_r["hit_day"].mean(), 1),
            "matches_count": total,
            "details": results,
        }


def calculate_win_loss_ratio(matches_results: List[Dict]) -> Tuple[float, float]:
    """Calculate win rate and win/loss ratio (for Kelly criterion)."""
    if not matches_results:
        return 0.5, 1.0

    wins = [r["final_return"] for r in matches_results if r.get("final_return", 0) > 0]
    losses = [abs(r["final_return"]) for r in matches_results if r.get("final_return", 0) < 0]

    total = len(wins) + len(losses)
    if total == 0:
        return 0.5, 1.0

    win_rate = len(wins) / total
    avg_win = np.mean(wins) if wins else 0
    avg_loss = np.mean(losses) if losses else 1
    win_loss_ratio = avg_win / avg_loss if avg_loss > 0 else 1.0

    return win_rate, win_loss_ratio
