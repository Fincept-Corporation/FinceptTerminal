"""Accretion / dilution engine - one transparent model for the headline, the EPS bridge, the 3-year view and the
sensitivity grids of the M&A tab, so every number on the Accretion/Dilution view reconciles with the others.

Model (year y = 1..3, all amounts in the user's currency, rates as fractions):
    new_debt         = cash consideration - acquirer cash used
    pre-tax effects  = synergies x phase_y - integration_y - amortisation - new_debt x cost_of_debt
                       - cash_used x cash_yield
    pro forma NI_y   = acquirer NI_y + target NI_y + pre-tax effects x (1 - tax rate)
    new shares       = stock consideration / acquirer share price
    pro forma EPS_y  = pro forma NI_y / (acquirer shares + new shares)
    accretion_y      = pro forma EPS_y / (acquirer NI_y / acquirer shares) - 1
NI_y grows from the supplied net income at the supplied earnings growth (0 = flat). Pre-tax effects are taxed at the
supplied rate whatever their sign (the usual simplification: a loss is assumed to shield other income).

Nothing is assumed silently: a figure the model needs and the caller did not give is reported in `missing_inputs`
(and the run fails with that message); every rule used is listed under `assumptions`.
"""
from typing import Any, Dict, List, Optional, Tuple

_YEARS = 3


def _num(d: Dict[str, Any], *keys, default=None) -> Optional[float]:
    for k in keys:
        v = d.get(k)
        if isinstance(v, (int, float)) and not isinstance(v, bool):
            return float(v)
    return default


class AccretionInputError(ValueError):
    def __init__(self, message: str, missing: List[str]):
        super().__init__(message)
        self.missing = missing


def normalize(p: Dict[str, Any]) -> Dict[str, Any]:
    """Service parameters (panel / MCP shapes) -> the engine's inputs. Raises AccretionInputError for gaps."""
    acq = p.get("acquirer") if isinstance(p.get("acquirer"), dict) else {}
    tgt = p.get("target") if isinstance(p.get("target"), dict) else {}
    missing: List[str] = []

    acq_ni = _num(acq, "net_income", default=_num(p, "acquirer_net_income"))
    acq_shares = _num(acq, "shares", "shares_outstanding", default=_num(p, "acquirer_shares"))
    tgt_ni = _num(tgt, "net_income", default=_num(p, "target_net_income"))
    price = _num(p, "deal_value", "purchase_price")
    if acq_ni is None:
        missing.append("acquirer_net_income")
    if not acq_shares or acq_shares <= 0:
        missing.append("acquirer_shares")
    if tgt_ni is None:
        missing.append("target_net_income")
    if not price or price <= 0:
        missing.append("deal_value")
    cash_pct = _num(p, "cash_pct", "cash_percentage", default=None)
    if cash_pct is None:
        missing.append("cash_pct")
        cash_pct = 0.0
    if cash_pct > 1.0:
        cash_pct /= 100.0
    cash_pct = min(max(cash_pct, 0.0), 1.0)
    acq_price = _num(acq, "share_price", "stock_price", default=_num(p, "acquirer_share_price", "acquirer_stock_price"))
    if cash_pct < 1.0 and (not acq_price or acq_price <= 0):
        missing.append("acquirer_share_price")
    if missing:
        labels = {
            "acquirer_net_income": "the acquirer's net income",
            "acquirer_shares": "the acquirer's shares outstanding",
            "target_net_income": "the target's net income",
            "deal_value": "the purchase price (equity value)",
            "cash_pct": "the cash share of the consideration",
            "acquirer_share_price": "the acquirer's share price (needed to count the new shares)",
        }
        raise AccretionInputError("Enter " + ", ".join(labels.get(m, m) for m in missing) + ".", missing)

    phase = p.get("synergy_phase")
    if not isinstance(phase, list) or not phase:
        phase = [_num(p, "synergy_phase_y1", default=1.0), _num(p, "synergy_phase_y2", default=1.0),
                 _num(p, "synergy_phase_y3", default=1.0)]
    phase = [min(max(float(x) / (100.0 if float(x) > 1.0 else 1.0), 0.0), 1.0) for x in phase[:_YEARS]]
    while len(phase) < _YEARS:
        phase.append(phase[-1] if phase else 1.0)
    integ = p.get("integration_costs_by_year")
    if not isinstance(integ, list):
        integ = [_num(p, "integration_costs_y1", "integration_costs", default=0.0),
                 _num(p, "integration_costs_y2", default=0.0), _num(p, "integration_costs_y3", default=0.0)]
    integ = [float(x or 0.0) for x in integ[:_YEARS]] + [0.0] * max(0, _YEARS - len(integ))

    cash_cons = price * cash_pct
    cash_used = _num(p, "cash_from_balance_sheet", "acquirer_cash_used", default=0.0)
    cash_used = min(max(cash_used, 0.0), cash_cons)
    return {
        "acq_ni": acq_ni, "acq_shares": acq_shares, "acq_price": acq_price or 0.0, "tgt_ni": tgt_ni,
        "acq_revenue": _num(acq, "revenue"), "tgt_revenue": _num(tgt, "revenue"),
        "price": price, "premium": _num(p, "premium"), "cash_pct": cash_pct,
        "cash_used": cash_used,
        "cost_of_debt": _num(p, "cost_of_debt", "debt_interest_rate", default=0.0),
        "cash_yield": _num(p, "cash_yield", "interest_on_cash", default=0.0),
        "tax_rate": min(max(_num(p, "tax_rate", default=0.0), 0.0), 0.99),
        "synergies": _num(p, "synergies", "run_rate_synergies", default=0.0),
        "phase": phase, "integration": integ,
        "amortization": _num(p, "amortization", "intangibles_amortization", default=0.0),
        "acq_growth": _num(p, "acquirer_ni_growth", "earnings_growth", default=0.0),
        "tgt_growth": _num(p, "target_ni_growth", "earnings_growth", default=0.0),
    }


def _year(x: Dict[str, Any], y: int, price: float, cash_pct: float, synergies: float) -> Dict[str, float]:
    """Year y (1-based) of the deal at `price`, `cash_pct`, run-rate `synergies`."""
    t = x["tax_rate"]
    acq_ni = x["acq_ni"] * (1.0 + x["acq_growth"]) ** (y - 1)
    tgt_ni = x["tgt_ni"] * (1.0 + x["tgt_growth"]) ** (y - 1)
    cash_cons = price * cash_pct
    cash_used = min(x["cash_used"], cash_cons)
    new_debt = cash_cons - cash_used
    stock = price - cash_cons
    new_shares = stock / x["acq_price"] if stock > 0 else 0.0
    syn = synergies * x["phase"][y - 1]
    integ = x["integration"][y - 1]
    amort = x["amortization"]
    interest = new_debt * x["cost_of_debt"]
    forgone = cash_used * x["cash_yield"]
    pretax = syn - integ - amort - interest - forgone
    pf_ni = acq_ni + tgt_ni + pretax * (1.0 - t)
    shares = x["acq_shares"]
    pf_shares = shares + new_shares
    sa_eps = acq_ni / shares
    pf_eps = pf_ni / pf_shares
    acc = (pf_eps / sa_eps - 1.0) * 100.0 if sa_eps > 0 else None
    return {
        "year": y, "acquirer_net_income": acq_ni, "target_net_income": tgt_ni, "synergies_realized": syn,
        "integration_costs": integ, "amortization": amort, "interest_on_new_debt": interest,
        "interest_forgone_on_cash": forgone, "pro_forma_net_income": pf_ni, "new_shares_issued": new_shares,
        "pro_forma_shares": pf_shares, "standalone_eps": sa_eps, "pro_forma_eps": pf_eps,
        "eps_change": pf_eps - sa_eps, "accretion_dilution_pct": acc,
        "new_debt": new_debt, "cash_used": cash_used, "stock_consideration": stock, "cash_consideration": cash_cons,
    }


def eps_bridge(x: Dict[str, Any], yr: Dict[str, float]) -> List[Dict[str, Any]]:
    """Standalone EPS -> pro forma EPS, each step in EPS on the acquirer's standalone share count; the last step
    is the effect of the new shares. The steps add up exactly to the pro forma EPS."""
    s = x["acq_shares"]
    k = 1.0 - x["tax_rate"]
    steps = [
        ("Acquirer standalone EPS", yr["standalone_eps"], "total"),
        ("Target net income", yr["target_net_income"] / s, "delta"),
        ("Synergies (after tax)", yr["synergies_realized"] * k / s, "delta"),
        ("Integration costs (after tax)", -yr["integration_costs"] * k / s, "delta"),
        ("Intangibles amortisation (after tax)", -yr["amortization"] * k / s, "delta"),
        ("Interest on new debt (after tax)", -yr["interest_on_new_debt"] * k / s, "delta"),
        ("Interest forgone on cash (after tax)", -yr["interest_forgone_on_cash"] * k / s, "delta"),
        ("New shares issued", yr["pro_forma_net_income"] / yr["pro_forma_shares"] - yr["pro_forma_net_income"] / s,
         "delta"),
        ("Pro forma EPS", yr["pro_forma_eps"], "total"),
    ]
    return [{"step": n, "value": v, "kind": kind} for n, v, kind in steps]


def breakeven_synergies(x: Dict[str, Any], price: float, cash_pct: float) -> Optional[float]:
    """Run-rate pre-tax synergies at which year-1 pro forma EPS equals standalone EPS (closed form).
    None when year 1 realises no synergies or the tax rate is 100%."""
    ph = x["phase"][0]
    k = 1.0 - x["tax_rate"]
    if ph <= 0 or k <= 0:
        return None
    base = _year(x, 1, price, cash_pct, 0.0)
    need_ni = base["standalone_eps"] * base["pro_forma_shares"]
    gap_after_tax = need_ni - base["pro_forma_net_income"]
    return max(gap_after_tax / k / ph, 0.0)


def max_premium_eps_neutral(x: Dict[str, Any]) -> Optional[float]:
    """Premium (percent) at which year-1 EPS is unchanged, holding the cash mix and synergies; bisection on the
    premium between -50% and +300%. None without a premium input or when the sign does not change."""
    if x["premium"] is None:
        return None
    unaffected = x["price"] / (1.0 + x["premium"])

    def acc(prem: float) -> float:
        r = _year(x, 1, unaffected * (1.0 + prem), x["cash_pct"], x["synergies"])
        return r["eps_change"]

    lo, hi = -0.5, 3.0
    f_lo, f_hi = acc(lo), acc(hi)
    if f_lo * f_hi > 0:
        return None
    for _ in range(80):
        mid = (lo + hi) / 2.0
        f_mid = acc(mid)
        if f_lo * f_mid <= 0:
            hi, f_hi = mid, f_mid
        else:
            lo, f_lo = mid, f_mid
    return (lo + hi) / 2.0 * 100.0


def _axis_index(values: List[float], base: float) -> int:
    return min(range(len(values)), key=lambda i: abs(values[i] - base)) if values else -1


def sensitivity(x: Dict[str, Any], steps: int = 5) -> Dict[str, Any]:
    """Year-1 accretion over premium (or purchase price) x run-rate synergies, and premium x cash mix."""
    steps = max(3, min(int(steps), 9))
    half = (steps - 1) // 2
    if x["premium"] is not None:
        base_prem = x["premium"]
        unaffected = x["price"] / (1.0 + base_prem)
        prems = [max(base_prem + (i - half) * 0.10, 0.0) for i in range(steps)]
        prems = sorted(set(round(v, 6) for v in prems))
        prices = [unaffected * (1.0 + v) for v in prems]
        row_values = [v * 100.0 for v in prems]
        row_axis = "premium_pct"
        base_row = _axis_index(prems, base_prem)
    else:
        moves = [(i - half) * 0.10 for i in range(steps)]
        prices = [x["price"] * (1.0 + m) for m in moves]
        row_values = prices
        row_axis = "purchase_price"
        base_row = half
    syn0 = x["synergies"]
    if syn0 > 0:
        syn_values = [syn0 * f for f in (0.0, 0.5, 1.0, 1.5, 2.0)]
        base_col = 2
    else:
        be = breakeven_synergies(x, x["price"], x["cash_pct"]) or 0.0
        syn_values = [be * f for f in (0.0, 0.5, 1.0, 1.5, 2.0)] if be > 0 else [0.0]
        base_col = 0
    cash_values = sorted(set([0.0, 0.25, 0.5, 0.75, 1.0, round(x["cash_pct"], 6)]))
    if x["acq_price"] <= 0:
        cash_values = [1.0]  # no share price: only an all-cash deal can be counted
    base_cash = _axis_index(cash_values, x["cash_pct"])

    acc_syn, eps_syn, acc_cash = [], [], []
    for price in prices:
        acc_syn.append([_year(x, 1, price, x["cash_pct"], s)["accretion_dilution_pct"] for s in syn_values])
        eps_syn.append([_year(x, 1, price, x["cash_pct"], s)["pro_forma_eps"] for s in syn_values])
        acc_cash.append([_year(x, 1, price, c, syn0)["accretion_dilution_pct"] for c in cash_values])
    return {
        "row_axis": row_axis,
        "row_values": row_values,
        "synergy_values": syn_values,
        "cash_pct_values": [c * 100.0 for c in cash_values],
        "accretion_premium_synergies": acc_syn,
        "eps_premium_synergies": eps_syn,
        "accretion_premium_cash": acc_cash,
        "base_row": base_row,
        "base_col": base_col,
        "base_col_cash": base_cash,
    }


def analyze(p: Dict[str, Any]) -> Dict[str, Any]:
    x = normalize(p)
    years = [_year(x, y, x["price"], x["cash_pct"], x["synergies"]) for y in range(1, _YEARS + 1)]
    y1 = years[0]
    be = breakeven_synergies(x, x["price"], x["cash_pct"])
    acc = y1["accretion_dilution_pct"]
    status = "Accretive" if y1["eps_change"] > 0 else ("Dilutive" if y1["eps_change"] < 0 else "Neutral")
    assumptions = [
        "Year 1 = the supplied net income; years 2-3 grow it at the supplied earnings growth (0 = flat).",
        "Cash consideration is paid first from the acquirer cash entered, the rest with new debt at the cost of debt.",
        "Stock consideration is issued at the acquirer share price entered.",
        "Synergies, integration costs, amortisation and interest are taxed at the tax rate entered.",
    ]
    out = {
        "accretion_dilution": {
            "standalone_eps": y1["standalone_eps"],
            "pro_forma_eps": y1["pro_forma_eps"],
            "eps_change": y1["eps_change"],
            "accretion_dilution_pct": acc,
            "is_accretive": y1["eps_change"] > 0,
            "status": status,
        },
        "deal_overview": {
            "purchase_price": x["price"],
            "cash_consideration": y1["cash_consideration"],
            "stock_consideration": y1["stock_consideration"],
            "acquirer_cash_used": y1["cash_used"],
            "new_debt": y1["new_debt"],
            "new_shares_issued": y1["new_shares_issued"],
            "pro_forma_shares": y1["pro_forma_shares"],
            "acquirer_ownership_pct": x["acq_shares"] / y1["pro_forma_shares"] * 100.0,
            "purchase_pe": (x["price"] / x["tgt_ni"]) if x["tgt_ni"] > 0 else None,
            "acquirer_pe": (x["acq_price"] / y1["standalone_eps"]) if (x["acq_price"] > 0 and y1["standalone_eps"] > 0)
            else None,
        },
        "eps_bridge": eps_bridge(x, y1),
        "years": years,
        "breakeven_synergies": be,
        "max_premium_eps_neutral_pct": max_premium_eps_neutral(x),
        "sensitivity": sensitivity(x, int(_num(p, "sensitivity_steps", default=5))),
        "assumptions": assumptions,
    }
    if acc is None:
        out["notes"] = ["The acquirer's standalone EPS is zero or negative, so accretion is shown as the EPS change "
                        "only."]
    if be is None:
        out.setdefault("notes", []).append("Breakeven synergies need some synergies realised in year 1.")
    return out
