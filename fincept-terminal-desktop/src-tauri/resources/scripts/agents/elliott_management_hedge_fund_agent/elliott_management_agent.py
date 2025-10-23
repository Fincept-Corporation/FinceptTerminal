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


class ElliottSignal(BaseModel):
    signal: Literal["bullish", "bearish", "neutral"]
    confidence: float
    reasoning: str
    activist_potential: dict


def elliott_management_hedge_fund_agent(state: AgentState, agent_id: str = "elliott_management_hedge_fund_agent"):
    """
    Elliott Management: Activist investing, event-driven strategies
    Structure: Research → Legal → Activism → Event-Driven → Paul Singer Decision
    Philosophy: Catalyst-driven value creation, shareholder activism, distressed opportunities
    """
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    api_key = get_api_key_from_state(state, "FINANCIAL_DATASETS_API_KEY")
    analysis_data = {}
    elliott_analysis = {}

    for ticker in tickers:
        progress.update_status(agent_id, ticker, "Research team analyzing activist opportunity")

        metrics = get_financial_metrics(ticker, end_date, period="annual", limit=6, api_key=api_key)
        financial_line_items = search_line_items(
            ticker,
            [
                "revenue", "net_income", "free_cash_flow", "total_debt",
                "shareholders_equity", "operating_margin", "total_assets",
                "current_assets", "current_liabilities", "research_and_development"
            ],
            end_date,
            period="annual",
            limit=6,
            api_key=api_key,
        )
        market_cap = get_market_cap(ticker, end_date, api_key=api_key)

        # Elliott's activist-focused analysis structure
        progress.update_status(agent_id, ticker, "Research team fundamental analysis")
        research_team = research_team_analysis(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Legal team governance analysis")
        legal_team = legal_team_governance_analysis(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Activism team catalyst identification")
        activism_team = activism_team_analysis(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Event-driven team analysis")
        event_driven_team = event_driven_team_analysis(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Paul Singer strategic decision")
        paul_singer_decision = paul_singer_strategic_decision(
            research_team, legal_team, activism_team, event_driven_team
        )

        # Elliott's catalyst-weighted scoring
        total_score = (
                activism_team["score"] * 0.35 +
                research_team["score"] * 0.25 +
                event_driven_team["score"] * 0.2 +
                legal_team["score"] * 0.2
        )

        # Elliott's activist decision framework
        activist_potential = activism_team.get("activist_score", 0)
        if total_score >= 7.5 and activist_potential > 0.7:
            signal = "bullish"
        elif total_score <= 4.0 or legal_team.get("governance_score", 0) < 0.3:
            signal = "bearish"
        else:
            signal = "neutral"

        analysis_data[ticker] = {
            "signal": signal,
            "score": total_score,
            "research_team": research_team,
            "legal_team": legal_team,
            "activism_team": activism_team,
            "event_driven_team": event_driven_team,
            "paul_singer_decision": paul_singer_decision
        }

        elliott_output = generate_elliott_output(ticker, analysis_data, state, agent_id)

        elliott_analysis[ticker] = {
            "signal": elliott_output.signal,
            "confidence": elliott_output.confidence,
            "reasoning": elliott_output.reasoning,
            "activist_potential": elliott_output.activist_potential
        }

        progress.update_status(agent_id, ticker, "Done", analysis=elliott_output.reasoning)

    message = HumanMessage(content=json.dumps(elliott_analysis), name=agent_id)

    if state["metadata"]["show_reasoning"]:
        show_agent_reasoning(elliott_analysis, "Elliott Management")

    state["data"]["analyst_signals"][agent_id] = elliott_analysis
    progress.update_status(agent_id, None, "Done")

    return {"messages": [message], "data": state["data"]}


def research_team_analysis(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Elliott's research team fundamental analysis"""
    score = 0
    details = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No research data"}

    # Hidden asset value
    asset_value_opportunity = analyze_hidden_asset_value(financial_line_items, market_cap)
    if asset_value_opportunity > 0.3:
        score += 3
        details.append(f"Hidden asset value opportunity: {asset_value_opportunity:.1%} of market cap")

    # Underperforming business segments
    business_optimization = identify_business_optimization_opportunities(financial_line_items)
    if business_optimization > 0.6:
        score += 2.5
        details.append("Significant business optimization opportunities identified")

    # Capital allocation inefficiencies
    capital_allocation_issues = analyze_capital_allocation_inefficiencies(financial_line_items)
    if capital_allocation_issues > 0.7:
        score += 2
        details.append("Poor capital allocation creating value opportunity")

    # Margin improvement potential
    margin_expansion_potential = analyze_margin_expansion_potential(financial_line_items)
    if margin_expansion_potential > 0.5:
        score += 1.5
        details.append(f"Margin expansion potential: {margin_expansion_potential:.1%}")

    # Sum-of-the-parts analysis
    sotp_discount = calculate_sum_of_parts_discount(financial_line_items, market_cap)
    if sotp_discount > 0.2:
        score += 2
        details.append(f"Sum-of-parts discount: {sotp_discount:.1%}")

    return {"score": score, "details": "; ".join(details)}


def legal_team_governance_analysis(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Elliott's legal team corporate governance analysis"""
    score = 0
    details = []
    governance_issues = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No governance data"}

    # Management accountability
    management_performance = assess_management_performance(metrics, financial_line_items)
    if management_performance < 0.4:
        governance_issues.append("Poor management performance")
        score += 2  # Opportunity for change
        details.append("Management underperformance creates activist opportunity")

    # Board composition analysis
    board_independence = analyze_board_independence_proxy(financial_line_items)
    if board_independence < 0.6:
        governance_issues.append("Board independence concerns")
        score += 1.5
        details.append("Board independence issues identified")

    # Executive compensation analysis
    compensation_alignment = analyze_executive_compensation_proxy(financial_line_items)
    if compensation_alignment < 0.5:
        governance_issues.append("Executive compensation misalignment")
        score += 1.5
        details.append("Executive compensation not aligned with performance")

    # Shareholder rights
    shareholder_rights_score = assess_shareholder_rights_proxy(market_cap)
    if shareholder_rights_score > 0.7:
        score += 2
        details.append("Strong shareholder rights framework")

    # Acquisition defense analysis
    takeover_defenses = analyze_takeover_defenses_proxy(market_cap)
    if takeover_defenses < 0.5:
        score += 1
        details.append("Limited takeover defenses")

    governance_score = score / 8  # Normalize to 0-1

    if governance_issues:
        details.append(f"Governance issues: {'; '.join(governance_issues)}")

    return {
        "score": score,
        "details": "; ".join(details),
        "governance_issues": governance_issues,
        "governance_score": governance_score
    }


def activism_team_analysis(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Elliott's activism team catalyst and campaign analysis"""
    score = 0
    details = []
    activist_catalysts = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No activism data"}

    # Strategic alternatives analysis
    strategic_alternatives = identify_strategic_alternatives(financial_line_items, market_cap)
    if strategic_alternatives > 0.7:
        score += 3
        activist_catalysts.append("Strategic alternatives")
        details.append("Strong case for strategic alternatives (spin-offs, sales)")

    # Operational turnaround potential
    turnaround_potential = assess_operational_turnaround_potential(financial_line_items)
    if turnaround_potential > 0.6:
        score += 2.5
        activist_catalysts.append("Operational improvements")
        details.append("Significant operational turnaround opportunity")

    # Balance sheet optimization
    balance_sheet_optimization = analyze_balance_sheet_optimization(financial_line_items)
    if balance_sheet_optimization > 0.5:
        score += 2
        activist_catalysts.append("Balance sheet optimization")
        details.append("Balance sheet optimization opportunities")

    # Market position improvement
    market_position_catalyst = analyze_market_position_catalyst(financial_line_items)
    if market_position_catalyst > 0.6:
        score += 1.5
        activist_catalysts.append("Market position improvement")
        details.append("Market position improvement potential")

    # Management change catalyst
    management_change_potential = assess_management_change_potential(metrics, financial_line_items)
    if management_change_potential > 0.7:
        score += 2
        activist_catalysts.append("Management change")
        details.append("Strong case for management changes")

    # ESG improvement opportunities
    esg_catalyst = identify_esg_improvement_opportunities(financial_line_items)
    if esg_catalyst > 0.5:
        score += 1
        activist_catalysts.append("ESG improvements")
        details.append("ESG improvement opportunities identified")

    activist_score = score / 12  # Normalize to 0-1

    return {
        "score": score,
        "details": "; ".join(details),
        "activist_catalysts": activist_catalysts,
        "activist_score": activist_score
    }


def event_driven_team_analysis(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Elliott's event-driven team analysis"""
    score = 0
    details = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No event-driven data"}

    # M&A probability analysis
    ma_probability = assess_ma_probability(financial_line_items, market_cap)
    if ma_probability > 0.6:
        score += 3
        details.append(f"High M&A probability: {ma_probability:.1%}")

    # Spin-off opportunities
    spinoff_potential = analyze_spinoff_potential(financial_line_items)
    if spinoff_potential > 0.7:
        score += 2.5
        details.append("Strong spin-off potential identified")

    # Special dividend potential
    special_dividend_potential = analyze_special_dividend_potential(financial_line_items)
    if special_dividend_potential > 0.6:
        score += 2
        details.append("Special dividend potential")

    # Distressed situation analysis
    distressed_opportunity = analyze_distressed_opportunity(financial_line_items)
    if distressed_opportunity > 0.5:
        score += 2
        details.append("Distressed investment opportunity")

    # Regulatory catalyst analysis
    regulatory_catalyst = identify_regulatory_catalysts(financial_line_items)
    if regulatory_catalyst > 0.5:
        score += 1.5
        details.append("Regulatory catalysts identified")

    # Litigation catalyst
    litigation_catalyst = assess_litigation_catalyst_potential(financial_line_items)
    if litigation_catalyst > 0.4:
        score += 1
        details.append("Potential litigation catalysts")

    return {"score": score, "details": "; ".join(details)}


def paul_singer_strategic_decision(research_team: dict, legal_team: dict,
                                   activism_team: dict, event_driven_team: dict) -> dict:
    """Paul Singer's strategic decision framework"""
    details = []

    # Elliott's decision framework
    fundamental_value = research_team["score"] / 10
    governance_opportunity = legal_team["score"] / 8
    activist_potential = activism_team.get("activist_score", 0)
    event_catalyst = event_driven_team["score"] / 10

    # Paul Singer's strategic analysis
    overall_opportunity = (
            fundamental_value * 0.3 +
            activist_potential * 0.35 +
            event_catalyst * 0.25 +
            governance_opportunity * 0.1
    )

    # Campaign complexity assessment
    campaign_complexity = assess_campaign_complexity(activism_team, legal_team)

    # Risk-reward analysis
    if overall_opportunity > 0.8 and activist_potential > 0.7:
        if campaign_complexity < 0.6:
            decision = "Launch activist campaign"
            details.append("High-conviction activist opportunity with manageable complexity")
        else:
            decision = "Complex activist situation - detailed planning required"
            details.append("High opportunity but complex campaign execution needed")
    elif overall_opportunity > 0.6:
        if event_catalyst > 0.7:
            decision = "Event-driven position"
            details.append("Strong event-driven catalyst without activism requirement")
        else:
            decision = "Moderate activist position"
            details.append("Moderate opportunity for value creation through activism")
    elif fundamental_value > 0.6 and activist_potential < 0.4:
        decision = "Traditional value investment"
        details.append("Fundamental value present but limited activist opportunity")
    else:
        decision = "Pass - insufficient catalyst potential"
        details.append("Insufficient catalyst opportunity for Elliott's strategy")

    # Timeline assessment
    if "Management change" in activism_team.get("activist_catalysts", []):
        expected_timeline = "12-18 months"
    elif "Strategic alternatives" in activism_team.get("activist_catalysts", []):
        expected_timeline = "18-24 months"
    else:
        expected_timeline = "6-12 months"

    return {
        "decision": decision,
        "overall_opportunity": overall_opportunity,
        "campaign_complexity": campaign_complexity,
        "expected_timeline": expected_timeline,
        "details": "; ".join(details)
    }


# Helper functions for Elliott's analysis
def analyze_hidden_asset_value(financial_line_items: list, market_cap: float) -> float:
    """Analyze hidden asset value opportunities"""
    if not financial_line_items or not market_cap:
        return 0

    latest = financial_line_items[0]

    # Real estate and other assets potentially undervalued
    if latest.total_assets and latest.total_debt:
        net_assets = latest.total_assets - latest.total_debt
        if net_assets > market_cap:
            hidden_value = (net_assets - market_cap) / market_cap
            return min(hidden_value, 1.0)

    return 0


def identify_business_optimization_opportunities(financial_line_items: list) -> float:
    """Identify business optimization opportunities"""
    # Look for declining margins or inconsistent performance
    margins = [item.operating_margin for item in financial_line_items if item.operating_margin]

    if len(margins) >= 3:
        recent_margin = margins[0]
        historical_best = max(margins)

        if historical_best > recent_margin * 1.3:  # 30%+ margin decline from peak
            return 0.8  # High optimization opportunity
        elif historical_best > recent_margin * 1.15:  # 15%+ margin decline
            return 0.6  # Moderate optimization opportunity

    return 0.3


def analyze_capital_allocation_inefficiencies(financial_line_items: list) -> float:
    """Analyze capital allocation inefficiencies"""
    # Look for poor FCF to earnings conversion or excessive capex
    fcf_vals = [item.free_cash_flow for item in financial_line_items if item.free_cash_flow]
    earnings = [item.net_income for item in financial_line_items if item.net_income]

    if fcf_vals and earnings and len(fcf_vals) == len(earnings):
        total_fcf = sum(fcf_vals)
        total_earnings = sum(earnings)

        if total_earnings > 0:
            cash_conversion = total_fcf / total_earnings
            if cash_conversion < 0.6:  # Poor cash conversion
                return 0.8
            elif cash_conversion < 0.8:
                return 0.5

    return 0.3


def analyze_margin_expansion_potential(financial_line_items: list) -> float:
    """Analyze margin expansion potential"""
    margins = [item.operating_margin for item in financial_line_items if item.operating_margin]

    if margins:
        current_margin = margins[0]
        if current_margin < 0.1:  # <10% operating margin
            return 0.7  # High margin expansion potential
        elif current_margin < 0.15:  # <15% operating margin
            return 0.5  # Moderate margin expansion potential

    return 0.2


def calculate_sum_of_parts_discount(financial_line_items: list, market_cap: float) -> float:
    """Calculate sum-of-the-parts discount (simplified)"""
    # This would typically require detailed business segment analysis
    # Simplified approach based on total assets vs market cap

    if not financial_line_items or not market_cap:
        return 0

    latest = financial_line_items[0]
    if latest.total_assets:
        # Assume asset-based valuation at 1.2x book value
        estimated_sotp_value = latest.total_assets * 1.2
        if estimated_sotp_value > market_cap:
            discount = (estimated_sotp_value - market_cap) / market_cap
            return min(discount, 0.5)  # Cap at 50%

    return 0


def assess_management_performance(metrics: list, financial_line_items: list) -> float:
    """Assess management performance"""
    if not metrics:
        return 0.5

    # ROE trend analysis
    roe_vals = [m.return_on_equity for m in metrics if m.return_on_equity]
    if len(roe_vals) >= 3:
        recent_roe = sum(roe_vals[:2]) / 2
        historical_roe = sum(roe_vals[2:]) / len(roe_vals[2:])

        if recent_roe > historical_roe * 1.2:
            return 0.8  # Improving performance
        elif recent_roe < historical_roe * 0.8:
            return 0.3  # Declining performance

    # Revenue growth consistency
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 4:
        growth_years = sum(1 for i in range(len(revenues) - 1) if revenues[i] >= revenues[i + 1])
        consistency = growth_years / (len(revenues) - 1)
        return consistency

    return 0.5


def analyze_board_independence_proxy(financial_line_items: list) -> float:
    """Analyze board independence (proxy through governance metrics)"""
    # In practice, would analyze board composition data
    # Proxy: companies with consistent performance likely have better governance

    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings:
        positive_years = sum(1 for e in earnings if e > 0)
        return positive_years / len(earnings)

    return 0.6  # Default moderate score


def analyze_executive_compensation_proxy(financial_line_items: list) -> float:
    """Analyze executive compensation alignment (proxy)"""
    # Proxy: FCF growth vs revenue growth alignment
    fcf_vals = [item.free_cash_flow for item in financial_line_items if item.free_cash_flow]
    revenues = [item.revenue for item in financial_line_items if item.revenue]

    if len(fcf_vals) >= 3 and len(revenues) >= 3:
        fcf_growth = (fcf_vals[0] / fcf_vals[-1]) - 1
        revenue_growth = (revenues[0] / revenues[-1]) - 1

        # Good alignment if FCF grows faster than revenue
        if fcf_growth > revenue_growth:
            return 0.8
        elif fcf_growth > revenue_growth * 0.8:
            return 0.6

    return 0.4  # Poor alignment assumed


def assess_shareholder_rights_proxy(market_cap: float) -> float:
    """Assess shareholder rights (proxy through market cap)"""
    # Larger companies typically have better shareholder rights
    if market_cap > 10_000_000_000:  # $10B+
        return 0.8
    elif market_cap > 1_000_000_000:  # $1B+
        return 0.6
    else:
        return 0.4


def analyze_takeover_defenses_proxy(market_cap: float) -> float:
    """Analyze takeover defenses (proxy)"""
    # Smaller companies typically have fewer defenses
    if market_cap < 5_000_000_000:  # <$5B
        return 0.3  # Limited defenses
    elif market_cap < 20_000_000_000:  # <$20B
        return 0.6  # Moderate defenses
    else:
        return 0.8  # Strong defenses


def identify_strategic_alternatives(financial_line_items: list, market_cap: float) -> float:
    """Identify strategic alternatives potential"""
    if not financial_line_items:
        return 0

    latest = financial_line_items[0]

    # Look for conglomerate discount or diversified business
    # Proxy: high asset base relative to market cap suggests complex structure
    if latest.total_assets and market_cap:
        asset_ratio = latest.total_assets / market_cap
        if asset_ratio > 3:  # Highly asset-intensive
            return 0.8
        elif asset_ratio > 2:
            return 0.6

    return 0.4


def assess_operational_turnaround_potential(financial_line_items: list) -> float:
    """Assess operational turnaround potential"""
    # Look for declining margins with stable/growing revenue
    margins = [item.operating_margin for item in financial_line_items if item.operating_margin]
    revenues = [item.revenue for item in financial_line_items if item.revenue]

    if len(margins) >= 3 and len(revenues) >= 3:
        margin_trend = margins[0] - margins[-1]  # Recent vs historical
        revenue_stability = revenues[0] / revenues[-1]  # Growth ratio

        if margin_trend < -0.05 and revenue_stability > 0.9:  # Declining margins, stable revenue
            return 0.8
        elif margin_trend < 0 and revenue_stability > 0.8:
            return 0.6

    return 0.3


def analyze_balance_sheet_optimization(financial_line_items: list) -> float:
    """Analyze balance sheet optimization opportunities"""
    if not financial_line_items:
        return 0

    latest = financial_line_items[0]

    # Look for excess cash or conservative capital structure
    if latest.current_assets and latest.current_liabilities:
        excess_liquidity = latest.current_assets - latest.current_liabilities
        if latest.total_assets and excess_liquidity > latest.total_assets * 0.2:
            return 0.8  # Significant excess cash

    # Low debt utilization
    if latest.total_debt and latest.shareholders_equity:
        debt_ratio = latest.total_debt / latest.shareholders_equity
        if debt_ratio < 0.3:  # Very conservative
            return 0.6

    return 0.3


def analyze_market_position_catalyst(financial_line_items: list) -> float:
    """Analyze market position improvement catalyst"""
    # Look for companies with declining market share (revenue issues)
    revenues = [item.revenue for item in financial_line_items if item.revenue]

    if len(revenues) >= 4:
        recent_decline = revenues[0] < revenues[1]
        historical_strength = max(revenues[2:]) > revenues[0] * 1.2

        if recent_decline and historical_strength:
            return 0.7  # Lost market position, recovery potential

    return 0.4


def assess_management_change_potential(metrics: list, financial_line_items: list) -> float:
    """Assess management change potential"""
    # Poor performance metrics suggest management change opportunity
    management_score = assess_management_performance(metrics, financial_line_items)

    if management_score < 0.3:
        return 0.8  # Strong case for management change
    elif management_score < 0.5:
        return 0.6  # Moderate case for management change

    return 0.2


def identify_esg_improvement_opportunities(financial_line_items: list) -> float:
    """Identify ESG improvement opportunities"""
    # Proxy through R&D spending and operational efficiency
    rd_vals = [item.research_and_development for item in financial_line_items if item.research_and_development]
    revenues = [item.revenue for item in financial_line_items if item.revenue]

    if rd_vals and revenues:
        rd_intensity = sum(rd_vals) / sum(revenues[:len(rd_vals)])
        if rd_intensity < 0.02:  # <2% R&D intensity
            return 0.6  # ESG improvement opportunity through innovation

    return 0.4


def assess_ma_probability(financial_line_items: list, market_cap: float) -> float:
    """Assess M&A probability"""
    if not financial_line_items or not market_cap:
        return 0

    latest = financial_line_items[0]

    # Attractive M&A targets: strong assets, reasonable valuation
    if latest.total_assets and latest.total_debt:
        net_assets = latest.total_assets - latest.total_debt
        asset_multiple = market_cap / net_assets if net_assets > 0 else 0

        if 0.8 <= asset_multiple <= 1.5:  # Reasonable valuation
            return 0.7
        elif asset_multiple < 0.8:  # Cheap
            return 0.6

    return 0.3


def analyze_spinoff_potential(financial_line_items: list) -> float:
    """Analyze spin-off potential"""
    # Look for complex business structures (high assets, multiple segments implied)
    if not financial_line_items:
        return 0

    latest = financial_line_items[0]

    # Proxy: high asset base suggests complex business
    if latest.total_assets and latest.revenue:
        asset_intensity = latest.total_assets / latest.revenue
        if asset_intensity > 3:  # Very asset-intensive
            return 0.8
        elif asset_intensity > 2:
            return 0.6

    return 0.3


def analyze_special_dividend_potential(financial_line_items: list) -> float:
    """Analyze special dividend potential"""
    # Look for excess cash generation
    fcf_vals = [item.free_cash_flow for item in financial_line_items if item.free_cash_flow]

    if fcf_vals:
        avg_fcf = sum(fcf_vals) / len(fcf_vals)
        if avg_fcf > 0:
            # Check for cash accumulation
            latest = financial_line_items[0]
            if latest.current_assets:
                # Proxy for cash accumulation
                return min(avg_fcf / (latest.current_assets * 0.1), 1.0)

    return 0.3


def analyze_distressed_opportunity(financial_line_items: list) -> float:
    """Analyze distressed investment opportunity"""
    # Look for financial distress signals
    if not financial_line_items:
        return 0

    latest = financial_line_items[0]

    # High debt, low liquidity
    if latest.total_debt and latest.current_assets and latest.current_liabilities:
        debt_burden = latest.total_debt / latest.current_assets if latest.current_assets > 0 else 10
        liquidity_ratio = latest.current_assets / latest.current_liabilities

        if debt_burden > 2 and liquidity_ratio < 1.2:
            return 0.7  # Distressed situation
        elif debt_burden > 1.5 or liquidity_ratio < 1.0:
            return 0.5  # Some distress

    return 0.2


def identify_regulatory_catalysts(financial_line_items: list) -> float:
    """Identify regulatory catalysts"""
    # Industry-specific regulatory changes (simplified)
    return 0.4  # Placeholder - would analyze specific industry regulations


def assess_litigation_catalyst_potential(financial_line_items: list) -> float:
    """Assess litigation catalyst potential"""
    # Look for companies with potential legal value creation
    return 0.3  # Placeholder - would analyze specific legal situations


def assess_campaign_complexity(activism_team: dict, legal_team: dict) -> float:
    """Assess activist campaign complexity"""
    governance_issues = len(legal_team.get("governance_issues", []))
    activist_catalysts = len(activism_team.get("activist_catalysts", []))

    # More issues = more complex campaign
    complexity_score = (governance_issues * 0.15) + (activist_catalysts * 0.1)

    return min(complexity_score, 1.0)


def generate_elliott_output(ticker: str, analysis_data: dict, state: AgentState, agent_id: str) -> ElliottSignal:
    """Generate Elliott Management's activist investment decision"""
    template = ChatPromptTemplate.from_messages([
        ("system", """You are Elliott Management's AI system, implementing Paul Singer's activist hedge fund approach:

        ORGANIZATIONAL STRUCTURE:
        - Research Team: Fundamental analysis and hidden value identification
        - Legal Team: Corporate governance and shareholder rights analysis
        - Activism Team: Catalyst identification and campaign strategy
        - Event-Driven Team: M&A, spin-offs, and special situations
        - Paul Singer Strategic Decision: Overall campaign and value creation strategy

        PHILOSOPHY:
        1. Catalyst-Driven Investing: Focus on specific catalysts for value creation
        2. Shareholder Activism: Active engagement to unlock shareholder value
        3. Event-Driven Opportunities: M&A arbitrage, spin-offs, special situations
        4. Governance Improvement: Board changes, management accountability
        5. Strategic Alternatives: Spin-offs, divestitures, strategic sales
        6. Operational Improvements: Margin expansion, cost reduction
        7. Balance Sheet Optimization: Capital allocation, leverage, dividends

        REASONING STYLE:
        - Identify specific catalysts and value creation opportunities
        - Assess management performance and governance issues
        - Analyze strategic alternatives and operational improvements
        - Consider campaign complexity and execution timeline
        - Express conviction in activist value creation potential
        - Discuss legal and governance framework for activism
        - Focus on risk-adjusted returns from catalyst realization

        Return investment signal with detailed activist potential analysis."""),

        ("human", """Apply Elliott Management's activist analysis to {ticker}:

        {analysis_data}

        Provide investment signal in JSON format:
        {{
          "signal": "bullish" | "bearish" | "neutral",
          "confidence": float (0-100),
          "reasoning": "string",
          "activist_potential": {{
            "catalyst_strength": float,
            "campaign_complexity": float,
            "expected_timeline": "string",
            "value_creation_potential": float,
            "governance_opportunity": float
          }}
        }}""")
    ])

    prompt = template.invoke({"analysis_data": json.dumps(analysis_data, indent=2), "ticker": ticker})

    def create_default_elliott_signal():
        return ElliottSignal(
            signal="neutral",
            confidence=0.0,
            reasoning="Analysis error, defaulting to neutral",
            activist_potential={
                "catalyst_strength": 0.5,
                "campaign_complexity": 0.5,
                "expected_timeline": "12-18 months",
                "value_creation_potential": 0.5,
                "governance_opportunity": 0.5
            }
        )

    return call_llm(
        prompt=prompt,
        pydantic_model=ElliottSignal,
        agent_name=agent_id,
        state=state,
        default_factory=create_default_elliott_signal,
    )