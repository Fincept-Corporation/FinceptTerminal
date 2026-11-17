"""
VisionQuant Utilities - Shared helpers for Pattern Intelligence
"""

import os
import sys
import json
import threading
import numpy as np
import pandas as pd
from datetime import datetime

# The wall-clock cap on one whole Yahoo call (a call can make several requests: cookie,
# crumb, data); fincept_yahoo applies its own per-request timeout.
YF_CALL_DEADLINE = 60


def with_deadline(fn, seconds=YF_CALL_DEADLINE, what="request"):
    """Run fn() and return its result, or raise TimeoutError after `seconds`.

    The call runs on a daemon thread, so a stalled connection cannot keep the
    process (and the runner slot) busy past the deadline."""
    result = {}

    def run():
        try:
            result["value"] = fn()
        except BaseException as e:  # re-raised on the caller's thread
            result["error"] = e

    t = threading.Thread(target=run, daemon=True)
    t.start()
    t.join(seconds)
    if t.is_alive():
        raise TimeoutError(f"{what} did not answer within {seconds:.0f} s")
    if "error" in result:
        raise result["error"]
    return result.get("value")


def fincept_yahoo():
    """The shared Yahoo layer (bar cache, daily profile cache, limiter / back-off); the scripts
    folder is one level up."""
    scripts_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if scripts_root not in sys.path:
        sys.path.append(scripts_root)
    import fincept_yahoo as fy
    return fy


def get_data_dir():
    """Resolve the VisionQuant data directory: <FINCEPT_DATA_DIR>/vision_quant (the
    trained model, the pattern index and its chart images); run by hand, the same
    folder under the edition's default data root (fincept_paths.data_root())."""
    fincept_dir = os.environ.get("FINCEPT_DATA_DIR", "")
    if fincept_dir:
        return os.path.join(fincept_dir, "vision_quant")
    scripts_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if scripts_root not in sys.path:
        sys.path.append(scripts_root)
    import fincept_paths
    return os.path.join(str(fincept_paths.data_root()), "vision_quant")


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
    Fetch OHLCV data from Yahoo through fincept_yahoo's shared bar cache (one long download
    per symbol shared by every script; later calls slice it or fetch only the missing tail).

    Args:
        symbol: Ticker string (e.g. 'AAPL', '600519.SS')
        start: Start date string 'YYYY-MM-DD' (without `end`: up to today)
        end: End date string 'YYYY-MM-DD' (used together with `start`)
        period: yfinance period if no start is given

    Returns:
        DataFrame with Open, High, Low, Close, Volume columns and a timezone-naive
        DatetimeIndex (exchange-local dates), or None
    """
    fy = fincept_yahoo()

    if start:
        # `start` alone used to fall through to period="5y", so a build asked
        # for 2024-01-01 indexed bars from five years back.
        fetch = lambda: fy.ticker_history(symbol, start=start, end=end, auto_adjust=True, actions=False)
    else:
        fetch = lambda: fy.ticker_history(symbol, period=period, auto_adjust=True, actions=False)
    df = with_deadline(fetch, what=f"yfinance history for {symbol}")

    if df is None or df.empty:
        return None

    df = normalize_ohlcv(df)
    # yfinance indexes are timezone-aware (e.g. America/New_York). Every caller
    # compares them with naive datetimes; that raised TypeError ("Invalid
    # comparison between dtype=datetime64[ns, ...] and datetime"), which the
    # pattern search swallowed — so no match got a DTW / correlation score.
    if isinstance(df.index, pd.DatetimeIndex) and df.index.tz is not None:
        df.index = df.index.tz_localize(None)

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

    # A plain bool: `Series.sum() > 0` is numpy.bool_, which mplfinance's
    # isinstance(value, bool) validator rejects — the index build crashed at
    # the first chart.
    has_volume = bool("Volume" in df.columns and df["Volume"].sum() > 0)

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


def _finite(obj):
    """Copy of obj with NaN / +-Infinity (incl. numpy floats) replaced by None."""
    if isinstance(obj, (float, np.floating)):
        return float(obj) if np.isfinite(obj) else None
    if isinstance(obj, dict):
        return {k: _finite(v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple)):
        return [_finite(v) for v in obj]
    return obj


def output_json(obj):
    """Print JSON to stdout (Fincept protocol). Never emits NaN / Infinity."""
    print(json.dumps(_finite(obj), default=str, ensure_ascii=False, allow_nan=False), flush=True)


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
