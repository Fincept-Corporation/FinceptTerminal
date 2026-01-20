"""
SuperAgent - Triage routing for query distribution

Routes queries to appropriate sub-agents based on intent classification.
Similar to ValueCell's agent orchestration pattern.
"""

import re
from typing import Dict, Any, Optional, List, Callable
from dataclasses import dataclass, field
from enum import Enum
import logging

logger = logging.getLogger(__name__)


class QueryIntent(Enum):
    """Classified query intents"""
    TRADING = "trading"
    PORTFOLIO = "portfolio"
    ANALYSIS = "analysis"
    RISK = "risk"
    NEWS = "news"
    GEOPOLITICS = "geopolitics"
    ECONOMICS = "economics"
    RESEARCH = "research"
    GENERAL = "general"


@dataclass
class RouteConfig:
    """Configuration for a route"""
    intent: QueryIntent
    agent_id: str
    keywords: List[str] = field(default_factory=list)
    patterns: List[str] = field(default_factory=list)
    priority: int = 0
    config_override: Dict[str, Any] = field(default_factory=dict)


@dataclass
class RoutingResult:
    """Result of routing decision"""
    intent: QueryIntent
    agent_id: str
    confidence: float
    config: Dict[str, Any]
    matched_keywords: List[str] = field(default_factory=list)
    matched_patterns: List[str] = field(default_factory=list)


class IntentClassifier:
    """
    Classifies query intent using keyword matching and patterns.
    Can be extended with ML-based classification.
    """

    # Default keyword mappings
    DEFAULT_KEYWORDS = {
        QueryIntent.TRADING: [
            "buy", "sell", "trade", "order", "position", "long", "short",
            "stop loss", "take profit", "limit order", "market order",
            "entry", "exit", "fill", "execute"
        ],
        QueryIntent.PORTFOLIO: [
            "portfolio", "holdings", "allocation", "rebalance", "diversify",
            "weight", "exposure", "balance", "assets", "positions"
        ],
        QueryIntent.ANALYSIS: [
            "analyze", "analysis", "valuation", "dcf", "fair value",
            "fundamental", "technical", "chart", "indicator", "pattern",
            "support", "resistance", "trend"
        ],
        QueryIntent.RISK: [
            "risk", "var", "volatility", "drawdown", "sharpe", "beta",
            "correlation", "hedge", "exposure", "stress test", "scenario"
        ],
        QueryIntent.NEWS: [
            "news", "headline", "announcement", "earnings", "report",
            "press release", "update", "breaking"
        ],
        QueryIntent.GEOPOLITICS: [
            "geopolitical", "war", "conflict", "sanction", "trade war",
            "election", "policy", "government", "diplomatic", "military"
        ],
        QueryIntent.ECONOMICS: [
            "gdp", "inflation", "interest rate", "fed", "central bank",
            "employment", "jobs", "cpi", "ppi", "economic"
        ],
        QueryIntent.RESEARCH: [
            "research", "deep dive", "investigate", "study", "comprehensive",
            "detailed", "in-depth", "explore"
        ]
    }

    # Default patterns
    DEFAULT_PATTERNS = {
        QueryIntent.TRADING: [
            r'\b(buy|sell)\s+\d+',
            r'\b(long|short)\s+[A-Z]{1,5}\b',
            r'place\s+(market|limit)\s+order'
        ],
        QueryIntent.ANALYSIS: [
            r'analyze\s+[A-Z]{1,5}',
            r'what.*think.*about\s+[A-Z]{1,5}',
            r'[A-Z]{1,5}\s+(outlook|forecast|prediction)'
        ],
        QueryIntent.PORTFOLIO: [
            r'my\s+portfolio',
            r'rebalance.*portfolio',
            r'portfolio.*allocation'
        ]
    }

    def __init__(
        self,
        custom_keywords: Dict[QueryIntent, List[str]] = None,
        custom_patterns: Dict[QueryIntent, List[str]] = None
    ):
        self.keywords = {**self.DEFAULT_KEYWORDS}
        self.patterns = {**self.DEFAULT_PATTERNS}

        if custom_keywords:
            for intent, kws in custom_keywords.items():
                self.keywords.setdefault(intent, []).extend(kws)

        if custom_patterns:
            for intent, pts in custom_patterns.items():
                self.patterns.setdefault(intent, []).extend(pts)

        # Compile patterns
        self._compiled_patterns = {
            intent: [re.compile(p, re.IGNORECASE) for p in patterns]
            for intent, patterns in self.patterns.items()
        }

    def classify(self, query: str) -> List[tuple[QueryIntent, float, List[str], List[str]]]:
        """
        Classify query intent.

        Returns:
            List of (intent, confidence, matched_keywords, matched_patterns) sorted by confidence
        """
        query_lower = query.lower()
        results = []

        for intent in QueryIntent:
            matched_keywords = []
            matched_patterns = []

            # Check keywords
            if intent in self.keywords:
                for kw in self.keywords[intent]:
                    if kw.lower() in query_lower:
                        matched_keywords.append(kw)

            # Check patterns
            if intent in self._compiled_patterns:
                for pattern in self._compiled_patterns[intent]:
                    if pattern.search(query):
                        matched_patterns.append(pattern.pattern)

            # Calculate confidence
            if matched_keywords or matched_patterns:
                keyword_score = len(matched_keywords) * 0.3
                pattern_score = len(matched_patterns) * 0.5
                confidence = min(1.0, keyword_score + pattern_score)
                results.append((intent, confidence, matched_keywords, matched_patterns))

        # Sort by confidence descending
        results.sort(key=lambda x: x[1], reverse=True)

        # If no matches, return GENERAL
        if not results:
            results.append((QueryIntent.GENERAL, 0.1, [], []))

        return results


class SuperAgent:
    """
    SuperAgent - Routes queries to appropriate sub-agents.

    Features:
    - Intent classification
    - Multi-agent routing
    - Fallback handling
    - Response aggregation
    """

    def __init__(
        self,
        api_keys: Optional[Dict[str, str]] = None,
        routes: Optional[List[RouteConfig]] = None
    ):
        self.api_keys = api_keys or {}
        self.classifier = IntentClassifier()
        self.routes: Dict[QueryIntent, RouteConfig] = {}
        self.fallback_agent_id = "core_agent"

        # Initialize default routes
        self._setup_default_routes()

        # Apply custom routes
        if routes:
            for route in routes:
                self.add_route(route)

    def _setup_default_routes(self) -> None:
        """Setup default routing table"""
        default_routes = [
            RouteConfig(
                intent=QueryIntent.TRADING,
                agent_id="trading_agent",
                keywords=["buy", "sell", "trade"],
                priority=10,
                config_override={"tools": ["yfinance", "calculator"]}
            ),
            RouteConfig(
                intent=QueryIntent.PORTFOLIO,
                agent_id="portfolio_agent",
                keywords=["portfolio", "allocation"],
                priority=9,
                config_override={"tools": ["yfinance", "calculator"]}
            ),
            RouteConfig(
                intent=QueryIntent.ANALYSIS,
                agent_id="analysis_agent",
                keywords=["analyze", "valuation"],
                priority=8,
                config_override={"reasoning": True, "tools": ["yfinance", "duckduckgo"]}
            ),
            RouteConfig(
                intent=QueryIntent.RISK,
                agent_id="risk_agent",
                keywords=["risk", "volatility"],
                priority=8,
                config_override={"tools": ["yfinance", "calculator"]}
            ),
            RouteConfig(
                intent=QueryIntent.NEWS,
                agent_id="news_agent",
                keywords=["news", "headlines"],
                priority=5,
                config_override={"tools": ["duckduckgo"]}
            ),
            RouteConfig(
                intent=QueryIntent.GEOPOLITICS,
                agent_id="geopolitics_agent",
                keywords=["geopolitical", "conflict"],
                priority=5,
                config_override={"tools": ["duckduckgo"]}
            ),
            RouteConfig(
                intent=QueryIntent.ECONOMICS,
                agent_id="economics_agent",
                keywords=["gdp", "inflation"],
                priority=6,
                config_override={"tools": ["duckduckgo", "calculator"]}
            ),
            RouteConfig(
                intent=QueryIntent.RESEARCH,
                agent_id="research_agent",
                keywords=["research", "investigate"],
                priority=7,
                config_override={"reasoning": True, "tools": ["duckduckgo", "yfinance"]}
            ),
            RouteConfig(
                intent=QueryIntent.GENERAL,
                agent_id="core_agent",
                keywords=[],
                priority=0,
                config_override={}
            )
        ]

        for route in default_routes:
            self.routes[route.intent] = route

    def add_route(self, route: RouteConfig) -> None:
        """Add or update a route"""
        self.routes[route.intent] = route
        # Update classifier keywords
        if route.keywords:
            self.classifier.keywords.setdefault(route.intent, []).extend(route.keywords)

    def route(self, query: str) -> RoutingResult:
        """
        Route query to appropriate agent.

        Args:
            query: User query

        Returns:
            RoutingResult with agent info and confidence
        """
        # Classify intent
        classifications = self.classifier.classify(query)

        if not classifications:
            # Fallback to general
            return RoutingResult(
                intent=QueryIntent.GENERAL,
                agent_id=self.fallback_agent_id,
                confidence=0.0,
                config={}
            )

        # Get top classification
        top_intent, confidence, matched_kw, matched_pt = classifications[0]

        # Get route config
        route = self.routes.get(top_intent, self.routes.get(QueryIntent.GENERAL))

        return RoutingResult(
            intent=top_intent,
            agent_id=route.agent_id if route else self.fallback_agent_id,
            confidence=confidence,
            config=route.config_override if route else {},
            matched_keywords=matched_kw,
            matched_patterns=matched_pt
        )

    def route_multi(self, query: str, max_agents: int = 3) -> List[RoutingResult]:
        """
        Route to multiple agents for complex queries.

        Args:
            query: User query
            max_agents: Maximum number of agents to route to

        Returns:
            List of RoutingResults sorted by relevance
        """
        classifications = self.classifier.classify(query)
        results = []

        for intent, confidence, matched_kw, matched_pt in classifications[:max_agents]:
            if confidence < 0.2:  # Skip low confidence
                continue

            route = self.routes.get(intent)
            if route:
                results.append(RoutingResult(
                    intent=intent,
                    agent_id=route.agent_id,
                    confidence=confidence,
                    config=route.config_override,
                    matched_keywords=matched_kw,
                    matched_patterns=matched_pt
                ))

        return results

    def execute(
        self,
        query: str,
        session_id: Optional[str] = None,
        context: Optional[Dict[str, Any]] = None
    ) -> Dict[str, Any]:
        """
        Route and execute query.

        Args:
            query: User query
            session_id: Optional session ID
            context: Optional context

        Returns:
            Response from routed agent
        """
        from finagent_core.core_agent import CoreAgent
        from finagent_core.agent_loader import get_loader

        # Route query
        routing = self.route(query)
        logger.info(f"Routed to {routing.agent_id} with confidence {routing.confidence:.2f}")

        # Build config
        config = {
            "model": {"provider": "openai", "model_id": "gpt-4-turbo"},
            "instructions": self._get_instructions_for_intent(routing.intent),
            **routing.config
        }

        # Try to load specific agent
        loader = get_loader()
        try:
            agent = loader.create_agent(routing.agent_id, self.api_keys, config)
        except Exception:
            # Fallback to CoreAgent
            agent = CoreAgent(api_keys=self.api_keys)

        # Execute
        try:
            response = agent.run(query, config, session_id)
            content = agent.get_response_content(response) if hasattr(agent, 'get_response_content') else str(response)

            return {
                "success": True,
                "response": content,
                "routing": {
                    "intent": routing.intent.value,
                    "agent_id": routing.agent_id,
                    "confidence": routing.confidence,
                    "matched_keywords": routing.matched_keywords
                }
            }
        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "routing": {
                    "intent": routing.intent.value,
                    "agent_id": routing.agent_id,
                    "confidence": routing.confidence
                }
            }

    def execute_multi(
        self,
        query: str,
        session_id: Optional[str] = None,
        aggregate: bool = True
    ) -> Dict[str, Any]:
        """
        Execute query on multiple routed agents and aggregate results.

        Args:
            query: User query
            session_id: Optional session ID
            aggregate: Whether to aggregate responses

        Returns:
            Aggregated or individual responses
        """
        routings = self.route_multi(query)

        if not routings:
            return self.execute(query, session_id)

        responses = []
        for routing in routings:
            result = self.execute(query, session_id)
            result["routing"]["priority"] = routing.confidence
            responses.append(result)

        if aggregate and len(responses) > 1:
            # Simple aggregation - combine successful responses
            combined = []
            for resp in responses:
                if resp.get("success"):
                    combined.append(f"[{resp['routing']['intent']}]\n{resp['response']}")

            return {
                "success": True,
                "response": "\n\n---\n\n".join(combined),
                "responses": responses,
                "aggregated": True
            }

        return {
            "success": True,
            "responses": responses,
            "aggregated": False
        }

    def _get_instructions_for_intent(self, intent: QueryIntent) -> str:
        """Get specialized instructions for intent"""
        instructions = {
            QueryIntent.TRADING: """You are a trading assistant. Help users with:
- Order placement and execution
- Position management
- Entry/exit strategies
- Market timing""",

            QueryIntent.PORTFOLIO: """You are a portfolio manager. Help users with:
- Portfolio allocation and rebalancing
- Diversification strategies
- Asset selection
- Position sizing""",

            QueryIntent.ANALYSIS: """You are a financial analyst. Help users with:
- Stock valuation (DCF, comparables)
- Fundamental analysis
- Technical analysis
- Market research""",

            QueryIntent.RISK: """You are a risk analyst. Help users with:
- Risk metrics (VaR, Sharpe, etc.)
- Volatility analysis
- Stress testing
- Hedging strategies""",

            QueryIntent.NEWS: """You are a financial news analyst. Help users with:
- Market news and headlines
- Earnings announcements
- Company updates
- Sector news""",

            QueryIntent.GEOPOLITICS: """You are a geopolitical analyst. Help users with:
- Geopolitical risk assessment
- International relations
- Policy analysis
- Global macro trends""",

            QueryIntent.ECONOMICS: """You are an economist. Help users with:
- Economic indicators
- Central bank policy
- Macro trends
- Economic forecasting""",

            QueryIntent.RESEARCH: """You are a research analyst. Help users with:
- Deep-dive research
- Comprehensive analysis
- Industry studies
- Investment theses""",

            QueryIntent.GENERAL: """You are a helpful financial AI assistant."""
        }

        return instructions.get(intent, instructions[QueryIntent.GENERAL])


# Entry point for Tauri commands
def route_query(query: str, api_keys: Dict[str, str] = None) -> Dict[str, Any]:
    """Route a query and return routing info"""
    agent = SuperAgent(api_keys=api_keys)
    routing = agent.route(query)
    return {
        "success": True,
        "intent": routing.intent.value,
        "agent_id": routing.agent_id,
        "confidence": routing.confidence,
        "matched_keywords": routing.matched_keywords,
        "matched_patterns": routing.matched_patterns,
        "config": routing.config
    }


def execute_query(
    query: str,
    api_keys: Dict[str, str] = None,
    session_id: str = None
) -> Dict[str, Any]:
    """Route and execute a query"""
    agent = SuperAgent(api_keys=api_keys)
    return agent.execute(query, session_id)
