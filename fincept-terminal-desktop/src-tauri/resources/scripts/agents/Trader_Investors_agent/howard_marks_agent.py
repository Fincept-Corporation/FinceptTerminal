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
from typing_extensions import Literal
from fincept_terminal.Agents.src.utils.progress import progress
from fincept_terminal.Agents.src.utils.llm import call_llm
from fincept_terminal.Agents.src.utils.api_key import get_api_key_from_state


class MarksSignal(BaseModel):
    signal: Literal["bullish", "bearish", "neutral"]
    confidence: float
    reasoning: str


def howard_marks_agent(state: AgentState, agent_id: str = "howard_marks_agent"):
    """
    Howard Marks: Second-level thinking, risk assessment, cycles
    Focus: Market psychology, risk-adjusted returns, contrarian positioning
    """
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    api_key = get_api_key_from_state(state, "FINANCIAL_DATASETS_API_KEY")
    analysis_data = {}
    marks_analysis = {}

    for ticker in tickers:
        progress.update_status(agent_id, ticker, "Applying second-level thinking")
        metrics = get_financial_metrics(ticker, end_date, period="annual", limit=7, api_key=api_key)

        financial_line_items = search_line_items(
            ticker,
            [
                "revenue", "net_income", "free_cash_flow", "total_debt",
                "shareholders_equity", "total_assets", "operating_margin",
                "current_assets", "current_liabilities"
            ],
            end_date,
            period="annual",
            limit=7,
            api_key=api_key,
        )

        market_cap = get_market_cap(ticker, end_date, api_key=api_key)

        # Marks' analytical framework
        risk_assessment = analyze_risk_factors(metrics, financial_line_items, market_cap)
        cycle_positioning = analyze_cycle_position(metrics, financial_line_items)
        second_level_thinking = apply_second_level_thinking(metrics, financial_line_items, market_cap)
        market_psychology = analyze_market_psychology_indicators(metrics, market_cap)
        asymmetric_returns = evaluate_asymmetric_opportunities(financial_line_items, market_cap)

        # Marks' risk-adjusted scoring
        total_score = (
                risk_assessment["score"] * 0.3 +
                second_level_thinking["score"] * 0.25 +
                asymmetric_returns["score"] * 0.2 +
                cycle_positioning["score"] * 0.15 +
                market_psychology["score"] * 0.1
        )

        # Marks' nuanced approach
        if total_score >= 7.5:
            signal = "bullish"
        elif total_score <= 4.5:
            signal = "bearish"
        else:
            signal = "neutral"

        analysis_data[ticker] = {
            "signal": signal,
            "score": total_score,
            "risk_assessment": risk_assessment,
            "cycle_positioning": cycle_positioning,
            "second_level_thinking": second_level_thinking,
            "market_psychology": market_psychology,
            "asymmetric_returns": asymmetric_returns
        }

        marks_output = generate_marks_output(ticker, analysis_data, state, agent_id)

        marks_analysis[ticker] = {
            "signal": marks_output.signal,
            "confidence": marks_output.confidence,
            "reasoning": marks_output.reasoning
        }

        progress.update_status(agent_id, ticker, "Done", analysis=marks_output.reasoning)

    message = HumanMessage(content=json.dumps(marks_analysis), name=agent_id)

    if state["metadata"]["show_reasoning"]:
        show_agent_reasoning(marks_analysis, "Howard Marks Agent")

    state["data"]["analyst_signals"][agent_id] = marks_analysis
    progress.update_status(agent_id, None, "Done")

    return {"messages": [message], "data": state["data"]}


def analyze_risk_factors(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Marks' comprehensive risk assessment framework"""
    score = 0
    details = []
    risk_factors = []

    if not metrics or not financial_line_items:
        return {"score": 0, "details": "No data for risk analysis"}

    latest_metrics = metrics[0]
    latest_financial = financial_line_items[0]

    # Financial leverage risk
    if latest_metrics.debt_to_equity:
        if latest_metrics.debt_to_equity < 0.3:
            score += 2
            details.append(f"Low leverage risk: {latest_metrics.debt_to_equity:.2f} D/E")
        elif latest_metrics.debt_to_equity > 1.0:
            risk_factors.append("High leverage risk")
            score -= 1

    # Earnings quality and sustainability
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings and len(earnings) >= 5:
        earnings_volatility = (max(earnings) - min(earnings)) / max(abs(max(earnings)), abs(min(earnings)))
        if earnings_volatility < 0.5:
            score += 2
            details.append(f"Stable earnings pattern: {earnings_volatility:.2f} volatility")
        else:
            risk_factors.append("High earnings volatility")

    # Liquidity risk
    if latest_metrics.current_ratio:
        if latest_metrics.current_ratio > 1.5:
            score += 1
            details.append(f"Adequate liquidity: {latest_metrics.current_ratio:.1f} current ratio")
        elif latest_metrics.current_ratio < 1.0:
            risk_factors.append("Liquidity concerns")
            score -= 2

    # Business model sustainability
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if revenues and len(revenues) >= 5:
        revenue_trend = sum(1 for i in range(len(revenues) - 1) if revenues[i] >= revenues[i + 1])
        if revenue_trend >= len(revenues) * 0.7:
            score += 1
            details.append("Sustainable revenue pattern")
        else:
            risk_factors.append("Declining revenue trend")

    # Margin compression risk
    margins = [item.operating_margin for item in financial_line_items if item.operating_margin]
    if margins and len(margins) >= 3:
        margin_trend = margins[0] - margins[-1]
        if margin_trend > 0.02:  # 2% margin improvement
            score += 1
            details.append("Improving margin profile")
        elif margin_trend < -0.05:  # 5% margin decline
            risk_factors.append("Margin compression risk")
            score -= 1

    if risk_factors:
        details.append(f"Risk factors: {'; '.join(risk_factors)}")

    return {"score": score, "details": "; ".join(details), "risk_factors": risk_factors}


def analyze_cycle_position(metrics: list, financial_line_items: list) -> dict:
    """Marks' cycle awareness analysis"""
    score = 0
    details = []

    if not metrics or not financial_line_items:
        return {"score": 0, "details": "No cycle data"}

    # Revenue cycle analysis
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 5:
        # Analyze cyclical patterns
        recent_growth = (revenues[0] / revenues[2]) ** (1 / 2) - 1  # 2-year CAGR
        longer_growth = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1  # Full period CAGR

        if recent_growth < longer_growth * 0.5:  # Significant slowdown
            score += 2  # Good time to buy in cycle
            details.append("Potential cyclical trough - contrarian opportunity")
        elif recent_growth > longer_growth * 2:  # Acceleration
            score -= 1  # Potential cycle peak
            details.append("Potential cyclical peak - exercise caution")

    # Margin cycle analysis
    margins = [item.operating_margin for item in financial_line_items if item.operating_margin]
    if len(margins) >= 5:
        current_margin = margins[0]
        avg_margin = sum(margins) / len(margins)

        if current_margin < avg_margin * 0.8:  # Below average margins
            score += 1
            details.append("Below-average margins suggest cycle opportunity")

    # Return on equity cycle
    roe_vals = [m.return_on_equity for m in metrics if m.return_on_equity]
    if len(roe_vals) >= 5:
        current_roe = roe_vals[0]
        avg_roe = sum(roe_vals) / len(roe_vals)

        if current_roe < avg_roe * 0.7:  # Significantly below average
            score += 2
            details.append("Depressed ROE suggests cyclical opportunity")

    return {"score": score, "details": "; ".join(details)}


def apply_second_level_thinking(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Marks' second-level thinking analysis"""
    score = 0
    details = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No data for second-level analysis"}

    latest_metrics = metrics[0]
    latest_financial = financial_line_items[0]

    # What is the market NOT seeing?
    # Low valuation with improving fundamentals
    if latest_metrics.price_to_book and latest_metrics.price_to_book < 1.2:
        # Check if fundamentals are actually improving
        earnings = [item.net_income for item in financial_line_items if item.net_income]
        if len(earnings) >= 3 and earnings[0] > earnings[1] > 0:
            score += 3
            details.append("Market pessimism vs improving fundamentals - second-level opportunity")

    # Hidden asset value
    if latest_financial.total_assets and latest_financial.total_debt:
        net_asset_value = latest_financial.total_assets - latest_financial.total_debt
        if net_asset_value > market_cap * 1.3:
            score += 2
            details.append("Hidden asset value not reflected in price")

    # Temporary problems vs permanent impairment
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 5:
        recent_decline = revenues[0] < revenues[1]
        historical_growth = revenues[-1] < revenues[-3]  # Was growing historically

        if recent_decline and historical_growth:
            score += 1
            details.append("Temporary setback in historically growing business")

    # Quality company at cyclical low
    if latest_metrics.return_on_equity:
        historical_roe = [m.return_on_equity for m in metrics if m.return_on_equity]
        if len(historical_roe) >= 5:
            avg_roe = sum(historical_roe[1:]) / len(historical_roe[1:])  # Exclude current
            if avg_roe > 0.15 and latest_metrics.return_on_equity < avg_roe * 0.6:
                score += 2
                details.append("Quality company at cyclical low ROE")

    return {"score": score, "details": "; ".join(details)}


def analyze_market_psychology_indicators(metrics: list, market_cap: float) -> dict:
    """Marks' market psychology assessment"""
    score = 0
    details = []

    if not metrics or not market_cap:
        return {"score": 0, "details": "No psychology indicators"}

    latest_metrics = metrics[0]

    # Extreme valuation as psychology indicator
    if latest_metrics.price_to_earnings:
        if latest_metrics.price_to_earnings < 8:  # Extreme pessimism
            score += 2
            details.append(f"Market pessimism: {latest_metrics.price_to_earnings:.1f} P/E")
        elif latest_metrics.price_to_earnings > 30:  # Extreme optimism
            score -= 2
            details.append(f"Market euphoria: {latest_metrics.price_to_earnings:.1f} P/E")

    # Book value psychology
    if latest_metrics.price_to_book:
        if latest_metrics.price_to_book < 0.8:  # Trading below book
            score += 1
            details.append("Market pricing below book value - potential pessimism")
        elif latest_metrics.price_to_book > 3:  # High premium
            score -= 1
            details.append("Market paying high premium - potential optimism")

    # Contrarian indicators (simplified)
    # Would typically incorporate sentiment data, analyst revisions, etc.
    if latest_metrics.debt_to_equity and latest_metrics.debt_to_equity > 0.8:
        score += 1  # High debt companies often overlooked
        details.append("High debt company - potential contrarian opportunity")

    return {"score": score, "details": "; ".join(details)}


def evaluate_asymmetric_opportunities(financial_line_items: list, market_cap: float) -> dict:
    """Marks' asymmetric risk/return evaluation"""
    score = 0
    details = []

    if not financial_line_items or not market_cap:
        return {"score": 0, "details": "No asymmetric data"}

    latest = financial_line_items[0]

    # Downside protection analysis
    total_assets = latest.total_assets if latest.total_assets else 0
    total_debt = latest.total_debt if latest.total_debt else 0

    # Asset floor
    if total_assets > 0:
        net_assets = total_assets - total_debt
        if net_assets > market_cap * 0.8:  # Strong asset backing
            score += 2
            details.append("Strong downside protection from asset backing")

    # Upside potential from normalized earnings
    earnings = [item.net_income for item in financial_line_items if item.net_income and item.net_income > 0]
    if earnings and len(earnings) >= 3:
        # Use best historical earnings as proxy for potential
        best_earnings = max(earnings)
        current_earnings = earnings[0] if earnings[0] > 0 else earnings[1] if len(earnings) > 1 else 0

        if best_earnings > current_earnings * 1.5:  # Significant upside potential
            normalized_value = best_earnings * 15  # 15x multiple
            upside_ratio = normalized_value / market_cap

            if upside_ratio > 2:  # 100%+ upside potential
                score += 3
                details.append(f"Significant upside potential: {upside_ratio:.1f}x from normalized earnings")

    # Free cash flow asymmetry
    if latest.free_cash_flow and latest.free_cash_flow > 0:
        fcf_yield = latest.free_cash_flow / market_cap
        if fcf_yield > 0.1:  # 10%+ FCF yield
            score += 1
            details.append(f"Attractive FCF yield provides downside support: {fcf_yield:.1%}")

    return {"score": score, "details": "; ".join(details)}


def generate_marks_output(ticker: str, analysis_data: dict, state: AgentState, agent_id: str) -> MarksSignal:
    """Generate Marks-style risk-aware investment decision"""
    template = ChatPromptTemplate.from_messages([
        ("system", """You are Howard Marks' AI agent, applying sophisticated risk-aware value investing:

        1. Second-level thinking: "What is the market not seeing?"
        2. Risk assessment: "Risk is the probability of loss, not volatility"
        3. Cycle awareness: Understand where we are in the cycle
        4. Market psychology: Recognize euphoria and pessimism
        5. Asymmetric opportunities: Limited downside, significant upside
        6. Contrarian positioning: Buy when others are selling
        7. Risk-adjusted returns: Focus on risk-adjusted, not absolute returns

        Reasoning style:
        - Emphasize risk analysis and downside protection
        - Consider market psychology and sentiment
        - Apply second-level thinking to find hidden opportunities
        - Discuss cyclical positioning and timing
        - Focus on asymmetric risk/reward profiles
        - Express nuanced, probabilistic thinking
        - Acknowledge uncertainty and multiple scenarios

        Return bullish for asymmetric opportunities with limited downside and strong risk-adjusted returns."""),

        ("human", """Apply second-level thinking and risk analysis to {ticker}:

        {analysis_data}

        Provide investment signal in JSON format:
        {{
          "signal": "bullish" | "bearish" | "neutral",
          "confidence": float (0-100),
          "reasoning": "string"
        }}""")
    ])

    prompt = template.invoke({"analysis_data": json.dumps(analysis_data, indent=2), "ticker": ticker})

    def create_default_marks_signal():
        return MarksSignal(signal="neutral", confidence=0.0, reasoning="Analysis error, defaulting to neutral")

    return call_llm(
        prompt=prompt,
        pydantic_model=MarksSignal,
        agent_name=agent_id,
        state=state,
        default_factory=create_default_marks_signal,
    )