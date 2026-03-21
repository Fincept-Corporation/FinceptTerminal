"""Alternative Investments CLI

Unified command-line interface for alternative investment analytics.
"""

import sys
import json
import argparse
from decimal import Decimal
from typing import Dict, Any, List
from datetime import datetime, timedelta

from config import AssetClass, HedgeFundStrategy, CommoditySector, RealEstateType, AssetParameters, MarketData
from digital_assets import DigitalAssetAnalyzer
from hedge_funds import HedgeFundAnalyzer
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


def main():
    parser = argparse.ArgumentParser(description='Alternative Investments Analytics CLI')
    subparsers = parser.add_subparsers(dest='command', help='Available commands')

    # Digital Assets
    digital = subparsers.add_parser('digital-assets', help='Digital asset analytics')
    digital.add_argument('--data', required=True, help='Asset data (JSON)')
    digital.add_argument('--method', default='fundamental', help='Analysis method: fundamental, volatility, onchain')

    # Hedge Funds
    hedge = subparsers.add_parser('hedge-funds', help='Hedge fund analytics')
    hedge.add_argument('--data', required=True, help='Fund data (JSON)')
    hedge.add_argument('--method', default='metrics', help='Analysis method: metrics, performance, fees')

    # Natural Resources
    natural = subparsers.add_parser('natural-resources', help='Natural resource analytics')
    natural.add_argument('--data', required=True, help='Resource data (JSON)')
    natural.add_argument('--method', default='basis', help='Analysis method: basis, contango, futures')

    # Private Capital
    private = subparsers.add_parser('private-capital', help='Private capital analytics')
    private.add_argument('--data', required=True, help='Investment data (JSON)')
    private.add_argument('--method', default='metrics', help='Analysis method: metrics, irr, moic')

    # Real Estate
    real_estate = subparsers.add_parser('real-estate', help='Real estate analytics')
    real_estate.add_argument('--data', required=True, help='Property data (JSON)')
    real_estate.add_argument('--method', default='noi', help='Analysis method: noi, caprate, dcf')

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
    var_annuity.add_argument('--method', default='fees', help='Analysis method: fees, tax, alternatives, verdict')

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        return

    try:
        result = None

        if args.command == 'digital-assets':
            data = json.loads(args.data)
            result = analyze_digital_assets(data, args.method)

        elif args.command == 'hedge-funds':
            data = json.loads(args.data)
            result = analyze_hedge_funds(data, args.method)

        elif args.command == 'natural-resources':
            data = json.loads(args.data)
            result = analyze_natural_resources(data, args.method)

        elif args.command == 'private-capital':
            data = json.loads(args.data)
            result = analyze_private_capital(data, args.method)

        elif args.command == 'real-estate':
            data = json.loads(args.data)
            result = analyze_real_estate(data, args.method)

        elif args.command == 'intl-reit':
            data = json.loads(args.data)
            result = analyze_international_reit(data, args.method)

        elif args.command == 'performance':
            returns = json.loads(args.returns)
            benchmark = json.loads(args.benchmark) if args.benchmark else None
            result = analyze_performance(returns, benchmark, args.method)

        elif args.command == 'risk':
            returns = json.loads(args.returns)
            result = analyze_risk(returns, args.method, args.confidence_level)

        elif args.command == 'tips':
            data = json.loads(args.data)
            result = analyze_tips(data, args.method)

        elif args.command == 'ibonds':
            data = json.loads(args.data)
            result = analyze_ibonds(data, args.method)

        elif args.command == 'high-yield':
            data = json.loads(args.data)
            result = analyze_high_yield(data, args.method)

        elif args.command == 'preferred-stocks':
            data = json.loads(args.data)
            result = analyze_preferred_stocks(data, args.method)

        elif args.command == 'pme':
            data = json.loads(args.data)
            result = analyze_pme(data, args.method)

        elif args.command == 'convertible-bonds':
            data = json.loads(args.data)
            result = analyze_convertible_bonds(data, args.method)

        elif args.command == 'annuities':
            data = json.loads(args.data)
            result = analyze_annuities(data, args.method)

        elif args.command == 'inflation-annuity':
            data = json.loads(args.data)
            result = analyze_inflation_annuity(data, args.method)

        elif args.command == 'em-bonds':
            data = json.loads(args.data)
            result = analyze_em_bonds(data, args.method)

        elif args.command == 'managed-futures':
            data = json.loads(args.data)
            result = analyze_managed_futures(data, args.method)

        elif args.command == 'market-neutral':
            data = json.loads(args.data)
            result = analyze_market_neutral(data, args.method)

        elif args.command == 'stable-value':
            data = json.loads(args.data)
            result = analyze_stable_value(data, args.method)

        elif args.command == 'eia':
            data = json.loads(args.data)
            result = analyze_eia(data, args.method)

        elif args.command == 'asset-location':
            data = json.loads(args.data)
            result = analyze_asset_location(data, args.method, Decimal(str(args.tax_bracket)))

        elif args.command == 'covered-calls':
            data = json.loads(args.data)
            result = analyze_covered_calls(data, args.method)

        elif args.command == 'sri':
            data = json.loads(args.data)
            result = analyze_sri(data, args.method)

        elif args.command == 'leveraged-funds':
            data = json.loads(args.data)
            result = analyze_leveraged_funds(data, args.method)

        elif args.command == 'structured-products':
            data = json.loads(args.data)
            result = analyze_structured_products(data, args.method)

        elif args.command == 'variable-annuities':
            data = json.loads(args.data)
            result = analyze_variable_annuities(data, args.method)

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

        # Execute requested analysis
        if method == 'fundamental':
            metrics = analyzer.fundamental_metrics()
        elif method == 'volatility':
            metrics = analyzer.calculate_volatility_metrics()
        elif method == 'onchain':
            metrics = analyzer.onchain_metrics()
        else:
            metrics = analyzer.fundamental_metrics()

        return {
            'success': True,
            'asset_type': 'digital_assets',
            'method': method,
            'metrics': metrics
        }
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_hedge_funds(data: Dict[str, Any], method: str = 'metrics') -> Dict[str, Any]:
    """Analyze hedge funds"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.HEDGE_FUND,
            ticker=data.get('ticker'),
            name=data.get('name'),
            currency=data.get('currency', 'USD')
        )

        for key in ['strategy', 'gross_exposure', 'net_exposure', 'leverage',
                    'high_water_mark', 'hurdle_rate', 'redemption_frequency', 'lock_up_period']:
            if key in data:
                setattr(params, key, data[key])

        analyzer = HedgeFundAnalyzer(params)

        if 'market_data' in data:
            handler = DataHandler()
            market_data = handler.standardize_price_data(data['market_data'])
            analyzer.add_market_data(market_data)

        if method == 'metrics':
            metrics = analyzer.calculate_strategy_metrics()
        elif method == 'performance':
            metrics = analyzer.calculate_performance()
        elif method == 'fees':
            metrics = analyzer.calculate_fee_impact(
                data.get('months', 12),
                data.get('management_fee', 0.02),
                data.get('performance_fee', 0.20)
            )
        else:
            metrics = analyzer.calculate_strategy_metrics()

        return {
            'success': True,
            'asset_type': 'hedge_funds',
            'method': method,
            'metrics': metrics
        }
    except Exception as e:
        return {'success': False, 'error': str(e)}


def analyze_natural_resources(data: Dict[str, Any], method: str = 'basis') -> Dict[str, Any]:
    """Analyze natural resources"""
    try:
        params = AssetParameters(
            asset_class=AssetClass.COMMODITIES,
            ticker=data.get('ticker'),
            name=data.get('name'),
            currency=data.get('currency', 'USD')
        )

        for key in ['commodity_sector', 'spot_price', 'futures_prices',
                    'storage_cost', 'convenience_yield', 'contract_size']:
            if key in data:
                setattr(params, key, data[key])

        analyzer = CommodityAnalyzer(params)

        if method == 'basis':
            metrics = analyzer.calculate_futures_basis(
                Decimal(str(data.get('futures_price', 100))),
                data.get('expiry_months', 3)
            )
        elif method == 'contango':
            metrics = analyzer.analyze_contango_backwardation()
        elif method == 'futures':
            metrics = analyzer.calculate_theoretical_futures_price(
                data.get('time_to_expiry_years', 0.25)
            )
        else:
            metrics = analyzer.calculate_futures_basis(
                Decimal(str(data.get('futures_price', 100))),
                data.get('expiry_months', 3)
            )

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

        # Add cash flows if provided
        if 'cash_flows' in data:
            handler = DataHandler()
            cash_flows = handler.standardize_cash_flows(data['cash_flows'])
            analyzer.add_cash_flows(cash_flows)

        # Update NAV if provided
        if 'current_nav' in data:
            analyzer.update_nav(
                Decimal(str(data['current_nav'])),
                data.get('nav_date', '2024-12-31')
            )

        if method == 'metrics':
            metrics = analyzer.calculate_key_metrics()
        elif method == 'irr':
            metrics = {'irr': float(analyzer.calculate_irr()) if analyzer.calculate_irr() else None}
        elif method == 'moic':
            metrics = analyzer.calculate_moic()
        else:
            metrics = analyzer.calculate_key_metrics()

        return {
            'success': True,
            'asset_type': 'private_capital',
            'method': method,
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

        if method == 'noi':
            noi = analyzer.calculate_noi()
            metrics = {'noi': float(noi)}
        elif method == 'caprate':
            cap_rate = analyzer.calculate_cap_rate()
            metrics = {'cap_rate': float(cap_rate) if cap_rate else None}
        elif method == 'dcf':
            metrics = analyzer.dcf_valuation(
                data.get('projection_years', 10),
                Decimal(str(data.get('terminal_cap_rate', 0.06))),
                Decimal(str(data.get('discount_rate', 0.08)))
            )
        else:
            noi = analyzer.calculate_noi()
            metrics = {'noi': float(noi)}

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
                    'timestamp': f'2024-{i+1:02d}-01',
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
                [Decimal(str(1 - confidence_level))]
            )
        elif method == 'cvar':
            metrics = analyzer.conditional_var_analysis(
                decimal_returns,
                [Decimal(str(1 - confidence_level))]
            )
        elif method == 'stress':
            metrics = analyzer.stress_testing(decimal_returns)
        else:
            metrics = analyzer.value_at_risk_analysis(
                decimal_returns,
                [Decimal(str(1 - confidence_level))]
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

        for key in ['par_value', 'dividend_rate', 'current_price', 'call_price', 'years_to_call']:
            if key in data:
                setattr(params, key, Decimal(str(data[key])))

        analyzer = PreferredStockAnalyzer(params)

        if method == 'yield_analysis':
            metrics = analyzer.calculate_yields()
        elif method == 'call_risk':
            metrics = analyzer.analyze_call_risk()
        elif method == 'dividend_safety':
            metrics = analyzer.dividend_suspension_risk_analysis(
                data.get('issuer_credit_rating', 'BBB'),
                Decimal(str(data.get('interest_coverage', 3.0)))
            )
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

        if method == 'conversion_premium':
            metrics = analyzer.calculate_conversion_premium(Decimal(str(data.get('stock_price', 50))))
        elif method == 'bond_floor':
            metrics = analyzer.calculate_bond_floor(Decimal(str(data.get('market_yield', 0.06))))
        elif method == 'upside_participation':
            scenarios = [Decimal(str(p)) for p in data.get('stock_price_scenarios', [40, 50, 60, 70])]
            metrics = analyzer.calculate_upside_participation(scenarios)
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

        analyzer = EmergingMarketBondAnalyzer(params)

        if method == 'yield_spread':
            metrics = analyzer.calculate_yield_metrics()
        elif method == 'default_risk':
            metrics = analyzer.sovereign_default_risk_analysis(
                Decimal(str(data.get('historical_default_rate', 0.03))),
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
                data.get('years', 10)
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

        analyzer = EquityIndexedAnnuityAnalyzer(params)

        if method == 'crediting':
            metrics = analyzer.calculate_credited_return(Decimal(str(data.get('index_return', 0.10))))
        elif method == 'upside_limitation':
            scenarios = [Decimal(str(s)) for s in data.get('market_scenarios', [0.05, 0.10, 0.15, 0.20, 0.30])]
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
            years = data.get('years', 30)
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
                    'ltcg_rate', 'holding_period_days']:
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
            metrics = analyzer.performance_comparison()
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

        for key in ['leverage_multiple', 'daily_volatility', 'expense_ratio']:
            if key in data:
                setattr(params, key, Decimal(str(data[key])))

        analyzer = LeveragedFundAnalyzer(params)

        if method == 'decay':
            metrics = analyzer.volatility_decay_analysis(
                data.get('days', 252),
                Decimal(str(data.get('volatility', 0.20)))
            )
        elif method == 'volatility':
            metrics = analyzer.amplification_analysis()
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
            metrics = analyzer.complexity_analysis()
        elif method == 'costs':
            metrics = analyzer.hidden_costs_analysis()
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

        for key in ['premium', 'me_fee', 'investment_fee', 'admin_fee', 'surrender_period']:
            if key in data:
                setattr(params, key, Decimal(str(data[key])) if isinstance(data[key], (int, float)) else data[key])

        analyzer = VariableAnnuityAnalyzer(params)

        if method == 'fees':
            metrics = analyzer.fee_analysis()
        elif method == 'tax':
            metrics = analyzer.tax_efficiency_analysis()
        elif method == 'alternatives':
            metrics = analyzer.compare_to_alternatives(
                Decimal(str(data.get('alternative_return', 0.08))),
                Decimal(str(data.get('alternative_fee', 0.0020)))
            )
        elif method == 'verdict':
            metrics = analyzer.analysis_verdict()
        else:
            metrics = analyzer.calculate_key_metrics()

        return {'success': True, 'product_type': 'variable_annuity', 'method': method, 'metrics': metrics}
    except Exception as e:
        return {'success': False, 'error': str(e)}


if __name__ == '__main__':
    main()
