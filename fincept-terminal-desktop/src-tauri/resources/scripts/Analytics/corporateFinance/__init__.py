"""Corporate Finance & M&A Analysis Module"""
__version__ = "1.0.0"

from .deal_database.deal_tracker import MADealTracker
from .deal_database.deal_scanner import MADealScanner
from .deal_database.deal_parser import MADealParser

__all__ = ['MADealTracker', 'MADealScanner', 'MADealParser']
