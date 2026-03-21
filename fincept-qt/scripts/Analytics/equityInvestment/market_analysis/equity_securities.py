"""
Equity Securities Overview Module
=================================

Comprehensive analysis of equity security types and characteristics
following CFA curriculum standards.

Covers:
- Common Stock Characteristics and Types
- Preferred Stock Types and Features
- Voting Rights and Ownership Characteristics
- Public vs Private Equity
- Methods for Investing in Non-Domestic Equities (ADR, GDR, Direct)
- Risk and Return Characteristics by Equity Type
- Role of Equity in Company Financing

===== DATA SOURCES REQUIRED =====
INPUT:
  - Security specifications and features
  - Market data for returns analysis
  - Exchange and regulatory information

OUTPUT:
  - Security classification and characteristics
  - Risk/return profiles
  - Investment vehicle comparisons
  - Financing structure analysis
"""

import numpy as np
from typing import List, Dict, Any, Optional, Tuple
from dataclasses import dataclass
from enum import Enum
import json
import sys


class EquityType(Enum):
    """Types of equity securities"""
    COMMON_STOCK = "common_stock"
    PREFERRED_STOCK = "preferred_stock"
    CONVERTIBLE_PREFERRED = "convertible_preferred"
    PARTICIPATING_PREFERRED = "participating_preferred"
    DEPOSITORY_RECEIPT = "depository_receipt"
    PRIVATE_EQUITY = "private_equity"


class VotingRightsType(Enum):
    """Types of voting rights structures"""
    ONE_SHARE_ONE_VOTE = "one_share_one_vote"
    DUAL_CLASS = "dual_class"
    NON_VOTING = "non_voting"
    CUMULATIVE_VOTING = "cumulative_voting"
    STATUTORY_VOTING = "statutory_voting"


class DepositoryReceiptType(Enum):
    """Types of depository receipts"""
    SPONSORED_ADR = "sponsored_adr"
    UNSPONSORED_ADR = "unsponsored_adr"
    GDR = "global_depository_receipt"
    EDR = "european_depository_receipt"


class PrivateEquityStage(Enum):
    """Private equity investment stages"""
    SEED = "seed"
    EARLY_STAGE = "early_stage"
    EXPANSION = "expansion"
    MEZZANINE = "mezzanine"
    BUYOUT = "buyout"
    DISTRESSED = "distressed"


@dataclass
class EquitySecurityCharacteristics:
    """Characteristics of an equity security"""
    equity_type: EquityType
    voting_rights: bool
    voting_type: Optional[VotingRightsType]
    dividend_rights: bool
    dividend_priority: str  # "first", "pari_passu", "residual"
    liquidation_priority: str  # "senior", "subordinate", "residual"
    callable: bool
    convertible: bool
    cumulative_dividends: bool
    participating: bool


class CommonStockAnalyzer:
    """
    Analysis of common stock characteristics and types.
    """

    def describe_common_stock(self) -> Dict[str, Any]:
        """
        Describe fundamental characteristics of common stock.
        """
        return {
            "definition": "Ownership interest representing residual claim on company assets and earnings",
            "key_characteristics": {
                "ownership": "Proportional ownership of the company",
                "voting_rights": "Generally one vote per share (may vary)",
                "dividend_rights": "Residual claim - after preferred dividends",
                "liquidation_rights": "Last priority - after all creditors and preferred",
                "preemptive_rights": "May have right to maintain ownership percentage",
                "limited_liability": "Liability limited to investment amount"
            },
            "types": {
                "classified_shares": {
                    "description": "Different classes with different rights",
                    "examples": ["Class A (voting)", "Class B (non-voting or limited voting)"]
                },
                "founders_shares": {
                    "description": "Special shares for company founders",
                    "features": "Often have super-voting rights"
                }
            },
            "risk_return_profile": {
                "return_sources": ["Capital appreciation", "Dividends"],
                "risk_level": "Higher than bonds and preferred stock",
                "volatility": "Subject to market and company-specific risk"
            }
        }

    def analyze_voting_rights(self, voting_type: VotingRightsType) -> Dict[str, Any]:
        """
        Analyze different voting rights structures.
        """
        voting_info = {
            VotingRightsType.ONE_SHARE_ONE_VOTE: {
                "description": "Traditional structure - each share gets one vote",
                "board_elections": "Shareholders vote based on shares owned",
                "advantage": "Democratic, aligns voting with economic interest",
                "disadvantage": "Large shareholders have proportional control"
            },
            VotingRightsType.DUAL_CLASS: {
                "description": "Different share classes have different voting power",
                "example": "Class A: 1 vote/share, Class B: 10 votes/share",
                "advantage": "Allows founders to maintain control while raising capital",
                "disadvantage": "Misalignment between voting power and economic interest",
                "common_users": ["Tech companies", "Family businesses", "Media companies"]
            },
            VotingRightsType.NON_VOTING: {
                "description": "Shares with no voting rights",
                "use_case": "Raising capital without diluting control",
                "compensation": "May have higher dividends or other preferences"
            },
            VotingRightsType.CUMULATIVE_VOTING: {
                "description": "Shareholders can cumulate votes for director elections",
                "calculation": "Total votes = shares × number of directors",
                "advantage": "Helps minority shareholders elect board members",
                "example": "100 shares × 5 directors = 500 votes to allocate"
            },
            VotingRightsType.STATUTORY_VOTING: {
                "description": "Each share gets one vote per director position",
                "limitation": "Cannot cumulate votes across candidates",
                "effect": "Majority shareholders control all board seats"
            }
        }

        return voting_info.get(voting_type, {"error": "Unknown voting type"})

    def analyze_dual_class_structure(
        self,
        class_a_shares: int,
        class_a_votes_per_share: int,
        class_b_shares: int,
        class_b_votes_per_share: int
    ) -> Dict[str, Any]:
        """
        Analyze control implications of dual-class share structure.
        """
        total_shares = class_a_shares + class_b_shares
        total_votes = (class_a_shares * class_a_votes_per_share +
                      class_b_shares * class_b_votes_per_share)

        class_a_economic = class_a_shares / total_shares
        class_a_voting = (class_a_shares * class_a_votes_per_share) / total_votes

        class_b_economic = class_b_shares / total_shares
        class_b_voting = (class_b_shares * class_b_votes_per_share) / total_votes

        return {
            "class_a": {
                "shares": class_a_shares,
                "votes_per_share": class_a_votes_per_share,
                "total_votes": class_a_shares * class_a_votes_per_share,
                "economic_ownership": class_a_economic * 100,
                "voting_power": class_a_voting * 100
            },
            "class_b": {
                "shares": class_b_shares,
                "votes_per_share": class_b_votes_per_share,
                "total_votes": class_b_shares * class_b_votes_per_share,
                "economic_ownership": class_b_economic * 100,
                "voting_power": class_b_voting * 100
            },
            "analysis": {
                "voting_economic_divergence_a": class_a_voting - class_a_economic,
                "voting_economic_divergence_b": class_b_voting - class_b_economic,
                "governance_implications": self._assess_governance_implications(
                    class_a_voting, class_b_voting
                )
            }
        }

    def _assess_governance_implications(
        self,
        class_a_voting: float,
        class_b_voting: float
    ) -> str:
        if class_b_voting > 0.50:
            return "Class B shareholders have majority control"
        elif class_b_voting > 0.33:
            return "Class B can block major corporate actions requiring supermajority"
        else:
            return "Voting power relatively balanced between classes"


class PreferredStockAnalyzer:
    """
    Analysis of preferred stock characteristics and types.
    """

    def describe_preferred_stock(self) -> Dict[str, Any]:
        """
        Describe fundamental characteristics of preferred stock.
        """
        return {
            "definition": "Hybrid security with characteristics of both debt and equity",
            "key_characteristics": {
                "dividend_priority": "Paid before common stock dividends",
                "fixed_dividend": "Usually pays fixed dividend (like bond coupon)",
                "no_maturity": "Typically perpetual (no maturity date)",
                "limited_voting": "Generally no voting rights (exceptions exist)",
                "liquidation_priority": "Senior to common, junior to debt",
                "no_growth": "Does not participate in company growth (usually)"
            },
            "risk_return_profile": {
                "return_source": "Fixed dividends",
                "risk_level": "Between bonds and common stock",
                "interest_rate_sensitivity": "Price moves inversely with rates"
            }
        }

    def compare_preferred_types(self) -> Dict[str, Any]:
        """
        Compare different types of preferred stock.
        """
        return {
            "cumulative_vs_non_cumulative": {
                "cumulative": {
                    "feature": "Unpaid dividends accumulate as 'dividends in arrears'",
                    "investor_benefit": "Must receive all past due dividends before common",
                    "risk": "Lower - provides dividend protection",
                    "typical_yield": "Lower (compensates for protection)"
                },
                "non_cumulative": {
                    "feature": "Missed dividends are permanently lost",
                    "investor_benefit": "Current dividends paid before common",
                    "risk": "Higher - no recovery of missed dividends",
                    "typical_yield": "Higher (compensates for risk)"
                }
            },
            "participating_vs_non_participating": {
                "participating": {
                    "feature": "Shares in dividends above stated rate with common",
                    "example": "6% preferred that also gets share of extra dividends",
                    "investor_benefit": "Upside potential beyond fixed dividend",
                    "typical_yield": "Lower (has equity-like upside)"
                },
                "non_participating": {
                    "feature": "Limited to stated dividend rate only",
                    "investor_benefit": "Predictable income stream",
                    "typical_yield": "Standard for preferred"
                }
            },
            "convertible": {
                "feature": "Can be converted to common stock at predetermined ratio",
                "conversion_ratio": "Number of common shares per preferred share",
                "investor_benefit": "Downside protection with upside participation",
                "when_to_convert": "When common stock value exceeds preferred value",
                "typical_yield": "Lower (embedded option value)"
            },
            "callable": {
                "feature": "Issuer can redeem shares at specified price",
                "call_price": "Usually at par or slight premium",
                "issuer_benefit": "Refinance if rates decline",
                "investor_risk": "Reinvestment risk if called",
                "typical_yield": "Higher (compensates for call risk)"
            },
            "adjustable_rate": {
                "feature": "Dividend rate adjusts based on reference rate",
                "reference": "Often tied to Treasury rates",
                "investor_benefit": "Protection against rising rates",
                "typical_yield": "Lower initial rate than fixed"
            }
        }

    def calculate_preferred_conversion_value(
        self,
        common_stock_price: float,
        conversion_ratio: float
    ) -> Dict[str, Any]:
        """
        Calculate conversion value and analyze conversion decision.
        """
        conversion_value = common_stock_price * conversion_ratio

        return {
            "common_stock_price": common_stock_price,
            "conversion_ratio": conversion_ratio,
            "conversion_value": conversion_value,
            "interpretation": "Value of preferred if converted to common stock"
        }

    def analyze_conversion_decision(
        self,
        preferred_market_price: float,
        preferred_par_value: float,
        dividend_rate: float,
        common_price: float,
        conversion_ratio: float,
        common_dividend: float
    ) -> Dict[str, Any]:
        """
        Analyze whether to convert preferred to common.
        """
        conversion_value = common_price * conversion_ratio
        premium_over_conversion = (preferred_market_price - conversion_value) / conversion_value

        preferred_annual_dividend = preferred_par_value * dividend_rate
        common_annual_dividend = common_dividend * conversion_ratio

        dividend_advantage = preferred_annual_dividend - common_annual_dividend
        breakeven_years = premium_over_conversion * preferred_market_price / dividend_advantage if dividend_advantage > 0 else float('inf')

        return {
            "preferred_market_price": preferred_market_price,
            "conversion_value": conversion_value,
            "conversion_premium": premium_over_conversion * 100,
            "preferred_annual_income": preferred_annual_dividend,
            "common_annual_income": common_annual_dividend,
            "income_advantage": dividend_advantage,
            "breakeven_years": breakeven_years,
            "recommendation": self._conversion_recommendation(
                premium_over_conversion, dividend_advantage
            )
        }

    def _conversion_recommendation(
        self,
        premium: float,
        dividend_advantage: float
    ) -> str:
        if premium < 0:
            return "Convert - conversion value exceeds preferred price"
        elif dividend_advantage < 0:
            return "Convert - common provides higher income"
        else:
            return f"Hold preferred - income advantage of ${dividend_advantage:.2f}/year"


class PublicPrivateEquityComparison:
    """
    Compare public and private equity investments.
    """

    def compare_characteristics(self) -> Dict[str, Any]:
        """
        Compare public vs private equity characteristics.
        """
        return {
            "public_equity": {
                "definition": "Shares traded on public stock exchanges",
                "characteristics": {
                    "liquidity": "High - can buy/sell quickly",
                    "pricing": "Continuous market-determined prices",
                    "transparency": "High - SEC reporting requirements",
                    "minimum_investment": "Low - can buy single shares",
                    "regulation": "Extensive SEC/exchange oversight",
                    "valuation": "Market price readily available",
                    "diversification": "Easy to achieve",
                    "transaction_costs": "Low - small bid-ask spreads"
                },
                "advantages": [
                    "Liquidity and easy exit",
                    "Transparent pricing",
                    "Low minimum investment",
                    "Easy diversification",
                    "Strong regulatory protection"
                ],
                "disadvantages": [
                    "Market volatility affects prices",
                    "Short-term focus pressure",
                    "Limited access to smaller companies"
                ]
            },
            "private_equity": {
                "definition": "Ownership in companies not publicly traded",
                "characteristics": {
                    "liquidity": "Low - limited secondary market",
                    "pricing": "Infrequent, appraised values",
                    "transparency": "Low - limited disclosure",
                    "minimum_investment": "High - typically $250K+",
                    "regulation": "Less regulated (often exempt)",
                    "valuation": "Complex, based on models",
                    "diversification": "Difficult - large commitments",
                    "transaction_costs": "High - due diligence, legal"
                },
                "advantages": [
                    "Potential for higher returns",
                    "Less short-term pressure",
                    "Access to unique opportunities",
                    "Active ownership/governance",
                    "Lower market correlation"
                ],
                "disadvantages": [
                    "Illiquidity - capital locked up",
                    "High minimum investment",
                    "Complex due diligence",
                    "Less transparency",
                    "Higher fees"
                ]
            }
        }

    def describe_private_equity_stages(self) -> Dict[str, Any]:
        """
        Describe different stages of private equity investment.
        """
        return {
            "venture_capital_stages": {
                PrivateEquityStage.SEED.value: {
                    "description": "Initial funding for concept development",
                    "typical_investment": "$50K - $2M",
                    "company_stage": "Pre-revenue, product development",
                    "risk": "Highest - most fail",
                    "return_potential": "Highest if successful"
                },
                PrivateEquityStage.EARLY_STAGE.value: {
                    "description": "Funding for product launch and initial operations",
                    "typical_investment": "$2M - $15M",
                    "company_stage": "Early revenue, proving model",
                    "risk": "Very high",
                    "return_potential": "High"
                },
                PrivateEquityStage.EXPANSION.value: {
                    "description": "Funding to scale proven business model",
                    "typical_investment": "$10M - $50M",
                    "company_stage": "Growing revenue, expanding market",
                    "risk": "Moderate",
                    "return_potential": "Moderate-high"
                }
            },
            "buyout_stages": {
                PrivateEquityStage.MEZZANINE.value: {
                    "description": "Pre-IPO or late-stage financing",
                    "typical_investment": "$20M - $100M+",
                    "company_stage": "Mature, preparing for exit",
                    "risk": "Lower",
                    "return_potential": "Moderate"
                },
                PrivateEquityStage.BUYOUT.value: {
                    "description": "Acquiring controlling interest in mature company",
                    "types": ["LBO (Leveraged)", "MBO (Management)", "MBI (Management Buy-In)"],
                    "typical_investment": "$50M - $10B+",
                    "risk": "Moderate - established companies",
                    "return_potential": "Moderate"
                },
                PrivateEquityStage.DISTRESSED.value: {
                    "description": "Investing in troubled companies",
                    "strategy": "Restructure and turnaround",
                    "risk": "High - operational challenges",
                    "return_potential": "High if turnaround succeeds"
                }
            }
        }

    def analyze_private_equity_returns(self) -> Dict[str, Any]:
        """
        Explain private equity return measurement.
        """
        return {
            "return_measures": {
                "irr": {
                    "name": "Internal Rate of Return",
                    "description": "Time-weighted return considering cash flow timing",
                    "use": "Standard PE performance metric",
                    "calculation": "Rate where NPV of cash flows = 0"
                },
                "moic": {
                    "name": "Multiple on Invested Capital",
                    "description": "Total value returned divided by capital invested",
                    "use": "Simple return multiple",
                    "example": "2.0x MOIC = doubled investment"
                },
                "dpi": {
                    "name": "Distributions to Paid-In",
                    "description": "Cash returned relative to cash invested",
                    "use": "Realized return measure"
                },
                "rvpi": {
                    "name": "Residual Value to Paid-In",
                    "description": "Remaining NAV relative to invested capital",
                    "use": "Unrealized return measure"
                },
                "tvpi": {
                    "name": "Total Value to Paid-In",
                    "description": "DPI + RVPI",
                    "use": "Total return (realized + unrealized)"
                }
            },
            "j_curve": {
                "description": "Pattern of PE fund returns over time",
                "early_years": "Negative returns (fees, write-downs)",
                "middle_years": "Returns improve as investments mature",
                "later_years": "Positive returns as exits occur",
                "typical_duration": "7-10 years for full cycle"
            }
        }


class DepositoryReceiptAnalyzer:
    """
    Analysis of depositary receipts for international investment.
    """

    def describe_depositary_receipts(self) -> Dict[str, Any]:
        """
        Describe depositary receipt structures and types.
        """
        return {
            "definition": "Negotiable certificate representing shares in a foreign company",
            "structure": {
                "underlying_shares": "Foreign company shares held by depositary bank",
                "ratio": "May represent 1 share, fraction, or multiple shares",
                "trading": "Trade on local exchange in local currency",
                "dividends": "Converted and paid in local currency"
            },
            "types": {
                "adr": {
                    "name": "American Depositary Receipt",
                    "trading_location": "US exchanges (NYSE, NASDAQ)",
                    "currency": "US Dollars",
                    "regulation": "SEC oversight"
                },
                "gdr": {
                    "name": "Global Depositary Receipt",
                    "trading_location": "Multiple markets (often London, Luxembourg)",
                    "currency": "Usually USD or EUR",
                    "use": "Access to multiple markets simultaneously"
                }
            }
        }

    def compare_adr_levels(self) -> Dict[str, Any]:
        """
        Compare different levels of ADR programs.
        """
        return {
            "level_1": {
                "listing": "OTC (Pink Sheets)",
                "sec_registration": "Minimal - Form F-6 only",
                "reporting": "Home country standards only",
                "capital_raising": "No",
                "advantages": [
                    "Low cost to establish",
                    "Minimal regulatory burden",
                    "Quick to implement"
                ],
                "disadvantages": [
                    "Lower visibility",
                    "Limited investor access",
                    "Lower liquidity"
                ]
            },
            "level_2": {
                "listing": "NYSE, NASDAQ, AMEX",
                "sec_registration": "Full - Form 20-F",
                "reporting": "US GAAP reconciliation required",
                "capital_raising": "No",
                "advantages": [
                    "Higher visibility",
                    "Better liquidity",
                    "Broader investor access"
                ],
                "disadvantages": [
                    "Higher compliance costs",
                    "SEC reporting requirements",
                    "No capital raising"
                ]
            },
            "level_3": {
                "listing": "NYSE, NASDAQ, AMEX",
                "sec_registration": "Full - Form F-1/20-F",
                "reporting": "US GAAP or IFRS with reconciliation",
                "capital_raising": "Yes - public offerings",
                "advantages": [
                    "Full capital market access",
                    "Highest visibility",
                    "Best liquidity"
                ],
                "disadvantages": [
                    "Highest compliance costs",
                    "Full SEC reporting",
                    "Sarbanes-Oxley compliance"
                ]
            },
            "rule_144a": {
                "listing": "Private placement",
                "sec_registration": "Exempt",
                "reporting": "Minimal",
                "capital_raising": "Yes - to QIBs only",
                "advantages": [
                    "Quick access to US capital",
                    "Lower compliance costs",
                    "Confidential offering"
                ],
                "disadvantages": [
                    "Limited to qualified buyers",
                    "Less liquid secondary market",
                    "Lower visibility"
                ]
            }
        }

    def compare_investment_methods(self) -> Dict[str, Any]:
        """
        Compare methods for investing in non-domestic equities.
        """
        return {
            "direct_investment": {
                "description": "Buy shares directly on foreign exchange",
                "advantages": [
                    "Full market access",
                    "No currency conversion spread from DR",
                    "Vote directly"
                ],
                "disadvantages": [
                    "Currency risk management required",
                    "Foreign brokerage account needed",
                    "Time zone issues",
                    "Different settlement conventions",
                    "Withholding tax complexity"
                ],
                "best_for": "Sophisticated investors with large positions"
            },
            "depositary_receipts": {
                "description": "Buy ADRs/GDRs on local exchange",
                "advantages": [
                    "Trades in local currency",
                    "Local market hours",
                    "Familiar settlement",
                    "Simplified tax reporting"
                ],
                "disadvantages": [
                    "DR fees reduce returns",
                    "Not all companies have DRs",
                    "Possible tracking error",
                    "Currency exposure still exists"
                ],
                "best_for": "Most individual investors"
            },
            "global_mutual_funds_etfs": {
                "description": "Invest through diversified funds",
                "advantages": [
                    "Instant diversification",
                    "Professional management",
                    "Low minimum investment",
                    "Simple execution"
                ],
                "disadvantages": [
                    "Management fees",
                    "Less control over holdings",
                    "Possible tracking error",
                    "Tax inefficiency possible"
                ],
                "best_for": "Investors seeking diversified exposure"
            }
        }


class EquityRiskReturnAnalyzer:
    """
    Analyze risk and return characteristics of equity securities.
    """

    def compare_equity_risk_return(self) -> Dict[str, Any]:
        """
        Compare risk/return profiles of different equity types.
        """
        return {
            "common_stock": {
                "expected_return": "Highest among equity types",
                "return_sources": ["Dividends", "Capital appreciation"],
                "systematic_risk": "Full market exposure",
                "unsystematic_risk": "Company-specific risk",
                "typical_volatility": "15-25% annually",
                "priority_in_distress": "Lowest - residual claim"
            },
            "preferred_stock": {
                "expected_return": "Between bonds and common",
                "return_sources": ["Fixed dividends"],
                "systematic_risk": "Interest rate sensitive",
                "unsystematic_risk": "Credit risk",
                "typical_volatility": "10-15% annually",
                "priority_in_distress": "Senior to common"
            },
            "convertible_preferred": {
                "expected_return": "Hybrid - depends on conversion",
                "return_sources": ["Dividends", "Conversion value"],
                "systematic_risk": "Mixed equity/fixed income",
                "unsystematic_risk": "Company credit + equity",
                "typical_volatility": "Varies with equity exposure",
                "priority_in_distress": "Senior to common (until converted)"
            },
            "private_equity": {
                "expected_return": "Highest - illiquidity premium",
                "return_sources": ["Capital appreciation", "Dividends/distributions"],
                "systematic_risk": "Lower correlation to public markets",
                "unsystematic_risk": "Very high - concentrated positions",
                "typical_volatility": "Difficult to measure (smoothed)",
                "priority_in_distress": "Depends on structure"
            }
        }

    def calculate_equity_risk_premium(
        self,
        equity_return: float,
        risk_free_rate: float
    ) -> Dict[str, Any]:
        """
        Calculate equity risk premium.
        """
        erp = equity_return - risk_free_rate

        return {
            "equity_return": equity_return,
            "risk_free_rate": risk_free_rate,
            "equity_risk_premium": erp,
            "historical_average": 0.05,  # ~5% historical average
            "interpretation": self._interpret_erp(erp)
        }

    def _interpret_erp(self, erp: float) -> str:
        if erp > 0.08:
            return "High ERP - investors demanding high compensation for risk"
        elif erp > 0.04:
            return "Normal ERP - consistent with historical averages"
        elif erp > 0:
            return "Low ERP - may indicate expensive equity valuations"
        else:
            return "Negative ERP - anomalous, typically during bubbles"


class EquityFinancingRole:
    """
    Analyze role of equity in company financing.
    """

    def describe_equity_financing_role(self) -> Dict[str, Any]:
        """
        Describe how equity is used to finance company assets.
        """
        return {
            "sources_of_equity": {
                "retained_earnings": {
                    "description": "Profits reinvested in the business",
                    "cost": "Implicit - opportunity cost of dividends",
                    "advantages": "No dilution, no issuance costs",
                    "limitations": "Limited by profitability"
                },
                "new_share_issuance": {
                    "description": "Selling new shares to investors",
                    "cost": "Explicit cost of equity + issuance costs",
                    "advantages": "No mandatory payments, no maturity",
                    "limitations": "Dilutes existing shareholders"
                },
                "private_placements": {
                    "description": "Selling shares to select investors",
                    "cost": "Negotiated terms",
                    "advantages": "Faster, lower costs than public",
                    "limitations": "Limited investor pool"
                }
            },
            "equity_vs_debt_tradeoffs": {
                "equity_advantages": [
                    "No mandatory payments",
                    "No maturity date",
                    "No collateral required",
                    "Improves debt capacity",
                    "Permanent capital"
                ],
                "equity_disadvantages": [
                    "Higher cost than debt (after tax)",
                    "Dilutes ownership and control",
                    "Dividends not tax-deductible",
                    "Issuance costs higher"
                ],
                "when_equity_preferred": [
                    "High business risk",
                    "Uncertain cash flows",
                    "Growth opportunities",
                    "High existing leverage"
                ]
            },
            "balance_sheet_role": {
                "equity_ratio": "Total Equity / Total Assets",
                "interpretation": "Proportion of assets financed by owners",
                "industry_variation": "Varies by industry and business model"
            }
        }

    def calculate_financing_mix(
        self,
        total_assets: float,
        total_equity: float,
        total_debt: float
    ) -> Dict[str, Any]:
        """
        Analyze company's financing mix.
        """
        equity_ratio = total_equity / total_assets
        debt_ratio = total_debt / total_assets
        debt_to_equity = total_debt / total_equity

        return {
            "total_assets": total_assets,
            "total_equity": total_equity,
            "total_debt": total_debt,
            "equity_ratio": equity_ratio,
            "debt_ratio": debt_ratio,
            "debt_to_equity": debt_to_equity,
            "interpretation": self._interpret_financing_mix(equity_ratio)
        }

    def _interpret_financing_mix(self, equity_ratio: float) -> str:
        if equity_ratio > 0.6:
            return "Conservative financing - primarily equity financed"
        elif equity_ratio > 0.4:
            return "Balanced financing mix"
        else:
            return "Aggressive financing - primarily debt financed"


def main():
    """CLI entry point for equity securities analysis."""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Command required",
            "available_commands": [
                "common_stock",
                "preferred_types",
                "voting_rights",
                "dual_class",
                "public_vs_private",
                "private_equity_stages",
                "adr_levels",
                "investment_methods",
                "risk_return",
                "equity_financing"
            ]
        }))
        return

    command = sys.argv[1]

    try:
        if command == "common_stock":
            analyzer = CommonStockAnalyzer()
            result = analyzer.describe_common_stock()
            print(json.dumps(result, indent=2))

        elif command == "preferred_types":
            analyzer = PreferredStockAnalyzer()
            result = analyzer.compare_preferred_types()
            print(json.dumps(result, indent=2))

        elif command == "voting_rights":
            analyzer = CommonStockAnalyzer()
            result = {
                "cumulative": analyzer.analyze_voting_rights(VotingRightsType.CUMULATIVE_VOTING),
                "statutory": analyzer.analyze_voting_rights(VotingRightsType.STATUTORY_VOTING),
                "dual_class": analyzer.analyze_voting_rights(VotingRightsType.DUAL_CLASS)
            }
            print(json.dumps(result, indent=2))

        elif command == "dual_class":
            analyzer = CommonStockAnalyzer()
            result = analyzer.analyze_dual_class_structure(
                class_a_shares=10000000,
                class_a_votes_per_share=1,
                class_b_shares=1000000,
                class_b_votes_per_share=10
            )
            print(json.dumps(result, indent=2))

        elif command == "public_vs_private":
            comparison = PublicPrivateEquityComparison()
            result = comparison.compare_characteristics()
            print(json.dumps(result, indent=2))

        elif command == "private_equity_stages":
            comparison = PublicPrivateEquityComparison()
            result = comparison.describe_private_equity_stages()
            print(json.dumps(result, indent=2, default=str))

        elif command == "adr_levels":
            analyzer = DepositoryReceiptAnalyzer()
            result = analyzer.compare_adr_levels()
            print(json.dumps(result, indent=2))

        elif command == "investment_methods":
            analyzer = DepositoryReceiptAnalyzer()
            result = analyzer.compare_investment_methods()
            print(json.dumps(result, indent=2))

        elif command == "risk_return":
            analyzer = EquityRiskReturnAnalyzer()
            result = analyzer.compare_equity_risk_return()
            print(json.dumps(result, indent=2))

        elif command == "equity_financing":
            analyzer = EquityFinancingRole()
            result = analyzer.describe_equity_financing_role()
            print(json.dumps(result, indent=2))

        else:
            print(json.dumps({"error": f"Unknown command: {command}"}))

    except Exception as e:
        print(json.dumps({"error": str(e)}))


if __name__ == "__main__":
    main()
