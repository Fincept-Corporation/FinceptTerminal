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


class WhitmanSignal(BaseModel):
    signal: Literal["bullish", "bearish", "neutral"]
    confidence: float
    reasoning: str


def marty_whitman_agent(state: AgentState, agent_id: str = "marty_whitman_agent"):
    """
    Marty Whitman: Safe & cheap, asset-based investing
    Focus: Balance sheet analysis, asset values, financial safety
    """
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    api_key = get_api_key_from_state(state, "FINANCIAL_DATASETS_API_KEY")
    analysis_data = {}
    whitman_analysis = {}

    for ticker in tickers:
        progress.update_status(agent_id, ticker, "Analyzing safe & cheap criteria")
        metrics = get_financial_metrics(ticker, end_date, period="annual", limit=5, api_key=api_key)

        financial_line_items = search_line_items(
            ticker,
            [
                "revenue", "net_income", "total_assets", "current_assets",
                "current_liabilities", "total_debt", "shareholders_equity",
                "cash_and_equivalents", "accounts_receivable", "inventory",
                "property_plant_equipment", "goodwill"
            ],
            end_date,
            period="annual",
            limit=5,
            api_key=api_key,
        )

        market_cap = get_market_cap(ticker, end_date, api_key=api_key)

        # Whitman's asset-focused framework
        asset_value_analysis = analyze_asset_values(financial_line_items, market_cap)
        financial_safety = analyze_financial_safety(financial_line_items)
        balance_sheet_strength = analyze_balance_sheet_quality(financial_line_items)
        safe_and_cheap = evaluate_safe_and_cheap_criteria(metrics, financial_line_items, market_cap)
        credit_worthiness = analyze_credit_worthiness(financial_line_items)

        # Whitman's asset-based scoring
        total_score = (
                asset_value_analysis["score"] * 0.3 +
                financial_safety["score"] * 0.25 +
                safe_and_cheap["score"] * 0.2 +
                balance_sheet_strength["score"] * 0.15 +
                credit_worthiness["score"] * 0.1
        )

        # Whitman's safety-first approach
        if total_score >= 8.0:
            signal = "bullish"
        elif total_score <= 4.5:
            signal = "bearish"
        else:
            signal = "neutral"

        analysis_data[ticker] = {
            "signal": signal,
            "score": total_score,
            "asset_value_analysis": asset_value_analysis,
            "financial_safety": financial_safety,
            "balance_sheet_strength": balance_sheet_strength,
            "safe_and_cheap": safe_and_cheap,
            "credit_worthiness": credit_worthiness
        }

        whitman_output = generate_whitman_output(ticker, analysis_data, state, agent_id)

        whitman_analysis[ticker] = {
            "signal": whitman_output.signal,
            "confidence": whitman_output.confidence,
            "reasoning": whitman_output.reasoning
        }

        progress.update_status(agent_id, ticker, "Done", analysis=whitman_output.reasoning)

    message = HumanMessage(content=json.dumps(whitman_analysis), name=agent_id)

    if state["metadata"]["show_reasoning"]:
        show_agent_reasoning(whitman_analysis, "Marty Whitman Agent")

    state["data"]["analyst_signals"][agent_id] = whitman_analysis
    progress.update_status(agent_id, None, "Done")

    return {"messages": [message], "data": state["data"]}


def analyze_asset_values(financial_line_items: list, market_cap: float) -> dict:
    """Whitman's asset-based valuation analysis"""
    score = 0
    details = []

    if not financial_line_items or not market_cap:
        return {"score": 0, "details": "No asset data"}

    latest = financial_line_items[0]

    # Tangible asset value
    total_assets = latest.total_assets if latest.total_assets else 0
    goodwill = latest.goodwill if latest.goodwill else 0
    tangible_assets = total_assets - goodwill
    total_debt = latest.total_debt if latest.total_debt else 0

    if tangible_assets > 0:
        net_tangible_assets = tangible_assets - total_debt
        if net_tangible_assets > market_cap:
            score += 4
            details.append(f"Trading below tangible assets: {(net_tangible_assets / market_cap):.1f}x")
        elif net_tangible_assets > market_cap * 0.8:
            score += 3
            details.append(f"Strong tangible asset backing: {(net_tangible_assets / market_cap):.1f}x")
        elif net_tangible_assets > market_cap * 0.6:
            score += 2
            details.append(f"Adequate tangible asset backing: {(net_tangible_assets / market_cap):.1f}x")

    # Working capital value
    current_assets = latest.current_assets if latest.current_assets else 0
    current_liabilities = latest.current_liabilities if latest.current_liabilities else 0
    working_capital = current_assets - current_liabilities

    if working_capital > 0:
        wc_ratio = working_capital / market_cap
        if wc_ratio > 0.5:
            score += 2
            details.append(f"Strong working capital: {wc_ratio:.1%} of market cap")
        elif wc_ratio > 0.2:
            score += 1
            details.append(f"Positive working capital: {wc_ratio:.1%} of market cap")

    # Book value relationship
    shareholders_equity = latest.shareholders_equity if latest.shareholders_equity else 0
    if shareholders_equity > 0:
        price_to_book = market_cap / shareholders_equity
        if price_to_book < 0.8:
            score += 2
            details.append(f"Trading below 80% of book: {price_to_book:.2f} P/B")
        elif price_to_book < 1.2:
            score += 1
            details.append(f"Reasonable P/B ratio: {price_to_book:.2f}")

    return {"score": score, "details": "; ".join(details)}


def analyze_financial_safety(financial_line_items: list) -> dict:
    """Whitman's financial safety analysis"""
    score = 0
    details = []

    if not financial_line_items:
        return {"score": 0, "details": "No safety data"}

    latest = financial_line_items[0]

    # Debt safety
    total_debt = latest.total_debt if latest.total_debt else 0
    shareholders_equity = latest.shareholders_equity if latest.shareholders_equity else 0

    if shareholders_equity > 0:
        debt_to_equity = total_debt / shareholders_equity
        if debt_to_equity < 0.3:
            score += 3
            details.append(f"Very safe debt level: {debt_to_equity:.2f} D/E")
        elif debt_to_equity < 0.6:
            score += 2
            details.append(f"Safe debt level: {debt_to_equity:.2f} D/E")
        elif debt_to_equity > 1.5:
            score -= 1
            details.append(f"High debt concern: {debt_to_equity:.2f} D/E")

    # Liquidity safety
    current_assets = latest.current_assets if latest.current_assets else 0
    current_liabilities = latest.current_liabilities if latest.current_liabilities else 0

    if current_liabilities > 0:
        current_ratio = current_assets / current_liabilities
        if current_ratio > 2.0:
            score += 2
            details.append(f"Strong liquidity: {current_ratio:.1f} current ratio")
        elif current_ratio > 1.5:
            score += 1
            details.append(f"Adequate liquidity: {current_ratio:.1f} current ratio")
        elif current_ratio < 1.0:
            score -= 2
            details.append(f"Liquidity concern: {current_ratio:.1f} current ratio")

    # Cash position
    cash = latest.cash_and_equivalents if latest.cash_and_equivalents else 0
    total_assets = latest.total_assets if latest.total_assets else 0

    if total_assets > 0:
        cash_ratio = cash / total_assets
        if cash_ratio > 0.15:  # 15%+ cash
            score += 2
            details.append(f"Strong cash position: {cash_ratio:.1%} of assets")
        elif cash_ratio > 0.05:
            score += 1
            details.append(f"Reasonable cash position: {cash_ratio:.1%} of assets")

    return {"score": score, "details": "; ".join(details)}


def analyze_balance_sheet_quality(financial_line_items: list) -> dict:
    """Whitman's balance sheet quality assessment"""
    score = 0
    details = []

    if not financial_line_items:
        return {"score": 0, "details": "No balance sheet data"}

    latest = financial_line_items[0]

    # Asset quality analysis
    current_assets = latest.current_assets if latest.current_assets else 0
    cash = latest.cash_and_equivalents if latest.cash_and_equivalents else 0
    receivables = latest.accounts_receivable if latest.accounts_receivable else 0
    inventory = latest.inventory if latest.inventory else 0

    # High-quality current assets
    if current_assets > 0:
        quality_assets = cash + receivables  # Cash + receivables are higher quality
        quality_ratio = quality_assets / current_assets
        if quality_ratio > 0.7:
            score += 2
            details.append(f"High-quality current assets: {quality_ratio:.1%} cash+receivables")
        elif quality_ratio > 0.5:
            score += 1
            details.append(f"Reasonable asset quality: {quality_ratio:.1%} cash+receivables")

    # Fixed asset efficiency
    ppe = latest.property_plant_equipment if latest.property_plant_equipment else 0
    revenue = latest.revenue if latest.revenue else 0

    if ppe > 0 and revenue > 0:
        asset_turnover = revenue / ppe
        if asset_turnover > 3.0:  # Efficient use of fixed assets
            score += 1
            details.append(f"Efficient fixed asset use: {asset_turnover:.1f}x turnover")

    # Goodwill concerns
    goodwill = latest.goodwill if latest.goodwill else 0
    total_assets = latest.total_assets if latest.total_assets else 0

    if total_assets > 0:
        goodwill_ratio = goodwill / total_assets
        if goodwill_ratio < 0.1:  # Low goodwill
            score += 2
            details.append(f"Low goodwill risk: {goodwill_ratio:.1%} of assets")
        elif goodwill_ratio > 0.3:  # High goodwill
            score -= 1
            details.append(f"High goodwill concern: {goodwill_ratio:.1%} of assets")

    # Equity growth
    equity_vals = [item.shareholders_equity for item in financial_line_items if item.shareholders_equity]
    if len(equity_vals) >= 3:
        equity_growth = (equity_vals[0] / equity_vals[-1]) ** (1 / len(equity_vals)) - 1
        if equity_growth > 0.08:  # 8%+ equity growth
            score += 1
            details.append(f"Growing equity base: {equity_growth:.1%} CAGR")

    return {"score": score, "details": "; ".join(details)}


def evaluate_safe_and_cheap_criteria(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Whitman's 'safe and cheap' evaluation"""
    score = 0
    details = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No safe & cheap data"}

    latest_metrics = metrics[0]
    latest_financial = financial_line_items[0]

    # "Safe" criteria
    safety_score = 0

    # Low debt
    if latest_metrics.debt_to_equity and latest_metrics.debt_to_equity < 0.5:
        safety_score += 1
        details.append(f"Safe debt level: {latest_metrics.debt_to_equity:.2f} D/E")

    # Strong balance sheet
    if latest_metrics.current_ratio and latest_metrics.current_ratio > 1.5:
        safety_score += 1
        details.append(f"Safe liquidity: {latest_metrics.current_ratio:.1f} current ratio")

    # Asset backing
    if latest_financial.total_assets and latest_financial.total_debt:
        asset_coverage = latest_financial.total_assets / latest_financial.total_debt
        if asset_coverage > 3.0:
            safety_score += 1
            details.append(f"Safe asset coverage: {asset_coverage:.1f}x")

    # "Cheap" criteria
    cheap_score = 0

    # Low P/B
    if latest_metrics.price_to_book and latest_metrics.price_to_book < 1.5:
        cheap_score += 1
        details.append(f"Cheap P/B: {latest_metrics.price_to_book:.2f}")

    # Low P/E
    if latest_metrics.price_to_earnings and latest_metrics.price_to_earnings < 15:
        cheap_score += 1
        details.append(f"Cheap P/E: {latest_metrics.price_to_earnings:.1f}")

    # Asset value
    if latest_financial.shareholders_equity and latest_financial.shareholders_equity > market_cap:
        cheap_score += 1
        details.append("Cheap relative to book value")

    # Combined safe & cheap score
    if safety_score >= 2 and cheap_score >= 2:
        score = 10  # Perfect safe & cheap
        details.append("Meets Whitman's safe & cheap criteria")
    elif safety_score >= 2:
        score = 6  # Safe but not cheap
        details.append("Safe but not particularly cheap")
    elif cheap_score >= 2:
        score = 4  # Cheap but not safe
        details.append("Cheap but safety concerns")
    else:
        score = 2  # Neither safe nor cheap
        details.append("Does not meet safe & cheap criteria")

    return {"score": score, "details": "; ".join(details)}


def analyze_credit_worthiness(financial_line_items: list) -> dict:
    """Whitman's credit analysis approach"""
    score = 0
    details = []

    if not financial_line_items:
        return {"score": 0, "details": "No credit data"}

    latest = financial_line_items[0]

    # Interest coverage capability
    operating_income = latest.net_income if latest.net_income else 0  # Simplified
    total_debt = latest.total_debt if latest.total_debt else 0

    if total_debt > 0:
        # Estimate interest expense at 5%
        estimated_interest = total_debt * 0.05
        if operating_income > estimated_interest * 3:  # 3x coverage
            score += 2
            details.append("Strong estimated interest coverage")
        elif operating_income > estimated_interest:
            score += 1
            details.append("Adequate estimated interest coverage")
    elif total_debt == 0:
        score += 3
        details.append("No debt - excellent credit position")

    # Cash flow coverage
    free_cash_flow = latest.free_cash_flow if latest.free_cash_flow else 0
    if free_cash_flow > 0 and total_debt > 0:
        debt_paydown_years = total_debt / free_cash_flow
        if debt_paydown_years < 5:
            score += 2
            details.append(f"Can pay down debt in {debt_paydown_years:.1f} years")
        elif debt_paydown_years < 10:
            score += 1
            details.append(f"Reasonable debt paydown period: {debt_paydown_years:.1f} years")

    # Working capital strength
    current_assets = latest.current_assets if latest.current_assets else 0
    current_liabilities = latest.current_liabilities if latest.current_liabilities else 0

    if current_liabilities > 0:
        working_capital_ratio = current_assets / current_liabilities
        if working_capital_ratio > 2.0:
            score += 1
            details.append(f"Strong working capital for credit: {working_capital_ratio:.1f}x")

    return {"score": score, "details": "; ".join(details)}


def generate_whitman_output(ticker: str, analysis_data: dict, state: AgentState, agent_id: str) -> WhitmanSignal:
    """Generate Whitman-style safe & cheap investment decision"""
    template = ChatPromptTemplate.from_messages([
        ("system", """You are Marty Whitman's AI agent, applying safe & cheap asset-based investing:

        1. Safe & Cheap: Both criteria must be met - financial safety AND attractive valuation
        2. Asset-based analysis: Focus on balance sheet values and asset backing
        3. Financial safety: Low debt, strong liquidity, conservative capital structure
        4. Credit analysis: Evaluate creditworthiness and financial flexibility
        5. Tangible value: Prefer tangible assets over intangibles and goodwill
        6. Balance sheet focus: Income statement is secondary to balance sheet strength
        7. Risk management: Downside protection through asset values

        Reasoning style:
        - Emphasize balance sheet analysis and asset values
        - Focus on financial safety and conservative capital structure
        - Discuss tangible asset backing and liquidation values
        - Consider credit worthiness and debt capacity
        - Apply rigorous safe & cheap criteria
        - Express preference for asset-rich, debt-light companies
        - Acknowledge when safety or cheapness criteria are not met

        Return bullish only for companies that are both financially safe AND attractively cheap."""),

        ("human", """Apply safe & cheap asset-based analysis to {ticker}:

        {analysis_data}

        Provide investment signal in JSON format:
        {{
          "signal": "bullish" | "bearish" | "neutral",
          "confidence": float (0-100),
          "reasoning": "string"
        }}""")
    ])

    prompt = template.invoke({"analysis_data": json.dumps(analysis_data, indent=2), "ticker": ticker})

    def create_default_whitman_signal():
        return WhitmanSignal(signal="neutral", confidence=0.0, reasoning="Analysis error, defaulting to neutral")

    return call_llm(
        prompt=prompt,
        pydantic_model=WhitmanSignal,
        agent_name=agent_id,
        state=state,
        default_factory=create_default_whitman_signal,
    )