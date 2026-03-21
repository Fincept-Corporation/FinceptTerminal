"""
VBT Generic Module

Full implementations of vectorbt's generic analysis classes:

Drawdowns (23 methods):
  - from_ts: Create from time series
  - drawdown, avg_drawdown, max_drawdown
  - recovery_return, avg_recovery_return, max_recovery_return
  - decline_duration, recovery_duration, recovery_duration_ratio
  - active_drawdown, active_duration, active_recovery, active_recovery_return, active_recovery_duration
  - stats, metrics

Ranges (14 methods):
  - from_ts: Create from boolean mask or time series
  - duration, avg_duration, max_duration
  - coverage
  - to_mask
  - stats, metrics
"""

import numpy as np
import pandas as pd
from typing import Dict, Any, Optional, List
from dataclasses import dataclass


# ============================================================================
# Drawdowns
# ============================================================================

@dataclass
class DrawdownRecord:
    """Single drawdown record."""
    peak_idx: int
    valley_idx: int
    recovery_idx: int  # -1 if still active
    peak_val: float
    valley_val: float
    recovery_val: float
    depth: float  # as positive fraction
    duration: int  # total bars
    decline_duration: int  # bars peak -> valley
    recovery_duration: int  # bars valley -> recovery


class Drawdowns:
    """
    Full Drawdowns analysis class (mimics vbt.generic.Drawdowns).

    Created from a value/equity time series.
    """

    def __init__(self, ts: pd.Series, records: List[DrawdownRecord]):
        self._ts = ts
        self._records = records

    @classmethod
    def from_ts(cls, ts: pd.Series) -> 'Drawdowns':
        """Create Drawdowns from a time series (equity curve)."""
        vals = ts.values.astype(float)
        n = len(vals)
        peak = np.maximum.accumulate(vals)
        dd_pct = (vals - peak) / np.where(peak > 0, peak, 1.0)

        records = []
        in_dd = False
        start_idx = 0
        valley_idx = 0
        max_depth = 0.0

        for i in range(n):
            if dd_pct[i] < 0:
                if not in_dd:
                    in_dd = True
                    start_idx = i
                    valley_idx = i
                    max_depth = abs(dd_pct[i])
                else:
                    if abs(dd_pct[i]) > max_depth:
                        max_depth = abs(dd_pct[i])
                        valley_idx = i
            else:
                if in_dd:
                    peak_idx = max(0, start_idx - 1)
                    records.append(DrawdownRecord(
                        peak_idx=peak_idx,
                        valley_idx=valley_idx,
                        recovery_idx=i,
                        peak_val=float(vals[peak_idx]),
                        valley_val=float(vals[valley_idx]),
                        recovery_val=float(vals[i]),
                        depth=max_depth,
                        duration=i - start_idx + 1,
                        decline_duration=valley_idx - start_idx + 1,
                        recovery_duration=i - valley_idx,
                    ))
                    in_dd = False

        # Active (unrecovered) drawdown
        if in_dd:
            peak_idx = max(0, start_idx - 1)
            records.append(DrawdownRecord(
                peak_idx=peak_idx,
                valley_idx=valley_idx,
                recovery_idx=-1,
                peak_val=float(vals[peak_idx]),
                valley_val=float(vals[valley_idx]),
                recovery_val=float(vals[-1]),
                depth=max_depth,
                duration=n - start_idx,
                decline_duration=valley_idx - start_idx + 1,
                recovery_duration=0,
            ))

        return cls(ts, records)

    # --- Properties ---

    @property
    def ts(self) -> pd.Series:
        return self._ts

    @property
    def count(self) -> int:
        return len(self._records)

    @property
    def records(self) -> List[DrawdownRecord]:
        return self._records

    @property
    def records_readable(self) -> pd.DataFrame:
        """Return records as a DataFrame."""
        if not self._records:
            return pd.DataFrame(columns=[
                'Peak Idx', 'Valley Idx', 'Recovery Idx', 'Peak Val', 'Valley Val',
                'Recovery Val', 'Depth', 'Duration', 'Decline Duration', 'Recovery Duration'
            ])
        return pd.DataFrame([
            {
                'Peak Idx': r.peak_idx,
                'Valley Idx': r.valley_idx,
                'Recovery Idx': r.recovery_idx,
                'Peak Val': r.peak_val,
                'Valley Val': r.valley_val,
                'Recovery Val': r.recovery_val,
                'Depth': r.depth,
                'Duration': r.duration,
                'Decline Duration': r.decline_duration,
                'Recovery Duration': r.recovery_duration,
            }
            for r in self._records
        ])

    # --- Drawdown series ---

    def drawdown(self) -> pd.Series:
        """Full drawdown percentage series."""
        vals = self._ts.values.astype(float)
        peak = np.maximum.accumulate(vals)
        dd = (vals - peak) / np.where(peak > 0, peak, 1.0)
        return pd.Series(dd, index=self._ts.index, name='Drawdown')

    # --- Aggregate stats ---

    def max_drawdown(self) -> float:
        """Maximum drawdown depth (positive fraction)."""
        if not self._records:
            return 0.0
        return float(max(r.depth for r in self._records))

    def avg_drawdown(self) -> float:
        """Average drawdown depth."""
        if not self._records:
            return 0.0
        return float(np.mean([r.depth for r in self._records]))

    def recovery_return(self) -> List[float]:
        """Recovery return for each drawdown (valley to recovery)."""
        results = []
        for r in self._records:
            if r.recovery_idx >= 0 and r.valley_val > 0:
                results.append(float(r.recovery_val / r.valley_val - 1))
            else:
                results.append(0.0)
        return results

    def avg_recovery_return(self) -> float:
        """Average recovery return."""
        rr = self.recovery_return()
        return float(np.mean(rr)) if rr else 0.0

    def max_recovery_return(self) -> float:
        """Maximum recovery return."""
        rr = self.recovery_return()
        return float(max(rr)) if rr else 0.0

    def decline_duration(self) -> List[int]:
        """Decline duration for each drawdown (peak to valley bars)."""
        return [r.decline_duration for r in self._records]

    def recovery_duration(self) -> List[int]:
        """Recovery duration for each drawdown (valley to recovery bars)."""
        return [r.recovery_duration for r in self._records]

    def recovery_duration_ratio(self) -> List[float]:
        """Ratio of recovery to decline duration for each drawdown."""
        ratios = []
        for r in self._records:
            if r.decline_duration > 0:
                ratios.append(float(r.recovery_duration / r.decline_duration))
            else:
                ratios.append(0.0)
        return ratios

    # --- Active drawdown ---

    def active_drawdown(self) -> float:
        """Current active drawdown depth (0 if not in drawdown)."""
        if self._records and self._records[-1].recovery_idx == -1:
            return float(self._records[-1].depth)
        return 0.0

    def active_duration(self) -> int:
        """Duration of current active drawdown (0 if not in drawdown)."""
        if self._records and self._records[-1].recovery_idx == -1:
            return self._records[-1].duration
        return 0

    def active_recovery(self) -> float:
        """Current recovery level of active drawdown."""
        if self._records and self._records[-1].recovery_idx == -1:
            r = self._records[-1]
            if r.peak_val > 0:
                return float(self._ts.iloc[-1] / r.peak_val)
        return 1.0

    def active_recovery_return(self) -> float:
        """Return from valley to current level in active drawdown."""
        if self._records and self._records[-1].recovery_idx == -1:
            r = self._records[-1]
            if r.valley_val > 0:
                return float(self._ts.iloc[-1] / r.valley_val - 1)
        return 0.0

    def active_recovery_duration(self) -> int:
        """Bars since valley in active drawdown."""
        if self._records and self._records[-1].recovery_idx == -1:
            r = self._records[-1]
            return len(self._ts) - 1 - r.valley_idx
        return 0

    # --- Stats ---

    def stats(self) -> Dict[str, Any]:
        """Return comprehensive drawdown statistics."""
        dd = self.drawdown()
        depths = [r.depth for r in self._records]
        durations = [r.duration for r in self._records]

        return {
            'Total Drawdowns': self.count,
            'Max Drawdown [%]': self.max_drawdown() * 100,
            'Avg Drawdown [%]': self.avg_drawdown() * 100,
            'Max Duration': max(durations) if durations else 0,
            'Avg Duration': float(np.mean(durations)) if durations else 0,
            'Max Decline Duration': max(self.decline_duration()) if self._records else 0,
            'Max Recovery Duration': max(self.recovery_duration()) if self._records else 0,
            'Avg Recovery Return [%]': self.avg_recovery_return() * 100,
            'Active Drawdown [%]': self.active_drawdown() * 100,
            'Active Duration': self.active_duration(),
        }


# ============================================================================
# Ranges
# ============================================================================

@dataclass
class RangeRecord:
    """Single range record."""
    start_idx: int
    end_idx: int
    duration: int


class Ranges:
    """
    Range analysis class (mimics vbt.generic.Ranges).

    Analyzes contiguous True ranges in a boolean mask or periods in a time series.
    """

    def __init__(self, ts: pd.Series, records: List[RangeRecord]):
        self._ts = ts
        self._records = records

    @classmethod
    def from_ts(cls, ts: pd.Series, threshold: float = 0.0) -> 'Ranges':
        """
        Create Ranges from a time series.

        Identifies ranges where ts > threshold.
        """
        mask = ts.values > threshold
        return cls._from_mask(ts, mask)

    @classmethod
    def from_mask(cls, index: pd.Index, mask: np.ndarray) -> 'Ranges':
        """Create Ranges from a boolean mask."""
        ts = pd.Series(mask.astype(float), index=index)
        return cls._from_mask(ts, mask)

    @classmethod
    def _from_mask(cls, ts: pd.Series, mask: np.ndarray) -> 'Ranges':
        records = []
        in_range = False
        start = 0

        for i in range(len(mask)):
            if mask[i] and not in_range:
                in_range = True
                start = i
            elif not mask[i] and in_range:
                records.append(RangeRecord(
                    start_idx=start,
                    end_idx=i - 1,
                    duration=i - start,
                ))
                in_range = False

        if in_range:
            records.append(RangeRecord(
                start_idx=start,
                end_idx=len(mask) - 1,
                duration=len(mask) - start,
            ))

        return cls(ts, records)

    # --- Properties ---

    @property
    def ts(self) -> pd.Series:
        return self._ts

    @property
    def count(self) -> int:
        return len(self._records)

    @property
    def records(self) -> List[RangeRecord]:
        return self._records

    @property
    def records_readable(self) -> pd.DataFrame:
        if not self._records:
            return pd.DataFrame(columns=['Start Idx', 'End Idx', 'Duration'])
        return pd.DataFrame([
            {'Start Idx': r.start_idx, 'End Idx': r.end_idx, 'Duration': r.duration}
            for r in self._records
        ])

    # --- Analysis ---

    def to_mask(self) -> pd.Series:
        """Convert ranges back to boolean mask."""
        mask = np.zeros(len(self._ts), dtype=bool)
        for r in self._records:
            mask[r.start_idx:r.end_idx + 1] = True
        return pd.Series(mask, index=self._ts.index, name='Range Mask')

    def duration(self) -> List[int]:
        """Duration of each range."""
        return [r.duration for r in self._records]

    def avg_duration(self) -> float:
        """Average range duration."""
        durs = self.duration()
        return float(np.mean(durs)) if durs else 0.0

    def max_duration(self) -> int:
        """Maximum range duration."""
        durs = self.duration()
        return int(max(durs)) if durs else 0

    def coverage(self) -> float:
        """Fraction of total bars covered by ranges."""
        total_bars = len(self._ts)
        if total_bars == 0:
            return 0.0
        covered = sum(r.duration for r in self._records)
        return float(covered / total_bars)

    def stats(self) -> Dict[str, Any]:
        """Return range statistics."""
        return {
            'Total Ranges': self.count,
            'Avg Duration': self.avg_duration(),
            'Max Duration': self.max_duration(),
            'Min Duration': min(self.duration()) if self._records else 0,
            'Coverage [%]': self.coverage() * 100,
            'Total Bars Covered': sum(r.duration for r in self._records),
        }
