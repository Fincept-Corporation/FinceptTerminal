# ============================================================================
# Fincept Terminal - Strategy Engine: price reads (adapters) and the engine's file caches
#
# PRICES. The engine no longer downloads or caches bars itself: fetch_bars(),
# fetch_daily_dicts() and fetch_actions() are thin adapters over price_feed (which reads
# base.price_store, the OHLCV cache every backtesting engine shares - exchange session dates,
# one request per missing symbol, none when warm). Their signatures and return formats are
# unchanged (end dates EXCLUSIVE), so every caller (the runner, universe_data, option pricing,
# continuous futures) works as before. The JSON price files this module used to write under
# <data root>/strategy_engine/yahoo_<interval>_<mode>/ are neither read nor written any more
# (that folder can be deleted).
#
# FILE CACHES that are not prices (fundamentals snapshots, ETF holdings, custom-data files)
# live under cache_root() = fincept_paths.cache_dir('strategy_engine')
#   = <data root>/cache/scripts/strategy_engine   (re-fetchable caches, edition coexistence rule)
# Files are written atomically (temp file + rename), so parallel runs are safe.
# ============================================================================

import json
import os
import sys
import tempfile
from pathlib import Path
from typing import Dict, Iterable, List


def _fincept_paths():
    scripts_root = str(Path(__file__).resolve().parents[2])  # fincept_engine -> strategies -> scripts
    if scripts_root not in sys.path:
        sys.path.append(scripts_root)
    import fincept_paths
    return fincept_paths


def cache_root() -> Path:
    """<data root>/cache/scripts/strategy_engine (fincept_paths.cache_dir; FINCEPT_DATA_DIR is honoured
    there): the engine's re-fetchable non-price caches (universe/, custom_data/)."""
    return _fincept_paths().cache_dir('strategy_engine')


_WINDOWS_RESERVED = {'CON', 'PRN', 'AUX', 'NUL', *(f'COM{i}' for i in range(1, 10)), *(f'LPT{i}' for i in range(1, 10))}


def _safe_name(ticker: str) -> str:
    """A file name for a ticker on any OS: other characters become '_', and Windows device names
    (CON, NUL, COM1...) - reserved even with an extension - get a leading '_'."""
    name = ''.join(c if c.isalnum() or c in '-_.' else '_' for c in ticker) or '_'
    return '_' + name if name.split('.')[0].upper() in _WINDOWS_RESERVED else name


def atomic_write_json(path: Path, obj) -> None:
    """Write JSON so a reader never sees a half-written file (temp + rename)."""
    tmp = None
    try:
        fd, tmp = tempfile.mkstemp(prefix='.tmp', dir=str(path.parent))
        with os.fdopen(fd, 'w', encoding='utf-8') as f:
            json.dump(obj, f, separators=(',', ':'))
        for attempt in range(5):
            try:
                os.replace(tmp, path)
                break
            except PermissionError:
                # Windows: the target is open in another process for a moment; try again shortly
                if attempt == 4:
                    raise
                import time
                time.sleep(0.05 * (attempt + 1))
    except OSError:
        try:
            if tmp:
                os.unlink(tmp)
        except Exception:
            pass


def read_json(path: Path):
    try:
        with open(path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def _feed():
    from .price_feed import default_feed
    return default_feed()


def fetch_bars(tickers: Iterable[str], start: str, end: str, interval: str = '1d', mode: str = 'adj',
               with_actions: bool = False) -> Dict[str, dict]:
    """Bars for each ticker between start (inclusive) and end (EXCLUSIVE), 'YYYY-MM-DD', from the price store.

    mode 'adj': OHLC split- and dividend-adjusted; mode 'raw': OHLC split-adjusted only, plus the adjusted
    close as a 7th column, and the split events from start up to the store's last bar.
    Returns {ticker: {'bars': [[time, o, h, l, c, v(, adj_close)], ...], 'dividends': [[date, amount]],
    'splits': [[date, ratio]]}} for tickers the store can serve (others are absent)."""
    return _feed().bars(tickers, start, end, interval, mode)


def fetch_daily_dicts(tickers: Iterable[str], start: str, end: str, mode: str = 'adj') -> Dict[str, List[Dict]]:
    """The runner's bar format: {ticker: [{time, open, high, low, close, volume(, adj_close)}, ...]} (end EXCLUSIVE)."""
    return _feed().daily_dicts(tickers, start, end, mode)


def fetch_actions(ticker: str, start: str, end: str) -> dict:
    """{'dividends': [[date, amount]], 'dividends_raw': [[date, amount as paid]], 'splits': [[date, ratio]]}
    in [start, end) from the price store."""
    return _feed().actions(ticker, start, end)
