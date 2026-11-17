"""
Fixed Income Analytics CLI
==========================

Single entry point for the fixed income analytics modules in this directory.

Contract
--------
    python Analytics/fixedIncome/cli.py <command> '<json params>'

prints exactly ONE JSON payload on stdout (NaN / Infinity never appear; an uncomputable field is null,
usually with a *_note saying why). Failures are {"error": "<plain message>"} (exit code 0, like the other
analytics scripts). Diagnostics go to stderr.

    python cli.py list                      every command with its description and parameters
    python cli.py help '{"command": "ytm"}' one command

Conventions shared by every command (details in fi_common.py)
-------------------------------------------------------------
* rates, yields, spreads and probabilities are decimals (0.05 = 5 %); *_bps / *_pct fields say so.
* bond yields are nominal annual rates compounded `frequency` times a year (frequency 0 = zero-coupon,
  annual compounding); `price` is the clean price, dirty_price = price + accrued_interest.
* durations are in years (Macaulay and modified are both reported), convexity in years^2.
* spot (zero) rates are annually compounded unless a field name says otherwise.
* if a parameter you passed was not used by the command, the result lists it in `unused_params`.
"""

import importlib
import json
import logging
import os
import sys
from typing import Any, Dict

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)
try:
    import fincept_json
except ImportError:  # run by path without PYTHONPATH=scripts
    sys.path.append(os.path.dirname(os.path.dirname(_HERE)))
    import fincept_json

logging.basicConfig(level=logging.WARNING, stream=sys.stderr)
logger = logging.getLogger("fixedIncome.cli")

# module key -> (python module, dispatcher function)
MODULES = {
    'bond_pricing': ('bond_pricing', 'run_bond_pricing_analysis'),
    'duration': ('duration_convexity', 'run_duration_analysis'),
    'yield_curve': ('yield_curve', 'run_yield_curve_analysis'),
    'credit': ('credit_analysis', 'run_credit_analysis'),
    'structured': ('structured_products', 'run_structured_products_analysis'),
    'portfolio': ('bond_portfolio', 'run_portfolio_analysis'),
    'bond_features': ('bond_features', 'run_bond_features_analysis'),
    'market_structure': ('market_structure', 'run_market_structure_analysis'),
    'floating_rate': ('floating_rate', 'run_floating_rate_analysis'),
    'sovereign': ('sovereign_credit', 'run_sovereign_analysis'),
}

_BOND = ['face_value', 'coupon_rate', 'years_to_maturity', 'ytm', 'frequency',
         'settlement_date', 'maturity_date', 'day_count']
_EFF = ['price', 'price_up', 'price_down', 'delta_yield']
_FRN = ['quoted_margin', 'reference_rate', 'periods_remaining', 'years_to_maturity', 'reset_frequency',
        'face_value']
_MM = ['price', 'face_value', 'days_to_maturity']
_HOLD = ['holdings']


def _c(module, analysis_type, description, params, aliases=None):
    return {'module': module, 'analysis_type': analysis_type, 'description': description,
            'params': params, 'aliases': aliases or {}}


COMMANDS: Dict[str, Dict[str, Any]] = {
    # ---- bond pricing ----------------------------------------------------------------------------
    'price': _c('bond_pricing', 'price', 'Bond price (clean, dirty, accrued) from YTM; fractional periods and '
                'settlement/maturity dates supported',
                ['ytm', 'face_value', 'coupon_rate', 'years_to_maturity', 'frequency',
                 'settlement_date', 'maturity_date', 'day_count']),
    'ytm': _c('bond_pricing', 'ytm', 'Yield to maturity from price (clean by default; price_type=dirty)',
              ['price', 'face_value', 'coupon_rate', 'years_to_maturity', 'frequency', 'price_type',
               'settlement_date', 'maturity_date', 'day_count']),
    'ytc': _c('bond_pricing', 'ytc', 'Yield to call (call date treated as a coupon date)',
              ['price', 'face_value', 'coupon_rate', 'years_to_call', 'call_price', 'frequency', 'price_type']),
    'ytw': _c('bond_pricing', 'ytw', 'Yield to worst: min of YTM and YTC over call_schedule [[years, price]]',
              ['price', 'face_value', 'coupon_rate', 'years_to_maturity', 'call_schedule', 'frequency',
               'price_type']),
    'clean_dirty': _c('bond_pricing', 'clean_dirty', 'Clean and dirty price between coupon dates '
                      '(day_count 30/360, 30E/360, ACT/360, ACT/365, ACT/ACT)',
                      ['ytm', 'face_value', 'coupon_rate', 'years_to_maturity', 'days_since_last_coupon',
                       'days_in_coupon_period', 'frequency', 'day_count', 'settlement_date', 'maturity_date']),
    'spot_rate': _c('bond_pricing', 'spot_rate', 'Spot rate (annual compounding) from a zero-coupon price',
                    ['price', 'face_value', 'years_to_maturity']),
    'forward_rate': _c('bond_pricing', 'forward_rate', 'Implied forward rate f(t1,t2) from two spot rates',
                       ['spot_rate_1', 'spot_rate_2', 't1', 't2']),
    'price_spot_curve': _c('bond_pricing', 'price_spot_curve', 'Price a bond on a spot curve '
                           '(one annually-compounded spot rate per coupon period)',
                           ['spot_rates', 'face_value', 'coupon_rate', 'frequency']),

    # ---- duration & convexity -----------------------------------------------------------------------
    'macaulay_duration': _c('duration', 'macaulay_duration', 'Macaulay duration (years) with cash-flow table', _BOND),
    'modified_duration': _c('duration', 'modified_duration', 'Modified duration, dollar duration and DV01', _BOND),
    'effective_duration': _c('duration', 'effective_duration', 'Effective duration from bumped prices', _EFF),
    'convexity': _c('duration', 'convexity', 'Convexity (years^2) and dollar convexity', _BOND),
    'effective_convexity': _c('duration', 'effective_convexity', 'Effective convexity from bumped prices', _EFF),
    'price_change': _c('duration', 'price_change', 'Price change estimate from duration + convexity',
                       ['modified_duration', 'convexity', 'price', 'yield_change']),
    'key_rate_duration': _c('duration', 'key_rate_duration', 'Key rate durations (tent shocks) on a spot curve',
                            ['spot_rates', 'face_value', 'coupon_rate', 'frequency', 'delta_rate',
                             'key_rate_maturities']),
    'portfolio_duration': _c('duration', 'portfolio_duration', 'MV-weighted duration/convexity of bonds',
                             ['bonds']),

    # ---- yield curve --------------------------------------------------------------------------------
    'bootstrap': _c('yield_curve', 'bootstrap', 'Bootstrap the spot curve from bond prices or from par yields '
                    '(maturities + yields); reprices every input exactly',
                    ['bonds', 'frequency', 'maturities', 'yields']),
    'forward_curve': _c('yield_curve', 'forward_curve', 'Forward rates between consecutive (or given) maturities '
                        'from annually-compounded spot rates',
                        ['spot_rates', 'forward_periods', 'maturities', 'yields']),
    'nelson_siegel': _c('yield_curve', 'nelson_siegel', 'Fit Nelson-Siegel (>= 4 points)', ['maturities', 'yields']),
    'svensson': _c('yield_curve', 'svensson', 'Fit Svensson (>= 6 points)', ['maturities', 'yields']),
    'interpolate': _c('yield_curve', 'interpolate', 'Yields at target maturities (linear, cubic_spline, '
                      'nelson_siegel, svensson)', ['maturities', 'yields', 'target_maturities', 'method']),
    'curve_shape': _c('yield_curve', 'curve_shape', 'Curve shape, slope, butterfly, 2s10s and 3m10y spreads',
                      ['maturities', 'yields']),
    'g_spread': _c('yield_curve', 'g_spread', 'G-spread over the government yield', ['bond_ytm', 'treasury_ytm']),
    'z_spread': _c('yield_curve', 'z_spread', 'Z-spread over the spot curve (exact root)',
                   ['bond_price', 'cash_flows', 'spot_rates', 'maturities', 'yields']),
    'oas': _c('yield_curve', 'oas', 'Z-spread minus a SUPPLIED option cost (no rate tree; option_value required)',
              ['bond_price', 'cash_flows', 'spot_rates', 'option_value', 'maturities', 'yields']),
    'compare_spreads': _c('yield_curve', 'compare_spreads', 'G-, I- and (optionally) Z-spread side by side',
                          ['bond_ytm', 'treasury_ytm', 'swap_rate', 'bond_price', 'cash_flows', 'spot_rates']),

    # ---- credit -------------------------------------------------------------------------------------
    'expected_loss': _c('credit', 'expected_loss', 'Expected credit loss EAD x PD x LGD',
                        ['exposure', 'probability_of_default', 'recovery_rate']),
    'unexpected_loss': _c('credit', 'unexpected_loss', 'Unexpected loss (std. dev. of loss)',
                          ['exposure', 'probability_of_default', 'recovery_rate', 'lgd_volatility']),
    'credit_var': _c('credit', 'credit_var', 'Credit VaR, Vasicek single-factor model',
                     ['exposure', 'probability_of_default', 'recovery_rate', 'confidence_level', 'correlation']),
    'pd_from_spread': _c('credit', 'pd_from_spread', 'Risk-neutral PD implied by a credit spread',
                         ['credit_spread', 'recovery_rate', 'risk_free_rate']),
    'merton_pd': _c('credit', 'merton_pd', 'PD from the Merton structural model',
                    ['asset_value', 'asset_volatility', 'debt_face_value', 'risk_free_rate', 'time_horizon',
                     'asset_return']),
    'historical_pd': _c('credit', 'historical_pd', 'Table default rate by rating (approximate averages)',
                        ['rating', 'years']),
    'rating_transition': _c('credit', 'rating_transition', 'One-year rating transition probabilities', ['rating']),
    'cumulative_pd': _c('credit', 'cumulative_pd', 'Cumulative and marginal PDs for a constant annual PD',
                        ['annual_pd', 'years', 'method']),
    'spread_analysis': _c('credit', 'spread_analysis', 'Observed spread vs pure expected-loss spread',
                          ['observed_spread', 'rating', 'maturity', 'recovery_rate']),
    'recovery_rate': _c('credit', 'recovery_rate', 'Recovery estimate by seniority (+ collateral)',
                        ['seniority', 'collateral_value', 'debt_outstanding']),

    # ---- structured products ------------------------------------------------------------------------
    'mbs_cash_flows': _c('structured', 'mbs_cash_flows', 'MBS pass-through cash flows (PSA or CPR), totals and WAL',
                         ['principal_balance', 'wac', 'wam', 'psa_speed', 'cpr', 'wala', 'passthrough_rate',
                          'max_rows']),
    'wal': _c('structured', 'wal', 'Weighted average life of an MBS pool',
              ['principal_balance', 'wac', 'wam', 'psa_speed', 'cpr', 'wala']),
    'wal_sensitivity': _c('structured', 'wal_sensitivity', 'WAL under several PSA speeds vs 100 PSA',
                          ['principal_balance', 'wac', 'wam', 'psa_speeds', 'wala']),
    'prepayment_schedule': _c('structured', 'prepayment_schedule', 'PSA prepayment schedule (CPR/SMM by month)',
                              ['wam', 'wala', 'psa_speed', 'max_rows']),
    'abs_cash_flows': _c('structured', 'abs_cash_flows', 'ABS cash flows with defaults, recoveries, ABS prepayments',
                         ['principal_balance', 'coupon_rate', 'term_months', 'default_rate', 'loss_severity',
                          'abs_speed']),
    'credit_enhancement': _c('structured', 'credit_enhancement', 'ABS credit enhancement vs expected loss',
                             ['pool_balance', 'subordination_pct', 'reserve_account', 'expected_loss',
                              'excess_spread']),
    'sequential_cmo': _c('structured', 'sequential_cmo', 'Sequential-pay CMO tranche cash flows and WALs',
                         ['collateral_balance', 'tranche_sizes', 'coupon_rates', 'wac', 'wam', 'psa_speed',
                          'passthrough_rate', 'z_tranche', 'wala']),
    'cpr_smm_convert': _c('structured', 'cpr_smm_convert', 'Convert CPR <-> SMM', ['cpr', 'smm']),

    # ---- portfolio ----------------------------------------------------------------------------------
    'portfolio_metrics': _c('portfolio', 'portfolio_metrics', 'Portfolio duration, convexity, yield, DV01', _HOLD),
    'duration_contribution': _c('portfolio', 'duration_contribution', 'Duration contribution by holding', _HOLD),
    'key_rate_exposure': _c('portfolio', 'key_rate_exposure', 'Maturity-bucket exposure (nearest key tenor)',
                            ['holdings', 'key_rates']),
    'immunization_requirements': _c('portfolio', 'immunization_requirements', 'Classical immunization conditions',
                                    ['liability_pv', 'liability_duration', 'current_yield']),
    'immunize_two_bonds': _c('portfolio', 'immunize_two_bonds', 'Two-bond duration-matched allocation',
                             ['liability_duration', 'liability_pv', 'bond1', 'bond2']),
    'rebalancing_check': _c('portfolio', 'rebalancing_check', 'Duration drift vs target',
                            ['current_duration', 'target_duration', 'threshold', 'time_elapsed']),
    'contingent_immunization': _c('portfolio', 'contingent_immunization', 'Contingent immunization cushion and trigger',
                                  ['assets', 'liabilities_pv', 'floor_rate', 'current_rate', 'duration', 'frequency']),
    'tracking_error': _c('portfolio', 'tracking_error', 'Tracking error and information ratio',
                         ['portfolio_returns', 'benchmark_returns', 'periods_per_year']),
    'cash_flow_matching': _c('portfolio', 'cash_flow_matching', 'Dedicate bonds to a liability schedule',
                             ['liabilities', 'available_bonds']),
    'cell_matching': _c('portfolio', 'cell_matching', 'Index vs portfolio cell weights',
                        ['index_cells', 'portfolio_cells']),

    # ---- bond features (reference text + cash-flow structures) --------------------------------------
    'describe_bond_type': _c('bond_features', 'describe_bond', 'Describe a bond type', ['bond_type']),
    'analyze_covenants': _c('bond_features', 'covenants', 'Affirmative / negative covenants (or all)',
                            ['covenant_type']),
    'analyze_contingencies': _c('bond_features', 'contingencies', 'Embedded options: call, put, conversion, '
                                'sinking_fund, cap_floor (or all)', ['contingency_type'],
                                {'provision_type': 'contingency_type'}),
    'cash_flow_structure': _c('bond_features', 'cash_flow_structure', 'Cash-flow schedule: bullet, amortizing, '
                              'sinking_fund, step_up, deferred',
                              ['structure_type', 'face_value', 'coupon_rate', 'maturity_years', 'frequency',
                               'sinking_start', 'sinking_amount', 'step_schedule', 'defer_years', 'max_rows']),
    'legal_tax': _c('bond_features', 'legal_tax', 'Legal, regulatory and tax considerations', ['jurisdiction']),

    # ---- market structure ---------------------------------------------------------------------------
    'market_segments': _c('market_structure', 'market_segments', 'Fixed income market segments (all or one)',
                          ['segment']),
    'fixed_income_indexes': _c('market_structure', 'indexes', 'Fixed income index families (all or one category)',
                               ['index_category']),
    'primary_vs_secondary': _c('market_structure', 'primary_secondary', 'Primary vs secondary bond markets', []),
    'repo_mechanics': _c('market_structure', 'repo_mechanics', 'Repo cash, interest, haircut and leverage',
                         ['collateral_value', 'haircut', 'repo_rate', 'term_days', 'details']),
    'short_term_funding': _c('market_structure', 'short_term_funding', 'Short-term funding instruments',
                             ['funding_type'], {'instrument': 'funding_type'}),
    'ig_vs_hy_funding': _c('market_structure', 'ig_vs_hy', 'Investment-grade vs high-yield funding', []),

    # ---- floating rate & money market ---------------------------------------------------------------
    'frn_price': _c('floating_rate', 'frn_price', 'FRN price from discount margin', _FRN + ['discount_margin']),
    'discount_margin': _c('floating_rate', 'discount_margin', 'FRN discount margin from price', _FRN + ['price']),
    'zero_discount_margin': _c('floating_rate', 'zero_discount_margin', 'Z-DM over a spot (or forward) curve',
                               _FRN + ['price', 'spot_rates', 'forward_rates']),
    'frn_with_caps_floors': _c('floating_rate', 'frn_cap_floor', 'Capped/floored FRN, Black-76 cap and floor values',
                               _FRN + ['cap_rate', 'floor_rate', 'rate_volatility', 'discount_margin'],
                               {'volatility': 'rate_volatility'}),
    'reference_rate_info': _c('floating_rate', 'reference_rates', 'Reference rates (SOFR, EURIBOR, SONIA, LIBOR '
                              'transition)', ['rate_type'], {'rate_name': 'rate_type'}),
    'money_market_yield': _c('floating_rate', 'money_market_yield', 'Money market yield; yield_type money_market '
                             '(default), discount, bond_equivalent, effective_annual or all', _MM + ['yield_type']),
    'compare_mm_yields': _c('floating_rate', 'compare_yields', 'All money market yield measures', _MM),
    'discount_yield': _c('floating_rate', 'discount_yield', 'Bank discount yield', _MM),
    'bond_equivalent_yield': _c('floating_rate', 'bond_equivalent_yield', 'Bond-equivalent (365-day) yield', _MM),
    'effective_annual_yield': _c('floating_rate', 'effective_annual_yield', 'Effective annual yield', _MM),
    'price_from_discount': _c('floating_rate', 'price_from_discount', 'Price from a bank discount yield',
                              ['face_value', 'discount_yield', 'days_to_maturity']),

    # ---- sovereign / municipal (indicative scorecards + reference text) ------------------------------
    'sovereign_rating': _c('sovereign', 'sovereign_rating', 'Indicative sovereign rating scorecard', ['factors']),
    'sovereign_ability_to_pay': _c('sovereign', 'sovereign_ability_to_pay', 'Ability-to-pay score', ['factors']),
    'sovereign_willingness': _c('sovereign', 'sovereign_willingness', 'Willingness-to-pay score', ['factors']),
    'local_vs_foreign_debt': _c('sovereign', 'local_vs_foreign_debt', 'Local vs foreign currency debt scores',
                                ['factors']),
    'default_restructuring': _c('sovereign', 'default_restructuring', 'Default warning signs and restructuring',
                                ['factors', 'has_imf_program', 'debt_to_exports']),
    'municipal_go': _c('sovereign', 'municipal_go', 'GO bond scorecard', ['factors', 'population', 'median_income']),
    'municipal_revenue': _c('sovereign', 'municipal_revenue', 'Revenue bond scorecard',
                            ['project_type', 'debt_service_coverage', 'rate_covenant', 'additional_bonds_test',
                             'essentiality']),
    'go_vs_revenue': _c('sovereign', 'go_vs_revenue', 'GO vs revenue bonds (reference text)', []),
    'govt_vs_corporate': _c('sovereign', 'govt_vs_corporate', 'Government vs corporate bonds (reference text)', []),
    'relative_value': _c('sovereign', 'relative_value', 'Sovereign / corporate / tax-equivalent muni yields',
                         ['sovereign_yield', 'corporate_spread', 'muni_yield', 'tax_rate']),
}


def list_commands(only: Any = None) -> Dict[str, Any]:
    """Commands grouped by module (or one command when `only` names it)."""
    if only is not None:
        if only not in COMMANDS:
            return {'error': f'Unknown command: {only}', 'available_commands': sorted(COMMANDS)}
        info = COMMANDS[only]
        out = {'command': only, 'module': info['module'], 'description': info['description'],
               'params': info['params']}
        if info['aliases']:
            out['param_aliases'] = info['aliases']
        return out
    modules: Dict[str, list] = {}
    for cmd, info in COMMANDS.items():
        row = {'command': cmd, 'description': info['description'], 'params': info['params']}
        if info['aliases']:
            row['param_aliases'] = info['aliases']
        modules.setdefault(info['module'], []).append(row)
    return {
        'usage': "python cli.py <command> '<json params>'",
        'modules': modules,
        'total_commands': len(COMMANDS),
        'conventions': {
            'rates': 'decimals (0.05 = 5%)',
            'bond_yield': 'nominal annual, compounded `frequency` times a year (0 = zero-coupon, annual)',
            'price': 'clean (quoted) price; dirty_price = price + accrued_interest',
            'duration': 'years (Macaulay and modified both reported); convexity in years^2',
            'spot_rate': 'annual compounding unless the field name says otherwise',
            'errors': '{"error": "<message>"}',
        },
    }


def execute_command(command: str, params: Any) -> Dict[str, Any]:
    """Run one command; always returns a dict (an {'error': ...} dict on failure)."""
    if command in ('list', 'help', '--help', '-h'):
        only = params.get('command') if isinstance(params, dict) else None
        return list_commands(only)

    if command not in COMMANDS:
        return {'error': f'Unknown command: {command}', 'available_commands': sorted(COMMANDS)}
    if not isinstance(params, dict):
        return {'error': 'params must be a JSON object'}

    info = COMMANDS[command]
    p = dict(params)
    for old, new in info['aliases'].items():
        if old in p and new not in p:
            p[new] = p[old]
    p['analysis_type'] = info['analysis_type']

    module_name, func_name = MODULES[info['module']]
    try:
        module = importlib.import_module(module_name)
        result = getattr(module, func_name)(p)
    except Exception as e:  # never a traceback instead of JSON
        logger.error("%s: %s: %s", command, type(e).__name__, e)
        result = {'error': str(e) or type(e).__name__}
    if not isinstance(result, dict):
        result = {'result': result}

    allowed = set(info['params']) | set(info['aliases']) | {'analysis_type'}
    unused = sorted(k for k in params if k not in allowed)
    if unused:
        result['unused_params'] = unused
    return result


def main(argv=None) -> None:
    """CLI entry point: exactly one JSON payload on stdout."""
    argv = sys.argv if argv is None else argv
    if len(argv) < 2:
        fincept_json.emit({
            'usage': "python cli.py <command> '<json params>'",
            'help': "python cli.py list   |   python cli.py help '{\"command\": \"price\"}'",
            'examples': [
                'python cli.py price \'{"ytm": 0.06, "coupon_rate": 0.05, "years_to_maturity": 10}\'',
                'python cli.py modified_duration \'{"coupon_rate": 0.05, "years_to_maturity": 10, "ytm": 0.06}\'',
                'python cli.py bootstrap \'{"maturities": [0.5, 1, 2, 5, 10], "yields": [0.05, 0.049, 0.046, 0.043, 0.043]}\'',
                'python cli.py wal \'{"psa_speed": 150}\'',
            ],
            'commands': sorted(COMMANDS),
            'total_commands': len(COMMANDS),
        })
        return

    command = argv[1]
    params: Any = {}
    if len(argv) > 2 and argv[2].strip():
        try:
            params = json.loads(argv[2])
        except ValueError as e:
            fincept_json.emit({'error': f'Invalid JSON parameters: {e}'})
            return
    if not isinstance(params, dict):
        fincept_json.emit({'error': 'params must be a JSON object'})
        return

    fincept_json.emit(execute_command(command, params))


if __name__ == "__main__":
    main()
