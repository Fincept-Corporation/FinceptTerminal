"""
Base Economic Agent Class
Provides common functionality for all economic ideology agents
"""

from abc import ABC, abstractmethod
from typing import Dict, List, Any, Optional
from dataclasses import dataclass
from datetime import datetime
import json

@dataclass
class EconomicData:
    """Structure for economic data points"""
    gdp: float
    inflation: float
    unemployment: float
    interest_rate: float
    trade_balance: float
    government_spending: float
    tax_rate: float
    private_investment: float
    consumer_confidence: float
    timestamp: datetime

@dataclass
class PolicyRecommendation:
    """Structure for policy recommendations"""
    title: str
    description: str
    priority: str  # high, medium, low
    expected_impact: str
    implementation_time: str
    risk_level: str

class EconomicAgent(ABC):
    """Abstract base class for economic ideology agents"""

    def __init__(self, name: str, description: str):
        self.name = name
        self.description = description
        self.principles = []
        self.priorities = []

    @abstractmethod
    def analyze_economy(self, data: EconomicData) -> Dict[str, Any]:
        """Analyze economic data from the perspective of this ideology"""
        pass

    @abstractmethod
    def get_policy_recommendations(self, data: EconomicData, analysis: Dict[str, Any]) -> List[PolicyRecommendation]:
        """Generate policy recommendations based on economic analysis"""
        pass

    @abstractmethod
    def evaluate_policy(self, policy: Dict[str, Any], data: EconomicData) -> Dict[str, Any]:
        """Evaluate a specific policy from this ideology's perspective"""
        pass

    def calculate_health_score(self, data: EconomicData) -> float:
        """Calculate overall economic health score (0-100)"""
        # Base implementation - can be overridden by specific agents
        gdp_score = min(max(data.gdp / 100 * 50, 0), 50)  # GDP growth contribution
        employment_score = max(50 - data.unemployment * 2, 0)  # Employment contribution
        inflation_score = max(25 - abs(data.inflation - 2) * 10, 0)  # Inflation contribution (target 2%)

        return min(gdp_score + employment_score + inflation_score, 100)

    def format_analysis(self, analysis: Dict[str, Any]) -> str:
        """Format analysis results for human-readable output"""
        return json.dumps(analysis, indent=2, default=str)

    def __str__(self):
        return f"{self.name}: {self.description}"