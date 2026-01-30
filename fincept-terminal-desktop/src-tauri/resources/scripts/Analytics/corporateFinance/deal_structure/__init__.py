"""Deal Structure Analysis Module"""
from .payment_structure import PaymentStructureAnalyzer, PaymentType
from .earnout_calculator import EarnoutCalculator, EarnoutTranche, EarnoutMetric
from .exchange_ratio import ExchangeRatioCalculator
from .collar_mechanisms import CollarMechanism, CollarType
from .cvr_valuation import CVRValuation, CVRTrigger, CVRType

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
