"""Synergy Analysis Module"""
import sys
from pathlib import Path

# Add Analytics path for absolute imports
analytics_path = Path(__file__).parent.parent.parent
sys.path.insert(0, str(analytics_path))

# Use absolute imports instead of relative imports
from corporateFinance.synergies.revenue_synergies import RevenueSynergyAnalyzer, RevenueSynergyType, SynergyOpportunity
from corporateFinance.synergies.cost_synergies import CostSynergyAnalyzer, CostSynergyType, CostReduction
from corporateFinance.synergies.integration_costs import IntegrationCostAnalyzer, IntegrationCategory, IntegrationCost
from corporateFinance.synergies.synergy_valuation import SynergyValuation

__all__ = [
    'RevenueSynergyAnalyzer',
    'RevenueSynergyType',
    'SynergyOpportunity',
    'CostSynergyAnalyzer',
    'CostSynergyType',
    'CostReduction',
    'IntegrationCostAnalyzer',
    'IntegrationCategory',
    'IntegrationCost',
    'SynergyValuation'
]
