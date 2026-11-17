"""Where backtesting files live - ONLY through scripts/fincept_paths.py (edition coexistence rule).

    data_root()            fincept_paths.data_root(): FINCEPT_DATA_DIR (set by the terminal for every
                           script), else the edition's default data root
    cache_root('prices')   fincept_paths.cache_dir('backtesting', 'prices')
                           = <data_root>/cache/scripts/backtesting/prices   (created on demand)

Every cache of the backtesting scripts (price store + its lock files, numba's compiled kernels,
yfinance's timezone cache) sits under cache_root(...): downloads and caches that can be fetched
again, as fincept_paths lays them out. Nothing is written to the scripts folder, the home
directory or a fixed name in the temp folder. Cancel flags are paths the host passes
(`cancelFile`); scripts only test whether they exist.
"""

from __future__ import annotations

import sys
from pathlib import Path

# base -> backtesting -> Analytics -> scripts
_SCRIPTS_ROOT = str(Path(__file__).resolve().parents[3])
CACHE_NAME = 'backtesting'


def _fincept_paths():
    if _SCRIPTS_ROOT not in sys.path:
        sys.path.append(_SCRIPTS_ROOT)
    import fincept_paths
    return fincept_paths


def data_root() -> Path:
    return _fincept_paths().data_root()


def cache_root(*parts: str) -> Path:
    """<data_root>/cache/scripts/backtesting/<parts> (fincept_paths.cache_dir), created on demand."""
    return _fincept_paths().cache_dir(CACHE_NAME, *parts)
