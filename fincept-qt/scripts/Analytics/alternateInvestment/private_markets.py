"""Private markets workbench - a fund's dated cash flows -> performance, PME, pacing, book.

CLI (the terminal):  python private_markets.py <command> --stdin      (input JSON object on stdin)
CLI (by hand):       python private_markets.py <command> '<json>'
Output: one JSON object, {"success": true, "data": {...}} or {"success": false, "error": "..."}.

Ledger rows (every command that takes "flows"): {"date": "yyyy-mm-dd", "kind": call|distribution|nav|fee,
"amount": positive number, "note": "..."}. call / fee = cash paid into the fund (fees count as paid-in and
against the commitment), distribution = cash paid back, nav = the fund's reported value on that date.
Rates are fractions (0.0557 = 5.57 %), amounts in the fund's currency.

Commands
  metrics   {flows, commitment?}                        IRR (dated, actual/365), TVPI, DPI, RVPI, paid-in, unfunded
  timeline  {flows, commitment?}                        J-curve series + year-end TVPI / DPI / IRR table
  pme       {flows, commitment?, index: "SPY", prices?: [{date, close}]}
            Kaplan-Schoar PME, Long-Nickels PME IRR, PME+ IRR, Direct Alpha (index from Yahoo Finance,
            adjusted close = dividends reinvested; `prices` given = no download)
  pacing    {commitment, life_years, call_rates[], bow, growth, yield, vintages?, years?}
            Takahashi & Alexander (2002) commitment model: yearly calls / distributions / NAV / unfunded
  book      {funds: [{id, name, strategy, vintage, commitment, currency, flows}]}  pooled IRR / TVPI ...

Methods and references
  XIRR: NPV(r) = sum cf_i / (1 + r)^((d_i - d_0) / 365); every sign change of NPV on a rate grid
        from -99.99 % to 100,000 % is bracketed and solved by bisection (no starting guess to diverge
        from); with several roots the one nearest 10 % is reported (Excel's default guess) and the
        result says so. Reference: Microsoft Excel XIRR example -> 0.373362535.
  NAV roll-forward: flows after the latest NAV statement adjust it (calls added, distributions
        subtracted, floored at 0) - the usual "adjusted NAV" of LP reporting.
  KS-PME: Kaplan & Schoar (2005), J. Finance 60(4): sum(D_t * I_T / I_t) + NAV_T over sum(C_t * I_T / I_t).
  Long-Nickels PME: Long & Nickels (1996): the fund's calls bought the index and its distributions sold
        it; the IRR of the calls, distributions and that index position's value at T.
  PME+: Rouvinez (2003): distributions scaled by lambda so the index position ends at the fund's NAV.
  Direct Alpha: Gredil, Griffiths & Stucke (2014): IRR of the flows compounded to T with the index
        (reported annual; continuous = ln(1 + a)).
  Pacing: Takahashi & Alexander (2002), "Illiquid Alternative Asset Fund Modeling", J. Portfolio
        Management 28(2): C_t = RC_t * (commitment - paid-in), D_t = RD_t * NAV_{t-1} * (1 + G),
        RD_t = max(Y, (t / L)^B), NAV_t = NAV_{t-1} * (1 + G) + C_t - D_t.
"""

import contextlib
import json
import math
import os
import sys
from datetime import date, datetime, timedelta

_SCRIPTS_DIR = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
if _SCRIPTS_DIR not in sys.path:
    sys.path.append(_SCRIPTS_DIR)

try:
    import fincept_json
except ImportError:  # pragma: no cover - run from an unusual folder
    fincept_json = None


def quiet_stdout():
    """Library prints (yfinance, pypme) go to stderr: stdout carries exactly one JSON reply.

    fincept_json's guard is thread-safe and re-entrant; contextlib.redirect_stdout is the fallback
    when the shared module is not on the path. Shared with fund_analytics / real_estate_proforma.
    """
    if fincept_json is not None:
        return fincept_json.stdout_to_stderr()
    return contextlib.redirect_stdout(sys.stderr)  # pragma: no cover


FLOW_KINDS =("call", "distribution", "nav", "fee")
MAX_SERIES_POINTS = 600


class InputError(ValueError):
    """A problem with what the user entered; the message is shown as is."""


# ── Rates of return ──────────────────────────────────────────────────────────

def _npv(rate, amounts, times):
    total = 0.0
    base = 1.0 + rate
    for a, t in zip(amounts, times):
        if t == 0:
            total += a
        else:
            total += a / (base ** t)
    return total


def _rate_grid():
    grid = []
    r = -0.9999
    # dense where returns live, sparse beyond
    while r < -0.5:
        grid.append(r)
        r += 0.01
    r = -0.5
    while r < 1.0:
        grid.append(r)
        r += 0.005
    r = 1.0
    while r < 1000.0:
        grid.append(r)
        r *= 1.15
    grid.append(1000.0)
    return grid


def _solve(amounts, times):
    """All roots of NPV(r) = 0 found by bracketing on the grid, then bisection."""
    grid = _rate_grid()
    values = []
    for r in grid:
        try:
            values.append(_npv(r, amounts, times))
        except (OverflowError, ZeroDivisionError):
            values.append(float("nan"))
    roots = []
    for i in range(len(grid) - 1):
        a, b = values[i], values[i + 1]
        if not (math.isfinite(a) and math.isfinite(b)):
            continue
        if a == 0.0:
            roots.append(grid[i])
            continue
        if (a < 0) != (b < 0):
            lo, hi, flo = grid[i], grid[i + 1], a
            for _ in range(200):
                mid = (lo + hi) / 2.0
                fm = _npv(mid, amounts, times)
                if fm == 0.0 or (hi - lo) < 1e-14:
                    lo = hi = mid
                    break
                if (fm < 0) == (flo < 0):
                    lo, flo = mid, fm
                else:
                    hi = mid
            roots.append((lo + hi) / 2.0)
    return roots


def irr_from_times(amounts, times):
    """(rate | None, note). `times` in years from the first flow."""
    if len(amounts) < 2:
        return None, "IRR needs at least one capital call and a later distribution or NAV."
    if not any(a > 0 for a in amounts) or not any(a < 0 for a in amounts):
        return None, "IRR needs money going in and money coming back (a call and a later distribution or NAV)."
    if max(times) - min(times) <= 0:
        return None, "All cash flows are on the same day, so there is no period to earn a rate over."
    roots = _solve(amounts, times)
    if not roots:
        return None, "No rate between -99.99 % and 100,000 % makes the cash flows add up to zero."
    best = min(roots, key=lambda r: abs(r - 0.10))
    note = ""
    if len(roots) > 1:
        note = "More than one rate fits these cash flows (%s); showing the one nearest 10 %%." % (
            ", ".join("%.2f %%" % (r * 100.0) for r in sorted(roots)))
    return best, note


def xirr(dates, amounts):
    """Dated IRR, actual/365 from the first date. (rate | None, note)."""
    if not dates:
        return irr_from_times([], [])
    d0 = min(dates)
    times = [(d - d0).days / 365.0 for d in dates]
    return irr_from_times(list(amounts), times)


def periodic_irr(amounts):
    """IRR of evenly spaced flows (period 1). (rate | None, note)."""
    return irr_from_times(list(amounts), [float(i) for i in range(len(amounts))])


# ── Ledger ───────────────────────────────────────────────────────────────────

def _parse_date(text):
    s = str(text or "").strip()[:10]
    try:
        return datetime.strptime(s, "%Y-%m-%d").date()
    except ValueError:
        return None


def _kind_of(text):
    k = str(text or "").strip().lower()
    synonyms = {"capital call": "call", "contribution": "call", "drawdown": "call", "paid-in": "call",
                "paid in": "call", "investment": "call", "dist": "distribution", "dist.": "distribution",
                "return of capital": "distribution", "proceeds": "distribution", "net asset value": "nav",
                "valuation": "nav", "value": "nav", "residual value": "nav", "fair value": "nav",
                "fees": "fee", "management fee": "fee", "type": ""}
    k = synonyms.get(k, k)
    return k if k in FLOW_KINDS else None


def parse_flows(rows):
    """[(date, kind, amount)] sorted by date (nav last on its day), and how many rows were dropped."""
    out, ignored = [], 0
    for r in rows or []:
        if not isinstance(r, dict):
            ignored += 1
            continue
        d = _parse_date(r.get("date"))
        k = _kind_of(r.get("kind", r.get("type")))
        try:
            a = abs(float(r.get("amount")))
        except (TypeError, ValueError):
            a = None
        if d is None or k is None or a is None or not math.isfinite(a):
            ignored += 1
            continue
        out.append((d, k, a))
    order = {"call": 0, "fee": 1, "distribution": 2, "nav": 3}
    out.sort(key=lambda f: (f[0], order[f[1]]))
    return out, ignored


def valuation_state(flows, as_of=None):
    """Figures of the ledger as of `as_of` (inclusive; None = everything)."""
    fs = [f for f in flows if as_of is None or f[0] <= as_of]
    called = sum(a for d, k, a in fs if k == "call")
    fees = sum(a for d, k, a in fs if k == "fee")
    distributed = sum(a for d, k, a in fs if k == "distribution")
    navs = [(d, a) for d, k, a in fs if k == "nav"]
    cash = [(d, k, a) for d, k, a in fs if k != "nav"]
    st = {
        "called": called, "fees": fees, "paid_in": called + fees, "distributed": distributed,
        "nav": None, "nav_statement": None, "nav_statement_date": None, "rolled_forward": 0,
        "valuation_date": None, "first_date": fs[0][0] if fs else None,
    }
    last_cash = max((d for d, _, _ in cash), default=None)
    if navs:
        nd, nv = navs[-1]
        later = [(d, k, a) for d, k, a in cash if d > nd]
        adj = nv + sum(a for d, k, a in later if k in ("call", "fee")) - sum(a for d, k, a in later if k == "distribution")
        st["nav"] = max(0.0, adj)
        st["nav_statement"] = nv
        st["nav_statement_date"] = nd
        st["rolled_forward"] = len(later)
        st["valuation_date"] = max(nd, last_cash) if last_cash else nd
    else:
        st["valuation_date"] = last_cash
    if as_of is not None and st["valuation_date"] is not None and st["nav"] is not None:
        st["valuation_date"] = as_of if as_of > st["valuation_date"] else st["valuation_date"]
    return st, fs


def irr_flows(fs, st):
    """Dates and signed amounts for the IRR: calls / fees out, distributions in, NAV at the valuation date."""
    dates, amounts = [], []
    for d, k, a in fs:
        if k in ("call", "fee"):
            dates.append(d)
            amounts.append(-a)
        elif k == "distribution":
            dates.append(d)
            amounts.append(a)
    if st["nav"] is not None and st["nav"] > 0 and st["valuation_date"] is not None:
        dates.append(st["valuation_date"])
        amounts.append(st["nav"])
    return dates, amounts


def _ratio(num, den):
    return num / den if den and den > 0 else None


def fund_metrics(flows, commitment=None, as_of=None):
    st, fs = valuation_state(flows, as_of)
    if not fs:
        raise InputError("Add the fund's cash flows: dated capital calls, distributions and NAV statements.")
    notes = []
    dates, amounts = irr_flows(fs, st)
    rate, irr_note = xirr(dates, amounts) if dates else (None, "")
    if irr_note:
        notes.append(irr_note)
    nav = st["nav"]
    if nav is None:
        notes.append("No NAV statement entered: the remaining value is counted as zero (realised figures only).")
    elif st["rolled_forward"]:
        notes.append("The latest NAV statement (%s) was rolled forward for %d later cash flow(s)."
                     % (st["nav_statement_date"].isoformat(), st["rolled_forward"]))
    paid_in = st["paid_in"]
    residual = nav or 0.0
    out = {
        "irr": rate,
        "tvpi": _ratio(st["distributed"] + residual, paid_in),
        "dpi": _ratio(st["distributed"], paid_in),
        "rvpi": _ratio(residual, paid_in) if nav is not None else None,
        "paid_in": paid_in, "called": st["called"], "fees": st["fees"],
        "distributed": st["distributed"], "nav": nav,
        "nav_statement": st["nav_statement"],
        "nav_date": st["nav_statement_date"].isoformat() if st["nav_statement_date"] else None,
        "valuation_date": st["valuation_date"].isoformat() if st["valuation_date"] else None,
        "first_date": st["first_date"].isoformat() if st["first_date"] else None,
        "net_cash": st["distributed"] - paid_in,
        "total_value": st["distributed"] + residual,
        "gain": st["distributed"] + residual - paid_in,
        "flow_count": len(fs),
        "notes": notes,
    }
    if commitment is not None and commitment > 0:
        out["commitment"] = commitment
        out["unfunded"] = max(0.0, commitment - paid_in)
        out["paid_in_pct"] = paid_in / commitment
    return out


# ── Timeline (J-curve) ───────────────────────────────────────────────────────

def _downsample(idx_count, limit=MAX_SERIES_POINTS):
    if idx_count <= limit:
        return list(range(idx_count))
    step = (idx_count - 1) / float(limit - 1)
    keep = sorted({int(round(i * step)) for i in range(limit)})
    if keep[-1] != idx_count - 1:
        keep.append(idx_count - 1)
    return keep


def timeline(flows, commitment=None):
    m = fund_metrics(flows, commitment)
    dates = sorted({d for d, _, _ in flows})
    pts = []
    for d in dates:
        st, _ = valuation_state(flows, d)
        nav = st["nav"]
        pts.append({
            "date": d.isoformat(),
            "paid_in": st["paid_in"],
            "distributed": st["distributed"],
            "nav": nav,
            "net_cash": st["distributed"] - st["paid_in"],
            "total_value": (st["distributed"] + nav) if nav is not None else None,
        })
    keep = _downsample(len(pts))
    series = [pts[i] for i in keep]

    year_end = []
    if dates:
        last = _parse_date(m["valuation_date"]) or dates[-1]
        for y in range(dates[0].year, last.year + 1):
            ye = min(date(y, 12, 31), last)
            st, fs = valuation_state(flows, ye)
            row = {"year": y, "as_of": ye.isoformat(), "paid_in": st["paid_in"], "distributed": st["distributed"],
                   "nav": st["nav"], "dpi": _ratio(st["distributed"], st["paid_in"]), "tvpi": None, "irr": None}
            if st["nav"] is not None:
                row["tvpi"] = _ratio(st["distributed"] + st["nav"], st["paid_in"])
                st2 = dict(st)
                st2["valuation_date"] = ye
                ds, am = irr_flows(fs, st2)
                rate, _ = xirr(ds, am)
                row["irr"] = rate
            year_end.append(row)
    return {"metrics": m, "series": series, "points": len(pts), "year_end": year_end}


# ── PME ──────────────────────────────────────────────────────────────────────

def _price_lookup(prices):
    """prices: sorted [(date, close)] -> function(date) -> close on or before it (None before the start)."""
    import bisect
    ds = [p[0] for p in prices]

    def at(d):
        i = bisect.bisect_right(ds, d) - 1
        return prices[i][1] if i >= 0 else None
    return at


def fetch_index_prices(symbol, start, end):
    """Daily adjusted closes from Yahoo Finance (dividends reinvested). [(date, close)] or raises InputError."""
    try:
        import yfinance as yf
    except ImportError:
        raise InputError("The yfinance package is missing, so index prices cannot be loaded.")
    try:
        with quiet_stdout():
            hist = yf.Ticker(symbol).history(start=(start - timedelta(days=10)).isoformat(),
                                             end=(end + timedelta(days=5)).isoformat(),
                                             interval="1d", auto_adjust=True)
    except Exception as e:  # network / Yahoo
        raise InputError("Could not load prices for %s - PME not shown; fund metrics above are unaffected. (%s)"
                         % (symbol, str(e).splitlines()[0][:120] if str(e) else type(e).__name__))
    if hist is None or len(hist) == 0 or "Close" not in hist:
        raise InputError("Could not load prices for %s - PME not shown; fund metrics above are unaffected." % symbol)
    out = []
    for ts, close in zip(hist.index, hist["Close"]):
        try:
            c = float(close)
        except (TypeError, ValueError):
            continue
        if math.isfinite(c) and c > 0:
            out.append((ts.date(), c))
    if not out:
        raise InputError("Could not load prices for %s - PME not shown; fund metrics above are unaffected." % symbol)
    return out


def pme_measures(flows, prices):
    """PME measures of the ledger against index closes `prices` [(date, close)] (sorted)."""
    st, fs = valuation_state(flows)
    if st["valuation_date"] is None:
        raise InputError("Add the fund's cash flows first.")
    if not any(k in ("call", "fee") for _, k, _ in fs):
        raise InputError("PME needs at least one capital call.")
    at = _price_lookup(prices)
    T = st["valuation_date"]
    I_T = at(T)
    first = fs[0][0]
    if at(first) is None:
        raise InputError("The index price history starts on %s, after the fund's first cash flow (%s)."
                         % (prices[0][0].isoformat(), first.isoformat()))
    if I_T is None:
        raise InputError("No index price on or before the valuation date %s." % T.isoformat())
    notes = []
    if prices[-1][0] < T - timedelta(days=7):
        notes.append("The index has no price after %s; the valuation date %s uses that last close."
                     % (prices[-1][0].isoformat(), T.isoformat()))
    nav = st["nav"] or 0.0
    contrib = [(d, a) for d, k, a in fs if k in ("call", "fee")]
    dist = [(d, a) for d, k, a in fs if k == "distribution"]
    fv_c = sum(a * I_T / at(d) for d, a in contrib)
    fv_d = sum(a * I_T / at(d) for d, a in dist)
    ks = (fv_d + nav) / fv_c if fv_c > 0 else None

    # Direct Alpha: IRR of index-compounded flows.
    ddates, damts = [], []
    for d, a in contrib:
        ddates.append(d)
        damts.append(-a * I_T / at(d))
    for d, a in dist:
        ddates.append(d)
        damts.append(a * I_T / at(d))
    if nav > 0:
        ddates.append(T)
        damts.append(nav)
    alpha, alpha_note = xirr(ddates, damts)

    # Long-Nickels: the index position bought with calls, sold with distributions.
    ln_nav = sum(a * I_T / at(d) for d, a in contrib) - sum(a * I_T / at(d) for d, a in dist)
    ln_irr, ln_note = None, ""
    if ln_nav < 0:
        ln_note = ("Long-Nickels PME is not defined here: the distributions sold more of the index than the calls "
                   "bought (index position below zero). Use KS-PME or Direct Alpha.")
    else:
        ldates = [d for d, _ in contrib] + [d for d, _ in dist] + [T]
        lamts = [-a for _, a in contrib] + [a for _, a in dist] + [ln_nav]
        ln_irr, ln_note = xirr(ldates, lamts)

    # PME+: distributions scaled so the index position ends at the fund's NAV.
    pme_plus, pp_note, lam = None, "", None
    if fv_d > 0:
        lam = (fv_c - nav) / fv_d
        if lam < 0:
            pp_note = "PME+ is not defined here: the NAV is larger than the index value of all calls."
        else:
            pdates = [d for d, _ in contrib] + [d for d, _ in dist] + ([T] if nav > 0 else [])
            pamts = [-a for _, a in contrib] + [lam * a for _, a in dist] + ([nav] if nav > 0 else [])
            pme_plus, pp_note = xirr(pdates, pamts)
    else:
        pp_note = "PME+ needs at least one distribution."

    fdates, famts = irr_flows(fs, st)
    fund_irr, _ = xirr(fdates, famts)
    out = {
        "valuation_date": T.isoformat(),
        "index_start": at(first),
        "index_end": I_T,
        "index_return": I_T / at(first) - 1.0,
        "ks_pme": ks,
        "direct_alpha": alpha,
        "direct_alpha_continuous": math.log1p(alpha) if alpha is not None and alpha > -1 else None,
        "ln_pme_irr": ln_irr,
        "ln_index_nav": ln_nav,
        "pme_plus_irr": pme_plus,
        "pme_plus_lambda": lam,
        "fund_irr": fund_irr,
        "irr_spread_ln": (fund_irr - ln_irr) if (fund_irr is not None and ln_irr is not None) else None,
        "notes": [n for n in (alpha_note, ln_note, pp_note) if n] + notes,
    }
    if nav <= 0:
        out["notes"].append("No NAV entered: the PME compares realised cash flows only.")
    return out


def pme_series(flows, prices):
    """Fund value vs the Long-Nickels index-equivalent value, at month ends and cash-flow dates."""
    st, fs = valuation_state(flows)
    at = _price_lookup(prices)
    first, T = fs[0][0], st["valuation_date"]
    points = set(d for d, _, _ in fs)
    d = date(first.year, first.month, 1)
    while d <= T:
        nxt = date(d.year + (d.month // 12), d.month % 12 + 1, 1)
        points.add(min(nxt - timedelta(days=1), T))
        d = nxt
    out = []
    for p in sorted(points):
        if p < first:
            continue
        s, sub = valuation_state(flows, p)
        I_p = at(p)
        if I_p is None:
            continue
        idx_val = sum(a * I_p / at(d2) for d2, k, a in sub if k in ("call", "fee")) - \
            sum(a * I_p / at(d2) for d2, k, a in sub if k == "distribution")
        fund_val = s["nav"]
        out.append({"date": p.isoformat(), "fund_nav": fund_val, "index_equivalent": idx_val,
                    "distributed": s["distributed"], "paid_in": s["paid_in"]})
    keep = _downsample(len(out))
    return [out[i] for i in keep]


def mpme_pypme(dates, cashflows, prices, pme_prices):
    """pypme's modified PME (verbose_xpme): (pme_irr, asset_irr). For cross-checks and full NAV paths."""
    with quiet_stdout():
        from pypme import verbose_xpme
        pme_irr, asset_irr, _ = verbose_xpme(dates=dates, cashflows=cashflows, prices=prices, pme_prices=pme_prices)
    return float(pme_irr), float(asset_irr)


def pme(flows, index_symbol, prices_rows=None, commitment=None):
    st, fs = valuation_state(flows)
    if st["valuation_date"] is None:
        raise InputError("Add the fund's cash flows first.")
    symbol = (index_symbol or "").strip().upper() or "SPY"
    if prices_rows:
        prices = []
        for r in prices_rows:
            d = _parse_date(r.get("date"))
            try:
                c = float(r.get("close"))
            except (TypeError, ValueError):
                continue
            if d and math.isfinite(c) and c > 0:
                prices.append((d, c))
        prices.sort()
        source = "prices supplied with the request"
    else:
        prices = fetch_index_prices(symbol, fs[0][0], st["valuation_date"])
        source = "Yahoo Finance adjusted close (dividends reinvested)"
    if not prices:
        raise InputError("Could not load prices for %s - PME not shown; fund metrics above are unaffected." % symbol)
    out = pme_measures(flows, prices)
    out["index"] = symbol
    out["index_source"] = source
    out["index_first_price_date"] = prices[0][0].isoformat()
    out["index_last_price_date"] = prices[-1][0].isoformat()
    out["series"] = pme_series(flows, prices)
    out["metrics"] = fund_metrics(flows, commitment)
    return out


# ── Pacing (Takahashi & Alexander) ───────────────────────────────────────────

def pacing(inp):
    commitment = float(inp.get("commitment", 0) or 0)
    if not commitment > 0:
        raise InputError("Enter the commitment (a positive amount).")
    life = int(inp.get("life_years", 12) or 12)
    if life < 2 or life > 30:
        raise InputError("Fund life must be between 2 and 30 years.")
    rates = [float(x) for x in (inp.get("call_rates") or [0.25, 0.333, 0.5])]
    if not rates or any(not (0 <= r <= 1) for r in rates):
        raise InputError("Call rates are fractions of the uncalled commitment between 0 and 1 (e.g. 0.25, 0.333, 0.5).")
    bow = float(inp.get("bow", 2.5))
    growth = float(inp.get("growth", 0.13))
    yld = float(inp.get("yield", 0.0))
    if bow <= 0:
        raise InputError("The bow must be positive (around 2-3 for buyout funds).")
    if not (-0.9 < growth < 2.0):
        raise InputError("Growth must be between -90 % and 200 % a year.")
    if not (0 <= yld <= 1):
        raise InputError("Yield (minimum distribution rate) must be between 0 and 100 %.")
    vintages = max(1, min(int(inp.get("vintages", 1) or 1), 20))
    years = int(inp.get("years", life + vintages - 1) or (life + vintages - 1))
    years = max(life, min(years, 60))

    def one_fund(n_years):
        rows, nav, paid = [], 0.0, 0.0
        for t in range(1, n_years + 1):
            rc = rates[min(t - 1, len(rates) - 1)] if t <= life else 0.0
            c = rc * (commitment - paid)
            rd = max(yld, (t / float(life)) ** bow) if t <= life else 1.0
            rd = min(rd, 1.0)
            grown = nav * (1.0 + growth)
            dist = rd * grown
            nav = grown + c - dist
            paid += c
            rows.append((c, dist, nav, commitment - paid))
        return rows

    fund = one_fund(years)
    yearly = []
    cum = 0.0
    peak, peak_year, breakeven = 0.0, None, None
    for y in range(1, years + 1):
        calls = dists = nav = unfunded = 0.0
        committed = 0.0
        for v in range(vintages):
            t = y - v
            if t < 1:
                continue
            committed += commitment
            c, d, n, u = fund[t - 1]
            calls += c
            dists += d
            nav += n
            unfunded += u
        net = dists - calls
        cum += net
        if cum < peak:
            peak, peak_year = cum, y
        if breakeven is None and y > 1 and cum >= 0 and peak < 0:
            breakeven = y
        yearly.append({"year": y, "calls": calls, "distributions": dists, "net_cash": net, "nav": nav,
                       "unfunded": unfunded, "cumulative_net": cum, "committed": committed,
                       "exposure": nav + unfunded})
    total_calls = sum(r["calls"] for r in yearly)
    total_dist = sum(r["distributions"] for r in yearly)
    return {
        "yearly": yearly,
        "peak_net_outflow": -peak,
        "peak_year": peak_year,
        "breakeven_year": breakeven,
        "total_called": total_calls,
        "total_distributed": total_dist,
        "final_nav": yearly[-1]["nav"] if yearly else 0.0,
        "multiple": (total_dist + (yearly[-1]["nav"] if yearly else 0.0)) / total_calls if total_calls > 0 else None,
        "inputs": {"commitment": commitment, "life_years": life, "call_rates": rates, "bow": bow,
                   "growth": growth, "yield": yld, "vintages": vintages, "years": years},
        "method": "Takahashi & Alexander (2002) model; projections from your assumptions, not a forecast.",
    }


# ── Book ─────────────────────────────────────────────────────────────────────

def book(funds_in):
    if not funds_in:
        raise InputError("The book is empty: add a fund and its cash flows.")
    funds, by_ccy = [], {}
    for f in funds_in:
        flows, ignored = parse_flows(f.get("flows"))
        commitment = float(f.get("commitment", 0) or 0)
        row = {"id": f.get("id", ""), "name": f.get("name", ""), "strategy": f.get("strategy", "") or "other",
               "vintage": int(f.get("vintage", 0) or 0), "currency": (f.get("currency") or "USD").upper(),
               "commitment": commitment, "ignored_rows": ignored}
        if flows:
            m = fund_metrics(flows, commitment if commitment > 0 else None)
            row.update({k: m.get(k) for k in ("irr", "tvpi", "dpi", "rvpi", "paid_in", "distributed", "nav",
                                               "valuation_date", "unfunded", "paid_in_pct")})
            if row.get("unfunded") is None:
                row["unfunded"] = None
            st, fs = valuation_state(flows)
            ds, am = irr_flows(fs, st)
            g = by_ccy.setdefault(row["currency"], {"dates": [], "amounts": [], "paid_in": 0.0, "distributed": 0.0,
                                                   "nav": 0.0, "commitment": 0.0, "unfunded": 0.0, "funds": 0})
            g["dates"] += ds
            g["amounts"] += am
            g["paid_in"] += m["paid_in"]
            g["distributed"] += m["distributed"]
            g["nav"] += m["nav"] or 0.0
            g["commitment"] += commitment
            g["unfunded"] += m.get("unfunded") or 0.0
            g["funds"] += 1
        else:
            row.update({"irr": None, "tvpi": None, "dpi": None, "paid_in": 0.0, "distributed": 0.0, "nav": None,
                        "unfunded": commitment if commitment > 0 else None})
            g = by_ccy.setdefault(row["currency"], {"dates": [], "amounts": [], "paid_in": 0.0, "distributed": 0.0,
                                                   "nav": 0.0, "commitment": 0.0, "unfunded": 0.0, "funds": 0})
            g["commitment"] += commitment
            g["unfunded"] += commitment
            g["funds"] += 1
        funds.append(row)

    pooled = []
    for ccy, g in sorted(by_ccy.items()):
        rate, note = xirr(g["dates"], g["amounts"]) if g["dates"] else (None, "")
        pooled.append({"currency": ccy, "funds": g["funds"], "irr": rate, "irr_note": note,
                       "tvpi": _ratio(g["distributed"] + g["nav"], g["paid_in"]),
                       "dpi": _ratio(g["distributed"], g["paid_in"]), "paid_in": g["paid_in"],
                       "distributed": g["distributed"], "nav": g["nav"], "commitment": g["commitment"],
                       "unfunded": g["unfunded"]})

    def group(key):
        out = {}
        for r in funds:
            k = str(r.get(key) or "-")
            e = out.setdefault((r["currency"], k), {"currency": r["currency"], key: k, "funds": 0, "commitment": 0.0,
                                                    "nav": 0.0, "paid_in": 0.0, "distributed": 0.0})
            e["funds"] += 1
            e["commitment"] += r["commitment"]
            e["nav"] += r.get("nav") or 0.0
            e["paid_in"] += r.get("paid_in") or 0.0
            e["distributed"] += r.get("distributed") or 0.0
        rows = list(out.values())
        for e in rows:
            e["tvpi"] = _ratio(e["distributed"] + e["nav"], e["paid_in"])
        return sorted(rows, key=lambda e: (e["currency"], str(e[key])))

    notes = []
    if len(by_ccy) > 1:
        notes.append("Funds in %d currencies: pooled figures are per currency (no FX conversion)." % len(by_ccy))
    return {"funds": funds, "pooled": pooled, "by_strategy": group("strategy"), "by_vintage": group("vintage"),
            "notes": notes}


# ── Entry point ──────────────────────────────────────────────────────────────

def run(command, inp):
    if command in ("metrics", "timeline", "pme"):
        flows, ignored = parse_flows(inp.get("flows"))
        if not flows:
            raise InputError("Add the fund's cash flows: dated capital calls, distributions and NAV statements.")
        commitment = inp.get("commitment")
        commitment = float(commitment) if commitment not in (None, "") else None
        if command == "metrics":
            data = fund_metrics(flows, commitment)
        elif command == "timeline":
            data = timeline(flows, commitment)
        else:
            data = pme(flows, inp.get("index") or "SPY", inp.get("prices"), commitment)
        if ignored:
            data["ignored_rows"] = ignored
        return data
    if command == "pacing":
        return pacing(inp)
    if command == "book":
        return book(inp.get("funds") or [])
    raise InputError("Unknown command '%s' (metrics, timeline, pme, pacing, book)." % command)


def _emit(obj):
    if fincept_json is not None:
        print(fincept_json.dumps(obj))
    else:  # pragma: no cover
        print(json.dumps(obj, default=str))


def main(argv):
    if not argv:
        _emit({"success": False, "error": "Usage: private_markets.py <metrics|timeline|pme|pacing|book> --stdin"})
        return 1
    command = argv[0]
    try:
        if len(argv) > 1 and argv[1] == "--stdin":
            raw = sys.stdin.read()
        elif len(argv) > 1:
            raw = argv[1]
        else:
            raw = "{}"
        inp = json.loads(raw or "{}")
        if not isinstance(inp, dict):
            raise InputError("The input must be a JSON object.")
        data = run(command, inp)
        _emit({"success": True, "command": command, "data": data})
        return 0
    except InputError as e:
        _emit({"success": False, "error": str(e)})
        return 0
    except json.JSONDecodeError as e:
        _emit({"success": False, "error": "The input is not valid JSON (%s)." % e.msg})
        return 0
    except Exception as e:  # never a traceback on stdout
        _emit({"success": False, "error": "%s: %s" % (type(e).__name__, str(e)[:300])})
        return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
