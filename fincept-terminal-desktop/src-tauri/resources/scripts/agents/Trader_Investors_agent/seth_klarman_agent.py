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


class KlarmanSignal(BaseModel):
    signal: Literal["bullish", "bearish", "neutral"]
    confidence: float
    reasoning: str


def seth_klarman_agent(state: AgentState, agent_id: str = "seth_klarman_agent"):
    """
    Seth Klarman: Absolute return, risk-first approach, contrarian
    Focus: Capital preservation, asymmetric risk/reward, distressed opportunities
    """
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    api_key = get_api_key_from_state(state, "FINANCIAL_DATASETS_API_KEY")
    analysis_data = {}
    klarman_analysis = {}

    for ticker in tickers:
        progress.update_status(agent_id, ticker, "Analyzing downside protection")
        metrics = get_financial_metrics(ticker, end_date, period="annual", limit=5, api_key=api_key)

        financial_line_items = search_line_items(
            ticker,
            [
                "revenue", "net_income", "total_assets", "current_assets",
                "current_liabilities", "total_debt", "cash_and_equivalents",
                "shareholders_equity", "free_cash_flow", "operating_income"
            ],
            end_date,
            period="annual",
            limit=5,
            api_key=api_key,
        )

        market_cap = get_market_cap(ticker, end_date, api_key=api_key)

        # Klarman's risk-first analyses
        downside_protection = analyze_downside_protection(financial_line_items, market_cap)
        asymmetric_opportunity = analyze_asymmetric_risk_reward(metrics, financial_line_items, market_cap)
        contrarian_indicators = analyze_contrarian_opportunity(metrics, financial_line_items)
        balance_sheet_strength = analyze_balance_sheet_fortress(financial_line_items)
        margin_of_safety = calculate_klarman_margin_of_safety(financial_line_items, market_cap)

        # Klarman's conservative scoring (risk-adjusted)
        total_score = (
                downside_protection["score"] * 0.3 +
                balance_sheet_strength["score"] * 0.25 +
                margin_of_safety["score"] * 0.2 +
                asymmetric_opportunity["score"] * 0.15 +
                contrarian_indicators["score"] * 0.1
        )

        # Klarman's high bar for investment
        if total_score >= 8.0:
            signal = "bullish"
        elif total_score <= 4.0:
            signal = "bearish"
        else:
            signal = "neutral"  # Most opportunities don't meet Klarman's standards

        analysis_data[ticker] = {
            "signal": signal,
            "score": total_score,
            "downside_protection": downside_protection,
            "asymmetric_opportunity": asymmetric_opportunity,
            "contrarian_indicators": contrarian_indicators,
            "balance_sheet_strength": balance_sheet_strength,
            "margin_of_safety": margin_of_safety
        }

        klarman_output = generate_klarman_output(ticker, analysis_data, state, agent_id)

        klarman_analysis[ticker] = {
            "signal": klarman_output.signal,
            "confidence": klarman_output.confidence,
            "reasoning": klarman_output.reasoning
        }

        progress.update_status(agent_id, ticker, "Done", analysis=klarman_output.reasoning)

    message = HumanMessage(content=json.dumps(klarman_analysis), name=agent_id)

    if state["metadata"]["show_reasoning"]:
        show_agent_reasoning(klarman_analysis, "Seth Klarman Agent")

    state["data"]["analyst_signals"][agent_id] = klarman_analysis
    progress.update_status(agent_id, None, "Done")

    return {"messages": [message], "data": state["data"]}


def analyze_downside_protection(financial_line_items: list, market_cap: float) -> dict:
    """Klarman's obsession with downside protection"""
    score = 0
    details = []

    if not financial_line_items or not market_cap:
        return {"score": 0, "details": "No data for downside analysis"}

    latest = financial_line_items[0]

    # Asset coverage
    total_assets = latest.total_assets if latest.total_assets else 0
    total_debt = latest.total_debt if latest.total_debt else 0

    if total_assets > 0:
        asset_coverage = total_assets / market_cap
        if asset_coverage > 2.0:
            score += 3
            details.append(f"Strong asset coverage: {asset_coverage:.1f}x market cap")
        elif asset_coverage > 1.5:
            score += 2
            details.append(f"Adequate asset coverage: {asset_coverage:.1f}x market cap")

    # Liquidation value protection
    current_assets = latest.current_assets if latest.current_assets else 0
    current_liabilities = latest.current_liabilities if latest.current_liabilities else 0

    # Conservative liquidation values
    cash = latest.cash_and_equivalents if latest.cash_and_equivalents else 0
    liquidation_value = cash + (current_assets - cash) * 0.7  # 70% recovery on non-cash assets
    net_liquidation = liquidation_value - current_liabilities - total_debt

    if net_liquidation > 0:
        liquidation_ratio = net_liquidation / market_cap
        if liquidation_ratio > 0.5:
            score += 4
            details.append(f"Exceptional liquidation protection: {liquidation_ratio:.1%}")
        elif liquidation_ratio > 0.2:
            score += 2
            details.append(f"Good liquidation value: {liquidation_ratio:.1%}")

    # Earnings floor
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings:
        worst_earnings = min(earnings)
        if worst_earnings > 0:
            score += 2
            details.append("No losses in available periods")
        elif worst_earnings > -market_cap * 0.05:  # Small losses relative to market cap
            score += 1
            details.append("Limited downside from worst historical performance")

    return {"score": score, "details": "; ".join(details)}


def analyze_asymmetric_risk_reward(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Klarman's focus on asymmetric opportunities"""
    score = 0
    details = []

    if not financial_line_items or not market_cap:
        return {"score": 0, "details": "No data for asymmetric analysis"}

    latest = financial_line_items[0]

    # Upside potential vs downside risk
    # Calculate normalized earnings power
    earnings = [item.net_income for item in financial_line_items if item.net_income and item.net_income > 0]
    if earnings:
        avg_earnings = sum(earnings) / len(earnings)
        normalized_pe = market_cap / avg_earnings if avg_earnings > 0 else 0

        # If trading at low multiple with asset protection, asymmetric opportunity
        if normalized_pe < 12 and latest.total_assets and latest.total_assets > market_cap:
            score += 3
            details.append(f"Asymmetric setup: {normalized_pe:.1f} P/E with asset backing")

    # Free cash flow yield vs risk
    if latest.free_cash_flow and latest.free_cash_flow > 0:
        fcf_yield = latest.free_cash_flow / market_cap
        debt_ratio = (latest.total_debt / latest.total_assets) if latest.total_assets else 1

        # High FCF yield with low debt = good risk/reward
        if fcf_yield > 0.1 and debt_ratio < 0.3:
            score += 2
            details.append(f"Strong FCF yield ({fcf_yield:.1%}) with low debt")

    # Catalyst potential (simplified)
    # Look for companies with improving trends that market hasn't recognized
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 3:
        recent_growth = (revenues[0] / revenues[1]) - 1
        if recent_growth > 0.1:  # 10%+ recent growth
            score += 1
            details.append(f"Potential catalyst: {recent_growth:.1%} recent revenue growth")

    return {"score": score, "details": "; ".join(details)}


def analyze_contrarian_opportunity(metrics: list, financial_line_items: list) -> dict:
    """Klarman's contrarian approach to investing"""
    score = 0
    details = []

    if not metrics or not financial_line_items:
        return {"score": 0, "details": "No data for contrarian analysis"}

    latest_metrics = metrics[0]

    # Low valuation multiples (contrarian indicator)
    if latest_metrics.price_to_book and latest_metrics.price_to_book < 1.0:
        score += 2
        details.append(f"Contrarian P/B: {latest_metrics.price_to_book:.2f}")

    if latest_metrics.price_to_earnings and latest_metrics.price_to_earnings < 10:
        score += 2
        details.append(f"Contrarian P/E: {latest_metrics.price_to_earnings:.1f}")

    # Temporary earnings depression
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if len(earnings) >= 3:
        recent_earnings = earnings[0]
        historical_avg = sum(earnings[1:]) / len(earnings[1:])

        if recent_earnings < historical_avg * 0.7 and recent_earnings > 0:
            score += 2
            details.append("Potential temporary earnings depression")

    # Industry or company-specific distress signals
    # (Would typically require more external data, simplified here)
    if latest_metrics.current_ratio and latest_metrics.current_ratio < 1.2:
        # Low current ratio might indicate distress (opportunity for contrarian)
        score += 1
        details.append("Potential distress situation for contrarian investment")

    return {"score": score, "details": "; ".join(details)}


def analyze_balance_sheet_fortress(financial_line_items: list) -> dict:
    """Klarman's requirement for fortress balance sheets"""
    score = 0
    details = []

    if not financial_line_items:
        return {"score": 0, "details": "No balance sheet data"}

    latest = financial_line_items[0]

    # Cash and liquidity
    cash = latest.cash_and_equivalents if latest.cash_and_equivalents else 0
    current_assets = latest.current_assets if latest.current_assets else 0
    current_liabilities = latest.current_liabilities if latest.current_liabilities else 0

    if current_liabilities > 0:
        current_ratio = current_assets / current_liabilities
        if current_ratio > 2.0:
            score += 2
            details.append(f"Strong liquidity: {current_ratio:.1f} current ratio")

    # Debt levels
    total_debt = latest.total_debt if latest.total_debt else 0
    shareholders_equity = latest.shareholders_equity if latest.shareholders_equity else 0

    if shareholders_equity > 0:
        debt_to_equity = total_debt / shareholders_equity
        if debt_to_equity < 0.3:
            score += 3
            details.append(f"Conservative debt: {debt_to_equity:.2f} D/E")
        elif debt_to_equity < 0.5:
            score += 1
            details.append(f"Moderate debt: {debt_to_equity:.2f} D/E")

    # Working capital strength
    working_capital = current_assets - current_liabilities
    if working_capital > 0 and latest.total_assets:
        wc_ratio = working_capital / latest.total_assets
        if wc_ratio > 0.2:
            score += 2
            details.append(f"Strong working capital: {wc_ratio:.1%} of assets")

    # Interest coverage
    if latest.operating_income and total_debt > 0:
        # Simplified interest coverage (assume 5% interest rate)
        estimated_interest = total_debt * 0.05
        if latest.operating_income > estimated_interest * 5:
            score += 2
            details.append("Strong interest coverage capability")

    return {"score": score, "details": "; ".join(details)}


def calculate_klarman_margin_of_safety(financial_line_items: list, market_cap: float) -> dict:
    """Klarman's conservative margin of safety calculation"""
    score = 0
    details = []

    if not financial_line_items or not market_cap:
        return {"score": 0, "details": "No data for margin of safety"}

    latest = financial_line_items[0]

    # Conservative asset-based valuation
    total_assets = latest.total_assets if latest.total_assets else 0
    total_debt = latest.total_debt if latest.total_debt else 0

    # Apply conservative haircuts to assets
    conservative_asset_value = total_assets * 0.7  # 30% haircut
    net_asset_value = conservative_asset_value - total_debt

    if net_asset_value > 0:
        asset_margin = (net_asset_value - market_cap) / market_cap
        if asset_margin > 0.5:
            score += 3
            details.append(f"Excellent asset margin of safety: {asset_margin:.1%}")
        elif asset_margin > 0.2:
            score += 2
            details.append(f"Good asset margin of safety: {asset_margin:.1%}")

    # Conservative earnings-based valuation
    earnings = [item.net_income for item in financial_line_items if item.net_income and item.net_income > 0]
    if earnings:
        # Use conservative multiple (10x) on average earnings
        avg_earnings = sum(earnings) / len(earnings)
        conservative_earnings_value = avg_earnings * 10

        earnings_margin = (conservative_earnings_value - market_cap) / market_cap
        if earnings_margin > 0.3:
            score += 2
            details.append(f"Earnings margin of safety: {earnings_margin:.1%}")

    # Free cash flow margin of safety
    if latest.free_cash_flow and latest.free_cash_flow > 0:
        # Conservative 12x FCF multiple
        fcf_value = latest.free_cash_flow * 12
        fcf_margin = (fcf_value - market_cap) / market_cap

        if fcf_margin > 0.25:
            score += 2
            details.append(f"FCF margin of safety: {fcf_margin:.1%}")

    return {"score": score, "details": "; ".join(details)}


def generate_klarman_output(ticker: str, analysis_data: dict, state: AgentState, agent_id: str) -> KlarmanSignal:
    """Generate Klarman-style risk-first investment decision"""
    template = ChatPromptTemplate.from_messages([
        ("system", """You are Seth Klarman's AI agent, following absolute return value investing principles:

        1. Risk first: "Return OF capital is more important than return ON capital"
        2. Margin of safety: Buy with significant downside protection
        3. Contrarian approach: Buy when others are selling
        4. Asymmetric risk/reward: Limited downside, substantial upside
        5. Balance sheet fortress: Strong financial position required
        6. Patient opportunism: Wait for exceptional opportunities
        7. Absolute return focus: Preserve capital above all else

        Reasoning style:
        - Emphasize downside protection and risk analysis first
        - Focus on balance sheet strength and asset coverage
        - Discuss margin of safety quantitatively
        - Consider contrarian and distressed opportunities
        - Express skepticism and conservatism
        - Require exceptional risk/reward ratios
        - Acknowledge when opportunities don't meet standards

        Return bullish only for exceptional risk/reward opportunities with substantial downside protection."""),

        ("human", """Apply risk-first value analysis to {ticker}:

        {analysis_data}

        Provide investment signal in JSON format:
        {{
          "signal": "bullish" | "bearish" | "neutral",
          "confidence": float (0-100),
          "reasoning": "string"
        }}""")
    ])

    prompt = template.invoke({"analysis_data": json.dumps(analysis_data, indent=2), "ticker": ticker})

    def create_default_klarman_signal():
        return KlarmanSignal(signal="neutral", confidence=0.0, reasoning="Analysis error, defaulting to neutral")

    return call_llm(
        prompt=prompt,
        pydantic_model=KlarmanSignal,
        agent_name=agent_id,
        state=state,
        default_factory=create_default_klarman_signal,
    )