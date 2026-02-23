"""
VisionQuant Utilities - Shared helpers for Pattern Intelligence
"""

import os
import sys
import json
import numpy as np
import pandas as pd
from datetime import datetime


def get_data_dir():
    """Resolve the VisionQuant data directory."""
    fincept_dir = os.environ.get("FINCEPT_DATA_DIR", "")
    if fincept_dir:
        return os.path.join(fincept_dir, "vision_quant")
    # Fallback: ~/.fincept/vision_quant
    home = os.path.expanduser("~")
    return os.path.join(home, ".fincept", "vision_quant")


def ensure_dirs():
    """Create the full directory structure for VisionQuant data."""
    base = get_data_dir()
    dirs = [
        base,
        os.path.join(base, "models"),
        os.path.join(base, "indices"),
        os.path.join(base, "images"),
        os.path.join(base, "data"),
    ]
    for d in dirs:
        os.makedirs(d, exist_ok=True)
    return base


def get_model_path():
    return os.path.join(get_data_dir(), "models", "attention_cae_best.pth")


def get_index_path():
    return os.path.join(get_data_dir(), "indices", "cae_faiss_attention.bin")


def get_meta_path():
    return os.path.join(get_data_dir(), "indices", "meta_data_attention.csv")


def normalize_ohlcv(df):
    """
    Normalize OHLCV DataFrame to standard column names.
    yfinance returns: Open, High, Low, Close, Volume (already standard).
    Handles various common formats.
    """
    if df is None or df.empty:
        return df

    col_map = {}
    for c in df.columns:
        cl = c.lower().strip()
        if cl in ("open", "opening"):
            col_map[c] = "Open"
        elif cl in ("high", "highest"):
            col_map[c] = "High"
        elif cl in ("low", "lowest"):
            col_map[c] = "Low"
        elif cl in ("close", "closing", "adj close"):
            col_map[c] = "Close"
        elif cl in ("volume", "vol"):
            col_map[c] = "Volume"

    if col_map:
        df = df.rename(columns=col_map)

    # Ensure datetime index
    if not isinstance(df.index, pd.DatetimeIndex):
        try:
            df.index = pd.to_datetime(df.index)
        except Exception:
            pass

    return df


def fetch_ohlcv(symbol, start=None, end=None, period="5y"):
    """
    Fetch OHLCV data via yfinance.

    Args:
        symbol: Ticker string (e.g. 'AAPL', '600519.SS')
        start: Start date string 'YYYY-MM-DD'
        end: End date string 'YYYY-MM-DD'
        period: yfinance period if start/end not given

    Returns:
        DataFrame with Open, High, Low, Close, Volume columns
    """
    import yfinance as yf

    ticker = yf.Ticker(symbol)
    if start and end:
        df = ticker.history(start=start, end=end, auto_adjust=True)
    else:
        df = ticker.history(period=period, auto_adjust=True)

    if df is None or df.empty:
        return None

    df = normalize_ohlcv(df)

    # Keep only OHLCV columns
    keep = [c for c in ["Open", "High", "Low", "Close", "Volume"] if c in df.columns]
    df = df[keep].copy()
    df.dropna(inplace=True)

    return df


def generate_kline_image(ohlcv_df, save_path, window=60, style="international",
                         dpi=75, figsize=(3, 3)):
    """
    Render a candlestick chart to PNG using mplfinance.

    Args:
        ohlcv_df: DataFrame with OHLCV columns and DatetimeIndex
        save_path: Output PNG path
        window: Number of bars to render (default 60)
        style: 'international' (green=up) or 'chinese' (red=up)
        dpi: Image DPI (default 75)
        figsize: Figure size tuple (default (3, 3))

    Returns:
        save_path on success, None on failure
    """
    import mplfinance as mpf
    import matplotlib
    matplotlib.use("Agg")

    if ohlcv_df is None or len(ohlcv_df) < 10:
        return None

    # Take the last `window` bars
    df = ohlcv_df.tail(window).copy()

    # Ensure required columns
    required = ["Open", "High", "Low", "Close"]
    for c in required:
        if c not in df.columns:
            return None

    # Style: green=up, red=down (international standard)
    if style == "chinese":
        mc = mpf.make_marketcolors(up="red", down="green", edge="inherit",
                                   wick="inherit", volume="in")
    else:
        mc = mpf.make_marketcolors(up="green", down="red", edge="inherit",
                                   wick="inherit", volume="in")

    s = mpf.make_mpf_style(marketcolors=mc, gridstyle="", y_on_right=False,
                           rc={"axes.edgecolor": "black"})

    os.makedirs(os.path.dirname(save_path), exist_ok=True)

    has_volume = "Volume" in df.columns and df["Volume"].sum() > 0

    mpf.plot(
        df,
        type="candle",
        style=s,
        volume=has_volume,
        figsize=figsize,
        tight_layout=True,
        savefig=dict(fname=save_path, dpi=dpi, bbox_inches="tight", pad_inches=0),
        axisoff=True,
    )

    # Resize to 224x224 for model input
    try:
        from PIL import Image
        img = Image.open(save_path).convert("RGB").resize((224, 224), Image.LANCZOS)
        img.save(save_path)
    except Exception:
        pass

    return save_path


def json_response(status, data=None, error=None):
    """Build a standard JSON response for the Fincept protocol."""
    resp = {"status": status}
    if data is not None:
        resp["data"] = data
    if error is not None:
        resp["error"] = str(error)
    return resp


def output_json(obj):
    """Print JSON to stdout (Fincept protocol)."""
    print(json.dumps(obj, default=str, ensure_ascii=False))


def output_progress(message, pct=None):
    """Print a progress line to stdout."""
    line = {"type": "progress", "message": message}
    if pct is not None:
        line["percent"] = pct
    print(json.dumps(line, ensure_ascii=False), flush=True)


def parse_args():
    """Parse command + JSON params from sys.argv (Fincept protocol)."""
    command = sys.argv[1] if len(sys.argv) > 1 else ""
    params = {}
    if len(sys.argv) > 2:
        try:
            params = json.loads(sys.argv[2])
        except json.JSONDecodeError:
            params = {}
    return command, params
