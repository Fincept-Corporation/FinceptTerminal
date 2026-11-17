"""Alternative Investments CLI

Unified command-line interface for alternative investment analytics.
"""

import os
import sys
import json
import math
import argparse
from decimal import Decimal
from typing import Dict, Any, List
from datetime import datetime, timedelta

# scripts/ (fincept_irr) is on PYTHONPATH when the terminal runs this; also when run by hand.
_SCRIPTS_DIR = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
if _SCRIPTS_DIR not in sys.path:
    sys.path.append(_SCRIPTS_DIR)

from config import AssetClass, HedgeFundStrategy, CommoditySector, RealEstateType, AssetParameters, MarketData
from digital_assets import DigitalAssetAnalyzer
from natural_resources import CommodityAnalyzer
from private_capital import PrivateEquityAnalyzer
from real_estate import RealEstateAnalyzer, InternationalREITAnalyzer
from performance_metrics import PerformanceAnalyzer
from risk_analyzer import RiskAnalyzer
from data_handler import DataHandler

# New modules from alternative investment analysis
from inflation_protected import TIPSAnalyzer, IBondAnalyzer
from high_yield_bonds import HighYieldBondAnalyzer
from preferred_stocks import PreferredStockAnalyzer
from precious_metals import PreciousMetalsEquityAnalyzer
from convertible_bonds import ConvertibleBondAnalyzer
from fixed_annuities import FixedAnnuityAnalyzer, InflationIndexedAnnuityAnalyzer
from emerging_market_bonds import EmergingMarketBondAnalyzer
from managed_futures import ManagedFuturesAnalyzer
from market_neutral import MarketNeutralAnalyzer
from stable_value import StableValueFundAnalyzer
from equity_indexed_annuities import EquityIndexedAnnuityAnalyzer
from asset_location import AssetLocationAnalyzer
from covered_calls import CoveredCallAnalyzer
from sri_funds import SRIFundAnalyzer
from leveraged_funds import LeveragedFundAnalyzer
from structured_products import StructuredProductAnalyzer
from variable_annuities import VariableAnnuityAnalyzer


def decimal_default(obj):
    """JSON serializer for Decimal objects"""
    if isinstance(obj, Decimal):
        return float(obj)
    raise TypeError(f"Object of type {type(obj)} is not JSON serializable")


# ── Screen entry point ───────────────────────────────────────────────────────
# The Alternative Investments screen sends its form as-is (display-oriented keys); the names each
# analyzer reads are filled in here, so the screen and this file cannot drift apart. An alias only
# applies when the analyzer's own key is absent (a caller may always send the analyzer's names).
_SCREEN_ALIASES = {
    'digital-assets': {'volume_24h': 'trading_volume_24h', 'max_supply': 'total_supply'},
    'real-estate': {'gross_income': 'gross_rental_income'},
    'tips': {'face_value': 'acquisition_price', 'current_market_value': 'current_price'},
    'ibonds': {'purchase_price': 'face_value'},
    'high-yield': {'par_value': 'face_value', 'price': 'current_market_value'},
    'convertible-bonds': {'par_value': 'face_value', 'current_price': 'current_market_value'},
    'preferred-stocks': {'par_value': 'acquisition_price', 'current_price': 'current_market_value'},
    'annuities': {'premium': 'acquisition_price', 'annual_payout_rate': 'annuity_rate'},
    'stable-value': {'book_value': 'acquisition_price', 'market_value': 'current_market_value'},
    'covered-calls': {'shares': 'shares_owned', 'premium': 'option_premium'},
    'sri': {'sri_return': 'fund_return'},
    'natural-resources': {'sector': 'commodity_sector'},
    'variable-annuities': {'me_fee': 'mortality_expense_fee', 'investment_fee': 'investment_mgmt_fee',
                           'surrender_period': 'surrender_years'},
    'eia': {'premium': 'acquisition_price', 'surrender_period': 'surrender_charge_years'},
}

# (analyzer, method) pairs whose output is general product commentary that does not depend on any
# input. They stay available as "product guides", but without the score ("THE UGLY", "1/10") that
# made them read like an analysis of the user's numbers.
GUIDE_METHODS = {
    ('structured-products', 'complexity'), ('structured-products', 'costs'), ('structured-products', 'verdict'),
    ('covered-calls', 'verdict'),
    ('leveraged-funds', 'verdict'),
    ('variable-annuities', 'verdict'),
    ('sri', 'approaches'), ('sri', 'verdict'),
    ('ibonds', 'tax_efficiency'),
    ('preferred-stocks', 'dividend_safety'),
}

_GUIDE_NOTE = ('General product guide: the same text for every input. It is background reading, '
               'not an analysis of the numbers you entered.')


def _apply_screen_aliases(command: str, data: Dict[str, Any]) -> Dict[str, Any]:
    data = dict(data)
    for src, dst in _SCREEN_ALIASES.get(command, {}).items():
        if src in data and dst not in data:
            data[dst] = data[src]
    return data


def _as_guide(metrics: Any) -> Any:
    """Strip the score / verdict label from a fixed-text method and mark it as a guide."""
    if not isinstance(metrics, dict):
        return metrics
    out = {k: v for k, v in metrics.items() if k not in ('rating', 'category')}
    title = out.pop('product', None) or out.pop('strategy', None)
    guide = {'guide': True, 'guide_note': _GUIDE_NOTE}
    if title:
        guide['guide_title'] = title
    guide.update(out)
    return guide


def run_command(command: str, data: Dict[str, Any], method: str) -> Dict[str, Any]:
    """One analyzer call as the screen makes it: alias the form keys, run, mark guides."""
    fn = _SCREEN_ANALYZERS[command]
    data = _apply_screen_aliases(command, data)
    if command == 'asset-location':
        result = fn(data, method, Decimal(str(data.get('tax_bracket', 0.24))))
    else:
        result = fn(data, method)
    if (command, method) in GUIDE_METHODS and isinstance(result, dict) and result.get('success', True):
        result = dict(result)
        result['metrics'] = _as_guide(result.get('metrics'))
        result['guide'] = True
    return result


def main():
    parser = argparse.ArgumentParser(description='Alternative Investments Analytics CLI')
    subparsers = parser.add_subparsers(dest='command', help='Available commands')

    # Digital Assets
    digital = subparsers.add_parser('digital-assets', help='Digital asset analytics')
    digital.add_argument('--data', required=True, help='Asset data (JSON)')
    # volatility / on-chain metrics need a price / chain history this command does not fetch yet.
    digital.add_argument('--method', default='fundamental', help='Analysis method: fundamental')

    # Hedge Funds
    hedge = subparsers.add_parser('hedge-funds', help='Hedge fund analytics')
    hedge.add_argument('--data', required=True, help='Fund data (JSON)')
    hedge.add_argument('--method', default='metrics', help='Analysis method: metrics (needs monthly_returns)')

    # Natural Resources
    natural = subparsers.add_parser('natural-resources', help='Natural resource analytics')
    natural.add_argument('--data', required=True, help='Resource data (JSON)')
    natural.add_argument('--method', default='basis', help='Analysis method: basis, contango, futures')

    # Private Capital
    private = subparsers.add_parser('private-capital', help='Private capital analytics')
    private.add_argument('--data', required=True, help='Investment data (JSON)')
    private.add_argument('--method', default='metrics', help='Analysis method: metrics')

    # Real Estate
    real_estate = subparsers.add_parser('real-estate', help='Real estate analytics')
    real_estate.add_argument('--data', required=True, help='Property data (JSON)')
    real_estate.add_argument('--method', default='summary',
                             help='Analysis method: summary, financing, dcf (noi, caprate kept as single numbers)')

    # International REITs
    intl_reit = subparsers.add_parser('intl-reit', help='International REIT analytics')
    intl_reit.add_argument('--data', required=True, help='International REIT data (JSON)')
    intl_reit.add_argument('--method', default='diversification', help='Analysis method: diversification, currency, expense, regional')

    # Performance Metrics
    performance = subparsers.add_parser('performance', help='Performance metrics')
    performance.add_argument('--returns', required=True, help='Returns data (JSON)')
    performance.add_argument('--benchmark', help='Benchmark returns (JSON)')
    performance.add_argument('--method', default='twr', help='Analysis method: twr, mwr, sharpe')

    # Risk Analysis
    risk = subparsers.add_parser('risk', help='Risk analysis')
    risk.add_argument('--returns', required=True, help='Returns data (JSON)')
    risk.add_argument('--method', default='var', help='Analysis method: var, cvar, stress')
    risk.add_argument('--confidence-level', type=float, default=0.95, help='VaR confidence level')

    # TIPS (Inflation-Protected Securities)
    tips = subparsers.add_parser('tips', help='TIPS analytics')
    tips.add_argument('--data', required=True, help='TIPS data (JSON)')
    tips.add_argument('--method', default='real_yield', help='Analysis method: real_yield, inflation_scenarios, tax_efficiency')

    # I Bonds
    ibonds = subparsers.add_parser('ibonds', help='I Bonds (Series I Savings Bonds) analytics')
    ibonds.add_argument('--data', required=True, help='I Bond data (JSON)')
    ibonds.add_argument('--method', default='composite_rate', help='Analysis method: composite_rate, penalty, compare_tips, tax_efficiency')

    # High-Yield Bonds
    high_yield = subparsers.add_parser('high-yield', help='High-yield bond analytics')
    high_yield.add_argument('--data', required=True, help='Bond data (JSON)')
    high_yield.add_argument('--method', default='credit_analysis', help='Analysis method: credit_analysis, default_prob, equity_behavior')

    # Preferred Stocks
    preferred = subparsers.add_parser('preferred-stocks', help='Preferred stock analytics')
    preferred.add_argument('--data', required=True, help='Preferred stock data (JSON)')
    preferred.add_argument('--method', default='yield_analysis', help='Analysis method: yield_analysis, call_risk, dividend_safety')

    # Precious Metals Equities
    pme = subparsers.add_parser('pme', help='Precious metals equities analytics')
    pme.add_argument('--data', required=True, help='PME data (JSON)')
    pme.add_argument('--method', default='correlation', help='Analysis method: correlation, drawdowns, crisis_performance')

    # Convertible Bonds
    convertible = subparsers.add_parser('convertible-bonds', help='Convertible bond analytics')
    convertible.add_argument('--data', required=True, help='Convertible bond data (JSON)')
    convertible.add_argument('--method', default='conversion_premium', help='Analysis method: conversion_premium, bond_floor, upside_participation')

    # Fixed Annuities
    annuity = subparsers.add_parser('annuities', help='Fixed annuity analytics')
    annuity.add_argument('--data', required=True, help='Annuity data (JSON)')
    annuity.add_argument('--method', default='payouts', help='Analysis method: payouts, inflation_erosion, self_insurance')

    # Inflation-Indexed Annuities
    inflation_annuity = subparsers.add_parser('inflation-annuity', help='Inflation-indexed annuity analytics')
    inflation_annuity.add_argument('--data', required=True, help='Inflation annuity data (JSON)')
    inflation_annuity.add_argument('--method', default='compare_fixed', help='Analysis method: compare_fixed, compare_tips, longevity, inflation_value')

    # Emerging Market Bonds
    em_bonds = subparsers.add_parser('em-bonds', help='Emerging market bond analytics')
    em_bonds.add_argument('--data', required=True, help='EM bond data (JSON)')
    em_bonds.add_argument('--method', default='yield_spread', help='Analysis method: yield_spread, default_risk, currency_risk')

    # Managed Futures
    managed_futures = subparsers.add_parser('managed-futures', help='Managed futures / CTA analytics')
    managed_futures.add_argument('--data', required=True, help='Managed futures data (JSON)')
    managed_futures.add_argument('--method', default='trend_following', help='Analysis method: trend_following, crisis_alpha, fee_impact')

    # Market-Neutral Funds
    market_neutral = subparsers.add_parser('market-neutral', help='Market-neutral fund analytics')
    market_neutral.add_argument('--data', required=True, help='Market-neutral fund data (JSON)')
    market_neutral.add_argument('--method', default='beta_analysis', help='Analysis method: beta_analysis, factor_exposure, leverage_risk')

    # Stable Value Funds
    stable_value = subparsers.add_parser('stable-value', help='Stable value fund analytics')
    stable_value.add_argument('--data', required=True, help='Stable value fund data (JSON)')
    stable_value.add_argument('--method', default='market_to_book', help='Analysis method: market_to_book, crediting_rate, suitability')

    # Equity-Indexed Annuities
    eia = subparsers.add_parser('eia', help='Equity-indexed annuity analytics')
    eia.add_argument('--data', required=True, help='EIA data (JSON)')
    eia.add_argument('--method', default='crediting', help='Analysis method: crediting, upside_limitation, surrender_charges')

    # Asset Location
    asset_loc = subparsers.add_parser('asset-location', help='Tax-efficient asset location analysis')
    asset_loc.add_argument('--data', required=True, help='Asset or portfolio data (JSON)')
    asset_loc.add_argument('--method', default='optimal', help='Analysis method: optimal, value_added, portfolio, muni_bond')
    asset_loc.add_argument('--tax-bracket', type=float, default=0.24, help='Marginal tax rate (e.g., 0.24 for 24%%)')

    # Covered Calls
    covered_calls = subparsers.add_parser('covered-calls', help='Covered call strategy analytics')
    covered_calls.add_argument('--data', required=True, help='Covered call position data (JSON)')
    covered_calls.add_argument('--method', default='tax', help='Analysis method: tax, opportunity_cost, alternative, verdict')

    # SRI Funds
    sri = subparsers.add_parser('sri', help='Socially responsible investing fund analytics')
    sri.add_argument('--data', required=True, help='SRI fund data (JSON)')
    sri.add_argument('--method', default='performance', help='Analysis method: performance, screening, expenses, approaches')

    # Leveraged Funds
    leveraged = subparsers.add_parser('leveraged-funds', help='Leveraged funds (2x, 3x ETFs) analytics')
    leveraged.add_argument('--data', required=True, help='Leveraged fund data (JSON)')
    leveraged.add_argument('--method', default='decay', help='Analysis method: decay, volatility, verdict')

    # Structured Products
    structured = subparsers.add_parser('structured-products', help='Structured investment products analytics')
    structured.add_argument('--data', required=True, help='Structured product data (JSON)')
    structured.add_argument('--method', default='complexity', help='Analysis method: complexity, costs, verdict')

    # Variable Annuities
    var_annuity = subparsers.add_parser('variable-annuities', help='Variable annuities analytics')
    var_annuity.add_argument('--data', required=True, help='Variable annuity data (JSON)')
    var_annuity.add_argument('--method', default='fees',
                             help='Analysis method: fees, tax, alternatives, surrender, verdict')

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        return

    try:
        result = None

        # `--data -` reads the JSON from stdin: the terminal sends the form that way, so a long
        # cash-flow or return series never meets the Windows command-line length limit.
        if getattr(args, 'data', None) == '-':
            args.data = sys.stdin.read()

        if args.command == 'intl-reit':
            data = json.loads(args.data)
            result = analyze_international_reit(data, args.method)

        elif args.command == 'performance':
            returns = json.loads(args.returns)
            benchmark = json.loads(args.benchmark) if args.benchmark else None
            result = analyze_performance(returns, benchmark, args.method)

        elif args.command == 'risk':
            returns = json.loads(args.returns)
            result = analyze_risk(returns, args.method, args.confidence_level)

        elif args.command == 'pme':
            data = json.loads(args.data)
            result = analyze_pme(data, args.method)

        elif args.command in _SCREEN_ANALYZERS:
            data = json.loads(args.data)
            if not isinstance(data, dict):
                raise ValueError('--data must be a JSON object')
            if args.command == 'asset-location' and 'tax_bracket' not in data:
                data['tax_bracket'] = args.tax_bracket
            result = run_command(args.command, data, args.method)

        # Output result as JSON
        print(json.dumps(result, default=decimal_default, indent=2))

    except Exception as e:
        error_result = {
            'success': False,
            'error': str(e),
            'error_type': type(e).__name__
        }
        print(json.dumps(error_result, indent=2))
        sys.exit(1)


def analyze_digital_assets(data: Dict[str, Any], method: str = 'fundamental') -> Dict[str, Any]:
    """Analyze digital assets"""
    try:
        # Create parameters
        params = AssetParameters(
            asset_class=AssetClass.DIGITAL_ASSETS,
            ticker=data.get('ticker'),
            name=data.get('name'),
            currency=data.get('currency', 'USD')
        )

        # Set digital asset specific params
        for key in ['asset_type', 'blockchain', 'market_cap', 'circulating_supply',
                    'total_supply', 'trading_volume_24h', 'staking_yield', 'protocol_revenue']:
            if key in data:
                setattr(params, key, data[key])

        analyzer = DigitalAssetAnalyzer(params)

        # Add market data if provided
        if 'market_data' in data:
            handler = DataHandler()
            market_data = handler.standardize_price_data(data['market_data'])
            analyzer.add_market_data(market_data)

        # Execute requested analysis. Volatility and on-chain metrics need a price / chain history that
        # this command does not fetch (they crashed with a missing-method error); say so plainly.
        if method in ('volatility', 'onchain'):
            return {'success': False,
                    'error': 'Volatility and on-chain metrics need a price history, which this analyzer '
                             'does not fetch yet. Use the fundamental method.'}
        metrics = analyzer.fundamental_metrics()

        return {
            'success': True,
            'asset_type': 'digital_assets',
            'method': method,
            'metrics': metrics
        }
    except Exception as e:
        return {'success': False, 'error': str(e)}


def _monthly_return_stats(returns: List[float], rf_annual: float) -> Dict[str, Any]:
    """Annualised statistics of a MONTHLY return series given as decimals.

    Cumulative return and CAGR compound the series; volatility, Sharpe and Sortino
    are annualised with sqrt(12); Sharpe/Sortino use the monthly equivalent of the
    annual risk-free rate (Sortino's downside deviation is measured below it).
    """
    n = len(returns)
    growth, peak, max_dd = 1.0, 1.0, 0.0
    for r in returns:
        growth *= (1.0 + r)
        peak = max(peak, growth)
        max_dd = max(max_dd, 1.0 - growth / peak)
    years = n / 12.0
    cagr = growth ** (1.0 / years) - 1.0 if growth > 0 and years > 0 else None
    mean = sum(returns) / n
    vol_m = math.sqrt(sum((r - mean) ** 2 for r in returns) / (n - 1)) if n > 1 else 0.0
    rf_m = (1.0 + rf_annual) ** (1.0 / 12.0) - 1.0
    mean_excess = sum(r - rf_m for r in returns) / n
    downside = math.sqrt(sum(min(0.0, r - rf_m) ** 2 for r in returns) / n)
    return {
        'months': n,
        'cumulative_return': growth - 1.0,
        'cagr': cagr,
        'annualized_volatility': vol_m * math.sqrt(12.0),
        'sharpe_ratio': (mean_excess / vol_m) * math.sqrt(12.0) if vol_m > 0 else None,
        'sortino_ratio': (mean_excess / downside) * math.sqrt(12.0) if downside > 0 else None,
        'max_drawdown': max_dd,
        'best_month_return': max(returns),
        'worst_month_return': min(returns),
        'positive_months_rate': sum(1 for r in returns if r > 0) / n,
    }


def _net_of_hedge_fund_fees(gross: List[float], mgmt_fee: float, perf_fee: float,
                            hurdle: float) -> tuple:
    """Monthly NET returns after a management fee accrued monthly on NAV and a
    performance fee crystallised every 12 months (and at the end) on gains above
    the high-water mark grown at the hurdle rate (hard hurdle). Returns
    (net_returns, total_fees_per_1_invested)."""
    nav, hwm, fees = 1.0, 1.0, 0.0
    block_start_hwm = 1.0
    net = []
    for i, r in enumerate(gross):
        prev = nav
        nav *= (1.0 + r)
        m = nav * mgmt_fee / 12.0
        nav -= m
        fees += m
        if (i + 1) % 12 == 0 or i == len(gross) - 1:
            months_in_block = (i % 12) + 1
            threshold = block_start_hwm * (1.0 + hurdle) ** (months_in_block / 12.0)
            if nav > threshold:
                p = perf_fee * (nav - threshold)
                nav -= p
                fees += p
            hwm = max(hwm, nav)
            block_start_hwm = hwm
        net.append(nav / prev - 1.0)
    return net, fees


def analyze_hedge_funds(data: Dict[str, Any], method: str = 'metrics') -> Dict[str, Any]:
    """Hedge fund performance from the fund's own MONTHLY GROSS returns.

    data: monthly_returns (list of decimals, oldest first, >= 12 months),
          management_fee, performance_fee, hurdle_rate, risk_free_rate (annual
          decimals), strategy_type (label only).
    Returns gross and net-of-fee statistics and the fee drag. Without a return
    series there is nothing to measure, so the call fails instead of inventing data.
    """
    try:
        returns = [float(r) for r in data.get('monthly_returns', []) or []]
        if len(returns) < 12:
            return {'success': False,
                    'error': 'Hedge fund metrics need at least 12 monthly returns (monthly_returns, '
                             f'as decimals); got {len(returns)}'}
        if any(r <= -1.0 for r in returns):
            return {'success': False, 'error': 'A monthly return of -100% or worse is not a valid return'}

        mgmt = float(data.get('management_fee', 0.02))
        perf = float(data.get('performance_fee', 0.20))
        hurdle = float(data.get('hurdle_rate', 0.0))
        rf = float(data.get('risk_free_rate', 0.0))

        net, fees = _net_of_hedge_fund_fees(returns, mgmt, perf, hurdle)
        gross_stats = _monthly_return_stats(returns, rf)
        net_stats = _monthly_return_stats(net, rf)
        drag = (gross_stats['cagr'] - net_stats['cagr']
                if gross_stats['cagr'] is not None and net_stats['cagr'] is not None else None)

        # Growth of 100, gross and net, month by month (charted against the month number, or the
        # dates when the series came with them).
        dates = data.get('dates') if isinstance(data.get('dates'), list) else None
        if dates is not None and len(dates) != len(returns):
            dates = None
        g_val, n_val, path = 100.0, 100.0, []
        for i, (rg, rn) in enumerate(zip(returns, net)):
            g_val *= (1.0 + rg)
            n_val *= (1.0 + rn)
            point = {'date': str(dates[i])[:10]} if dates else {'months': i + 1}
            point.update({'gross_value': g_val, 'net_value': n_val})
            path.append(point)

        metrics = {
            'gross': gross_stats,
            'net_of_fees': net_stats,
            'fee_drag_annual': drag,
            'total_fees_per_100_invested': fees * 100.0,
            'fee_terms': {
                'management_fee': mgmt,
                'performance_fee': perf,
                'hurdle_rate': hurdle,
            },
            'fee_model': 'Management fee accrued monthly on NAV; performance fee crystallised '
                         'annually above the high-water mark grown at the hurdle (hard hurdle).',
            'growth_of_100': path,
        }
        return {'success': True, 'asset_type': 'hedge_funds', 'method': 'metrics', 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def _nr_decimal(v):
    """JSON numbers -> Decimal (the commodity analyzer does Decimal arithmetic)."""
    if isinstance(v, (int, float)) and not isinstance(v, bool):
        return Decimal(str(v))
    return v


def _commodity_term_structure(analyzer) -> Dict[str, Any]:
    """Basis at every point of the futures curve, the curve's shape, and the roll yield
    between the first two contracts (built from the analyzer's basis / roll-yield methods)."""
    if not analyzer.spot_price:
        return {'error': 'Spot price required'}
    curve = sorted(analyzer.futures_prices.items())
    if not curve:
        return {'error': 'At least one futures price is required'}
    points = []
    for months, price in curve:
        b = analyzer.calculate_futures_basis(price, months)
        points.append({'expiry_months': months, 'futures_price': price, 'basis': b['basis'],
                       'basis_percentage': b['basis_percentage'], 'annualized_basis': b['annualized_basis'],
                       'market_structure': b['market_structure']})
    structures = {p['market_structure'] for p in points}
    result = {
        'spot_price': analyzer.spot_price,
        'curve_shape': structures.pop() if len(structures) == 1 else 'mixed',
        'commodity_sector': getattr(analyzer.commodity_sector, 'value', str(analyzer.commodity_sector)),
        'term_structure': points,
    }
    if len(curve) >= 2:
        (m1, p1), (m2, p2) = curve[0], curve[1]
        roll = analyzer.roll_yield_analysis(p1, p2, max(1, (m2 - m1) * 30))
        if 'error' not in roll:
            result['roll_yield'] = roll
    return result


def analyze_natural_resources(data: Dict[str, Any], method: str = 'basis') -> Dict[str, Any]:
    """Analyze natural resources"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.COMMODITIES,
            ticker=data.get('ticker'),
            name=data.get('name'),
            currency=data.get('currency', 'USD')
        )

        # Numbers are converted on the way in: a raw JSON float spot price made every
        # method fail with "unsupported operand type(s) for -: 'decimal.Decimal' and 'float'".
        for key in ['spot_price', 'storage_cost', 'convenience_yield', 'contract_size']:
            if key in data:
                setattr(params, key, _nr_decimal(data[key]))

        # The analyzer reads the sector as a CommoditySector (the form sends "sector").
        sector = data.get('commodity_sector', data.get('sector'))
        if sector is not None:
            try:
                params.commodity_sector = CommoditySector(str(sector).lower())
            except ValueError:
                pass

        # Futures curve {months: price}: an explicit futures_prices map, plus the form's
        # 3 / 6 / 12-month contracts.
        curve = {}
        raw_curve = data.get('futures_prices')
        if isinstance(raw_curve, dict):
            for k, v in raw_curve.items():
                try:
                    curve[int(float(k))] = _nr_decimal(v)
                except (TypeError, ValueError):
                    pass
        for months, key in ((1, 'one_month_futures'), (3, 'three_month_futures'),
                            (6, 'six_month_futures'), (12, 'twelve_month_futures')):
            if months not in curve and isinstance(data.get(key), (int, float)):
                curve[months] = _nr_decimal(data[key])
        if curve:
            params.futures_prices = dict(sorted(curve.items()))

        analyzer = CommodityAnalyzer(params)

        if method == 'contango':
            metrics = _commodity_term_structure(analyzer)
        elif method == 'futures':
            t = _nr_decimal(data.get('time_to_expiry_years', 0.25))
            metrics = {
                'theoretical_futures_price': analyzer.theoretical_futures_price(t),
                'spot_price': analyzer.spot_price,
                'time_to_expiry_years': t,
                'storage_cost': analyzer.storage_cost,
                'convenience_yield': analyzer.convenience_yield,
            }
        else:
            expiry = int(data.get('expiry_months', 3))
            futures_price = data.get('futures_price', curve.get(expiry, 100))
            metrics = analyzer.calculate_futures_basis(_nr_decimal(futures_price), expiry)

        return {
            'success': True,
            'asset_type': 'natural_resources',
            'method': method,
            'metrics': metrics
        }
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_private_capital(data: Dict[str, Any], method: str = 'metrics') -> Dict[str, Any]:
    """Analyze private capital"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.PRIVATE_EQUITY,
            ticker=data.get('ticker'),
            name=data.get('name'),
            currency=data.get('currency', 'USD')
        )

        for key in ['fund_life', 'vintage_year', 'commitment']:
            if key in data:
                setattr(params, key, data[key])

        analyzer = PrivateEquityAnalyzer(params)

        # Cash flows from the LP's side. Each row is {date, amount} with an optional `type`:
        #   call         - capital paid in (sign ignored; a bare negative amount is a call too)
        #   distribution - cash returned (a bare positive amount)
        #   nav          - the fund's reported value on that date (the latest one is the NAV)
        # Calls and distributions go through the analyzer's handlers so paid-in and distributed
        # totals are tracked (add_cash_flows alone left both at 0 in the output).
        flows = []
        navs = []
        for item in data.get('cash_flows') or []:
            date = str(item.get('date', ''))[:10]
            try:
                datetime.strptime(date, '%Y-%m-%d')
            except ValueError:
                raise ValueError(f"Cash-flow date '{item.get('date', '')}' is not a date (use YYYY-MM-DD)")
            amount = Decimal(str(item.get('amount', 0)))
            kind = str(item.get('type', '') or '').strip().lower()
            if kind == 'nav':
                navs.append((date, abs(amount)))
            elif kind == 'call':
                flows.append((date, -abs(amount)))
            elif kind == 'distribution':
                flows.append((date, abs(amount)))
            else:
                flows.append((date, amount))
        for date, amount in sorted(flows):
            if amount < 0:
                analyzer.process_capital_call(abs(amount), date)
            elif amount > 0:
                analyzer.process_distribution(amount, date)
        if not analyzer.cash_flows:
            return {'success': False,
                    'error': 'Private-capital metrics need at least one capital call (a dated amount paid in)'}

        if data.get('commitment'):
            vintage = data.get('vintage_year') or (int(analyzer.cash_flows[0].date[:4]) if analyzer.cash_flows else
                                                   datetime.now().year)
            analyzer.add_commitment(Decimal(str(data['commitment'])), int(vintage))

        # NAV: the latest NAV row of the ledger, else current_nav. Its date is the terminal date of the
        # IRR, so it is never silently "today" (the result drifted daily): an explicit nav_date wins,
        # then the NAV row's date, then the last cash-flow date - and the date used is reported.
        last_flow_date = max(d for d, _ in flows) if flows else None
        nav_value, nav_date = None, None
        if navs:
            nav_date, nav_value = sorted(navs)[-1]
        elif 'current_nav' in data:
            nav_value = Decimal(str(data['current_nav']))
        if nav_value is not None:
            nav_date = str(data.get('nav_date') or '')[:10] or nav_date or last_flow_date
            datetime.strptime(nav_date, '%Y-%m-%d')
            analyzer.update_nav(nav_value, nav_date)

        # irr / moic used to call methods the analyzer does not have (AttributeError); every figure
        # is part of the key metrics.
        metrics = analyzer.calculate_key_metrics()
        if isinstance(metrics, dict) and 'error' not in metrics:
            if nav_date:
                metrics['nav_date'] = nav_date
            # Cumulative ledger (paid in, distributed, net cash) - the J-curve of the investment.
            ledger, paid, dist = [], 0.0, 0.0
            for date, amount in sorted(flows):
                a = float(amount)
                if a < 0:
                    paid += -a
                else:
                    dist += a
                ledger.append({'date': date, 'amount': a, 'cumulative_paid_in': paid,
                               'cumulative_distributed': dist, 'net_cash_flow': dist - paid})
            metrics['cash_flow_ledger'] = ledger

        return {
            'success': True,
            'asset_type': 'private_capital',
            'method': 'metrics',
            'metrics': metrics
        }
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_real_estate(data: Dict[str, Any], method: str = 'noi') -> Dict[str, Any]:
    """Analyze real estate"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.REAL_ESTATE,
            ticker=data.get('ticker'),
            name=data.get('name'),
            currency=data.get('currency', 'USD')
        )

        for key in ['property_type', 'acquisition_price', 'current_market_value',
                    'gross_rental_income', 'operating_expenses', 'vacancy_rate', 'cap_rate']:
            if key in data:
                setattr(params, key, Decimal(str(data[key])) if isinstance(data[key], (int, float)) else data[key])

        analyzer = RealEstateAnalyzer(params)

        # Assumptions that used to be hidden constants (10 years, 6% exit cap, 8% discount, 3% growth)
        # are inputs now, with the same defaults.
        hold = int(round(float(data.get('hold_period_years', data.get('projection_years', 10)))))
        exit_cap = float(data.get('exit_cap_rate', data.get('terminal_cap_rate', 0.06)))
        discount = float(data.get('discount_rate', 0.08))
        growth = float(data.get('rent_growth', 0.03))

        if method == 'noi':
            noi = analyzer.calculate_noi()
            metrics = {'noi': float(noi)}
        elif method == 'caprate':
            cap_rate = analyzer.calculate_cap_rate()
            metrics = {'cap_rate': float(cap_rate) if cap_rate else None}
        elif method == 'dcf':
            metrics = analyzer.dcf_valuation(hold, Decimal(str(exit_cap)), Decimal(str(discount)),
                                             Decimal(str(growth)), include_schedule=True)
            if isinstance(metrics, dict) and 'error' not in metrics:
                metrics = {k: (float(v) if isinstance(v, Decimal) else v) for k, v in metrics.items()}
                price = float(analyzer.acquisition_price or 0)
                if price > 0:
                    from fincept_irr import irr as periodic_irr
                    flows = [-price] + [row['cash_flow'] for row in metrics['yearly_cash_flows']]
                    metrics['purchase_price'] = price
                    metrics['value_minus_price'] = metrics['dcf_value'] - price
                    metrics['unlevered_irr'] = periodic_irr(flows)
                metrics['assumptions'] = {'hold_period_years': hold, 'exit_cap_rate': exit_cap,
                                          'discount_rate': discount, 'rent_growth': growth}
        elif method == 'financing':
            metrics = analyzer.financing_analysis(
                float(data.get('loan_amount', 0)), float(data.get('interest_rate', 0.065)),
                float(data.get('loan_term', 30)), hold, exit_cap, growth)
        else:
            # summary: NOI, cap rate and operating ratios of the year-0 (entered) run rate.
            egi = (analyzer.gross_rental_income or Decimal('0')) * (Decimal('1') - analyzer.vacancy_rate)
            noi = analyzer.calculate_noi()
            cap_rate = analyzer.calculate_cap_rate()
            metrics = {
                'noi': float(noi),
                'cap_rate': float(cap_rate) if cap_rate else None,
                'effective_gross_income': float(egi),
                'vacancy_loss_amount': float((analyzer.gross_rental_income or Decimal('0')) - egi),
            }
            for key, value in analyzer.calculate_real_estate_ratios().items():
                metrics[key] = float(value)
            price = analyzer.acquisition_price
            if price and analyzer.gross_rental_income:
                metrics['gross_rent_multiplier'] = float(price / analyzer.gross_rental_income)
            method = 'summary'

        return {
            'success': True,
            'asset_type': 'real_estate',
            'method': method,
            'metrics': metrics
        }
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_international_reit(data: Dict[str, Any], method: str = 'diversification') -> Dict[str, Any]:
    """Analyze International REITs"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.REAL_ESTATE,
            name=data.get('name', 'International REIT'),
            currency=data.get('currency', 'EUR')
        )

        # Set International REIT specific parameters
        params.region = data.get('region', 'Europe')
        params.local_currency_return = Decimal(str(data.get('local_return', 0.05)))
        params.currency_return = Decimal(str(data.get('currency_return', 0.0)))
        params.expense_ratio = Decimal(str(data.get('expense_ratio', 0.005)))
        params.correlation_with_us = Decimal(str(data.get('correlation_us', 0.65)))

        # Optional REIT metrics
        if 'total_assets' in data:
            params.total_assets = Decimal(str(data['total_assets']))
        if 'total_debt' in data:
            params.total_debt = Decimal(str(data['total_debt']))
        if 'property_value' in data:
            params.property_value = Decimal(str(data['property_value']))

        analyzer = InternationalREITAnalyzer(params)

        if method == 'diversification':
            metrics = analyzer.correlation_benefit_analysis()
        elif method == 'currency':
            metrics = analyzer.currency_risk_analysis()
        elif method == 'expense':
            metrics = analyzer.expense_impact_analysis()
        elif method == 'regional':
            metrics = analyzer.regional_characteristics()
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'asset_type': 'international_reit', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_performance(returns: List, benchmark: List = None, method: str = 'twr') -> Dict[str, Any]:
    """Calculate performance metrics"""
    try:
        analyzer = PerformanceAnalyzer()
        handler = DataHandler()

        # Convert returns to MarketData format
        market_data = []
        for i, ret in enumerate(returns):
            if isinstance(ret, dict):
                market_data.append(ret)
            else:
                market_data.append({
                    # Plain values are MONTH-END PRICES, one per month (the old
                    # f'2024-{i+1:02d}-01' stamp broke past 12 points: month 13).
                    'timestamp': f'{2000 + i // 12:04d}-{i % 12 + 1:02d}-01',
                    'price': ret
                })

        prices = handler.standardize_price_data(market_data)

        if method == 'twr':
            metrics = analyzer.calculate_time_weighted_return(prices)
        elif method == 'sharpe':
            metrics = analyzer.calculate_sharpe_ratio(prices)
        elif method == 'sortino':
            metrics = analyzer.calculate_sortino_ratio(prices)
        else:
            metrics = analyzer.calculate_time_weighted_return(prices)

        return {
            'success': True,
            'analysis_type': 'performance',
            'method': method,
            'metrics': metrics
        }
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_risk(returns: List, method: str = 'var', confidence_level: float = 0.95) -> Dict[str, Any]:
    """Calculate risk metrics"""
    try:
        analyzer = RiskAnalyzer()

        # Convert to Decimal
        decimal_returns = [Decimal(str(r)) for r in returns]

        if method == 'var':
            metrics = analyzer.value_at_risk_analysis(
                decimal_returns,
                [Decimal('1') - Decimal(str(confidence_level))]
            )
        elif method == 'cvar':
            metrics = analyzer.conditional_var_analysis(
                decimal_returns,
                [Decimal('1') - Decimal(str(confidence_level))]
            )
        elif method == 'stress':
            metrics = analyzer.stress_testing(decimal_returns)
        else:
            metrics = analyzer.value_at_risk_analysis(
                decimal_returns,
                [Decimal('1') - Decimal(str(confidence_level))]
            )

        return {
            'success': True,
            'analysis_type': 'risk',
            'method': method,
            'confidence_level': confidence_level,
            'metrics': metrics
        }
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_tips(data: Dict[str, Any], method: str = 'real_yield') -> Dict[str, Any]:
    """Analyze TIPS (Treasury Inflation-Protected Securities)"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.FIXED_INCOME,
            name=data.get('name', 'TIPS'),
            currency=data.get('currency', 'USD')
        )

        for key in ['acquisition_price', 'coupon_rate', 'maturity_years', 'current_market_value']:
            if key in data:
                setattr(params, key, Decimal(str(data[key])))

        analyzer = TIPSAnalyzer(params)

        if method == 'real_yield':
            metrics = analyzer.calculate_real_yield(Decimal(str(data.get('current_price', 1000))))
        elif method == 'inflation_scenarios':
            scenarios = [Decimal(str(s)) for s in data.get('inflation_scenarios', [0.02, 0.03, 0.04])]
            metrics = analyzer.calculate_inflation_protection_value(scenarios)
        elif method == 'tax_efficiency':
            metrics = analyzer.tax_efficiency_analysis(
                Decimal(str(data.get('tax_rate_ordinary', 0.30))),
                Decimal(str(data.get('tax_rate_capital_gains', 0.15)))
            )
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'asset_type': 'tips', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_ibonds(data: Dict[str, Any], method: str = 'composite_rate') -> Dict[str, Any]:
    """Analyze I Bonds (Series I Savings Bonds)"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.FIXED_INCOME,
            name=data.get('name', 'I Bond'),
            currency='USD'  # I Bonds only USD
        )

        # Set I Bond specific parameters
        params.acquisition_price = Decimal(str(data.get('face_value', 10000)))
        params.fixed_rate = Decimal(str(data.get('fixed_rate', 0.0)))
        params.inflation_rate = Decimal(str(data.get('inflation_rate', 0.03)))
        params.years_held = data.get('years_held', 0)
        params.purchase_date = data.get('purchase_date', datetime.now().isoformat())

        analyzer = IBondAnalyzer(params)

        if method == 'composite_rate':
            metrics = {
                'composite_rate': float(analyzer.calculate_composite_rate()),
                'components': {
                    'fixed_rate': float(analyzer.fixed_rate),
                    'inflation_rate': float(analyzer.inflation_rate)
                },
                'current_value': float(analyzer.calculate_nav())
            }
        elif method == 'penalty':
            metrics = analyzer.penalty_analysis()
        elif method == 'compare_tips':
            tips_yield = Decimal(str(data.get('tips_yield', 0.02)))
            metrics = analyzer.compare_to_tips(tips_yield)
            # The margin, not only which one is higher.
            metrics['rate_difference_vs_tips'] = metrics['i_bond_rate'] - float(tips_yield)
        elif method == 'tax_efficiency':
            metrics = analyzer.tax_efficiency_analysis()
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'asset_type': 'ibonds', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_high_yield(data: Dict[str, Any], method: str = 'credit_analysis') -> Dict[str, Any]:
    """Analyze high-yield bonds"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.FIXED_INCOME,
            name=data.get('name', 'High Yield Bond'),
            currency=data.get('currency', 'USD')
        )

        for key in ['face_value', 'coupon_rate', 'maturity_years', 'current_market_value', 'credit_rating']:
            if key in data:
                value = data[key]
                setattr(params, key, Decimal(str(value)) if isinstance(value, (int, float)) else value)
        # The analyzer reads the par amount as `acquisition_price`; the entered par value never
        # reached the yield (it was always 1,000).
        if 'face_value' in data and 'acquisition_price' not in data:
            params.acquisition_price = Decimal(str(data['face_value']))
        # Rating tables are keyed by the letter grade: a notched rating (BB+, B-) used to fall
        # through to a generic 10% default rate and a 200-500 bp "typical" range.
        if isinstance(getattr(params, 'credit_rating', None), str):
            params.credit_rating = params.credit_rating.strip().upper().rstrip('+-')

        analyzer = HighYieldBondAnalyzer(params)

        if method == 'credit_analysis':
            metrics = analyzer.calculate_yield_spread(
                Decimal(str(data.get('treasury_yield', 0.04)))
            )
        elif method == 'default_prob':
            metrics = analyzer.estimate_default_probability()
        elif method == 'equity_behavior':
            equity_returns = [Decimal(str(r)) for r in data.get('equity_returns', [])]
            metrics = analyzer.equity_risk_analysis(equity_returns)
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'asset_type': 'high_yield', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_preferred_stocks(data: Dict[str, Any], method: str = 'yield_analysis') -> Dict[str, Any]:
    """Analyze preferred stocks"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.EQUITY,
            name=data.get('name', 'Preferred Stock'),
            currency=data.get('currency', 'USD')
        )

        # The analyzer reads par as `acquisition_price` and the price as `current_market_value` (the
        # screen's aliases); par_value / current_price were set under names it never reads, so the
        # yield was always the coupon rate on 25 / 25.
        for key in ['acquisition_price', 'dividend_rate', 'current_market_value', 'call_price']:
            if key in data:
                setattr(params, key, Decimal(str(data[key])))
        # First call date: an explicit date, else today + years_to_call (the analyzer only knew dates).
        call_date = str(data.get('call_date') or '').strip()[:10]
        years_to_call = data.get('years_to_call')
        if not call_date and isinstance(years_to_call, (int, float)) and years_to_call > 0:
            call_date = (datetime.now() + timedelta(days=float(years_to_call) * 365.25)).strftime('%Y-%m-%d')
        if call_date:
            params.call_date = call_date

        analyzer = PreferredStockAnalyzer(params)

        if method == 'yield_analysis':
            metrics = {
                'current_yield': float(analyzer.calculate_current_yield()),
                'annual_dividend': float(analyzer.dividend_rate * analyzer.par_value),
                'yield_to_call': analyzer.calculate_yield_to_call()
            }
        elif method == 'call_risk':
            metrics = analyzer.analyze_call_risk()
        elif method == 'dividend_safety':
            metrics = analyzer.analyze_dividend_suspension_risk()
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'asset_type': 'preferred_stocks', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_pme(data: Dict[str, Any], method: str = 'correlation') -> Dict[str, Any]:
    """Analyze precious metals equities"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.EQUITY,
            name=data.get('name', 'PME Index'),
            currency=data.get('currency', 'USD')
        )

        analyzer = PreciousMetalsEquityAnalyzer(params)

        # Add market data if provided
        if 'market_data' in data:
            handler = DataHandler()
            market_data = handler.standardize_price_data(data['market_data'])
            analyzer.add_market_data(market_data)

        if method == 'correlation':
            stock_returns = [Decimal(str(r)) for r in data.get('stock_returns', [])]
            inflation_rates = [Decimal(str(r)) for r in data.get('inflation_rates', [])]
            metrics = analyzer.correlation_analysis(stock_returns, inflation_rates)
        elif method == 'drawdowns':
            metrics = analyzer.calculate_drawdowns()
        elif method == 'crisis_performance':
            metrics = analyzer.crisis_performance_analysis()
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'asset_type': 'pme', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_convertible_bonds(data: Dict[str, Any], method: str = 'conversion_premium') -> Dict[str, Any]:
    """Analyze convertible bonds"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.FIXED_INCOME,
            name=data.get('name', 'Convertible Bond'),
            currency=data.get('currency', 'USD')
        )

        for key in ['face_value', 'coupon_rate', 'maturity_years', 'current_market_value',
                    'conversion_ratio', 'stock_price', 'credit_spread']:
            if key in data:
                setattr(params, key, Decimal(str(data[key])))

        analyzer = ConvertibleBondAnalyzer(params)

        # Yield of the issuer's straight (non-convertible) debt: given directly, or Treasury + the
        # issuer's credit spread. It prices the bond floor.
        if 'market_yield' in data:
            market_yield = Decimal(str(data['market_yield']))
        else:
            market_yield = (Decimal(str(data.get('treasury_yield', 0.04))) +
                            Decimal(str(data.get('credit_spread', 0.02))))

        if method == 'conversion_premium':
            metrics = analyzer.calculate_conversion_premium(Decimal(str(data.get('stock_price', 50))))
        elif method == 'bond_floor':
            metrics = analyzer.calculate_bond_floor(market_yield)
            metrics['market_yield'] = float(market_yield)
        elif method == 'upside_participation':
            # Scenarios around the ENTERED stock price (-30% .. +50%); a fixed 40/50/60/70 ladder was
            # used whatever the price was. The bond floor cushions the downside scenarios.
            if 'stock_price_scenarios' in data:
                scenarios = [Decimal(str(p)) for p in data['stock_price_scenarios']]
            else:
                s0 = Decimal(str(data.get('stock_price', analyzer.stock_price)))
                scenarios = [s0 * (Decimal('1') + Decimal(str(m))) for m in (-0.30, -0.15, 0.15, 0.30, 0.50)]
            metrics = analyzer.calculate_upside_participation(scenarios, market_yield)
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'asset_type': 'convertible_bonds', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_annuities(data: Dict[str, Any], method: str = 'payouts') -> Dict[str, Any]:
    """Analyze fixed annuities"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.ALTERNATIVE,
            name=data.get('name', 'Fixed Annuity'),
            currency=data.get('currency', 'USD')
        )

        for key in ['acquisition_price', 'annuity_rate', 'payout_years', 'surrender_charge_years',
                    'surrender_charge_rate', 'insurer_rating']:
            if key in data:
                value = data[key]
                setattr(params, key, Decimal(str(value)) if isinstance(value, (int, float)) else value)

        analyzer = FixedAnnuityAnalyzer(params)

        if method == 'payouts':
            metrics = analyzer.calculate_total_payouts()
            # The payout stream's own return (premium out, level payouts back) - the figure a
            # "guaranteed rate" field could only have restated.
            try:
                irr = analyzer.calculate_internal_rate_return()
                metrics['internal_rate_of_return'] = float(irr) if irr is not None else None
            except Exception:
                metrics['internal_rate_of_return'] = None
        elif method == 'inflation_erosion':
            metrics = analyzer.inflation_erosion_analysis(Decimal(str(data.get('inflation_rate', 0.03))))
        elif method == 'self_insurance':
            metrics = analyzer.compare_to_self_insurance(
                Decimal(str(data.get('alternative_return', 0.04))),
                Decimal(str(data.get('withdrawal_rate', 0.04)))
            )
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'asset_type': 'annuities', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_inflation_annuity(data: Dict[str, Any], method: str = 'compare_fixed') -> Dict[str, Any]:
    """Analyze inflation-indexed annuities"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.ALTERNATIVE,
            name=data.get('name', 'Inflation-Indexed Annuity'),
            currency=data.get('currency', 'USD')
        )

        for key in ['acquisition_price', 'real_payout_rate', 'inflation_rate', 'payout_years',
                    'surrender_charge_years', 'insurer_rating', 'age']:
            if key in data:
                value = data[key]
                setattr(params, key, Decimal(str(value)) if isinstance(value, (int, float)) else value)

        analyzer = InflationIndexedAnnuityAnalyzer(params)

        if method == 'compare_fixed':
            fixed_rate = Decimal(str(data.get('fixed_payout_rate', 0.055)))
            metrics = analyzer.compare_to_fixed_annuity(fixed_rate)
            # Totals over the payout term: the term is what decides which annuity pays more.
            infl = float(analyzer.inflation_rate)
            indexed0 = metrics['inflation_indexed_initial']
            fixed0 = metrics['fixed_annuity_initial']
            n = int(analyzer.payout_years)
            total_indexed = sum(indexed0 * (1 + infl) ** (y - 1) for y in range(1, n + 1))
            metrics['payout_term_years'] = n
            metrics['total_indexed_payouts'] = total_indexed
            metrics['total_fixed_payouts'] = fixed0 * n
            metrics['indexed_minus_fixed_over_term'] = total_indexed - fixed0 * n
        elif method == 'compare_tips':
            tips_yield = Decimal(str(data.get('tips_real_yield', 0.02)))
            metrics = analyzer.compare_to_tips_ladder(tips_yield)
        elif method == 'longevity':
            metrics = analyzer.longevity_break_even()
        elif method == 'inflation_value':
            metrics = analyzer.inflation_protection_value()
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'asset_type': 'inflation_annuity', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_em_bonds(data: Dict[str, Any], method: str = 'yield_spread') -> Dict[str, Any]:
    """Analyze emerging market bonds"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.FIXED_INCOME,
            name=data.get('name', 'EM Bond'),
            currency=data.get('currency', 'USD')
        )

        for key in ['face_value', 'coupon_rate', 'maturity_years', 'current_market_value',
                    'sovereign_rating', 'credit_spread', 'country']:
            if key in data:
                value = data[key]
                setattr(params, key, Decimal(str(value)) if isinstance(value, (int, float)) else value)

        if method == 'currency_risk' and not data.get('currency'):
            # The screen offers this method for local-currency bonds (a hard-currency bond has no
            # FX risk to measure).
            params.currency = 'LOCAL'

        analyzer = EmergingMarketBondAnalyzer(params)

        if method == 'yield_spread':
            metrics = analyzer.calculate_yield_metrics(
                Decimal(str(data['treasury_yield'])) if 'treasury_yield' in data else None)
        elif method == 'default_risk':
            # Annual default probability: given, else from the sovereign rating (the 5-year cumulative
            # rates the high-yield analyzer uses, annualised). A fixed 3% was used for every rating.
            if 'historical_default_rate' in data:
                pd_annual = Decimal(str(data['historical_default_rate']))
            else:
                five_year = {'AAA': 0.0001, 'AA': 0.0005, 'A': 0.0015, 'BBB': 0.005, 'BB': 0.025,
                             'B': 0.08, 'CCC': 0.25, 'CC': 0.40, 'C': 0.60}
                grade = str(data.get('sovereign_rating', 'BB')).strip().upper().rstrip('+-')
                c5 = five_year.get(grade, 0.10)
                pd_annual = Decimal(str(1.0 - (1.0 - c5) ** (1.0 / 5.0)))
            metrics = analyzer.sovereign_default_risk_analysis(
                pd_annual,
                Decimal(str(data.get('recovery_rate', 0.30)))
            )
        elif method == 'currency_risk':
            metrics = analyzer.currency_risk_analysis(
                Decimal(str(data.get('local_currency_volatility', 0.15))),
                Decimal(str(data.get('fx_correlation', -0.30)))
            )
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'asset_type': 'em_bonds', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_managed_futures(data: Dict[str, Any], method: str = 'trend_following') -> Dict[str, Any]:
    """Analyze managed futures / CTAs"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.HEDGE_FUND,
            name=data.get('name', 'Managed Futures Fund'),
            currency=data.get('currency', 'USD')
        )

        # Set defaults first
        params.management_fee = Decimal('0.02')
        params.performance_fee = Decimal('0.20')
        params.hurdle_rate = Decimal('0.0')
        params.strategy_type = 'Systematic Trend-Following'

        # Override with provided values
        for key in ['strategy_type', 'management_fee', 'performance_fee', 'hurdle_rate']:
            if key in data:
                value = data[key]
                setattr(params, key, Decimal(str(value)) if isinstance(value, (int, float)) else value)

        analyzer = ManagedFuturesAnalyzer(params)

        # Add market data if provided (can be prices or returns)
        if 'market_data' in data:
            handler = DataHandler()
            market_data = handler.standardize_price_data(data['market_data'])
            analyzer.add_market_data(market_data)
        elif 'returns' in data:
            # Convert returns to price series
            returns = [Decimal(str(r)) for r in data['returns']]
            prices = [Decimal('100')]  # Start at 100
            for ret in returns:
                prices.append(prices[-1] * (Decimal('1') + ret))

            # Create market data from prices
            from datetime import datetime, timedelta
            market_data = []
            base_date = datetime.now()
            for i, price in enumerate(prices):
                market_data.append(MarketData(
                    timestamp=(base_date + timedelta(days=i)).isoformat(),
                    price=price,
                    volume=Decimal('0')
                ))
            analyzer.add_market_data(market_data)

        # Add crisis periods if provided
        if 'crisis_periods' in data:
            for crisis in data['crisis_periods']:
                analyzer.add_crisis_period(
                    name=crisis.get('name', crisis.get('start', 'Crisis')),
                    start_date=crisis.get('start', ''),
                    end_date=crisis.get('end', ''),
                    cta_return=Decimal(str(crisis.get('fund_return', 0))),
                    stock_return=Decimal(str(crisis.get('market_return', 0))),
                    bond_return=Decimal(str(crisis.get('bond_return', 0)))
                )

        if method == 'trend_following':
            price_series = [Decimal(str(p)) for p in data.get('price_series', [])]
            metrics = analyzer.trend_following_analysis(price_series)
        elif method == 'crisis_alpha':
            metrics = analyzer.crisis_alpha_analysis()
        elif method == 'fee_impact':
            metrics = analyzer.fee_impact_analysis(
                Decimal(str(data.get('gross_return', 0.10))),
                int(round(float(data.get('years', 10))))
            )
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'asset_type': 'managed_futures', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_market_neutral(data: Dict[str, Any], method: str = 'beta_analysis') -> Dict[str, Any]:
    """Analyze market-neutral funds"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.HEDGE_FUND,
            name=data.get('name', 'Market Neutral Fund'),
            currency=data.get('currency', 'USD')
        )

        for key in ['long_exposure', 'short_exposure', 'management_fee', 'performance_fee']:
            if key in data:
                setattr(params, key, Decimal(str(data[key])))
        # GROSS LEVERAGE alone (older callers): a market-neutral book split evenly long / short.
        if 'gross_leverage' in data and 'long_exposure' not in data and 'short_exposure' not in data:
            half = Decimal(str(data['gross_leverage'])) / Decimal('2')
            params.long_exposure = half
            params.short_exposure = half

        analyzer = MarketNeutralAnalyzer(params)

        # Add market data if provided (can be prices or returns)
        if 'market_data' in data:
            handler = DataHandler()
            market_data = handler.standardize_price_data(data['market_data'])
            analyzer.add_market_data(market_data)
        elif 'returns' in data:
            # Convert returns to price series
            returns = [Decimal(str(r)) for r in data['returns']]
            prices = [Decimal('100')]  # Start at 100
            for ret in returns:
                prices.append(prices[-1] * (Decimal('1') + ret))

            # Create market data from prices
            from datetime import datetime, timedelta
            market_data = []
            base_date = datetime.now()
            for i, price in enumerate(prices):
                market_data.append(MarketData(
                    timestamp=(base_date + timedelta(days=i)).isoformat(),
                    price=price,
                    volume=Decimal('0')
                ))
            analyzer.add_market_data(market_data)

        if method == 'beta_analysis':
            market_returns = [Decimal(str(r)) for r in data.get('market_returns', [])]
            metrics = analyzer.calculate_beta(market_returns)
        elif method == 'factor_exposure':
            value_returns = [Decimal(str(r)) for r in data.get('value_returns', [])]
            size_returns = [Decimal(str(r)) for r in data.get('size_returns', [])]
            momentum_returns = [Decimal(str(r)) for r in data.get('momentum_returns', [])]
            metrics = analyzer.factor_exposure_analysis(value_returns, size_returns, momentum_returns)
        elif method == 'leverage_risk':
            metrics = analyzer.leverage_risk_analysis()
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'asset_type': 'market_neutral', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_stable_value(data: Dict[str, Any], method: str = 'market_to_book') -> Dict[str, Any]:
    """Analyze stable value funds"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.FIXED_INCOME,
            name=data.get('name', 'Stable Value Fund'),
            currency=data.get('currency', 'USD')
        )

        for key in ['acquisition_price', 'current_market_value', 'crediting_rate', 'wrap_provider',
                    'wrap_fee', 'portfolio_duration', 'portfolio_yield']:
            if key in data:
                value = data[key]
                setattr(params, key, Decimal(str(value)) if isinstance(value, (int, float)) else value)

        analyzer = StableValueFundAnalyzer(params)

        if method == 'market_to_book':
            metrics = analyzer.calculate_market_to_book_ratio()
        elif method == 'crediting_rate':
            metrics = analyzer.crediting_rate_analysis(
                Decimal(str(data.get('treasury_yield', 0.04))),
                Decimal(str(data.get('credit_spread', 0.01)))
            )
        elif method == 'suitability':
            metrics = analyzer.suitability_analysis(
                data.get('investor_age', 50),
                data.get('retirement_age', 65),
                data.get('risk_tolerance', 'moderate')
            )
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'asset_type': 'stable_value', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_eia(data: Dict[str, Any], method: str = 'crediting') -> Dict[str, Any]:
    """Analyze equity-indexed annuities"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.ALTERNATIVE,
            name=data.get('name', 'Equity-Indexed Annuity'),
            currency=data.get('currency', 'USD')
        )

        for key in ['acquisition_price', 'participation_rate', 'cap_rate', 'floor_rate', 'spread',
                    'term_years', 'surrender_charge_years', 'initial_surrender_charge', 'insurer', 'insurer_rating']:
            if key in data:
                value = data[key]
                setattr(params, key, Decimal(str(value)) if isinstance(value, (int, float)) else value)
        # Year counts are loop bounds in the analyzer (a Decimal broke range()).
        for key in ('surrender_charge_years', 'term_years'):
            if key in data:
                setattr(params, key, max(1, int(round(float(data[key])))))

        analyzer = EquityIndexedAnnuityAnalyzer(params)

        if method == 'crediting':
            metrics = analyzer.calculate_credited_return(Decimal(str(data.get('index_return', 0.10))))
        elif method == 'upside_limitation':
            # Down years are included so the floor shows what it protects (only up years were used,
            # where the floor can never bind).
            scenarios = [Decimal(str(s)) for s in
                         data.get('market_scenarios', [-0.20, -0.10, 0.05, 0.10, 0.15, 0.20, 0.30])]
            metrics = analyzer.upside_limitation_analysis(scenarios)
        elif method == 'surrender_charges':
            metrics = analyzer.surrender_charge_schedule()
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'asset_type': 'eia', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_asset_location(data: Dict[str, Any], method: str = 'optimal',
                           tax_bracket: Decimal = Decimal('0.24')) -> Dict[str, Any]:
    """Analyze tax-efficient asset location"""
    try:
        analyzer = AssetLocationAnalyzer(tax_bracket=tax_bracket)

        if method == 'optimal':
            # Single asset optimal location
            asset_class = data.get('asset_class', 'reits')
            metrics = analyzer.optimal_location(asset_class)

        elif method == 'value_added':
            # Calculate value of optimal location
            asset_class = data.get('asset_class', 'reits')
            asset_value = Decimal(str(data.get('value', 100000)))
            years = int(round(float(data.get('years', 30))))  # an exponent: a float broke Decimal ** years
            metrics = analyzer.location_value_added(asset_value, asset_class, years)

        elif method == 'portfolio':
            # Portfolio-wide location strategy
            portfolio = data.get('portfolio', [])
            metrics = analyzer.portfolio_location_strategy(portfolio)

        elif method == 'muni_bond':
            # Municipal vs taxable bond decision
            muni_yield = Decimal(str(data.get('municipal_yield', 0.03)))
            taxable_yield = Decimal(str(data.get('taxable_yield', 0.04)))
            metrics = analyzer.municipal_bond_decision(muni_yield, taxable_yield)

        elif method == 'foreign_tax_credit':
            # Foreign tax credit analysis
            foreign_div_yield = Decimal(str(data.get('foreign_dividend_yield', 0.02)))
            metrics = analyzer.foreign_tax_credit_analysis(foreign_div_yield)

        else:
            metrics = analyzer.analysis_verdict()

        return {'success': True, 'analysis_type': 'asset_location', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_covered_calls(data: Dict[str, Any], method: str = 'tax') -> Dict[str, Any]:
    """Analyze covered call strategy"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.ALTERNATIVE,
            name=data.get('name', 'Covered Call Position'),
            currency=data.get('currency', 'USD')
        )

        for key in ['stock_price', 'shares_owned', 'strike_price', 'option_premium',
                    'days_to_expiration', 'option_commission', 'ordinary_tax_rate',
                    'ltcg_rate', 'holding_period_days', 'cost_basis']:
            if key in data:
                value = data[key]
                setattr(params, key, Decimal(str(value)) if isinstance(value, (int, float)) else value)

        analyzer = CoveredCallAnalyzer(params)

        if method == 'tax':
            metrics = analyzer.tax_consequences()
        elif method == 'opportunity_cost':
            metrics = analyzer.opportunity_cost_analysis()
        elif method == 'alternative':
            metrics = analyzer.better_alternative()
        elif method == 'verdict':
            metrics = analyzer.analysis_verdict()
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'strategy': 'covered_calls', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_sri(data: Dict[str, Any], method: str = 'performance') -> Dict[str, Any]:
    """Analyze SRI fund"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.EQUITY,
            name=data.get('name', 'SRI Fund'),
            currency=data.get('currency', 'USD')
        )

        for key in ['expense_ratio', 'benchmark_expense', 'fund_return', 'benchmark_return',
                    'num_holdings', 'benchmark_holdings', 'excluded_sectors_pct']:
            if key in data:
                value = data[key]
                setattr(params, key, Decimal(str(value)) if isinstance(value, (int, float)) else value)

        if 'negative_screens' in data:
            params.negative_screens = data['negative_screens']
        if 'positive_screens' in data:
            params.positive_screens = data['positive_screens']

        analyzer = SRIFundAnalyzer(params)

        if method == 'performance':
            # Over the entered period (a fixed 10 years was used whatever TIME PERIOD said).
            metrics = analyzer.performance_comparison(int(round(float(data.get('time_period_years', 10)))))
        elif method == 'screening':
            metrics = analyzer.screening_impact_analysis()
        elif method == 'expenses':
            metrics = analyzer.expense_ratio_analysis()
        elif method == 'approaches':
            metrics = analyzer.compare_sri_approaches()
        elif method == 'verdict':
            metrics = analyzer.analysis_verdict()
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'fund_type': 'sri', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_leveraged_funds(data: Dict[str, Any], method: str = 'decay') -> Dict[str, Any]:
    """Analyze leveraged funds (2x, 3x ETFs)"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.EQUITY,
            name=data.get('name', 'Leveraged Fund'),
            currency=data.get('currency', 'USD')
        )

        for key in ['daily_volatility', 'expense_ratio']:
            if key in data:
                setattr(params, key, Decimal(str(data[key])))
        # The analyzer reads `leverage_ratio`; this used to set `leverage_multiple`
        # only, so every fund was analysed as 2x whatever the input said.
        leverage = data.get('leverage_ratio', data.get('leverage_multiple'))
        if leverage is not None:
            params.leverage_ratio = Decimal(str(leverage))

        analyzer = LeveragedFundAnalyzer(params)

        if method in ('decay', 'volatility'):
            metrics = analyzer.volatility_decay_example()
            # Expected drag for THIS fund from the entered index return / volatility (the two fixed
            # paths above only illustrate it). Daily-rebalanced L x fund, lognormal index:
            #   E[log growth] = L*mu - L^2*sigma^2/2 - costs  vs  L * (mu - sigma^2/2) for L x the index,
            # so the volatility drag is (L^2 - L) * sigma^2 / 2 per year.
            if 'index_volatility' in data or 'index_return' in data:
                lev = float(analyzer.leverage_ratio)
                mu = float(data.get('index_return', 0.08))
                sigma = float(data.get('index_volatility', 0.18))
                years = max(1.0, float(data.get('holding_years', 1)))
                cost = float(data.get('expense_ratio', analyzer.expense_ratio))
                drift = math.log(1.0 + mu) + sigma ** 2 / 2.0   # arithmetic drift of the index
                index_log = drift - sigma ** 2 / 2.0
                fund_log = lev * drift - (lev ** 2) * sigma ** 2 / 2.0 - cost
                drag = (lev ** 2 - lev) * sigma ** 2 / 2.0
                metrics['expected_path'] = {
                    'holding_years': years,
                    'index_annual_return': mu,
                    'index_volatility': sigma,
                    'expense_ratio': cost,
                    'volatility_drag_annual': drag,
                    'index_total_return': math.exp(index_log * years) - 1.0,
                    'leverage_times_index_return': lev * (math.exp(index_log * years) - 1.0),
                    'expected_fund_total_return': math.exp(fund_log * years) - 1.0,
                    'expected_fund_annual_return': math.exp(fund_log) - 1.0,
                }
        elif method == 'verdict':
            metrics = analyzer.analysis_verdict()
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'product_type': 'leveraged_fund', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_structured_products(data: Dict[str, Any], method: str = 'complexity') -> Dict[str, Any]:
    """Analyze structured investment products"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.ALTERNATIVE,
            name=data.get('name', 'Structured Product'),
            currency=data.get('currency', 'USD')
        )

        for key in ['principal', 'participation_rate', 'cap_rate', 'maturity_years']:
            if key in data:
                setattr(params, key, Decimal(str(data[key])))

        analyzer = StructuredProductAnalyzer(params)

        if method == 'complexity':
            metrics = analyzer.analysis_verdict()
        elif method == 'costs':
            metrics = analyzer.analysis_verdict()
        elif method == 'verdict':
            metrics = analyzer.analysis_verdict()
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'product_type': 'structured_product', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_variable_annuities(data: Dict[str, Any], method: str = 'fees') -> Dict[str, Any]:
    """Analyze variable annuities"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.ALTERNATIVE,
            name=data.get('name', 'Variable Annuity'),
            currency=data.get('currency', 'USD')
        )

        # The analyzer's own attribute names (the screen's me_fee / investment_fee / surrender_period
        # are aliased to them): under the old names the M&E and fund fees and the surrender period
        # were never read and 1.25% / 0.75% / 7 years were reported whatever was entered.
        for key in ['premium', 'mortality_expense_fee', 'investment_mgmt_fee', 'admin_fee',
                    'guaranteed_withdrawal_benefit', 'death_benefit_rider', 'initial_surrender_charge',
                    'ordinary_tax_rate', 'ltcg_rate']:
            if key in data and isinstance(data[key], (int, float)):
                setattr(params, key, Decimal(str(data[key])))
        if 'surrender_years' in data:
            params.surrender_years = max(0, int(round(float(data['surrender_years']))))

        analyzer = VariableAnnuityAnalyzer(params)

        if method == 'fees':
            metrics = analyzer.total_annual_cost()
        elif method == 'tax':
            years = int(round(float(data.get('years', 20))))
            gross_return = Decimal(str(data.get('gross_return', 0.08)))
            metrics = analyzer.tax_deferral_myth(years, gross_return)
        elif method == 'alternatives':
            metrics = analyzer.compare_to_alternatives()
        elif method == 'surrender':
            if analyzer.surrender_years < 1:
                return {'success': False, 'error': 'Enter a surrender period of at least one year'}
            metrics = analyzer.surrender_charge_schedule()
        elif method == 'verdict':
            metrics = analyzer.analysis_verdict()
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'product_type': 'variable_annuity', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


# Commands that take `--data` and go through run_command (the screen's analyzers).
_SCREEN_ANALYZERS = {
    'digital-assets': analyze_digital_assets,
    'hedge-funds': analyze_hedge_funds,
    'natural-resources': analyze_natural_resources,
    'private-capital': analyze_private_capital,
    'real-estate': analyze_real_estate,
    'tips': analyze_tips,
    'ibonds': analyze_ibonds,
    'high-yield': analyze_high_yield,
    'preferred-stocks': analyze_preferred_stocks,
    'convertible-bonds': analyze_convertible_bonds,
    'annuities': analyze_annuities,
    'inflation-annuity': analyze_inflation_annuity,
    'em-bonds': analyze_em_bonds,
    'managed-futures': analyze_managed_futures,
    'market-neutral': analyze_market_neutral,
    'stable-value': analyze_stable_value,
    'eia': analyze_eia,
    'asset-location': analyze_asset_location,
    'covered-calls': analyze_covered_calls,
    'sri': analyze_sri,
    'leveraged-funds': analyze_leveraged_funds,
    'structured-products': analyze_structured_products,
    'variable-annuities': analyze_variable_annuities,
}


if __name__ == '__main__':
    main()
