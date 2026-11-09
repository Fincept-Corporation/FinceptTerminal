#!/usr/bin/env python3
"""
Fincept Terminal - Shared Strategy Loading Helpers

Analytics/backtesting/base/fincept_strategy_runner.py reads a path out of the
strategy registry and exec()s the file it points at. The containment check that
guards that exec() lives here (issue #369: a second loader, the former
strategies/live_runner.py, once drifted from it; live deployment is now the C++
algo engine), so any future loader must use this one contract.
"""

from pathlib import Path
from typing import Union


class StrategyPathError(ValueError):
    """A registry path resolved outside the strategies directory.

    Subclasses ValueError so callers with an existing `except ValueError`
    handler keep catching it.
    """


def resolve_strategy_path(strategies_dir: Union[str, Path], path: Union[str, Path]) -> Path:
    """Resolve a registry `path` against `strategies_dir`, asserting containment.

    `Path / value` (and `os.path.join`) silently ESCAPE the root when `value` is
    absolute ("C:/x", "/etc/x") or walks up ("../../x"), and both callers exec()
    whatever they read. Resolve first, then assert the result is still under the
    root. `.resolve()` follows symlinks, so a link pointing out of the tree is
    rejected too.

    Returns the resolved absolute path. Existence is deliberately NOT checked -
    the two callers report a missing file in different ways.

    Raises StrategyPathError if the path escapes the strategies directory.
    """
    root = Path(strategies_dir).resolve()
    resolved = (root / Path(path)).resolve()
    if not resolved.is_relative_to(root):
        raise StrategyPathError(
            f"Strategy path escapes the strategies directory: {path}"
        )
    return resolved
