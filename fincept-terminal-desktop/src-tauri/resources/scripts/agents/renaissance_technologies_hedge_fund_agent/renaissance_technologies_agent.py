"""
===== DATA SOURCES REQUIRED =====
# INPUT:
#   - ticker symbols (array)
#   - end_date (string)
#   - FINANCIAL_DATASETS_API_KEY
#
# OUTPUT:
#   - Financial metrics: ROE, D/E, Current Ratio
#   - Financial line items: Revenue, Net Income, FCF, Debt, Equity, Retained Earnings, Operating Margin, R&D, Shares
#   - Market Cap
#
# PARAMETERS:
#   - period="annual"
#   - limit=10 years
"""

from fincept_terminal.Agents.src.graph.state import AgentState, show_agent_reasoning
from fincept_terminal.Agents.src.tools.api import get_financial_metrics, get_market_cap, search_line_items
from langchain_core.prompts import ChatPromptTemplate
from langchain_core.messages import HumanMessage
from pydantic import BaseModel
import json
import math
from typing_extensions import Literal
from fincept_terminal.Agents.src.utils.progress import progress
from fincept_terminal.Agents.src.utils.llm import call_llm
from fincept_terminal.Agents.src.utils.api_key import get_api_key_from_state


class RenaissanceSignal(BaseModel):
    signal: Literal["bullish", "bearish", "neutral"]
    confidence: float
    reasoning: str
    statistical_edge: float
    signal_strength: dict


def renaissance_technologies_agent(state: AgentState, agent_id: str = "renaissance_technologies_agent"):
    """
    Renaissance Technologies: Pure quantitative, statistical arbitrage
    Structure: Signal Generation → Risk Models → Execution Optimization → Jim Simons Systematic Decision
    Philosophy: Mathematical models, statistical edges, high-frequency systematic trading
    """
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    api_key = get_api_key_from_state(state, "FINANCIAL_DATASETS_API_KEY")
    analysis_data = {}
    renaissance_analysis = {}

    for ticker in tickers:
        progress.update_status(agent_id, ticker, "Signal generation algorithms analyzing patterns")

        metrics = get_financial_metrics(ticker, end_date, period="quarterly", limit=20, api_key=api_key)
        financial_line_items = search_line_items(
            ticker,
            [
                "revenue", "net_income", "free_cash_flow", "total_assets",
                "shareholders_equity", "operating_margin", "total_debt",
                "current_assets", "current_liabilities", "outstanding_shares"
            ],
            end_date,
            period="quarterly",
            limit=20,
            api_key=api_key,
        )
        market_cap = get_market_cap(ticker, end_date, api_key=api_key)

        # Renaissance's algorithmic analysis structure
        progress.update_status(agent_id, ticker, "Signal generation team analysis")
        signal_generation = signal_generation_team_analysis(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Risk modeling team analysis")
        risk_modeling = risk_modeling_team_analysis(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Execution optimization analysis")
        execution_optimization = execution_optimization_analysis(financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Statistical arbitrage models")
        statistical_arbitrage = statistical_arbitrage_analysis(metrics, financial_line_items)

        progress.update_status(agent_id, ticker, "Jim Simons systematic synthesis")
        simons_systematic_decision = simons_systematic_synthesis(
            signal_generation, risk_modeling, execution_optimization, statistical_arbitrage
        )

        # Renaissance's mathematical scoring
        total_score = (
                signal_generation["score"] * 0.35 +
                statistical_arbitrage["score"] * 0.3 +
                risk_modeling["score"] * 0.2 +
                execution_optimization["score"] * 0.15
        )

        # Renaissance's systematic thresholds
        if total_score >= 8.0 and signal_generation.get("statistical_significance", 0) > 0.95:
            signal = "bullish"
        elif total_score <= 3.0 or signal_generation.get("statistical_significance", 0) < 0.6:
            signal = "bearish"
        else:
            signal = "neutral"

        analysis_data[ticker] = {
            "signal": signal,
            "score": total_score,
            "signal_generation": signal_generation,
            "risk_modeling": risk_modeling,
            "execution_optimization": execution_optimization,
            "statistical_arbitrage": statistical_arbitrage,
            "simons_systematic_decision": simons_systematic_decision
        }

        renaissance_output = generate_renaissance_output(ticker, analysis_data, state, agent_id)

        renaissance_analysis[ticker] = {
            "signal": renaissance_output.signal,
            "confidence": renaissance_output.confidence,
            "reasoning": renaissance_output.reasoning,
            "statistical_edge": renaissance_output.statistical_edge,
            "signal_strength": renaissance_output.signal_strength
        }

        progress.update_status(agent_id, ticker, "Done", analysis=renaissance_output.reasoning)

    message = HumanMessage(content=json.dumps(renaissance_analysis), name=agent_id)

    if state["metadata"]["show_reasoning"]:
        show_agent_reasoning(renaissance_analysis, "Renaissance Technologies")

    state["data"]["analyst_signals"][agent_id] = renaissance_analysis
    progress.update_status(agent_id, None, "Done")

    return {"messages": [message], "data": state["data"]}


def signal_generation_team_analysis(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Renaissance's signal generation algorithms"""
    score = 0
    details = []
    signals = {}

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No signal data"}

    # Mean reversion signals
    mean_reversion_score = calculate_mean_reversion_signals(metrics, financial_line_items)
    signals["mean_reversion"] = mean_reversion_score
    score += mean_reversion_score * 2

    # Momentum signals
    momentum_score = calculate_momentum_signals(metrics, financial_line_items)
    signals["momentum"] = momentum_score
    score += momentum_score * 2

    # Statistical pattern recognition
    pattern_score = identify_statistical_patterns(financial_line_items)
    signals["patterns"] = pattern_score
    score += pattern_score * 1.5

    # Cross-sectional signals
    cross_sectional_score = calculate_cross_sectional_signals(metrics)
    signals["cross_sectional"] = cross_sectional_score
    score += cross_sectional_score * 1.5

    # Factor exposure analysis
    factor_exposure = analyze_factor_exposures(metrics, financial_line_items)
    signals["factor_exposure"] = factor_exposure
    score += factor_exposure * 1

    # Statistical significance calculation
    statistical_significance = calculate_statistical_significance(signals)

    details.append(f"Mean reversion signal: {mean_reversion_score:.2f}")
    details.append(f"Momentum signal: {momentum_score:.2f}")
    details.append(f"Pattern recognition: {pattern_score:.2f}")
    details.append(f"Statistical significance: {statistical_significance:.2f}")

    return {
        "score": min(score, 10),  # Cap at 10
        "details": "; ".join(details),
        "signals": signals,
        "statistical_significance": statistical_significance
    }


def risk_modeling_team_analysis(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Renaissance's risk modeling team analysis"""
    score = 0
    details = []
    risk_metrics = {}

    if not metrics or not financial_line_items:
        return {"score": 0, "details": "No risk modeling data"}

    # Volatility modeling
    volatility_score = model_volatility_patterns(financial_line_items)
    risk_metrics["volatility"] = volatility_score
    if volatility_score > 0.7:
        score += 2
        details.append("Low volatility risk profile")

    # Correlation analysis
    correlation_risk = analyze_correlation_risks(metrics, financial_line_items)
    risk_metrics["correlation"] = correlation_risk
    if correlation_risk < 0.6:  # Low correlation is good
        score += 2
        details.append("Low correlation risk with market factors")

    # Liquidity risk modeling
    liquidity_risk = model_liquidity_risk(financial_line_items, market_cap)
    risk_metrics["liquidity"] = liquidity_risk
    if liquidity_risk > 0.8:
        score += 1
        details.append("Adequate liquidity for position sizing")

    # Factor risk decomposition
    factor_risks = decompose_factor_risks(metrics)
    risk_metrics["factor_risks"] = factor_risks
    if factor_risks.get("idiosyncratic_risk", 0) > 0.6:
        score += 2
        details.append("High idiosyncratic risk component - good for alpha generation")

    # Drawdown risk analysis
    drawdown_risk = calculate_maximum_drawdown_risk(financial_line_items)
    risk_metrics["drawdown"] = drawdown_risk
    if drawdown_risk < 0.3:  # Low drawdown risk
        score += 1
        details.append("Limited drawdown risk based on historical patterns")

    return {
        "score": score,
        "details": "; ".join(details),
        "risk_metrics": risk_metrics
    }


def execution_optimization_analysis(financial_line_items: list, market_cap: float) -> dict:
    """Renaissance's execution optimization analysis"""
    score = 0
    details = []

    if not financial_line_items or not market_cap:
        return {"score": 0, "details": "No execution data"}

    # Market impact modeling
    market_impact = estimate_market_impact(market_cap)
    if market_impact < 0.02:  # Low market impact
        score += 3
        details.append(f"Low market impact: {market_impact:.3f}")
    elif market_impact < 0.05:
        score += 2
        details.append(f"Moderate market impact: {market_impact:.3f}")

    # Execution timing optimization
    timing_score = analyze_execution_timing(financial_line_items)
    score += timing_score * 2
    details.append(f"Execution timing score: {timing_score:.2f}")

    # Transaction cost analysis
    transaction_costs = estimate_transaction_costs(market_cap)
    if transaction_costs < 0.01:  # Low transaction costs
        score += 2
        details.append(f"Low transaction costs: {transaction_costs:.3f}")

    # Slippage minimization
    slippage_risk = calculate_slippage_risk(market_cap)
    if slippage_risk < 0.5:
        score += 1
        details.append("Low slippage risk for systematic execution")

    return {"score": score, "details": "; ".join(details)}


def statistical_arbitrage_analysis(metrics: list, financial_line_items: list) -> dict:
    """Renaissance's statistical arbitrage models"""
    score = 0
    details = []
    arbitrage_signals = {}

    if not metrics or not financial_line_items:
        return {"score": 0, "details": "No statistical arbitrage data"}

    # Pairs trading signals
    pairs_signal = calculate_pairs_trading_signals(metrics, financial_line_items)
    arbitrage_signals["pairs"] = pairs_signal
    score += pairs_signal * 2

    # Statistical mispricing detection
    mispricing_score = detect_statistical_mispricing(metrics, financial_line_items)
    arbitrage_signals["mispricing"] = mispricing_score
    score += mispricing_score * 2.5

    # Regime change detection
    regime_change = detect_regime_changes(financial_line_items)
    arbitrage_signals["regime_change"] = regime_change
    score += regime_change * 1.5

    # Market microstructure signals
    microstructure_score = analyze_market_microstructure(metrics)
    arbitrage_signals["microstructure"] = microstructure_score
    score += microstructure_score * 1

    details.append(f"Pairs trading signal: {pairs_signal:.2f}")
    details.append(f"Mispricing detection: {mispricing_score:.2f}")
    details.append(f"Regime change signal: {regime_change:.2f}")

    return {
        "score": min(score, 10),
        "details": "; ".join(details),
        "arbitrage_signals": arbitrage_signals
    }


def simons_systematic_synthesis(signal_generation: dict, risk_modeling: dict,
                                execution_optimization: dict, statistical_arbitrage: dict) -> dict:
    """Jim Simons' systematic synthesis and decision"""
    details = []

    # Mathematical model synthesis
    signal_strength = signal_generation.get("statistical_significance", 0)
    risk_adjustment = risk_modeling.get("risk_metrics", {}).get("volatility", 0.5)
    execution_feasibility = execution_optimization["score"] / 10
    arbitrage_opportunity = statistical_arbitrage["score"] / 10

    # Simons' systematic decision framework
    overall_edge = (signal_strength * 0.4 +
                    arbitrage_opportunity * 0.3 +
                    execution_feasibility * 0.2 +
                    (1 - risk_adjustment) * 0.1)

    if overall_edge > 0.8:
        decision = "High conviction systematic trade"
        details.append("Strong statistical edge with low risk and good execution")
    elif overall_edge > 0.6:
        decision = "Moderate systematic position"
        details.append("Adequate statistical edge for systematic strategy")
    elif signal_strength < 0.6:
        decision = "Insufficient statistical significance"
        details.append("Statistical significance below systematic threshold")
    else:
        decision = "Pass - inadequate systematic edge"
        details.append("No clear systematic advantage identified")

    # Mathematical model confidence
    model_confidence = min(signal_strength * arbitrage_opportunity * execution_feasibility, 1.0)

    return {
        "decision": decision,
        "overall_edge": overall_edge,
        "model_confidence": model_confidence,
        "details": "; ".join(details)
    }


# Mathematical and statistical helper functions
def calculate_mean_reversion_signals(metrics: list, financial_line_items: list) -> float:
    """Calculate mean reversion signals"""
    if not metrics:
        return 0.5

    # Price-to-earnings mean reversion
    pe_ratios = [m.price_to_earnings for m in metrics if m.price_to_earnings and m.price_to_earnings > 0]
    if len(pe_ratios) >= 4:
        current_pe = pe_ratios[0]
        historical_mean = sum(pe_ratios[1:]) / len(pe_ratios[1:])

        # Mean reversion signal strength
        deviation = abs(current_pe - historical_mean) / historical_mean
        if current_pe < historical_mean * 0.8:  # 20% below mean
            return min(deviation, 1.0)
        elif current_pe > historical_mean * 1.2:  # 20% above mean
            return -min(deviation, 1.0)

    return 0.0


def calculate_momentum_signals(metrics: list, financial_line_items: list) -> float:
    """Calculate momentum signals"""
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 4:
        # Revenue momentum
        recent_growth = (revenues[0] / revenues[1]) - 1
        historical_growth = (revenues[1] / revenues[-1]) ** (1 / (len(revenues) - 2)) - 1

        momentum_strength = (recent_growth - historical_growth) / abs(
            historical_growth) if historical_growth != 0 else 0
        return max(min(momentum_strength, 1.0), -1.0)

    return 0.0


def identify_statistical_patterns(financial_line_items: list) -> float:
    """Identify statistical patterns in financial data"""
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if len(earnings) >= 8:
        # Pattern recognition using simple autocorrelation
        autocorr = calculate_autocorrelation(earnings, lag=1)
        pattern_strength = abs(autocorr)
        return min(pattern_strength, 1.0)

    return 0.3


def calculate_cross_sectional_signals(metrics: list) -> float:
    """Calculate cross-sectional ranking signals"""
    if not metrics:
        return 0.5

    latest = metrics[0]
    # Simplified cross-sectional score based on multiple metrics
    score_components = []

    if latest.return_on_equity:
        score_components.append(min(latest.return_on_equity / 0.15, 1.0))  # Normalize by 15%

    if latest.price_to_earnings and latest.price_to_earnings > 0:
        score_components.append(max(1 - (latest.price_to_earnings / 20), 0))  # Lower P/E is better

    if score_components:
        return sum(score_components) / len(score_components)

    return 0.5


def analyze_factor_exposures(metrics: list, financial_line_items: list) -> float:
    """Analyze factor exposures"""
    # Simplified factor analysis
    if not metrics:
        return 0.5

    latest = metrics[0]
    factor_score = 0

    # Value factor
    if latest.price_to_book and latest.price_to_book < 2.0:
        factor_score += 0.3

    # Quality factor
    if latest.return_on_equity and latest.return_on_equity > 0.12:
        factor_score += 0.3

    # Momentum factor
    if financial_line_items:
        revenues = [item.revenue for item in financial_line_items if item.revenue]
        if len(revenues) >= 2 and revenues[0] > revenues[1]:
            factor_score += 0.2

    return min(factor_score, 1.0)


def calculate_statistical_significance(signals: dict) -> float:
    """Calculate overall statistical significance"""
    signal_values = [v for v in signals.values() if isinstance(v, (int, float))]
    if not signal_values:
        return 0.5

    # Simplified statistical significance based on signal consistency
    signal_consistency = 1 - (max(signal_values) - min(signal_values)) / 2
    signal_strength = sum(abs(v) for v in signal_values) / len(signal_values)

    return (signal_consistency * 0.6 + signal_strength * 0.4)


def model_volatility_patterns(financial_line_items: list) -> float:
    """Model volatility patterns"""
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 4:
        volatility = calculate_coefficient_of_variation(revenues)
        return max(1 - volatility, 0)  # Lower volatility = higher score
    return 0.5


def analyze_correlation_risks(metrics: list, financial_line_items: list) -> float:
    """Analyze correlation with market factors"""
    # Simplified correlation analysis
    return 0.4  # Placeholder - would calculate correlation with market indices


def model_liquidity_risk(financial_line_items: list, market_cap: float) -> float:
    """Model liquidity risk"""
    if market_cap > 10_000_000_000:  # $10B+ market cap
        return 0.9
    elif market_cap > 1_000_000_000:  # $1B+ market cap
        return 0.7
    else:
        return 0.4


def decompose_factor_risks(metrics: list) -> dict:
    """Decompose factor risks"""
    return {
        "market_risk": 0.4,
        "size_risk": 0.2,
        "value_risk": 0.1,
        "momentum_risk": 0.1,
        "idiosyncratic_risk": 0.2
    }


def calculate_maximum_drawdown_risk(financial_line_items: list) -> float:
    """Calculate maximum drawdown risk"""
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings:
        peak = max(earnings)
        trough = min(earnings)
        if peak > 0:
            return abs(trough - peak) / peak
    return 0.5


def estimate_market_impact(market_cap: float) -> float:
    """Estimate market impact for trading"""
    if market_cap > 50_000_000_000:  # $50B+
        return 0.001
    elif market_cap > 10_000_000_000:  # $10B+
        return 0.005
    elif market_cap > 1_000_000_000:  # $1B+
        return 0.02
    else:
        return 0.1


def analyze_execution_timing(financial_line_items: list) -> float:
    """Analyze optimal execution timing"""
    return 0.7  # Placeholder for execution timing analysis


def estimate_transaction_costs(market_cap: float) -> float:
    """Estimate transaction costs"""
    if market_cap > 10_000_000_000:
        return 0.005
    elif market_cap > 1_000_000_000:
        return 0.01
    else:
        return 0.03


def calculate_slippage_risk(market_cap: float) -> float:
    """Calculate slippage risk"""
    return 1 / (1 + math.log10(market_cap / 1_000_000_000))  # Decreases with market cap


def calculate_pairs_trading_signals(metrics: list, financial_line_items: list) -> float:
    """Calculate pairs trading signals"""
    return 0.6  # Placeholder for pairs trading analysis


def detect_statistical_mispricing(metrics: list, financial_line_items: list) -> float:
    """Detect statistical mispricing"""
    if not metrics:
        return 0.5

    latest = metrics[0]
    mispricing_score = 0

    # P/E vs growth mispricing
    if latest.price_to_earnings and latest.price_to_earnings > 0:
        revenues = [item.revenue for item in financial_line_items if item.revenue]
        if len(revenues) >= 3:
            growth = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1
            expected_pe = 15 + (growth * 100)  # Simple growth-adjusted P/E
            if latest.price_to_earnings < expected_pe * 0.8:
                mispricing_score += 0.5

    return mispricing_score


def detect_regime_changes(financial_line_items: list) -> float:
    """Detect regime changes in business fundamentals"""
    margins = []
    for item in financial_line_items:
        if item.net_income and item.revenue and item.revenue > 0:
            margins.append(item.net_income / item.revenue)

    if len(margins) >= 4:
        recent_avg = sum(margins[:2]) / 2
        historical_avg = sum(margins[2:]) / len(margins[2:])

        if abs(recent_avg - historical_avg) > 0.05:  # 5% margin change
            return 0.8

    return 0.2


def analyze_market_microstructure(metrics: list) -> float:
    """Analyze market microstructure signals"""
    return 0.5  # Placeholder for microstructure analysis


def calculate_autocorrelation(data: list, lag: int) -> float:
    """Calculate autocorrelation with given lag"""
    if len(data) <= lag:
        return 0

    mean_val = sum(data) / len(data)
    numerator = sum((data[i] - mean_val) * (data[i + lag] - mean_val)
                    for i in range(len(data) - lag))
    denominator = sum((x - mean_val) ** 2 for x in data)

    return numerator / denominator if denominator != 0 else 0


def calculate_coefficient_of_variation(data: list) -> float:
    """Calculate coefficient of variation"""
    if not data:
        return 1.0

    mean_val = sum(data) / len(data)
    if mean_val == 0:
        return 1.0

    variance = sum((x - mean_val) ** 2 for x in data) / len(data)
    std_dev = math.sqrt(variance)

    return std_dev / abs(mean_val)


def generate_renaissance_output(ticker: str, analysis_data: dict, state: AgentState,
                                agent_id: str) -> RenaissanceSignal:
    """Generate Renaissance's systematic quantitative decision"""
    template = ChatPromptTemplate.from_messages([
        ("system", """You are Renaissance Technologies' AI system, implementing Jim Simons' quantitative systematic approach:

        ORGANIZATIONAL STRUCTURE:
        - Signal Generation Team: Mathematical pattern recognition and statistical signals
        - Risk Modeling Team: Volatility, correlation, and factor risk analysis
        - Execution Optimization Team: Market impact and transaction cost modeling
        - Statistical Arbitrage Team: Pairs trading and mispricing detection
        - Jim Simons Systematic Synthesis: Mathematical model integration

        PHILOSOPHY:
        1. Pure Quantitative: Mathematical models over fundamental analysis
        2. Statistical Edge: Identify small, consistent statistical advantages
        3. Systematic Execution: Remove human emotion and bias
        4. High Frequency: Exploit short-term market inefficiencies
        5. Risk Management: Sophisticated mathematical risk models
        6. Diversification: Many small bets rather than few large ones
        7. Continuous Learning: Models adapt and evolve systematically

        REASONING STYLE:
        - Reference mathematical models and statistical significance
        - Discuss signal generation algorithms and pattern recognition
        - Consider risk-adjusted returns and Sharpe ratios
        - Apply systematic decision thresholds and confidence intervals
        - Express reasoning in quantitative terms
        - Acknowledge model limitations and uncertainty
        - Focus on statistical edge and execution feasibility

        Return the investment signal with statistical edge and signal strength metrics."""),

        ("human", """Apply Renaissance's quantitative systematic analysis to {ticker}:

        {analysis_data}

        Provide investment signal in JSON format:
        {{
          "signal": "bullish" | "bearish" | "neutral",
          "confidence": float (0-100),
          "reasoning": "string",
          "statistical_edge": float (0-1),
          "signal_strength": {{
            "mean_reversion": float,
            "momentum": float,
            "statistical_arbitrage": float,
            "risk_adjusted_return": float
          }}
        }}""")
    ])

    prompt = template.invoke({"analysis_data": json.dumps(analysis_data, indent=2), "ticker": ticker})

    def create_default_renaissance_signal():
        return RenaissanceSignal(
            signal="neutral",
            confidence=0.0,
            reasoning="Analysis error, defaulting to neutral",
            statistical_edge=0.0,
            signal_strength={
                "mean_reversion": 0.0,
                "momentum": 0.0,
                "statistical_arbitrage": 0.0,
                "risk_adjusted_return": 0.0
            }
        )

    return call_llm(
        prompt=prompt,
        pydantic_model=RenaissanceSignal,
        agent_name=agent_id,
        state=state,
        default_factory=create_default_renaissance_signal,
    )