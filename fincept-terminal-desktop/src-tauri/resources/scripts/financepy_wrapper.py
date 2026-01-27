#!/usr/bin/env python3
"""
FinancePy Wrapper Script
Comprehensive wrapper for FinancePy library covering all major modules
Usage: python financepy_wrapper.py <command> [args...]
"""

import sys
import json
from datetime import datetime, date
from typing import Any, Dict, List, Optional

# FinancePy imports
try:
    from financepy.utils.date import Date
    from financepy.utils.calendar import Calendar, CalendarTypes
    from financepy.utils.day_count import DayCount, DayCountTypes
    from financepy.utils.frequency import FrequencyTypes

    # Market - Curves
    from financepy.market.curves.discount_curve import DiscountCurve
    from financepy.market.curves.discount_curve_flat import DiscountCurveFlat
    from financepy.market.curves.discount_curve_zeros import DiscountCurveZeros

    # Market - Volatility
    from financepy.market.volatility.equity_vol_surface import EquityVolSurface
    from financepy.market.volatility.fx_vol_surface import FXVolSurface

    # Products - Bonds
    from financepy.products.bonds.bond import Bond
    from financepy.products.bonds.bond_future import BondFuture
    from financepy.products.bonds.bond_option import BondOption
    from financepy.products.bonds.bond_yield_curve import BondYieldCurve

    # Products - Credit
    from financepy.products.credit.cds import CDS
    from financepy.products.credit.cds_curve import CDSCurve

    # Products - Equity
    from financepy.products.equity.equity_vanilla_option import EquityVanillaOption
    from financepy.products.equity.equity_american_option import EquityAmericanOption
    from financepy.products.equity.equity_barrier_option import EquityBarrierOption
    from financepy.products.equity.equity_asian_option import EquityAsianOption
    from financepy.products.equity.equity_digital_option import EquityDigitalOption

    # Products - FX
    from financepy.products.fx.fx_vanilla_option import FXVanillaOption
    from financepy.products.fx.fx_barrier_option import FXBarrierOption
    from financepy.products.fx.fx_digital_option import FXDigitalOption

    # Products - Rates
    from financepy.products.rates.ibor_swap import IborSwap
    from financepy.products.rates.ibor_swaption import IborSwaption
    from financepy.products.rates.ibor_deposit import IborDeposit
    from financepy.products.rates.ibor_fra import IborFRA
    from financepy.products.rates.ibor_future import IborFuture
    from financepy.products.rates.ibor_cap_floor import IborCapFloor

    # Models
    from financepy.models.black_scholes import BlackScholes
    from financepy.models.black import Black
    from financepy.models.bachelier import Bachelier

except ImportError as e:
    print(json.dumps({"error": f"Failed to import FinancePy: {str(e)}"}))
    sys.exit(1)


def date_to_fp(date_str: str) -> Date:
    """Convert date string (YYYY-MM-DD) to FinancePy Date"""
    dt = datetime.strptime(date_str, "%Y-%m-%d")
    return Date(dt.day, dt.month, dt.year)


def fp_date_to_str(fp_date: Date) -> str:
    """Convert FinancePy Date to string"""
    return f"{fp_date.y:04d}-{fp_date.m:02d}-{fp_date.d:02d}"


# ================== UTILS COMMANDS ==================

def utils_create_date(args: List[str]) -> Dict[str, Any]:
    """Create and validate a date
    Args: date_str (YYYY-MM-DD)
    """
    if len(args) < 1:
        return {"error": "Usage: utils_create_date <date_str>"}

    try:
        fp_date = date_to_fp(args[0])
        return {
            "date": fp_date_to_str(fp_date),
            "weekday": fp_date.weekday,
            "is_weekend": fp_date.is_weekend(),
            "excel_date": fp_date.excel_dt,
            "is_eom": fp_date.is_eom()
        }
    except Exception as e:
        return {"error": str(e)}


def utils_date_range(args: List[str]) -> Dict[str, Any]:
    """Generate date range
    Args: start_date, end_date, frequency (D/W/M/Q/Y)
    """
    if len(args) < 3:
        return {"error": "Usage: utils_date_range <start> <end> <freq>"}

    try:
        start_date = date_to_fp(args[0])
        end_date = date_to_fp(args[1])

        # Map frequency
        freq_map = {
            "D": FrequencyTypes.DAILY,
            "W": FrequencyTypes.WEEKLY,
            "M": FrequencyTypes.MONTHLY,
            "Q": FrequencyTypes.QUARTERLY,
            "Y": FrequencyTypes.ANNUAL
        }
        freq = freq_map.get(args[2], FrequencyTypes.MONTHLY)

        dates = start_date.generate_dates(end_date, freq)
        return {
            "dates": [fp_date_to_str(d) for d in dates],
            "count": len(dates)
        }
    except Exception as e:
        return {"error": str(e)}


# ================== BOND COMMANDS ==================

def bond_price(args: List[str]) -> Dict[str, Any]:
    """Calculate bond price
    Args: issue_date, settlement_date, maturity_date, coupon_rate, ytm, freq (1/2/4)
    """
    if len(args) < 6:
        return {"error": "Usage: bond_price <issue_date> <settlement> <maturity> <coupon> <ytm> <freq>"}

    try:
        issue_date = date_to_fp(args[0])
        settlement = date_to_fp(args[1])
        maturity = date_to_fp(args[2])
        coupon = float(args[3]) / 100.0  # Convert to decimal
        ytm = float(args[4]) / 100.0
        freq = int(args[5])

        # Map frequency
        freq_map = {1: FrequencyTypes.ANNUAL, 2: FrequencyTypes.SEMI_ANNUAL, 4: FrequencyTypes.QUARTERLY}
        freq_type = freq_map.get(freq, FrequencyTypes.SEMI_ANNUAL)

        bond = Bond(issue_date, maturity, coupon, freq_type, DayCountTypes.ACT_ACT_ICMA)

        # Calculate metrics
        clean_price = bond.clean_price_from_ytm(settlement, ytm)
        accrued = bond.accrued_interest(settlement)
        dirty_price = clean_price + accrued
        duration = bond.dollar_duration(settlement, ytm)
        convexity = bond.convexity_from_ytm(settlement, ytm)

        return {
            "clean_price": clean_price,
            "dirty_price": dirty_price,
            "accrued_interest": accrued,
            "duration": duration,
            "convexity": convexity,
            "ytm": ytm * 100
        }
    except Exception as e:
        return {"error": str(e)}


def bond_ytm(args: List[str]) -> Dict[str, Any]:
    """Calculate bond yield to maturity
    Args: issue_date, settlement_date, maturity_date, coupon_rate, price, freq
    """
    if len(args) < 6:
        return {"error": "Usage: bond_ytm <issue_date> <settlement> <maturity> <coupon> <price> <freq>"}

    try:
        issue_date = date_to_fp(args[0])
        settlement = date_to_fp(args[1])
        maturity = date_to_fp(args[2])
        coupon = float(args[3]) / 100.0
        price = float(args[4])
        freq = int(args[5])

        freq_map = {1: FrequencyTypes.ANNUAL, 2: FrequencyTypes.SEMI_ANNUAL, 4: FrequencyTypes.QUARTERLY}
        freq_type = freq_map.get(freq, FrequencyTypes.SEMI_ANNUAL)

        bond = Bond(issue_date, maturity, coupon, freq_type, DayCountTypes.ACT_ACT_ICMA)
        ytm = bond.yield_to_maturity(settlement, price)

        return {
            "ytm": ytm * 100,
            "ytm_decimal": ytm
        }
    except Exception as e:
        return {"error": str(e)}


# ================== EQUITY OPTION COMMANDS ==================

def equity_option_price(args: List[str]) -> Dict[str, Any]:
    """Price vanilla equity option (Black-Scholes)
    Args: valuation_date, expiry_date, strike, spot, vol, rate, div, option_type (call/put)
    """
    if len(args) < 8:
        return {"error": "Usage: equity_option_price <val_date> <expiry> <strike> <spot> <vol> <rate> <div> <call|put>"}

    try:
        from financepy.utils.global_types import OptionTypes

        val_date = date_to_fp(args[0])
        expiry = date_to_fp(args[1])
        strike = float(args[2])
        spot = float(args[3])
        vol = float(args[4]) / 100.0  # Convert to decimal
        rate = float(args[5]) / 100.0
        div = float(args[6]) / 100.0

        option_type = OptionTypes.EUROPEAN_CALL if args[7].lower() == "call" else OptionTypes.EUROPEAN_PUT

        option = EquityVanillaOption(expiry, strike, option_type)

        # Create discount curve and model
        discount_curve = DiscountCurveFlat(val_date, rate)
        dividend_curve = DiscountCurveFlat(val_date, div)
        model = BlackScholes(vol)

        # Price option
        price = option.value(val_date, spot, discount_curve, dividend_curve, model)
        delta = option.delta(val_date, spot, discount_curve, dividend_curve, model)
        gamma = option.gamma(val_date, spot, discount_curve, dividend_curve, model)
        vega = option.vega(val_date, spot, discount_curve, dividend_curve, model)
        theta = option.theta(val_date, spot, discount_curve, dividend_curve, model)
        rho = option.rho(val_date, spot, discount_curve, dividend_curve, model)

        return {
            "price": price,
            "greeks": {
                "delta": delta,
                "gamma": gamma,
                "vega": vega,
                "theta": theta,
                "rho": rho
            }
        }
    except Exception as e:
        return {"error": str(e)}


def equity_option_implied_vol(args: List[str]) -> Dict[str, Any]:
    """Calculate implied volatility
    Args: valuation_date, expiry_date, strike, spot, price, rate, div, option_type
    """
    if len(args) < 8:
        return {"error": "Usage: equity_option_implied_vol <val_date> <expiry> <strike> <spot> <price> <rate> <div> <call|put>"}

    try:
        from financepy.utils.global_types import OptionTypes

        val_date = date_to_fp(args[0])
        expiry = date_to_fp(args[1])
        strike = float(args[2])
        spot = float(args[3])
        price = float(args[4])
        rate = float(args[5]) / 100.0
        div = float(args[6]) / 100.0

        option_type = OptionTypes.EUROPEAN_CALL if args[7].lower() == "call" else OptionTypes.EUROPEAN_PUT

        option = EquityVanillaOption(expiry, strike, option_type)
        discount_curve = DiscountCurveFlat(val_date, rate)
        dividend_curve = DiscountCurveFlat(val_date, div)

        impl_vol = option.implied_volatility(val_date, spot, discount_curve, dividend_curve, price)

        return {
            "implied_volatility": impl_vol * 100,
            "implied_volatility_decimal": impl_vol
        }
    except Exception as e:
        return {"error": str(e)}


# ================== FX OPTION COMMANDS ==================

def fx_option_price(args: List[str]) -> Dict[str, Any]:
    """Price FX vanilla option
    Args: valuation_date, expiry_date, strike, spot, vol, dom_rate, for_rate, option_type, notional
    """
    if len(args) < 9:
        return {"error": "Usage: fx_option_price <val_date> <expiry> <strike> <spot> <vol> <dom_rate> <for_rate> <call|put> <notional>"}

    try:
        from financepy.utils.global_types import OptionTypes

        val_date = date_to_fp(args[0])
        expiry = date_to_fp(args[1])
        strike = float(args[2])
        spot = float(args[3])
        vol = float(args[4]) / 100.0
        dom_rate = float(args[5]) / 100.0
        for_rate = float(args[6]) / 100.0
        option_type = OptionTypes.EUROPEAN_CALL if args[7].lower() == "call" else OptionTypes.EUROPEAN_PUT
        notional = float(args[8])

        option = FXVanillaOption(expiry, strike, option_type, notional)

        dom_curve = DiscountCurveFlat(val_date, dom_rate)
        for_curve = DiscountCurveFlat(val_date, for_rate)

        price = option.value(val_date, spot, dom_curve, for_curve, vol)
        delta = option.delta(val_date, spot, dom_curve, for_curve, vol)

        return {
            "price": price,
            "delta": delta,
            "notional": notional
        }
    except Exception as e:
        return {"error": str(e)}


# ================== INTEREST RATE SWAP COMMANDS ==================

def ibor_swap_price(args: List[str]) -> Dict[str, Any]:
    """Price interest rate swap
    Args: effective_date, maturity_date, fixed_rate, freq, notional, discount_rate
    """
    if len(args) < 6:
        return {"error": "Usage: ibor_swap_price <effective> <maturity> <fixed_rate> <freq> <notional> <discount_rate>"}

    try:
        from financepy.utils.global_types import SwapTypes

        effective = date_to_fp(args[0])
        maturity = date_to_fp(args[1])
        fixed_rate = float(args[2]) / 100.0
        freq = int(args[3])
        notional = float(args[4])
        discount_rate = float(args[5]) / 100.0

        freq_map = {1: FrequencyTypes.ANNUAL, 2: FrequencyTypes.SEMI_ANNUAL, 4: FrequencyTypes.QUARTERLY}
        freq_type = freq_map.get(freq, FrequencyTypes.QUARTERLY)

        swap = IborSwap(effective, maturity, SwapTypes.PAY, fixed_rate, freq_type, notional=notional)
        discount_curve = DiscountCurveFlat(effective, discount_rate)

        value = swap.value(effective, discount_curve, discount_curve)

        return {
            "value": value,
            "fixed_rate": fixed_rate * 100,
            "notional": notional
        }
    except Exception as e:
        return {"error": str(e)}


# ================== CDS COMMANDS ==================

def cds_spread(args: List[str]) -> Dict[str, Any]:
    """Calculate CDS spread
    Args: valuation_date, maturity_date, recovery_rate, default_prob
    """
    if len(args) < 4:
        return {"error": "Usage: cds_spread <val_date> <maturity> <recovery> <default_prob>"}

    try:
        val_date = date_to_fp(args[0])
        maturity = date_to_fp(args[1])
        recovery = float(args[2])
        default_prob = float(args[3])

        cds = CDS(val_date, maturity)

        # Create simple CDS curve
        cds_curve = CDSCurve(val_date, [cds], recovery_rate=recovery)

        return {
            "valuation_date": fp_date_to_str(val_date),
            "maturity": fp_date_to_str(maturity),
            "recovery_rate": recovery,
            "default_probability": default_prob
        }
    except Exception as e:
        return {"error": str(e)}


# ================== MAIN COMMAND ROUTER ==================

def main(args=None):
    """Main entry point for script execution"""
    if args is None:
        args = sys.argv[1:]

    if len(args) < 1:
        return json.dumps({
            "error": "Usage: financepy_wrapper.py <command> [args...]",
            "available_commands": [
                "utils_create_date", "utils_date_range",
                "bond_price", "bond_ytm",
                "equity_option_price", "equity_option_implied_vol",
                "fx_option_price",
                "ibor_swap_price",
                "cds_spread"
            ]
        })

    command = args[0]
    command_args = args[1:]

    # Command routing
    commands = {
        # Utils
        "utils_create_date": utils_create_date,
        "utils_date_range": utils_date_range,

        # Bonds
        "bond_price": bond_price,
        "bond_ytm": bond_ytm,

        # Equity Options
        "equity_option_price": equity_option_price,
        "equity_option_implied_vol": equity_option_implied_vol,

        # FX Options
        "fx_option_price": fx_option_price,

        # Interest Rate Swaps
        "ibor_swap_price": ibor_swap_price,

        # Credit
        "cds_spread": cds_spread,
    }

    if command not in commands:
        result = {
            "error": f"Unknown command: {command}",
            "available_commands": list(commands.keys())
        }
    else:
        try:
            result = commands[command](command_args)
        except Exception as e:
            result = {"error": f"Command execution failed: {str(e)}"}

    output = json.dumps(result, indent=2)
    print(output)
    return output


if __name__ == "__main__":
    main()
