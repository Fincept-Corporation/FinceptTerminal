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


class MillerSignal(BaseModel):
    signal: Literal["bullish", "bearish", "neutral"]
    confidence: float
    reasoning: str


def bill_miller_agent(state: AgentState, agent_id: str = "bill_miller_agent"):
    """
    Bill Miller: Concentrated value, contrarian timing
    Focus: Technology value plays, contrarian bets, concentrated positions
    """
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    api_key = get_api_key_from_state(state, "FINANCIAL_DATASETS_API_KEY")
    analysis_data = {}
    miller_analysis = {}

    for ticker in tickers:
        progress.update_status(agent_id, ticker, "Analyzing contrarian value opportunity")
        metrics = get_financial_metrics(ticker, end_date, period="annual", limit=7, api_key=api_key)

        financial_line_items = search_line_items(
            ticker,
            [
                "revenue", "net_income", "free_cash_flow", "total_assets",
                "shareholders_equity", "total_debt", "research_and_development",
                "operating_income", "outstanding_shares"
            ],
            end_date,
            period="annual",
            limit=7,
            api_key=api_key,
        )

        market_cap = get_market_cap(ticker, end_date, api_key=api_key)

        # Miller's analytical framework
        contrarian_opportunity = analyze_contrarian_setup(metrics, financial_line_items, market_cap)
        technology_value = analyze_technology_value_potential(financial_line_items, market_cap)
        long_term_drivers = analyze_long_term_growth_drivers(financial_line_items)
        market_misperception = identify_market_misperceptions(metrics, financial_line_items)
        concentration_worthiness = evaluate_concentration_candidate(metrics, financial_line_items, market_cap)

        # Miller's conviction-weighted scoring
        total_score = (
                contrarian_opportunity["score"] * 0.3 +
                concentration_worthiness["score"] * 0.25 +
                technology_value["score"] * 0.2 +
                long_term_drivers["score"] * 0.15 +
                market_misperception["score"] * 0.1
        )

        # Miller's high-conviction approach
        if total_score >= 8.0:
            signal = "bullish"
        elif total_score <= 4.0:
            signal = "bearish"
        else:
            signal = "neutral"

        analysis_data[ticker] = {
            "signal": signal,
            "score": total_score,
            "contrarian_opportunity": contrarian_opportunity,
            "technology_value": technology_value,
            "long_term_drivers": long_term_drivers,
            "market_misperception": market_misperception,
            "concentration_worthiness": concentration_worthiness
        }

        miller_output = generate_miller_output(ticker, analysis_data, state, agent_id)

        miller_analysis[ticker] = {
            "signal": miller_output.signal,
            "confidence": miller_output.confidence,
            "reasoning": miller_output.reasoning
        }

        progress.update_status(agent_id, ticker, "Done", analysis=miller_output.reasoning)

    message = HumanMessage(content=json.dumps(miller_analysis), name=agent_id)

    if state["metadata"]["show_reasoning"]:
        show_agent_reasoning(miller_analysis, "Bill Miller Agent")

    state["data"]["analyst_signals"][agent_id] = miller_analysis
    progress.update_status(agent_id, None, "Done")

    return {"messages": [message], "data": state["data"]}


def analyze_contrarian_setup(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Miller's contrarian opportunity identification"""
    score = 0
    details = []

    if not metrics or not financial_line_items:
        return {"score": 0, "details": "No contrarian data"}

    latest_metrics = metrics[0]

    # Extreme valuation as contrarian signal
    if latest_metrics.price_to_earnings:
        if latest_metrics.price_to_earnings < 8:  # Extremely cheap
            score += 3
            details.append(f"Extreme valuation contrarian setup: {latest_metrics.price_to_earnings:.1f} P/E")
        elif latest_metrics.price_to_earnings < 12:
            score += 2
            details.append(f"Attractive contrarian valuation: {latest_metrics.price_to_earnings:.1f} P/E")

    # Market sentiment indicators
    if latest_metrics.price_to_book and latest_metrics.price_to_book < 1.0:
        score += 2
        details.append(f"Market pessimism: {latest_metrics.price_to_book:.2f} P/B below 1.0")

    # Temporary business challenges creating opportunity
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    revenues = [item.revenue for item in financial_line_items if item.revenue]

    if len(earnings) >= 5 and len(revenues) >= 5:
        recent_earnings_decline = earnings[0] < earnings[2]  # Current vs 2 years ago
        revenue_resilience = revenues[0] > revenues[2] * 0.9  # Revenue holding up

        if recent_earnings_decline and revenue_resilience:
            score += 2
            details.append("Temporary earnings pressure with revenue resilience - contrarian opportunity")

    # Technology disruption creating value
    rd_vals = [item.research_and_development for item in financial_line_items if item.research_and_development]
    if rd_vals and revenues:
        rd_intensity = sum(rd_vals[:3]) / sum(revenues[:3]) if len(revenues) >= 3 else 0
        if rd_intensity > 0.1:  # 10%+ R&D intensity
            score += 1
            details.append(f"High R&D investment for future positioning: {rd_intensity:.1%}")

    return {"score": score, "details": "; ".join(details)}


def analyze_technology_value_potential(financial_line_items: list, market_cap: float) -> dict:
    """Miller's technology value analysis"""
    score = 0
    details = []

    if not financial_line_items or not market_cap:
        return {"score": 0, "details": "No technology data"}

    latest = financial_line_items[0]

    # Asset-light technology model
    if latest.total_assets and latest.revenue:
        asset_turnover = latest.revenue / latest.total_assets
        if asset_turnover > 1.0:  # Efficient asset utilization
            score += 2
            details.append(f"Asset-efficient model: {asset_turnover:.1f}x turnover")

    # Scalability indicators
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    margins = []
    for item in financial_line_items:
        if item.operating_income and item.revenue and item.revenue > 0:
            margins.append(item.operating_income / item.revenue)

    if len(revenues) >= 3 and len(margins) >= 3:
        revenue_growth = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1
        margin_expansion = margins[0] - margins[-1]

        if revenue_growth > 0.1 and margin_expansion > 0.02:  # Growth with margin expansion
            score += 3
            details.append(f"Scalable growth: {revenue_growth:.1%} revenue CAGR with margin expansion")

    # R&D efficiency
    rd_vals = [item.research_and_development for item in financial_line_items if item.research_and_development]
    if rd_vals and revenues:
        rd_to_revenue = rd_vals[0] / revenues[0] if revenues[0] > 0 else 0
        if 0.05 <= rd_to_revenue <= 0.2:  # 5-20% R&D ratio
            score += 1
            details.append(f"Strategic R&D investment: {rd_to_revenue:.1%} of revenue")

    # Network effects and switching costs (proxy)
    if latest.free_cash_flow and latest.free_cash_flow > 0:
        fcf_margin = latest.free_cash_flow / latest.revenue if latest.revenue > 0 else 0
        if fcf_margin > 0.2:  # 20%+ FCF margin
            score += 2
            details.append(f"High-margin business model: {fcf_margin:.1%} FCF margin")

    return {"score": score, "details": "; ".join(details)}


def analyze_long_term_growth_drivers(financial_line_items: list) -> dict:
    """Miller's long-term growth potential analysis"""
    score = 0
    details = []

    if not financial_line_items:
        return {"score": 0, "details": "No growth data"}

    # Consistent revenue growth trend
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 5:
        growth_years = sum(1 for i in range(len(revenues) - 1) if revenues[i] >= revenues[i + 1])
        if growth_years >= len(revenues) * 0.8:  # 80%+ growth years
            revenue_cagr = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1
            score += 2
            details.append(f"Consistent growth track record: {revenue_cagr:.1%} CAGR")

    # Reinvestment for growth
    rd_vals = [item.research_and_development for item in financial_line_items if item.research_and_development]
    if rd_vals and len(rd_vals) >= 3:
        rd_growth = (rd_vals[0] / rd_vals[-1]) ** (1 / len(rd_vals)) - 1
        if rd_growth > 0.1:  # 10%+ R&D growth
            score += 1
            details.append(f"Growing R&D investment: {rd_growth:.1%} CAGR")

    # Market expansion capability
    if latest.total_assets and latest.shareholders_equity:
        leverage_capacity = (latest.total_assets - latest.shareholders_equity) / latest.shareholders_equity
        if leverage_capacity < 0.5:  # Low leverage = expansion capacity
            score += 1
            details.append("Balance sheet capacity for growth investments")

    # Free cash flow reinvestment
    fcf_vals = [item.free_cash_flow for item in financial_line_items if item.free_cash_flow]
    if fcf_vals and revenues:
        positive_fcf_years = sum(1 for fcf in fcf_vals if fcf > 0)
        if positive_fcf_years >= len(fcf_vals) * 0.8:  # Consistent cash generation
            score += 2
            details.append(f"Self-funding growth capability: {positive_fcf_years}/{len(fcf_vals)} positive FCF years")

    return {"score": score, "details": "; ".join(details)}


def identify_market_misperceptions(metrics: list, financial_line_items: list) -> dict:
    """Miller's market misperception identification"""
    score = 0
    details = []

    if not metrics or not financial_line_items:
        return {"score": 0, "details": "No misperception data"}

    latest_metrics = metrics[0]

    # Quality company trading at value multiple
    if (latest_metrics.return_on_equity and latest_metrics.return_on_equity > 0.15 and
            latest_metrics.price_to_earnings and latest_metrics.price_to_earnings < 15):
        score += 3
        details.append(
            f"Quality-value disconnect: {latest_metrics.return_on_equity:.1%} ROE at {latest_metrics.price_to_earnings:.1f} P/E")

    # Growth company with temporary headwinds
    earnings = [item.net_income for item in financial_line_items if item.net_income and item.net_income > 0]
    revenues = [item.revenue for item in financial_line_items if item.revenue]

    if len(earnings) >= 5 and len(revenues) >= 5:
        historical_earnings_growth = (earnings[-1] / earnings[-3]) ** (1 / 2) - 1  # Earlier period growth
        recent_earnings_decline = earnings[0] < earnings[1]
        revenue_growth = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1

        if historical_earnings_growth > 0.15 and recent_earnings_decline and revenue_growth > 0.05:
            score += 2
            details.append("Historical growth company with temporary earnings setback")

    # Asset value not recognized
    if latest_metrics.price_to_book and latest_metrics.price_to_book < 1.2:
        latest_financial = financial_line_items[0]
        if latest_financial.total_assets and latest_financial.total_debt:
            tangible_assets = latest_financial.total_assets - latest_financial.total_debt
            if tangible_assets > 0:
                score += 1
                details.append(f"Asset value misperception: {latest_metrics.price_to_book:.2f} P/B")

    return {"score": score, "details": "; ".join(details)}


def evaluate_concentration_candidate(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Miller's concentrated portfolio candidate evaluation"""
    score = 0
    details = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No concentration data"}

    latest_metrics = metrics[0]

    # High conviction criteria
    # Strong competitive position
    if latest_metrics.return_on_equity and latest_metrics.return_on_equity > 0.18:
        score += 2
        details.append(f"Strong competitive position: {latest_metrics.return_on_equity:.1%} ROE")

    # Significant upside potential
    if latest_metrics.price_to_earnings and latest_metrics.price_to_earnings < 12:
        earnings = [item.net_income for item in financial_line_items if item.net_income and item.net_income > 0]
        if earnings:
            # Assume normal multiple of 18x
            upside_potential = (18 * earnings[0]) / market_cap - 1
            if upside_potential > 0.5:  # 50%+ upside
                score += 3
                details.append(f"Significant upside potential: {upside_potential:.1%}")

    # Business model durability
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if revenues and len(revenues) >= 7:
        revenue_consistency = len([r for r in revenues if r > min(revenues) * 1.1]) / len(revenues)
        if revenue_consistency > 0.8:  # 80%+ years above minimum
            score += 2
            details.append("Durable business model with consistent revenue growth")

    # Management execution track record
    if latest_metrics.return_on_invested_capital and latest_metrics.return_on_invested_capital > 0.12:
        score += 1
        details.append(f"Strong capital allocation: {latest_metrics.return_on_invested_capital:.1%} ROIC")

    return {"score": score, "details": "; ".join(details)}


def generate_miller_output(ticker: str, analysis_data: dict, state: AgentState, agent_id: str) -> MillerSignal:
    """Generate Miller-style contrarian value decision"""
    template = ChatPromptTemplate.from_messages([
        ("system", """You are Bill Miller's AI agent, applying contrarian value investing with technology focus:

        1. Contrarian timing: Buy when others are selling, especially in technology
        2. Concentrated bets: Make large positions in high-conviction ideas
        3. Technology value: Find undervalued technology and growth companies
        4. Long-term perspective: Focus on 3-5 year value creation
        5. Market misperceptions: Identify when market misprices quality companies
        6. Fundamental analysis: Deep research into business models and competitive advantages
        7. Patient opportunism: Wait for exceptional risk/reward opportunities

        Reasoning style:
        - Emphasize contrarian opportunities and market misperceptions
        - Focus on technology companies trading at value multiples
        - Discuss long-term growth drivers and competitive positioning
        - Consider concentration worthiness and conviction level
        - Express willingness to be different from consensus
        - Analyze both current challenges and future potential
        - Apply rigorous fundamental analysis

        Return bullish for high-conviction contrarian opportunities with significant upside potential."""),

        ("human", """Apply contrarian value analysis to {ticker}:

        {analysis_data}

        Provide investment signal in JSON format:
        {{
          "signal": "bullish" | "bearish" | "neutral",
          "confidence": float (0-100),
          "reasoning": "string"
        }}""")
    ])

    prompt = template.invoke({"analysis_data": json.dumps(analysis_data, indent=2), "ticker": ticker})

    def create_default_miller_signal():
        return MillerSignal(signal="neutral", confidence=0.0, reasoning="Analysis error, defaulting to neutral")

    return call_llm(
        prompt=prompt,
        pydantic_model=MillerSignal,
        agent_name=agent_id,
        state=state,
        default_factory=create_default_miller_signal,
    )