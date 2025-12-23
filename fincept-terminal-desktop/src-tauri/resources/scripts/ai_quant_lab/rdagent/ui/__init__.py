"""
RD-Agent UI Components
Streamlit-based user interfaces for experiment tracking and monitoring
"""

from typing import Dict, Any

try:
    from .streamlit_dashboard import RDAgentDashboard
    UI_AVAILABLE = True
except ImportError:
    UI_AVAILABLE = False
    RDAgentDashboard = None


__all__ = ['RDAgentDashboard', 'UI_AVAILABLE']
