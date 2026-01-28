"""
Fast-Trade Analysis Module

Trade execution logic wrapper covering fast_trade.run_analysis:

Position Management:
- enter_position(): Execute entry with lot sizing and commission
- exit_position(): Execute exit with commission calculation
- apply_logic_to_df(): Apply trading logic to full DataFrame

Fee Calculation:
- calculate_fee(): Commission calculation for order size

Currency Conversion:
- convert_aux_to_base(): Convert auxiliary currency to base
- convert_base_to_aux(): Convert base currency to auxiliary

Account Management:
- calculate_new_account_value_on_enter(): Compute new balance after entry
"""

import pandas as pd
import numpy as np
from typing import Dict, Any, List, Optional, Tuple


# ============================================================================
# Fee Calculation
# ============================================================================

def calculate_fee(order_size: float, comission: float) -> float:
    """
    Calculate commission fee for a trade.

    Args:
        order_size: Total order value
        comission: Commission rate (e.g. 0.001 for 0.1%)

    Returns:
        Fee amount
    """
    try:
        from fast_trade.run_analysis import calculate_fee as ft_fee
        return ft_fee(order_size, comission)
    except ImportError:
        return order_size * comission


# ============================================================================
# Currency Conversion
# ============================================================================

def convert_aux_to_base(new_aux: float, close: float) -> float:
    """
    Convert auxiliary currency to base currency.

    In crypto trading, converts coin amount to USD equivalent.

    Args:
        new_aux: Amount in auxiliary currency
        close: Current close price

    Returns:
        Base currency amount
    """
    try:
        from fast_trade.run_analysis import convert_aux_to_base as ft_convert
        return ft_convert(new_aux, close)
    except ImportError:
        return new_aux * close


def convert_base_to_aux(new_base: float, close: float) -> float:
    """
    Convert base currency to auxiliary currency.

    In crypto trading, converts USD to coin amount.

    Args:
        new_base: Amount in base currency
        close: Current close price

    Returns:
        Auxiliary currency amount
    """
    try:
        from fast_trade.run_analysis import convert_base_to_aux as ft_convert
        return ft_convert(new_base, close)
    except ImportError:
        if close == 0:
            return 0.0
        return new_base / close


# ============================================================================
# Account Value Calculation
# ============================================================================

def calculate_new_account_value_on_enter(
    base_transaction_amount: float,
    account_value_list: List[float],
    account_value: float
) -> float:
    """
    Calculate new account value when entering a position.

    Determines how much capital to allocate to the new position.

    Args:
        base_transaction_amount: Base trade size
        account_value_list: History of account values
        account_value: Current account value

    Returns:
        New account value after entry
    """
    try:
        from fast_trade.run_analysis import calculate_new_account_value_on_enter as ft_calc
        return ft_calc(base_transaction_amount, account_value_list, account_value)
    except ImportError:
        return account_value - base_transaction_amount


# ============================================================================
# Position Entry & Exit
# ============================================================================

def enter_position(
    account_value_list: List[float],
    lot_size: float,
    account_value: float,
    max_lot_size: float,
    close: float,
    comission: float
) -> Tuple[float, float, float]:
    """
    Execute entry into a position.

    Handles lot sizing, max position limits, and commission deduction.

    Args:
        account_value_list: History of account values
        lot_size: Desired position size (as fraction of account)
        account_value: Current account value
        max_lot_size: Maximum allowed position size
        close: Current close price
        comission: Commission rate

    Returns:
        Tuple of (new_account_value, position_size, fee)
    """
    try:
        from fast_trade.run_analysis import enter_position as ft_enter
        return ft_enter(
            account_value_list, lot_size, account_value,
            max_lot_size, close, comission
        )
    except ImportError:
        trade_amount = min(account_value * lot_size, account_value * max_lot_size)
        fee = calculate_fee(trade_amount, comission)
        position = convert_base_to_aux(trade_amount - fee, close)
        new_value = account_value - trade_amount
        return new_value, position, fee


def exit_position(
    account_value_list: List[float],
    close: float,
    new_aux: float,
    comission: float
) -> Tuple[float, float]:
    """
    Execute exit from a position.

    Converts position back to base currency and deducts commission.

    Args:
        account_value_list: History of account values
        close: Current close price
        new_aux: Position size in auxiliary currency
        comission: Commission rate

    Returns:
        Tuple of (new_account_value, fee)
    """
    try:
        from fast_trade.run_analysis import exit_position as ft_exit
        return ft_exit(account_value_list, close, new_aux, comission)
    except ImportError:
        base_value = convert_aux_to_base(new_aux, close)
        fee = calculate_fee(base_value, comission)
        new_value = base_value - fee
        if account_value_list:
            new_value += account_value_list[-1]
        return new_value, fee


# ============================================================================
# Full Logic Application
# ============================================================================

def apply_logic_to_df(
    df: pd.DataFrame,
    backtest: Dict[str, Any]
) -> pd.DataFrame:
    """
    Apply complete trading logic to a DataFrame.

    Processes entry/exit signals and simulates portfolio equity changes
    including commission, lot sizing, and trailing stops.

    This is the core simulation engine that:
    1. Iterates through each bar
    2. Checks entry/exit conditions
    3. Executes trades with proper sizing
    4. Tracks account value and positions
    5. Applies trailing stop losses
    6. Records all actions and equity values

    Args:
        df: DataFrame with OHLCV + indicator columns and 'action' column
        backtest: Strategy config with:
            - base_balance: Starting capital
            - comission: Commission rate
            - trailing_stop_loss: Trailing stop % (0 to disable)
            - lot_size: Position size fraction (default 1.0 = all-in)
            - max_lot_size: Maximum position size fraction

    Returns:
        DataFrame with added columns:
            - account_value: Account equity at each bar
            - aux_value: Position size in auxiliary currency
            - action: Final action taken (e/x/h/'')
            - total: Total portfolio value
    """
    try:
        from fast_trade.run_analysis import apply_logic_to_df as ft_apply
        return ft_apply(df, backtest)
    except ImportError:
        raise ImportError("fast-trade not installed. Run: pip install fast-trade")
