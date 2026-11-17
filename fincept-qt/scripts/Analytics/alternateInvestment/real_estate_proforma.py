"""Real-estate pro forma and listed-REIT comparison.

CLI (the terminal):  python real_estate_proforma.py <command> --stdin   (input JSON object on stdin)
CLI (by hand):       python real_estate_proforma.py <command> '<json>'
                     python real_estate_proforma.py --selftest
Output: {"success": true, "data": {...}} or {"success": false, "error": "..."}. Rates are fractions.

proforma - an annual property model held `hold_years` and sold at the end of the last year:
  income    gross_rent (year-1 potential rent, annual), other_income, rent_growth, vacancy (share of potential
            rent + other income)
  expenses  opex (year-1 operating expenses excl. management), opex_growth, mgmt_fee (share of effective gross
            income), capex_reserve (year-1, grows with opex_growth)
  purchase  price, closing_costs (share of price)
  financing loan_amount OR ltv (share of price; loan_amount wins when > 0), interest_rate, amort_years
            (0 = interest only), io_years (interest-only years before amortisation), loan_fees (share of loan)
  exit      exit_cap (applied to the NOI of the year after the sale - the buyer's first year), selling_costs
  sensitivity  exit_cap_step (default 0.0025), rent_growth_step (default 0.01), steps (odd, default 5)
  Debt service is monthly (standard annuity: P = L r / (1 - (1 + r)^-n), r = rate / 12); years are summed.
  Returns: yearly table, going-in cap, unlevered / levered IRR (annual periods), equity multiple, DSCR by year
  and minimum, LTV, debt yield, cash-on-cash year 1, exit proceeds, and the exit cap x rent growth matrix of
  levered IRR (same model, one call) with the base cell's indexes.

reit_compare - {tickers: [...] (<= 10), ffo_per_share: {TICKER: number}} -> one row per ticker from Yahoo Finance
  (yfinance): price, dividend yield (annual dividend rate / price), market cap, debt / equity (total debt / book
  equity), price / book, 1-year total return (adjusted close, dividends reinvested). P/FFO only for tickers
  whose FFO per share you entered - FFO is not in Yahoo's data and is never estimated here.
"""

import json
import math
import os
import sys
import time

_HERE = os.path.dirname(os.path.abspath(__file__))
_SCRIPTS_DIR = os.path.abspath(os.path.join(_HERE, "..", ".."))
for _p in (_HERE, _SCRIPTS_DIR):
    if _p not in sys.path:
        sys.path.append(_p)

try:
    import fincept_json
except ImportError:  # pragma: no cover
    fincept_json = None

from private_markets import InputError, periodic_irr, quiet_stdout  # noqa: E402

MAX_REITS = 10


# ── Loan ─────────────────────────────────────────────────────────────────────

def monthly_payment(principal, annual_rate, years):
    """Standard annuity payment per month; interest only when years <= 0."""
    if principal <= 0:
        return 0.0
    r = annual_rate / 12.0
    if years <= 0:
        return principal * r
    n = int(round(years * 12))
    if r == 0:
        return principal / n
    return principal * r / (1.0 - (1.0 + r) ** (-n))


def loan_schedule(principal, annual_rate, amort_years, io_years, years):
    """Per year: (debt service, interest, principal repaid, balance at year end)."""
    rows = []
    bal = principal
    r = annual_rate / 12.0
    amort_pay = None
    for y in range(1, years + 1):
        ds = interest = repaid = 0.0
        for m in range(12):
            month = (y - 1) * 12 + m
            i = bal * r
            if bal <= 0:
                pay = 0.0
            elif month < io_years * 12 or amort_years <= 0:
                pay = i
            else:
                if amort_pay is None:
                    amort_pay = monthly_payment(bal, annual_rate, amort_years)
                pay = min(amort_pay, bal + i)
            prin = pay - i
            bal = max(0.0, bal - prin)
            ds += pay
            interest += i
            repaid += prin
        rows.append((ds, interest, repaid, bal))
    return rows


# ── Pro forma ────────────────────────────────────────────────────────────────

def _f(inp, key, default=0.0):
    v = inp.get(key, default)
    if v in (None, ""):
        return default
    try:
        x = float(v)
    except (TypeError, ValueError):
        raise InputError("'%s' must be a number." % key)
    if not math.isfinite(x):
        raise InputError("'%s' must be a finite number." % key)
    return x


def _model(p, rent_growth=None, exit_cap=None, with_table=True):
    g = p["rent_growth"] if rent_growth is None else rent_growth
    cap = p["exit_cap"] if exit_cap is None else exit_cap
    H = p["hold_years"]
    price = p["price"]
    loan = p["loan"]
    years = []
    sched = loan_schedule(loan, p["interest_rate"], p["amort_years"], p["io_years"], H)
    noi_list = []
    for t in range(1, H + 2):  # year H+1 only for the exit NOI
        gpr = p["gross_rent"] * (1.0 + g) ** (t - 1)
        other = p["other_income"] * (1.0 + g) ** (t - 1)
        vac = p["vacancy"] * (gpr + other)
        egi = gpr + other - vac
        opex = p["opex"] * (1.0 + p["opex_growth"]) ** (t - 1)
        mgmt = p["mgmt_fee"] * egi
        noi = egi - opex - mgmt
        capex = p["capex_reserve"] * (1.0 + p["opex_growth"]) ** (t - 1)
        noi_list.append(noi)
        if t <= H:
            ds, interest, repaid, bal = sched[t - 1]
            row = {"year": t, "potential_rent": gpr, "other_income": other, "vacancy": vac, "egi": egi,
                   "opex": opex, "mgmt_fee": mgmt, "noi": noi, "capex": capex, "cf_before_debt": noi - capex,
                   "debt_service": ds, "interest": interest, "principal": repaid, "cfads": noi - capex - ds,
                   "loan_balance": bal, "dscr": (noi / ds) if ds > 0 else None}
            years.append(row)
    exit_noi = noi_list[H]  # year H+1
    if cap <= 0:
        raise InputError("The exit cap rate must be above 0.")
    sale_price = exit_noi / cap
    sale_costs = sale_price * p["selling_costs"]
    net_sale = sale_price - sale_costs
    payoff = years[-1]["loan_balance"]
    equity = price * (1.0 + p["closing_costs"]) + loan * p["loan_fees"] - loan
    unlev = [-(price * (1.0 + p["closing_costs"]))] + [r["cf_before_debt"] for r in years]
    unlev[-1] += net_sale
    lev = [-equity] + [r["cfads"] for r in years]
    lev[-1] += net_sale - payoff
    return {"years": years, "sale_price": sale_price, "sale_costs": sale_costs, "net_sale": net_sale,
            "loan_payoff": payoff, "exit_noi": exit_noi, "equity": equity, "unlevered_flows": unlev,
            "levered_flows": lev}


def _params(inp):
    p = {
        "price": _f(inp, "price", 0.0),
        "closing_costs": _f(inp, "closing_costs", 0.02),
        "gross_rent": _f(inp, "gross_rent", 0.0),
        "other_income": _f(inp, "other_income", 0.0),
        "rent_growth": _f(inp, "rent_growth", 0.03),
        "vacancy": _f(inp, "vacancy", 0.05),
        "opex": _f(inp, "opex", 0.0),
        "opex_growth": _f(inp, "opex_growth", 0.03),
        "mgmt_fee": _f(inp, "mgmt_fee", 0.0),
        "capex_reserve": _f(inp, "capex_reserve", 0.0),
        "interest_rate": _f(inp, "interest_rate", 0.0),
        "amort_years": int(_f(inp, "amort_years", 30)),
        "io_years": int(_f(inp, "io_years", 0)),
        "loan_fees": _f(inp, "loan_fees", 0.0),
        "hold_years": int(_f(inp, "hold_years", 10)),
        "exit_cap": _f(inp, "exit_cap", 0.06),
        "selling_costs": _f(inp, "selling_costs", 0.02),
    }
    if p["price"] <= 0:
        raise InputError("Enter the purchase price (a positive amount).")
    if p["gross_rent"] <= 0:
        raise InputError("Enter the year-1 gross potential rent (a positive annual amount).")
    if not (1 <= p["hold_years"] <= 30):
        raise InputError("The hold period must be between 1 and 30 years.")
    for k in ("vacancy", "mgmt_fee", "closing_costs", "selling_costs", "loan_fees"):
        if not (0 <= p[k] < 1):
            raise InputError("'%s' must be a share between 0 and 100 %%." % k)
    if p["amort_years"] < 0 or p["io_years"] < 0:
        raise InputError("Amortisation and interest-only years cannot be negative.")
    loan_amount = _f(inp, "loan_amount", 0.0)
    ltv = _f(inp, "ltv", 0.0)
    if loan_amount > 0:
        p["loan"] = loan_amount
    else:
        if not (0 <= ltv < 1):
            raise InputError("LTV must be a share between 0 and 100 %.")
        p["loan"] = ltv * p["price"]
    if p["loan"] >= p["price"] * (1 + p["closing_costs"]):
        raise InputError("The loan covers the whole purchase: there is no equity to earn a return on.")
    return p


def proforma(inp):
    p = _params(inp)
    m = _model(p)
    years = m["years"]
    unlev_irr, u_note = periodic_irr(m["unlevered_flows"])
    lev_irr, l_note = periodic_irr(m["levered_flows"])
    inflows = sum(x for x in m["levered_flows"][1:] if x > 0)
    equity = m["equity"]
    dscrs = [r["dscr"] for r in years if r["dscr"] is not None]
    noi1 = years[0]["noi"]
    notes = [n for n in (u_note, l_note) if n]
    if any(r["cfads"] < 0 for r in years):
        notes.append("Cash flow after debt service is negative in %d year(s): equity must fund the shortfall."
                     % sum(1 for r in years if r["cfads"] < 0))

    # Sensitivity: exit cap (rows) x rent growth (cols) -> levered IRR.
    steps = int(_f(inp, "steps", 5))
    steps = max(3, min(steps if steps % 2 == 1 else steps + 1, 9))
    cap_step = _f(inp, "exit_cap_step", 0.0025)
    g_step = _f(inp, "rent_growth_step", 0.01)
    half = (steps - 1) // 2
    caps = [p["exit_cap"] + (i - half) * cap_step for i in range(steps)]
    growths = [p["rent_growth"] + (j - half) * g_step for j in range(steps)]
    matrix, uml = [], []
    for c in caps:
        row, urow = [], []
        for gr in growths:
            if c <= 0:
                row.append(None)
                urow.append(None)
                continue
            mm = _model(p, rent_growth=gr, exit_cap=c)
            r_lev, _ = periodic_irr(mm["levered_flows"])
            r_unl, _ = periodic_irr(mm["unlevered_flows"])
            row.append(r_lev)
            urow.append(r_unl)
        matrix.append(row)
        uml.append(urow)

    return {
        "summary": {
            "going_in_cap": noi1 / p["price"],
            "unlevered_irr": unlev_irr,
            "levered_irr": lev_irr,
            "equity_multiple": inflows / equity if equity > 0 else None,
            "equity": equity,
            "loan": p["loan"],
            "ltv": p["loan"] / p["price"],
            "debt_yield": (noi1 / p["loan"]) if p["loan"] > 0 else None,
            "dscr_min": min(dscrs) if dscrs else None,
            "dscr_year1": years[0]["dscr"],
            "cash_on_cash_y1": years[0]["cfads"] / equity if equity > 0 else None,
            "noi_year1": noi1,
            "exit_noi": m["exit_noi"],
            "sale_price": m["sale_price"],
            "net_sale": m["net_sale"],
            "loan_payoff": m["loan_payoff"],
            "exit_equity": m["net_sale"] - m["loan_payoff"],
            "monthly_payment_amortising": monthly_payment(p["loan"], p["interest_rate"], p["amort_years"])
            if p["amort_years"] > 0 else None,
        },
        "years": years,
        "unlevered_flows": m["unlevered_flows"],
        "levered_flows": m["levered_flows"],
        "sensitivity": {"metric": "levered_irr", "row_title": "Exit cap rate", "col_title": "Rent growth",
                        "rows": caps, "cols": growths, "values": matrix, "unlevered_values": uml,
                        "base_row": half, "base_col": half},
        "notes": notes,
        "method": "Annual model; exit value = NOI of year %d / exit cap; debt service monthly, summed by year."
                  % (p["hold_years"] + 1),
    }


# ── REIT comparison ──────────────────────────────────────────────────────────

def _num(info, *keys):
    for k in keys:
        v = info.get(k)
        try:
            x = float(v)
        except (TypeError, ValueError):
            continue
        if math.isfinite(x) and x != 0:
            return x
    return None


def reit_row(symbol, ffo_per_share=None):
    import yfinance as yf
    t = yf.Ticker(symbol)
    try:
        with quiet_stdout():  # yfinance prints failures to stdout, which carries only the JSON reply
            info = t.info or {}
    except Exception as e:
        return {"ticker": symbol, "error": "No data from Yahoo Finance (%s)." % (str(e).splitlines()[0][:100] if str(e) else type(e).__name__)}
    price = _num(info, "currentPrice", "regularMarketPrice", "previousClose")
    if price is None:
        return {"ticker": symbol, "error": "Yahoo Finance has no price for %s." % symbol}
    row = {"ticker": symbol, "name": info.get("shortName") or info.get("longName") or symbol,
           "currency": info.get("currency") or "", "price": price,
           "quote_type": info.get("quoteType") or "", "industry": info.get("industry") or ""}
    div_rate = _num(info, "dividendRate", "trailingAnnualDividendRate")
    row["dividend_rate"] = div_rate
    row["dividend_yield"] = (div_rate / price) if div_rate else None
    row["market_cap"] = _num(info, "marketCap")
    debt = _num(info, "totalDebt")
    bvps = _num(info, "bookValue")
    shares = _num(info, "sharesOutstanding")
    book_equity = bvps * shares if (bvps and shares) else None
    row["total_debt"] = debt
    row["book_equity"] = book_equity
    row["debt_to_equity"] = (debt / book_equity) if (debt and book_equity and book_equity > 0) else None
    row["price_to_book"] = (price / bvps) if (bvps and bvps > 0) else None
    cash = _num(info, "totalCash")
    ebitda = _num(info, "ebitda")
    row["net_debt_to_ebitda"] = ((debt - (cash or 0.0)) / ebitda) if (debt and ebitda and ebitda > 0) else None
    try:
        with quiet_stdout():
            hist = t.history(period="1y", interval="1d", auto_adjust=True)
        closes = [float(c) for c in hist["Close"] if c == c and float(c) > 0] if hist is not None and len(hist) else []
    except Exception:
        closes = []
    row["total_return_1y"] = (closes[-1] / closes[0] - 1.0) if len(closes) >= 200 else None
    if ffo_per_share:
        row["ffo_per_share"] = ffo_per_share
        row["p_ffo"] = price / ffo_per_share if ffo_per_share > 0 else None
        row["ffo_yield"] = ffo_per_share / price
    missing = [k for k in ("dividend_yield", "market_cap", "debt_to_equity", "price_to_book", "total_return_1y")
               if row.get(k) is None]
    if missing:
        row["not_derivable"] = missing
    return row


def reit_compare(inp):
    raw = inp.get("tickers") or []
    if isinstance(raw, str):
        raw = [s for s in raw.replace(";", ",").split(",")]
    tickers = []
    for s in raw:
        s = str(s).strip().upper()
        if s and s not in tickers:
            tickers.append(s)
    if not tickers:
        raise InputError("Add at least one REIT ticker (for example O, PLD, SPG).")
    if len(tickers) > MAX_REITS:
        raise InputError("At most %d tickers per comparison." % MAX_REITS)
    ffo = {str(k).upper(): v for k, v in (inp.get("ffo_per_share") or {}).items()}
    try:
        import yfinance  # noqa: F401
    except ImportError:
        raise InputError("The yfinance package is missing, so REIT data cannot be loaded.")
    rows = []
    for i, s in enumerate(tickers):
        if i:
            time.sleep(0.3)  # polite to the free source
        f = ffo.get(s)
        try:
            f = float(f) if f not in (None, "") else None
        except (TypeError, ValueError):
            f = None
        rows.append(reit_row(s, f if f and f > 0 else None))
    ok = [r for r in rows if "error" not in r]
    if not ok:
        raise InputError("Yahoo Finance returned no data for %s. Check the tickers or try again later."
                         % ", ".join(tickers))
    notes = ["FFO / AFFO are not in Yahoo Finance's data: P/FFO is shown only where you entered FFO per share.",
             "Dividend yield = annual dividend rate / price; debt / equity = total debt / book equity; "
             "1-year total return from adjusted closes (dividends reinvested)."]
    return {"rows": rows, "source": "Yahoo Finance (yfinance)", "notes": notes}


# ── Self-test (hand-checked spreadsheet cases) ───────────────────────────────

def selftest():
    checks = []
    # 1. Annuity: 1,000,000 at 6 % over 30 years, monthly -> 5,995.51 (standard mortgage formula).
    pay = monthly_payment(1000000, 0.06, 30)
    checks.append(("annuity payment 1,000,000 @ 6% / 30y", round(pay, 2), 5995.51))
    # 2. All-cash flat property: NOI 60,000 a year, price 1,000,000, exit cap 6 % -> sale 1,000,000; IRR 6 %.
    base = {"price": 1000000, "closing_costs": 0, "gross_rent": 60000, "rent_growth": 0, "vacancy": 0, "opex": 0,
            "opex_growth": 0, "hold_years": 5, "exit_cap": 0.06, "selling_costs": 0, "ltv": 0}
    r = proforma(base)["summary"]
    checks.append(("unlevered IRR, flat 6% yield bought and sold at 6% cap", round(r["unlevered_irr"], 10), 0.06))
    # 3. Same with a 50 % interest-only loan at 4 %: CF 60,000 - 20,000 = 40,000 on 500,000 equity, exit equity
    #    1,000,000 - 500,000 -> levered IRR 8 % exactly; DSCR 3.0x; debt yield 12 %.
    lev = dict(base, ltv=0.5, interest_rate=0.04, amort_years=0)
    r = proforma(lev)["summary"]
    checks.append(("levered IRR, 50% IO loan @ 4%", round(r["levered_irr"], 10), 0.08))
    checks.append(("DSCR", round(r["dscr_min"], 10), 3.0))
    checks.append(("debt yield", round(r["debt_yield"], 10), 0.12))
    checks.append(("equity multiple 5y", round(r["equity_multiple"], 10), round((5 * 40000 + 500000) / 500000.0, 10)))
    # 4. Amortising balance after 12 payments of the 1,000,000 / 6 % / 30y loan: 987,719.88 (standard
    #    amortisation table: B_n = L(1+r)^n - P((1+r)^n - 1)/r).
    rr = 0.005
    bal_ref = 1000000 * (1 + rr) ** 12 - pay * ((1 + rr) ** 12 - 1) / rr
    sched = loan_schedule(1000000, 0.06, 30, 0, 1)
    checks.append(("balance after 1 year", round(sched[0][3], 2), round(bal_ref, 2)))
    failed = [c for c in checks if abs(c[1] - c[2]) > 1e-6]
    return {"checks": [{"name": n, "got": g, "expected": e, "ok": abs(g - e) <= 1e-6} for n, g, e in checks],
            "passed": not failed}


def run(command, inp):
    if command == "proforma":
        return proforma(inp)
    if command == "reit_compare":
        return reit_compare(inp)
    if command == "selftest":
        return selftest()
    raise InputError("Unknown command '%s' (proforma, reit_compare, selftest)." % command)


def _emit(obj):
    if fincept_json is not None:
        print(fincept_json.dumps(obj))
    else:  # pragma: no cover
        print(json.dumps(obj, default=str))


def main(argv):
    if argv and argv[0] == "--selftest":
        res = selftest()
        _emit({"success": res["passed"], "data": res})
        return 0 if res["passed"] else 1
    if not argv:
        _emit({"success": False, "error": "Usage: real_estate_proforma.py <proforma|reit_compare> --stdin"})
        return 1
    command = argv[0]
    try:
        raw = sys.stdin.read() if (len(argv) > 1 and argv[1] == "--stdin") else (argv[1] if len(argv) > 1 else "{}")
        inp = json.loads(raw or "{}")
        if not isinstance(inp, dict):
            raise InputError("The input must be a JSON object.")
        _emit({"success": True, "command": command, "data": run(command, inp)})
        return 0
    except InputError as e:
        _emit({"success": False, "error": str(e)})
        return 0
    except json.JSONDecodeError as e:
        _emit({"success": False, "error": "The input is not valid JSON (%s)." % e.msg})
        return 0
    except Exception as e:
        _emit({"success": False, "error": "%s: %s" % (type(e).__name__, str(e)[:300])})
        return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
