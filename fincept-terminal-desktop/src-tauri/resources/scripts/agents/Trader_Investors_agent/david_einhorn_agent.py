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


class EinhornSignal(BaseModel):
    signal: Literal["bullish", "bearish", "neutral"]
    confidence: float
    reasoning: str


def david_einhorn_agent(state: AgentState, agent_id: str = "david_einhorn_agent"):
    """
    David Einhorn: Short selling + value, forensic accounting
    Focus: Accounting irregularities, overvalued shorts, undervalued longs
    """
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    api_key = get_api_key_from_state(state, "FINANCIAL_DATASETS_API_KEY")
    analysis_data = {}
    einhorn_analysis = {}

    for ticker in tickers:
        progress.update_status(agent_id, ticker, "Conducting forensic accounting analysis")
        metrics = get_financial_metrics(ticker, end_date, period="annual", limit=5, api_key=api_key)

        financial_line_items = search_line_items(
            ticker,
            [
                "revenue", "net_income", "operating_income", "free_cash_flow",
                "total_assets", "current_assets", "accounts_receivable",
                "inventory", "total_debt", "shareholders_equity", "depreciation"
            ],
            end_date,
            period="annual",
            limit=5,
            api_key=api_key,
        )

        market_cap = get_market_cap(ticker, end_date, api_key=api_key)

        # Einhorn's analytical framework
        accounting_quality = analyze_accounting_quality(financial_line_items)
        earnings_quality = analyze_earnings_vs_cash_flow(financial_line_items)
        valuation_vs_reality = analyze_valuation_disconnect(metrics, financial_line_items, market_cap)
        red_flags = identify_red_flags(financial_line_items)
        fundamental_value = calculate_fundamental_value(financial_line_items, market_cap)

        # Einhorn's long/short framework
        total_score = (
                accounting_quality["score"] * 0.25 +
                earnings_quality["score"] * 0.25 +
                fundamental_value["score"] * 0.2 +
                valuation_vs_reality["score"] * 0.2 +
                red_flags["score"] * 0.1
        )

        # Einhorn's approach: strong convictions both ways
        if total_score >= 7.5:
            signal = "bullish"
        elif total_score <= 3.5:
            signal = "bearish"
        else:
            signal = "neutral"

        analysis_data[ticker] = {
            "signal": signal,
            "score": total_score,
            "accounting_quality": accounting_quality,
            "earnings_quality": earnings_quality,
            "valuation_vs_reality": valuation_vs_reality,
            "red_flags": red_flags,
            "fundamental_value": fundamental_value
        }

        einhorn_output = generate_einhorn_output(ticker, analysis_data, state, agent_id)

        einhorn_analysis[ticker] = {
            "signal": einhorn_output.signal,
            "confidence": einhorn_output.confidence,
            "reasoning": einhorn_output.reasoning
        }

        progress.update_status(agent_id, ticker, "Done", analysis=einhorn_output.reasoning)

    message = HumanMessage(content=json.dumps(einhorn_analysis), name=agent_id)

    if state["metadata"]["show_reasoning"]:
        show_agent_reasoning(einhorn_analysis, "David Einhorn Agent")

    state["data"]["analyst_signals"][agent_id] = einhorn_analysis
    progress.update_status(agent_id, None, "Done")

    return {"messages": [message], "data": state["data"]}


def analyze_accounting_quality(financial_line_items: list) -> dict:
    """Einhorn's forensic accounting analysis"""
    score = 0
    details = []
    red_flags = []

    if not financial_line_items:
        return {"score": 0, "details": "No accounting data"}

    # Revenue quality analysis
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    receivables = [item.accounts_receivable for item in financial_line_items if item.accounts_receivable]

    if len(revenues) >= 3 and len(receivables) >= 3:
        # Days sales outstanding trend
        recent_dso = (receivables[0] / revenues[0]) * 365 if revenues[0] > 0 else 0
        older_dso = (receivables[-1] / revenues[-1]) * 365 if revenues[-1] > 0 else 0

        if recent_dso > older_dso * 1.3:  # 30%+ increase in DSO
            red_flags.append("Deteriorating receivables collection")
            score -= 2
        elif recent_dso < older_dso * 1.1:  # Stable or improving
            score += 1
            details.append("Stable receivables management")

    # Inventory quality
    inventories = [item.inventory for item in financial_line_items if item.inventory]
    if len(inventories) >= 3 and len(revenues) >= 3:
        recent_inventory_turns = revenues[0] / inventories[0] if inventories[0] > 0 else 0
        older_inventory_turns = revenues[-1] / inventories[-1] if inventories[-1] > 0 else 0

        if recent_inventory_turns < older_inventory_turns * 0.8:  # Slowing turns
            red_flags.append("Deteriorating inventory turns")
            score -= 1
        elif recent_inventory_turns > older_inventory_turns * 1.1:
            score += 1
            details.append("Improving inventory management")

    # Asset quality
    assets = [item.total_assets for item in financial_line_items if item.total_assets]
    if len(assets) >= 3:
        asset_growth = (assets[0] / assets[-1]) ** (1 / len(assets)) - 1
        revenue_growth = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1 if revenues else 0

        if asset_growth > revenue_growth * 1.5:  # Assets growing much faster than revenue
            red_flags.append("Asset growth exceeds revenue growth")
            score -= 1
        else:
            score += 1
            details.append("Reasonable asset growth relative to revenue")

    if red_flags:
        details.append(f"Red flags: {'; '.join(red_flags)}")

    return {"score": score, "details": "; ".join(details), "red_flags": red_flags}


def analyze_earnings_vs_cash_flow(financial_line_items: list) -> dict:
    """Einhorn's earnings quality through cash flow analysis"""
    score = 0
    details = []

    if not financial_line_items:
        return {"score": 0, "details": "No cash flow data"}

    # Earnings vs cash flow divergence
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    cash_flows = [item.free_cash_flow for item in financial_line_items if item.free_cash_flow]

    if len(earnings) >= 3 and len(cash_flows) >= 3:
        # Compare cumulative earnings vs cash flow
        total_earnings = sum(earnings)
        total_cash_flow = sum(cash_flows)

        if total_cash_flow > total_earnings * 0.8:  # FCF is 80%+ of earnings
            score += 2
            details.append(f"Strong cash conversion: FCF/Earnings = {total_cash_flow / total_earnings:.2f}")
        elif total_cash_flow < total_earnings * 0.5:  # FCF is <50% of earnings
            score -= 2
            details.append(f"Poor cash conversion: FCF/Earnings = {total_cash_flow / total_earnings:.2f}")

    # Accruals analysis (simplified)
    if earnings and cash_flows:
        recent_accruals = earnings[0] - cash_flows[0] if len(earnings) > 0 and len(cash_flows) > 0 else 0
        if abs(recent_accruals) > abs(earnings[0]) * 0.3:  # High accruals relative to earnings
            score -= 1
            details.append("High accruals relative to earnings - quality concern")
        else:
            score += 1
            details.append("Reasonable accruals level")

    # Consistency of cash generation
    if cash_flows:
        positive_fcf_years = sum(1 for fcf in cash_flows if fcf > 0)
        if positive_fcf_years == len(cash_flows):
            score += 2
            details.append(f"Consistent cash generation: {positive_fcf_years}/{len(cash_flows)} years")
        elif positive_fcf_years < len(cash_flows) * 0.6:
            score -= 1
            details.append("Inconsistent cash generation")

    return {"score": score, "details": "; ".join(details)}


def analyze_valuation_disconnect(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Einhorn's analysis of valuation vs fundamental reality"""
    score = 0
    details = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No valuation data"}

    latest_metrics = metrics[0]
    latest_financial = financial_line_items[0]

    # P/E vs growth analysis
    if latest_metrics.price_to_earnings and latest_metrics.price_to_earnings > 0:
        earnings = [item.net_income for item in financial_line_items if item.net_income and item.net_income > 0]
        if len(earnings) >= 3:
            earnings_growth = (earnings[0] / earnings[-1]) ** (1 / len(earnings)) - 1
            peg_ratio = latest_metrics.price_to_earnings / (earnings_growth * 100) if earnings_growth > 0 else float(
                'inf')

            if peg_ratio < 1.0:  # Undervalued relative to growth
                score += 2
                details.append(f"Attractive PEG ratio: {peg_ratio:.2f}")
            elif peg_ratio > 2.0:  # Overvalued relative to growth
                score -= 2
                details.append(f"Expensive PEG ratio: {peg_ratio:.2f}")

    # Book value reality check
    if latest_metrics.price_to_book:
        if latest_metrics.price_to_book < 1.0:  # Trading below book
            score += 1
            details.append(f"Trading below book value: {latest_metrics.price_to_book:.2f} P/B")
        elif latest_metrics.price_to_book > 3.0 and latest_metrics.return_on_equity and latest_metrics.return_on_equity < 0.15:
            score -= 2
            details.append(
                f"High P/B with mediocre ROE: {latest_metrics.price_to_book:.2f} P/B, {latest_metrics.return_on_equity:.1%} ROE")

    # Asset-based valuation
    if latest_financial.total_assets and latest_financial.total_debt:
        tangible_book = latest_financial.total_assets - latest_financial.total_debt
        if tangible_book > market_cap:
            score += 2
            details.append("Trading below tangible asset value")

    return {"score": score, "details": "; ".join(details)}


def identify_red_flags(financial_line_items: list) -> dict:
    """Einhorn's red flag identification system"""
    score = 0
    details = []
    flags = []

    if not financial_line_items:
        return {"score": 0, "details": "No data for red flag analysis"}

    # Rapid asset growth without corresponding revenue growth
    assets = [item.total_assets for item in financial_line_items if item.total_assets]
    revenues = [item.revenue for item in financial_line_items if item.revenue]

    if len(assets) >= 3 and len(revenues) >= 3:
        asset_cagr = (assets[0] / assets[-1]) ** (1 / len(assets)) - 1
        revenue_cagr = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1

        if asset_cagr > revenue_cagr + 0.1:  # Assets growing 10%+ faster than revenue
            flags.append("Asset growth exceeds revenue growth")
            score -= 2

    # Declining margins with flat/growing revenue
    margins = []
    for item in financial_line_items:
        if item.revenue and item.operating_income and item.revenue > 0:
            margins.append(item.operating_income / item.revenue)

    if len(margins) >= 3:
        margin_trend = margins[0] - margins[-1]
        if margin_trend < -0.05:  # 5%+ margin decline
            flags.append("Significant margin compression")
            score -= 1

    # Inconsistent cash flow patterns
    cash_flows = [item.free_cash_flow for item in financial_line_items if item.free_cash_flow]
    if cash_flows:
        negative_years = sum(1 for fcf in cash_flows if fcf < 0)
        if negative_years > len(cash_flows) * 0.4:  # >40% negative FCF years
            flags.append("Inconsistent cash generation")
            score -= 1

    # High receivables growth
    receivables = [item.accounts_receivable for item in financial_line_items if item.accounts_receivable]
    if len(receivables) >= 3 and len(revenues) >= 3:
        receivables_growth = (receivables[0] / receivables[-1]) ** (1 / len(receivables)) - 1
        revenue_growth = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1

        if receivables_growth > revenue_growth + 0.15:  # Receivables growing 15%+ faster
            flags.append("Receivables growing faster than revenue")
            score -= 1

    if not flags:
        score += 2
        details.append("No major red flags identified")
    else:
        details.append(f"Red flags: {'; '.join(flags)}")

    return {"score": score, "details": "; ".join(details), "flags": flags}


def calculate_fundamental_value(financial_line_items: list, market_cap: float) -> dict:
    """Einhorn's fundamental valuation analysis"""
    score = 0
    details = []

    if not financial_line_items or not market_cap:
        return {"score": 0, "details": "No fundamental data"}

    latest = financial_line_items[0]

    # Asset-based valuation
    if latest.total_assets and latest.total_debt:
        book_value = latest.total_assets - latest.total_debt
        pb_ratio = market_cap / book_value if book_value > 0 else float('inf')

        if pb_ratio < 1.0:
            score += 3
            details.append(f"Trading below book value: {pb_ratio:.2f} P/B")
        elif pb_ratio < 1.5:
            score += 1
            details.append(f"Reasonable book value: {pb_ratio:.2f} P/B")

    # Earnings-based valuation
    earnings = [item.net_income for item in financial_line_items if item.net_income and item.net_income > 0]
    if earnings:
        avg_earnings = sum(earnings) / len(earnings)
        pe_ratio = market_cap / avg_earnings

        if pe_ratio < 12:
            score += 2
            details.append(f"Low earnings multiple: {pe_ratio:.1f} P/E")
        elif pe_ratio > 25:
            score -= 1
            details.append(f"High earnings multiple: {pe_ratio:.1f} P/E")

    # Cash flow valuation
    if latest.free_cash_flow and latest.free_cash_flow > 0:
        fcf_yield = latest.free_cash_flow / market_cap
        if fcf_yield > 0.08:  # 8%+ FCF yield
            score += 2
            details.append(f"Attractive FCF yield: {fcf_yield:.1%}")
        elif fcf_yield < 0.03:  # <3% FCF yield
            score -= 1
            details.append(f"Low FCF yield: {fcf_yield:.1%}")

    return {"score": score, "details": "; ".join(details)}


def generate_einhorn_output(ticker: str, analysis_data: dict, state: AgentState, agent_id: str) -> EinhornSignal:
    """Generate Einhorn-style forensic analysis decision"""
    template = ChatPromptTemplate.from_messages([
        ("system", """You are David Einhorn's AI agent, applying forensic accounting and value analysis:

        1. Forensic accounting: Scrutinize financial statements for quality and red flags
        2. Earnings quality: Compare earnings to cash flows, analyze accruals
        3. Value vs. price: Identify disconnects between valuation and reality
        4. Short candidates: Find overvalued companies with accounting issues
        5. Long candidates: Find undervalued companies with clean accounting
        6. Red flag detection: Identify warning signs of financial manipulation
        7. Contrarian positioning: Take positions opposite to market sentiment

        Reasoning style:
        - Emphasize accounting quality and financial statement analysis
        - Focus on earnings vs. cash flow divergences
        - Identify specific red flags and warning signs
        - Discuss valuation relative to fundamental reality
        - Express skepticism toward popular stocks
        - Provide detailed forensic analysis
        - Consider both long and short opportunities

        Return bullish for undervalued companies with clean accounting, bearish for overvalued companies with red flags."""),

        ("human", """Apply forensic accounting analysis to {ticker}:

        {analysis_data}

        Provide investment signal in JSON format:
        {{
          "signal": "bullish" | "bearish" | "neutral",
          "confidence": float (0-100),
          "reasoning": "string"
        }}""")
    ])

    prompt = template.invoke({"analysis_data": json.dumps(analysis_data, indent=2), "ticker": ticker})

    def create_default_einhorn_signal():
        return EinhornSignal(signal="neutral", confidence=0.0, reasoning="Analysis error, defaulting to neutral")

    return call_llm(
        prompt=prompt,
        pydantic_model=EinhornSignal,
        agent_name=agent_id,
        state=state,
        default_factory=create_default_einhorn_signal,
    )