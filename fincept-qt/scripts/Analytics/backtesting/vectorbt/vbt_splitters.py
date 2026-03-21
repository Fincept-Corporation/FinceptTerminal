"""
VBT Splitters Module

Cross-validation splitters for walk-forward analysis and time-series validation.
Covers all vectorbt splitter classes:

- BaseSplitter: Abstract base
- RangeSplitter: Split by date ranges
- RollingSplitter: Rolling window splits
- ExpandingSplitter: Expanding window splits

Each splitter produces (train, test) index pairs for use in
optimization and backtesting validation.
"""

import numpy as np
import pandas as pd
from typing import List, Tuple, Optional, Generator


class BaseSplitter:
    """Abstract base splitter."""

    def split(self, index: pd.Index) -> Generator[Tuple[np.ndarray, np.ndarray], None, None]:
        raise NotImplementedError

    def get_n_splits(self, index: pd.Index) -> int:
        return len(list(self.split(index)))

    def split_ranges_into_sets(
        self,
        index: pd.Index,
    ) -> List[Tuple[pd.Index, pd.Index]]:
        """
        Split index into (train_index, test_index) pairs.

        Returns list of (train_idx, test_idx) tuples using actual index values.
        """
        result = []
        for train_mask, test_mask in self.split(index):
            train_idx = index[train_mask]
            test_idx = index[test_mask]
            result.append((train_idx, test_idx))
        return result


class RangeSplitter(BaseSplitter):
    """
    Split by explicit date ranges.

    Mimics vbt.RangeSplitter.

    Usage:
        splitter = RangeSplitter(ranges=[
            ('2020-01-01', '2020-06-30', '2020-07-01', '2020-12-31'),
            ('2021-01-01', '2021-06-30', '2021-07-01', '2021-12-31'),
        ])
    """

    def __init__(
        self,
        ranges: List[Tuple[str, str, str, str]] = None,
    ):
        """
        Args:
            ranges: List of (train_start, train_end, test_start, test_end) tuples
        """
        self._ranges = ranges or []

    def split(self, index: pd.Index) -> Generator[Tuple[np.ndarray, np.ndarray], None, None]:
        for train_start, train_end, test_start, test_end in self._ranges:
            train_mask = (index >= train_start) & (index <= train_end)
            test_mask = (index >= test_start) & (index <= test_end)
            if train_mask.any() and test_mask.any():
                yield train_mask.values, test_mask.values


class RollingSplitter(BaseSplitter):
    """
    Rolling window cross-validation splitter.

    Mimics vbt.RollingSplitter.

    Creates fixed-size rolling windows where:
    - Training window is `window_len` bars
    - Test window is `test_len` bars
    - Windows advance by `step` bars

    Example with window_len=100, test_len=20, step=20:
        Split 1: Train [0:100],   Test [100:120]
        Split 2: Train [20:120],  Test [120:140]
        Split 3: Train [40:140],  Test [140:160]
        ...
    """

    def __init__(
        self,
        window_len: int = 252,
        test_len: int = 63,
        step: int = 21,
        set_lens: Optional[Tuple[int, int]] = None,
    ):
        """
        Args:
            window_len: Training window length (bars)
            test_len: Test window length (bars)
            step: Step size between windows
            set_lens: Alternative (train_len, test_len) tuple
        """
        if set_lens is not None:
            self._window_len = set_lens[0]
            self._test_len = set_lens[1]
        else:
            self._window_len = window_len
            self._test_len = test_len
        self._step = step

    def split(self, index: pd.Index) -> Generator[Tuple[np.ndarray, np.ndarray], None, None]:
        n = len(index)
        total_len = self._window_len + self._test_len

        start = 0
        while start + total_len <= n:
            train_end = start + self._window_len
            test_end = train_end + self._test_len

            train_mask = np.zeros(n, dtype=bool)
            test_mask = np.zeros(n, dtype=bool)
            train_mask[start:train_end] = True
            test_mask[train_end:test_end] = True

            yield train_mask, test_mask
            start += self._step


class ExpandingSplitter(BaseSplitter):
    """
    Expanding window cross-validation splitter.

    Mimics vbt.ExpandingSplitter.

    Training window grows from `min_len` to cover all available data.
    Test window is always `test_len` bars after training.

    Example with min_len=100, test_len=20, step=20:
        Split 1: Train [0:100],   Test [100:120]
        Split 2: Train [0:120],   Test [120:140]
        Split 3: Train [0:140],   Test [140:160]
        ...
    """

    def __init__(
        self,
        min_len: int = 252,
        test_len: int = 63,
        step: int = 21,
    ):
        """
        Args:
            min_len: Minimum training window length
            test_len: Test window length
            step: Step size for expanding
        """
        self._min_len = min_len
        self._test_len = test_len
        self._step = step

    def split(self, index: pd.Index) -> Generator[Tuple[np.ndarray, np.ndarray], None, None]:
        n = len(index)
        train_end = self._min_len

        while train_end + self._test_len <= n:
            test_end = train_end + self._test_len

            train_mask = np.zeros(n, dtype=bool)
            test_mask = np.zeros(n, dtype=bool)
            train_mask[0:train_end] = True
            test_mask[train_end:test_end] = True

            yield train_mask, test_mask
            train_end += self._step


# ============================================================================
# Utility: Purged / Embargo Splitter (bonus, common in ML for finance)
# ============================================================================

class PurgedKFoldSplitter(BaseSplitter):
    """
    Purged K-Fold splitter for financial time series.

    Removes data around test boundaries to prevent look-ahead bias.
    Common in ML for trading (Lopez de Prado).

    Args:
        n_splits: Number of folds
        purge_len: Bars to remove before test set
        embargo_len: Bars to remove after test set
    """

    def __init__(
        self,
        n_splits: int = 5,
        purge_len: int = 5,
        embargo_len: int = 5,
    ):
        self._n_splits = n_splits
        self._purge_len = purge_len
        self._embargo_len = embargo_len

    def split(self, index: pd.Index) -> Generator[Tuple[np.ndarray, np.ndarray], None, None]:
        n = len(index)
        fold_size = n // self._n_splits

        for i in range(self._n_splits):
            test_start = i * fold_size
            test_end = min(test_start + fold_size, n)

            # Purge and embargo
            purge_start = max(0, test_start - self._purge_len)
            embargo_end = min(n, test_end + self._embargo_len)

            train_mask = np.ones(n, dtype=bool)
            test_mask = np.zeros(n, dtype=bool)

            # Remove test + purge + embargo from training
            train_mask[purge_start:embargo_end] = False
            test_mask[test_start:test_end] = True

            if train_mask.any() and test_mask.any():
                yield train_mask, test_mask
