"""Deal Structure Analysis Module"""
import sys
from pathlib import Path

# Add Analytics path for absolute imports
analytics_path = Path(__file__).parent.parent.parent
sys.path.insert(0, str(analytics_path))

# Use absolute imports instead of relative imports
from corporateFinance.deal_structure.payment_structure import PaymentStructureAnalyzer, PaymentType
from corporateFinance.deal_structure.earnout_calculator import EarnoutCalculator, EarnoutTranche, EarnoutMetric
from corporateFinance.deal_structure.exchange_ratio import ExchangeRatioCalculator
from corporateFinance.deal_structure.collar_mechanisms import CollarMechanism, CollarType
from corporateFinance.deal_structure.cvr_valuation import CVRValuation, CVRTrigger, CVRType

__all__ = [
    'PaymentStructureAnalyzer',
    'PaymentType',
    'EarnoutCalculator',
    'EarnoutTranche',
    'EarnoutMetric',
    'ExchangeRatioCalculator',
    'CollarMechanism',
    'CollarType',
    'CVRValuation',
    'CVRTrigger',
    'CVRType'
]
