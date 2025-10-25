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


class MungerSignal(BaseModel):
    signal: Literal["bullish", "bearish", "neutral"]
    confidence: float
    reasoning: str


def charlie_munger_agent(state: AgentState, agent_id: str = "charlie_munger_agent"):
    """
    Charlie Munger: Mental models, quality businesses, patience
    Focus: Multidisciplinary thinking, concentrated bets, long-term compounding
    """
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    api_key = get_api_key_from_state(state, "FINANCIAL_DATASETS_API_KEY")
    analysis_data = {}
    munger_analysis = {}

    for ticker in tickers:
        progress.update_status(agent_id, ticker, "Applying multidisciplinary mental models")
        metrics = get_financial_metrics(ticker, end_date, period="annual", limit=10, api_key=api_key)

        financial_line_items = search_line_items(
            ticker,
            [
                "revenue", "net_income", "free_cash_flow", "shareholders_equity",
                "total_debt", "retained_earnings", "operating_margin",
                "research_and_development", "outstanding_shares", "total_assets"
            ],
            end_date,
            period="annual",
            limit=10,
            api_key=api_key,
        )

        market_cap = get_market_cap(ticker, end_date, api_key=api_key)

        # Munger's mental model analyses
        business_quality = analyze_mental_model_quality(metrics, financial_line_items)
        moat_durability = analyze_competitive_moat_durability(metrics, financial_line_items)
        management_rationality = analyze_management_rationality(financial_line_items)
        compound_growth = analyze_compounding_potential(financial_line_items)
        incentive_alignment = analyze_incentive_structures(financial_line_items)

        # Munger's weighted approach (fewer, better decisions)
        total_score = (
                business_quality["score"] * 0.3 +
                moat_durability["score"] * 0.25 +
                management_rationality["score"] * 0.2 +
                compound_growth["score"] * 0.15 +
                incentive_alignment["score"] * 0.1
        )

        # Munger's high-conviction approach
        if total_score >= 8.5:  # Very high bar
            signal = "bullish"
        elif total_score <= 3.0:
            signal = "bearish"
        else:
            signal = "neutral"

        analysis_data[ticker] = {
            "signal": signal,
            "score": total_score,
            "business_quality": business_quality,
            "moat_durability": moat_durability,
            "management_rationality": management_rationality,
            "compound_growth": compound_growth,
            "incentive_alignment": incentive_alignment
        }

        munger_output = generate_munger_output(ticker, analysis_data, state, agent_id)

        munger_analysis[ticker] = {
            "signal": munger_output.signal,
            "confidence": munger_output.confidence,
            "reasoning": munger_output.reasoning
        }

        progress.update_status(agent_id, ticker, "Done", analysis=munger_output.reasoning)

    message = HumanMessage(content=json.dumps(munger_analysis), name=agent_id)

    if state["metadata"]["show_reasoning"]:
        show_agent_reasoning(munger_analysis, "Charlie Munger Agent")

    state["data"]["analyst_signals"][agent_id] = munger_analysis
    progress.update_status(agent_id, None, "Done")

    return {"messages": [message], "data": state["data"]}


def analyze_mental_model_quality(metrics: list, financial_line_items: list) -> dict:
    """Munger's multidisciplinary analysis of business quality"""
    score = 0
    details = []

    if not metrics or not financial_line_items:
        return {"score": 0, "details": "No data for quality analysis"}

    # Psychology model: Customer psychology and habits
    # Measured by revenue consistency and margin stability
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if revenues and len(revenues) >= 5:
        revenue_volatility = (max(revenues) - min(revenues)) / max(revenues)
        if revenue_volatility < 0.2:  # Stable customer behavior
            score += 2
            details.append(f"Stable customer psychology: {revenue_volatility:.1%} revenue volatility")

    # Economics model: High return on capital
    roic_vals = [m.return_on_invested_capital for m in metrics if m.return_on_invested_capital]
    if roic_vals:
        avg_roic = sum(roic_vals) / len(roic_vals)
        if avg_roic > 0.15:  # 15%+ ROIC consistently
            score += 3
            details.append(f"Superior capital efficiency: {avg_roic:.1%} avg ROIC")

    # Math model: Compound growth potential
    earnings = [item.net_income for item in financial_line_items if item.net_income and item.net_income > 0]
    if len(earnings) >= 5:
        cagr = (earnings[0] / earnings[-1]) ** (1 / len(earnings)) - 1
        if cagr > 0.1:  # 10%+ earnings CAGR
            score += 2
            details.append(f"Compounding machine: {cagr:.1%} earnings CAGR")

    # Physics model: Competitive forces and market position
    margins = [item.operating_margin for item in financial_line_items if item.operating_margin]
    if margins:
        avg_margin = sum(margins) / len(margins)
        if avg_margin > 0.2:  # 20%+ operating margins
            score += 2
            details.append(f"Pricing power evidence: {avg_margin:.1%} avg operating margin")

    return {"score": score, "details": "; ".join(details)}


def analyze_competitive_moat_durability(metrics: list, financial_line_items: list) -> dict:
    """Munger's focus on durable competitive advantages"""
    score = 0
    details = []

    if not metrics:
        return {"score": 0, "details": "No metrics for moat analysis"}

    # Consistent high returns (sign of moat)
    roe_vals = [m.return_on_equity for m in metrics if m.return_on_equity]
    if roe_vals:
        high_roe_years = sum(1 for roe in roe_vals if roe > 0.15)
        if high_roe_years >= len(roe_vals) * 0.8:  # 80% of years
            score += 3
            details.append(f"Durable high returns: {high_roe_years}/{len(roe_vals)} years ROE >15%")

    # Market share stability (revenue growth matching/exceeding market)
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 5:
        growth_consistency = sum(1 for i in range(len(revenues) - 1) if revenues[i] >= revenues[i + 1])
        if growth_consistency >= len(revenues) * 0.7:
            score += 2
            details.append("Market position strength: consistent revenue progression")

    # R&D efficiency (innovation moat)
    rd_vals = [item.research_and_development for item in financial_line_items if item.research_and_development]
    if rd_vals and revenues:
        rd_intensity = sum(rd_vals) / sum(revenues[:len(rd_vals)])
        if 0.05 <= rd_intensity <= 0.15:  # Efficient innovation spend
            score += 2
            details.append(f"Innovation moat: {rd_intensity:.1%} R&D efficiency")

    # Capital requirements (barrier to entry)
    assets = [item.total_assets for item in financial_line_items if item.total_assets]
    if assets and revenues:
        asset_turnover = revenues[0] / assets[0] if assets[0] > 0 else 0
        if asset_turnover > 1.5:  # Asset-light model
            score += 1
            details.append(f"Capital-efficient model: {asset_turnover:.1f}x asset turnover")
        elif asset_turnover < 0.5:  # High barriers
            score += 1
            details.append(f"High barriers to entry: {asset_turnover:.1f}x asset turnover")

    return {"score": score, "details": "; ".join(details)}


def analyze_management_rationality(financial_line_items: list) -> dict:
    """Munger's assessment of management decision-making"""
    score = 0
    details = []

    if not financial_line_items:
        return {"score": 0, "details": "No data for management analysis"}

    # Capital allocation rationality
    fcf_vals = [item.free_cash_flow for item in financial_line_items if item.free_cash_flow]
    retained_earnings = [item.retained_earnings for item in financial_line_items if item.retained_earnings]

    if len(retained_earnings) >= 3:
        # Check if retained earnings are creating value
        re_growth = (retained_earnings[0] - retained_earnings[-1]) / abs(retained_earnings[-1])
        if re_growth > 0.5:  # 50%+ growth in retained earnings
            score += 2
            details.append(f"Value-creating reinvestment: {re_growth:.1%} retained earnings growth")

    # Share buyback rationality (buying when cheap)
    shares = [item.outstanding_shares for item in financial_line_items if item.outstanding_shares]
    earnings = [item.net_income for item in financial_line_items if item.net_income and item.net_income > 0]

    if len(shares) >= 3 and len(earnings) >= 3:
        share_reduction = (shares[-1] - shares[0]) / shares[-1]
        earnings_growth = (earnings[0] - earnings[-1]) / earnings[-1]

        if share_reduction > 0.05 and earnings_growth > share_reduction:
            score += 2
            details.append("Rational buybacks: reducing shares while growing earnings")

    # Debt management
    debt_vals = [item.total_debt for item in financial_line_items if item.total_debt is not None]
    if debt_vals:
        if all(debt <= debt_vals[0] * 1.1 for debt in debt_vals):  # Controlled debt growth
            score += 1
            details.append("Conservative debt management")

    # Dividend policy consistency
    # Note: Would need dividend data for full analysis
    if fcf_vals and all(fcf > 0 for fcf in fcf_vals):
        score += 1
        details.append("Consistent cash generation enables rational capital allocation")

    return {"score": score, "details": "; ".join(details)}


def analyze_compounding_potential(financial_line_items: list) -> dict:
    """Munger's emphasis on long-term compounding"""
    score = 0
    details = []

    if not financial_line_items:
        return {"score": 0, "details": "No data for compounding analysis"}

    # Revenue compounding
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 5:
        revenue_cagr = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1
        if revenue_cagr > 0.1:
            score += 2
            details.append(f"Revenue compounding: {revenue_cagr:.1%} CAGR")

    # Earnings compounding quality
    earnings = [item.net_income for item in financial_line_items if item.net_income and item.net_income > 0]
    if len(earnings) >= 5:
        earnings_cagr = (earnings[0] / earnings[-1]) ** (1 / len(earnings)) - 1
        if earnings_cagr > revenue_cagr if 'revenue_cagr' in locals() else 0.08:
            score += 2
            details.append(f"Earnings leverage: {earnings_cagr:.1%} CAGR")

    # Free cash flow compounding
    fcf_vals = [item.free_cash_flow for item in financial_line_items if item.free_cash_flow and item.free_cash_flow > 0]
    if len(fcf_vals) >= 3:
        fcf_growth = (fcf_vals[0] / fcf_vals[-1]) ** (1 / len(fcf_vals)) - 1
        if fcf_growth > 0.12:  # 12%+ FCF growth
            score += 3
            details.append(f"Cash compounding: {fcf_growth:.1%} FCF CAGR")

    # Reinvestment rate efficiency
    if retained_earnings and len(retained_earnings) >= 3:
        reinvestment_efficiency = earnings_cagr / (
                    retained_earnings[0] / revenues[0]) if 'earnings_cagr' in locals() and revenues else 0
        if reinvestment_efficiency > 2.0:  # High returns on reinvestment
            score += 1
            details.append("Efficient reinvestment compounding")

    return {"score": score, "details": "; ".join(details)}


def analyze_incentive_structures(financial_line_items: list) -> dict:
    """Munger's focus on proper incentive alignment"""
    score = 0
    details = []

    if not financial_line_items:
        return {"score": 0, "details": "No data for incentive analysis"}

    # Management ownership alignment (proxy through performance)
    # If management is aligned, we should see improving metrics over time

    # Consistent margin improvement (management efficiency)
    margins = [item.operating_margin for item in financial_line_items if item.operating_margin]
    if len(margins) >= 3:
        margin_trend = sum(1 for i in range(len(margins) - 1) if margins[i] >= margins[i + 1])
        if margin_trend >= len(margins) * 0.6:
            score += 2
            details.append("Management efficiency: improving operational margins")

    # Capital efficiency improvement
    assets = [item.total_assets for item in financial_line_items if item.total_assets]
    revenues = [item.revenue for item in financial_line_items if item.revenue]

    if len(assets) >= 3 and len(revenues) >= 3:
        recent_efficiency = revenues[0] / assets[0] if assets[0] > 0 else 0
        older_efficiency = revenues[-1] / assets[-1] if assets[-1] > 0 else 0

        if recent_efficiency > older_efficiency * 1.1:
            score += 1
            details.append("Improving capital efficiency over time")

    # Long-term value creation focus
    shareholders_equity = [item.shareholders_equity for item in financial_line_items if item.shareholders_equity]
    if len(shareholders_equity) >= 5:
        equity_cagr = (shareholders_equity[0] / shareholders_equity[-1]) ** (1 / len(shareholders_equity)) - 1
        if equity_cagr > 0.1:
            score += 2
            details.append(f"Shareholder value creation: {equity_cagr:.1%} equity CAGR")

    return {"score": score, "details": "; ".join(details)}


def generate_munger_output(ticker: str, analysis_data: dict, state: AgentState, agent_id: str) -> MungerSignal:
    """Generate Munger-style investment decision using mental models"""
    template = ChatPromptTemplate.from_messages([
        ("system", """You are Charlie Munger's AI agent, applying multidisciplinary mental models to investing:

        1. Mental Models: Psychology, economics, math, physics applied to business analysis
        2. Quality over quantity: "It's better to buy a wonderful company at a fair price"
        3. Long-term compounding: "The first rule of compounding is to never interrupt it unnecessarily"
        4. Rational management: Assess capital allocation and incentive alignment
        5. Durable moats: Sustainable competitive advantages over decades
        6. Concentrated bets: "Diversification is protection against ignorance"
        7. Patience and discipline: Wait for exceptional opportunities

        Reasoning style:
        - Use multidisciplinary frameworks and analogies
        - Emphasize long-term thinking and compounding
        - Focus on business quality and management rationality
        - Apply inverse thinking: "What could go wrong?"
        - Reference mental models and psychological biases
        - Express strong convictions when warranted
        - Admit when outside circle of competence

        Return bullish only for exceptional businesses with durable moats and rational management."""),

        ("human", """Apply multidisciplinary mental models to analyze {ticker}:

        {analysis_data}

        Provide investment signal in JSON format:
        {{
          "signal": "bullish" | "bearish" | "neutral",
          "confidence": float (0-100),
          "reasoning": "string"
        }}""")
    ])

    prompt = template.invoke({"analysis_data": json.dumps(analysis_data, indent=2), "ticker": ticker})

    def create_default_munger_signal():
        return MungerSignal(signal="neutral", confidence=0.0, reasoning="Analysis error, defaulting to neutral")

    return call_llm(
        prompt=prompt,
        pydantic_model=MungerSignal,
        agent_name=agent_id,
        state=state,
        default_factory=create_default_munger_signal,
    )