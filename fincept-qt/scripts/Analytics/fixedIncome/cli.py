"""
Fixed Income Analytics CLI
==========================

Command-line interface for all fixed income analytics modules.
Provides unified access to bond pricing, duration, yield curve,
credit analysis, structured products, and portfolio analytics.

Usage:
    python cli.py <command> [params_json]

Commands:
    bond_pricing    - Bond valuation and yield calculations
    duration        - Duration and convexity analysis
    yield_curve     - Term structure and spread analysis
    credit          - Credit risk analysis
    structured      - MBS/ABS analysis
    portfolio       - Portfolio analytics and immunization
    bond_features   - Bond types, covenants, contingencies
    market_structure - Market segments, repos, indexes
    floating_rate   - FRN pricing, money market instruments
    sovereign       - Sovereign and municipal credit analysis
    list            - List all available commands
"""

import sys
import json
import logging
from typing import Dict, Any

# Import all modules
from bond_pricing import run_bond_pricing_analysis
from duration_convexity import run_duration_analysis
from yield_curve import run_yield_curve_analysis
from credit_analysis import run_credit_analysis
from structured_products import run_structured_products_analysis
from bond_portfolio import run_portfolio_analysis
from bond_features import BondFeaturesAnalyzer
from market_structure import MarketStructureAnalyzer
from floating_rate import FloatingRateAnalyzer, MoneyMarketAnalyzer
from sovereign_credit import SovereignCreditAnalyzer, MunicipalCreditAnalyzer, GovernmentVsCorporateComparison

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


# Command registry with descriptions
COMMANDS = {
    # Bond Pricing Commands
    'price': {
        'module': 'bond_pricing',
        'analysis_type': 'price',
        'description': 'Calculate bond price from YTM',
        'params': ['ytm', 'face_value', 'coupon_rate', 'years_to_maturity', 'frequency']
    },
    'ytm': {
        'module': 'bond_pricing',
        'analysis_type': 'ytm',
        'description': 'Calculate yield to maturity from price',
        'params': ['price', 'face_value', 'coupon_rate', 'years_to_maturity', 'frequency']
    },
    'ytc': {
        'module': 'bond_pricing',
        'analysis_type': 'ytc',
        'description': 'Calculate yield to call',
        'params': ['price', 'face_value', 'coupon_rate', 'years_to_call', 'call_price', 'frequency']
    },
    'ytw': {
        'module': 'bond_pricing',
        'analysis_type': 'ytw',
        'description': 'Calculate yield to worst',
        'params': ['price', 'face_value', 'coupon_rate', 'years_to_maturity', 'call_schedule', 'frequency']
    },
    'clean_dirty': {
        'module': 'bond_pricing',
        'analysis_type': 'clean_dirty',
        'description': 'Calculate clean and dirty prices',
        'params': ['ytm', 'face_value', 'coupon_rate', 'years_to_maturity', 'days_since_last_coupon']
    },
    'spot_rate': {
        'module': 'bond_pricing',
        'analysis_type': 'spot_rate',
        'description': 'Calculate spot rate from zero-coupon bond',
        'params': ['price', 'face_value', 'years_to_maturity']
    },
    'forward_rate': {
        'module': 'bond_pricing',
        'analysis_type': 'forward_rate',
        'description': 'Calculate implied forward rate',
        'params': ['spot_rate_1', 'spot_rate_2', 't1', 't2']
    },

    # Duration & Convexity Commands
    'macaulay_duration': {
        'module': 'duration',
        'analysis_type': 'macaulay_duration',
        'description': 'Calculate Macaulay duration',
        'params': ['face_value', 'coupon_rate', 'years_to_maturity', 'ytm', 'frequency']
    },
    'modified_duration': {
        'module': 'duration',
        'analysis_type': 'modified_duration',
        'description': 'Calculate modified duration and DV01',
        'params': ['face_value', 'coupon_rate', 'years_to_maturity', 'ytm', 'frequency']
    },
    'effective_duration': {
        'module': 'duration',
        'analysis_type': 'effective_duration',
        'description': 'Calculate effective duration (for bonds with options)',
        'params': ['price', 'price_up', 'price_down', 'delta_yield']
    },
    'convexity': {
        'module': 'duration',
        'analysis_type': 'convexity',
        'description': 'Calculate bond convexity',
        'params': ['face_value', 'coupon_rate', 'years_to_maturity', 'ytm', 'frequency']
    },
    'price_change': {
        'module': 'duration',
        'analysis_type': 'price_change',
        'description': 'Estimate price change using duration and convexity',
        'params': ['modified_duration', 'convexity', 'price', 'yield_change']
    },

    # Yield Curve Commands
    'bootstrap': {
        'module': 'yield_curve',
        'analysis_type': 'bootstrap',
        'description': 'Bootstrap spot curve from bond prices',
        'params': ['bonds', 'frequency']
    },
    'forward_curve': {
        'module': 'yield_curve',
        'analysis_type': 'forward_curve',
        'description': 'Calculate forward rate curve',
        'params': ['spot_rates', 'forward_periods']
    },
    'nelson_siegel': {
        'module': 'yield_curve',
        'analysis_type': 'nelson_siegel',
        'description': 'Fit Nelson-Siegel model to yield curve',
        'params': ['maturities', 'yields']
    },
    'curve_shape': {
        'module': 'yield_curve',
        'analysis_type': 'curve_shape',
        'description': 'Analyze yield curve shape',
        'params': ['maturities', 'yields']
    },
    'g_spread': {
        'module': 'yield_curve',
        'analysis_type': 'g_spread',
        'description': 'Calculate G-spread over Treasury',
        'params': ['bond_ytm', 'treasury_ytm']
    },
    'z_spread': {
        'module': 'yield_curve',
        'analysis_type': 'z_spread',
        'description': 'Calculate Z-spread',
        'params': ['bond_price', 'cash_flows', 'spot_rates']
    },
    'oas': {
        'module': 'yield_curve',
        'analysis_type': 'oas',
        'description': 'Calculate option-adjusted spread',
        'params': ['bond_price', 'cash_flows', 'spot_rates', 'option_value']
    },

    # Credit Analysis Commands
    'expected_loss': {
        'module': 'credit',
        'analysis_type': 'expected_loss',
        'description': 'Calculate expected credit loss',
        'params': ['exposure', 'probability_of_default', 'recovery_rate']
    },
    'unexpected_loss': {
        'module': 'credit',
        'analysis_type': 'unexpected_loss',
        'description': 'Calculate unexpected loss',
        'params': ['exposure', 'probability_of_default', 'recovery_rate', 'lgd_volatility']
    },
    'credit_var': {
        'module': 'credit',
        'analysis_type': 'credit_var',
        'description': 'Calculate Credit VaR',
        'params': ['exposure', 'probability_of_default', 'recovery_rate', 'confidence_level']
    },
    'pd_from_spread': {
        'module': 'credit',
        'analysis_type': 'pd_from_spread',
        'description': 'Derive default probability from credit spread',
        'params': ['credit_spread', 'recovery_rate', 'risk_free_rate']
    },
    'merton_pd': {
        'module': 'credit',
        'analysis_type': 'merton_pd',
        'description': 'Calculate PD using Merton model',
        'params': ['asset_value', 'asset_volatility', 'debt_face_value', 'risk_free_rate']
    },
    'historical_pd': {
        'module': 'credit',
        'analysis_type': 'historical_pd',
        'description': 'Get historical default rate by rating',
        'params': ['rating', 'years']
    },
    'rating_transition': {
        'module': 'credit',
        'analysis_type': 'rating_transition',
        'description': 'Analyze rating transition probabilities',
        'params': ['rating']
    },

    # Structured Products Commands
    'mbs_cash_flows': {
        'module': 'structured',
        'analysis_type': 'mbs_cash_flows',
        'description': 'Project MBS cash flows',
        'params': ['principal_balance', 'wac', 'wam', 'psa_speed', 'wala']
    },
    'wal': {
        'module': 'structured',
        'analysis_type': 'wal',
        'description': 'Calculate weighted average life',
        'params': ['principal_balance', 'wac', 'wam', 'psa_speed']
    },
    'wal_sensitivity': {
        'module': 'structured',
        'analysis_type': 'wal_sensitivity',
        'description': 'Analyze WAL sensitivity to prepayment',
        'params': ['principal_balance', 'wac', 'wam', 'psa_speeds']
    },
    'prepayment_schedule': {
        'module': 'structured',
        'analysis_type': 'prepayment_schedule',
        'description': 'Generate PSA prepayment schedule',
        'params': ['wam', 'wala', 'psa_speed']
    },
    'abs_cash_flows': {
        'module': 'structured',
        'analysis_type': 'abs_cash_flows',
        'description': 'Project ABS cash flows with defaults',
        'params': ['principal_balance', 'coupon_rate', 'term_months', 'default_rate']
    },
    'credit_enhancement': {
        'module': 'structured',
        'analysis_type': 'credit_enhancement',
        'description': 'Analyze ABS credit enhancement',
        'params': ['pool_balance', 'subordination_pct', 'reserve_account', 'expected_loss']
    },
    'sequential_cmo': {
        'module': 'structured',
        'analysis_type': 'sequential_cmo',
        'description': 'Analyze sequential pay CMO',
        'params': ['collateral_balance', 'tranche_sizes', 'wac', 'wam', 'psa_speed']
    },

    # Portfolio Commands
    'portfolio_metrics': {
        'module': 'portfolio',
        'analysis_type': 'portfolio_metrics',
        'description': 'Calculate portfolio duration, convexity, DV01',
        'params': ['holdings']
    },
    'duration_contribution': {
        'module': 'portfolio',
        'analysis_type': 'duration_contribution',
        'description': 'Calculate duration contribution by holding',
        'params': ['holdings']
    },
    'key_rate_exposure': {
        'module': 'portfolio',
        'analysis_type': 'key_rate_exposure',
        'description': 'Analyze key rate exposures',
        'params': ['holdings', 'key_rates']
    },
    'immunization_requirements': {
        'module': 'portfolio',
        'analysis_type': 'immunization_requirements',
        'description': 'Calculate immunization requirements',
        'params': ['liability_pv', 'liability_duration', 'current_yield']
    },
    'immunize_two_bonds': {
        'module': 'portfolio',
        'analysis_type': 'immunize_two_bonds',
        'description': 'Create immunized portfolio with two bonds',
        'params': ['liability_duration', 'liability_pv', 'bond1', 'bond2']
    },
    'rebalancing_check': {
        'module': 'portfolio',
        'analysis_type': 'rebalancing_check',
        'description': 'Check if rebalancing is needed',
        'params': ['current_duration', 'target_duration', 'threshold']
    },
    'contingent_immunization': {
        'module': 'portfolio',
        'analysis_type': 'contingent_immunization',
        'description': 'Calculate contingent immunization parameters',
        'params': ['assets', 'liabilities_pv', 'floor_rate', 'current_rate']
    },
    'tracking_error': {
        'module': 'portfolio',
        'analysis_type': 'tracking_error',
        'description': 'Calculate tracking error vs benchmark',
        'params': ['portfolio_returns', 'benchmark_returns']
    },

    # Bond Features Commands
    'describe_bond_type': {
        'module': 'bond_features',
        'analysis_type': 'describe_bond_type',
        'description': 'Describe bond type characteristics',
        'params': ['bond_type']
    },
    'analyze_covenants': {
        'module': 'bond_features',
        'analysis_type': 'analyze_covenants',
        'description': 'Analyze bond covenants (affirmative/negative)',
        'params': ['bond_type', 'is_high_yield']
    },
    'analyze_contingencies': {
        'module': 'bond_features',
        'analysis_type': 'analyze_contingencies',
        'description': 'Analyze contingency provisions (calls, puts, conversions)',
        'params': ['provision_type', 'details']
    },
    'cash_flow_structure': {
        'module': 'bond_features',
        'analysis_type': 'cash_flow_structure',
        'description': 'Analyze bond cash flow structure',
        'params': ['structure_type']
    },

    # Market Structure Commands
    'market_segments': {
        'module': 'market_structure',
        'analysis_type': 'market_segments',
        'description': 'Describe fixed income market segments',
        'params': ['segment']
    },
    'fixed_income_indexes': {
        'module': 'market_structure',
        'analysis_type': 'fixed_income_indexes',
        'description': 'Describe major fixed income indexes',
        'params': ['index_category']
    },
    'primary_vs_secondary': {
        'module': 'market_structure',
        'analysis_type': 'primary_vs_secondary',
        'description': 'Compare primary and secondary markets',
        'params': []
    },
    'repo_mechanics': {
        'module': 'market_structure',
        'analysis_type': 'repo_mechanics',
        'description': 'Analyze repo/reverse repo transactions',
        'params': ['repo_type', 'details']
    },
    'short_term_funding': {
        'module': 'market_structure',
        'analysis_type': 'short_term_funding',
        'description': 'Analyze short-term funding instruments',
        'params': ['instrument']
    },
    'ig_vs_hy_funding': {
        'module': 'market_structure',
        'analysis_type': 'ig_vs_hy_funding',
        'description': 'Compare IG vs HY funding characteristics',
        'params': []
    },

    # Floating Rate Commands
    'frn_price': {
        'module': 'floating_rate',
        'analysis_type': 'frn_price',
        'description': 'Calculate FRN price with discount margin',
        'params': ['quoted_margin', 'discount_margin', 'reference_rate', 'years_to_maturity']
    },
    'discount_margin': {
        'module': 'floating_rate',
        'analysis_type': 'discount_margin',
        'description': 'Calculate FRN discount margin from price',
        'params': ['price', 'quoted_margin', 'reference_rate', 'years_to_maturity']
    },
    'zero_discount_margin': {
        'module': 'floating_rate',
        'analysis_type': 'zero_discount_margin',
        'description': 'Calculate Z-DM for FRN',
        'params': ['price', 'quoted_margin', 'forward_rates', 'years_to_maturity']
    },
    'frn_with_caps_floors': {
        'module': 'floating_rate',
        'analysis_type': 'frn_with_caps_floors',
        'description': 'Analyze FRN with embedded caps/floors',
        'params': ['cap_rate', 'floor_rate', 'reference_rate', 'volatility']
    },
    'reference_rate_info': {
        'module': 'floating_rate',
        'analysis_type': 'reference_rate_info',
        'description': 'Get reference rate information (SOFR, EURIBOR, etc.)',
        'params': ['rate_name']
    },
    'money_market_yield': {
        'module': 'floating_rate',
        'analysis_type': 'money_market_yield',
        'description': 'Convert between money market yield measures',
        'params': ['price', 'face_value', 'days_to_maturity', 'yield_type']
    },
    'compare_mm_yields': {
        'module': 'floating_rate',
        'analysis_type': 'compare_mm_yields',
        'description': 'Compare all money market yield measures',
        'params': ['price', 'face_value', 'days_to_maturity']
    },

    # Sovereign Credit Commands
    'sovereign_rating': {
        'module': 'sovereign',
        'analysis_type': 'sovereign_rating',
        'description': 'Calculate implied sovereign credit rating',
        'params': ['factors']
    },
    'sovereign_ability_to_pay': {
        'module': 'sovereign',
        'analysis_type': 'sovereign_ability_to_pay',
        'description': 'Analyze sovereign ability to pay',
        'params': ['factors']
    },
    'sovereign_willingness': {
        'module': 'sovereign',
        'analysis_type': 'sovereign_willingness',
        'description': 'Analyze sovereign willingness to pay',
        'params': ['factors']
    },
    'local_vs_foreign_debt': {
        'module': 'sovereign',
        'analysis_type': 'local_vs_foreign_debt',
        'description': 'Compare local vs foreign currency sovereign debt',
        'params': ['factors']
    },
    'default_restructuring': {
        'module': 'sovereign',
        'analysis_type': 'default_restructuring',
        'description': 'Analyze default and restructuring factors',
        'params': ['factors', 'has_imf_program', 'debt_to_exports']
    },
    'municipal_go': {
        'module': 'sovereign',
        'analysis_type': 'municipal_go',
        'description': 'Analyze municipal general obligation bonds',
        'params': ['factors', 'population', 'median_income']
    },
    'municipal_revenue': {
        'module': 'sovereign',
        'analysis_type': 'municipal_revenue',
        'description': 'Analyze municipal revenue bonds',
        'params': ['project_type', 'debt_service_coverage', 'rate_covenant', 'essentiality']
    },
    'go_vs_revenue': {
        'module': 'sovereign',
        'analysis_type': 'go_vs_revenue',
        'description': 'Compare GO vs Revenue bonds',
        'params': []
    },
    'govt_vs_corporate': {
        'module': 'sovereign',
        'analysis_type': 'govt_vs_corporate',
        'description': 'Compare government vs corporate bonds',
        'params': []
    },
    'relative_value': {
        'module': 'sovereign',
        'analysis_type': 'relative_value',
        'description': 'Analyze relative value across bond types',
        'params': ['sovereign_yield', 'corporate_spread', 'muni_yield', 'tax_rate']
    },
}


def run_bond_features_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """Execute bond features analysis."""
    analyzer = BondFeaturesAnalyzer()
    analysis_type = params.get('analysis_type', '')

    if analysis_type == 'describe_bond_type':
        return analyzer.describe_bond_type(params.get('bond_type', 'corporate'))
    elif analysis_type == 'analyze_covenants':
        return analyzer.analyze_covenants(
            params.get('bond_type', 'corporate'),
            params.get('is_high_yield', False)
        )
    elif analysis_type == 'analyze_contingencies':
        return analyzer.analyze_contingency_provisions(
            params.get('provision_type', 'callable'),
            params.get('details', {})
        )
    elif analysis_type == 'cash_flow_structure':
        return analyzer.calculate_cash_flow_structure(
            params.get('structure_type', 'bullet')
        )
    else:
        return {'error': f'Unknown bond_features analysis type: {analysis_type}'}


def run_market_structure_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """Execute market structure analysis."""
    analyzer = MarketStructureAnalyzer()
    analysis_type = params.get('analysis_type', '')

    if analysis_type == 'market_segments':
        return analyzer.describe_market_segments(params.get('segment', 'corporate'))
    elif analysis_type == 'fixed_income_indexes':
        return analyzer.describe_fixed_income_indexes(params.get('index_category', 'aggregate'))
    elif analysis_type == 'primary_vs_secondary':
        return analyzer.compare_primary_secondary_markets()
    elif analysis_type == 'repo_mechanics':
        return analyzer.analyze_repo_mechanics(
            params.get('repo_type', 'overnight'),
            params.get('details', {})
        )
    elif analysis_type == 'short_term_funding':
        return analyzer.analyze_short_term_funding(params.get('instrument', 'commercial_paper'))
    elif analysis_type == 'ig_vs_hy_funding':
        return analyzer.compare_ig_vs_hy_funding()
    else:
        return {'error': f'Unknown market_structure analysis type: {analysis_type}'}


def run_floating_rate_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """Execute floating rate analysis."""
    frn_analyzer = FloatingRateAnalyzer()
    mm_analyzer = MoneyMarketAnalyzer()
    analysis_type = params.get('analysis_type', '')

    if analysis_type == 'frn_price':
        return frn_analyzer.calculate_frn_price(
            quoted_margin=params.get('quoted_margin', 0.01),
            discount_margin=params.get('discount_margin', 0.01),
            reference_rate=params.get('reference_rate', 0.05),
            years_to_maturity=params.get('years_to_maturity', 5)
        )
    elif analysis_type == 'discount_margin':
        return frn_analyzer.calculate_discount_margin(
            price=params.get('price', 100),
            quoted_margin=params.get('quoted_margin', 0.01),
            reference_rate=params.get('reference_rate', 0.05),
            years_to_maturity=params.get('years_to_maturity', 5)
        )
    elif analysis_type == 'zero_discount_margin':
        return frn_analyzer.calculate_z_dm(
            price=params.get('price', 100),
            quoted_margin=params.get('quoted_margin', 0.01),
            forward_rates=params.get('forward_rates', [0.05] * 10),
            years_to_maturity=params.get('years_to_maturity', 5)
        )
    elif analysis_type == 'frn_with_caps_floors':
        return frn_analyzer.analyze_frn_with_caps_floors(
            cap_rate=params.get('cap_rate'),
            floor_rate=params.get('floor_rate'),
            reference_rate=params.get('reference_rate', 0.05),
            volatility=params.get('volatility', 0.01)
        )
    elif analysis_type == 'reference_rate_info':
        return frn_analyzer.describe_reference_rates(params.get('rate_name', 'SOFR'))
    elif analysis_type == 'money_market_yield':
        return mm_analyzer.calculate_money_market_yield(
            price=params.get('price', 98),
            face_value=params.get('face_value', 100),
            days_to_maturity=params.get('days_to_maturity', 90)
        )
    elif analysis_type == 'compare_mm_yields':
        return mm_analyzer.compare_yield_measures(
            price=params.get('price', 98),
            face_value=params.get('face_value', 100),
            days_to_maturity=params.get('days_to_maturity', 90)
        )
    else:
        return {'error': f'Unknown floating_rate analysis type: {analysis_type}'}


def run_sovereign_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """Execute sovereign credit analysis."""
    from sovereign_credit import SovereignCreditFactors, MunicipalCreditFactors

    sov_analyzer = SovereignCreditAnalyzer()
    muni_analyzer = MunicipalCreditAnalyzer()
    comparison = GovernmentVsCorporateComparison()
    analysis_type = params.get('analysis_type', '')

    # Helper to build SovereignCreditFactors from params
    def build_sovereign_factors(f: Dict) -> SovereignCreditFactors:
        return SovereignCreditFactors(
            institutional_effectiveness=f.get('institutional_effectiveness', 70),
            political_stability=f.get('political_stability', 70),
            rule_of_law=f.get('rule_of_law', 70),
            corruption_index=f.get('corruption_index', 30),
            gdp_growth_rate=f.get('gdp_growth_rate', 2.0),
            gdp_per_capita=f.get('gdp_per_capita', 50000),
            inflation_rate=f.get('inflation_rate', 2.0),
            unemployment_rate=f.get('unemployment_rate', 5.0),
            current_account_balance_gdp=f.get('current_account_balance_gdp', 0),
            government_debt_gdp=f.get('government_debt_gdp', 60),
            fiscal_balance_gdp=f.get('fiscal_balance_gdp', -3),
            interest_expense_revenue=f.get('interest_expense_revenue', 10),
            foreign_reserves_months_imports=f.get('foreign_reserves_months_imports', 6),
            external_debt_gdp=f.get('external_debt_gdp', 40),
            fx_regime=f.get('fx_regime', 'floating'),
            reserve_currency_issuer=f.get('reserve_currency_issuer', False)
        )

    def build_municipal_factors(f: Dict) -> MunicipalCreditFactors:
        return MunicipalCreditFactors(
            tax_base_diversity=f.get('tax_base_diversity', 70),
            revenue_volatility=f.get('revenue_volatility', 30),
            economic_base_strength=f.get('economic_base_strength', 70),
            debt_per_capita=f.get('debt_per_capita', 2000),
            debt_service_coverage=f.get('debt_service_coverage', 2.0),
            unfunded_pension_liability=f.get('unfunded_pension_liability', 3000),
            budget_management=f.get('budget_management', 75),
            reserve_levels=f.get('reserve_levels', 15),
            state_support_level=f.get('state_support_level', 'moderate'),
            legal_framework=f.get('legal_framework', 'strong')
        )

    if analysis_type == 'sovereign_rating':
        factors = build_sovereign_factors(params.get('factors', {}))
        return sov_analyzer.calculate_sovereign_rating(factors)
    elif analysis_type == 'sovereign_ability_to_pay':
        factors = build_sovereign_factors(params.get('factors', {}))
        return sov_analyzer.analyze_ability_to_pay(factors)
    elif analysis_type == 'sovereign_willingness':
        factors = build_sovereign_factors(params.get('factors', {}))
        return sov_analyzer.analyze_willingness_to_pay(factors)
    elif analysis_type == 'local_vs_foreign_debt':
        factors = build_sovereign_factors(params.get('factors', {}))
        return sov_analyzer.compare_local_vs_foreign_currency_debt(factors)
    elif analysis_type == 'default_restructuring':
        factors = build_sovereign_factors(params.get('factors', {}))
        return sov_analyzer.analyze_default_restructuring_factors(
            factors,
            params.get('has_imf_program', False),
            params.get('debt_to_exports', 100)
        )
    elif analysis_type == 'municipal_go':
        factors = build_municipal_factors(params.get('factors', {}))
        return muni_analyzer.analyze_general_obligation_bonds(
            factors,
            params.get('population', 100000),
            params.get('median_income', 50000)
        )
    elif analysis_type == 'municipal_revenue':
        return muni_analyzer.analyze_revenue_bonds(
            project_type=params.get('project_type', 'Water System'),
            debt_service_coverage=params.get('debt_service_coverage', 1.5),
            rate_covenant=params.get('rate_covenant', 1.25),
            additional_bonds_test=params.get('additional_bonds_test', True),
            essentiality=params.get('essentiality', 'essential')
        )
    elif analysis_type == 'go_vs_revenue':
        return muni_analyzer.compare_go_vs_revenue_bonds()
    elif analysis_type == 'govt_vs_corporate':
        return comparison.compare_issuance_characteristics()
    elif analysis_type == 'relative_value':
        return comparison.analyze_relative_value(
            sovereign_yield=params.get('sovereign_yield', 0.04),
            corporate_spread=params.get('corporate_spread', 0.015),
            muni_yield=params.get('muni_yield', 0.035),
            tax_rate=params.get('tax_rate', 0.35)
        )
    else:
        return {'error': f'Unknown sovereign analysis type: {analysis_type}'}


def list_commands() -> Dict[str, Any]:
    """List all available commands grouped by module."""
    modules = {}

    for cmd, info in COMMANDS.items():
        module = info['module']
        if module not in modules:
            modules[module] = []
        modules[module].append({
            'command': cmd,
            'description': info['description'],
            'params': info['params']
        })

    return {
        'modules': modules,
        'total_commands': len(COMMANDS)
    }


def execute_command(command: str, params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Execute a fixed income analytics command.

    Args:
        command: Command name
        params: Command parameters

    Returns:
        Analysis results
    """
    if command == 'list':
        return list_commands()

    if command not in COMMANDS:
        return {
            'error': f'Unknown command: {command}',
            'available_commands': list(COMMANDS.keys())
        }

    cmd_info = COMMANDS[command]
    module = cmd_info['module']
    analysis_type = cmd_info['analysis_type']

    # Add analysis_type to params
    params['analysis_type'] = analysis_type

    # Route to appropriate module
    try:
        if module == 'bond_pricing':
            return run_bond_pricing_analysis(params)
        elif module == 'duration':
            return run_duration_analysis(params)
        elif module == 'yield_curve':
            return run_yield_curve_analysis(params)
        elif module == 'credit':
            return run_credit_analysis(params)
        elif module == 'structured':
            return run_structured_products_analysis(params)
        elif module == 'portfolio':
            return run_portfolio_analysis(params)
        elif module == 'bond_features':
            return run_bond_features_analysis(params)
        elif module == 'market_structure':
            return run_market_structure_analysis(params)
        elif module == 'floating_rate':
            return run_floating_rate_analysis(params)
        elif module == 'sovereign':
            return run_sovereign_analysis(params)
        else:
            return {'error': f'Unknown module: {module}'}
    except Exception as e:
        logger.error(f"Command execution error: {str(e)}")
        return {'error': str(e)}


def main():
    """Main CLI entry point."""
    if len(sys.argv) < 2:
        # Show help
        result = {
            'usage': 'python cli.py <command> [params_json]',
            'examples': [
                'python cli.py list',
                'python cli.py price \'{"ytm": 0.06, "coupon_rate": 0.05, "years_to_maturity": 10}\'',
                'python cli.py modified_duration \'{"coupon_rate": 0.05, "years_to_maturity": 10, "ytm": 0.06}\'',
                'python cli.py wal \'{"psa_speed": 150}\'',
            ],
            'modules': ['bond_pricing', 'duration', 'yield_curve', 'credit', 'structured', 'portfolio', 'bond_features', 'market_structure', 'floating_rate', 'sovereign']
        }
        print(json.dumps(result, indent=2))
        return

    command = sys.argv[1]

    # Parse parameters
    if len(sys.argv) > 2:
        try:
            params = json.loads(sys.argv[2])
        except json.JSONDecodeError as e:
            print(json.dumps({'error': f'Invalid JSON parameters: {str(e)}'}))
            return
    else:
        params = {}

    # Execute command
    result = execute_command(command, params)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
