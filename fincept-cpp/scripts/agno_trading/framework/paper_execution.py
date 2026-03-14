"""
Paper trading execution gateway.
Simulates order fills without connecting to a real exchange.
"""

import time
from typing import List
from .base_execution import BaseExecutionGateway
from .types import TradeInstruction, TxResult, FeatureVector, TxStatus, Side


class PaperExecutionGateway(BaseExecutionGateway):
    """
    Paper trading execution gateway.

    Simulates instant fills at market price with configurable slippage and fees.
    """

    def __init__(self, fee_bps: float = 10.0, default_slippage_bps: float = 5.0):
        """
        Initialize paper trading gateway.

        Args:
            fee_bps: Trading fee in basis points (default 10 bps = 0.1%)
            default_slippage_bps: Default slippage in basis points (default 5 bps = 0.05%)
        """
        self.fee_bps = fee_bps
        self.default_slippage_bps = default_slippage_bps

    async def execute(
        self,
        instructions: List[TradeInstruction],
        market_features: List[FeatureVector]
    ) -> List[TxResult]:
        """
        Execute trade instructions in paper trading mode.

        Simulates fills at current market price ± slippage.
        """
        results = []

        # Build price map from features
        price_map = {
            feat.symbol: feat.close
            for feat in market_features
        }

        timestamp_ms = int(time.time() * 1000)

        for inst in instructions:
            # Skip HOLD instructions
            if inst.side == Side.HOLD:
                continue

            # Get reference price
            if inst.symbol not in price_map:
                # Reject if no price available
                results.append(TxResult(
                    instruction=inst,
                    status=TxStatus.REJECTED,
                    error_message=f"No market price available for {inst.symbol}",
                    timestamp_ms=timestamp_ms
                ))
                continue

            reference_price = price_map[inst.symbol]

            # Apply slippage
            slippage_bps = min(inst.max_slippage_bps, self.default_slippage_bps)
            slippage_factor = slippage_bps / 10000.0

            if inst.side == Side.BUY:
                # Buy at ask (higher price)
                fill_price = reference_price * (1 + slippage_factor)
            else:  # SELL
                # Sell at bid (lower price)
                fill_price = reference_price * (1 - slippage_factor)

            # Calculate notional and fee
            notional = inst.quantity * fill_price
            fee = notional * (self.fee_bps / 10000.0)

            # Create successful fill
            results.append(TxResult(
                instruction=inst,
                status=TxStatus.FILLED,
                fill_price=fill_price,
                fill_quantity=inst.quantity,
                fee=fee,
                notional=notional,
                timestamp_ms=timestamp_ms
            ))

        return results

    async def test_connection(self) -> bool:
        """
        Paper trading always "connected".
        """
        return True

    async def close(self) -> None:
        """
        No cleanup needed for paper trading.
        """
        pass
