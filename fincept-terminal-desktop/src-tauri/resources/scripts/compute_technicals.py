"""
Compute all technical indicators from historical data
This script receives historical OHLCV data as JSON and returns all computed technicals
"""

import sys
import json
import pandas as pd

# Add technicals directory to path
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'technicals'))

from technicals.momentum_indicators import calculate_all_momentum_indicators
from technicals.volume_indicators import calculate_all_volume_indicators
from technicals.volatility_indicators import calculate_all_volatility_indicators
from technicals.trend_indicators import calculate_all_trend_indicators
from technicals.others_indicators import calculate_all_others_indicators


def compute_all_technicals(historical_data_json):
    """
    Compute all technical indicators from historical data

    Args:
        historical_data_json: JSON string with array of OHLCV data

    Returns:
        JSON string with all computed technical indicators
    """
    try:
        # Parse input data
        data = json.loads(historical_data_json)
        df = pd.DataFrame(data)

        # Ensure required columns exist and are properly named
        required_columns = ['open', 'high', 'low', 'close']
        for col in required_columns:
            if col not in df.columns:
                return json.dumps({
                    "success": False,
                    "error": f"Missing required column: {col}"
                })

        # Compute all indicators
        result_df = df.copy()

        # Trend indicators
        result_df = calculate_all_trend_indicators(result_df)

        # Momentum indicators
        result_df = calculate_all_momentum_indicators(result_df)

        # Volatility indicators
        result_df = calculate_all_volatility_indicators(result_df)

        # Volume indicators (only if volume data exists)
        if 'volume' in result_df.columns:
            result_df = calculate_all_volume_indicators(result_df)

        # Other indicators
        result_df = calculate_all_others_indicators(result_df)

        # Replace NaN with None for proper JSON encoding
        result_df = result_df.where(pd.notnull(result_df), None)

        # Convert to JSON
        result_json = result_df.to_json(orient='records')

        return json.dumps({
            "success": True,
            "data": json.loads(result_json),
            "indicator_columns": {
                "trend": [
                    "sma_20", "ema_12", "wma_9", "macd", "macd_signal", "macd_diff",
                    "trix", "mass_index", "ichimoku_conversion", "ichimoku_base",
                    "ichimoku_a", "ichimoku_b", "kst", "kst_signal", "dpo", "cci",
                    "adx", "adx_pos", "adx_neg", "vortex_pos", "vortex_neg",
                    "psar", "psar_up", "psar_down", "psar_up_indicator", "psar_down_indicator",
                    "stc", "aroon_up", "aroon_down", "aroon_indicator"
                ],
                "momentum": [
                    "rsi", "stoch_k", "stoch_d", "stoch_rsi", "stoch_rsi_k", "stoch_rsi_d",
                    "williams_r", "ao", "kama", "roc", "tsi", "uo",
                    "ppo", "ppo_signal", "ppo_hist", "pvo", "pvo_signal", "pvo_hist"
                ],
                "volatility": [
                    "atr", "bb_mavg", "bb_hband", "bb_lband", "bb_pband", "bb_wband",
                    "bb_hband_indicator", "bb_lband_indicator",
                    "kc_mavg", "kc_hband", "kc_lband", "kc_pband", "kc_wband",
                    "kc_hband_indicator", "kc_lband_indicator",
                    "dc_hband", "dc_lband", "dc_mband", "dc_pband", "dc_wband", "ui"
                ],
                "volume": [
                    "adi", "obv", "cmf", "fi", "eom", "eom_signal",
                    "vpt", "nvi", "vwap", "mfi"
                ],
                "others": [
                    "daily_return", "daily_log_return", "cumulative_return"
                ]
            }
        })

    except Exception as e:
        return json.dumps({
            "success": False,
            "error": str(e)
        })


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(json.dumps({
            "success": False,
            "error": "Usage: python compute_technicals.py '<historical_data_json>'"
        }))
        sys.exit(1)

    historical_data_json = sys.argv[1]
    result = compute_all_technicals(historical_data_json)
    print(result)
