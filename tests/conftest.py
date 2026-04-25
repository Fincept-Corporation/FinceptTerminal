"""Pytest configuration for FinceptTerminal tests."""
import sys
from pathlib import Path

# Add scripts directory to path so fred_gatherer can be imported
scripts_dir = Path(__file__).parent.parent / "fincept-qt" / "scripts"
if str(scripts_dir) not in sys.path:
    sys.path.insert(0, str(scripts_dir))
