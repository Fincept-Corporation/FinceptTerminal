"""
TP/SL Calculator

Dynamic Take Profit and Stop Loss calculation based on volatility and market conditions.
Inspired by Alpha Arena's adaptive TP/SL logic.
"""

from typing import Dict, Tuple
import math


class TPSLCalculator:
    """
    Calculate dynamic TP/SL levels based on:
    - Current volatility (ATR)
    - Risk/reward ratio
    - Market conditions
    """

    @staticmethod
    def calculate_dynamic_tp_sl(
        entry_price: float,
        direction: str,
        volatility: float,
        risk_reward_ratio: float = 2.0,
        atr_multiplier: float = 2.0
    ) -> Dict[str, float]:
        """
        Calculate TP/SL levels dynamically

        Args:
            entry_price: Entry price
            direction: 'long' or 'short'
            volatility: Current volatility (ATR or standard deviation)
            risk_reward_ratio: Desired risk/reward ratio (default 2:1)
            atr_multiplier: Multiplier for ATR to set SL distance

        Returns:
            Dictionary with stop_loss, take_profit, risk_amount, reward_amount
        """

        # Calculate stop loss distance based on volatility
        sl_distance = volatility * atr_multiplier

        if direction.lower() == 'long':
            stop_loss = entry_price - sl_distance
            take_profit = entry_price + (sl_distance * risk_reward_ratio)
        else:  # short
            stop_loss = entry_price + sl_distance
            take_profit = entry_price - (sl_distance * risk_reward_ratio)

        return {
            "stop_loss": round(stop_loss, 2),
            "take_profit": round(take_profit, 2),
            "sl_distance": round(sl_distance, 2),
            "tp_distance": round(sl_distance * risk_reward_ratio, 2),
            "risk_reward_ratio": risk_reward_ratio
        }

    @staticmethod
    def calculate_from_percentage(
        entry_price: float,
        direction: str,
        sl_percent: float = 2.0,
        tp_percent: float = 5.0
    ) -> Dict[str, float]:
        """
        Calculate TP/SL based on percentage

        Args:
            entry_price: Entry price
            direction: 'long' or 'short'
            sl_percent: Stop loss percentage (e.g., 2.0 for 2%)
            tp_percent: Take profit percentage (e.g., 5.0 for 5%)

        Returns:
            Dictionary with stop_loss and take_profit
        """

        sl_amount = entry_price * (sl_percent / 100)
        tp_amount = entry_price * (tp_percent / 100)

        if direction.lower() == 'long':
            stop_loss = entry_price - sl_amount
            take_profit = entry_price + tp_amount
        else:  # short
            stop_loss = entry_price + sl_amount
            take_profit = entry_price - tp_amount

        return {
            "stop_loss": round(stop_loss, 2),
            "take_profit": round(take_profit, 2),
            "sl_percent": sl_percent,
            "tp_percent": tp_percent,
            "risk_reward_ratio": tp_percent / sl_percent
        }

    @staticmethod
    def calculate_trailing_stop(
        entry_price: float,
        current_price: float,
        direction: str,
        trail_percent: float = 1.0
    ) -> float:
        """
        Calculate trailing stop loss

        Args:
            entry_price: Original entry price
            current_price: Current market price
            direction: 'long' or 'short'
            trail_percent: Trailing distance as percentage

        Returns:
            New stop loss level
        """

        if direction.lower() == 'long':
            # For long, trail below current price
            trailing_stop = current_price * (1 - trail_percent / 100)
            # Never move stop down
            original_stop = entry_price * (1 - trail_percent / 100)
            return max(trailing_stop, original_stop)
        else:  # short
            # For short, trail above current price
            trailing_stop = current_price * (1 + trail_percent / 100)
            # Never move stop up
            original_stop = entry_price * (1 + trail_percent / 100)
            return min(trailing_stop, original_stop)

    @staticmethod
    def estimate_volatility_from_price_history(
        prices: list,
        period: int = 14
    ) -> float:
        """
        Estimate volatility from price history using ATR approximation

        Args:
            prices: List of recent prices
            period: Period for calculation

        Returns:
            Estimated volatility
        """

        if len(prices) < period + 1:
            # Use simple percentage of current price if not enough data
            return prices[-1] * 0.02 if prices else 0

        # Calculate simple volatility as average price range
        ranges = []
        for i in range(len(prices) - period, len(prices)):
            if i > 0:
                ranges.append(abs(prices[i] - prices[i-1]))

        avg_range = sum(ranges) / len(ranges) if ranges else 0
        return avg_range

    @staticmethod
    def validate_tp_sl(
        entry_price: float,
        stop_loss: float,
        take_profit: float,
        direction: str,
        min_risk_reward: float = 1.5
    ) -> Dict[str, any]:
        """
        Validate TP/SL levels

        Args:
            entry_price: Entry price
            stop_loss: Stop loss level
            take_profit: Take profit level
            direction: 'long' or 'short'
            min_risk_reward: Minimum acceptable risk/reward ratio

        Returns:
            Validation result with errors if any
        """

        errors = []
        warnings = []

        if direction.lower() == 'long':
            # For long: SL < entry < TP
            if stop_loss >= entry_price:
                errors.append("Stop loss must be below entry price for long positions")

            if take_profit <= entry_price:
                errors.append("Take profit must be above entry price for long positions")

            if stop_loss < entry_price and take_profit > entry_price:
                risk = entry_price - stop_loss
                reward = take_profit - entry_price
                rr_ratio = reward / risk if risk > 0 else 0

                if rr_ratio < min_risk_reward:
                    warnings.append(f"Risk/reward ratio {rr_ratio:.2f} is below minimum {min_risk_reward}")

        else:  # short
            # For short: SL > entry > TP
            if stop_loss <= entry_price:
                errors.append("Stop loss must be above entry price for short positions")

            if take_profit >= entry_price:
                errors.append("Take profit must be below entry price for short positions")

            if stop_loss > entry_price and take_profit < entry_price:
                risk = stop_loss - entry_price
                reward = entry_price - take_profit
                rr_ratio = reward / risk if risk > 0 else 0

                if rr_ratio < min_risk_reward:
                    warnings.append(f"Risk/reward ratio {rr_ratio:.2f} is below minimum {min_risk_reward}")

        return {
            "valid": len(errors) == 0,
            "errors": errors,
            "warnings": warnings
        }
