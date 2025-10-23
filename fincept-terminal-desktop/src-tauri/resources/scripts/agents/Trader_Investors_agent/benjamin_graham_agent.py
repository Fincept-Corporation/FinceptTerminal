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


class GrahamSignal(BaseModel):
    signal: Literal["bullish", "bearish", "neutral"]
    confidence: float
    reasoning: str


def benjamin_graham_agent(state: AgentState, agent_id: str = "benjamin_graham_agent"):
    """
    Benjamin Graham: Margin of safety, net-net stocks, mathematical approach
    Focus: Quantitative screens, asset protection, statistical cheapness
    """
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    api_key = get_api_key_from_state(state, "FINANCIAL_DATASETS_API_KEY")
    analysis_data = {}
    graham_analysis = {}

    for ticker in tickers:
        progress.update_status(agent_id, ticker, "Applying Graham's quantitative screens")
        metrics = get_financial_metrics(ticker, end_date, period="annual", limit=5, api_key=api_key)

        financial_line_items = search_line_items(
            ticker,
            [
                "revenue", "net_income", "total_assets", "current_assets",
                "current_liabilities", "total_debt", "cash_and_equivalents",
                "inventory", "accounts_receivable", "shareholders_equity"
            ],
            end_date,
            period="annual",
            limit=5,
            api_key=api_key,
        )

        market_cap = get_market_cap(ticker, end_date, api_key=api_key)

        # Graham's systematic analyses
        net_net_analysis = analyze_net_net_value(financial_line_items, market_cap)
        defensive_criteria = analyze_defensive_investor_criteria(metrics, financial_line_items)
        margin_of_safety = calculate_margin_of_safety(metrics, financial_line_items, market_cap)
        earnings_stability = analyze_earnings_stability(financial_line_items)
        asset_protection = analyze_asset_protection(financial_line_items, market_cap)

        # Graham's systematic scoring
        total_score = (
                net_net_analysis["score"] * 0.3 +
                defensive_criteria["score"] * 0.25 +
                margin_of_safety["score"] * 0.2 +
                earnings_stability["score"] * 0.15 +
                asset_protection["score"] * 0.1
        )

        # Graham's mechanical approach
        if total_score >= 7.0:
            signal = "bullish"
        elif total_score <= 3.0:
            signal = "bearish"
        else:
            signal = "neutral"

        analysis_data[ticker] = {
            "signal": signal,
            "score": total_score,
            "net_net_analysis": net_net_analysis,
            "defensive_criteria": defensive_criteria,
            "margin_of_safety": margin_of_safety,
            "earnings_stability": earnings_stability,
            "asset_protection": asset_protection
        }

        graham_output = generate_graham_output(ticker, analysis_data, state, agent_id)

        graham_analysis[ticker] = {
            "signal": graham_output.signal,
            "confidence": graham_output.confidence,
            "reasoning": graham_output.reasoning
        }

        progress.update_status(agent_id, ticker, "Done", analysis=graham_output.reasoning)

    message = HumanMessage(content=json.dumps(graham_analysis), name=agent_id)

    if state["metadata"]["show_reasoning"]:
        show_agent_reasoning(graham_analysis, "Benjamin Graham Agent")

    state["data"]["analyst_signals"][agent_id] = graham_analysis
    progress.update_status(agent_id, None, "Done")

    return {"messages": [message], "data": state["data"]}


def analyze_net_net_value(financial_line_items: list, market_cap: float) -> dict:
    """Graham's net-net working capital analysis"""
    score = 0
    details = []

    if not financial_line_items or not market_cap:
        return {"score": 0, "details": "No data for net-net analysis"}

    latest = financial_line_items[0]

    # Calculate Net-Net Working Capital
    current_assets = latest.current_assets if latest.current_assets else 0
    current_liabilities = latest.current_liabilities if latest.current_liabilities else 0
    total_debt = latest.total_debt if latest.total_debt else 0

    # Conservative liquidation values
    cash = latest.cash_and_equivalents if latest.cash_and_equivalents else 0
    receivables = (latest.accounts_receivable * 0.85) if latest.accounts_receivable else 0
    inventory = (latest.inventory * 0.5) if latest.inventory else 0

    liquidation_value = cash + receivables + inventory
    net_net_value = liquidation_value - current_liabilities - total_debt

    if net_net_value > 0:
        nnwc_ratio = net_net_value / market_cap
        if nnwc_ratio > 0.5:  # Trading below 50% of liquidation value
            score += 5
            details.append(f"Exceptional net-net: {nnwc_ratio:.1%} of market cap")
        elif nnwc_ratio > 0.2:
            score += 3
            details.append(f"Good net-net value: {nnwc_ratio:.1%} of market cap")
        else:
            score += 1
            details.append(f"Some net-net value: {nnwc_ratio:.1%} of market cap")
    else:
        details.append("No net-net working capital protection")

    return {"score": score, "details": "; ".join(details)}


def analyze_defensive_investor_criteria(metrics: list, financial_line_items: list) -> dict:
    """Graham's 10 criteria for defensive investors"""
    score = 0
    criteria_met = 0
    details = []

    if not metrics or not financial_line_items:
        return {"score": 0, "details": "No data for defensive criteria"}

    latest_metrics = metrics[0]
    latest_financial = financial_line_items[0]

    # 1. Size requirement (large company)
    if latest_financial.revenue and latest_financial.revenue > 100_000_000:  # $100M+ revenue
        criteria_met += 1
        details.append("✓ Adequate size")

    # 2. Strong financial condition (current ratio > 2)
    if latest_metrics.current_ratio and latest_metrics.current_ratio > 2.0:
        criteria_met += 1
        details.append(f"✓ Strong current ratio: {latest_metrics.current_ratio:.1f}")

    # 3. Earnings stability (positive earnings last 10 years)
    earnings_history = [item.net_income for item in financial_line_items if item.net_income]
    if len(earnings_history) >= 5:
        positive_years = sum(1 for e in earnings_history if e > 0)
        if positive_years == len(earnings_history):
            criteria_met += 1
            details.append(f"✓ Earnings stability: {positive_years}/{len(earnings_history)} positive years")

    # 4. Dividend record (consistent dividends)
    # Note: Would need dividend data, assuming if profitable and mature
    if latest_financial.net_income and latest_financial.net_income > 0:
        criteria_met += 0.5  # Partial credit
        details.append("? Dividend assumption based on profitability")

    # 5. Earnings growth (33% increase over 10 years)
    if len(earnings_history) >= 5:
        if earnings_history[0] > earnings_history[-1] * 1.33:
            criteria_met += 1
            details.append("✓ Adequate earnings growth")

    # 6. Moderate P/E ratio
    if latest_metrics.price_to_earnings and latest_metrics.price_to_earnings < 15:
        criteria_met += 1
        details.append(f"✓ Reasonable P/E: {latest_metrics.price_to_earnings:.1f}")

    # 7. Moderate P/B ratio
    if latest_metrics.price_to_book and latest_metrics.price_to_book < 1.5:
        criteria_met += 1
        details.append(f"✓ Conservative P/B: {latest_metrics.price_to_book:.1f}")

    # 8-10. Additional safety criteria
    if latest_metrics.debt_to_equity and latest_metrics.debt_to_equity < 0.5:
        criteria_met += 1
        details.append(f"✓ Low debt: {latest_metrics.debt_to_equity:.2f} D/E")

    score = (criteria_met / 8) * 10  # Normalize to 10-point scale

    return {
        "score": score,
        "criteria_met": criteria_met,
        "details": "; ".join(details)
    }


def calculate_margin_of_safety(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Graham's margin of safety calculation"""
    score = 0
    details = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No data for margin of safety"}

    latest_metrics = metrics[0]
    latest_financial = financial_line_items[0]

    # Book value approach
    if latest_financial.shareholders_equity and latest_financial.shareholders_equity > 0:
        book_value = latest_financial.shareholders_equity
        pb_ratio = market_cap / book_value

        if pb_ratio < 0.67:  # Trading below 2/3 of book value
            score += 4
            details.append(f"Excellent margin of safety: {pb_ratio:.2f} P/B")
        elif pb_ratio < 1.0:
            score += 2
            details.append(f"Good margin of safety: {pb_ratio:.2f} P/B")

    # Earnings approach (conservative P/E)
    if latest_financial.net_income and latest_financial.net_income > 0:
        pe_ratio = market_cap / latest_financial.net_income

        if pe_ratio < 10:
            score += 3
            details.append(f"Conservative earnings multiple: {pe_ratio:.1f} P/E")
        elif pe_ratio < 15:
            score += 1
            details.append(f"Reasonable earnings multiple: {pe_ratio:.1f} P/E")

    # Asset coverage
    if latest_financial.total_assets and latest_financial.total_debt:
        asset_coverage = latest_financial.total_assets / latest_financial.total_debt
        if asset_coverage > 2.0:
            score += 2
            details.append(f"Strong asset coverage: {asset_coverage:.1f}x")

    return {"score": score, "details": "; ".join(details)}


def analyze_earnings_stability(financial_line_items: list) -> dict:
    """Graham emphasized earnings stability over growth"""
    score = 0
    details = []

    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if len(earnings) < 3:
        return {"score": 0, "details": "Insufficient earnings history"}

    # No losses in recent years
    losses = sum(1 for e in earnings if e < 0)
    if losses == 0:
        score += 3
        details.append("No losses in available periods")
    elif losses <= 1:
        score += 1
        details.append(f"Only {losses} loss year")

    # Earnings consistency (low volatility)
    if earnings:
        avg_earnings = sum(earnings) / len(earnings)
        volatility = sum((e - avg_earnings) ** 2 for e in earnings) / len(earnings)
        coefficient_of_variation = (volatility ** 0.5) / abs(avg_earnings) if avg_earnings != 0 else float('inf')

        if coefficient_of_variation < 0.3:
            score += 2
            details.append(f"Low earnings volatility: {coefficient_of_variation:.2f} CV")

    return {"score": score, "details": "; ".join(details)}


def analyze_asset_protection(financial_line_items: list, market_cap: float) -> dict:
    """Graham's focus on asset protection for bondholders and stockholders"""
    score = 0
    details = []

    if not financial_line_items or not market_cap:
        return {"score": 0, "details": "No asset data"}

    latest = financial_line_items[0]

    # Working capital adequacy
    if latest.current_assets and latest.current_liabilities:
        working_capital = latest.current_assets - latest.current_liabilities
        if working_capital > 0:
            wc_ratio = working_capital / market_cap
            if wc_ratio > 0.3:
                score += 2
                details.append(f"Strong working capital: {wc_ratio:.1%} of market cap")

    # Debt coverage
    if latest.total_assets and latest.total_debt:
        if latest.total_debt == 0:
            score += 3
            details.append("Debt-free company")
        else:
            debt_ratio = latest.total_debt / latest.total_assets
            if debt_ratio < 0.3:
                score += 2
                details.append(f"Conservative debt: {debt_ratio:.1%} of assets")

    # Tangible book value
    if latest.shareholders_equity:
        tangible_equity = latest.shareholders_equity  # Simplified
        if tangible_equity > market_cap * 0.5:
            score += 2
            details.append("Trading below tangible book value")

    return {"score": score, "details": "; ".join(details)}


def generate_graham_output(ticker: str, analysis_data: dict, state: AgentState, agent_id: str) -> GrahamSignal:
    """Generate Graham-style systematic investment decision"""
    template = ChatPromptTemplate.from_messages([
        ("system", """You are Benjamin Graham's AI agent, the father of value investing. Follow his systematic approach:

        1. Mathematical and quantitative analysis over speculation
        2. Margin of safety: "Price is what you pay, value is what you get"
        3. Net-net working capital for maximum safety
        4. Defensive investor criteria: 10-point checklist
        5. Asset protection: Focus on balance sheet strength
        6. Earnings stability over growth
        7. Systematic approach: Remove emotion from investing
        8. Statistical cheapness: Buy groups of undervalued securities

        Reasoning style:
        - Emphasize quantitative metrics and ratios
        - Focus on asset protection and balance sheet
        - Stress margin of safety in every decision
        - Use systematic, unemotional language
        - Prefer statistical evidence over narratives
        - Always consider downside protection first

        Return bullish only for statistically cheap, safe companies with adequate margin of safety."""),

        ("human", """Apply Graham's systematic analysis to {ticker}:

        {analysis_data}

        Provide investment signal in JSON format:
        {{
          "signal": "bullish" | "bearish" | "neutral",
          "confidence": float (0-100),
          "reasoning": "string"
        }}""")
    ])

    prompt = template.invoke({"analysis_data": json.dumps(analysis_data, indent=2), "ticker": ticker})

    def create_default_graham_signal():
        return GrahamSignal(signal="neutral", confidence=0.0, reasoning="Analysis error, defaulting to neutral")

    return call_llm(
        prompt=prompt,
        pydantic_model=GrahamSignal,
        agent_name=agent_id,
        state=state,
        default_factory=create_default_graham_signal,
    )