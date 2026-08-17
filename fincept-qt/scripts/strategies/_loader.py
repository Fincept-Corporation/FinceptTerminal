#!/usr/bin/env python3
"""
Fincept Terminal - Shared Strategy Loading Helpers

Two places read a path out of the strategy registry and exec() the file it
points at:

  - Analytics/backtesting/base/fincept_strategy_runner.py  (backtest)
  - strategies/live_runner.py                              (paper/live deploy)

They each implemented path resolution independently and drifted apart - one had
a containment check and the other did not (issue #369). The check lives here now
so both loaders share one contract and cannot diverge again.
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
