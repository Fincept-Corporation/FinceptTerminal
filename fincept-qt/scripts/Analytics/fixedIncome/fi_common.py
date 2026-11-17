"""
Shared helpers for scripts/Analytics/fixedIncome (a library, not a CLI: no __main__).

Conventions used by every module in this directory
--------------------------------------------------
* Rates, yields and spreads are decimals (0.05 = 5 %), never percent.
* A bond yield (ytm / ytc / ytw) is a nominal annual rate compounded `frequency`
  times a year (frequency 2 = US bond-equivalent basis). frequency 0 means a
  zero-coupon bond priced with annual compounding.
* `price` is the clean (quoted) price. dirty_price = price + accrued_interest =
  present value of the remaining cash flows. For a bond whose remaining life is
  a whole number of coupon periods the two are equal.
* Time to maturity given in years is measured from settlement. When
  years_to_maturity * frequency is not a whole number the first period is a
  fractional one (street convention): cash flow k is discounted with
  (1 + y/f) ** -(k - 1 + v), where v is the fraction of the current coupon
  period still to run, and accrued interest is coupon_per_period * (1 - v).
* Day counts (accrued interest when dates or days are supplied):
    30/360  - 30/360 bond basis (ISDA): AI = C_annual * days30 / 360
    30E/360 - Eurobond basis:           AI = C_annual * days30E / 360
    ACT/360 - AI = C_annual * actual_days / 360
    ACT/365 - AI = C_annual * actual_days / 365 (fixed)
    ACT/ACT - ICMA: AI = C_period * actual_days / actual_days_in_period
  The discounting exponent v uses 30/360 days for the 30/360 family and actual
  days otherwise. Coupon dates are rolled back from maturity in whole months
  (unadjusted for business days; day-of-month clamped to the month end, and every
  coupon falls on a month end when maturity does - the end-of-month rule).
"""

from __future__ import annotations

import bisect
import calendar
import json
import math
import os

# The matrices here are tiny; multi-threaded BLAS only burns CPU on a shared host. Respect explicit settings.
for _var in ("OMP_NUM_THREADS", "OPENBLAS_NUM_THREADS", "MKL_NUM_THREADS"):
    os.environ.setdefault(_var, "1")
from datetime import date, datetime
from statistics import NormalDist
from typing import Any, Callable, Dict, Iterable, List, Optional, Sequence, Tuple

__all__ = [
    "FIError", "num", "num_list", "text", "boolean", "pairs", "require_dict",
    "norm_day_count", "parse_date", "BondTiming", "timing_from_years", "timing_from_dates",
    "timing_from_days", "bond_flows", "dirty_price", "solve_yield", "risk_measures",
    "find_root", "ZeroCurve", "bootstrap_zero_curve", "par_yield_instruments",
    "bond_instruments", "NORMAL", "VALID_FREQUENCIES", "r",
]

NORMAL = NormalDist()

# Coupon frequencies that divide a year into whole months (0 = zero coupon).
VALID_FREQUENCIES = (0, 1, 2, 3, 4, 6, 12)

_MISSING = object()


class FIError(ValueError):
    """Bad input. The message is plain text meant for the caller."""


def r(x: Optional[float], nd: int = 6) -> Optional[float]:
    """round() that passes None (and non-finite) through as None."""
    if x is None:
        return None
    x = float(x)
    if math.isnan(x) or math.isinf(x):
        return None
    return round(x, nd)


# ---------------------------------------------------------------------------
# Parameter coercion / validation
# ---------------------------------------------------------------------------

def require_dict(value: Any, what: str = "params") -> Dict[str, Any]:
    if not isinstance(value, dict):
        raise FIError(f"{what} must be a JSON object")
    return value


def _fmt(x: Any) -> str:
    if isinstance(x, float):
        return f"{x:g}"
    return repr(x) if isinstance(x, str) else str(x)


def _lookup(params: Dict[str, Any], key: str, aliases: Iterable[str]) -> Tuple[Optional[str], Any]:
    for k in (key, *aliases):
        if k in params:
            return k, params[k]
    return None, _MISSING


def _to_float(name: str, value: Any) -> float:
    if isinstance(value, bool):
        raise FIError(f"{name} must be a number (got {_fmt(value)})")
    if isinstance(value, (int, float)):
        out = float(value)
    elif isinstance(value, str):
        try:
            out = float(value.strip())
        except ValueError:
            raise FIError(f"{name} must be a number (got {_fmt(value)})") from None
    else:
        raise FIError(f"{name} must be a number (got {type(value).__name__})")
    if math.isnan(out) or math.isinf(out):
        raise FIError(f"{name} must be a finite number")
    return out


def _check_bounds(name: str, x: float, lo, hi, lo_open: bool, hi_open: bool) -> None:
    if lo is not None:
        if lo_open and not x > lo:
            raise FIError(f"{name} must be > {_fmt(lo)} (got {_fmt(x)})")
        if not lo_open and not x >= lo:
            raise FIError(f"{name} must be >= {_fmt(lo)} (got {_fmt(x)})")
    if hi is not None:
        if hi_open and not x < hi:
            raise FIError(f"{name} must be < {_fmt(hi)} (got {_fmt(x)})")
        if not hi_open and not x <= hi:
            raise FIError(f"{name} must be <= {_fmt(hi)} (got {_fmt(x)})")


def num(params: Dict[str, Any], key: str, default: Any = _MISSING, *, aliases: Sequence[str] = (),
        lo=None, hi=None, lo_open: bool = False, hi_open: bool = False,
        integer: bool = False, allow_none: bool = False):
    """Read a numeric parameter. Strings such as "0.06" are accepted.

    A JSON null counts as "not given" unless allow_none, in which case None is returned.
    """
    used, value = _lookup(params, key, aliases)
    if value is None:
        if allow_none:
            return None
        value = _MISSING
    if value is _MISSING:
        if default is _MISSING:
            raise FIError(f"{key} is required")
        value = default
        if value is None:
            return None
        used = key
    name = used or key
    x = _to_float(name, value)
    if integer:
        if abs(x - round(x)) > 1e-9:
            raise FIError(f"{name} must be a whole number (got {_fmt(x)})")
        x = int(round(x))
    _check_bounds(name, x, lo, hi, lo_open, hi_open)
    return x


def as_num(value: Any, name: str, *, lo=None, hi=None, lo_open: bool = False, hi_open: bool = False) -> float:
    """Coerce a single value (e.g. a list element) to a bounded float with a plain error message."""
    x = _to_float(name, value)
    _check_bounds(name, x, lo, hi, lo_open, hi_open)
    return x


def num_list(params: Dict[str, Any], key: str, default: Any = _MISSING, *, aliases: Sequence[str] = (),
             min_len: int = 0, lo=None, hi=None, lo_open: bool = False, hi_open: bool = False) -> List[float]:
    used, value = _lookup(params, key, aliases)
    if value is None or value is _MISSING:
        if default is _MISSING:
            raise FIError(f"{key} is required (a list of numbers)")
        value = default
        used = key
    name = used or key
    if isinstance(value, str):
        try:
            value = json.loads(value)
        except ValueError:
            raise FIError(f"{name} must be a list of numbers") from None
    if not isinstance(value, (list, tuple)):
        raise FIError(f"{name} must be a list of numbers")
    out = []
    for i, v in enumerate(value):
        x = _to_float(f"{name}[{i}]", v)
        _check_bounds(f"{name}[{i}]", x, lo, hi, lo_open, hi_open)
        out.append(x)
    if len(out) < min_len:
        raise FIError(f"{name} needs at least {min_len} value(s) (got {len(out)})")
    return out


def text(params: Dict[str, Any], key: str, default: Any = _MISSING, *, aliases: Sequence[str] = (),
         choices: Optional[Iterable[str]] = None, lower: bool = True, allow_none: bool = False):
    used, value = _lookup(params, key, aliases)
    if value is None or value is _MISSING:
        if allow_none and value is None and used is not None:
            return None
        if default is _MISSING:
            raise FIError(f"{key} is required")
        if default is None:
            return None
        value = default
        used = key
    name = used or key
    if not isinstance(value, str):
        raise FIError(f"{name} must be a string (got {_fmt(value)})")
    out = value.strip()
    if lower:
        out = out.lower()
    if choices is not None:
        choices = list(choices)
        if out not in choices:
            raise FIError(f"{name} must be one of: {', '.join(choices)} (got {_fmt(value)})")
    return out


def boolean(params: Dict[str, Any], key: str, default: bool = False, *, aliases: Sequence[str] = ()) -> bool:
    used, value = _lookup(params, key, aliases)
    if value is None or value is _MISSING:
        return default
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)) and value in (0, 1):
        return bool(value)
    if isinstance(value, str) and value.strip().lower() in ("true", "false", "1", "0", "yes", "no"):
        return value.strip().lower() in ("true", "1", "yes")
    raise FIError(f"{used or key} must be true or false (got {_fmt(value)})")


_PAIR_VALUE_KEYS = ("rate", "spot_rate", "yield", "value", "forward_rate", "amount", "cash_flow")


def pairs(params: Dict[str, Any], key: str, default: Any = _MISSING, *, aliases: Sequence[str] = (),
          min_len: int = 1, what: str = "[time, value]") -> List[Tuple[float, float]]:
    """Read a list of (time, value) pairs given as [[t, v], ...] or [{"maturity": t, "rate": v}, ...]."""
    used, value = _lookup(params, key, aliases)
    if value is None or value is _MISSING:
        if default is _MISSING:
            raise FIError(f"{key} is required (a list of {what} pairs)")
        value = default
        used = key
    name = used or key
    if not isinstance(value, (list, tuple)):
        raise FIError(f"{name} must be a list of {what} pairs")
    out = []
    for i, item in enumerate(value):
        if isinstance(item, dict):
            t = item.get("maturity", item.get("time", item.get("t")))
            v = next((item[k] for k in _PAIR_VALUE_KEYS if k in item), None)
            if t is None or v is None:
                raise FIError(f"{name}[{i}] needs a maturity/time and a rate/value")
        elif isinstance(item, (list, tuple)) and len(item) == 2:
            t, v = item
        else:
            raise FIError(f"{name}[{i}] must be a {what} pair")
        out.append((_to_float(f"{name}[{i}] time", t), _to_float(f"{name}[{i}] value", v)))
    if len(out) < min_len:
        raise FIError(f"{name} needs at least {min_len} point(s) (got {len(out)})")
    return out


# ---------------------------------------------------------------------------
# Root finding (pure Python: bracketed bisection polished by secant steps)
# ---------------------------------------------------------------------------

def overflow_safe(fn: Callable[[float], float]) -> Callable[[float], float]:
    """Wrap a 'present value minus target' function: float overflow (a discount base near zero raised to a
    large negative power) means the present value is unbounded, i.e. +inf."""
    def wrapped(x: float) -> float:
        try:
            return fn(x)
        except (OverflowError, ZeroDivisionError):  # base**t overflowed, or underflowed to 0 in a divisor
            return float("inf")
    return wrapped


def find_root(fn: Callable[[float], float], lo: float, hi: float, *, tol: float = 1e-12,
              max_iter: int = 300) -> float:
    """Root of a continuous fn on [lo, hi] whose ends have opposite signs (Brent-style safeguarded).
    fn must be a decreasing 'PV minus target' style function if it can overflow (overflow -> +inf)."""
    fn = overflow_safe(fn)
    flo, fhi = fn(lo), fn(hi)
    if flo == 0:
        return lo
    if fhi == 0:
        return hi
    if (flo > 0) == (fhi > 0):
        raise FIError("no solution in the search range")
    a, b, fa, fb = lo, hi, flo, fhi
    for _ in range(max_iter):
        # secant guess, fall back to bisection when it leaves the bracket
        m = 0.5 * (a + b)
        if fb != fa:
            s = b - fb * (b - a) / (fb - fa)
            x = s if (min(a, b) < s < max(a, b)) else m
        else:
            x = m
        fx = fn(x)
        if fx == 0 or abs(b - a) < tol:
            return x
        if (fx > 0) == (fa > 0):
            a, fa = x, fx
        else:
            b, fb = x, fx
        # guarantee shrinkage: also evaluate the midpoint of the new bracket
        m = 0.5 * (a + b)
        fm = fn(m)
        if fm == 0:
            return m
        if (fm > 0) == (fa > 0):
            a, fa = m, fm
        else:
            b, fb = m, fm
        if abs(b - a) < tol:
            break
    return 0.5 * (a + b)


# ---------------------------------------------------------------------------
# Dates and day counts
# ---------------------------------------------------------------------------

_DAY_COUNT_ALIASES = {
    "30/360": "30/360", "30u/360": "30/360", "30/360 us": "30/360", "bond basis": "30/360",
    "thirty_360": "30/360", "30_360": "30/360",
    "30e/360": "30E/360", "eurobond": "30E/360", "30e_360": "30E/360", "thirty_360_eu": "30E/360",
    "act/360": "ACT/360", "actual/360": "ACT/360", "a/360": "ACT/360", "act_360": "ACT/360",
    "act/365": "ACT/365", "act/365f": "ACT/365", "actual/365": "ACT/365", "a/365": "ACT/365",
    "act_365": "ACT/365", "actual/365 fixed": "ACT/365",
    "act/act": "ACT/ACT", "actual/actual": "ACT/ACT", "act/act icma": "ACT/ACT", "icma": "ACT/ACT",
    "isma": "ACT/ACT", "act_act": "ACT/ACT",
}
DAY_COUNTS = ("30/360", "30E/360", "ACT/360", "ACT/365", "ACT/ACT")


def norm_day_count(value: Any) -> str:
    if value is None:
        return "30/360"
    if not isinstance(value, str) or value.strip().lower() not in _DAY_COUNT_ALIASES:
        raise FIError(f"day_count must be one of: {', '.join(DAY_COUNTS)} (got {_fmt(value)})")
    return _DAY_COUNT_ALIASES[value.strip().lower()]


def parse_date(value: Any, name: str) -> date:
    if isinstance(value, date):
        return value
    if not isinstance(value, str):
        raise FIError(f"{name} must be a date string YYYY-MM-DD")
    try:
        return datetime.strptime(value.strip()[:10], "%Y-%m-%d").date()
    except ValueError:
        raise FIError(f"{name} must be a date string YYYY-MM-DD (got {_fmt(value)})") from None


def add_months(d: date, months: int, eom: bool = False) -> date:
    """Shift by whole months; the day is clamped to the month end, or forced to it when eom is True."""
    y, m0 = divmod(d.month - 1 + months, 12)
    y += d.year
    m = m0 + 1
    last = calendar.monthrange(y, m)[1]
    return date(y, m, last if eom else min(d.day, last))


def days_30_360(d1: date, d2: date, euro: bool = False) -> int:
    D1, D2 = d1.day, d2.day
    if euro:
        D1, D2 = min(D1, 30), min(D2, 30)
    else:
        if D1 == 31:
            D1 = 30
        if D2 == 31 and D1 == 30:
            D2 = 30
    return 360 * (d2.year - d1.year) + 30 * (d2.month - d1.month) + (D2 - D1)


def year_fraction(d1: date, d2: date, day_count: str) -> float:
    """Year fraction between two dates (used for zero-coupon bonds given by dates)."""
    if day_count == "30/360":
        return days_30_360(d1, d2) / 360.0
    if day_count == "30E/360":
        return days_30_360(d1, d2, euro=True) / 360.0
    if day_count == "ACT/360":
        return (d2 - d1).days / 360.0
    if day_count == "ACT/365":
        return (d2 - d1).days / 365.0
    # ACT/ACT ISDA: split by calendar year
    total, cur = 0.0, d1
    while cur < d2:
        nxt = min(date(cur.year + 1, 1, 1), d2)
        total += (nxt - cur).days / (366.0 if calendar.isleap(cur.year) else 365.0)
        cur = nxt
    return total


# ---------------------------------------------------------------------------
# Bond timing: how many coupons remain and where settlement sits in the period
# ---------------------------------------------------------------------------

class BondTiming:
    """n remaining coupons; v = fraction of the current period still to run (0 < v <= 1);
    accrued_fraction = accrued interest per unit of face value (already in currency per 1 face)."""

    __slots__ = ("n", "v", "frequency", "accrued_per_face", "years_from_settlement", "info")

    def __init__(self, n: int, v: float, frequency: int, accrued_per_face: float,
                 years_from_settlement: float, info: Dict[str, Any]):
        self.n, self.v, self.frequency = n, v, frequency
        self.accrued_per_face = accrued_per_face
        self.years_from_settlement = years_from_settlement
        self.info = info


def check_frequency(frequency: int, allow_zero: bool = True) -> int:
    if frequency not in VALID_FREQUENCIES or (frequency == 0 and not allow_zero):
        allowed = [str(f) for f in VALID_FREQUENCIES if allow_zero or f]
        raise FIError(f"frequency must be one of {', '.join(allowed)} (0 = zero-coupon); got {frequency}")
    return frequency


def timing_from_years(years: float, frequency: int, coupon_rate: float) -> BondTiming:
    """Remaining life in years measured from settlement (fractional first period allowed)."""
    if years <= 0:
        raise FIError(f"years_to_maturity must be > 0 (got {_fmt(years)})")
    if frequency == 0:
        return BondTiming(1, 1.0, 0, 0.0, years, {"basis": "years from settlement", "compounding": "annual"})
    x = years * frequency
    n = int(math.ceil(x - 1e-9))
    n = max(n, 1)
    v = x - (n - 1)
    if abs(v - 1.0) < 1e-9:
        v = 1.0
    v = min(max(v, 1e-12), 1.0)
    w = 1.0 - v
    ai = coupon_rate / frequency * w
    return BondTiming(n, v, frequency, ai, years, {
        "basis": "years from settlement",
        "remaining_coupons": n,
        "fraction_of_period_to_next_coupon": round(v, 10),
        "accrual_fraction": round(w, 10),
        "day_count": "period fraction (ACT/ACT-style)",
    })


def timing_from_days(period_count: float, frequency: int, coupon_rate: float, days_since: float,
                     days_in_period: Optional[float], day_count: str, years_explicit_days: bool) -> BondTiming:
    """Settlement `days_since` days into the current coupon period.

    period_count = years_to_maturity * frequency. When it is a whole number it is taken as the number
    of coupons remaining counted from the LAST coupon date; otherwise years_to_maturity is taken from
    settlement and must agree with days_since.
    """
    if frequency == 0:
        raise FIError("clean/dirty pricing needs a coupon-paying bond (frequency 1, 2, 3, 4, 6 or 12)")
    if day_count in ("30/360", "30E/360"):
        E = 360.0 / frequency
        if days_in_period is not None and abs(days_in_period - E) > 1e-9:
            # 30/360 periods are always 360/f days; a different value is a contradiction
            raise FIError(f"days_in_coupon_period must be {E:g} under {day_count} with frequency {frequency}")
    elif days_in_period is None:
        E = (360.0 if day_count == "ACT/360" else 365.0) / frequency
    else:
        E = float(days_in_period)
    if E <= 0:
        raise FIError("days_in_coupon_period must be > 0")
    A = float(days_since)
    if A < 0 or A >= E:
        raise FIError(f"days_since_last_coupon must be >= 0 and < days_in_coupon_period ({E:g}); got {A:g}")
    whole = abs(period_count - round(period_count)) < 1e-9
    if whole:
        n = int(round(period_count))
        if n < 1:
            raise FIError("years_to_maturity must cover at least one coupon period")
        basis = "years from the last coupon date"
    else:
        n = int(math.ceil(period_count - 1e-9))
        implied_A = (1.0 - (period_count - (n - 1))) * E
        if years_explicit_days and abs(implied_A - A) > 0.5:
            raise FIError(
                "years_to_maturity and days_since_last_coupon disagree: a whole number of coupon periods means "
                "maturity counted from the last coupon date; a fractional one means time from settlement, which "
                f"implies {implied_A:.1f} days since the last coupon (got {A:g}). Pass settlement_date and "
                "maturity_date instead to avoid the ambiguity")
        A = implied_A
        basis = "years from settlement"
    w = A / E
    v = 1.0 - w
    if day_count in ("30/360", "30E/360", "ACT/360"):
        ai = coupon_rate * A / 360.0
    elif day_count == "ACT/365":
        ai = coupon_rate * A / 365.0
    else:  # ACT/ACT ICMA
        ai = coupon_rate / frequency * w
    years_from_settlement = (n - 1 + v) / frequency
    return BondTiming(n, v, frequency, ai, years_from_settlement, {
        "basis": basis,
        "remaining_coupons": n,
        "days_since_last_coupon": round(A, 6),
        "days_in_coupon_period": round(E, 6),
        "fraction_of_period_to_next_coupon": round(v, 10),
        "accrual_fraction": round(w, 10),
        "day_count": day_count,
        "years_from_settlement": round(years_from_settlement, 10),
    })


def timing_from_dates(settlement: date, maturity: date, frequency: int, coupon_rate: float,
                      day_count: str) -> BondTiming:
    if maturity <= settlement:
        raise FIError("maturity_date must be after settlement_date")
    if frequency == 0:
        t = year_fraction(settlement, maturity, day_count)
        return BondTiming(1, 1.0, 0, 0.0, t, {"basis": "dates", "settlement_date": settlement.isoformat(),
                                              "maturity_date": maturity.isoformat(), "day_count": day_count,
                                              "years_from_settlement": round(t, 10), "compounding": "annual"})
    step = 12 // frequency
    # end-of-month rule: a bond maturing on the last day of a month pays on month ends
    eom = maturity.day == calendar.monthrange(maturity.year, maturity.month)[1]
    k = 1
    while add_months(maturity, -k * step, eom) > settlement:
        k += 1
        if k > 12 * 200:
            raise FIError("maturity_date is too far from settlement_date")
    prev_cpn = add_months(maturity, -k * step, eom)
    next_cpn = add_months(maturity, -(k - 1) * step, eom)
    n = k
    if day_count in ("30/360", "30E/360"):
        euro = day_count == "30E/360"
        E = 360.0 / frequency
        A = float(days_30_360(prev_cpn, settlement, euro))
        A = min(A, E)
        w = A / E
        v = 1.0 - w
        ai = coupon_rate * A / 360.0
    else:
        E = float((next_cpn - prev_cpn).days)
        A = float((settlement - prev_cpn).days)
        DSC = float((next_cpn - settlement).days)
        w = A / E
        v = DSC / E
        if day_count == "ACT/360":
            ai = coupon_rate * A / 360.0
        elif day_count == "ACT/365":
            ai = coupon_rate * A / 365.0
        else:
            ai = coupon_rate / frequency * w
    if v <= 0:  # settlement on a coupon date is handled by the loop; guard anyway
        v = 1.0
    years_from_settlement = (n - 1 + v) / frequency
    return BondTiming(n, v, frequency, ai, years_from_settlement, {
        "basis": "dates",
        "settlement_date": settlement.isoformat(),
        "maturity_date": maturity.isoformat(),
        "previous_coupon_date": prev_cpn.isoformat(),
        "next_coupon_date": next_cpn.isoformat(),
        "remaining_coupons": n,
        "days_since_last_coupon": A,
        "days_in_coupon_period": E,
        "fraction_of_period_to_next_coupon": round(v, 10),
        "accrual_fraction": round(w, 10),
        "day_count": day_count,
        "years_from_settlement": round(years_from_settlement, 10),
    })


def resolve_timing(params: Dict[str, Any], coupon_rate: float, frequency: int, *,
                   years_key: str = "years_to_maturity", default_years: float = 10.0) -> BondTiming:
    """Timing from settlement_date/maturity_date when both are given, else from years_to_maturity."""
    has_dates = params.get("settlement_date") is not None or params.get("maturity_date") is not None
    if has_dates:
        if params.get("settlement_date") is None or params.get("maturity_date") is None:
            raise FIError("give both settlement_date and maturity_date (YYYY-MM-DD), or neither")
        dc = norm_day_count(params.get("day_count"))
        return timing_from_dates(parse_date(params["settlement_date"], "settlement_date"),
                                 parse_date(params["maturity_date"], "maturity_date"),
                                 frequency, coupon_rate, dc)
    years = num(params, years_key, default_years, lo=0, lo_open=True)
    return timing_from_years(years, frequency, coupon_rate)


# ---------------------------------------------------------------------------
# Cash flows, price, yield, duration
# ---------------------------------------------------------------------------

def bond_flows(face: float, coupon_rate: float, timing: BondTiming,
               redemption: Optional[float] = None) -> List[Tuple[float, float]]:
    """[(exponent_in_periods, amount)]; for frequency 0 the exponent is in years."""
    red = face if redemption is None else redemption
    if timing.frequency == 0:
        return [(timing.years_from_settlement, red)]
    c = face * coupon_rate / timing.frequency
    flows = [(k - 1 + timing.v, c) for k in range(1, timing.n + 1)]
    e, amt = flows[-1]
    flows[-1] = (e, amt + red)
    return flows


def _base(y: float, frequency: int) -> float:
    return 1.0 + (y if frequency == 0 else y / frequency)


def dirty_price(y: float, flows: Sequence[Tuple[float, float]], frequency: int) -> float:
    b = _base(y, frequency)
    if b <= 0:
        return float("inf")
    try:
        return sum(cf * b ** (-e) for e, cf in flows)
    except OverflowError:
        return float("inf")


def solve_yield(target_dirty: float, flows: Sequence[Tuple[float, float]], frequency: int) -> float:
    """Yield (nominal annual, compounded `frequency` times; annual for 0) that prices flows at target."""
    if target_dirty <= 0:
        raise FIError("price must be > 0")
    lo = -(frequency if frequency else 1) * 0.999999
    hi = 1.0
    fn = lambda y: dirty_price(y, flows, frequency) - target_dirty
    if fn(lo) < 0:
        raise FIError("price is too high: no yield above -100% reproduces it")
    while fn(hi) > 0:
        hi *= 2.0
        if hi > 1e6:
            raise FIError("price is too low: the implied yield exceeds 1,000,000%")
    return find_root(fn, lo, hi, tol=1e-14)


def risk_measures(y: float, flows: Sequence[Tuple[float, float]], frequency: int) -> Dict[str, float]:
    """Macaulay / modified duration and convexity (years, years^2) on the dirty price."""
    f = frequency or 1
    b = _base(y, frequency)
    P = 0.0
    s1 = 0.0
    s2 = 0.0
    for e, cf in flows:
        pv = cf * b ** (-e)
        P += pv
        s1 += e * pv
        s2 += e * (e + 1.0) * pv
    if P <= 0:
        raise FIError("bond price is not positive at this yield")
    mac = s1 / P / f
    mod = mac / b
    conv = s2 / (P * b * b * f * f)
    return {"dirty_price": P, "macaulay": mac, "modified": mod, "convexity": conv, "weighted_sum": s1 / f}


# ---------------------------------------------------------------------------
# Zero curve and bootstrap
# ---------------------------------------------------------------------------

class ZeroCurve:
    """Continuously-compounded zero rates at knot maturities; linear interpolation in the zero rate,
    flat extrapolation on both sides."""

    def __init__(self):
        self.t: List[float] = []
        self.z: List[float] = []

    def z_at(self, t: float, extra: Optional[Tuple[float, float]] = None) -> float:
        ts, zs = self.t, self.z
        if extra is not None:
            ts, zs = ts + [extra[0]], zs + [extra[1]]
        if not ts:
            raise FIError("empty curve")
        if t <= ts[0]:
            return zs[0]
        if t >= ts[-1]:
            return zs[-1]
        i = bisect.bisect_right(ts, t)
        t0, t1, z0, z1 = ts[i - 1], ts[i], zs[i - 1], zs[i]
        return z0 + (z1 - z0) * (t - t0) / (t1 - t0)

    def df(self, t: float, extra: Optional[Tuple[float, float]] = None) -> float:
        if t <= 0:
            return 1.0
        return math.exp(-self.z_at(t, extra) * t)

    def add(self, t: float, z: float) -> None:
        self.t.append(t)
        self.z.append(z)


def _instrument_flows(maturity: float, coupon_rate: float, face: float, frequency: int) -> Tuple[List[Tuple[float, float]], float]:
    """Cash flows [(time_years, amount)] and accrued interest for a bond `maturity` years from settlement."""
    if frequency == 0 or coupon_rate == 0:
        return [(maturity, face)], 0.0
    timing = timing_from_years(maturity, frequency, coupon_rate)
    c = face * coupon_rate / frequency
    flows = [((k - 1 + timing.v) / frequency, c) for k in range(1, timing.n + 1)]
    t_last, amt = flows[-1]
    flows[-1] = (t_last, amt + face)
    return flows, face * timing.accrued_per_face


def bond_instruments(bonds: Any, default_frequency: int, *, maturity_keys=("maturity", "years")) -> List[Dict[str, Any]]:
    if not isinstance(bonds, list) or not bonds:
        raise FIError("bonds must be a non-empty list of {maturity, coupon_rate, price[, face_value, frequency, price_type]}")
    out = []
    for i, b in enumerate(bonds):
        if not isinstance(b, dict):
            raise FIError(f"bonds[{i}] must be an object")
        mkey = next((k for k in maturity_keys if b.get(k) is not None), None)
        if mkey is None:
            raise FIError(f"bonds[{i}] needs a maturity (years from settlement)")
        T = num(b, mkey, lo=0, lo_open=True)
        c = num(b, "coupon_rate", 0.0, lo=0)
        F = num(b, "face_value", 1000.0, lo=0, lo_open=True)
        P = num(b, "price", lo=0, lo_open=True)
        f = check_frequency(num(b, "frequency", default_frequency, integer=True))
        ptype = text(b, "price_type", "clean", choices=("clean", "dirty"))
        flows, ai = _instrument_flows(T, c, F, f)
        dirty = P + ai if ptype == "clean" else P
        out.append({"maturity": T, "coupon_rate": c, "face_value": F, "frequency": f, "price": P,
                    "price_type": ptype, "accrued_interest": ai, "dirty_price": dirty, "flows": flows,
                    "input_index": i})
    return out


def par_yield_instruments(maturities: Sequence[float], yields: Sequence[float], frequency: int,
                          face: float = 100.0) -> List[Dict[str, Any]]:
    """Par bonds from (maturity, par yield) points. Maturities up to one coupon period are treated as
    zero-coupon instruments with the yield compounded at `frequency` (bills on a bond-equivalent basis)."""
    out = []
    f = frequency or 1
    for i, (T, y) in enumerate(zip(maturities, yields)):
        if T <= 0:
            raise FIError(f"maturities[{i}] must be > 0")
        if frequency == 0 or T <= 1.0 / f + 1e-12:
            dfac = (1.0 + y / f) ** (-T * f) if frequency else (1.0 + y) ** (-T)
            flows = [(T, face)]
            dirty = face * dfac
            ai = 0.0
            price, ptype = dirty, "zero-coupon at the yield"
        else:
            flows, ai = _instrument_flows(T, y, face, frequency)
            dirty = face + ai
            price, ptype = face, "par"
        out.append({"maturity": T, "coupon_rate": y, "face_value": face, "frequency": frequency, "price": price,
                    "price_type": ptype, "accrued_interest": ai, "dirty_price": dirty, "flows": flows,
                    "input_index": i})
    return out


def bootstrap_zero_curve(instruments: List[Dict[str, Any]]) -> Tuple[ZeroCurve, List[Dict[str, Any]]]:
    """Sequential bootstrap. Each instrument adds one knot at its maturity; coupons between the previous
    knot and the new one are discounted with the zero rate interpolated towards the unknown knot, which is
    solved exactly. Instruments must have distinct maturities."""
    inst = sorted(instruments, key=lambda d: d["maturity"])
    for a, b in zip(inst, inst[1:]):
        if abs(a["maturity"] - b["maturity"]) < 1e-9:
            raise FIError(f"two instruments share maturity {a['maturity']:g}; give one price per maturity")
    curve = ZeroCurve()
    for d in inst:
        T = d["maturity"]
        flows = d["flows"]
        target = d["dirty_price"]

        def pv(z, flows=flows, T=T):
            return sum(cf * curve.df(t, (T, z)) for t, cf in flows)

        fn = overflow_safe(lambda z: pv(z) - target)
        lo, hi = -0.5, 1.0
        if fn(lo) < 0 or fn(hi) > 0:
            raise FIError(
                f"the bond maturing in {T:g}y (price {d['price']:g}) cannot be fitted: it implies a zero rate "
                "outside -50%..+100% given the shorter bonds; check its price and coupon")
        z = find_root(fn, lo, hi, tol=1e-15)
        curve.add(T, z)
        d["zero_rate_cc"] = z
    return curve, inst


def price_on_curve(flows: Sequence[Tuple[float, float]], curve: ZeroCurve) -> float:
    return sum(cf * curve.df(t) for t, cf in flows)


def linear_interp(xs: Sequence[float], ys: Sequence[float], x: float) -> Tuple[float, bool]:
    """Linear interpolation with flat extrapolation. Returns (value, extrapolated)."""
    if x <= xs[0]:
        return ys[0], x < xs[0] - 1e-12
    if x >= xs[-1]:
        return ys[-1], x > xs[-1] + 1e-12
    i = bisect.bisect_right(xs, x)
    x0, x1, y0, y1 = xs[i - 1], xs[i], ys[i - 1], ys[i]
    return y0 + (y1 - y0) * (x - x0) / (x1 - x0), False


def sorted_curve(maturities: Sequence[float], values: Sequence[float], *, min_points: int = 2,
                 names: Tuple[str, str] = ("maturities", "yields"),
                 percent_guard: bool = True) -> Tuple[List[float], List[float]]:
    if len(maturities) != len(values):
        raise FIError(f"{names[0]} and {names[1]} must have the same length ({len(maturities)} vs {len(values)})")
    if len(maturities) < min_points:
        raise FIError(f"need at least {min_points} curve points (got {len(maturities)})")
    pts = sorted(zip(maturities, values))
    for i, (t, _) in enumerate(pts):
        if t < 0:
            raise FIError(f"{names[0]} must be >= 0")
        if i and abs(t - pts[i - 1][0]) < 1e-12:
            raise FIError(f"duplicate maturity {t:g} in {names[0]}")
    if percent_guard and any(abs(v) > 1.0 for _, v in pts):
        raise FIError(f"{names[1]} look like percentages; pass decimals (4.5% -> 0.045)")
    return [p[0] for p in pts], [p[1] for p in pts]


def emit_usage_or_run(argv: Sequence[str], runner: Callable[[Dict[str, Any]], Dict[str, Any]],
                      analysis_types: Sequence[str], module: str) -> None:
    """Shared __main__ for the modules: python <module>.py '<json with analysis_type>' -> one JSON payload."""
    import fincept_json  # caller guarantees scripts/ is importable
    if len(argv) < 2:
        fincept_json.emit({"error": f"usage: python {module}.py '<json params with analysis_type>' "
                                    f"(or use cli.py <command> '<json>')",
                           "analysis_types": list(analysis_types)})
        return
    try:
        params = json.loads(argv[1])
    except ValueError as e:
        fincept_json.emit({"error": f"Invalid JSON parameters: {e}"})
        return
    if not isinstance(params, dict):
        fincept_json.emit({"error": "params must be a JSON object"})
        return
    try:
        result = runner(params)
    except Exception as e:  # never a traceback instead of JSON
        result = {"error": str(e) or type(e).__name__}
    fincept_json.emit(result)


def import_fincept_json():
    """Make scripts/ importable when a module is run by path without PYTHONPATH."""
    try:
        import fincept_json  # noqa: F401
    except ImportError:
        import os
        import sys
        sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
        import fincept_json  # noqa: F401
