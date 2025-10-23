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


class GreenblattSignal(BaseModel):
    signal: Literal["bullish", "bearish", "neutral"]
    confidence: float
    reasoning: str


def joel_greenblatt_agent(state: AgentState, agent_id: str = "joel_greenblatt_agent"):
    """
    Joel Greenblatt: Magic formula (high ROC + low P/E)
    Focus: Systematic value investing, high returns on capital, cheap valuations
    """
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    api_key = get_api_key_from_state(state, "FINANCIAL_DATASETS_API_KEY")
    analysis_data = {}
    greenblatt_analysis = {}

    for ticker in tickers:
        progress.update_status(agent_id, ticker, "Applying Magic Formula criteria")
        metrics = get_financial_metrics(ticker, end_date, period="annual", limit=5, api_key=api_key)

        financial_line_items = search_line_items(
            ticker,
            [
                "revenue", "net_income", "total_assets", "current_assets",
                "current_liabilities", "total_debt", "shareholders_equity",
                "operating_income", "interest_expense", "free_cash_flow"
            ],
            end_date,
            period="annual",
            limit=5,
            api_key=api_key,
        )

        market_cap = get_market_cap(ticker, end_date, api_key=api_key)

        # Greenblatt's Magic Formula components
        earnings_yield = calculate_earnings_yield(financial_line_items, market_cap)
        return_on_capital = calculate_return_on_capital(financial_line_items)
        magic_formula_rank = calculate_magic_formula_rank(earnings_yield, return_on_capital)
        business_quality = analyze_business_quality_greenblatt(metrics, financial_line_items)
        special_situations = analyze_special_situations(financial_line_items, market_cap)

        # Greenblatt's systematic scoring
        total_score = (
                magic_formula_rank["score"] * 0.4 +
                earnings_yield["score"] * 0.25 +
                return_on_capital["score"] * 0.25 +
                business_quality["score"] * 0.1
        )

        # Magic Formula approach: systematic and mechanical
        if total_score >= 8.0:
            signal = "bullish"
        elif total_score <= 4.0:
            signal = "bearish"
        else:
            signal = "neutral"

        analysis_data[ticker] = {
            "signal": signal,
            "score": total_score,
            "earnings_yield": earnings_yield,
            "return_on_capital": return_on_capital,
            "magic_formula_rank": magic_formula_rank,
            "business_quality": business_quality,
            "special_situations": special_situations
        }

        greenblatt_output = generate_greenblatt_output(ticker, analysis_data, state, agent_id)

        greenblatt_analysis[ticker] = {
            "signal": greenblatt_output.signal,
            "confidence": greenblatt_output.confidence,
            "reasoning": greenblatt_output.reasoning
        }

        progress.update_status(agent_id, ticker, "Done", analysis=greenblatt_output.reasoning)

    message = HumanMessage(content=json.dumps(greenblatt_analysis), name=agent_id)

    if state["metadata"]["show_reasoning"]:
        show_agent_reasoning(greenblatt_analysis, "Joel Greenblatt Agent")

    state["data"]["analyst_signals"][agent_id] = greenblatt_analysis
    progress.update_status(agent_id, None, "Done")

    return {"messages": [message], "data": state["data"]}


def calculate_earnings_yield(financial_line_items: list, market_cap: float) -> dict:
    """Calculate Greenblatt's earnings yield (EBIT / Enterprise Value)"""
    score = 0
    details = []

    if not financial_line_items or not market_cap:
        return {"score": 0, "details": "No data for earnings yield", "value": None}

    latest = financial_line_items[0]

    # Calculate EBIT (Operating Income)
    ebit = latest.operating_income if latest.operating_income else 0

    # Calculate Enterprise Value
    total_debt = latest.total_debt if latest.total_debt else 0
    cash = latest.current_assets * 0.2 if latest.current_assets else 0  # Rough cash estimate
    enterprise_value = market_cap + total_debt - cash

    if enterprise_value > 0 and ebit > 0:
        earnings_yield = ebit / enterprise_value

        # Greenblatt's scoring system
        if earnings_yield > 0.15:  # 15%+ earnings yield
            score = 10
            details.append(f"Exceptional earnings yield: {earnings_yield:.1%}")
        elif earnings_yield > 0.12:
            score = 8
            details.append(f"Strong earnings yield: {earnings_yield:.1%}")
        elif earnings_yield > 0.09:
            score = 6
            details.append(f"Good earnings yield: {earnings_yield:.1%}")
        elif earnings_yield > 0.06:
            score = 4
            details.append(f"Fair earnings yield: {earnings_yield:.1%}")
        else:
            score = 2
            details.append(f"Low earnings yield: {earnings_yield:.1%}")
    else:
        earnings_yield = 0
        details.append("No positive earnings yield")

    return {
        "score": score,
        "value": earnings_yield,
        "details": "; ".join(details)
    }


def calculate_return_on_capital(financial_line_items: list) -> dict:
    """Calculate Greenblatt's return on invested capital"""
    score = 0
    details = []

    if not financial_line_items:
        return {"score": 0, "details": "No data for ROIC", "value": None}

    latest = financial_line_items[0]

    # Calculate ROIC = EBIT / (Net Working Capital + Net Fixed Assets)
    ebit = latest.operating_income if latest.operating_income else 0

    # Working Capital
    current_assets = latest.current_assets if latest.current_assets else 0
    current_liabilities = latest.current_liabilities if latest.current_liabilities else 0
    working_capital = current_assets - current_liabilities

    # Invested Capital (simplified)
    total_assets = latest.total_assets if latest.total_assets else 0
    invested_capital = max(working_capital + (total_assets - current_assets), total_assets * 0.5)

    if invested_capital > 0 and ebit > 0:
        roic = ebit / invested_capital

        # Greenblatt's ROIC scoring
        if roic > 0.25:  # 25%+ ROIC
            score = 10
            details.append(f"Exceptional ROIC: {roic:.1%}")
        elif roic > 0.20:
            score = 8
            details.append(f"Strong ROIC: {roic:.1%}")
        elif roic > 0.15:
            score = 6
            details.append(f"Good ROIC: {roic:.1%}")
        elif roic > 0.10:
            score = 4
            details.append(f"Fair ROIC: {roic:.1%}")
        else:
            score = 2
            details.append(f"Low ROIC: {roic:.1%}")
    else:
        roic = 0
        details.append("No positive return on capital")

    return {
        "score": score,
        "value": roic,
        "details": "; ".join(details)
    }


def calculate_magic_formula_rank(earnings_yield: dict, return_on_capital: dict) -> dict:
    """Combine earnings yield and ROIC for Magic Formula ranking"""
    score = 0
    details = []

    ey_score = earnings_yield.get("score", 0)
    roic_score = return_on_capital.get("score", 0)

    # Magic Formula combines both metrics
    combined_score = (ey_score + roic_score) / 2

    if combined_score >= 9:
        score = 10
        details.append("Magic Formula: Top tier (high ROIC + high earnings yield)")
    elif combined_score >= 7:
        score = 8
        details.append("Magic Formula: Strong candidate")
    elif combined_score >= 5:
        score = 6
        details.append("Magic Formula: Decent candidate")
    else:
        score = 3
        details.append("Magic Formula: Below average")

    # Bonus for exceptional combination
    ey_val = earnings_yield.get("value", 0)
    roic_val = return_on_capital.get("value", 0)

    if ey_val > 0.12 and roic_val > 0.20:
        score = min(10, score + 2)
        details.append("Exceptional Magic Formula combination")

    return {
        "score": score,
        "combined_score": combined_score,
        "details": "; ".join(details)
    }


def analyze_business_quality_greenblatt(metrics: list, financial_line_items: list) -> dict:
    """Greenblatt's additional quality checks beyond Magic Formula"""
    score = 0
    details = []

    if not metrics or not financial_line_items:
        return {"score": 0, "details": "No data for quality analysis"}

    # Consistent profitability
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings:
        positive_years = sum(1 for e in earnings if e > 0)
        if positive_years == len(earnings):
            score += 3
            details.append(f"Consistent profitability: {positive_years}/{len(earnings)} years")

    # Revenue growth
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 3:
        revenue_growth = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1
        if revenue_growth > 0.05:
            score += 2
            details.append(f"Revenue growth: {revenue_growth:.1%} CAGR")

    # Free cash flow generation
    fcf_vals = [item.free_cash_flow for item in financial_line_items if item.free_cash_flow]
    if fcf_vals:
        positive_fcf = sum(1 for fcf in fcf_vals if fcf > 0)
        if positive_fcf >= len(fcf_vals) * 0.8:
            score += 2
            details.append(f"Strong FCF generation: {positive_fcf}/{len(fcf_vals)} years")

    return {"score": score, "details": "; ".join(details)}


def analyze_special_situations(financial_line_items: list, market_cap: float) -> dict:
    """Greenblatt's special situation analysis"""
    score = 0
    details = []

    if not financial_line_items or not market_cap:
        return {"score": 0, "details": "No special situation data"}

    latest = financial_line_items[0]

    # Net cash position (special situation value)
    current_assets = latest.current_assets if latest.current_assets else 0
    total_debt = latest.total_debt if latest.total_debt else 0

    if current_assets > total_debt * 1.5:
        net_cash = current_assets - total_debt
        if net_cash > market_cap * 0.2:
            score += 2
            details.append(f"Net cash opportunity: {(net_cash / market_cap):.1%} of market cap")

    # Asset value relative to market cap
    if latest.total_assets:
        asset_ratio = latest.total_assets / market_cap
        if asset_ratio > 1.5:
            score += 1
            details.append(f"Asset value: {asset_ratio:.1f}x market cap")

    return {"score": score, "details": "; ".join(details)}


def generate_greenblatt_output(ticker: str, analysis_data: dict, state: AgentState, agent_id: str) -> GreenblattSignal:
    """Generate Greenblatt-style Magic Formula investment decision"""
    template = ChatPromptTemplate.from_messages([
        ("system", """You are Joel Greenblatt's AI agent, implementing the Magic Formula strategy:

        1. Magic Formula: High return on invested capital + High earnings yield
        2. Systematic approach: Remove emotion, follow the formula mechanically
        3. Value investing: Buy good businesses at cheap prices
        4. Special situations: Spinoffs, mergers, bankruptcies for extra returns
        5. Long-term perspective: Hold for 2-3 years minimum
        6. Diversification: Own 20-30 positions to reduce risk
        7. Patience: Trust the process even during underperformance

        Reasoning style:
        - Emphasize quantitative Magic Formula metrics
        - Focus on ROIC and earnings yield primarily
        - Discuss systematic, mechanical approach
        - Reference statistical advantages of the formula
        - Consider special situations as bonus opportunities
        - Express confidence in proven methodology
        - Acknowledge short-term volatility but long-term outperformance

        Return bullish for high Magic Formula scores (top quartile companies)."""),

        ("human", """Apply Magic Formula analysis to {ticker}:

        {analysis_data}

        Provide investment signal in JSON format:
        {{
          "signal": "bullish" | "bearish" | "neutral",
          "confidence": float (0-100),
          "reasoning": "string"
        }}""")
    ])

    prompt = template.invoke({"analysis_data": json.dumps(analysis_data, indent=2), "ticker": ticker})

    def create_default_greenblatt_signal():
        return GreenblattSignal(signal="neutral", confidence=0.0, reasoning="Analysis error, defaulting to neutral")

    return call_llm(
        prompt=prompt,
        pydantic_model=GreenblattSignal,
        agent_name=agent_id,
        state=state,
        default_factory=create_default_greenblatt_signal,
    )