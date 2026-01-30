"""Configuration for Corporate Finance Module"""
from pathlib import Path

BASE_DIR = Path(__file__).parent
SCRIPTS_DIR = BASE_DIR.parent.parent
RESOURCES_DIR = SCRIPTS_DIR.parent
DATABASE_DIR = RESOURCES_DIR / "databases"
MA_DATABASE = DATABASE_DIR / "ma_deals.db"

DEFAULT_DISCOUNT_RATE = 0.10
DEFAULT_TERMINAL_GROWTH = 0.025
DEFAULT_TAX_RATE = 0.21
DEFAULT_RISK_FREE_RATE = 0.045

MA_FILING_FORMS = ['8-K', 'S-4', 'DEFM14A', 'SC 13D', 'SC 13G', '425', 'PREM14A']
MATERIAL_EVENT_ITEMS = ['1.01', '2.01', '8.01']

INDUSTRIES = [
    'Technology', 'Healthcare', 'Financial Services', 'Industrials',
    'Consumer Discretionary', 'Consumer Staples', 'Energy', 'Materials',
    'Real Estate', 'Utilities', 'Telecommunications', 'Media'
]

DEAL_TYPES = ['Merger', 'Acquisition', 'Takeover', 'Joint Venture', 'Asset Purchase', 'Reverse Merger']
PAYMENT_METHODS = ['Cash', 'Stock', 'Mixed', 'Debt', 'Earnout']
DEAL_STATUS = ['Announced', 'Pending', 'Completed', 'Terminated', 'Withdrawn']
