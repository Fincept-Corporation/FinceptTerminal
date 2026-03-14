"""Configuration for Corporate Finance Module"""
import os
from pathlib import Path

BASE_DIR = Path(__file__).parent
SCRIPTS_DIR = BASE_DIR.parent.parent
RESOURCES_DIR = SCRIPTS_DIR.parent

# Database path: Use FINCEPT_DATA_DIR env var if set, otherwise fall back to user data dir
def get_database_dir() -> Path:
    """Get the database directory from environment or default user data location."""
    # Check for environment variable passed from Rust
    if data_dir := os.environ.get("FINCEPT_DATA_DIR"):
        return Path(data_dir)

    # Default to platform-specific user data directory
    if os.name == "nt":  # Windows
        base = Path(os.environ.get("LOCALAPPDATA", "C:/Users/Default/AppData/Local"))
    elif os.uname().sysname == "Darwin":  # macOS
        base = Path.home() / "Library" / "Application Support"
    else:  # Linux
        base = Path.home() / ".local" / "share"

    return base / "fincept-dev"

DATABASE_DIR = get_database_dir()
MA_DATABASE = DATABASE_DIR / "ma_deals.db"

# Ensure database directory exists
DATABASE_DIR.mkdir(parents=True, exist_ok=True)

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
