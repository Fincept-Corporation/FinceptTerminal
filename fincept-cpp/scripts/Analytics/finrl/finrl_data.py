"""
FinRL Data Downloading & Preprocessing
Wrapper around finrl.meta.data_processor, finrl.meta.preprocessor,
and all data processor backends (YahooFinance, Alpaca, CCXT, WRDS).

Covers:
- Multi-source data downloading (Yahoo Finance, Alpaca, CCXT crypto, WRDS)
- Data cleaning & gap-filling
- Technical indicator calculation (20+ indicators)
- Turbulence index computation
- VIX integration
- Feature engineering pipeline
- Train/test/trade data splitting
- Rolling window date generation

Usage (CLI):
    python finrl_data.py --action download --source yahoofinance --tickers AAPL MSFT GOOGL --start 2020-01-01 --end 2023-12-31 --interval 1D
    python finrl_data.py --action preprocess --source yahoofinance --tickers AAPL MSFT --start 2020-01-01 --end 2023-12-31
    python finrl_data.py --action add_indicators --input_file data.csv --indicators macd rsi_30 boll_ub boll_lb
    python finrl_data.py --action split --input_file data.csv --train_start 2020-01-01 --train_end 2022-12-31 --test_start 2023-01-01 --test_end 2023-12-31
    python finrl_data.py --action full_pipeline --source yahoofinance --tickers AAPL MSFT --start 2018-01-01 --end 2023-12-31 --train_end 2022-12-31 --test_start 2023-01-01
    python finrl_data.py --action rolling_dates --train_start 2018-01-01 --train_end 2020-12-31 --trade_start 2021-01-01 --trade_end 2023-12-31 --window 63
"""

import json
import sys
import os
import argparse
import numpy as np
import pandas as pd

from finrl.meta.data_processor import DataProcessor
from finrl.meta.preprocessor.preprocessors import FeatureEngineer, data_split
from finrl.meta.preprocessor.yahoodownloader import YahooDownloader
from finrl.meta.data_processors.func import (
    calc_train_trade_starts_ends_if_rolling,
    str2date, date2str,
)
from finrl.config import INDICATORS, DATA_SAVE_DIR


# ============================================================================
# Data Download Functions
# ============================================================================

def download_data(
    source: str = "yahoofinance",
    tickers: list = None,
    start_date: str = "2020-01-01",
    end_date: str = "2023-12-31",
    time_interval: str = "1D",
    save_path: str = None,
    **kwargs
) -> pd.DataFrame:
    """
    Download market data from various sources.

    Supported sources: yahoofinance, alpaca, ccxt, wrds
    """
    processor = DataProcessor(data_source=source, **kwargs)
    df = processor.download_data(
        ticker_list=tickers,
        start_date=start_date,
        end_date=end_date,
        time_interval=time_interval,
    )
    df = processor.clean_data(df)

    if save_path:
        os.makedirs(os.path.dirname(save_path) or ".", exist_ok=True)
        df.to_csv(save_path, index=False)

    return df


def download_yahoo(
    tickers: list,
    start_date: str,
    end_date: str,
    save_path: str = None,
) -> pd.DataFrame:
    """Download data via YahooDownloader (legacy interface)."""
    downloader = YahooDownloader(
        start_date=start_date,
        end_date=end_date,
        ticker_list=tickers,
    )
    df = downloader.fetch_data()

    if save_path:
        os.makedirs(os.path.dirname(save_path) or ".", exist_ok=True)
        df.to_csv(save_path, index=False)

    return df


# ============================================================================
# Feature Engineering Functions
# ============================================================================

def add_technical_indicators(
    df: pd.DataFrame,
    indicators: list = None,
    source: str = "yahoofinance",
) -> pd.DataFrame:
    """Add technical indicators to dataframe via DataProcessor."""
    if indicators is None:
        indicators = list(INDICATORS)
    processor = DataProcessor(data_source=source)
    return processor.add_technical_indicator(df, indicators)


def add_turbulence(df: pd.DataFrame, source: str = "yahoofinance") -> pd.DataFrame:
    """Add turbulence index to dataframe."""
    processor = DataProcessor(data_source=source)
    return processor.add_turbulence(df)


def add_vix(df: pd.DataFrame, source: str = "yahoofinance") -> pd.DataFrame:
    """Add VIX data to dataframe."""
    processor = DataProcessor(data_source=source)
    return processor.add_vix(df)


def feature_engineer(
    df: pd.DataFrame,
    use_technical_indicator: bool = True,
    tech_indicator_list: list = None,
    use_vix: bool = False,
    use_turbulence: bool = False,
    user_defined_feature: bool = False,
) -> pd.DataFrame:
    """
    Full feature engineering pipeline using FeatureEngineer.
    Cleans data, adds indicators, VIX, turbulence.
    """
    if tech_indicator_list is None:
        tech_indicator_list = list(INDICATORS)

    fe = FeatureEngineer(
        use_technical_indicator=use_technical_indicator,
        tech_indicator_list=tech_indicator_list,
        use_vix=use_vix,
        use_turbulence=use_turbulence,
        user_defined_feature=user_defined_feature,
    )
    return fe.preprocess_data(df)


# ============================================================================
# Data Splitting Functions
# ============================================================================

def split_data(
    df: pd.DataFrame,
    train_start: str,
    train_end: str,
    test_start: str = None,
    test_end: str = None,
    date_col: str = "date",
) -> dict:
    """Split dataframe into train and test sets."""
    train_df = data_split(df, train_start, train_end, target_date_col=date_col)
    result = {"train": train_df}

    if test_start and test_end:
        test_df = data_split(df, test_start, test_end, target_date_col=date_col)
        result["test"] = test_df

    return result


def generate_rolling_dates(
    train_start: str,
    train_end: str,
    trade_start: str,
    trade_end: str,
    rolling_window: int = 63,
) -> dict:
    """Generate rolling window train/trade date pairs."""
    import datetime
    train_dates = []
    d = str2date(train_start)
    end = str2date(train_end)
    delta = datetime.timedelta(days=1)
    while d <= end:
        train_dates.append(date2str(d))
        d += delta

    trade_dates = []
    d = str2date(trade_start)
    end = str2date(trade_end)
    while d <= end:
        trade_dates.append(date2str(d))
        d += delta

    train_starts, train_ends, trade_starts, trade_ends = \
        calc_train_trade_starts_ends_if_rolling(
            train_dates, trade_dates, rolling_window
        )

    return {
        "windows": [
            {
                "window": i,
                "train_start": train_starts[i],
                "train_end": train_ends[i],
                "trade_start": trade_starts[i],
                "trade_end": trade_ends[i],
            }
            for i in range(len(train_starts))
        ],
        "total_windows": len(train_starts),
    }


# ============================================================================
# Array Conversion (for Environment)
# ============================================================================

def df_to_arrays(
    df: pd.DataFrame,
    indicators: list = None,
    if_vix: bool = True,
    source: str = "yahoofinance",
) -> dict:
    """Convert preprocessed dataframe to arrays for environment."""
    if indicators is None:
        indicators = list(INDICATORS)
    processor = DataProcessor(data_source=source)
    arrays = processor.df_to_array(df, if_vix)
    return {
        "price_array_shape": list(arrays[0].shape) if len(arrays) > 0 else [],
        "tech_array_shape":  list(arrays[1].shape) if len(arrays) > 1 else [],
        "turbulence_array_shape": list(arrays[2].shape) if len(arrays) > 2 else [],
        "num_arrays": len(arrays),
    }


# ============================================================================
# Full Pipeline
# ============================================================================

def full_data_pipeline(
    source: str = "yahoofinance",
    tickers: list = None,
    start_date: str = "2018-01-01",
    end_date: str = "2023-12-31",
    time_interval: str = "1D",
    indicators: list = None,
    use_vix: bool = False,
    use_turbulence: bool = False,
    train_start: str = None,
    train_end: str = None,
    test_start: str = None,
    test_end: str = None,
    save_dir: str = None,
    **kwargs,
) -> dict:
    """
    Execute the full data pipeline:
    1. Download data
    2. Clean data
    3. Add technical indicators
    4. Optionally add VIX and turbulence
    5. Split into train/test
    """
    if tickers is None:
        tickers = ["AAPL", "MSFT", "GOOGL"]
    if indicators is None:
        indicators = list(INDICATORS)
    if train_start is None:
        train_start = start_date
    if train_end is None:
        train_end = end_date

    # Step 1: Download
    processor = DataProcessor(data_source=source, **kwargs)
    df = processor.download_data(tickers, start_date, end_date, time_interval)

    # Step 2: Clean
    df = processor.clean_data(df)

    # Step 3: Feature engineer
    fe = FeatureEngineer(
        use_technical_indicator=True,
        tech_indicator_list=indicators,
        use_vix=use_vix,
        use_turbulence=use_turbulence,
    )
    df = fe.preprocess_data(df)

    # Step 4: Split
    splits = split_data(df, train_start, train_end, test_start, test_end)

    # Step 5: Save
    if save_dir:
        os.makedirs(save_dir, exist_ok=True)
        df.to_csv(os.path.join(save_dir, "full_data.csv"), index=False)
        splits["train"].to_csv(os.path.join(save_dir, "train_data.csv"), index=False)
        if "test" in splits:
            splits["test"].to_csv(os.path.join(save_dir, "test_data.csv"), index=False)

    result = {
        "source": source,
        "tickers": tickers,
        "date_range": f"{start_date} to {end_date}",
        "indicators": indicators,
        "total_rows": len(df),
        "columns": list(df.columns),
        "train_rows": len(splits["train"]),
        "test_rows": len(splits.get("test", [])),
        "unique_dates": int(df["date"].nunique()) if "date" in df.columns else 0,
        "unique_tickers": int(df["tic"].nunique()) if "tic" in df.columns else len(tickers),
    }

    if save_dir:
        result["saved_to"] = save_dir

    return result


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="FinRL Data Manager")
    parser.add_argument("--action", required=True,
                        choices=["download", "preprocess", "add_indicators",
                                 "split", "full_pipeline", "rolling_dates",
                                 "list_sources"])
    parser.add_argument("--source", default="yahoofinance")
    parser.add_argument("--tickers", nargs="*", default=["AAPL", "MSFT", "GOOGL"])
    parser.add_argument("--start", default="2020-01-01")
    parser.add_argument("--end", default="2023-12-31")
    parser.add_argument("--interval", default="1D")
    parser.add_argument("--indicators", nargs="*")
    parser.add_argument("--use_vix", action="store_true")
    parser.add_argument("--use_turbulence", action="store_true")
    parser.add_argument("--train_start")
    parser.add_argument("--train_end")
    parser.add_argument("--test_start")
    parser.add_argument("--test_end")
    parser.add_argument("--trade_start")
    parser.add_argument("--trade_end")
    parser.add_argument("--window", type=int, default=63)
    parser.add_argument("--input_file")
    parser.add_argument("--save_dir", default=DATA_SAVE_DIR)

    args = parser.parse_args()

    if args.action == "list_sources":
        result = {
            "supported_sources": [
                {"name": "yahoofinance", "description": "Yahoo Finance - Free, global equities/ETFs/indices"},
                {"name": "alpaca", "description": "Alpaca Markets - US equities, requires API key"},
                {"name": "ccxt", "description": "CCXT - 100+ crypto exchanges"},
                {"name": "wrds", "description": "WRDS - Academic financial database"},
                {"name": "joinquant", "description": "JoinQuant - Chinese market data"},
                {"name": "quantconnect", "description": "QuantConnect - Multi-asset data"},
            ]
        }

    elif args.action == "download":
        df = download_data(
            source=args.source, tickers=args.tickers,
            start_date=args.start, end_date=args.end,
            time_interval=args.interval,
            save_path=os.path.join(args.save_dir, "downloaded_data.csv"),
        )
        result = {
            "status": "success",
            "rows": len(df),
            "columns": list(df.columns),
            "tickers": args.tickers,
            "date_range": f"{args.start} to {args.end}",
            "saved_to": os.path.join(args.save_dir, "downloaded_data.csv"),
        }

    elif args.action == "preprocess":
        result = full_data_pipeline(
            source=args.source, tickers=args.tickers,
            start_date=args.start, end_date=args.end,
            time_interval=args.interval,
            indicators=args.indicators,
            use_vix=args.use_vix,
            use_turbulence=args.use_turbulence,
            train_start=args.train_start, train_end=args.train_end,
            test_start=args.test_start, test_end=args.test_end,
            save_dir=args.save_dir,
        )

    elif args.action == "add_indicators":
        if args.input_file and os.path.exists(args.input_file):
            df = pd.read_csv(args.input_file)
        else:
            result = {"error": "Provide --input_file with valid CSV path"}
            print(json.dumps(result, indent=2))
            return

        df = add_technical_indicators(df, args.indicators, args.source)
        save_path = os.path.join(args.save_dir, "data_with_indicators.csv")
        df.to_csv(save_path, index=False)
        result = {
            "status": "success",
            "indicators_added": args.indicators or list(INDICATORS),
            "rows": len(df),
            "columns": list(df.columns),
            "saved_to": save_path,
        }

    elif args.action == "split":
        if args.input_file and os.path.exists(args.input_file):
            df = pd.read_csv(args.input_file)
        else:
            result = {"error": "Provide --input_file with valid CSV path"}
            print(json.dumps(result, indent=2))
            return

        splits = split_data(df, args.train_start, args.train_end,
                            args.test_start, args.test_end)
        result = {
            "train_rows": len(splits["train"]),
            "test_rows": len(splits.get("test", [])),
        }

    elif args.action == "full_pipeline":
        result = full_data_pipeline(
            source=args.source, tickers=args.tickers,
            start_date=args.start, end_date=args.end,
            time_interval=args.interval,
            indicators=args.indicators,
            use_vix=args.use_vix,
            use_turbulence=args.use_turbulence,
            train_start=args.train_start or args.start,
            train_end=args.train_end or args.end,
            test_start=args.test_start, test_end=args.test_end,
            save_dir=args.save_dir,
        )

    elif args.action == "rolling_dates":
        result = generate_rolling_dates(
            args.train_start or "2018-01-01",
            args.train_end or "2020-12-31",
            args.trade_start or "2021-01-01",
            args.trade_end or "2023-12-31",
            args.window,
        )

    else:
        result = {"error": f"Unknown action: {args.action}"}

    print(json.dumps(result, indent=2, default=str))


if __name__ == "__main__":
    main()
