"""
Structured Products Module - Principal Protection Notes, FRATs, etc.

Analysis: Structured Investment Products (THE UGLY)
Rating: 1/10 - Among worst products; complex, expensive, opaque
"""

from decimal import Decimal, getcontext
from typing import Dict, Any
from config import AssetParameters, AssetClass
from base_analytics import AlternativeInvestmentBase

getcontext().prec = 28


class StructuredProductAnalyzer(AlternativeInvestmentBase):
    """
    THE UGLY
    - Principal Protection Notes (PPNs)
    - FRAT structures
    - Market-linked CDs
    - Complexity hides fees
    - Illiquid, expensive, unnecessary
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.principal = getattr(parameters, 'principal', Decimal('10000'))
        self.participation_rate = getattr(parameters, 'participation_rate', Decimal('0.80'))  # 80% typical
        self.cap_rate = getattr(parameters, 'cap_rate', Decimal('0.10'))  # 10% cap
        self.term_years = getattr(parameters, 'term_years', 5)
        self.embedded_cost = getattr(parameters, 'embedded_cost', Decimal('0.03'))  # 3% annual

    def analysis_verdict(self) -> Dict[str, Any]:
        return {
            'product': 'Structured Products',
            'category': 'THE UGLY',
            'rating': '1/10',
            'analysis_summary': 'Complex, expensive, illiquid - avoid entirely',
            'key_problems': [
                'COMPLEXITY: Intentionally confusing to hide costs',
                'HIDDEN FEES: 2-4% embedded costs',
                'ILLIQUIDITY: No secondary market',
                'CREDIT RISK: Depends on issuer solvency',
                'OPPORTUNITY COST: Miss upside vs direct equity',
                'TAX INEFFICIENCY: Often ordinary income treatment'
            ],
            'analysis_quote': '"If you can\'t explain it, don\'t buy it. Structured products fail this test."',
            'better_alternative': 'Buy stocks + bonds separately - transparent, liquid, low-cost',
            'bottom_line': 'Banks profit, investors lose - always avoid'
        }

    def calculate_nav(self) -> Decimal:
        return self.principal

    def calculate_key_metrics(self) -> Dict[str, Any]:
        return {
            'participation_rate': float(self.participation_rate),
            'cap_rate': float(self.cap_rate),
            'embedded_annual_cost': float(self.embedded_cost),
            'analysis_category': 'THE UGLY'
        }


__all__ = ['StructuredProductAnalyzer']
