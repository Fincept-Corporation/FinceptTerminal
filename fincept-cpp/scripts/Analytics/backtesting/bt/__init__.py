"""
bt (Flexible Portfolio Backtesting) Provider

Portfolio-level strategy backtesting with composable algo blocks:
- Equal-weight, inverse volatility, mean-variance, risk parity
- Momentum selection and trend following
- Composable algo pipeline (bt.algos)

Uses bt library (MIT license, v1.1.2+) with ffn for performance stats.
"""

from .bt_provider import BtProvider

__all__ = ['BtProvider']
