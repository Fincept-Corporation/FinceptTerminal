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


class EveillardSignal(BaseModel):
    signal: Literal["bullish", "bearish", "neutral"]
    confidence: float
    reasoning: str


def jean_marie_eveillard_agent(state: AgentState, agent_id: str = "jean_marie_eveillard_agent"):
    """
    Jean-Marie Eveillard: Global value, capital preservation
    Focus: Downside protection, global diversification, conservative value
    """
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    api_key = get_api_key_from_state(state, "FINANCIAL_DATASETS_API_KEY")
    analysis_data = {}
    eveillard_analysis = {}

    for ticker in tickers:
        progress.update_status(agent_id, ticker, "Analyzing capital preservation potential")
        metrics = get_financial_metrics(ticker, end_date, period="annual", limit=6, api_key=api_key)

        financial_line_items = search_line_items(
            ticker,
            [
                "revenue", "net_income", "free_cash_flow", "total_assets",
                "current_assets", "current_liabilities", "total_debt",
                "shareholders_equity", "cash_and_equivalents"
            ],
            end_date,
            period="annual",
            limit=6,
            api_key=api_key,
        )

        market_cap = get_market_cap(ticker, end_date, api_key=api_key)

        # Eveillard's conservative framework
        capital_preservation = analyze_capital_preservation(financial_line_items, market_cap)
        balance_sheet_quality = analyze_conservative_balance_sheet(financial_line_items)
        valuation_safety = analyze_valuation_margin_of_safety(metrics, financial_line_items, market_cap)
        business_predictability = analyze_business_predictability(financial_line_items)
        downside_protection = calculate_downside_protection(financial_line_items, market_cap)

        # Eveillard's conservative scoring
        total_score = (
                capital_preservation["score"] * 0.3 +
                balance_sheet_quality["score"] * 0.25 +
                downside_protection["score"] * 0.2 +
                valuation_safety["score"] * 0.15 +
                business_predictability["score"] * 0.1
        )

        # Eveillard's very conservative approach
        if total_score >= 8.5:
            signal = "bullish"
        elif total_score <= 5.0:
            signal = "bearish"
        else:
            signal = "neutral"  # Often neutral - very selective

        analysis_data[ticker] = {
            "signal": signal,
            "score": total_score,
            "capital_preservation": capital_preservation,
            "balance_sheet_quality": balance_sheet_quality,
            "valuation_safety": valuation_safety,
            "business_predictability": business_predictability,
            "downside_protection": downside_protection
        }

        eveillard_output = generate_eveillard_output(ticker, analysis_data, state, agent_id)

        eveillard_analysis[ticker] = {
            "signal": eveillard_output.signal,
            "confidence": eveillard_output.confidence,
            "reasoning": eveillard_output.reasoning
        }

        progress.update_status(agent_id, ticker, "Done", analysis=eveillard_output.reasoning)

    message = HumanMessage(content=json.dumps(eveillard_analysis), name=agent_id)

    if state["metadata"]["show_reasoning"]:
        show_agent_reasoning(eveillard_analysis, "Jean-Marie Eveillard Agent")

    state["data"]["analyst_signals"][agent_id] = eveillard_analysis
    progress.update_status(agent_id, None, "Done")

    return {"messages": [message], "data": state["data"]}


def analyze_capital_preservation(financial_line_items: list, market_cap: float) -> dict:
    """Eveillard's capital preservation analysis"""
    score = 0
    details = []

    if not financial_line_items or not market_cap:
        return {"score": 0, "details": "No capital preservation data"}

    latest = financial_line_items[0]

    # Strong cash position
    cash = latest.cash_and_equivalents if latest.cash_and_equivalents else 0
    if cash > market_cap * 0.1:  # 10%+ cash relative to market cap
        score += 2
        details.append(f"Strong cash position: {(cash / market_cap):.1%} of market cap")

    # Asset backing
    if latest.total_assets and latest.total_debt:
        net_assets = latest.total_assets - latest.total_debt
        if net_assets > market_cap:
            score += 3
            details.append(f"Strong asset backing: {(net_assets / market_cap):.1f}x market cap")
        elif net_assets > market_cap * 0.7:
            score += 2
            details.append(f"Adequate asset backing: {(net_assets / market_cap):.1f}x market cap")

    # Working capital strength
    if latest.current_assets and latest.current_liabilities:
        working_capital = latest.current_assets - latest.current_liabilities
        if working_capital > market_cap * 0.2:
            score += 2
            details.append(f"Strong working capital: {(working_capital / market_cap):.1%} of market cap")

    # Dividend history (capital return)
    # Note: Would need dividend data, using profitability as proxy
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings:
        positive_years = sum(1 for e in earnings if e > 0)
        if positive_years == len(earnings):
            score += 1
            details.append(f"Consistent profitability: {positive_years}/{len(earnings)} years")

    return {"score": score, "details": "; ".join(details)}


def analyze_conservative_balance_sheet(financial_line_items: list) -> dict:
    """Eveillard's conservative balance sheet requirements"""
    score = 0
    details = []

    if not financial_line_items:
        return {"score": 0, "details": "No balance sheet data"}

    latest = financial_line_items[0]

    # Low debt levels
    if latest.total_debt and latest.shareholders_equity:
        debt_to_equity = latest.total_debt / latest.shareholders_equity
        if debt_to_equity < 0.2:  # Very conservative
            score += 3
            details.append(f"Very low debt: {debt_to_equity:.2f} D/E")
        elif debt_to_equity < 0.4:
            score += 2
            details.append(f"Conservative debt: {debt_to_equity:.2f} D/E")
        elif debt_to_equity > 1.0:
            score -= 1
            details.append(f"High debt concern: {debt_to_equity:.2f} D/E")

    # Strong liquidity
    if latest.current_assets and latest.current_liabilities:
        current_ratio = latest.current_assets / latest.current_liabilities
        if current_ratio > 2.0:
            score += 2
            details.append(f"Strong liquidity: {current_ratio:.1f} current ratio")
        elif current_ratio < 1.2:
            score -= 1
            details.append(f"Liquidity concern: {current_ratio:.1f} current ratio")

    # Asset quality
    if latest.cash_and_equivalents and latest.current_assets:
        cash_ratio = latest.cash_and_equivalents / latest.current_assets
        if cash_ratio > 0.3:  # 30%+ cash in current assets
            score += 1
            details.append(f"High-quality liquid assets: {cash_ratio:.1%} cash ratio")

    # Equity base stability
    equity_vals = [item.shareholders_equity for item in financial_line_items if item.shareholders_equity]
    if len(equity_vals) >= 3:
        equity_growth = (equity_vals[0] / equity_vals[-1]) ** (1 / len(equity_vals)) - 1
        if equity_growth > 0.05:  # 5%+ equity growth
            score += 1
            details.append(f"Growing equity base: {equity_growth:.1%} CAGR")

    return {"score": score, "details": "; ".join(details)}


def analyze_valuation_margin_of_safety(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Eveillard's margin of safety requirements"""
    score = 0
    details = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No valuation data"}

    latest_metrics = metrics[0]
    latest_financial = financial_line_items[0]

    # Conservative P/E analysis
    if latest_metrics.price_to_earnings and latest_metrics.price_to_earnings > 0:
        if latest_metrics.price_to_earnings < 12:
            score += 3
            details.append(f"Conservative P/E: {latest_metrics.price_to_earnings:.1f}")
        elif latest_metrics.price_to_earnings < 18:
            score += 1
            details.append(f"Reasonable P/E: {latest_metrics.price_to_earnings:.1f}")

    # Book value safety
    if latest_metrics.price_to_book:
        if latest_metrics.price_to_book < 1.0:
            score += 3
            details.append(f"Trading below book: {latest_metrics.price_to_book:.2f} P/B")
        elif latest_metrics.price_to_book < 1.5:
            score += 2
            details.append(f"Conservative P/B: {latest_metrics.price_to_book:.2f}")

    # Free cash flow yield
    if latest_financial.free_cash_flow and latest_financial.free_cash_flow > 0:
        fcf_yield = latest_financial.free_cash_flow / market_cap
        if fcf_yield > 0.08:  # 8%+ yield
            score += 2
            details.append(f"Attractive FCF yield: {fcf_yield:.1%}")
        elif fcf_yield > 0.05:
            score += 1
            details.append(f"Reasonable FCF yield: {fcf_yield:.1%}")

    # Dividend yield (simplified - would need actual dividend data)
    if latest_financial.net_income and latest_financial.net_income > 0:
        estimated_dividend_yield = (latest_financial.net_income * 0.3) / market_cap  # Assume 30% payout
        if estimated_dividend_yield > 0.03:  # 3%+ yield
            score += 1
            details.append(f"Potential dividend yield: {estimated_dividend_yield:.1%}")

    return {"score": score, "details": "; ".join(details)}


def analyze_business_predictability(financial_line_items: list) -> dict:
    """Eveillard's preference for predictable businesses"""
    score = 0
    details = []

    if not financial_line_items:
        return {"score": 0, "details": "No predictability data"}

    # Revenue stability
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 5:
        revenue_volatility = (max(revenues) - min(revenues)) / max(revenues)
        if revenue_volatility < 0.2:  # Low volatility
            score += 2
            details.append(f"Stable revenue pattern: {revenue_volatility:.1%} volatility")
        elif revenue_volatility > 0.5:
            score -= 1
            details.append(f"High revenue volatility: {revenue_volatility:.1%}")

    # Earnings consistency
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings:
        negative_years = sum(1 for e in earnings if e < 0)
        if negative_years == 0:
            score += 2
            details.append(f"No loss years in {len(earnings)} periods")
        elif negative_years <= 1:
            score += 1
            details.append(f"Only {negative_years} loss year(s)")

    # Cash flow predictability
    fcf_vals = [item.free_cash_flow for item in financial_line_items if item.free_cash_flow]
    if fcf_vals:
        positive_fcf_years = sum(1 for fcf in fcf_vals if fcf > 0)
        if positive_fcf_years >= len(fcf_vals) * 0.8:  # 80%+ positive years
            score += 2
            details.append(f"Consistent cash generation: {positive_fcf_years}/{len(fcf_vals)} years")

    # Business model simplicity (proxy: stable margins)
    margins = []
    for item in financial_line_items:
        if item.net_income and item.revenue and item.revenue > 0:
            margins.append(item.net_income / item.revenue)

    if len(margins) >= 3:
        margin_stability = max(margins) - min(margins)
        if margin_stability < 0.05:  # 5% margin range
            score += 1
            details.append(f"Stable profit margins: {margin_stability:.1%} range")

    return {"score": score, "details": "; ".join(details)}


def calculate_downside_protection(financial_line_items: list, market_cap: float) -> dict:
    """Eveillard's downside protection calculation"""
    score = 0
    details = []

    if not financial_line_items or not market_cap:
        return {"score": 0, "details": "No downside protection data"}

    latest = financial_line_items[0]

    # Liquidation value analysis
    current_assets = latest.current_assets if latest.current_assets else 0
    cash = latest.cash_and_equivalents if latest.cash_and_equivalents else 0

    # Conservative liquidation values
    conservative_assets = cash + (current_assets - cash) * 0.7  # 70% recovery on non-cash
    current_liabilities = latest.current_liabilities if latest.current_liabilities else 0
    total_debt = latest.total_debt if latest.total_debt else 0

    net_liquidation_value = conservative_assets - current_liabilities - total_debt

    if net_liquidation_value > 0:
        liquidation_ratio = net_liquidation_value / market_cap
        if liquidation_ratio > 0.7:  # 70%+ of market cap
            score += 4
            details.append(f"Excellent liquidation protection: {liquidation_ratio:.1%}")
        elif liquidation_ratio > 0.4:
            score += 2
            details.append(f"Good liquidation protection: {liquidation_ratio:.1%}")
        elif liquidation_ratio > 0.2:
            score += 1
            details.append(f"Some liquidation protection: {liquidation_ratio:.1%}")

    # Tangible book value protection
    if latest.shareholders_equity:
        book_protection = latest.shareholders_equity / market_cap
        if book_protection > 1.0:
            score += 2
            details.append(f"Trading below book value: {book_protection:.1f}x")
        elif book_protection > 0.8:
            score += 1
            details.append(f"Near book value protection: {book_protection:.1f}x")

    # Earnings floor
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings:
        worst_earnings = min(earnings)
        if worst_earnings > 0:
            score += 2
            details.append("No historical losses provide earnings floor")
        elif worst_earnings > -market_cap * 0.03:  # Small loss relative to market cap
            score += 1
            details.append("Limited historical downside from worst performance")

    return {"score": score, "details": "; ".join(details)}


def generate_eveillard_output(ticker: str, analysis_data: dict, state: AgentState, agent_id: str) -> EveillardSignal:
    """Generate Eveillard-style capital preservation decision"""
    template = ChatPromptTemplate.from_messages([
        ("system", """You are Jean-Marie Eveillard's AI agent, applying global value investing with capital preservation focus:

        1. Capital preservation: "Return of capital is more important than return on capital"
        2. Conservative balance sheets: Low debt, strong liquidity, asset backing
        3. Margin of safety: Buy with significant downside protection
        4. Predictable businesses: Avoid complexity and volatility
        5. Global perspective: Consider opportunities worldwide
        6. Patient approach: Wait for exceptional opportunities
        7. Risk awareness: Focus on what can go wrong

        Reasoning style:
        - Emphasize downside protection and risk management
        - Focus on balance sheet strength and financial conservatism
        - Discuss predictability and business simplicity
        - Consider liquidation values and asset backing
        - Express conservative skepticism
        - Require substantial margin of safety
        - Acknowledge when standards are not met

        Return bullish only for exceptionally safe investments with strong downside protection and reasonable upside."""),

        ("human", """Apply capital preservation analysis to {ticker}:

        {analysis_data}

        Provide investment signal in JSON format:
        {{
          "signal": "bullish" | "bearish" | "neutral",
          "confidence": float (0-100),
          "reasoning": "string"
        }}""")
    ])

    prompt = template.invoke({"analysis_data": json.dumps(analysis_data, indent=2), "ticker": ticker})

    def create_default_eveillard_signal():
        return EveillardSignal(signal="neutral", confidence=0.0, reasoning="Analysis error, defaulting to neutral")

    return call_llm(
        prompt=prompt,
        pydantic_model=EveillardSignal,
        agent_name=agent_id,
        state=state,
        default_factory=create_default_eveillard_signal,
    )