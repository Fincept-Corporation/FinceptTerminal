"""
Bond Features and Instrument Types Module
=========================================

Comprehensive bond features, types, and covenant analysis implementing
CFA Institute curriculum for fixed income instrument characteristics.

===== CFA CURRICULUM COVERAGE =====
- Fixed-Income Instrument Features
- Bond indenture contents
- Affirmative vs negative covenants
- Cash flow structures
- Contingency provisions (calls, puts, conversions)
- Legal, regulatory, and tax considerations

PARAMETERS:
  - bond_type: Type of fixed income instrument
  - covenant_type: affirmative or negative
  - contingency_type: call, put, conversion, etc.
"""

from dataclasses import dataclass, field
from typing import Dict, Any, List, Optional, Tuple
from enum import Enum
from datetime import date
import numpy as np
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class BondType(Enum):
    """Classification of bond types"""
    # By Issuer
    SOVEREIGN = "sovereign"
    AGENCY = "agency"
    SUPRANATIONAL = "supranational"
    CORPORATE = "corporate"
    MUNICIPAL = "municipal"

    # By Coupon Structure
    FIXED_RATE = "fixed_rate"
    FLOATING_RATE = "floating_rate"
    ZERO_COUPON = "zero_coupon"
    STEP_UP = "step_up"
    DEFERRED_COUPON = "deferred_coupon"
    PAYMENT_IN_KIND = "pik"

    # By Embedded Options
    CALLABLE = "callable"
    PUTABLE = "putable"
    CONVERTIBLE = "convertible"
    EXCHANGEABLE = "exchangeable"

    # By Principal Repayment
    BULLET = "bullet"
    AMORTIZING = "amortizing"
    SINKING_FUND = "sinking_fund"


class CouponType(Enum):
    """Coupon payment structures"""
    FIXED = "fixed"
    FLOATING = "floating"
    ZERO = "zero"
    STEP_UP = "step_up"
    STEP_DOWN = "step_down"
    DEFERRED = "deferred"
    PIK = "payment_in_kind"
    INFLATION_LINKED = "inflation_linked"


class CovenantType(Enum):
    """Bond covenant classifications"""
    AFFIRMATIVE = "affirmative"
    NEGATIVE = "negative"


class ContingencyType(Enum):
    """Embedded option types"""
    CALL = "call"
    PUT = "put"
    CONVERSION = "conversion"
    EXCHANGE = "exchange"
    MAKE_WHOLE_CALL = "make_whole_call"
    SINKING_FUND = "sinking_fund"
    CAP = "cap"
    FLOOR = "floor"


class DayCountConvention(Enum):
    """Day count conventions"""
    ACTUAL_360 = "ACT/360"
    ACTUAL_365 = "ACT/365"
    ACTUAL_ACTUAL = "ACT/ACT"
    THIRTY_360 = "30/360"
    THIRTY_360_EU = "30E/360"


@dataclass
class BondFeatures:
    """Complete bond feature specification"""
    issuer_type: BondType
    coupon_type: CouponType
    face_value: float = 1000.0
    coupon_rate: float = 0.05
    maturity_years: float = 10.0
    payment_frequency: int = 2
    day_count: DayCountConvention = DayCountConvention.THIRTY_360
    currency: str = "USD"
    is_callable: bool = False
    is_putable: bool = False
    is_convertible: bool = False
    has_sinking_fund: bool = False
    is_secured: bool = False
    seniority: str = "senior_unsecured"


class BondFeaturesAnalyzer:
    """
    Bond features and characteristics analyzer.

    Provides comprehensive analysis of bond types, structures,
    and embedded features per CFA curriculum.
    """

    def describe_bond_type(
        self,
        bond_type: str,
    ) -> Dict[str, Any]:
        """
        Describe characteristics of a bond type.

        Args:
            bond_type: Type of bond to describe

        Returns:
            Dictionary with bond type characteristics
        """
        bond_descriptions = {
            'sovereign': {
                'name': 'Sovereign Bond',
                'issuer': 'National government',
                'credit_backing': 'Full faith and credit of issuing government',
                'tax_treatment': 'Often exempt from local/state taxes',
                'liquidity': 'Highest liquidity (benchmark securities)',
                'examples': ['US Treasury', 'UK Gilts', 'German Bunds', 'JGBs'],
                'risk_factors': ['Interest rate risk', 'Inflation risk', 'Currency risk (foreign)'],
                'typical_investors': ['Central banks', 'Pension funds', 'Insurance companies']
            },
            'agency': {
                'name': 'Agency Bond',
                'issuer': 'Government-sponsored enterprises (GSEs)',
                'credit_backing': 'Implicit or explicit government guarantee',
                'tax_treatment': 'May have tax advantages',
                'liquidity': 'High liquidity',
                'examples': ['Fannie Mae', 'Freddie Mac', 'Federal Home Loan Banks'],
                'risk_factors': ['Credit risk (implicit guarantee)', 'Prepayment risk (MBS)'],
                'typical_investors': ['Banks', 'Money market funds', 'Insurance companies']
            },
            'supranational': {
                'name': 'Supranational Bond',
                'issuer': 'International organizations',
                'credit_backing': 'Multiple sovereign governments',
                'tax_treatment': 'Often tax-exempt',
                'liquidity': 'High liquidity',
                'examples': ['World Bank', 'IMF', 'European Investment Bank', 'Asian Development Bank'],
                'risk_factors': ['Currency risk', 'Political risk'],
                'typical_investors': ['Central banks', 'Sovereign wealth funds']
            },
            'corporate': {
                'name': 'Corporate Bond',
                'issuer': 'Private and public corporations',
                'credit_backing': 'Issuer assets and cash flows',
                'tax_treatment': 'Fully taxable interest',
                'liquidity': 'Varies by issue size and credit quality',
                'examples': ['Investment grade', 'High yield', 'Convertibles'],
                'risk_factors': ['Credit/default risk', 'Interest rate risk', 'Liquidity risk'],
                'typical_investors': ['Mutual funds', 'Insurance companies', 'Pension funds']
            },
            'municipal': {
                'name': 'Municipal Bond',
                'issuer': 'State and local governments',
                'credit_backing': 'Taxing authority or revenue streams',
                'tax_treatment': 'Often federal tax-exempt, sometimes state/local exempt',
                'liquidity': 'Lower than sovereigns/corporates',
                'examples': ['General obligation (GO)', 'Revenue bonds'],
                'risk_factors': ['Credit risk', 'Interest rate risk', 'Call risk'],
                'typical_investors': ['High-net-worth individuals', 'Muni bond funds']
            },
            'zero_coupon': {
                'name': 'Zero-Coupon Bond',
                'issuer': 'Various (sovereign, corporate)',
                'credit_backing': 'Depends on issuer',
                'tax_treatment': 'Phantom income taxed annually (OID)',
                'liquidity': 'Varies',
                'coupon_structure': 'No periodic payments, sold at deep discount',
                'duration': 'Duration equals maturity (highest interest rate sensitivity)',
                'risk_factors': ['High interest rate risk', 'Reinvestment risk eliminated'],
                'typical_investors': ['Long-term liability matching', 'Tax-advantaged accounts']
            },
            'floating_rate': {
                'name': 'Floating-Rate Note (FRN)',
                'issuer': 'Various (banks, corporates, governments)',
                'credit_backing': 'Depends on issuer',
                'coupon_structure': 'Reference rate + spread, resets periodically',
                'reference_rates': ['SOFR', 'EURIBOR', 'Prime rate'],
                'duration': 'Low duration (resets to par at each reset)',
                'risk_factors': ['Credit spread risk', 'Cap/floor risk', 'Reference rate risk'],
                'typical_investors': ['Banks', 'Money market funds', 'Short-duration investors']
            },
            'callable': {
                'name': 'Callable Bond',
                'issuer': 'Corporations, municipalities',
                'embedded_option': 'Issuer has right to redeem before maturity',
                'call_provisions': ['American (anytime)', 'European (specific date)', 'Bermudan (specific dates)'],
                'call_price': 'Usually at par or premium (make-whole provisions)',
                'yield_impact': 'Higher yield than non-callable (compensates for call risk)',
                'duration': 'Effective duration lower than stated duration',
                'risk_factors': ['Call risk', 'Reinvestment risk', 'Negative convexity'],
                'typical_investors': ['Yield seekers willing to accept call risk']
            },
            'putable': {
                'name': 'Putable Bond',
                'issuer': 'Various',
                'embedded_option': 'Investor has right to sell back to issuer',
                'put_provisions': 'Usually at par on specific dates',
                'yield_impact': 'Lower yield than non-putable (option value to investor)',
                'duration': 'Effective duration lower in rising rate environment',
                'risk_factors': ['Lower yield', 'Issuer credit quality'],
                'typical_investors': ['Risk-averse investors seeking downside protection']
            },
            'convertible': {
                'name': 'Convertible Bond',
                'issuer': 'Corporations',
                'embedded_option': 'Holder can convert to equity shares',
                'conversion_terms': 'Conversion ratio, conversion price, conversion period',
                'yield_impact': 'Lower coupon than straight bond (equity upside)',
                'valuation': 'Bond floor + equity option value',
                'risk_factors': ['Equity risk', 'Credit risk', 'Dilution risk'],
                'typical_investors': ['Hedge funds', 'Equity-oriented bond investors']
            }
        }

        bond_type_lower = bond_type.lower().replace('-', '_').replace(' ', '_')

        if bond_type_lower in bond_descriptions:
            return {
                'bond_type': bond_type,
                'description': bond_descriptions[bond_type_lower]
            }
        else:
            return {
                'error': f'Unknown bond type: {bond_type}',
                'available_types': list(bond_descriptions.keys())
            }

    def analyze_covenants(
        self,
        covenant_type: str = None,
    ) -> Dict[str, Any]:
        """
        Describe bond covenants and their implications.

        Args:
            covenant_type: 'affirmative' or 'negative' (None for both)

        Returns:
            Dictionary with covenant analysis
        """
        affirmative_covenants = {
            'description': 'Actions the issuer MUST take',
            'purpose': 'Ensure issuer maintains creditworthiness',
            'examples': [
                {
                    'covenant': 'Financial reporting',
                    'requirement': 'Provide audited financial statements',
                    'frequency': 'Quarterly and annually'
                },
                {
                    'covenant': 'Maintain insurance',
                    'requirement': 'Keep adequate insurance on assets',
                    'frequency': 'Ongoing'
                },
                {
                    'covenant': 'Pay taxes',
                    'requirement': 'Pay all taxes when due',
                    'frequency': 'As required'
                },
                {
                    'covenant': 'Maintain assets',
                    'requirement': 'Keep collateral in good condition',
                    'frequency': 'Ongoing'
                },
                {
                    'covenant': 'Comply with laws',
                    'requirement': 'Maintain legal compliance',
                    'frequency': 'Ongoing'
                },
                {
                    'covenant': 'Maintain credit rating',
                    'requirement': 'Take actions to preserve rating',
                    'frequency': 'Ongoing'
                }
            ],
            'violation_consequence': 'Technical default, acceleration possible'
        }

        negative_covenants = {
            'description': 'Actions the issuer MUST NOT take (restrictions)',
            'purpose': 'Protect bondholders from increased risk',
            'examples': [
                {
                    'covenant': 'Debt limitation',
                    'restriction': 'Cannot exceed specified debt levels',
                    'metric': 'Debt/EBITDA, Debt/Equity ratios'
                },
                {
                    'covenant': 'Lien restriction',
                    'restriction': 'Cannot pledge assets as collateral for other debt',
                    'exception': 'Permitted liens for specific purposes'
                },
                {
                    'covenant': 'Dividend restriction',
                    'restriction': 'Limits on dividend payments to shareholders',
                    'purpose': 'Preserve cash for debt service'
                },
                {
                    'covenant': 'Asset sale restriction',
                    'restriction': 'Cannot sell material assets without conditions',
                    'condition': 'Proceeds may need to repay debt'
                },
                {
                    'covenant': 'Merger restriction',
                    'restriction': 'Limits on M&A activity',
                    'condition': 'Surviving entity must assume debt'
                },
                {
                    'covenant': 'Investment restriction',
                    'restriction': 'Limits on investments or acquisitions',
                    'purpose': 'Prevent excessive risk-taking'
                },
                {
                    'covenant': 'Change of control',
                    'restriction': 'Put option triggered by ownership change',
                    'protection': 'Protects against LBO risk'
                }
            ],
            'violation_consequence': 'Event of default, potential acceleration'
        }

        result = {
            'indenture_definition': 'Legal contract between issuer and bondholders specifying terms, covenants, and rights',
            'trustee_role': 'Third party (usually bank) that monitors covenant compliance and represents bondholders'
        }

        if covenant_type is None or covenant_type.lower() == 'affirmative':
            result['affirmative_covenants'] = affirmative_covenants
        if covenant_type is None or covenant_type.lower() == 'negative':
            result['negative_covenants'] = negative_covenants

        result['covenant_strength'] = {
            'high_yield': 'More restrictive covenants (higher credit risk)',
            'investment_grade': 'Fewer, less restrictive covenants',
            'trend': 'Covenant-lite loans have become more common'
        }

        return result

    def analyze_contingency_provisions(
        self,
        contingency_type: str = None,
    ) -> Dict[str, Any]:
        """
        Analyze embedded options and contingency provisions.

        Args:
            contingency_type: Type of contingency to analyze

        Returns:
            Dictionary with contingency analysis
        """
        provisions = {
            'call': {
                'name': 'Call Provision',
                'benefits': 'Issuer',
                'description': 'Issuer right to redeem bonds before maturity',
                'types': {
                    'american_call': 'Callable anytime after call protection period',
                    'european_call': 'Callable only on specific date(s)',
                    'bermudan_call': 'Callable on multiple specific dates',
                    'make_whole_call': 'Callable at price based on Treasury + spread'
                },
                'call_protection': 'Initial period when bond cannot be called',
                'call_price': 'Usually par or declining premium schedule',
                'investor_impact': [
                    'Reinvestment risk in falling rate environment',
                    'Price appreciation limited (negative convexity)',
                    'Higher yield compensates for call risk'
                ],
                'valuation': 'Value of callable bond = Straight bond value - Call option value'
            },
            'put': {
                'name': 'Put Provision',
                'benefits': 'Investor',
                'description': 'Investor right to sell bond back to issuer',
                'types': {
                    'hard_put': 'Unconditional right to put at specified dates',
                    'soft_put': 'Conditional on certain events (rating downgrade)'
                },
                'put_price': 'Usually at par value',
                'investor_impact': [
                    'Downside protection in rising rate environment',
                    'Lower yield reflects option value',
                    'Floor on bond price'
                ],
                'valuation': 'Value of putable bond = Straight bond value + Put option value'
            },
            'conversion': {
                'name': 'Conversion Provision',
                'benefits': 'Investor (equity upside)',
                'description': 'Right to convert bonds into issuer common stock',
                'key_terms': {
                    'conversion_ratio': 'Number of shares per bond',
                    'conversion_price': 'Effective price paid per share',
                    'conversion_value': 'Current value if converted (shares × stock price)',
                    'conversion_premium': 'Premium paid over conversion value'
                },
                'investor_impact': [
                    'Equity upside participation',
                    'Lower coupon than straight bond',
                    'Downside protection (bond floor)'
                ],
                'valuation': 'Value = Max(Bond floor, Conversion value) + Time value',
                'forced_conversion': 'Issuer may call to force conversion when in-the-money'
            },
            'sinking_fund': {
                'name': 'Sinking Fund Provision',
                'benefits': 'Mixed (reduces credit risk, but reinvestment risk)',
                'description': 'Issuer must retire portion of debt periodically',
                'methods': {
                    'open_market': 'Issuer buys bonds in secondary market',
                    'lottery': 'Random selection of bonds at par',
                    'pro_rata': 'All bondholders redeemed proportionally'
                },
                'investor_impact': [
                    'Reduces credit risk (debt declining)',
                    'Creates reinvestment risk',
                    'Average life shorter than maturity'
                ]
            },
            'cap_floor': {
                'name': 'Cap and Floor (Floating Rate)',
                'benefits': 'Cap benefits issuer, Floor benefits investor',
                'description': 'Limits on floating rate coupon adjustments',
                'cap': {
                    'definition': 'Maximum coupon rate',
                    'impact': 'Limits upside for investor in rising rates',
                    'benefits': 'Issuer protected from high rates'
                },
                'floor': {
                    'definition': 'Minimum coupon rate',
                    'impact': 'Guarantees minimum income for investor',
                    'benefits': 'Investor protected from very low rates'
                },
                'collar': 'Combination of cap and floor'
            }
        }

        if contingency_type:
            contingency_lower = contingency_type.lower()
            if contingency_lower in provisions:
                return {
                    'contingency_type': contingency_type,
                    'analysis': provisions[contingency_lower]
                }
            else:
                return {
                    'error': f'Unknown contingency type: {contingency_type}',
                    'available_types': list(provisions.keys())
                }

        return {
            'provisions_benefiting_issuer': ['Call', 'Cap', 'Make-whole call'],
            'provisions_benefiting_investor': ['Put', 'Floor', 'Conversion'],
            'all_provisions': provisions
        }

    def analyze_legal_tax_considerations(
        self,
        jurisdiction: str = "US",
    ) -> Dict[str, Any]:
        """
        Analyze legal, regulatory, and tax considerations.

        Args:
            jurisdiction: Country/region for analysis

        Returns:
            Dictionary with legal/tax analysis
        """
        considerations = {
            'legal_framework': {
                'indenture': 'Trust Indenture Act of 1939 (US) - requires trustee for public offerings',
                'securities_laws': 'Securities Act of 1933 - registration requirements',
                'disclosure': 'SEC Rule 144A - private placement exemption',
                'bankruptcy': 'Priority of claims in bankruptcy proceedings'
            },
            'regulatory_considerations': {
                'bank_regulations': {
                    'basel_iii': 'Risk-weighted capital requirements',
                    'liquidity_coverage': 'HQLA (High Quality Liquid Assets) treatment',
                    'nsfr': 'Net Stable Funding Ratio impact'
                },
                'insurance_regulations': {
                    'naic_ratings': 'Insurance company capital charges by rating',
                    'admitted_assets': 'Eligibility for statutory accounting'
                },
                'investment_restrictions': {
                    'erisa': 'Fiduciary requirements for pension funds',
                    'money_market': 'Rule 2a-7 eligibility requirements'
                }
            },
            'tax_considerations': {
                'interest_income': {
                    'corporate_bonds': 'Fully taxable at ordinary income rates',
                    'municipal_bonds': 'Generally federal tax-exempt',
                    'treasury_bonds': 'Federal tax, but state/local exempt'
                },
                'original_issue_discount': {
                    'description': 'Bonds issued below par (e.g., zero-coupon)',
                    'tax_treatment': 'Accrued discount taxed annually as phantom income',
                    'calculation': 'Constant yield method'
                },
                'market_discount': {
                    'description': 'Bonds purchased below par in secondary market',
                    'tax_treatment': 'Taxed at sale or maturity (ordinary income)',
                    'election': 'Can elect to accrue annually'
                },
                'capital_gains': {
                    'holding_period': 'Long-term if held > 1 year',
                    'rate': 'Preferential capital gains rates may apply'
                },
                'tax_loss_harvesting': 'Selling at loss to offset gains (wash sale rules)'
            },
            'international_considerations': {
                'withholding_tax': 'Non-resident investors may face withholding',
                'tax_treaties': 'May reduce or eliminate withholding',
                'eurobonds': 'Often issued to avoid withholding taxes',
                'fatca': 'US reporting requirements for foreign accounts'
            }
        }

        return {
            'jurisdiction': jurisdiction,
            'considerations': considerations,
            'key_takeaways': [
                'Tax treatment significantly affects after-tax returns',
                'Municipal bonds attractive for high-tax-bracket investors',
                'OID creates tax liability without cash flow',
                'Regulatory treatment affects institutional demand'
            ]
        }

    def calculate_cash_flow_structure(
        self,
        structure_type: str,
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        maturity_years: int = 10,
        frequency: int = 2,
        **kwargs
    ) -> Dict[str, Any]:
        """
        Calculate cash flows for different bond structures.

        Args:
            structure_type: bullet, amortizing, sinking_fund, step_up, deferred
            face_value: Par value
            coupon_rate: Annual coupon rate
            maturity_years: Years to maturity
            frequency: Payments per year

        Returns:
            Dictionary with cash flow schedule
        """
        periods = maturity_years * frequency
        periodic_rate = coupon_rate / frequency
        periodic_coupon = face_value * periodic_rate

        cash_flows = []

        if structure_type == 'bullet':
            # Standard bullet bond - interest only, principal at maturity
            for t in range(1, periods + 1):
                cf = {
                    'period': t,
                    'time_years': t / frequency,
                    'interest': round(periodic_coupon, 2),
                    'principal': round(face_value, 2) if t == periods else 0,
                    'total': round(periodic_coupon + (face_value if t == periods else 0), 2)
                }
                cash_flows.append(cf)

        elif structure_type == 'amortizing':
            # Level payment - principal + interest each period
            if periodic_rate > 0:
                payment = face_value * (periodic_rate * (1 + periodic_rate) ** periods) / \
                         ((1 + periodic_rate) ** periods - 1)
            else:
                payment = face_value / periods

            balance = face_value
            for t in range(1, periods + 1):
                interest = balance * periodic_rate
                principal = payment - interest
                balance -= principal
                cf = {
                    'period': t,
                    'time_years': t / frequency,
                    'interest': round(interest, 2),
                    'principal': round(principal, 2),
                    'total': round(payment, 2),
                    'ending_balance': round(max(balance, 0), 2)
                }
                cash_flows.append(cf)

        elif structure_type == 'sinking_fund':
            # Sinking fund - retire portion each year
            sinking_start = kwargs.get('sinking_start', maturity_years // 2)
            sinking_amount = kwargs.get('sinking_amount', face_value / (maturity_years - sinking_start + 1))

            balance = face_value
            for t in range(1, periods + 1):
                interest = balance * periodic_rate
                year = (t - 1) // frequency + 1

                # Sinking fund payment at year end
                if t % frequency == 0 and year >= sinking_start and year < maturity_years:
                    principal = sinking_amount
                elif t == periods:
                    principal = balance
                else:
                    principal = 0

                balance -= principal
                cf = {
                    'period': t,
                    'time_years': t / frequency,
                    'interest': round(interest, 2),
                    'principal': round(principal, 2),
                    'total': round(interest + principal, 2),
                    'ending_balance': round(max(balance, 0), 2)
                }
                cash_flows.append(cf)

        elif structure_type == 'step_up':
            # Step-up coupon - increases over time
            step_schedule = kwargs.get('step_schedule', [(5, coupon_rate * 1.2), (8, coupon_rate * 1.4)])

            for t in range(1, periods + 1):
                year = (t - 1) // frequency + 1
                current_rate = coupon_rate
                for step_year, step_rate in step_schedule:
                    if year >= step_year:
                        current_rate = step_rate

                interest = face_value * current_rate / frequency
                principal = face_value if t == periods else 0
                cf = {
                    'period': t,
                    'time_years': t / frequency,
                    'coupon_rate': round(current_rate, 4),
                    'interest': round(interest, 2),
                    'principal': round(principal, 2),
                    'total': round(interest + principal, 2)
                }
                cash_flows.append(cf)

        elif structure_type == 'deferred':
            # Deferred coupon - no interest initially
            defer_years = kwargs.get('defer_years', 3)

            for t in range(1, periods + 1):
                year = (t - 1) // frequency + 1
                if year <= defer_years:
                    interest = 0
                else:
                    interest = periodic_coupon

                principal = face_value if t == periods else 0
                cf = {
                    'period': t,
                    'time_years': t / frequency,
                    'interest': round(interest, 2),
                    'principal': round(principal, 2),
                    'total': round(interest + principal, 2),
                    'is_deferred': year <= defer_years
                }
                cash_flows.append(cf)
        else:
            return {'error': f'Unknown structure type: {structure_type}'}

        # Summary statistics
        total_interest = sum(cf['interest'] for cf in cash_flows)
        total_principal = sum(cf['principal'] for cf in cash_flows)

        return {
            'structure_type': structure_type,
            'face_value': face_value,
            'coupon_rate': coupon_rate,
            'maturity_years': maturity_years,
            'frequency': frequency,
            'cash_flows': cash_flows[:24],  # First 24 periods
            'total_periods': len(cash_flows),
            'total_interest': round(total_interest, 2),
            'total_principal': round(total_principal, 2),
            'total_cash_flows': round(total_interest + total_principal, 2)
        }


def run_bond_features_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Main entry point for bond features analysis.

    Args:
        params: Analysis parameters

    Returns:
        Analysis results
    """
    analysis_type = params.get('analysis_type', 'describe_bond')
    analyzer = BondFeaturesAnalyzer()

    try:
        if analysis_type == 'describe_bond':
            return analyzer.describe_bond_type(
                bond_type=params.get('bond_type', 'corporate')
            )

        elif analysis_type == 'covenants':
            return analyzer.analyze_covenants(
                covenant_type=params.get('covenant_type')
            )

        elif analysis_type == 'contingencies':
            return analyzer.analyze_contingency_provisions(
                contingency_type=params.get('contingency_type')
            )

        elif analysis_type == 'legal_tax':
            return analyzer.analyze_legal_tax_considerations(
                jurisdiction=params.get('jurisdiction', 'US')
            )

        elif analysis_type == 'cash_flow_structure':
            return analyzer.calculate_cash_flow_structure(
                structure_type=params.get('structure_type', 'bullet'),
                face_value=params.get('face_value', 1000),
                coupon_rate=params.get('coupon_rate', 0.05),
                maturity_years=params.get('maturity_years', 10),
                frequency=params.get('frequency', 2),
                **{k: v for k, v in params.items() if k not in ['analysis_type', 'structure_type', 'face_value', 'coupon_rate', 'maturity_years', 'frequency']}
            )

        else:
            return {'error': f'Unknown analysis type: {analysis_type}'}

    except Exception as e:
        logger.error(f"Bond features analysis error: {str(e)}")
        return {'error': str(e)}


if __name__ == "__main__":
    import sys
    import json

    if len(sys.argv) > 1:
        try:
            params = json.loads(sys.argv[1])
            result = run_bond_features_analysis(params)
            print(json.dumps(result, indent=2))
        except json.JSONDecodeError as e:
            print(json.dumps({'error': f'Invalid JSON: {str(e)}'}))
    else:
        # Demo
        print("Bond Features Demo:")

        analyzer = BondFeaturesAnalyzer()

        # Describe callable bond
        result = analyzer.describe_bond_type('callable')
        print(f"\nCallable Bond: {result['description']['description']}")

        # Covenant analysis
        result = analyzer.analyze_covenants('negative')
        print(f"\nNegative Covenants: {result['negative_covenants']['description']}")
