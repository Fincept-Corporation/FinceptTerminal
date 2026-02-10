"""M&A Deal Database Module"""
import sys
from pathlib import Path

# Add Analytics path for absolute imports
analytics_path = Path(__file__).parent.parent.parent
sys.path.insert(0, str(analytics_path))

try:
    from corporateFinance.deal_database.database_schema import MADatabase
    from corporateFinance.deal_database.deal_scanner import MADealScanner
    from corporateFinance.deal_database.deal_parser import MADealParser
    from corporateFinance.deal_database.deal_tracker import MADealTracker
    __all__ = ['MADatabase', 'MADealScanner', 'MADealParser', 'MADealTracker']
except ImportError:
    __all__ = []
