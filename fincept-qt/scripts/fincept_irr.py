"""Internal rate of return for equally spaced cash flows — one robust implementation for every script.

WHY THIS EXISTS
---------------
``numpy.irr`` was removed in NumPy 1.20 and ``numpy_financial`` is not in every install, so the scripts that
called ``np.irr`` inside a bare ``except`` silently reported an IRR of 0 % (or an unrelated fallback rate).
A missing IRR must never look like a computed one.

USAGE
-----
    from fincept_irr import irr
    rate = irr([-300.0, 0, 0, 0, 0, 1100.0])   # 0.29674... ; None when no IRR exists

``scripts/`` is on PYTHONPATH for every script PythonRunner starts. A script that may also be run by hand
from its own folder can fall back to inserting the scripts directory on ``sys.path`` before importing.

METHOD
------
NPV(r) = sum(cf_t / (1 + r)**t), t = 0..n, r > -1. NPV is evaluated on a fixed grid of rates from -99.99 %
to +10,000 %; every interval where NPV changes sign (or hits 0 exactly) is a bracket, refined by bisection
to ~1e-12. Conventional flows (one sign change) have exactly one root, so the bracket is unique. When the
flows change sign more than once and several rates zero the NPV, the root closest to 0 is returned (the
numpy_financial convention). Pure Python: no numpy/scipy needed, identical in venv-numpy1 and venv-numpy2.

Returns ``None`` when no IRR exists (no outflow, no inflow, no sign change of NPV in the range, non-finite
input). A total loss — outflows and nothing but zero inflows — returns -1.0 (-100 %), its limit value.
"""

from __future__ import annotations

import math
from typing import Iterable, List, Optional

__all__ = ["irr", "npv"]

# Rates at which NPV is sampled to find sign changes. Dense around 0 (where real IRRs live), sparse far out.
_GRID: List[float] = sorted(set(
    [-0.9999, -0.999, -0.995, -0.99, -0.98, -0.95, -0.9, -0.85, -0.8, -0.75, -0.7, -0.6, -0.5, -0.4, -0.3]
    + [i / 100.0 for i in range(-25, 101)]          # -25 % .. +100 % in 1 % steps
    + [1.25, 1.5, 1.75, 2.0, 2.5, 3.0, 4.0, 5.0, 7.5, 10.0, 20.0, 50.0, 100.0]))


def npv(rate: float, cash_flows: Iterable[float]) -> float:
    """NPV of `cash_flows` (t = 0, 1, 2, ...) at `rate` (> -1). NaN when it over/underflows."""
    try:
        base = 1.0 + rate
        total = 0.0
        disc = 1.0
        for cf in cash_flows:
            total += cf / disc
            disc *= base
        return total if math.isfinite(total) else float("nan")
    except (OverflowError, ZeroDivisionError):
        return float("nan")


def _bisect(cfs: List[float], lo: float, hi: float, f_lo: float) -> float:
    for _ in range(200):
        mid = 0.5 * (lo + hi)
        f_mid = npv(mid, cfs)
        if f_mid == 0.0 or (hi - lo) <= 1e-12 * max(1.0, abs(mid)):
            return mid
        if not math.isfinite(f_mid):
            # Cannot happen inside a bracket of two finite values for a polynomial; stay safe anyway.
            return mid
        if (f_mid < 0.0) == (f_lo < 0.0):
            lo, f_lo = mid, f_mid
        else:
            hi = mid
    return 0.5 * (lo + hi)


def irr(cash_flows: Iterable[float]) -> Optional[float]:
    """IRR as a fraction (0.15 = 15 %) of equally spaced cash flows starting at t = 0, or None (see module doc)."""
    try:
        cfs = [float(c) for c in cash_flows]
    except (TypeError, ValueError):
        return None
    if len(cfs) < 2 or not all(math.isfinite(c) for c in cfs):
        return None
    has_out = any(c < 0.0 for c in cfs)
    has_in = any(c > 0.0 for c in cfs)
    if has_out and not has_in:
        return -1.0  # total loss: NPV -> sum of outflows < 0 for every r; -100 % is the limit
    if not (has_out and has_in):
        return None

    roots: List[float] = []
    prev_r = None
    prev_f = float("nan")
    for r in _GRID:
        f = npv(r, cfs)
        if not math.isfinite(f):
            prev_r, prev_f = None, float("nan")
            continue
        if f == 0.0:
            roots.append(r)
        elif prev_r is not None and math.isfinite(prev_f) and prev_f != 0.0 and (prev_f < 0.0) != (f < 0.0):
            roots.append(_bisect(cfs, prev_r, r, prev_f))
        prev_r, prev_f = r, f
    if not roots:
        return None
    return min(roots, key=abs)
