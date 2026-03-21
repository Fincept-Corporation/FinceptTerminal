"""
Confidence Scorer

Calculate confidence scores for trading signals based on multiple factors.
Inspired by Alpha Arena's 0-1 confidence scoring system.
"""

from typing import Dict, List
import math


class ConfidenceScorer:
    """
    Calculate confidence scores (0.0 to 1.0) for trading signals

    Factors considered:
    - Technical indicator alignment
    - Trend strength
    - Volume confirmation
    - Risk/reward ratio
    - Market volatility
    """

    @staticmethod
    def calculate_comprehensive_confidence(
        indicator_alignment: float = 0.5,
        trend_strength: float = 0.5,
        volume_confirmation: float = 0.5,
        risk_reward_ratio: float = 2.0,
        volatility_factor: float = 0.5,
        weights: Dict[str, float] = None
    ) -> Dict[str, float]:
        """
        Calculate overall confidence score

        Args:
            indicator_alignment: 0-1, how many indicators agree
            trend_strength: 0-1, strength of trend
            volume_confirmation: 0-1, volume supports the move
            risk_reward_ratio: Actual R:R ratio
            volatility_factor: 0-1, volatility level (0=low, 1=high)
            weights: Custom weights for each factor

        Returns:
            Dictionary with confidence score and breakdown
        """

        # Default weights
        if weights is None:
            weights = {
                'indicators': 0.30,
                'trend': 0.25,
                'volume': 0.20,
                'risk_reward': 0.15,
                'volatility': 0.10
            }

        # Normalize risk/reward to 0-1 scale (assuming 3:1 is excellent)
        rr_normalized = min(risk_reward_ratio / 3.0, 1.0)

        # Lower volatility = higher confidence
        volatility_score = 1.0 - volatility_factor

        # Calculate weighted score
        confidence = (
            indicator_alignment * weights['indicators'] +
            trend_strength * weights['trend'] +
            volume_confirmation * weights['volume'] +
            rr_normalized * weights['risk_reward'] +
            volatility_score * weights['volatility']
        )

        # Ensure 0-1 range
        confidence = max(0.0, min(1.0, confidence))

        return {
            "confidence": round(confidence, 3),
            "breakdown": {
                "indicator_alignment": round(indicator_alignment, 2),
                "trend_strength": round(trend_strength, 2),
                "volume_confirmation": round(volume_confirmation, 2),
                "risk_reward_score": round(rr_normalized, 2),
                "volatility_score": round(volatility_score, 2)
            },
            "rating": ConfidenceScorer._get_confidence_rating(confidence)
        }

    @staticmethod
    def calculate_indicator_alignment(
        indicators: Dict[str, str]
    ) -> float:
        """
        Calculate how well indicators align

        Args:
            indicators: Dict of indicator signals (e.g., {'RSI': 'buy', 'MACD': 'buy', 'EMA': 'sell'})

        Returns:
            Alignment score 0-1
        """

        if not indicators:
            return 0.5

        signals = list(indicators.values())
        buy_count = signals.count('buy') + signals.count('bullish')
        sell_count = signals.count('sell') + signals.count('bearish')
        total = len(signals)

        # If all agree, score is 1.0
        alignment = max(buy_count, sell_count) / total if total > 0 else 0

        return alignment

    @staticmethod
    def calculate_trend_strength(
        current_price: float,
        ma_20: float,
        ma_50: float,
        ma_200: float = None
    ) -> float:
        """
        Calculate trend strength from moving averages

        Args:
            current_price: Current market price
            ma_20: 20-period MA
            ma_50: 50-period MA
            ma_200: 200-period MA (optional)

        Returns:
            Trend strength 0-1
        """

        score = 0.0

        # Check if price is trending
        if current_price > ma_20 > ma_50:
            score = 0.6  # Uptrend
        elif current_price < ma_20 < ma_50:
            score = 0.6  # Downtrend

        # Bonus for 200 MA alignment
        if ma_200:
            if (current_price > ma_20 > ma_50 > ma_200) or \
               (current_price < ma_20 < ma_50 < ma_200):
                score = 1.0  # Strong trend

        return score

    @staticmethod
    def calculate_volume_confirmation(
        current_volume: float,
        avg_volume: float,
        volume_threshold: float = 1.2
    ) -> float:
        """
        Calculate volume confirmation score

        Args:
            current_volume: Current volume
            avg_volume: Average volume
            volume_threshold: Multiplier for significant volume

        Returns:
            Volume confirmation 0-1
        """

        if avg_volume == 0:
            return 0.5

        volume_ratio = current_volume / avg_volume

        if volume_ratio >= volume_threshold:
            # High volume = strong confirmation
            return min(volume_ratio / 2.0, 1.0)
        else:
            # Low volume = weak confirmation
            return 0.3

    @staticmethod
    def adjust_for_market_conditions(
        base_confidence: float,
        market_condition: str
    ) -> float:
        """
        Adjust confidence based on market conditions

        Args:
            base_confidence: Base confidence score
            market_condition: 'trending', 'ranging', 'volatile', 'calm'

        Returns:
            Adjusted confidence
        """

        adjustments = {
            'trending': 1.1,      # Boost in trending markets
            'ranging': 0.9,       # Reduce in ranging markets
            'volatile': 0.85,     # Reduce in volatile markets
            'calm': 1.0,          # No change in calm markets
        }

        multiplier = adjustments.get(market_condition, 1.0)
        adjusted = base_confidence * multiplier

        return max(0.0, min(1.0, adjusted))

    @staticmethod
    def _get_confidence_rating(confidence: float) -> str:
        """Get text rating for confidence score"""

        if confidence >= 0.8:
            return "Very High"
        elif confidence >= 0.7:
            return "High"
        elif confidence >= 0.6:
            return "Medium"
        elif confidence >= 0.5:
            return "Low"
        else:
            return "Very Low"

    @staticmethod
    def recommend_action(confidence: float, min_threshold: float = 0.6) -> str:
        """
        Recommend action based on confidence

        Args:
            confidence: Confidence score
            min_threshold: Minimum confidence to trade

        Returns:
            Recommended action
        """

        if confidence >= min_threshold:
            if confidence >= 0.8:
                return "STRONG_EXECUTE"
            elif confidence >= 0.7:
                return "EXECUTE"
            else:
                return "EXECUTE_WITH_CAUTION"
        else:
            return "SKIP"
