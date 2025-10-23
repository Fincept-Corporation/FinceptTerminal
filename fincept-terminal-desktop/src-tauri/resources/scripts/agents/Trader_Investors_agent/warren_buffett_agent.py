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


class BuffettSignal(BaseModel):
    signal: Literal["bullish", "bearish", "neutral"]
    confidence: float
    reasoning: str


def warren_buffett_agent(state: AgentState, agent_id: str = "warren_buffett_agent"):
    """
    Warren Buffett: Buy wonderful companies at fair prices, hold forever
    Focus: Economic moats, predictable earnings, strong management, reasonable price
    """
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    api_key = get_api_key_from_state(state, "FINANCIAL_DATASETS_API_KEY")
    analysis_data = {}
    buffett_analysis = {}

    for ticker in tickers:
        progress.update_status(agent_id, ticker, "Analyzing wonderful business qualities")
        metrics = get_financial_metrics(ticker, end_date, period="annual", limit=10, api_key=api_key)

        financial_line_items = search_line_items(
            ticker,
            [
                "revenue", "net_income", "free_cash_flow", "total_debt",
                "shareholders_equity", "retained_earnings", "operating_margin",
                "research_and_development", "outstanding_shares"
            ],
            end_date,
            period="annual",
            limit=10,
            api_key=api_key,
        )

        market_cap = get_market_cap(ticker, end_date, api_key=api_key)

        # Buffett's key analyses
        moat_analysis = analyze_economic_moat(metrics, financial_line_items)
        earnings_predictability = analyze_earnings_predictability(financial_line_items)
        financial_strength = analyze_financial_fortress(metrics, financial_line_items)
        management_quality = analyze_capital_allocation(financial_line_items)
        valuation_analysis = buffett_valuation(financial_line_items, market_cap)

        total_score = (
                moat_analysis["score"] * 0.3 +
                earnings_predictability["score"] * 0.25 +
                financial_strength["score"] * 0.2 +
                management_quality["score"] * 0.15 +
                valuation_analysis["score"] * 0.1
        )

        # Buffett's binary approach: Wonderful business or pass
        if total_score >= 8.0:
            signal = "bullish"
        elif total_score <= 4.0:
            signal = "bearish"
        else:
            signal = "neutral"

        analysis_data[ticker] = {
            "signal": signal,
            "score": total_score,
            "moat_analysis": moat_analysis,
            "earnings_predictability": earnings_predictability,
            "financial_strength": financial_strength,
            "management_quality": management_quality,
            "valuation_analysis": valuation_analysis
        }

        buffett_output = generate_buffett_output(ticker, analysis_data, state, agent_id)

        buffett_analysis[ticker] = {
            "signal": buffett_output.signal,
            "confidence": buffett_output.confidence,
            "reasoning": buffett_output.reasoning
        }

        progress.update_status(agent_id, ticker, "Done", analysis=buffett_output.reasoning)

    message = HumanMessage(content=json.dumps(buffett_analysis), name=agent_id)

    if state["metadata"]["show_reasoning"]:
        show_agent_reasoning(buffett_analysis, "Warren Buffett Agent")

    state["data"]["analyst_signals"][agent_id] = buffett_analysis
    progress.update_status(agent_id, None, "Done")

    return {"messages": [message], "data": state["data"]}


def analyze_economic_moat(metrics: list, financial_line_items: list) -> dict:
    """Buffett's moat analysis: High ROE, consistent margins, pricing power"""
    score = 0
    details = []

    if not metrics:
        return {"score": 0, "details": "No metrics data"}

    # High and consistent ROE (15%+ for 7+ years)
    roe_vals = [m.return_on_equity for m in metrics if m.return_on_equity]
    if roe_vals:
        high_roe_count = sum(1 for roe in roe_vals if roe > 0.15)
        if high_roe_count >= 7:
            score += 3
            details.append(f"Exceptional ROE consistency: {high_roe_count}/10 years >15%")
        elif high_roe_count >= 5:
            score += 2
            details.append(f"Good ROE consistency: {high_roe_count}/10 years >15%")

    # Consistent operating margins (pricing power)
    margins = [item.operating_margin for item in financial_line_items if item.operating_margin]
    if margins and len(margins) >= 5:
        margin_stability = max(margins) - min(margins)
        avg_margin = sum(margins) / len(margins)
        if margin_stability < 0.05 and avg_margin > 0.15:
            score += 2
            details.append(f"Stable high margins: {avg_margin:.1%} avg, {margin_stability:.1%} volatility")

    # Revenue growth without excessive capex
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 5:
        cagr = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1
        if 0.05 <= cagr <= 0.15:  # Buffett likes steady, not explosive growth
            score += 2
            details.append(f"Steady revenue growth: {cagr:.1%} CAGR")

    return {"score": score, "details": "; ".join(details)}


def analyze_earnings_predictability(financial_line_items: list) -> dict:
    """Buffett values predictable, growing earnings"""
    score = 0
    details = []

    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if len(earnings) < 5:
        return {"score": 0, "details": "Insufficient earnings history"}

    # Positive earnings in 8+ of last 10 years
    positive_years = sum(1 for e in earnings if e > 0)
    if positive_years >= 8:
        score += 3
        details.append(f"Consistent profitability: {positive_years}/10 years positive")

    # Earnings growth trend
    if len(earnings) >= 10:
        recent_avg = sum(earnings[:5]) / 5
        older_avg = sum(earnings[5:10]) / 5
        if recent_avg > older_avg * 1.5:
            score += 2
            details.append("Strong earnings growth trend over decade")

    # Low earnings volatility
    if earnings:
        volatility = (max(earnings) - min(earnings)) / max(abs(max(earnings)), abs(min(earnings)))
        if volatility < 1.0:  # Relatively stable
            score += 2
            details.append(f"Low earnings volatility: {volatility:.2f}")

    return {"score": score, "details": "; ".join(details)}


def analyze_financial_fortress(metrics: list, financial_line_items: list) -> dict:
    """Buffett's fortress balance sheet requirements"""
    score = 0
    details = []

    if not metrics or not financial_line_items:
        return {"score": 0, "details": "No financial data"}

    latest = metrics[0]

    # Low debt-to-equity (financial safety)
    if latest.debt_to_equity and latest.debt_to_equity < 0.3:
        score += 3
        details.append(f"Conservative debt: {latest.debt_to_equity:.2f} D/E")

    # High current ratio (liquidity)
    if latest.current_ratio and latest.current_ratio > 1.5:
        score += 2
        details.append(f"Strong liquidity: {latest.current_ratio:.1f} current ratio")

    # Positive free cash flow consistency
    fcf_vals = [item.free_cash_flow for item in financial_line_items if item.free_cash_flow]
    if fcf_vals:
        positive_fcf = sum(1 for fcf in fcf_vals if fcf > 0)
        if positive_fcf >= 8:
            score += 2
            details.append(f"Reliable cash generation: {positive_fcf}/10 years positive FCF")

    return {"score": score, "details": "; ".join(details)}


def analyze_capital_allocation(financial_line_items: list) -> dict:
    """Buffett values management that allocates capital wisely"""
    score = 0
    details = []

    # Share buybacks when appropriate (declining share count)
    shares = [item.outstanding_shares for item in financial_line_items if item.outstanding_shares]
    if len(shares) >= 5:
        share_reduction = (shares[-1] - shares[0]) / shares[-1]
        if share_reduction > 0.1:  # 10%+ reduction
            score += 2
            details.append(f"Shareholder-friendly buybacks: {share_reduction:.1%} share reduction")

    # Retained earnings growth (reinvestment effectiveness)
    retained_earnings = [item.retained_earnings for item in financial_line_items if item.retained_earnings]
    if len(retained_earnings) >= 5:
        re_growth = (retained_earnings[0] - retained_earnings[-1]) / abs(retained_earnings[-1])
        if re_growth > 0.5:  # 50%+ growth in retained earnings
            score += 2
            details.append(f"Effective reinvestment: {re_growth:.1%} retained earnings growth")

    # Reasonable R&D (not excessive, not zero)
    rd_vals = [item.research_and_development for item in financial_line_items if item.research_and_development]
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if rd_vals and revenues:
        rd_intensity = sum(rd_vals) / sum(revenues)
        if 0.02 <= rd_intensity <= 0.08:  # 2-8% of revenue
            score += 1
            details.append(f"Balanced R&D spend: {rd_intensity:.1%} of revenue")

    return {"score": score, "details": "; ".join(details)}


def buffett_valuation(financial_line_items: list, market_cap: float) -> dict:
    """Buffett's 'fair price for wonderful business' approach"""
    score = 0
    details = []

    if not financial_line_items or not market_cap:
        return {"score": 0, "details": "No valuation data"}

    latest = financial_line_items[0]
    earnings = [item.net_income for item in financial_line_items if item.net_income and item.net_income > 0]

    if not earnings:
        return {"score": 0, "details": "No positive earnings for valuation"}

    # Owner earnings approximation (FCF)
    if latest.free_cash_flow and latest.free_cash_flow > 0:
        fcf_yield = latest.free_cash_flow / market_cap
        if fcf_yield > 0.08:  # 8%+ FCF yield
            score += 3
            details.append(f"Attractive FCF yield: {fcf_yield:.1%}")
        elif fcf_yield > 0.05:
            score += 1
            details.append(f"Reasonable FCF yield: {fcf_yield:.1%}")

    # PE ratio (Buffett likes reasonable multiples)
    if latest.net_income and latest.net_income > 0:
        pe_ratio = market_cap / latest.net_income
        if pe_ratio < 15:
            score += 2
            details.append(f"Reasonable valuation: {pe_ratio:.1f} P/E")
        elif pe_ratio < 25:
            score += 1
            details.append(f"Fair valuation: {pe_ratio:.1f} P/E")

    return {"score": score, "details": "; ".join(details)}


def generate_buffett_output(ticker: str, analysis_data: dict, state: AgentState, agent_id: str) -> BuffettSignal:
    """Generate Buffett-style investment decision"""
    template = ChatPromptTemplate.from_messages([
        ("system", """You are Warren Buffett's AI agent. Follow his investment philosophy:

        1. "Buy wonderful companies at fair prices" - Seek businesses with durable competitive advantages
        2. Economic moats: High ROE, pricing power, brand strength, network effects
        3. Predictable earnings: Consistent profitability over many years
        4. Financial fortress: Low debt, strong balance sheet, reliable cash flow
        5. Quality management: Efficient capital allocation, shareholder-friendly
        6. Long-term perspective: "Our favorite holding period is forever"
        7. Circle of competence: Understand the business model completely

        Reasoning style:
        - Focus on business fundamentals, not market movements
        - Emphasize competitive advantages and moats
        - Discuss management quality and capital allocation
        - Value predictability over growth
        - Use homespun analogies and simple explanations
        - Express high conviction for quality businesses

        Return signal: bullish (wonderful business at fair price), neutral (good business, wrong price), bearish (poor business fundamentals)"""),

        ("human", """Analyze {ticker} using Buffett's criteria:

        {analysis_data}

        Provide investment signal in JSON format:
        {{
          "signal": "bullish" | "bearish" | "neutral",
          "confidence": float (0-100),
          "reasoning": "string"
        }}""")
    ])

    prompt = template.invoke({"analysis_data": json.dumps(analysis_data, indent=2), "ticker": ticker})

    def create_default_buffett_signal():
        return BuffettSignal(signal="neutral", confidence=0.0, reasoning="Analysis error, defaulting to neutral")

    return call_llm(
        prompt=prompt,
        pydantic_model=BuffettSignal,
        agent_name=agent_id,
        state=state,
        default_factory=create_default_buffett_signal,
    )