"""Corporate Finance & M&A Analysis Module"""
import sys
from pathlib import Path

__version__ = "1.0.0"

# Add Analytics path for absolute imports
analytics_path = Path(__file__).parent.parent
sys.path.insert(0, str(analytics_path))

# Use absolute imports instead of relative imports
from corporateFinance.deal_database.deal_tracker import MADealTracker
from corporateFinance.deal_database.deal_scanner import MADealScanner
from corporateFinance.deal_database.deal_parser import MADealParser

__all__ = ['MADealTracker', 'MADealScanner', 'MADealParser']
