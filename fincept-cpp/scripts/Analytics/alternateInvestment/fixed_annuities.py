"""fixed_annuities Module"""

import numpy as np
import pandas as pd
from decimal import Decimal, getcontext
from typing import List, Dict, Optional, Any, Tuple
from datetime import datetime, timedelta
import logging

from config import (
    MarketData, CashFlow, Performance, AssetParameters, AssetClass,
    Constants, Config
)
from base_analytics import AlternativeInvestmentBase, FinancialMath

logger = logging.getLogger(__name__)


class FixedAnnuityAnalyzer(AlternativeInvestmentBase):
    """
    Fixed Annuity Analyzer

    CFA Standards: Retirement Planning, Annuities, Insurance Products

    Key Concepts from Key insight: - Immediate vs Deferred annuities
    - Fixed payout vs variable
    - Mortality credits (longevity insurance)
    - Surrender charges and liquidity constraints
    - Inflation risk (fixed payments lose purchasing power)
    - Opportunity cost vs self-insurance

    Verdict: "The Flawed" - High costs, inflation risk, loss of flexibility
    Better for some retirees with no bequest motive and longevity concerns
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.premium = parameters.acquisition_price if hasattr(parameters, 'acquisition_price') else Decimal('100000')
        self.annuity_rate = parameters.annuity_rate if hasattr(parameters, 'annuity_rate') else Decimal('0.05')
        self.payout_years = parameters.payout_years if hasattr(parameters, 'payout_years') else 20
        self.surrender_charge_years = parameters.surrender_charge_years if hasattr(parameters, 'surrender_charge_years') else 7
        self.surrender_charge_rate = parameters.surrender_charge_rate if hasattr(parameters, 'surrender_charge_rate') else Decimal('0.07')

        # Insurance company credit rating
        self.insurer_rating = parameters.insurer_rating if hasattr(parameters, 'insurer_rating') else 'A'

    def calculate_annual_payout(self) -> Decimal:
        """
        Calculate annual payout from fixed annuity

        Formula: Annual Payout = Premium × Annuity Rate

        Returns:
            Annual payout amount
        """
        return self.premium * self.annuity_rate

    def calculate_total_payouts(self) -> Dict[str, Any]:
        """
        Calculate total payouts over lifetime

        Returns:
            Payout summary
        """
        annual_payout = self.calculate_annual_payout()
        total_payouts = annual_payout * Decimal(str(self.payout_years))

        # Simple return
        total_return = (total_payouts - self.premium) / self.premium

        # Breakeven year (when total payouts = premium)
        breakeven_years = self.premium / annual_payout if annual_payout > 0 else Decimal('0')

        return {
            'premium_paid': float(self.premium),
            'annual_payout': float(annual_payout),
            'total_years': self.payout_years,
            'total_payouts': float(total_payouts),
            'total_return': float(total_return),
            'breakeven_years': float(breakeven_years),
            'interpretation': f'Must live {float(breakeven_years):.1f} years to recover premium'
        }

    def calculate_internal_rate_return(self) -> Decimal:
        """
        Calculate IRR of annuity cash flows

        CFA: IRR = discount rate that makes NPV = 0
        Cash flows: -Premium at t=0, +Annual Payouts for N years

        Returns:
            Internal rate of return
        """
        annual_payout = self.calculate_annual_payout()

        # Construct cash flows
        cash_flows = [-self.premium]  # Initial premium (outflow)
        for _ in range(self.payout_years):
            cash_flows.append(annual_payout)  # Annual payouts (inflows)

        # Calculate IRR using numpy
        try:
            irr = Decimal(str(np.irr(cash_flows)))
            return irr
        except:
            # Fallback: approximate IRR
            return self.annuity_rate

    def inflation_erosion_analysis(self, inflation_rate: Decimal = Decimal('0.03')) -> Dict[str, Any]:
        """
        Analyze purchasing power erosion from inflation

        PRIMARY Criticism: Fixed annuities lose purchasing power to inflation
        With 3% inflation, purchasing power cut in HALF over 24 years

        Args:
            inflation_rate: Expected annual inflation (default 3%)

        Returns:
            Inflation impact analysis
        """
        annual_payout = self.calculate_annual_payout()

        results = []
        cumulative_real_value = Decimal('0')

        for year in range(1, self.payout_years + 1):
            # Real value of payout (adjusted for inflation)
            real_payout = annual_payout / ((Decimal('1') + inflation_rate) ** Decimal(str(year)))
            cumulative_real_value += real_payout

            # Purchasing power as % of initial
            purchasing_power_pct = real_payout / annual_payout

            results.append({
                'year': year,
                'nominal_payout': float(annual_payout),
                'real_payout': float(real_payout),
                'purchasing_power_percentage': float(purchasing_power_pct),
                'cumulative_real_value': float(cumulative_real_value)
            })

        # Final purchasing power
        final_purchasing_power = results[-1]['purchasing_power_percentage'] if results else 0

        # Years to 50% purchasing power
        half_power_year = None
        for r in results:
            if r['purchasing_power_percentage'] <= 0.50:
                half_power_year = r['year']
                break

        return {
            'inflation_rate': float(inflation_rate),
            'initial_annual_payout': float(annual_payout),
            'final_year_real_payout': results[-1]['real_payout'] if results else 0,
            'final_purchasing_power': final_purchasing_power,
            'purchasing_power_lost': 1 - final_purchasing_power,
            'years_to_half_purchasing_power': half_power_year,
            'total_real_value_received': float(cumulative_real_value),
            'analysis_warning': f'At {float(inflation_rate):.1%} inflation, purchasing power drops to {final_purchasing_power:.1%} by year {self.payout_years}',
            'recommendation': 'Consider inflation-adjusted (TIPS-based) annuities or self-insurance with inflation-protected bonds'
        }

    def surrender_charge_analysis(self, surrender_year: int) -> Dict[str, Any]:
        """
        Analyze cost of surrendering annuity early

        Key insight: Surrender charges are PUNITIVE - trap money for years
        Typical: 7% charge declining over 7-10 years

        Args:
            surrender_year: Year considering surrender

        Returns:
            Surrender cost analysis
        """
        if surrender_year <= 0:
            return {'error': 'Surrender year must be positive'}

        # Calculate accumulated value (premium + interest)
        years_held = Decimal(str(surrender_year))
        accumulated_value = self.premium * ((Decimal('1') + self.annuity_rate) ** years_held)

        # Surrender charge (declines linearly to 0)
        if surrender_year > self.surrender_charge_years:
            surrender_charge_pct = Decimal('0')
        else:
            decline_per_year = self.surrender_charge_rate / Decimal(str(self.surrender_charge_years))
            surrender_charge_pct = self.surrender_charge_rate - (decline_per_year * (years_held - Decimal('1')))

        surrender_charge_dollar = accumulated_value * surrender_charge_pct

        # Net value after surrender
        net_value = accumulated_value - surrender_charge_dollar

        # Effective return after surrender charge
        effective_return = (net_value / self.premium) ** (Decimal('1') / years_held) - Decimal('1')

        return {
            'surrender_year': surrender_year,
            'accumulated_value': float(accumulated_value),
            'surrender_charge_percentage': float(surrender_charge_pct),
            'surrender_charge_dollar': float(surrender_charge_dollar),
            'net_value_after_surrender': float(net_value),
            'effective_annual_return': float(effective_return),
            'analysis_critique': 'Surrender charges are punitive and restrict liquidity - major disadvantage vs self-insurance'
        }

    def longevity_insurance_value(self, life_expectancy: int,
                                  probability_outlive: Decimal,
                                  alternative_return: Decimal) -> Dict[str, Any]:
        """
        Value the longevity insurance component

        Key insight: Annuities provide MORTALITY CREDITS - pooling longevity risk
        If you outlive life expectancy, you benefit from those who died early

        Args:
            life_expectancy: Expected lifespan (e.g., 85 years old)
            probability_outlive: Probability of living beyond expectancy (e.g., 0.40)
            alternative_return: Return available from self-insurance (e.g., bond portfolio)

        Returns:
            Longevity insurance value
        """
        annual_payout = self.calculate_annual_payout()

        # Self-insurance option: invest premium, withdraw annually
        self_insurance_value = self.premium
        years_outlive = max(0, self.payout_years - life_expectancy)

        # Value if outlive expectancy (extra years of payouts)
        extra_payouts = annual_payout * Decimal(str(years_outlive))

        # Present value of extra payouts
        pv_extra = Decimal('0')
        for year in range(life_expectancy + 1, self.payout_years + 1):
            pv_extra += annual_payout / ((Decimal('1') + alternative_return) ** Decimal(str(year)))

        # Mortality credit value
        mortality_credit = pv_extra * probability_outlive

        return {
            'life_expectancy': life_expectancy,
            'annuity_payout_years': self.payout_years,
            'years_outliving_expectancy': years_outlive,
            'extra_payouts_if_outlive': float(extra_payouts),
            'pv_extra_payouts': float(pv_extra),
            'probability_outlive': float(probability_outlive),
            'mortality_credit_value': float(mortality_credit),
            'mortality_credit_as_pct_premium': float(mortality_credit / self.premium),
            'interpretation': self._interpret_longevity_value(mortality_credit, self.premium)
        }

    def _interpret_longevity_value(self, credit: Decimal, premium: Decimal) -> str:
        """Interpret longevity insurance value"""
        pct = credit / premium if premium > 0 else Decimal('0')
        if pct > Decimal('0.15'):
            return 'High value - Significant longevity protection'
        elif pct > Decimal('0.08'):
            return 'Moderate value - Reasonable longevity insurance'
        else:
            return 'Low value - Limited longevity benefit vs self-insurance'

    def compare_to_self_insurance(self, alternative_return: Decimal,
                                 withdrawal_rate: Decimal = Decimal('0.04')) -> Dict[str, Any]:
        """
        Compare annuity to self-insurance (bond portfolio with withdrawals)

        Key insight: Most investors better off with self-insurance using TIPS/bonds
        Maintains flexibility, bequest option, inflation protection

        Args:
            alternative_return: Return on bond portfolio (e.g., TIPS yield)
            withdrawal_rate: Safe withdrawal rate (default 4%)

        Returns:
            Comparison analysis
        """
        # Annuity option
        annuity_payout = self.calculate_annual_payout()
        annuity_irr = self.calculate_internal_rate_return()

        # Self-insurance option
        self_insurance_withdrawal = self.premium * withdrawal_rate
        self_insurance_balance = self.premium

        # Track portfolio over time
        balances = []
        for year in range(1, self.payout_years + 1):
            # Earn return
            self_insurance_balance = self_insurance_balance * (Decimal('1') + alternative_return)
            # Withdraw
            self_insurance_balance -= self_insurance_withdrawal

            balances.append({
                'year': year,
                'balance': float(self_insurance_balance),
                'annuity_cumulative': float(annuity_payout * Decimal(str(year))),
                'self_insurance_cumulative': float(self_insurance_withdrawal * Decimal(str(year)))
            })

        # Final bequest value (self-insurance only)
        bequest_value = max(Decimal('0'), self_insurance_balance)

        return {
            'annuity_option': {
                'annual_income': float(annuity_payout),
                'total_income': float(annuity_payout * Decimal(str(self.payout_years))),
                'irr': float(annuity_irr),
                'bequest_value': 0.0,
                'inflation_protection': 'None',
                'liquidity': 'Very Low (surrender charges)'
            },
            'self_insurance_option': {
                'annual_income': float(self_insurance_withdrawal),
                'total_income': float(self_insurance_withdrawal * Decimal(str(self.payout_years))),
                'expected_return': float(alternative_return),
                'bequest_value': float(bequest_value),
                'inflation_protection': 'Yes (if TIPS-based)',
                'liquidity': 'High (access anytime)'
            },
            'winner_by_metric': {
                'annual_income': 'Annuity' if annuity_payout > self_insurance_withdrawal else 'Self-Insurance',
                'flexibility': 'Self-Insurance',
                'bequest_option': 'Self-Insurance',
                'inflation_protection': 'Self-Insurance (if TIPS)',
                'longevity_insurance': 'Annuity'
            },
            'analysis_recommendation': self._determine_annuity_suitability(annuity_payout, self_insurance_withdrawal, bequest_value)
        }

    def _determine_annuity_suitability(self, annuity_payout: Decimal,
                                      self_insurance: Decimal,
                                      bequest: Decimal) -> str:
        """Determine annuity suitability"""
        if bequest > self.premium * Decimal('0.50'):
            return 'Self-Insurance preferred - significant bequest value remains'
        elif annuity_payout > self_insurance * Decimal('1.25'):
            return 'Annuity may be suitable IF no bequest motive AND longevity concern'
        else:
            return 'Self-Insurance preferred - more flexibility, similar income'

    def counterparty_risk_analysis(self) -> Dict[str, Any]:
        """
        Analyze insurance company credit risk

        Warning: Annuities only as safe as the insurance company
        State guaranty funds have limits (typically $250k-500k)

        Returns:
            Counterparty risk assessment
        """
        rating_risk = {
            'AAA': {'risk': 'Very Low', 'default_prob': 0.0001},
            'AA': {'risk': 'Low', 'default_prob': 0.0005},
            'A': {'risk': 'Moderate', 'default_prob': 0.002},
            'BBB': {'risk': 'Moderate-High', 'default_prob': 0.005},
            'BB': {'risk': 'High', 'default_prob': 0.02},
            'B': {'risk': 'Very High', 'default_prob': 0.05}
        }

        risk_data = rating_risk.get(self.insurer_rating, rating_risk['A'])

        return {
            'insurer_rating': self.insurer_rating,
            'credit_risk_level': risk_data['risk'],
            'estimated_default_probability': risk_data['default_prob'],
            'state_guaranty_fund_limit': 250000,  # Typical US limit
            'premium_amount': float(self.premium),
            'covered_by_guaranty_fund': float(min(self.premium, Decimal('250000'))),
            'exposure_beyond_guaranty': float(max(Decimal('0'), self.premium - Decimal('250000'))),
            'analysis_warning': 'Annuities are unsecured claims on insurance company - credit risk matters',
            'recommendation': 'Diversify across multiple highly-rated insurers if large annuity purchase'
        }

    def analysis_verdict(self) -> Dict[str, Any]:
        """
        Complete analytical verdict on Fixed Annuities
        Based on "Alternative Investments Analysis"

        Category: "THE FLAWED"

        Returns:
            Complete verdict with recommendations
        """
        return {
            'asset_class': 'Fixed Annuities',
            'category': 'THE FLAWED',
            'overall_rating': '5/10 - Situational use only',

            'the_good': [
                'Provides guaranteed income for life (longevity insurance)',
                'Mortality credits benefit those who outlive life expectancy',
                'Removes investment management burden',
                'Can provide peace of mind for retirees',
                'Simplifies retirement income planning'
            ],

            'the_bad': [
                'NO inflation protection - purchasing power erodes significantly',
                'High costs embedded in low annuity rates',
                'Surrender charges trap money (typically 7-10 years)',
                'Loss of liquidity and flexibility',
                'No bequest value (money gone when you die)',
                'Counterparty risk (insurance company default)',
                'Opportunity cost vs self-insurance',
                'State guaranty funds have limits'
            ],

            'the_ugly': [
                'Sold with high commissions (conflicts of interest)',
                'Inflation risk often downplayed by salespeople',
                'Surrender charges are punitive traps',
                'Many retirees better off with self-insurance using TIPS',
                'Industry pushes annuities for profit, not client benefit'
            ],

            'key_findings': {
                'inflation_protection': 'NONE - major flaw for multi-decade retirement',
                'flexibility': 'Very Low - money locked up',
                'bequest': 'None - heirs get nothing',
                'costs': 'High - embedded in low payout rates',
                'longevity_insurance': 'Yes - main benefit',
                'better_alternative': 'Self-insurance with TIPS and bonds'
            },

            'analysis_quote': (
                '"For most retirees, self-insurance with a diversified bond portfolio '
                '(heavy on TIPS for inflation protection) is superior to fixed annuities. '
                'You maintain flexibility, liquidity, inflation protection, and bequest options. '
                'Annuities make sense only for those with strong longevity concerns, no bequest '
                'motive, and insufficient assets to self-insure."'
            ),

            'investment_recommendation': {
                'suitable_for': [
                    'Retirees with strong longevity concerns (family history of living to 95+)',
                    'Those with NO bequest motive (no heirs or desire to leave money)',
                    'Individuals unable to manage investments',
                    'Those needing guaranteed lifetime income for peace of mind',
                    'ONLY if using inflation-adjusted (TIPS-based) annuities'
                ],
                'not_suitable_for': [
                    'Anyone wanting to leave bequest to heirs',
                    'Those with average life expectancy or health concerns',
                    'Investors needing liquidity/flexibility',
                    'Anyone considering FIXED annuities (inflation risk)',
                    'Those who can self-insure with bonds'
                ],
                'better_alternatives': [
                    'Self-insurance: TIPS + high-quality bonds',
                    'Systematic withdrawals from diversified portfolio',
                    'Immediate annuity with inflation adjustment (if available)',
                    'Delaying Social Security (acts as inflation-adjusted annuity)'
                ]
            },

            'final_verdict': (
                'Fixed annuities are FLAWED for most investors. Inflation risk, loss of flexibility, '
                'no bequest value, and high embedded costs make them inferior to self-insurance '
                'with TIPS/bonds for the majority of retirees. Use annuities ONLY if: (1) strong '
                'longevity concern, (2) no bequest motive, (3) cannot self-manage investments, and '
                '(4) choose inflation-adjusted version. Even then, annuitize only PART of portfolio, '
                'not entire nest egg.'
            )
        }

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """
        Calculate comprehensive fixed annuity metrics

        Returns:
            All key metrics
        """
        annual_payout = self.calculate_annual_payout()
        total_payouts = self.calculate_total_payouts()
        irr = self.calculate_internal_rate_return()
        inflation_impact = self.inflation_erosion_analysis()

        return {
            'product_type': 'Fixed Annuity',
            'premium': float(self.premium),
            'annuity_rate': float(self.annuity_rate),
            'annual_payout': float(annual_payout),
            'payout_years': self.payout_years,
            'total_payouts_summary': total_payouts,
            'internal_rate_return': float(irr),
            'inflation_impact': inflation_impact,
            'insurer_rating': self.insurer_rating,
            'analysis_category': 'THE FLAWED',
            'recommendation': 'Consider self-insurance with TIPS/bonds instead for most investors'
        }

    def calculate_nav(self) -> Decimal:
        """Calculate current NAV"""
        return self.premium

    def calculate_performance(self) -> Dict[str, Any]:
        """
        Calculate performance metrics

        Returns:
            Performance analysis
        """
        irr = self.calculate_internal_rate_return()
        annual_payout = self.calculate_annual_payout()

        return {
            'internal_rate_return': float(irr),
            'annual_payout': float(annual_payout),
            'payout_rate': float(self.annuity_rate),
            'note': 'Actual performance depends on longevity and inflation'
        }

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive fixed annuity valuation summary"""
        return {
            "asset_overview": {
                "product_type": "Fixed Annuity",
                "premium": float(self.premium),
                "annuity_rate": float(self.annuity_rate),
                "payout_years": self.payout_years,
                "insurer_rating": self.insurer_rating
            },
            "key_metrics": self.calculate_key_metrics(),
            "analysis_category": "THE FLAWED",
            "recommendation": "Use only if strong longevity concern + no bequest motive; prefer TIPS/bonds for most"
        }




# This content should be inserted before final __all__ export in fixed_annuities.py

class InflationIndexedAnnuityAnalyzer(AlternativeInvestmentBase):
    """
    Inflation-Indexed Annuity Analyzer

    CFA Standards: Inflation-protected retirement income

    Key Concepts from Key insight: - Fixed annuity with inflation adjustment
    - Lower initial payout vs fixed annuity
    - Protects purchasing power
    - Higher cost than TIPS ladder
    - Mortality credits for longevity insurance
    - Inflation indexing formula (CPI-based)

    Verdict: "The Good with Caveats"
    Rating: 6/10 - Better than fixed annuity, worse than TIPS ladder
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.premium = parameters.acquisition_price if hasattr(parameters, 'acquisition_price') else Decimal('100000')
        self.real_payout_rate = parameters.real_payout_rate if hasattr(parameters, 'real_payout_rate') else Decimal('0.04')  # Real rate
        self.inflation_rate = parameters.inflation_rate if hasattr(parameters, 'inflation_rate') else Decimal('0.025')  # 2.5% expected
        self.payout_years = parameters.payout_years if hasattr(parameters, 'payout_years') else 25
        self.surrender_charge_years = parameters.surrender_charge_years if hasattr(parameters, 'surrender_charge_years') else 0  # Immediate annuity
        self.insurer_rating = parameters.insurer_rating if hasattr(parameters, 'insurer_rating') else 'A'
        self.age = parameters.age if hasattr(parameters, 'age') else 65

    def calculate_initial_payout(self) -> Decimal:
        """
        Calculate initial annual payout (real)
        Lower than fixed annuity due to inflation protection
        """
        return self.premium * self.real_payout_rate

    def project_payouts(self) -> List[Dict[str, Decimal]]:
        """Project inflation-adjusted payouts over time"""
        initial_payout = self.calculate_initial_payout()
        payouts = []

        for year in range(1, self.payout_years + 1):
            # Nominal payout grows with inflation
            nominal_payout = initial_payout * ((Decimal('1') + self.inflation_rate) ** (year - 1))

            # Real payout stays constant
            real_payout = initial_payout

            payouts.append({
                'year': year,
                'nominal_payout': nominal_payout,
                'real_payout': real_payout,
                'cumulative_nominal': sum(p['nominal_payout'] for p in payouts) + nominal_payout,
                'age': self.age + year
            })

        return payouts

    def compare_to_fixed_annuity(self, fixed_payout_rate: Decimal = Decimal('0.055')) -> Dict[str, Any]:
        """
        Compare inflation-indexed vs fixed annuity

        Fixed annuity: Higher initial payout, eroded by inflation
        Inflation-indexed: Lower initial, protected from inflation
        """
        # Inflation-indexed
        inflation_initial = float(self.calculate_initial_payout())

        # Fixed annuity
        fixed_initial = float(self.premium * fixed_payout_rate)

        # Crossover year (when inflation-indexed exceeds fixed in nominal terms)
        crossover_year = 0
        for year in range(1, 40):
            inflation_payout = inflation_initial * ((1 + float(self.inflation_rate)) ** (year - 1))
            if inflation_payout > fixed_initial:
                crossover_year = year
                break

        return {
            'inflation_indexed_initial': inflation_initial,
            'fixed_annuity_initial': fixed_initial,
            'initial_payout_ratio': inflation_initial / fixed_initial if fixed_initial > 0 else 0,
            'crossover_year': crossover_year,
            'crossover_age': self.age + crossover_year,
            'analysis_note': f'Inflation annuity pays less initially ({inflation_initial:.0f} vs {fixed_initial:.0f}), but catches up at year {crossover_year}'
        }

    def compare_to_tips_ladder(self, tips_real_yield: Decimal = Decimal('0.02')) -> Dict[str, Any]:
        """
        Compare to building TIPS ladder yourself

        TIPS ladder: More flexible, lower cost, same inflation protection
        Inflation annuity: Mortality credits, guaranteed for life
        """
        # TIPS ladder: self-managed
        tips_annual_income = self.premium * tips_real_yield

        # Annuity: includes mortality credits
        annuity_annual_income = float(self.calculate_initial_payout())

        # Mortality credit estimate (varies by age)
        mortality_credit = self.real_payout_rate - tips_real_yield

        return {
            'tips_ladder_income': float(tips_annual_income),
            'inflation_annuity_income': annuity_annual_income,
            'mortality_credit': float(mortality_credit),
            'mortality_credit_pct': float(mortality_credit / tips_real_yield) if tips_real_yield > 0 else 0,
            'annuity_advantage': annuity_annual_income - float(tips_annual_income),
            'analysis_verdict': 'TIPS ladder preferred unless longevity insurance needed',
            'tips_ladder_pros': [
                'Lower cost (no insurance fees)',
                'More flexible',
                'Bequest value',
                'No credit risk (Treasury backed)'
            ],
            'annuity_pros': [
                'Mortality credits (live longer than average)',
                'Guaranteed for life',
                'No management needed'
            ]
        }

    def longevity_break_even(self) -> Dict[str, Any]:
        """
        Calculate break-even age vs TIPS ladder

        When do mortality credits make annuity worthwhile?
        """
        # Assume TIPS ladder depletes at life expectancy (age 85)
        tips_depletion_age = 85
        annuity_pays_for_life = True

        # Value if live to 90
        years_to_90 = 90 - self.age
        total_payouts_to_90 = self.calculate_initial_payout() * Decimal(str(years_to_90))

        # Break-even
        total_cost = self.premium
        break_even_years = total_cost / self.calculate_initial_payout() if self.calculate_initial_payout() > 0 else Decimal('0')
        break_even_age = self.age + int(break_even_years)

        return {
            'premium_paid': float(self.premium),
            'break_even_years': float(break_even_years),
            'break_even_age': break_even_age,
            'typical_life_expectancy': 85,
            'tips_ladder_depletion_age': tips_depletion_age,
            'annuity_advantage_if_live_to_90': 'Significant - continues paying after TIPS depleted',
            'analysis_recommendation': 'Only if expect to live past 90 AND no bequest motive'
        }

    def inflation_protection_value(self) -> Dict[str, Any]:
        """Quantify value of inflation protection"""
        # 3% inflation over 25 years
        high_inflation = Decimal('0.04')  # 4% scenario
        years = 25

        # Fixed annuity loses purchasing power
        initial_payout = self.calculate_initial_payout()
        fixed_real_value_end = initial_payout / ((Decimal('1') + high_inflation) ** years)

        # Inflation-indexed maintains purchasing power
        indexed_real_value_end = initial_payout  # Unchanged

        purchasing_power_preserved = indexed_real_value_end / fixed_real_value_end

        return {
            'initial_payout': float(initial_payout),
            'years': years,
            'inflation_scenario': float(high_inflation),
            'fixed_annuity_real_value_end': float(fixed_real_value_end),
            'indexed_annuity_real_value_end': float(indexed_real_value_end),
            'purchasing_power_ratio': float(purchasing_power_preserved),
            'analysis_note': f'Inflation protection prevents {float((1 - fixed_real_value_end/indexed_real_value_end)*100):.0f}% loss of purchasing power'
        }

    def analysis_verdict(self) -> Dict[str, Any]:
        """verdict on Inflation-Indexed Annuities"""
        return {
            'asset_class': 'Inflation-Indexed Annuities',
            'category': 'THE GOOD (with caveats)',
            'rating': '6/10',
            'best_for': 'Retirees with longevity concern + no bequest motive',
            'pros': [
                'Inflation protection',
                'Longevity insurance (mortality credits)',
                'Guaranteed for life',
                'Purchasing power preserved',
                'No management needed'
            ],
            'cons': [
                'Lower initial payout vs fixed annuity',
                'More expensive than TIPS ladder',
                'Illiquid (surrender charges)',
                'Insurance company credit risk',
                'Lose principal and bequest value',
                'Fees reduce returns'
            ],
            'analysis_quote': '"If you need longevity insurance, inflation-indexed annuities are better than fixed. But a TIPS ladder is even better for most investors."',
            'analysis_hierarchy': '1st: TIPS Ladder, 2nd: Inflation Annuity, 3rd: Fixed Annuity',
            'recommendation': 'Use only if life expectancy >90 AND no heirs'
        }

    def calculate_nav(self) -> Decimal:
        """Current value (difficult to value, use premium)"""
        # Surrender value if applicable
        if self.surrender_charge_years > 0:
            surrender_value = self.premium * (Decimal('1') - self.surrender_charge_rate)
            return surrender_value
        # No surrender - annuitized
        return self.premium

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """Key inflation-indexed annuity metrics"""
        return {
            'premium': float(self.premium),
            'real_payout_rate': float(self.real_payout_rate),
            'initial_annual_payout': float(self.calculate_initial_payout()),
            'inflation_assumption': float(self.inflation_rate),
            'payout_years': self.payout_years,
            'comparison_to_fixed': self.compare_to_fixed_annuity(),
            'comparison_to_tips': self.compare_to_tips_ladder(),
            'longevity_analysis': self.longevity_break_even(),
            'analysis_category': 'THE GOOD (with caveats)'
        }

    def valuation_summary(self) -> Dict[str, Any]:
        """Valuation summary"""
        return {
            'asset_overview': {
                'type': 'Inflation-Indexed Annuity',
                'premium': float(self.premium),
                'insurer_rating': self.insurer_rating,
                'age_at_purchase': self.age
            },
            'key_metrics': self.calculate_key_metrics(),
            'analysis_category': 'THE GOOD (with caveats)',
            'analysis_hierarchy': 'TIPS Ladder > Inflation Annuity > Fixed Annuity'
        }

    def calculate_performance(self) -> Dict[str, Any]:
        """Performance metrics"""
        payouts = self.project_payouts()
        total_nominal = sum(p['nominal_payout'] for p in payouts)
        total_real = sum(p['real_payout'] for p in payouts)

        return {
            'total_nominal_payouts': float(total_nominal),
            'total_real_payouts': float(total_real),
            'return_of_premium_multiple': float(total_nominal / self.premium),
            'real_return_multiple': float(total_real / self.premium),
            'inflation_protected': True
        }


# Export
__all__ = ['FixedAnnuityAnalyzer', 'InflationIndexedAnnuityAnalyzer']
