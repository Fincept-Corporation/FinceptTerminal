"""
Fixed Income Market Structure Module
====================================

Market segments, issuers, trading mechanisms, indexes, and funding markets
implementing CFA Institute curriculum for fixed income markets.

===== CFA CURRICULUM COVERAGE =====
- Fixed-income market segments and participants
- Types of fixed-income indexes
- Primary vs secondary markets comparison
- Short-term funding alternatives (CP, repos)
- Repurchase agreements (repos)
- Investment-grade vs high-yield funding
- Government vs corporate issuance

PARAMETERS:
  - market_type: primary, secondary, money_market
  - issuer_type: corporate, government, municipal
  - instrument_type: cp, repo, bond
"""

from dataclasses import dataclass, field
from typing import Dict, Any, List, Optional, Tuple
from enum import Enum
from datetime import date, timedelta
import numpy as np
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class MarketSegment(Enum):
    """Fixed income market segments"""
    GOVERNMENT = "government"
    CORPORATE_IG = "corporate_investment_grade"
    CORPORATE_HY = "corporate_high_yield"
    MUNICIPAL = "municipal"
    AGENCY = "agency"
    MBS = "mortgage_backed"
    ABS = "asset_backed"
    MONEY_MARKET = "money_market"
    EMERGING_MARKETS = "emerging_markets"


class TradingVenue(Enum):
    """Trading venue types"""
    EXCHANGE = "exchange"
    OTC = "over_the_counter"
    ATS = "alternative_trading_system"
    DEALER = "dealer_market"


class FundingType(Enum):
    """Short-term funding types"""
    COMMERCIAL_PAPER = "commercial_paper"
    REPO = "repo"
    BANK_LOAN = "bank_loan"
    CREDIT_LINE = "credit_line"
    ASSET_BACKED_CP = "abcp"


class MarketStructureAnalyzer:
    """
    Fixed income market structure analysis.

    Provides comprehensive analysis of market segments, participants,
    and trading mechanisms per CFA curriculum.
    """

    def describe_market_segments(
        self,
        segment: str = None,
    ) -> Dict[str, Any]:
        """
        Describe fixed income market segments.

        Args:
            segment: Specific segment to describe (None for all)

        Returns:
            Dictionary with market segment descriptions
        """
        segments = {
            'government': {
                'name': 'Government Bond Market',
                'issuers': ['Sovereign governments', 'Central banks'],
                'size': 'Largest fixed income market globally',
                'key_markets': {
                    'us_treasury': {
                        'instruments': ['T-bills (≤1yr)', 'T-notes (2-10yr)', 'T-bonds (>10yr)', 'TIPS'],
                        'features': 'Most liquid, benchmark rates, full faith and credit'
                    },
                    'uk_gilts': {
                        'instruments': ['Conventional gilts', 'Index-linked gilts'],
                        'features': 'Deep liquid market'
                    },
                    'german_bunds': {
                        'instruments': ['Bunds', 'Bobls', 'Schatz'],
                        'features': 'Euro benchmark, lowest yield in eurozone'
                    },
                    'jgbs': {
                        'instruments': ['JGBs of various tenors'],
                        'features': 'Largest sovereign market, mostly domestic held'
                    }
                },
                'investors': ['Central banks', 'Pension funds', 'Insurance companies', 'Banks', 'Money market funds'],
                'trading': 'Primarily dealer market, some electronic platforms'
            },
            'corporate_ig': {
                'name': 'Investment Grade Corporate Bond Market',
                'issuers': ['Large corporations with BBB- or higher ratings'],
                'credit_quality': 'BBB-/Baa3 and above',
                'characteristics': {
                    'spread_range': '50-200bps over Treasuries typically',
                    'liquidity': 'Good for large issues, varies for smaller',
                    'covenant_protection': 'Generally minimal (covenant-lite)',
                    'issuance': 'Both public SEC-registered and 144A private'
                },
                'sub_segments': ['Financial', 'Industrial', 'Utility'],
                'investors': ['Insurance companies', 'Pension funds', 'Mutual funds', 'Banks'],
                'trading': 'OTC dealer market, increasing electronic trading'
            },
            'corporate_hy': {
                'name': 'High Yield (Junk) Bond Market',
                'issuers': ['Companies rated BB+ or below', 'Fallen angels', 'LBO financing'],
                'credit_quality': 'BB+/Ba1 and below',
                'characteristics': {
                    'spread_range': '300-1000+ bps over Treasuries',
                    'default_rate': '2-4% average, cyclical',
                    'recovery_rate': '40-50% average',
                    'covenant_protection': 'More restrictive than IG'
                },
                'key_considerations': [
                    'Higher credit risk',
                    'Greater spread volatility',
                    'Economic sensitivity',
                    'Call features common'
                ],
                'investors': ['High yield funds', 'Hedge funds', 'Insurance companies', 'CLOs'],
                'trading': 'Less liquid than IG, higher bid-ask spreads'
            },
            'municipal': {
                'name': 'Municipal Bond Market',
                'issuers': ['State governments', 'Local governments', 'Agencies', 'Authorities'],
                'types': {
                    'general_obligation': 'Backed by taxing power',
                    'revenue': 'Backed by specific revenue stream (tolls, utilities)'
                },
                'tax_treatment': {
                    'federal': 'Generally exempt from federal income tax',
                    'state_local': 'May be exempt if in-state investor',
                    'amt': 'Some issues subject to Alternative Minimum Tax'
                },
                'key_considerations': [
                    'Tax-equivalent yield calculation',
                    'Credit quality varies widely',
                    'State and local economy exposure',
                    'Call risk significant'
                ],
                'investors': ['High-net-worth individuals', 'Muni bond funds', 'Banks', 'Insurance'],
                'trading': 'Fragmented, less liquid, dealer market'
            },
            'money_market': {
                'name': 'Money Market',
                'instruments': {
                    'treasury_bills': 'Government short-term debt (≤1 year)',
                    'commercial_paper': 'Corporate short-term notes (≤270 days)',
                    'repos': 'Secured lending against collateral',
                    'fed_funds': 'Overnight interbank lending',
                    'cds': 'Bank certificates of deposit',
                    'bankers_acceptances': 'Trade finance instruments',
                    'eurodollar_deposits': 'USD deposits outside US'
                },
                'characteristics': {
                    'maturity': 'One year or less',
                    'credit_quality': 'Generally high quality',
                    'liquidity': 'Highly liquid',
                    'purpose': 'Cash management, liquidity'
                },
                'investors': ['Money market funds', 'Corporations', 'Banks', 'Governments'],
                'trading': 'Dealer market, electronic platforms'
            },
            'mbs_abs': {
                'name': 'Securitized Products Market',
                'types': {
                    'agency_mbs': 'Fannie Mae, Freddie Mac, Ginnie Mae',
                    'non_agency_mbs': 'Private label RMBS',
                    'cmbs': 'Commercial mortgage-backed',
                    'abs': 'Auto loans, credit cards, student loans, equipment',
                    'clo': 'Collateralized loan obligations'
                },
                'characteristics': {
                    'prepayment_risk': 'Key for MBS',
                    'credit_enhancement': 'Subordination, overcollateralization',
                    'complexity': 'Requires specialized analysis'
                },
                'investors': ['Banks', 'Insurance', 'Money managers', 'Hedge funds'],
                'trading': 'TBA market for agency MBS, OTC for others'
            }
        }

        if segment:
            segment_lower = segment.lower().replace(' ', '_').replace('-', '_')
            if segment_lower in segments:
                return {'segment': segment, 'details': segments[segment_lower]}
            else:
                return {'error': f'Unknown segment: {segment}', 'available': list(segments.keys())}

        return {'market_segments': segments}

    def describe_fixed_income_indexes(
        self,
    ) -> Dict[str, Any]:
        """
        Describe types and characteristics of fixed income indexes.

        Returns:
            Dictionary with index information
        """
        indexes = {
            'broad_market_indexes': {
                'bloomberg_aggregate': {
                    'name': 'Bloomberg US Aggregate Bond Index',
                    'coverage': 'Investment-grade, USD-denominated, fixed-rate taxable',
                    'components': ['Treasury', 'Government-related', 'Corporate', 'MBS', 'ABS', 'CMBS'],
                    'criteria': 'Minimum $300M outstanding, IG rated, >1yr maturity',
                    'use': 'Primary US bond market benchmark'
                },
                'global_aggregate': {
                    'name': 'Bloomberg Global Aggregate',
                    'coverage': 'Multi-currency global IG bonds',
                    'use': 'Global fixed income benchmark'
                }
            },
            'government_indexes': {
                'treasury_index': 'US Treasury securities only',
                'tips_index': 'Inflation-protected securities',
                'agency_index': 'Government agency securities'
            },
            'corporate_indexes': {
                'ig_corporate': {
                    'name': 'Bloomberg US Corporate Index',
                    'coverage': 'Investment-grade US corporate bonds',
                    'sub_indexes': ['Financial', 'Industrial', 'Utility']
                },
                'high_yield': {
                    'name': 'Bloomberg US High Yield Index',
                    'coverage': 'Below investment-grade corporates',
                    'criteria': 'Ba1/BB+ or below rated'
                }
            },
            'securitized_indexes': {
                'mbs_index': 'Agency mortgage-backed securities',
                'abs_index': 'Asset-backed securities',
                'cmbs_index': 'Commercial mortgage-backed'
            },
            'municipal_indexes': {
                'muni_bond_index': 'Investment-grade municipal bonds',
                'high_yield_muni': 'Below-IG municipal bonds'
            },
            'international_indexes': {
                'em_bond_index': 'Emerging market sovereign and corporate',
                'euro_aggregate': 'Euro-denominated bonds',
                'pan_european': 'European multi-currency'
            },
            'index_construction_considerations': {
                'weighting': 'Market-value weighted (larger issues have more weight)',
                'rebalancing': 'Monthly typically',
                'inclusion_criteria': 'Size, rating, maturity requirements',
                'limitations': [
                    'Bums problem - more debt = higher index weight',
                    'Turnover from rating changes',
                    'Liquidity differences from index weights',
                    'New issues enter at market prices (not par)'
                ]
            },
            'using_indexes': {
                'benchmarking': 'Measure portfolio performance',
                'passive_investing': 'Index funds and ETFs track indexes',
                'market_analysis': 'Understand market segments and returns',
                'risk_measurement': 'Duration, credit quality metrics'
            }
        }

        return indexes

    def compare_primary_secondary_markets(
        self,
    ) -> Dict[str, Any]:
        """
        Compare primary and secondary fixed income markets.

        Returns:
            Dictionary with market comparison
        """
        comparison = {
            'primary_market': {
                'definition': 'Market where new securities are issued and sold to initial investors',
                'bond_issuance_process': {
                    'underwriting': {
                        'firm_commitment': 'Underwriter guarantees proceeds, takes inventory risk',
                        'best_efforts': 'Underwriter sells without guarantee',
                        'bought_deal': 'Underwriter buys entire issue for resale'
                    },
                    'syndicate': 'Group of underwriters share risk and distribution',
                    'book_building': 'Process of collecting investor orders to set price'
                },
                'issuance_types': {
                    'public_offering': 'SEC-registered, available to all investors',
                    'rule_144a': 'Private placement to qualified institutional buyers (QIBs)',
                    'private_placement': 'Direct sale to limited investors'
                },
                'pricing': {
                    'spread_to_benchmark': 'Priced as spread over comparable Treasury',
                    'new_issue_concession': 'New issues often priced slightly cheaper',
                    'market_conditions': 'Pricing depends on market sentiment'
                },
                'participants': ['Issuers', 'Investment banks', 'Institutional investors', 'Rating agencies']
            },
            'secondary_market': {
                'definition': 'Market where previously issued securities trade between investors',
                'trading_mechanisms': {
                    'dealer_market': {
                        'description': 'Dealers quote bid/ask prices, make markets',
                        'dominant_for': 'Most bond trading'
                    },
                    'electronic_platforms': {
                        'request_for_quote': 'Investors request prices from dealers',
                        'all_to_all': 'Any participant can trade with any other',
                        'examples': 'MarketAxess, Tradeweb, Bloomberg'
                    },
                    'exchange_trading': 'Limited for bonds (some ETFs, retail platforms)'
                },
                'liquidity_factors': {
                    'issue_size': 'Larger issues more liquid',
                    'age': 'On-the-run more liquid than off-the-run',
                    'credit_quality': 'IG more liquid than HY',
                    'complexity': 'Plain vanilla more liquid than structured'
                },
                'participants': ['Dealers', 'Asset managers', 'Hedge funds', 'Insurance', 'Pension funds']
            },
            'comparison_to_equity_markets': {
                'trading_venue': {
                    'bonds': 'Primarily OTC, dealer-based',
                    'equity': 'Primarily exchange-traded'
                },
                'transparency': {
                    'bonds': 'Less transparent, TRACE provides some post-trade data',
                    'equity': 'Real-time quotes and trades visible'
                },
                'liquidity': {
                    'bonds': 'Fragmented across many issues, varies significantly',
                    'equity': 'Concentrated in fewer securities, generally more liquid'
                },
                'transaction_costs': {
                    'bonds': 'Wider bid-ask spreads, especially for smaller trades',
                    'equity': 'Tighter spreads, more standardized'
                },
                'settlement': {
                    'bonds': 'T+1 or T+2 typically',
                    'equity': 'T+1 standard'
                }
            }
        }

        return comparison

    def analyze_short_term_funding(
        self,
        funding_type: str = None,
    ) -> Dict[str, Any]:
        """
        Analyze short-term funding alternatives for corporations.

        Args:
            funding_type: Specific type to analyze

        Returns:
            Dictionary with funding analysis
        """
        funding_options = {
            'commercial_paper': {
                'description': 'Short-term unsecured promissory notes',
                'issuers': 'Large, creditworthy corporations and financial institutions',
                'maturity': '1 to 270 days (to avoid SEC registration)',
                'typical_maturity': '30-60 days average',
                'minimum_size': 'Usually $100,000 minimum',
                'credit_quality': 'Must be investment grade (A1/P1 for best rates)',
                'pricing': {
                    'basis': 'Discount to face value',
                    'spread': 'Typically 10-50 bps over T-bills for top-tier',
                    'formula': 'Price = Face × (1 - Rate × Days/360)'
                },
                'advantages': [
                    'Lower cost than bank loans',
                    'Flexibility in amount and timing',
                    'No collateral required'
                ],
                'disadvantages': [
                    'Requires backup credit line',
                    'Market access can disappear in crisis',
                    'Rating dependent'
                ],
                'backup_facility': 'Credit line required to assure investors of rollover ability'
            },
            'repo': {
                'description': 'Sale of securities with agreement to repurchase',
                'structure': {
                    'repo': 'Borrower sells securities, agrees to buy back',
                    'reverse_repo': 'Lender buys securities, agrees to sell back'
                },
                'collateral': {
                    'treasury': 'Lowest haircut (0-2%)',
                    'agency': 'Low haircut (2-5%)',
                    'corporate': 'Higher haircut (5-15%)',
                    'equity': 'Highest haircut (15-25%)'
                },
                'maturity': {
                    'overnight': 'Most common',
                    'term_repo': 'Days to months',
                    'open_repo': 'Rolls daily until terminated'
                },
                'pricing': {
                    'repo_rate': 'Interest rate on the loan',
                    'spread': 'Typically near fed funds/SOFR',
                    'haircut': 'Excess collateral required'
                },
                'uses': [
                    'Short-term financing',
                    'Leveraging bond portfolios',
                    'Central bank operations',
                    'Securities lending'
                ],
                'risks': [
                    'Counterparty risk',
                    'Collateral value fluctuation',
                    'Rollover/refinancing risk',
                    'Fire sale risk in stress'
                ]
            },
            'bank_credit_lines': {
                'types': {
                    'committed': {
                        'description': 'Bank legally obligated to lend',
                        'fee': 'Commitment fee on unused portion',
                        'use': 'Backup for CP, working capital'
                    },
                    'uncommitted': {
                        'description': 'Bank can refuse to lend',
                        'fee': 'No commitment fee',
                        'use': 'Convenience, may not be available in stress'
                    },
                    'revolving': {
                        'description': 'Can borrow, repay, re-borrow',
                        'maturity': 'Usually 3-5 years',
                        'pricing': 'SOFR + spread based on rating'
                    }
                },
                'covenants': 'Financial covenants often required',
                'advantages': ['Certainty of access', 'Flexible draw'],
                'disadvantages': ['Higher cost than CP', 'Covenant restrictions']
            },
            'asset_backed_cp': {
                'description': 'Commercial paper backed by asset pool',
                'structure': 'Conduit issues CP, uses proceeds to buy assets',
                'assets': ['Trade receivables', 'Auto loans', 'Credit cards'],
                'credit_enhancement': 'Overcollateralization, liquidity support',
                'use': 'Corporations monetize receivables, off-balance sheet',
                'advantages': ['Lower rate if assets high quality', 'Off-balance sheet'],
                'disadvantages': ['Complexity', 'Conduit risk', '2008 crisis issues']
            }
        }

        if funding_type:
            funding_lower = funding_type.lower().replace(' ', '_').replace('-', '_')
            # Handle common aliases
            if funding_lower in ['cp', 'commercial_paper']:
                funding_lower = 'commercial_paper'
            elif funding_lower in ['repo', 'repurchase', 'repurchase_agreement']:
                funding_lower = 'repo'
            elif funding_lower in ['credit_line', 'bank_loan', 'revolver']:
                funding_lower = 'bank_credit_lines'
            elif funding_lower in ['abcp', 'asset_backed_cp']:
                funding_lower = 'asset_backed_cp'

            if funding_lower in funding_options:
                return {'funding_type': funding_type, 'details': funding_options[funding_lower]}
            else:
                return {'error': f'Unknown funding type: {funding_type}', 'available': list(funding_options.keys())}

        return {'short_term_funding_options': funding_options}

    def analyze_repo_mechanics(
        self,
        collateral_value: float,
        haircut: float = 0.02,
        repo_rate: float = 0.05,
        term_days: int = 1,
    ) -> Dict[str, Any]:
        """
        Calculate repo transaction mechanics.

        Args:
            collateral_value: Market value of collateral
            haircut: Haircut percentage (e.g., 0.02 = 2%)
            repo_rate: Annual repo rate
            term_days: Number of days

        Returns:
            Dictionary with repo calculations
        """
        # Cash received (collateral less haircut)
        cash_received = collateral_value * (1 - haircut)

        # Repurchase price
        interest = cash_received * repo_rate * (term_days / 360)
        repurchase_price = cash_received + interest

        # Implied margin/equity
        margin = collateral_value - cash_received

        # Leverage ratio
        leverage = collateral_value / margin if margin > 0 else float('inf')

        return {
            'transaction_summary': {
                'collateral_market_value': round(collateral_value, 2),
                'haircut': f'{haircut * 100}%',
                'cash_received': round(cash_received, 2),
                'repo_rate': f'{repo_rate * 100}%',
                'term_days': term_days,
                'interest_cost': round(interest, 2),
                'repurchase_price': round(repurchase_price, 2)
            },
            'risk_metrics': {
                'margin_equity': round(margin, 2),
                'leverage_ratio': round(leverage, 2),
                'margin_call_trigger': 'If collateral value falls, margin call may occur'
            },
            'example_scenario': {
                'if_collateral_drops_5pct': {
                    'new_value': round(collateral_value * 0.95, 2),
                    'margin_deficit': round(collateral_value * 0.95 - cash_received - margin, 2),
                    'action': 'Borrower must post additional collateral or cash'
                }
            }
        }

    def compare_ig_vs_hy_funding(
        self,
    ) -> Dict[str, Any]:
        """
        Compare investment-grade vs high-yield corporate funding.

        Returns:
            Dictionary with funding comparison
        """
        comparison = {
            'investment_grade': {
                'credit_quality': 'BBB-/Baa3 or higher',
                'market_access': {
                    'bond_market': 'Full access to public and 144A markets',
                    'commercial_paper': 'Access to CP market',
                    'bank_loans': 'Favorable terms, lower spreads',
                    'credit_facilities': 'Committed facilities at low cost'
                },
                'typical_terms': {
                    'bond_maturity': '3-30 years',
                    'coupon': 'Fixed or floating',
                    'covenants': 'Minimal (incurrence-based)',
                    'call_features': 'Make-whole call common'
                },
                'pricing': {
                    'spread_range': '50-200 bps over Treasuries',
                    'factors': 'Rating, industry, maturity, market conditions'
                },
                'investor_base': 'Insurance, pension funds, mutual funds, banks'
            },
            'high_yield': {
                'credit_quality': 'BB+/Ba1 or lower',
                'market_access': {
                    'bond_market': '144A and public HY market',
                    'commercial_paper': 'Generally no access',
                    'bank_loans': 'Leveraged loan market',
                    'credit_facilities': 'Available but at higher cost with covenants'
                },
                'typical_terms': {
                    'bond_maturity': '5-10 years typically',
                    'coupon': 'Higher fixed rate, some PIK',
                    'covenants': 'More restrictive (maintenance covenants)',
                    'call_features': 'Non-call period, then callable at premium'
                },
                'pricing': {
                    'spread_range': '300-1000+ bps over Treasuries',
                    'factors': 'Rating, leverage, industry, cash flow stability'
                },
                'investor_base': 'HY funds, hedge funds, CLOs, crossover investors'
            },
            'key_differences': {
                'cost_of_capital': 'HY significantly higher (300-500+ bps more)',
                'covenant_package': 'HY has tighter restrictions',
                'market_access_reliability': 'IG more stable; HY market can close in stress',
                'refinancing_risk': 'Higher for HY issuers',
                'call_provisions': 'HY bonds callable after non-call period; IG make-whole',
                'security': 'HY more likely to be secured'
            },
            'fallen_angel_dynamics': {
                'definition': 'IG issuer downgraded to HY',
                'impact': [
                    'Forced selling by IG-only investors',
                    'Higher funding costs going forward',
                    'Potential covenant triggers',
                    'May create value opportunity for HY investors'
                ]
            },
            'rising_star_dynamics': {
                'definition': 'HY issuer upgraded to IG',
                'impact': [
                    'Spread compression',
                    'Access to broader investor base',
                    'Lower future funding costs',
                    'Index inclusion effects'
                ]
            }
        }

        return comparison


def run_market_structure_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Main entry point for market structure analysis.

    Args:
        params: Analysis parameters

    Returns:
        Analysis results
    """
    analysis_type = params.get('analysis_type', 'market_segments')
    analyzer = MarketStructureAnalyzer()

    try:
        if analysis_type == 'market_segments':
            return analyzer.describe_market_segments(
                segment=params.get('segment')
            )

        elif analysis_type == 'indexes':
            return analyzer.describe_fixed_income_indexes()

        elif analysis_type == 'primary_secondary':
            return analyzer.compare_primary_secondary_markets()

        elif analysis_type == 'short_term_funding':
            return analyzer.analyze_short_term_funding(
                funding_type=params.get('funding_type')
            )

        elif analysis_type == 'repo_mechanics':
            return analyzer.analyze_repo_mechanics(
                collateral_value=params.get('collateral_value', 1000000),
                haircut=params.get('haircut', 0.02),
                repo_rate=params.get('repo_rate', 0.05),
                term_days=params.get('term_days', 1)
            )

        elif analysis_type == 'ig_vs_hy':
            return analyzer.compare_ig_vs_hy_funding()

        else:
            return {'error': f'Unknown analysis type: {analysis_type}'}

    except Exception as e:
        logger.error(f"Market structure analysis error: {str(e)}")
        return {'error': str(e)}


if __name__ == "__main__":
    import sys
    import json

    if len(sys.argv) > 1:
        try:
            params = json.loads(sys.argv[1])
            result = run_market_structure_analysis(params)
            print(json.dumps(result, indent=2))
        except json.JSONDecodeError as e:
            print(json.dumps({'error': f'Invalid JSON: {str(e)}'}))
    else:
        # Demo
        print("Market Structure Demo:")

        analyzer = MarketStructureAnalyzer()

        # Repo mechanics
        result = analyzer.analyze_repo_mechanics(
            collateral_value=1000000, haircut=0.02, repo_rate=0.05, term_days=7
        )
        print(f"\nRepo: Borrow ${result['transaction_summary']['cash_received']:,.0f} against $1M collateral")
        print(f"Repurchase at ${result['transaction_summary']['repurchase_price']:,.0f}")
