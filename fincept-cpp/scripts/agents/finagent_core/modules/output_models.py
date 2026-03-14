"""
Output Models Module - Structured outputs for financial agents

Provides:
- Pydantic models for structured agent outputs
- Financial-specific output schemas
- Validation and type enforcement
- JSON schema generation
"""

from typing import Dict, Any, Optional, List, Union
from dataclasses import dataclass
from enum import Enum
import logging
import json

logger = logging.getLogger(__name__)

# Try to import Pydantic for structured outputs
try:
    from pydantic import BaseModel, Field, validator
    PYDANTIC_AVAILABLE = True
except ImportError:
    PYDANTIC_AVAILABLE = False
    BaseModel = object
    Field = lambda *args, **kwargs: None
    validator = lambda *args, **kwargs: lambda f: f


# ============================================================
# Enums for Financial Types
# ============================================================

class TradeAction(str, Enum):
    """Trade action types."""
    BUY = "buy"
    SELL = "sell"
    HOLD = "hold"
    STRONG_BUY = "strong_buy"
    STRONG_SELL = "strong_sell"


class RiskLevel(str, Enum):
    """Risk level classification."""
    LOW = "low"
    MEDIUM = "medium"
    HIGH = "high"
    VERY_HIGH = "very_high"


class TimeHorizon(str, Enum):
    """Investment time horizon."""
    INTRADAY = "intraday"
    SHORT_TERM = "short_term"  # Days to weeks
    MEDIUM_TERM = "medium_term"  # Weeks to months
    LONG_TERM = "long_term"  # Months to years


class Sentiment(str, Enum):
    """Market sentiment."""
    VERY_BEARISH = "very_bearish"
    BEARISH = "bearish"
    NEUTRAL = "neutral"
    BULLISH = "bullish"
    VERY_BULLISH = "very_bullish"


# ============================================================
# Output Models (Pydantic if available, else dataclass)
# ============================================================

if PYDANTIC_AVAILABLE:

    class TradeSignal(BaseModel):
        """Structured output for trade signals."""
        symbol: str = Field(..., description="Trading symbol")
        action: TradeAction = Field(..., description="Recommended action")
        confidence: float = Field(..., ge=0, le=1, description="Confidence level 0-1")
        entry_price: Optional[float] = Field(None, description="Suggested entry price")
        stop_loss: Optional[float] = Field(None, description="Stop loss price")
        take_profit: Optional[float] = Field(None, description="Take profit price")
        quantity: Optional[float] = Field(None, description="Suggested quantity")
        reasoning: str = Field(..., description="Explanation for the signal")
        time_horizon: Optional[TimeHorizon] = Field(None, description="Investment horizon")
        risk_level: Optional[RiskLevel] = Field(None, description="Risk level")

        @validator("confidence")
        def validate_confidence(cls, v):
            if v < 0 or v > 1:
                raise ValueError("Confidence must be between 0 and 1")
            return v

        @validator("stop_loss")
        def validate_stop_loss(cls, v, values):
            if v is not None and values.get("entry_price"):
                action = values.get("action")
                entry = values["entry_price"]
                if action in [TradeAction.BUY, TradeAction.STRONG_BUY] and v >= entry:
                    raise ValueError("Stop loss must be below entry price for long positions")
            return v

    class StockAnalysis(BaseModel):
        """Structured output for stock analysis."""
        symbol: str = Field(..., description="Stock symbol")
        company_name: Optional[str] = Field(None, description="Company name")
        current_price: float = Field(..., description="Current stock price")
        target_price: Optional[float] = Field(None, description="Price target")
        upside_potential: Optional[float] = Field(None, description="Upside potential percentage")

        # Valuation metrics
        pe_ratio: Optional[float] = Field(None, description="P/E ratio")
        pb_ratio: Optional[float] = Field(None, description="P/B ratio")
        ps_ratio: Optional[float] = Field(None, description="P/S ratio")
        ev_ebitda: Optional[float] = Field(None, description="EV/EBITDA")

        # Growth metrics
        revenue_growth: Optional[float] = Field(None, description="Revenue growth %")
        earnings_growth: Optional[float] = Field(None, description="Earnings growth %")

        # Technical indicators
        rsi: Optional[float] = Field(None, description="RSI value")
        macd_signal: Optional[str] = Field(None, description="MACD signal")
        trend: Optional[str] = Field(None, description="Price trend")

        # Sentiment
        sentiment: Optional[Sentiment] = Field(None, description="Overall sentiment")
        analyst_rating: Optional[str] = Field(None, description="Analyst consensus")

        # Summary
        summary: str = Field(..., description="Analysis summary")
        recommendation: TradeAction = Field(..., description="Investment recommendation")
        key_risks: List[str] = Field(default_factory=list, description="Key risks")
        key_catalysts: List[str] = Field(default_factory=list, description="Key catalysts")

    class PortfolioAnalysis(BaseModel):
        """Structured output for portfolio analysis."""
        total_value: float = Field(..., description="Total portfolio value")
        cash_balance: float = Field(..., description="Cash balance")
        invested_value: float = Field(..., description="Invested value")

        # Performance
        daily_return: Optional[float] = Field(None, description="Daily return %")
        weekly_return: Optional[float] = Field(None, description="Weekly return %")
        monthly_return: Optional[float] = Field(None, description="Monthly return %")
        ytd_return: Optional[float] = Field(None, description="YTD return %")
        total_return: Optional[float] = Field(None, description="Total return %")

        # Risk metrics
        volatility: Optional[float] = Field(None, description="Portfolio volatility")
        sharpe_ratio: Optional[float] = Field(None, description="Sharpe ratio")
        sortino_ratio: Optional[float] = Field(None, description="Sortino ratio")
        max_drawdown: Optional[float] = Field(None, description="Maximum drawdown %")
        beta: Optional[float] = Field(None, description="Portfolio beta")
        var_95: Optional[float] = Field(None, description="95% VaR")

        # Allocation
        sector_allocation: Dict[str, float] = Field(default_factory=dict)
        asset_allocation: Dict[str, float] = Field(default_factory=dict)
        top_holdings: List[Dict[str, Any]] = Field(default_factory=list)

        # Assessment
        risk_level: RiskLevel = Field(..., description="Overall risk level")
        diversification_score: Optional[float] = Field(None, description="Diversification score 0-100")
        recommendations: List[str] = Field(default_factory=list)

    class RiskAssessment(BaseModel):
        """Structured output for risk assessment."""
        overall_risk: RiskLevel = Field(..., description="Overall risk level")
        risk_score: float = Field(..., ge=0, le=100, description="Risk score 0-100")

        # Risk breakdown
        market_risk: float = Field(..., ge=0, le=100)
        credit_risk: float = Field(..., ge=0, le=100)
        liquidity_risk: float = Field(..., ge=0, le=100)
        concentration_risk: float = Field(..., ge=0, le=100)

        # Metrics
        var_1d_95: Optional[float] = Field(None, description="1-day 95% VaR")
        var_1d_99: Optional[float] = Field(None, description="1-day 99% VaR")
        expected_shortfall: Optional[float] = Field(None, description="Expected shortfall")

        # Stress scenarios
        stress_scenarios: List[Dict[str, Any]] = Field(default_factory=list)

        # Recommendations
        risk_factors: List[str] = Field(default_factory=list)
        mitigation_strategies: List[str] = Field(default_factory=list)

    class MarketAnalysis(BaseModel):
        """Structured output for market analysis."""
        market: str = Field(..., description="Market/Index name")
        current_level: float = Field(..., description="Current level")
        change_pct: float = Field(..., description="Change percentage")

        # Trend
        trend: str = Field(..., description="Market trend")
        trend_strength: Optional[str] = Field(None, description="Trend strength")

        # Sentiment
        sentiment: Sentiment = Field(..., description="Market sentiment")
        fear_greed_index: Optional[int] = Field(None, ge=0, le=100)

        # Breadth
        advancing_stocks: Optional[int] = Field(None)
        declining_stocks: Optional[int] = Field(None)
        new_highs: Optional[int] = Field(None)
        new_lows: Optional[int] = Field(None)

        # Sectors
        sector_performance: Dict[str, float] = Field(default_factory=dict)
        leading_sectors: List[str] = Field(default_factory=list)
        lagging_sectors: List[str] = Field(default_factory=list)

        # Outlook
        short_term_outlook: str = Field(..., description="Short-term outlook")
        key_levels: Dict[str, float] = Field(default_factory=dict)
        catalysts: List[str] = Field(default_factory=list)

    class ResearchReport(BaseModel):
        """Structured output for research reports."""
        title: str = Field(..., description="Report title")
        symbol: Optional[str] = Field(None, description="Primary symbol")
        report_type: str = Field(..., description="Type of report")
        date: str = Field(..., description="Report date")

        # Executive summary
        executive_summary: str = Field(..., description="Executive summary")
        key_findings: List[str] = Field(default_factory=list)

        # Investment thesis
        investment_thesis: str = Field(..., description="Investment thesis")
        bull_case: str = Field(..., description="Bull case scenario")
        bear_case: str = Field(..., description="Bear case scenario")

        # Recommendation
        recommendation: TradeAction = Field(...)
        target_price: Optional[float] = Field(None)
        confidence: float = Field(..., ge=0, le=1)

        # Risk factors
        risks: List[str] = Field(default_factory=list)

        # Appendix
        data_sources: List[str] = Field(default_factory=list)
        disclaimers: List[str] = Field(default_factory=list)

else:
    # Fallback dataclass implementations when Pydantic not available

    @dataclass
    class TradeSignal:
        symbol: str
        action: str
        confidence: float
        reasoning: str
        entry_price: Optional[float] = None
        stop_loss: Optional[float] = None
        take_profit: Optional[float] = None
        quantity: Optional[float] = None
        time_horizon: Optional[str] = None
        risk_level: Optional[str] = None

    @dataclass
    class StockAnalysis:
        symbol: str
        current_price: float
        summary: str
        recommendation: str
        company_name: Optional[str] = None
        target_price: Optional[float] = None
        sentiment: Optional[str] = None
        key_risks: List[str] = None
        key_catalysts: List[str] = None

    @dataclass
    class PortfolioAnalysis:
        total_value: float
        cash_balance: float
        invested_value: float
        risk_level: str
        recommendations: List[str] = None

    @dataclass
    class RiskAssessment:
        overall_risk: str
        risk_score: float
        market_risk: float
        credit_risk: float
        liquidity_risk: float
        concentration_risk: float

    @dataclass
    class MarketAnalysis:
        market: str
        current_level: float
        change_pct: float
        trend: str
        sentiment: str
        short_term_outlook: str

    @dataclass
    class ResearchReport:
        title: str
        report_type: str
        date: str
        executive_summary: str
        investment_thesis: str
        recommendation: str
        confidence: float


class OutputModelRegistry:
    """
    Registry for output models.

    Provides easy access to output models for agent configuration.
    """

    MODELS = {
        "trade_signal": TradeSignal,
        "stock_analysis": StockAnalysis,
        "portfolio_analysis": PortfolioAnalysis,
        "risk_assessment": RiskAssessment,
        "market_analysis": MarketAnalysis,
        "research_report": ResearchReport,
    }

    @classmethod
    def get_model(cls, name: str) -> type:
        """Get output model by name."""
        return cls.MODELS.get(name)

    @classmethod
    def list_models(cls) -> List[str]:
        """List available output models."""
        return list(cls.MODELS.keys())

    @classmethod
    def get_schema(cls, name: str) -> Dict[str, Any]:
        """Get JSON schema for a model."""
        model = cls.get_model(name)
        if model and PYDANTIC_AVAILABLE and hasattr(model, "schema"):
            return model.schema()
        return {}

    @classmethod
    def to_agent_config(cls, model_name: str) -> Dict[str, Any]:
        """
        Get Agno agent config for structured output.

        Args:
            model_name: Name of the output model

        Returns:
            Config dict for Agent initialization
        """
        model = cls.get_model(model_name)
        if not model:
            return {}

        return {
            "output_model": model,
            "structured_outputs": True,
            "parse_response": True
        }


class OutputValidator:
    """
    Validate agent outputs against schemas.
    """

    @staticmethod
    def validate(data: Dict[str, Any], model_name: str) -> Dict[str, Any]:
        """
        Validate data against a model schema.

        Args:
            data: Data to validate
            model_name: Name of the model to validate against

        Returns:
            Validation result with errors if any
        """
        model = OutputModelRegistry.get_model(model_name)
        if not model:
            return {"valid": False, "error": f"Unknown model: {model_name}"}

        try:
            if PYDANTIC_AVAILABLE and hasattr(model, "parse_obj"):
                instance = model.parse_obj(data)
                return {"valid": True, "data": instance.dict()}
            else:
                # Basic validation for dataclasses
                instance = model(**data)
                return {"valid": True, "data": data}

        except Exception as e:
            return {"valid": False, "error": str(e)}

    @staticmethod
    def coerce(data: Dict[str, Any], model_name: str) -> Dict[str, Any]:
        """
        Attempt to coerce data to match schema.

        Args:
            data: Data to coerce
            model_name: Target model name

        Returns:
            Coerced data or original if coercion fails
        """
        model = OutputModelRegistry.get_model(model_name)
        if not model:
            return data

        try:
            schema = OutputModelRegistry.get_schema(model_name)
            if not schema:
                return data

            # Attempt to coerce types based on schema
            coerced = {}
            properties = schema.get("properties", {})

            for key, prop in properties.items():
                if key in data:
                    value = data[key]
                    prop_type = prop.get("type")

                    if prop_type == "number" and isinstance(value, str):
                        try:
                            coerced[key] = float(value)
                        except ValueError:
                            coerced[key] = value
                    elif prop_type == "integer" and isinstance(value, str):
                        try:
                            coerced[key] = int(value)
                        except ValueError:
                            coerced[key] = value
                    elif prop_type == "boolean" and isinstance(value, str):
                        coerced[key] = value.lower() in ("true", "1", "yes")
                    else:
                        coerced[key] = value

            return coerced

        except Exception:
            return data
