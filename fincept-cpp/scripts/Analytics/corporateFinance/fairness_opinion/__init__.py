"""Fairness Opinion Framework Module"""
import sys
from pathlib import Path

# Add Analytics path for absolute imports
analytics_path = Path(__file__).parent.parent.parent
sys.path.insert(0, str(analytics_path))

# Use absolute imports instead of relative imports
from corporateFinance.fairness_opinion.valuation_framework import FairnessOpinionFramework, ValuationMethod, ValuationRange
from corporateFinance.fairness_opinion.premium_analysis import PremiumAnalysis
from corporateFinance.fairness_opinion.process_quality import ProcessQualityAssessment, ProcessType, Bidder, BidderType

__all__ = [
    'FairnessOpinionFramework',
    'ValuationMethod',
    'ValuationRange',
    'PremiumAnalysis',
    'ProcessQualityAssessment',
    'ProcessType',
    'Bidder',
    'BidderType'
]
