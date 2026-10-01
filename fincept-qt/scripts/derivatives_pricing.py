"""
Derivatives Pricing Bridge Script
==================================
CLI bridge for the QT terminal to invoke derivatives pricing calculations.
Wraps the Analytics/derivatives modules and returns JSON results.

Usage:
    python derivatives_pricing.py <command> [--param value ...]

Commands:
    bond_price          - Calculate bond price from YTM
    bond_ytm            - Calculate YTM from clean price
    option_price        - Black-Scholes option pricing + Greeks
    implied_vol         - Calculate implied volatility
    fx_option_price     - FX vanilla option pricing (Garman-Kohlhagen)
    swap_value          - Interest rate swap valuation
    cds_value           - Credit default swap valuation
    forward_price       - Forward/futures pricing
"""

import sys
import json
import argparse
import calendar
import math
from datetime import datetime, timedelta

try:
    import numpy as np
    from scipy.stats import norm
    from scipy.optimize import brentq
    HAS_SCIPY = True
except ImportError:
    HAS_SCIPY = False


def black_scholes_price(S, K, T, r, sigma, q=0.0, option_type="call"):
    """Black-Scholes-Merton option pricing"""
    if T <= 0:
        if option_type == "call":
            return max(0.0, S - K)
        else:
            return max(0.0, K - S)

    d1 = (math.log(S / K) + (r - q + 0.5 * sigma ** 2) * T) / (sigma * math.sqrt(T))
    d2 = d1 - sigma * math.sqrt(T)

    if option_type == "call":
        price = S * math.exp(-q * T) * norm.cdf(d1) - K * math.exp(-r * T) * norm.cdf(d2)
    else:
        price = K * math.exp(-r * T) * norm.cdf(-d2) - S * math.exp(-q * T) * norm.cdf(-d1)

    return price


def black_scholes_greeks(S, K, T, r, sigma, q=0.0, option_type="call"):
    """Calculate all Greeks for BSM model"""
    if T <= 0:
        return {"delta": 0, "gamma": 0, "theta": 0, "vega": 0, "rho": 0}

    d1 = (math.log(S / K) + (r - q + 0.5 * sigma ** 2) * T) / (sigma * math.sqrt(T))
    d2 = d1 - sigma * math.sqrt(T)

    sqrt_T = math.sqrt(T)
    pdf_d1 = norm.pdf(d1)
    exp_qT = math.exp(-q * T)
    exp_rT = math.exp(-r * T)

    # Gamma (same for call and put)
    gamma = (pdf_d1 * exp_qT) / (S * sigma * sqrt_T)

    # Vega (same for call and put) — per 1% move
    vega = S * exp_qT * pdf_d1 * sqrt_T * 0.01

    if option_type == "call":
        delta = exp_qT * norm.cdf(d1)
        theta = (-(S * pdf_d1 * sigma * exp_qT) / (2 * sqrt_T)
                 - r * K * exp_rT * norm.cdf(d2)
                 + q * S * exp_qT * norm.cdf(d1)) / 365.0
        rho = K * T * exp_rT * norm.cdf(d2) * 0.01
    else:
        delta = exp_qT * (norm.cdf(d1) - 1)
        theta = (-(S * pdf_d1 * sigma * exp_qT) / (2 * sqrt_T)
                 + r * K * exp_rT * norm.cdf(-d2)
                 - q * S * exp_qT * norm.cdf(-d1)) / 365.0
        rho = -K * T * exp_rT * norm.cdf(-d2) * 0.01

    return {
        "delta": round(delta, 6),
        "gamma": round(gamma, 6),
        "theta": round(theta, 6),
        "vega": round(vega, 6),
        "rho": round(rho, 6),
    }


def implied_volatility(S, K, T, r, market_price, q=0.0, option_type="call"):
    """Calculate implied volatility using Brent's method"""
    if T <= 0:
        return 0.0

    def objective(sigma):
        return black_scholes_price(S, K, T, r, sigma, q, option_type) - market_price

    try:
        iv = brentq(objective, 0.001, 10.0, xtol=1e-8, maxiter=200)
        return iv
    except (ValueError, RuntimeError):
        return -1.0


def _implied_vol_checked(S, K, T, r, market_price, q, option_type):
    """implied_volatility() with the failure reasons spelled out. Returns (iv, error)."""
    if S <= 0 or K <= 0:
        return None, "Spot and strike must be positive"
    if T <= 0:
        return None, "Time to expiry must be positive"
    if market_price <= 0:
        return None, "Market price must be positive"
    fwd_s = S * math.exp(-q * T)
    disc_k = K * math.exp(-r * T)
    if option_type == "call":
        lower, upper = max(fwd_s - disc_k, 0.0), fwd_s
    else:
        lower, upper = max(disc_k - fwd_s, 0.0), disc_k
    if market_price < lower - 1e-9:
        return None, ("Market price %.4f is below the option's no-arbitrage minimum (intrinsic value "
                      "%.4f) - no implied volatility exists" % (market_price, lower))
    if market_price > upper + 1e-9:
        return None, ("Market price %.4f is above the option's no-arbitrage maximum (%.4f) - "
                      "no implied volatility exists" % (market_price, upper))
    iv = implied_volatility(S, K, T, r, market_price, q, option_type)
    if iv < 0:
        return None, ("Could not converge on implied volatility - it lies outside the solver's "
                      "0.1% to 1000% range")
    return iv, None


def garman_kohlhagen_price(S, K, T, r_d, r_f, sigma, option_type="call", notional=1.0):
    """Garman-Kohlhagen FX option pricing"""
    if T <= 0:
        if option_type == "call":
            return max(0.0, S - K) * notional
        else:
            return max(0.0, K - S) * notional

    d1 = (math.log(S / K) + (r_d - r_f + 0.5 * sigma ** 2) * T) / (sigma * math.sqrt(T))
    d2 = d1 - sigma * math.sqrt(T)

    if option_type == "call":
        price = S * math.exp(-r_f * T) * norm.cdf(d1) - K * math.exp(-r_d * T) * norm.cdf(d2)
    else:
        price = K * math.exp(-r_d * T) * norm.cdf(-d2) - S * math.exp(-r_f * T) * norm.cdf(-d1)

    return price * notional


_VALID_FREQ = (1, 2, 3, 4, 6, 12)


def _add_months(d, months):
    """datetime + N calendar months (N may be negative); the day is clamped to the
    target month's length so 31-Aug + 6 months is 28/29-Feb, not an error."""
    idx = d.year * 12 + (d.month - 1) + months
    y, m = divmod(idx, 12)
    m += 1
    return d.replace(year=y, month=m, day=min(d.day, calendar.monthrange(y, m)[1]))


def _coupon_dates_after(settle, maturity, freq):
    """Coupon dates strictly after `settle`, ascending, built BACKWARD from maturity
    (the market convention), plus the last coupon date on or before `settle`."""
    step = 12 // freq
    k = 0
    after = []
    while True:
        d = _add_months(maturity, -k * step)
        if d <= settle:
            prev = d
            break
        after.append(d)
        k += 1
        if k > 4000:  # ~330 years of monthly coupons - a corrupt date, not a bond
            raise ValueError("Coupon schedule too long - check the dates")
    after.reverse()
    return prev, after


def _bond_core(issue_date, settlement_date, maturity_date, coupon_rate, ytm, freq):
    """Unrounded bond maths. Returns a dict of floats, or {"error": msg}.

    Cash flows are discounted at (w + k) coupon periods, where w is the fraction of
    the CURRENT coupon period still to run (actual days to the next coupon / days in
    the period) and k = 0..n-1 indexes the remaining coupons. The previous code used
    n = int(T * freq) whole periods from settlement, which dropped a coupon (and
    mis-timed every other cash flow) whenever settlement was not exactly on a coupon
    date - e.g. the default 2026-10-01 settlement against a 2029-01-01 maturity
    priced 4 coupons instead of 5.
    """
    if freq not in _VALID_FREQ:
        return {"error": "Payment frequency must be one of 1, 2, 3, 4, 6 or 12 per year"}
    coupon_dec = coupon_rate / 100.0
    y_per = (ytm / 100.0) / freq
    if 1.0 + y_per <= 0:
        return {"error": "Yield is too low to discount cash flows (YTM <= -100% per year)"}

    settle = datetime.strptime(settlement_date, "%Y-%m-%d")
    maturity = datetime.strptime(maturity_date, "%Y-%m-%d")
    issue = datetime.strptime(issue_date, "%Y-%m-%d")
    if settle >= maturity:
        return {"error": "Settlement must be before maturity"}

    prev, after = _coupon_dates_after(settle, maturity, freq)
    n = len(after)
    period_days = (after[0] - prev).days
    if period_days <= 0:
        return {"error": "Invalid coupon schedule"}
    w = (after[0] - settle).days / period_days

    coupon = coupon_dec / freq * 100.0  # per 100 face, per period
    pv = 0.0
    dur_num = 0.0
    conv_num = 0.0
    for k in range(n):
        periods = w + k
        t = periods / freq  # years from settlement
        cf = coupon + (100.0 if k == n - 1 else 0.0)
        df = (1.0 + y_per) ** (-periods)
        pv += cf * df
        dur_num += t * cf * df
        conv_num += t * (t + 1.0 / freq) * cf * df

    dirty = pv
    # Accrued interest runs from the last coupon (or the issue date, for a first
    # short coupon) to settlement, actual/actual within the period.
    accrual_start = max(prev, issue)
    accrued = coupon * (max((settle - accrual_start).days, 0) / period_days)
    clean = dirty - accrued

    mac = dur_num / dirty
    return {
        "clean_price": clean,
        "dirty_price": dirty,
        "accrued_interest": accrued,
        "duration": mac,
        "modified_duration": mac / (1.0 + y_per),
        "convexity": conv_num / (dirty * (1.0 + y_per) ** 2),
        "remaining_coupons": n,
        "next_coupon_date": after[0].strftime("%Y-%m-%d"),
    }


def bond_price_from_ytm(issue_date, settlement_date, maturity_date, coupon_rate, ytm, freq):
    """Calculate bond clean/dirty price from YTM"""
    res = _bond_core(issue_date, settlement_date, maturity_date, coupon_rate, ytm, freq)
    if "error" in res:
        return res
    return {
        "clean_price": round(res["clean_price"], 4),
        "dirty_price": round(res["dirty_price"], 4),
        "accrued_interest": round(res["accrued_interest"], 4),
        "duration": round(res["duration"], 4),
        "modified_duration": round(res["modified_duration"], 4),
        "convexity": round(res["convexity"], 4),
        "remaining_coupons": res["remaining_coupons"],
        "next_coupon_date": res["next_coupon_date"],
        "ytm": round(ytm, 4),
    }


def bond_ytm_from_price(issue_date, settlement_date, maturity_date, coupon_rate, clean_price, freq):
    """Calculate YTM (percent) from clean price using Brent's method"""
    first = _bond_core(issue_date, settlement_date, maturity_date, coupon_rate, 5.0, freq)
    if "error" in first:
        return first  # bad dates / frequency - say so instead of "could not converge"

    def price_at_ytm(y):
        res = _bond_core(issue_date, settlement_date, maturity_date, coupon_rate, y, freq)
        if "error" in res:
            return 999
        return res["clean_price"] - clean_price

    try:
        ytm_result = brentq(price_at_ytm, -5.0, 100.0, xtol=1e-10, maxiter=200)
        return {"ytm": round(ytm_result, 6)}
    except (ValueError, RuntimeError):
        return {"error": "Could not converge on YTM - the clean price is outside what any yield "
                         "between -5% and 100% can produce for this bond"}


def swap_value(effective_date, maturity_date, fixed_rate, freq, notional, discount_rate):
    """Simple interest rate swap valuation (fixed vs floating), flat continuously
    compounded discount curve. Value is to the payer of FIXED / receiver of floating."""
    fixed_rate_dec = fixed_rate / 100.0
    disc_rate_dec = discount_rate / 100.0

    if freq not in _VALID_FREQ:
        return {"error": "Payment frequency must be one of 1, 2, 3, 4, 6 or 12 per year"}
    eff = datetime.strptime(effective_date, "%Y-%m-%d")
    mat = datetime.strptime(maturity_date, "%Y-%m-%d")

    T = (mat - eff).days / 365.25
    if T <= 0:
        return {"error": "Maturity must be after effective date"}

    # Payment dates roll forward from the effective date; the last one is always
    # maturity (a short final period is kept, not dropped). The old n = int(T * freq)
    # lost a whole period whenever 365.25-day years made T*freq land just under an
    # integer - a 5-year semi-annual swap priced 9 payments instead of 10.
    step = 12 // freq
    dates = []
    k = 1
    while True:
        d = _add_months(eff, k * step)
        if d >= mat:
            break
        dates.append(d)
        k += 1
        if k > 4000:
            return {"error": "Payment schedule too long - check the dates"}
    dates.append(mat)

    fixed_pv = 0.0
    annuity = 0.0
    prev = eff
    for d in dates:
        dt = (d - prev).days / 365.25
        t_i = (d - eff).days / 365.25
        df = math.exp(-disc_rate_dec * t_i)
        fixed_pv += fixed_rate_dec * dt * notional * df
        annuity += dt * df
        prev = d

    # Floating leg PV (par at inception)
    float_pv = notional * (1 - math.exp(-disc_rate_dec * T))

    # Swap value = floating - fixed (receiver of floating / payer of fixed)
    swap_val = float_pv - fixed_pv

    # Par swap rate
    par_rate = (1 - math.exp(-disc_rate_dec * T)) / annuity if annuity > 0 else 0

    return {
        "swap_value": round(swap_val, 2),
        "fixed_leg_pv": round(fixed_pv, 2),
        "floating_leg_pv": round(float_pv, 2),
        "par_swap_rate": round(par_rate * 100, 4),
        "notional": notional,
        "tenor_years": round(T, 2),
        "payments": len(dates),
        "position": "payer of fixed / receiver of floating",
    }


def cds_value(valuation_date, maturity_date, recovery_rate, notional, spread_bps,
              risk_free_rate=5.0, coupon_bps=None):
    """Simple CDS valuation (credit-triangle hazard rate, flat risk-free rate).

    `spread_bps` is the MARKET spread - it sets the hazard rate. `coupon_bps` is the
    contractual premium actually paid; when omitted it equals the market spread (the
    old behaviour, which makes the upfront ~0 by construction). Value is to the
    protection BUYER: protection leg PV minus premium leg PV.
    """
    rec_rate = recovery_rate / 100.0
    spread = spread_bps / 10000.0
    coupon = (spread_bps if coupon_bps is None else coupon_bps) / 10000.0

    val = datetime.strptime(valuation_date, "%Y-%m-%d")
    mat = datetime.strptime(maturity_date, "%Y-%m-%d")

    T = (mat - val).days / 365.25
    if T <= 0:
        return {"error": "Maturity must be after valuation date"}

    # Simple hazard rate from spread: h = spread / (1 - R)
    lgd = 1 - rec_rate
    if lgd <= 0:
        return {"error": "LGD must be positive (recovery < 100%)"}

    hazard_rate = spread / lgd

    # Flat continuously-compounded risk-free rate (percent input, default 5%)
    r = risk_free_rate / 100.0

    # Premium leg PV (quarterly payments)
    premium_pv = 0.0
    n_quarters = max(1, int(T * 4))
    for i in range(1, n_quarters + 1):
        t_i = i * 0.25
        if t_i > T:
            break
        surv = math.exp(-hazard_rate * t_i)
        df = math.exp(-r * t_i)
        premium_pv += coupon * 0.25 * notional * surv * df

    # Protection leg PV
    protection_pv = 0.0
    n_steps = max(1, int(T * 12))
    dt = T / n_steps
    for i in range(1, n_steps + 1):
        t_i = i * dt
        surv_prev = math.exp(-hazard_rate * (t_i - dt))
        surv_curr = math.exp(-hazard_rate * t_i)
        default_prob = surv_prev - surv_curr
        df = math.exp(-r * t_i)
        protection_pv += lgd * notional * default_prob * df

    # Upfront value
    upfront = protection_pv - premium_pv

    # Breakeven spread
    annuity = 0.0
    for i in range(1, n_quarters + 1):
        t_i = i * 0.25
        if t_i > T:
            break
        surv = math.exp(-hazard_rate * t_i)
        df = math.exp(-r * t_i)
        annuity += 0.25 * notional * surv * df

    breakeven_spread = (protection_pv / annuity * 10000) if annuity > 0 else 0

    # Survival probability
    survival_prob = math.exp(-hazard_rate * T)

    return {
        "upfront_value": round(upfront, 2),
        "premium_leg_pv": round(premium_pv, 2),
        "protection_leg_pv": round(protection_pv, 2),
        "breakeven_spread_bps": round(breakeven_spread, 2),
        "hazard_rate": round(hazard_rate * 100, 4),
        "survival_probability": round(survival_prob * 100, 2),
        "notional": notional,
        "coupon_bps": round(coupon * 10000.0, 4),
        "risk_free_rate": round(risk_free_rate, 4),
    }


def forward_price(spot, r, T, q=0.0, storage=0.0, convenience=0.0):
    """Calculate forward/futures price using cost-of-carry model"""
    carry = r - q + storage - convenience
    fwd = spot * math.exp(carry * T)
    return {
        "forward_price": round(fwd, 4),
        "spot_price": spot,
        "carry_rate": round(carry * 100, 4),
        "time_to_expiry": T,
    }


def main():
    parser = argparse.ArgumentParser(description="Derivatives Pricing Engine")
    subparsers = parser.add_subparsers(dest="command")

    # Bond price
    bp = subparsers.add_parser("bond_price")
    bp.add_argument("--issue-date", required=True)
    bp.add_argument("--settlement-date", required=True)
    bp.add_argument("--maturity-date", required=True)
    bp.add_argument("--coupon-rate", type=float, required=True)
    bp.add_argument("--ytm", type=float, required=True)
    bp.add_argument("--freq", type=int, default=2)

    # Bond YTM
    by = subparsers.add_parser("bond_ytm")
    by.add_argument("--issue-date", required=True)
    by.add_argument("--settlement-date", required=True)
    by.add_argument("--maturity-date", required=True)
    by.add_argument("--coupon-rate", type=float, required=True)
    by.add_argument("--clean-price", type=float, required=True)
    by.add_argument("--freq", type=int, default=2)

    # Option price
    op = subparsers.add_parser("option_price")
    op.add_argument("--spot", type=float, required=True)
    op.add_argument("--strike", type=float, required=True)
    op.add_argument("--time", type=float, required=True, help="Time to expiry in years")
    op.add_argument("--rate", type=float, required=True, help="Risk-free rate as percentage")
    op.add_argument("--vol", type=float, required=True, help="Volatility as percentage")
    op.add_argument("--div-yield", type=float, default=0.0, help="Dividend yield as percentage")
    op.add_argument("--type", choices=["call", "put"], default="call")

    # Implied vol
    iv = subparsers.add_parser("implied_vol")
    iv.add_argument("--spot", type=float, required=True)
    iv.add_argument("--strike", type=float, required=True)
    iv.add_argument("--time", type=float, required=True)
    iv.add_argument("--rate", type=float, required=True)
    iv.add_argument("--market-price", type=float, required=True)
    iv.add_argument("--div-yield", type=float, default=0.0)
    iv.add_argument("--type", choices=["call", "put"], default="call")

    # FX option
    fx = subparsers.add_parser("fx_option_price")
    fx.add_argument("--spot", type=float, required=True)
    fx.add_argument("--strike", type=float, required=True)
    fx.add_argument("--time", type=float, required=True)
    fx.add_argument("--domestic-rate", type=float, required=True)
    fx.add_argument("--foreign-rate", type=float, required=True)
    fx.add_argument("--vol", type=float, required=True)
    fx.add_argument("--type", choices=["call", "put"], default="call")
    fx.add_argument("--notional", type=float, default=1000000)

    # Swap
    sw = subparsers.add_parser("swap_value")
    sw.add_argument("--effective-date", required=True)
    sw.add_argument("--maturity-date", required=True)
    sw.add_argument("--fixed-rate", type=float, required=True)
    sw.add_argument("--freq", type=int, default=2)
    sw.add_argument("--notional", type=float, default=1000000)
    sw.add_argument("--discount-rate", type=float, required=True)

    # CDS
    cd = subparsers.add_parser("cds_value")
    cd.add_argument("--valuation-date", required=True)
    cd.add_argument("--maturity-date", required=True)
    cd.add_argument("--recovery-rate", type=float, required=True)
    cd.add_argument("--notional", type=float, default=10000000)
    cd.add_argument("--spread-bps", type=float, required=True)
    cd.add_argument("--risk-free-rate", type=float, default=5.0, help="Risk-free rate as percentage")
    cd.add_argument("--coupon-bps", type=float, default=None,
                    help="Contractual premium in bps (default: equal to --spread-bps)")

    # Forward
    fw = subparsers.add_parser("forward_price")
    fw.add_argument("--spot", type=float, required=True)
    fw.add_argument("--rate", type=float, required=True)
    fw.add_argument("--time", type=float, required=True)
    fw.add_argument("--div-yield", type=float, default=0.0)

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        sys.exit(1)

    if not HAS_SCIPY:
        print(json.dumps({"error": "scipy not installed. Run: pip install scipy numpy"}))
        sys.exit(1)

    try:
        if args.command == "bond_price":
            result = bond_price_from_ytm(
                args.issue_date, args.settlement_date, args.maturity_date,
                args.coupon_rate, args.ytm, args.freq
            )
        elif args.command == "bond_ytm":
            result = bond_ytm_from_price(
                args.issue_date, args.settlement_date, args.maturity_date,
                args.coupon_rate, args.clean_price, args.freq
            )
        elif args.command == "option_price":
            S, K, T = args.spot, args.strike, args.time
            r, sigma, q = args.rate / 100, args.vol / 100, args.div_yield / 100
            if S <= 0 or K <= 0:
                result = {"error": "Spot and strike must be positive"}
            elif T <= 0:
                result = {"error": "Time to expiry must be positive"}
            elif sigma <= 0:
                result = {"error": "Volatility must be positive"}
            else:
                price = black_scholes_price(S, K, T, r, sigma, q, args.type)
                greeks = black_scholes_greeks(S, K, T, r, sigma, q, args.type)
                result = {"price": round(price, 6), "greeks": greeks}
        elif args.command == "implied_vol":
            S, K, T = args.spot, args.strike, args.time
            r, q = args.rate / 100, args.div_yield / 100
            iv_val, iv_err = _implied_vol_checked(S, K, T, r, args.market_price, q, args.type)
            if iv_err:
                result = {"error": iv_err}
            else:
                result = {"implied_volatility": round(iv_val * 100, 4)}
        elif args.command == "fx_option_price":
            S, K, T = args.spot, args.strike, args.time
            r_d, r_f = args.domestic_rate / 100, args.foreign_rate / 100
            sigma = args.vol / 100
            if S <= 0 or K <= 0:
                result = {"error": "Spot and strike must be positive"}
            elif T <= 0:
                result = {"error": "Time to expiry must be positive"}
            elif sigma <= 0:
                result = {"error": "Volatility must be positive"}
            else:
                price = garman_kohlhagen_price(S, K, T, r_d, r_f, sigma, args.type, args.notional)
                greeks = black_scholes_greeks(S, K, T, r_d, sigma, r_f, args.type)
                result = {
                    "price": round(price, 4),
                    "price_per_unit": round(price / args.notional, 6) if args.notional > 0 else 0,
                    "notional": args.notional,
                    "greeks": greeks,
                }
        elif args.command == "swap_value":
            result = swap_value(
                args.effective_date, args.maturity_date,
                args.fixed_rate, args.freq, args.notional, args.discount_rate
            )
        elif args.command == "cds_value":
            result = cds_value(
                args.valuation_date, args.maturity_date,
                args.recovery_rate, args.notional, args.spread_bps,
                args.risk_free_rate, args.coupon_bps
            )
        elif args.command == "forward_price":
            result = forward_price(
                args.spot, args.rate / 100, args.time, args.div_yield / 100
            )
        else:
            result = {"error": f"Unknown command: {args.command}"}

        print(json.dumps(result))

    except Exception as e:
        print(json.dumps({"error": str(e)}))
        sys.exit(1)


if __name__ == "__main__":
    main()
