
Fixed Income Government Analysis Module
=================================

Government securities analysis

===== DATA SOURCES REQUIRED =====
INPUT:
  - Bond prospectuses and issue documentation
  - Market price data and yield curve information
  - Credit ratings and default recovery rates
  - Interest rate benchmarks and swap curves
  - Macroeconomic data affecting fixed income markets

OUTPUT:
  - Bond valuation models and price calculations
  - Yield curve analysis and forward rate calculations
  - Credit risk assessment and spread analysis
  - Duration and convexity risk metrics
  - Portfolio optimization and immunization strategies

PARAMETERS:
  - yield_curve_method: Yield curve modeling method (default: 'nelson_siegel')
  - credit_spread: Credit spread assumption (default: 0.02)
  - recovery_rate: Default recovery rate (default: 0.40)
  - convexity_adjustment: Convexity adjustment factor (default: 1.0)
  - benchmark_curve: Benchmark yield curve (default: 'treasury')


from typing import List, Dict, Optional, Tuple
from decimal import Decimal
from datetime import date
from models import Bond, Portfolio
from credit_analytics import CreditRiskMeasures, ProbabilityOfDefault
from utils import ValidationUtils, cache_calculation
from config import Currency, CreditRating
from yield_curves import SpotCurve
from valuation import BondValuation


class SovereignCreditAnalysis:
    """Sovereign credit risk analysis and evaluation"""

    @staticmethod
    def sovereign_credit_factors() -> Dict[str, List[str]]:
        """Key factors for sovereign credit analysis"""
        return {
            'economic_factors': [
                'GDP growth rate',
                'GDP per capita',
                'Economic diversification',
                'Competitiveness',
                'Labor market flexibility',
                'Monetary policy credibility'
            ],
            'fiscal_factors': [
                'Government debt/GDP ratio',
                'Fiscal balance',
                'Interest burden',
                'Contingent liabilities',
                'Fiscal flexibility',
                'Revenue diversification'
            ],
            'external_factors': [
                'Current account balance',
                'External debt',
                'Foreign exchange reserves',
                'Exchange rate regime',
                'Export concentration',
                'Capital market access'
            ],
            'political_institutional': [
                'Political stability',
                'Institutional quality',
                'Government effectiveness',
                'Rule of law',
                'Corruption control',
                'Policy predictability'
            ]
        }

    @staticmethod
    def debt_sustainability_analysis(country_data: Dict[str, Decimal]) -> Dict[str, Decimal]:
        """Analyze sovereign debt sustainability"""
        # Extract key metrics
        debt_gdp = country_data.get('debt_to_gdp', Decimal('0.6'))
        gdp_growth = country_data.get('gdp_growth', Decimal('0.02'))
        interest_rate = country_data.get('avg_interest_rate', Decimal('0.03'))
        primary_balance = country_data.get('primary_balance_gdp', Decimal('0'))

        ValidationUtils.validate_percentage(debt_gdp, "Debt-to-GDP ratio", allow_negative=False)
        ValidationUtils.validate_percentage(gdp_growth, "GDP growth", allow_negative=True)
        ValidationUtils.validate_yield(interest_rate, "Average interest rate")
        ValidationUtils.validate_percentage(primary_balance, "Primary balance", allow_negative=True)

        # Debt dynamics: Δd = (r-g)d + primary_deficit
        # where d = debt/GDP, r = interest rate, g = growth rate

        real_interest_rate = interest_rate - gdp_growth
        debt_stabilizing_balance = -real_interest_rate * debt_gdp

        # Current debt change
        current_debt_change = real_interest_rate * debt_gdp - primary_balance

        # Projected debt/GDP in 5 years (simplified)
        projected_debt_gdp = debt_gdp
        for year in range(5):
            debt_change = real_interest_rate * projected_debt_gdp - primary_balance
            projected_debt_gdp += debt_change

        # Sustainability indicators
        sustainability_score = SovereignCreditAnalysis._calculate_sustainability_score(
            debt_gdp, gdp_growth, interest_rate, primary_balance
        )

        return {
            'current_debt_gdp': debt_gdp,
            'debt_stabilizing_balance': debt_stabilizing_balance,
            'current_primary_balance': primary_balance,
            'balance_gap': primary_balance - debt_stabilizing_balance,
            'current_debt_change': current_debt_change,
            'projected_debt_gdp_5y': projected_debt_gdp,
            'sustainability_score': sustainability_score,
            'fiscal_space': max(Decimal('0'), Decimal('0.9') - debt_gdp),  # Assume 90% threshold
            'sustainable': sustainability_score >= Decimal('0.7')
        }

    @staticmethod
    def _calculate_sustainability_score(debt_gdp: Decimal, gdp_growth: Decimal,
                                        interest_rate: Decimal, primary_balance: Decimal) -> Decimal:
        """Calculate composite sustainability score (0-1 scale)"""

        # Debt level score (lower is better)
        debt_score = max(Decimal('0'), Decimal('1') - debt_gdp / Decimal('1.5'))  # Penalty above 150%

        # Growth score (higher is better)
        growth_score = min(Decimal('1'), max(Decimal('0'), gdp_growth * Decimal('10')))  # 10% = perfect

        # Interest burden score (lower is better)
        interest_score = max(Decimal('0'), Decimal('1') - interest_rate * Decimal('10'))  # 10% = worst

        # Fiscal balance score (higher is better)
        balance_score = min(Decimal('1'), max(Decimal('0'), primary_balance * Decimal('20') + Decimal('0.5')))

        # Weighted average
        weights = [Decimal('0.3'), Decimal('0.25'), Decimal('0.25'), Decimal('0.2')]
        scores = [debt_score, growth_score, interest_score, balance_score]

        return sum(w * s for w, s in zip(weights, scores))

    @staticmethod
    def sovereign_spread_analysis(country: str, benchmark_yield: Decimal,
                                  sovereign_yield: Decimal, factors: Dict[str, Decimal]) -> Dict[str, Decimal]:
        """Analyze sovereign credit spreads"""
        ValidationUtils.validate_yield(benchmark_yield, "Benchmark yield")
        ValidationUtils.validate_yield(sovereign_yield, "Sovereign yield")

        # Basic spread calculation
        credit_spread = sovereign_yield - benchmark_yield
        spread_bps = credit_spread * Decimal('10000')

        # Factor decomposition (simplified)
        base_spread = Decimal('0.001')  # 10 bps base

        # Economic factors contribution
        gdp_factor = factors.get('gdp_growth', Decimal('0.02'))
        economic_contrib = max(Decimal('0'), (Decimal('0.02') - gdp_factor) * Decimal('0.5'))

        # Fiscal factors contribution
        debt_gdp = factors.get('debt_gdp', Decimal('0.6'))
        fiscal_contrib = max(Decimal('0'), (debt_gdp - Decimal('0.6')) * Decimal('0.02'))

        # External factors contribution
        current_account = factors.get('current_account_gdp', Decimal('0'))
        external_contrib = max(Decimal('0'), -current_account * Decimal('0.1'))

        # Political/institutional factors
        political_score = factors.get('political_stability', Decimal('0.7'))  # 0-1 scale
        political_contrib = max(Decimal('0'), (Decimal('0.7') - political_score) * Decimal('0.05'))

        theoretical_spread = base_spread + economic_contrib + fiscal_contrib + external_contrib + political_contrib

        return {
            'observed_spread': credit_spread,
            'spread_bps': spread_bps,
            'theoretical_spread': theoretical_spread,
            'spread_difference': credit_spread - theoretical_spread,
            'factor_contributions': {
                'base_spread': base_spread,
                'economic_factor': economic_contrib,
                'fiscal_factor': fiscal_contrib,
                'external_factor': external_contrib,
                'political_factor': political_contrib
            },
            'relative_value': 'cheap' if credit_spread > theoretical_spread else 'expensive'
        }


class MunicipalBondAnalysis:
    """Municipal bond analysis and evaluation"""

    @staticmethod
    def municipal_credit_factors() -> Dict[str, List[str]]:
        """Key factors for municipal credit analysis"""
        return {
            'economic_base': [
                'Economic diversity',
                'Employment trends',
                'Income levels',
                'Population growth',
                'Business environment',
                'Tax base stability'
            ],
            'financial_performance': [
                'Operating performance',
                'Debt burden',
                'Liquidity position',
                'Financial flexibility',
                'Capital planning',
                'Reserve levels'
            ],
            'debt_structure': [
                'Debt per capita',
                'Debt service coverage',
                'Direct vs overlapping debt',
                'Variable rate exposure',
                'Derivative usage',
                'Refinancing risk'
            ],
            'governance_management': [
                'Management quality',
                'Financial reporting',
                'Budget practices',
                'Transparency',
                'Political environment',
                'Stakeholder relations'
            ]
        }

    @staticmethod
    def go_bond_analysis(issuer_data: Dict[str, Decimal]) -> Dict[str, Decimal]:
        """Analyze General Obligation (GO) municipal bonds"""

        # Key GO bond metrics
        assessed_value = issuer_data.get('assessed_value', Decimal('1000000000'))  # $1B default
        population = issuer_data.get('population', Decimal('100000'))  # 100k default
        total_debt = issuer_data.get('total_debt', Decimal('50000000'))  # $50M default
        annual_debt_service = issuer_data.get('annual_debt_service', Decimal('5000000'))  # $5M default
        operating_revenues = issuer_data.get('operating_revenues', Decimal('100000000'))  # $100M default
        fund_balance = issuer_data.get('fund_balance', Decimal('20000000'))  # $20M default

        # Calculate key ratios
        debt_per_capita = total_debt / population
        debt_to_assessed_value = total_debt / assessed_value
        debt_service_coverage = operating_revenues / annual_debt_service
        fund_balance_ratio = fund_balance / operating_revenues

        # Credit quality scoring
        debt_burden_score = MunicipalBondAnalysis._score_debt_burden(debt_per_capita, debt_to_assessed_value)
        coverage_score = MunicipalBondAnalysis._score_coverage(debt_service_coverage)
        liquidity_score = MunicipalBondAnalysis._score_liquidity(fund_balance_ratio)

        overall_score = (debt_burden_score + coverage_score + liquidity_score) / Decimal('3')

        return {
            'debt_per_capita': debt_per_capita,
            'debt_to_assessed_value_pct': debt_to_assessed_value * Decimal('100'),
            'debt_service_coverage': debt_service_coverage,
            'fund_balance_ratio_pct': fund_balance_ratio * Decimal('100'),
            'credit_scores': {
                'debt_burden': debt_burden_score,
                'coverage': coverage_score,
                'liquidity': liquidity_score,
                'overall': overall_score
            },
            'credit_quality': MunicipalBondAnalysis._interpret_score(overall_score)
        }

    @staticmethod
    def revenue_bond_analysis(project_data: Dict[str, Decimal]) -> Dict[str, Decimal]:
        """Analyze Revenue municipal bonds"""

        # Revenue bond specific metrics
        gross_revenues = project_data.get('gross_revenues', Decimal('50000000'))
        operating_expenses = project_data.get('operating_expenses', Decimal('30000000'))
        debt_service = project_data.get('debt_service', Decimal('15000000'))
        rate_covenant = project_data.get('rate_covenant', Decimal('1.25'))  # 1.25x coverage requirement

        # Calculate coverage ratios
        net_revenues = gross_revenues - operating_expenses
        debt_service_coverage = net_revenues / debt_service if debt_service > 0 else Decimal('999')

        # Additional coverage tests
        additional_bonds_test = debt_service_coverage >= rate_covenant

        # Cash flow analysis
        free_cash_flow = net_revenues - debt_service
        cash_flow_margin = free_cash_flow / gross_revenues if gross_revenues > 0 else Decimal('0')

        # Project viability metrics
        utilization_rate = project_data.get('utilization_rate', Decimal('0.8'))
        capacity_factor = project_data.get('capacity_factor', Decimal('0.9'))

        return {
            'gross_revenues': gross_revenues,
            'net_revenues': net_revenues,
            'debt_service_coverage': debt_service_coverage,
            'rate_covenant': rate_covenant,
            'covenant_compliance': additional_bonds_test,
            'coverage_cushion': debt_service_coverage - rate_covenant,
            'free_cash_flow': free_cash_flow,
            'cash_flow_margin_pct': cash_flow_margin * Decimal('100'),
            'utilization_rate_pct': utilization_rate * Decimal('100'),
            'capacity_factor_pct': capacity_factor * Decimal('100'),
            'project_viability': MunicipalBondAnalysis._assess_project_viability(
                debt_service_coverage, utilization_rate, capacity_factor
            )
        }

    @staticmethod
    def _score_debt_burden(debt_per_capita: Decimal, debt_to_av: Decimal) -> Decimal:
        """Score debt burden (0-1 scale)"""
        # Lower debt burden = higher score
        per_capita_score = max(Decimal('0'), Decimal('1') - debt_per_capita / Decimal('5000'))  # $5k benchmark
        av_score = max(Decimal('0'), Decimal('1') - debt_to_av / Decimal('0.1'))  # 10% benchmark

        return (per_capita_score + av_score) / Decimal('2')

    @staticmethod
    def _score_coverage(coverage_ratio: Decimal) -> Decimal:
        """Score debt service coverage"""
        if coverage_ratio >= Decimal('3'):
            return Decimal('1')
        elif coverage_ratio >= Decimal('2'):
            return Decimal('0.8')
        elif coverage_ratio >= Decimal('1.5'):
            return Decimal('0.6')
        elif coverage_ratio >= Decimal('1.2'):
            return Decimal('0.4')
        else:
            return Decimal('0.2')

    @staticmethod
    def _score_liquidity(fund_balance_ratio: Decimal) -> Decimal:
        """Score liquidity position"""
        if fund_balance_ratio >= Decimal('0.25'):  # 25%+
            return Decimal('1')
        elif fund_balance_ratio >= Decimal('0.15'):  # 15-25%
            return Decimal('0.8')
        elif fund_balance_ratio >= Decimal('0.10'):  # 10-15%
            return Decimal('0.6')
        elif fund_balance_ratio >= Decimal('0.05'):  # 5-10%
            return Decimal('0.4')
        else:
            return Decimal('0.2')

    @staticmethod
    def _interpret_score(score: Decimal) -> str:
        """Interpret overall credit score"""
        if score >= Decimal('0.8'):
            return 'Strong'
        elif score >= Decimal('0.6'):
            return 'Good'
        elif score >= Decimal('0.4'):
            return 'Satisfactory'
        elif score >= Decimal('0.2'):
            return 'Weak'
        else:
            return 'Distressed'

    @staticmethod
    def _assess_project_viability(coverage: Decimal, utilization: Decimal, capacity: Decimal) -> str:
        """Assess revenue bond project viability"""
        score = (coverage / Decimal('2') + utilization + capacity) / Decimal('3')  # Normalize coverage

        if score >= Decimal('0.8'):
            return 'Highly Viable'
        elif score >= Decimal('0.6'):
            return 'Viable'
        elif score >= Decimal('0.4'):
            return 'Marginal'
        else:
            return 'Distressed'


class AgencyDebtAnalysis:
    """Government agency and GSE debt analysis"""

    @staticmethod
    def agency_types() -> Dict[str, Dict[str, str]]:
        """Classification of government agencies and GSEs"""
        return {
            'federal_agencies': {
                'description': 'Direct obligations of US government agencies',
                'credit_support': 'Full faith and credit backing',
                'examples': 'GNMA, SBA, Tennessee Valley Authority'
            },
            'government_sponsored_enterprises': {
                'description': 'Privately-owned, government-chartered entities',
                'credit_support': 'Implied government support (historically)',
                'examples': 'Fannie Mae, Freddie Mac, Federal Home Loan Banks'
            },
            'supranational_agencies': {
                'description': 'International organizations',
                'credit_support': 'Member country support',
                'examples': 'World Bank, Asian Development Bank, European Investment Bank'
            }
        }

    @staticmethod
    def gse_credit_analysis(gse_data: Dict[str, Decimal]) -> Dict[str, Decimal]:
        """Analyze Government Sponsored Enterprise credit quality"""

        # Capital adequacy
        tier1_capital = gse_data.get('tier1_capital', Decimal('50000000000'))
        risk_weighted_assets = gse_data.get('risk_weighted_assets', Decimal('500000000000'))
        tier1_ratio = tier1_capital / risk_weighted_assets

        # Asset quality
        total_assets = gse_data.get('total_assets', Decimal('1000000000000'))
        non_performing_assets = gse_data.get('non_performing_assets', Decimal('5000000000'))
        npa_ratio = non_performing_assets / total_assets

        # Profitability
        net_income = gse_data.get('net_income', Decimal('10000000000'))
        average_assets = gse_data.get('average_assets', total_assets)
        roa = net_income / average_assets

        # Liquidity
        liquid_assets = gse_data.get('liquid_assets', Decimal('100000000000'))
        short_term_debt = gse_data.get('short_term_debt', Decimal('200000000000'))
        liquidity_ratio = liquid_assets / short_term_debt

        # Government support assessment
        systemic_importance = gse_data.get('market_share', Decimal('0.3'))  # 30% market share
        government_ties = gse_data.get('government_charter_score', Decimal('0.8'))  # 0-1 scale

        # Overall assessment
        financial_strength = AgencyDebtAnalysis._assess_financial_strength(
            tier1_ratio, npa_ratio, roa, liquidity_ratio
        )

        government_support_score = (systemic_importance + government_ties) / Decimal('2')

        return {
            'capital_adequacy': {
                'tier1_ratio_pct': tier1_ratio * Decimal('100'),
                'regulatory_minimum': Decimal('4'),  # 4% minimum
                'well_capitalized': tier1_ratio >= Decimal('0.06')  # 6% well-capitalized
            },
            'asset_quality': {
                'npa_ratio_pct': npa_ratio * Decimal('100'),
                'asset_quality_score': max(Decimal('0'), Decimal('1') - npa_ratio * Decimal('20'))
            },
            'profitability': {
                'roa_pct': roa * Decimal('100'),
                'profitability_score': min(Decimal('1'), max(Decimal('0'), roa * Decimal('100')))
            },
            'liquidity': {
                'liquidity_ratio': liquidity_ratio,
                'liquidity_adequate': liquidity_ratio >= Decimal('0.5')
            },
            'government_support': {
                'support_score': government_support_score,
                'systemic_importance': systemic_importance,
                'government_ties': government_ties
            },
            'overall_assessment': {
                'financial_strength': financial_strength,
                'government_support': government_support_score,
                'combined_rating': AgencyDebtAnalysis._derive_rating(financial_strength, government_support_score)
            }
        }

    @staticmethod
    def _assess_financial_strength(tier1_ratio: Decimal, npa_ratio: Decimal,
                                   roa: Decimal, liquidity_ratio: Decimal) -> Decimal:
        """Assess standalone financial strength (0-1 scale)"""

        # Capital score
        capital_score = min(Decimal('1'), tier1_ratio / Decimal('0.08'))  # 8% = perfect

        # Asset quality score
        asset_score = max(Decimal('0'), Decimal('1') - npa_ratio * Decimal('50'))  # 2% NPA = 0 score

        # Profitability score
        profit_score = min(Decimal('1'), max(Decimal('0'), roa * Decimal('100')))  # 1% ROA = perfect

        # Liquidity score
        liquidity_score = min(Decimal('1'), liquidity_ratio / Decimal('1'))  # 100% = perfect

        # Weighted average
        weights = [Decimal('0.3'), Decimal('0.3'), Decimal('0.2'), Decimal('0.2')]
        scores = [capital_score, asset_score, profit_score, liquidity_score]

        return sum(w * s for w, s in zip(weights, scores))

    @staticmethod
    def _derive_rating(financial_strength: Decimal, government_support: Decimal) -> str:
        """Derive credit rating from financial strength and government support"""

        # Combined score with higher weight on government support for GSEs
        combined_score = financial_strength * Decimal('0.4') + government_support * Decimal('0.6')

        if combined_score >= Decimal('0.9'):
            return 'AAA'
        elif combined_score >= Decimal('0.8'):
            return 'AA'
        elif combined_score >= Decimal('0.7'):
            return 'A'
        elif combined_score >= Decimal('0.6'):
            return 'BBB'
        elif combined_score >= Decimal('0.5'):
            return 'BB'
        else:
            return 'B'


class InflationLinkedBonds:
    """Inflation-linked government bond analysis"""

    @staticmethod
    def tips_analysis(principal: Decimal, coupon_rate: Decimal,
                      inflation_rate: Decimal, years_remaining: Decimal) -> Dict[str, Decimal]:
        """Analyze Treasury Inflation-Protected Securities (TIPS)"""
        ValidationUtils.validate_positive(principal, "Principal")
        ValidationUtils.validate_positive(coupon_rate, "Coupon rate")
        ValidationUtils.validate_percentage(inflation_rate, "Inflation rate", allow_negative=True)
        ValidationUtils.validate_positive(years_remaining, "Years remaining")

        # Inflation-adjusted principal
        inflation_factor = (Decimal('1') + inflation_rate) ** years_remaining
        adjusted_principal = principal * inflation_factor

        # Annual coupon payment (based on adjusted principal)
        annual_coupon = adjusted_principal * coupon_rate

        # Real yield calculation (simplified)
        nominal_yield = coupon_rate + inflation_rate  # Approximation
        real_yield = coupon_rate  # TIPS coupon rate is the real yield

        # Breakeven inflation rate (what inflation needs to be for TIPS to outperform nominals)
        nominal_bond_yield = Decimal('0.05')  # Assume 5% nominal Treasury yield
        breakeven_inflation = nominal_bond_yield - real_yield

        return {
            'original_principal': principal,
            'inflation_adjusted_principal': adjusted_principal,
            'inflation_adjustment_factor': inflation_factor,
            'annual_coupon_payment': annual_coupon,
            'real_yield': real_yield,
            'nominal_yield_equivalent': nominal_yield,
            'breakeven_inflation_rate': breakeven_inflation,
            'inflation_protection': inflation_rate - breakeven_inflation,
            'attractive_vs_nominal': inflation_rate > breakeven_inflation
        }

    @staticmethod
    def inflation_swap_analysis(notional: Decimal, fixed_rate: Decimal,
                                expected_inflation: Decimal, tenor: Decimal) -> Dict[str, Decimal]:
        """Analyze inflation swap for hedging purposes"""
        ValidationUtils.validate_positive(notional, "Notional")
        ValidationUtils.validate_yield(fixed_rate, "Fixed rate")
        ValidationUtils.validate_percentage(expected_inflation, "Expected inflation", allow_negative=True)
        ValidationUtils.validate_positive(tenor, "Tenor")

        # Inflation swap PV calculation (simplified)
        fixed_leg_pv = notional * fixed_rate * tenor  # Simplified fixed leg

        # Floating leg expected value
        floating_leg_pv = notional * expected_inflation * tenor

        # Swap value to fixed rate payer
        swap_value = floating_leg_pv - fixed_leg_pv

        # Breakeven inflation
        breakeven = fixed_rate

        return {
            'notional_amount': notional,
            'fixed_rate': fixed_rate,
            'expected_inflation': expected_inflation,
            'fixed_leg_pv': fixed_leg_pv,
            'floating_leg_pv': floating_leg_pv,
            'swap_value_to_payer': swap_value,
            'breakeven_inflation': breakeven,
            'inflation_expectation_vs_breakeven': expected_inflation - breakeven
        }


# Utility functions for government debt analysis
def sovereign_debt_comparison(countries: Dict[str, Dict[str, Decimal]]) -> Dict[str, Dict]:
    """Compare sovereign debt metrics across countries"""

    comparison = {}

    for country, data in countries.items():
        sustainability = SovereignCreditAnalysis.debt_sustainability_analysis(data)

        comparison[country] = {
            'debt_gdp_ratio': data.get('debt_to_gdp', Decimal('0')),
            'sustainability_score': sustainability['sustainability_score'],
            'fiscal_space': sustainability['fiscal_space'],
            'sustainable': sustainability['sustainable'],
            'projected_debt_5y': sustainability['projected_debt_gdp_5y']
        }

    # Rank countries by sustainability
    ranked = sorted(comparison.items(),
                    key=lambda x: x[1]['sustainability_score'],
                    reverse=True)

    return {
        'country_analysis': comparison,
        'ranking_by_sustainability': [country for country, _ in ranked],
        'summary_statistics': {
            'average_debt_gdp': sum(data['debt_gdp_ratio'] for data in comparison.values()) / len(comparison),
            'countries_sustainable': sum(1 for data in comparison.values() if data['sustainable']),
            'total_countries': len(comparison)
        }
    }


def municipal_portfolio_analysis(portfolio: Portfolio,
                                 muni_data: Dict[str, Dict[str, Decimal]]) -> Dict[str, any]:
    """Analyze municipal bond portfolio"""

    total_exposure = Decimal('0')
    go_exposure = Decimal('0')
    revenue_exposure = Decimal('0')

    issuer_analysis = {}

    for bond, quantity in portfolio.holdings:
        if bond.sector == 'Municipal':
            exposure = bond.face_value * quantity
            total_exposure += exposure

            issuer = bond.issuer_name
            if issuer in muni_data:
                issuer_data = muni_data[issuer]

                # Determine bond type and analyze
                if issuer_data.get('bond_type') == 'GO':
                    go_exposure += exposure
                    analysis = MunicipalBondAnalysis.go_bond_analysis(issuer_data)
                else:
                    revenue_exposure += exposure
                    analysis = MunicipalBondAnalysis.revenue_bond_analysis(issuer_data)

                issuer_analysis[issuer] = {
                    'exposure': exposure,
                    'analysis': analysis,
                    'weight': exposure / total_exposure if total_exposure > 0 else Decimal('0')
                }

    return {
        'portfolio_composition': {
            'total_municipal_exposure': total_exposure,
            'go_bond_exposure': go_exposure,
            'revenue_bond_exposure': revenue_exposure,
            'go_percentage': go_exposure / total_exposure if total_exposure > 0 else Decimal('0'),
            'revenue_percentage': revenue_exposure / total_exposure if total_exposure > 0 else Decimal('0')
        },
        'issuer_analysis': issuer_analysis,
        'portfolio_quality_metrics': {
            'weighted_avg_coverage': sum(
                data['analysis'].get('debt_service_coverage', Decimal('1')) * data['weight']
                for data in issuer_analysis.values()
            ),
            'number_of_issuers': len(issuer_analysis),
            'concentration_risk': max(
                data['weight'] for data in issuer_analysis.values()) if issuer_analysis else Decimal('0')
        }
    }