"""
FinRL Configuration & Ticker Constants
Wrapper around finrl.config, finrl.config_tickers, finrl.meta.meta_config

Provides all default hyperparameters, date ranges, indicator lists,
and pre-built ticker universes for major global indices.

Usage (CLI):
    python finrl_config.py --action list_tickers --universe DOW_30
    python finrl_config.py --action list_indicators
    python finrl_config.py --action get_hyperparams --algorithm PPO
    python finrl_config.py --action get_dates
    python finrl_config.py --action custom --train_start 2018-01-01 --train_end 2022-12-31 --test_start 2023-01-01 --test_end 2023-12-31
"""

import json
import sys
import argparse

from finrl.config import (
    INDICATORS,
    TRAIN_START_DATE, TRAIN_END_DATE,
    TEST_START_DATE, TEST_END_DATE,
    TRADE_START_DATE, TRADE_END_DATE,
    A2C_PARAMS, PPO_PARAMS, DDPG_PARAMS, SAC_PARAMS, TD3_PARAMS,
    ERL_PARAMS, RLlib_PARAMS,
    DATA_SAVE_DIR, TRAINED_MODEL_DIR, TENSORBOARD_LOG_DIR, RESULTS_DIR,
)
from finrl.config_tickers import (
    DOW_30_TICKER, NAS_100_TICKER, SP_500_TICKER,
    HSI_50_TICKER, SSE_50_TICKER, CSI_300_TICKER,
    DAX_30_TICKER, CAC_40_TICKER, TECDAX_TICKER,
    MDAX_50_TICKER, SDAX_50_TICKER,
    LQ45_TICKER, SRI_KEHATI_TICKER,
    SINGLE_TICKER, FX_TICKER,
)

# ============================================================================
# Ticker Universes
# ============================================================================

TICKER_UNIVERSES = {
    "SINGLE":       SINGLE_TICKER,
    "DOW_30":       DOW_30_TICKER,
    "NAS_100":      NAS_100_TICKER,
    "SP_500":       SP_500_TICKER,
    "HSI_50":       HSI_50_TICKER,
    "SSE_50":       SSE_50_TICKER,
    "CSI_300":      CSI_300_TICKER,
    "DAX_30":       DAX_30_TICKER,
    "CAC_40":       CAC_40_TICKER,
    "TECDAX":       TECDAX_TICKER,
    "MDAX_50":      MDAX_50_TICKER,
    "SDAX_50":      SDAX_50_TICKER,
    "LQ45":         LQ45_TICKER,
    "SRI_KEHATI":   SRI_KEHATI_TICKER,
    "FX":           FX_TICKER,
}

# ============================================================================
# Algorithm Hyperparameters
# ============================================================================

ALGORITHM_PARAMS = {
    "a2c":   A2C_PARAMS,
    "ppo":   PPO_PARAMS,
    "ddpg":  DDPG_PARAMS,
    "sac":   SAC_PARAMS,
    "td3":   TD3_PARAMS,
    "erl":   ERL_PARAMS,
    "rllib": RLlib_PARAMS,
}

# ============================================================================
# Default Date Ranges
# ============================================================================

DEFAULT_DATES = {
    "train_start": TRAIN_START_DATE,
    "train_end":   TRAIN_END_DATE,
    "test_start":  TEST_START_DATE,
    "test_end":    TEST_END_DATE,
    "trade_start": TRADE_START_DATE,
    "trade_end":   TRADE_END_DATE,
}

# ============================================================================
# Directory Configuration
# ============================================================================

DIRECTORIES = {
    "data_save_dir":       DATA_SAVE_DIR,
    "trained_model_dir":   TRAINED_MODEL_DIR,
    "tensorboard_log_dir": TENSORBOARD_LOG_DIR,
    "results_dir":         RESULTS_DIR,
}


# ============================================================================
# Public API Functions
# ============================================================================

def get_ticker_list(universe: str) -> list:
    """Get ticker list by universe name."""
    key = universe.upper().replace(" ", "_")
    if key not in TICKER_UNIVERSES:
        raise ValueError(f"Unknown universe '{universe}'. Available: {list(TICKER_UNIVERSES.keys())}")
    return TICKER_UNIVERSES[key]


def get_hyperparams(algorithm: str) -> dict:
    """Get default hyperparameters for an algorithm."""
    key = algorithm.lower()
    if key not in ALGORITHM_PARAMS:
        raise ValueError(f"Unknown algorithm '{algorithm}'. Available: {list(ALGORITHM_PARAMS.keys())}")
    return ALGORITHM_PARAMS[key]


def get_indicators() -> list:
    """Get default technical indicator list."""
    return list(INDICATORS)


def get_all_available_indicators() -> list:
    """Get extended list of all supported indicators."""
    return [
        "macd", "boll_ub", "boll_lb", "rsi_30", "cci_30", "dx_30",
        "close_30_sma", "close_60_sma", "rsi_14", "rsi_7",
        "cci_14", "dx_14", "adx", "atr", "obv", "mfi",
        "willr", "stoch_k", "stoch_d", "ema_10", "ema_20", "ema_50",
        "close_5_sma", "close_10_sma", "close_20_sma", "close_120_sma",
        "vwap", "kdjk", "kdjd", "kdjj", "trix", "tema",
    ]


def get_default_dates() -> dict:
    """Get default date configuration."""
    return dict(DEFAULT_DATES)


def build_custom_config(
    tickers: list = None,
    universe: str = None,
    indicators: list = None,
    algorithm: str = "ppo",
    train_start: str = None,
    train_end: str = None,
    test_start: str = None,
    test_end: str = None,
    trade_start: str = None,
    trade_end: str = None,
    initial_amount: float = 1_000_000,
    hmax: int = 100,
    buy_cost_pct: float = 0.001,
    sell_cost_pct: float = 0.001,
    reward_scaling: float = 1e-4,
    data_source: str = "yahoofinance",
    time_interval: str = "1D",
) -> dict:
    """Build a complete configuration dict for FinRL pipeline."""
    if tickers is None:
        if universe:
            tickers = get_ticker_list(universe)
        else:
            tickers = DOW_30_TICKER

    if indicators is None:
        indicators = get_indicators()

    algo_params = get_hyperparams(algorithm)

    return {
        "tickers": tickers,
        "indicators": indicators,
        "algorithm": algorithm,
        "algorithm_params": algo_params,
        "dates": {
            "train_start": train_start or TRAIN_START_DATE,
            "train_end":   train_end   or TRAIN_END_DATE,
            "test_start":  test_start  or TEST_START_DATE,
            "test_end":    test_end    or TEST_END_DATE,
            "trade_start": trade_start or TRADE_START_DATE,
            "trade_end":   trade_end   or TRADE_END_DATE,
        },
        "environment": {
            "initial_amount": initial_amount,
            "hmax": hmax,
            "buy_cost_pct": buy_cost_pct,
            "sell_cost_pct": sell_cost_pct,
            "reward_scaling": reward_scaling,
        },
        "data_source": data_source,
        "time_interval": time_interval,
        "directories": DIRECTORIES,
        "stock_dim": len(tickers),
        "state_space": 1 + 2 * len(tickers) + len(indicators) * len(tickers),
        "action_space": len(tickers),
    }


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="FinRL Configuration Manager")
    parser.add_argument("--action", required=True,
                        choices=["list_tickers", "list_indicators", "list_universes",
                                 "get_hyperparams", "get_dates", "custom", "full_config"],
                        help="Action to perform")
    parser.add_argument("--universe", default="DOW_30", help="Ticker universe name")
    parser.add_argument("--algorithm", default="ppo", help="RL algorithm name")
    parser.add_argument("--tickers", nargs="*", help="Custom tickers list")
    parser.add_argument("--train_start", help="Training start date")
    parser.add_argument("--train_end", help="Training end date")
    parser.add_argument("--test_start", help="Test start date")
    parser.add_argument("--test_end", help="Test end date")
    parser.add_argument("--initial_amount", type=float, default=1_000_000)
    parser.add_argument("--data_source", default="yahoofinance")
    parser.add_argument("--time_interval", default="1D")

    args = parser.parse_args()

    if args.action == "list_tickers":
        result = {"universe": args.universe, "tickers": get_ticker_list(args.universe),
                  "count": len(get_ticker_list(args.universe))}

    elif args.action == "list_indicators":
        result = {"default_indicators": get_indicators(),
                  "all_available": get_all_available_indicators()}

    elif args.action == "list_universes":
        result = {k: {"count": len(v), "sample": v[:5]} for k, v in TICKER_UNIVERSES.items()}

    elif args.action == "get_hyperparams":
        result = {"algorithm": args.algorithm, "params": get_hyperparams(args.algorithm)}

    elif args.action == "get_dates":
        result = get_default_dates()

    elif args.action in ("custom", "full_config"):
        result = build_custom_config(
            tickers=args.tickers, universe=args.universe,
            algorithm=args.algorithm,
            train_start=args.train_start, train_end=args.train_end,
            test_start=args.test_start, test_end=args.test_end,
            initial_amount=args.initial_amount,
            data_source=args.data_source,
            time_interval=args.time_interval,
        )
    else:
        result = {"error": f"Unknown action: {args.action}"}

    print(json.dumps(result, indent=2, default=str))


if __name__ == "__main__":
    main()
