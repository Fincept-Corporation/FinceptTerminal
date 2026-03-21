"""Deal Comparison Module"""
import sys
from pathlib import Path

# Add Analytics path for absolute imports
analytics_path = Path(__file__).parent.parent.parent
sys.path.insert(0, str(analytics_path))

# Use absolute imports instead of relative imports
from corporateFinance.deal_comparison.deal_comparator import DealComparator, Deal

__all__ = ['DealComparator', 'Deal']
