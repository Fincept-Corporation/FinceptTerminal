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


class PershingSquareSignal(BaseModel):
    signal: Literal["bullish", "bearish", "neutral"]
    confidence: float
    reasoning: str
    investment_thesis: dict


def pershing_square_hedge_fund_agent(state: AgentState, agent_id: str = "pershing_square_hedge_fund_agent"):
    """
    Pershing Square: Concentrated activist investing, high-conviction bets
    Structure: Research → Activism Strategy → Public Relations → Risk Management → Bill Ackman Decision
    Philosophy: Concentrated positions, activist value creation, public campaigns
    """
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    api_key = get_api_key_from_state(state, "FINANCIAL_DATASETS_API_KEY")
    analysis_data = {}
    pershing_analysis = {}

    for ticker in tickers:
        progress.update_status(agent_id, ticker, "Research team fundamental analysis")

        metrics = get_financial_metrics(ticker, end_date, period="annual", limit=8, api_key=api_key)
        financial_line_items = search_line_items(
            ticker,
            [
                "revenue", "net_income", "free_cash_flow", "total_debt",
                "shareholders_equity", "operating_margin", "total_assets",
                "current_assets", "current_liabilities", "research_and_development"
            ],
            end_date,
            period="annual",
            limit=8,
            api_key=api_key,
        )
        market_cap = get_market_cap(ticker, end_date, api_key=api_key)

        # Pershing Square's concentrated analysis structure
        progress.update_status(agent_id, ticker, "Research team analysis")
        research_team = pershing_research_team_analysis(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Activism strategy team")
        activism_strategy = activism_strategy_team_analysis(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Public relations strategy")
        public_relations = public_relations_strategy_analysis(financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Risk management analysis")
        risk_management = pershing_risk_management_analysis(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Bill Ackman investment decision")
        ackman_decision = bill_ackman_investment_decision(
            research_team, activism_strategy, public_relations, risk_management
        )

        # Pershing Square's high-conviction scoring
        total_score = (
                research_team["score"] * 0.35 +
                activism_strategy["score"] * 0.3 +
                risk_management["score"] * 0.2 +
                public_relations["score"] * 0.15
        )

        # Pershing Square's concentrated decision framework
        conviction_level = ackman_decision.get("conviction_level", 0)
        if total_score >= 8.5 and conviction_level > 0.8:
            signal = "bullish"
        elif total_score <= 4.0 or conviction_level < 0.3:
            signal = "bearish"
        else:
            signal = "neutral"

        analysis_data[ticker] = {
            "signal": signal,
            "score": total_score,
            "research_team": research_team,
            "activism_strategy": activism_strategy,
            "public_relations": public_relations,
            "risk_management": risk_management,
            "ackman_decision": ackman_decision
        }

        pershing_output = generate_pershing_output(ticker, analysis_data, state, agent_id)

        pershing_analysis[ticker] = {
            "signal": pershing_output.signal,
            "confidence": pershing_output.confidence,
            "reasoning": pershing_output.reasoning,
            "investment_thesis": pershing_output.investment_thesis
        }

        progress.update_status(agent_id, ticker, "Done", analysis=pershing_output.reasoning)

    message = HumanMessage(content=json.dumps(pershing_analysis), name=agent_id)

    if state["metadata"]["show_reasoning"]:
        show_agent_reasoning(pershing_analysis, "Pershing Square")

    state["data"]["analyst_signals"][agent_id] = pershing_analysis
    progress.update_status(agent_id, None, "Done")

    return {"messages": [message], "data": state["data"]}


def pershing_research_team_analysis(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Pershing Square's research team deep fundamental analysis"""
    score = 0
    details = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No research data"}

    # High-quality business analysis
    business_quality = analyze_business_quality_metrics(metrics, financial_line_items)
    if business_quality > 0.8:
        score += 3
        details.append(f"Exceptional business quality: {business_quality:.2f}")
    elif business_quality > 0.6:
        score += 2
        details.append(f"High business quality: {business_quality:.2f}")

    # Competitive moat analysis
    moat_strength = analyze_competitive_moat_strength(metrics, financial_line_items)
    if moat_strength > 0.7:
        score += 3
        details.append(f"Strong competitive moat: {moat_strength:.2f}")

    # Management quality assessment
    management_quality = assess_management_quality(metrics, financial_line_items)
    if management_quality > 0.7:
        score += 2
        details.append(f"High-quality management: {management_quality:.2f}")
    elif management_quality < 0.4:
        score += 1.5  # Opportunity for improvement through activism
        details.append("Management improvement opportunity")

    # Intrinsic value vs market price
    intrinsic_value_ratio = calculate_intrinsic_value_ratio(financial_line_items, market_cap)
    if intrinsic_value_ratio > 1.5:
        score += 2.5
        details.append(f"Significant undervaluation: {intrinsic_value_ratio:.1f}x intrinsic value")
    elif intrinsic_value_ratio > 1.2:
        score += 1.5
        details.append(f"Moderate undervaluation: {intrinsic_value_ratio:.1f}x intrinsic value")

    # Long-term growth potential
    growth_potential = analyze_long_term_growth_potential(financial_line_items)
    if growth_potential > 0.6:
        score += 1.5
        details.append(f"Strong long-term growth potential: {growth_potential:.2f}")

    return {"score": score, "details": "; ".join(details)}


def activism_strategy_team_analysis(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Pershing Square's activism strategy team analysis"""
    score = 0
    details = []
    activist_opportunities = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No activism data"}

    # Board and governance opportunities
    governance_opportunity = assess_governance_improvement_opportunities(metrics, financial_line_items)
    if governance_opportunity > 0.7:
        score += 3
        activist_opportunities.append("Board restructuring")
        details.append("Significant governance improvement opportunity")

    # Strategic restructuring potential
    restructuring_potential = identify_strategic_restructuring_opportunities(financial_line_items, market_cap)
    if restructuring_potential > 0.6:
        score += 2.5
        activist_opportunities.append("Strategic restructuring")
        details.append("Strategic restructuring opportunity identified")

    # Capital allocation improvements
    capital_allocation_score = analyze_capital_allocation_opportunities(financial_line_items)
    if capital_allocation_score > 0.6:
        score += 2
        activist_opportunities.append("Capital allocation")
        details.append("Capital allocation improvement opportunity")

    # Operational efficiency improvements
    operational_efficiency = identify_operational_efficiency_opportunities(financial_line_items)
    if operational_efficiency > 0.5:
        score += 1.5
        activist_opportunities.append("Operational improvements")
        details.append("Operational efficiency opportunities")

    # ESG and sustainability improvements
    esg_opportunities = assess_esg_improvement_opportunities(financial_line_items)
    if esg_opportunities > 0.5:
        score += 1
        activist_opportunities.append("ESG improvements")
        details.append("ESG improvement opportunities")

    # Public campaign viability
    public_campaign_viability = assess_public_campaign_viability(market_cap)
    if public_campaign_viability > 0.7:
        score += 1
        details.append("High public campaign viability")

    return {
        "score": score,
        "details": "; ".join(details),
        "activist_opportunities": activist_opportunities
    }


def public_relations_strategy_analysis(financial_line_items: list, market_cap: float) -> dict:
    """Pershing Square's public relations strategy analysis"""
    score = 0
    details = []

    if not financial_line_items or not market_cap:
        return {"score": 0, "details": "No PR data"}

    # Public narrative strength
    narrative_strength = assess_public_narrative_strength(financial_line_items)
    if narrative_strength > 0.7:
        score += 2
        details.append("Strong public narrative potential")

    # Media coverage potential
    media_coverage_potential = analyze_media_coverage_potential(market_cap)
    if media_coverage_potential > 0.6:
        score += 1.5
        details.append("High media coverage potential")

    # Shareholder communication effectiveness
    shareholder_communication = assess_shareholder_communication_potential(market_cap)
    score += shareholder_communication
    details.append(f"Shareholder communication score: {shareholder_communication:.1f}")

    # Public pressure effectiveness
    public_pressure_effectiveness = evaluate_public_pressure_effectiveness(financial_line_items)
    if public_pressure_effectiveness > 0.6:
        score += 1.5
        details.append("Public pressure likely to be effective")

    # Regulatory and political considerations
    regulatory_considerations = analyze_regulatory_environment(financial_line_items)
    if regulatory_considerations > 0.5:
        score += 1
        details.append("Favorable regulatory environment")

    return {"score": score, "details": "; ".join(details)}


def pershing_risk_management_analysis(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Pershing Square's risk management for concentrated positions"""
    score = 0
    details = []
    risk_factors = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No risk data"}

    # Concentration risk assessment
    concentration_risk = assess_concentration_risk(market_cap)
    if concentration_risk < 0.3:
        score += 2
        details.append("Low concentration risk")
    elif concentration_risk > 0.7:
        risk_factors.append("High concentration risk")
        details.append("High concentration risk identified")

    # Liquidity risk for large positions
    liquidity_risk = assess_large_position_liquidity_risk(market_cap)
    if liquidity_risk < 0.2:
        score += 2
        details.append("Low liquidity risk for large positions")
    elif liquidity_risk > 0.6:
        risk_factors.append("Liquidity constraints")

    # Activist campaign risks
    campaign_risk = assess_activist_campaign_risks(financial_line_items)
    if campaign_risk < 0.4:
        score += 1.5
        details.append("Low activist campaign risk")
    elif campaign_risk > 0.7:
        risk_factors.append("High campaign execution risk")

    # Regulatory and legal risks
    regulatory_risk = assess_regulatory_and_legal_risks(market_cap)
    if regulatory_risk < 0.3:
        score += 1.5
        details.append("Low regulatory risk")
    elif regulatory_risk > 0.6:
        risk_factors.append("Regulatory concerns")

    # Reputational risk
    reputational_risk = assess_reputational_risk_exposure(financial_line_items)
    if reputational_risk < 0.4:
        score += 1
        details.append("Low reputational risk")
    elif reputational_risk > 0.7:
        risk_factors.append("Reputational risk exposure")

    # Downside protection analysis
    downside_protection = calculate_downside_protection(financial_line_items, market_cap)
    if downside_protection > 0.6:
        score += 2
        details.append("Strong downside protection")

    if risk_factors:
        details.append(f"Risk factors: {'; '.join(risk_factors)}")

    return {
        "score": score,
        "details": "; ".join(details),
        "risk_factors": risk_factors
    }


def bill_ackman_investment_decision(research_team: dict, activism_strategy: dict,
                                    public_relations: dict, risk_management: dict) -> dict:
    """Bill Ackman's high-conviction investment decision"""
    details = []

    # Ackman's decision framework
    fundamental_strength = research_team["score"] / 10
    activist_potential = activism_strategy["score"] / 10
    campaign_feasibility = public_relations["score"] / 8
    risk_acceptability = risk_management["score"] / 10

    # High-conviction threshold
    overall_conviction = (
            fundamental_strength * 0.4 +
            activist_potential * 0.35 +
            campaign_feasibility * 0.15 +
            risk_acceptability * 0.1
    )

    # Position sizing decision
    if overall_conviction > 0.85:
        position_size = "Large concentrated position (15-25%)"
        conviction_level = 0.9
        details.append("Exceptional opportunity warranting large concentrated position")
    elif overall_conviction > 0.7:
        position_size = "Significant position (8-15%)"
        conviction_level = 0.75
        details.append("High-conviction opportunity with strong activist potential")
    elif overall_conviction > 0.55:
        position_size = "Moderate position (3-8%)"
        conviction_level = 0.6
        details.append("Decent opportunity but below Pershing Square's typical standards")
    else:
        position_size = "Pass - insufficient conviction"
        conviction_level = 0.3
        details.append("Does not meet Pershing Square's high-conviction threshold")

    # Timeline estimation
    activist_opportunities = activism_strategy.get("activist_opportunities", [])
    if "Board restructuring" in activist_opportunities:
        expected_timeline = "18-36 months"
    elif "Strategic restructuring" in activist_opportunities:
        expected_timeline = "24-48 months"
    elif "Capital allocation" in activist_opportunities:
        expected_timeline = "12-24 months"
    else:
        expected_timeline = "6-18 months"

    # Expected returns
    if conviction_level > 0.8:
        expected_return_range = "25-50% annualized"
    elif conviction_level > 0.6:
        expected_return_range = "15-25% annualized"
    else:
        expected_return_range = "5-15% annualized"

    return {
        "position_size": position_size,
        "conviction_level": conviction_level,
        "expected_timeline": expected_timeline,
        "expected_return_range": expected_return_range,
        "overall_conviction": overall_conviction,
        "details": "; ".join(details)
    }


# Helper functions for Pershing Square analysis
def analyze_business_quality_metrics(metrics: list, financial_line_items: list) -> float:
    """Analyze business quality metrics"""
    if not metrics:
        return 0.5

    quality_score = 0
    components = 0

    # Return on equity consistency
    roe_vals = [m.return_on_equity for m in metrics if m.return_on_equity]
    if roe_vals:
        avg_roe = sum(roe_vals) / len(roe_vals)
        high_roe_years = sum(1 for roe in roe_vals if roe > 0.15)
        roe_consistency = high_roe_years / len(roe_vals)

        quality_score += min(avg_roe / 0.2, 1.0) * 0.5 + roe_consistency * 0.5
        components += 1

    # Free cash flow generation
    fcf_vals = [item.free_cash_flow for item in financial_line_items if item.free_cash_flow]
    if fcf_vals:
        positive_fcf_years = sum(1 for fcf in fcf_vals if fcf > 0)
        fcf_consistency = positive_fcf_years / len(fcf_vals)
        quality_score += fcf_consistency
        components += 1

    # Revenue growth consistency
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 5:
        growth_years = sum(1 for i in range(len(revenues) - 1) if revenues[i] >= revenues[i + 1])
        growth_consistency = growth_years / (len(revenues) - 1)
        quality_score += growth_consistency
        components += 1

    return quality_score / components if components > 0 else 0.5


def analyze_competitive_moat_strength(metrics: list, financial_line_items: list) -> float:
    """Analyze competitive moat strength"""
    if not metrics or not financial_line_items:
        return 0.5

    moat_indicators = 0
    total_indicators = 0

    # High and stable margins
    margins = [item.operating_margin for item in financial_line_items if item.operating_margin]
    if margins:
        avg_margin = sum(margins) / len(margins)
        margin_stability = 1 - ((max(margins) - min(margins)) / max(margins)) if max(margins) > 0 else 0

        if avg_margin > 0.15 and margin_stability > 0.7:
            moat_indicators += 1
        total_indicators += 1

    # Return on invested capital superiority
    if metrics:
        roic_vals = [m.return_on_invested_capital for m in metrics if m.return_on_invested_capital]
        if roic_vals:
            avg_roic = sum(roic_vals) / len(roic_vals)
            if avg_roic > 0.15:  # >15% ROIC
                moat_indicators += 1
            total_indicators += 1

    # Market position strength (proxy through revenue stability)
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 5:
        revenue_volatility = calculate_coefficient_of_variation(revenues)
        if revenue_volatility < 0.2:  # Low revenue volatility
            moat_indicators += 1
        total_indicators += 1

    return moat_indicators / total_indicators if total_indicators > 0 else 0.5


def assess_management_quality(metrics: list, financial_line_items: list) -> float:
    """Assess management quality"""
    if not metrics or not financial_line_items:
        return 0.5

    management_score = 0

    # Capital allocation effectiveness
    shares = [item.outstanding_shares for item in financial_line_items if item.outstanding_shares]
    if len(shares) >= 3:
        share_reduction = (shares[-1] - shares[0]) / shares[-1]
        if share_reduction > 0.1:  # 10%+ share reduction
            management_score += 0.3

    # ROE improvement over time
    if metrics and len(metrics) >= 3:
        roe_vals = [m.return_on_equity for m in metrics if m.return_on_equity]
        if len(roe_vals) >= 3:
            recent_roe = sum(roe_vals[:2]) / 2
            historical_roe = sum(roe_vals[2:]) / len(roe_vals[2:])
            if recent_roe > historical_roe:
                management_score += 0.4

    # Free cash flow vs earnings (capital discipline)
    fcf_vals = [item.free_cash_flow for item in financial_line_items if item.free_cash_flow]
    earnings = [item.net_income for item in financial_line_items if item.net_income]

    if fcf_vals and earnings and len(fcf_vals) == len(earnings):
        fcf_conversion = sum(fcf_vals) / sum(earnings) if sum(earnings) > 0 else 0
        if fcf_conversion > 0.8:
            management_score += 0.3

    return management_score


def calculate_intrinsic_value_ratio(financial_line_items: list, market_cap: float) -> float:
    """Calculate intrinsic value to market cap ratio"""
    if not financial_line_items or not market_cap:
        return 1.0

    latest = financial_line_items[0]

    # DCF-based intrinsic value
    if latest.free_cash_flow and latest.free_cash_flow > 0:
        # Conservative assumptions
        growth_rate = 0.06  # 6% growth
        terminal_growth = 0.03  # 3% terminal growth
        discount_rate = 0.10  # 10% discount rate

        # 10-year DCF
        dcf_value = 0
        for year in range(1, 11):
            future_fcf = latest.free_cash_flow * (1 + growth_rate) ** year
            present_value = future_fcf / ((1 + discount_rate) ** year)
            dcf_value += present_value

        # Terminal value
        terminal_fcf = latest.free_cash_flow * (1 + growth_rate) ** 10
        terminal_value = terminal_fcf * (1 + terminal_growth) / (discount_rate - terminal_growth)
        terminal_pv = terminal_value / ((1 + discount_rate) ** 10)
        dcf_value += terminal_pv

        return dcf_value / market_cap

    return 1.0


def analyze_long_term_growth_potential(financial_line_items: list) -> float:
    """Analyze long-term growth potential"""
    growth_factors = 0

    # Revenue growth trend
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 5:
        revenue_cagr = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1
        if revenue_cagr > 0.08:  # 8%+ revenue CAGR
            growth_factors += 0.3

    # R&D investment
    rd_vals = [item.research_and_development for item in financial_line_items if item.research_and_development]
    if rd_vals and revenues:
        rd_intensity = sum(rd_vals) / sum(revenues[:len(rd_vals)])
        if rd_intensity > 0.05:  # 5%+ R&D intensity
            growth_factors += 0.3

    # Market expansion capability
    latest = financial_line_items[0]
    if latest.current_assets and latest.current_liabilities:
        working_capital_ratio = latest.current_assets / latest.current_liabilities
        if working_capital_ratio > 1.5:  # Strong balance sheet for expansion
            growth_factors += 0.4

    return growth_factors


def assess_governance_improvement_opportunities(metrics: list, financial_line_items: list) -> float:
    """Assess governance improvement opportunities"""
    governance_score = 0

    # Management performance issues
    management_quality = assess_management_quality(metrics, financial_line_items)
    if management_quality < 0.5:
        governance_score += 0.4  # Board change opportunity

    # Capital allocation inefficiencies
    capital_allocation = analyze_capital_allocation_opportunities(financial_line_items)
    if capital_allocation > 0.6:
        governance_score += 0.3

    # Shareholder returns vs performance
    if metrics:
        latest = metrics[0]
        if latest.return_on_equity and latest.return_on_equity < 0.1:  # <10% ROE
            governance_score += 0.3  # Poor performance = governance opportunity

    return governance_score


def identify_strategic_restructuring_opportunities(financial_line_items: list, market_cap: float) -> float:
    """Identify strategic restructuring opportunities"""
    if not financial_line_items or not market_cap:
        return 0

    restructuring_score = 0
    latest = financial_line_items[0]

    # Sum-of-parts opportunity
    if latest.total_assets and latest.total_debt:
        asset_value = latest.total_assets - latest.total_debt
        if asset_value > market_cap * 1.3:  # 30%+ asset value premium
            restructuring_score += 0.4

    # Spin-off potential (proxy through asset complexity)
    if latest.total_assets and latest.revenue:
        asset_intensity = latest.total_assets / latest.revenue
        if asset_intensity > 2.5:  # Asset-heavy suggests complex structure
            restructuring_score += 0.3

    # Operational restructuring need
    margins = [item.operating_margin for item in financial_line_items if item.operating_margin]
    if margins and len(margins) >= 3:
        margin_decline = margins[-1] - margins[0]  # Historical vs current
        if margin_decline > 0.05:  # 5%+ margin decline
            restructuring_score += 0.3

    return restructuring_score


def analyze_capital_allocation_opportunities(financial_line_items: list) -> float:
    """Analyze capital allocation improvement opportunities"""
    capital_score = 0

    # Excess cash accumulation
    latest = financial_line_items[0]
    if latest.current_assets and latest.total_assets:
        cash_proxy = latest.current_assets * 0.3  # Rough cash estimate
        if cash_proxy > latest.total_assets * 0.15:  # >15% cash
            capital_score += 0.3

    # Poor FCF to earnings conversion
    fcf_vals = [item.free_cash_flow for item in financial_line_items if item.free_cash_flow]
    earnings = [item.net_income for item in financial_line_items if item.net_income]

    if fcf_vals and earnings and len(fcf_vals) == len(earnings):
        conversion_ratio = sum(fcf_vals) / sum(earnings) if sum(earnings) > 0 else 0
        if conversion_ratio < 0.7:  # Poor cash conversion
            capital_score += 0.4

    # No share buybacks despite profitability
    shares = [item.outstanding_shares for item in financial_line_items if item.outstanding_shares]
    if len(shares) >= 3:
        share_change = (shares[0] - shares[-1]) / shares[-1]
        if share_change < 0.05 and earnings and sum(earnings) > 0:  # No buybacks despite profit
            capital_score += 0.3

    return capital_score


def identify_operational_efficiency_opportunities(financial_line_items: list) -> float:
    """Identify operational efficiency opportunities"""
    efficiency_score = 0

    # Margin improvement potential
    margins = [item.operating_margin for item in financial_line_items if item.operating_margin]
    if margins:
        current_margin = margins[0]
        best_margin = max(margins)

        if best_margin > current_margin * 1.2:  # 20%+ margin improvement potential
            efficiency_score += 0.4

    # Asset utilization improvement
    if latest.total_assets and latest.revenue:
        asset_turnover = latest.revenue / latest.total_assets
        if asset_turnover < 1.0:  # Low asset utilization
            efficiency_score += 0.3

    # Cost structure optimization
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 3:
        revenue_growth = (revenues[0] / revenues[-1]) - 1
        margin_change = margins[0] - margins[-1] if margins and len(margins) >= 3 else 0

        if revenue_growth > 0 and margin_change < 0:  # Growing revenue, declining margins
            efficiency_score += 0.3

    return efficiency_score


def assess_esg_improvement_opportunities(financial_line_items: list) -> float:
    """Assess ESG improvement opportunities"""
    esg_score = 0

    # Environmental improvements (proxy through R&D)
    rd_vals = [item.research_and_development for item in financial_line_items if item.research_and_development]
    revenues = [item.revenue for item in financial_line_items if item.revenue]

    if rd_vals and revenues:
        rd_intensity = sum(rd_vals) / sum(revenues[:len(rd_vals)])
        if rd_intensity < 0.03:  # Low R&D = ESG improvement opportunity
            esg_score += 0.3

    # Governance improvements (covered in governance section)
    governance_opportunity = assess_governance_improvement_opportunities(metrics, financial_line_items)
    esg_score += governance_opportunity * 0.4

    # Social responsibility opportunities
    # Proxy through employee productivity (simplified)
    if latest.revenue and latest.total_assets:
        productivity_proxy = latest.revenue / latest.total_assets
        if productivity_proxy < 0.8:  # Low productivity
            esg_score += 0.3

    return min(esg_score, 1.0)


def assess_public_campaign_viability(market_cap: float) -> float:
    """Assess public campaign viability"""
    # Larger companies get more media attention
    if market_cap > 50_000_000_000:  # $50B+
        return 0.9
    elif market_cap > 10_000_000_000:  # $10B+
        return 0.8
    elif market_cap > 1_000_000_000:  # $1B+
        return 0.6
    else:
        return 0.3


def assess_public_narrative_strength(financial_line_items: list) -> float:
    """Assess strength of public narrative"""
    # Strong narrative if clear value creation opportunity
    narrative_score = 0

    # Underperformance narrative
    margins = [item.operating_margin for item in financial_line_items if item.operating_margin]
    if margins and len(margins) >= 3:
        margin_decline = margins[-1] - margins[0]
        if margin_decline > 0.03:  # Clear declining performance
            narrative_score += 0.4

    # Value creation potential narrative
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 3:
        revenue_stability = 1 - calculate_coefficient_of_variation(revenues)
        if revenue_stability > 0.7:  # Stable business with improvement potential
            narrative_score += 0.3

    # Asset value narrative
    latest = financial_line_items[0]
    if latest.total_assets and latest.total_debt:
        net_assets = latest.total_assets - latest.total_debt
        # Strong asset base provides narrative foundation
        narrative_score += 0.3

    return narrative_score


def analyze_media_coverage_potential(market_cap: float) -> float:
    """Analyze media coverage potential"""
    # Media coverage correlates with company size and visibility
    if market_cap > 20_000_000_000:  # $20B+
        return 0.8
    elif market_cap > 5_000_000_000:  # $5B+
        return 0.7
    elif market_cap > 1_000_000_000:  # $1B+
        return 0.5
    else:
        return 0.3


def assess_shareholder_communication_potential(market_cap: float) -> float:
    """Assess shareholder communication effectiveness potential"""
    # Larger companies have more institutional shareholders
    return min(market_cap / 20_000_000_000, 1.5)  # Normalize by $20B, cap at 1.5


def evaluate_public_pressure_effectiveness(financial_line_items: list) -> float:
    """Evaluate effectiveness of public pressure"""
    # Public pressure more effective when clear improvement opportunities exist
    pressure_score = 0

    # Clear performance issues
    margins = [item.operating_margin for item in financial_line_items if item.operating_margin]
    if margins:
        current_margin = margins[0]
        if current_margin < 0.1:  # <10% margins
            pressure_score += 0.4

    # Shareholder value destruction
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 3:
        revenue_decline = revenues[0] < revenues[-1]
        if revenue_decline:
            pressure_score += 0.3

    return pressure_score


def analyze_regulatory_environment(financial_line_items: list) -> float:
    """Analyze regulatory environment favorability"""
    # Simplified regulatory analysis
    return 0.6  # Neutral regulatory environment


def assess_concentration_risk(market_cap: float) -> float:
    """Assess concentration risk for large positions"""
    # Larger companies have lower concentration risk
    if market_cap > 100_000_000_000:  # $100B+
        return 0.1
    elif market_cap > 20_000_000_000:  # $20B+
        return 0.2
    elif market_cap > 5_000_000_000:  # $5B+
        return 0.4
    else:
        return 0.7


def assess_large_position_liquidity_risk(market_cap: float) -> float:
    """Assess liquidity risk for large positions"""
    # Liquidity risk inversely related to market cap
    if market_cap > 50_000_000_000:  # $50B+
        return 0.1
    elif market_cap > 10_000_000_000:  # $10B+
        return 0.2
    elif market_cap > 2_000_000_000:  # $2B+
        return 0.4
    else:
        return 0.8


def assess_activist_campaign_risks(financial_line_items: list) -> float:
    """Assess activist campaign execution risks"""
    campaign_risk = 0.3  # Base campaign risk

    # Financial distress increases campaign risk
    latest = financial_line_items[0]
    if latest.current_assets and latest.current_liabilities:
        current_ratio = latest.current_assets / latest.current_liabilities
        if current_ratio < 1.2:
            campaign_risk += 0.3

    # Poor performance reduces campaign risk (easier to make case)
    margins = [item.operating_margin for item in financial_line_items if item.operating_margin]
    if margins and margins[0] < 0.08:  # <8% margins
        campaign_risk -= 0.2

    return max(0, min(campaign_risk, 1.0))


def assess_regulatory_and_legal_risks(market_cap: float) -> float:
    """Assess regulatory and legal risks"""
    # Larger companies face more regulatory scrutiny
    if market_cap > 100_000_000_000:  # $100B+
        return 0.6
    elif market_cap > 20_000_000_000:  # $20B+
        return 0.4
    else:
        return 0.2


def assess_reputational_risk_exposure(financial_line_items: list) -> float:
    """Assess reputational risk exposure"""
    # Reputational risk based on business fundamentals
    risk_score = 0.3  # Base risk

    # Poor performance increases reputational risk
    margins = [item.operating_margin for item in financial_line_items if item.operating_margin]
    if margins and margins[0] < 0.05:  # <5% margins
        risk_score += 0.3

    return risk_score


def calculate_downside_protection(financial_line_items: list, market_cap: float) -> float:
    """Calculate downside protection"""
    if not financial_line_items or not market_cap:
        return 0.3

    protection_score = 0
    latest = financial_line_items[0]

    # Asset backing
    if latest.total_assets and latest.total_debt:
        net_assets = latest.total_assets - latest.total_debt
        if net_assets > market_cap * 0.8:
            protection_score += 0.4

    # Cash generation
    if latest.free_cash_flow and latest.free_cash_flow > 0:
        fcf_yield = latest.free_cash_flow / market_cap
        if fcf_yield > 0.08:  # 8%+ FCF yield
            protection_score += 0.3

    # Business stability
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if revenues:
        revenue_stability = 1 - calculate_coefficient_of_variation(revenues)
        protection_score += revenue_stability * 0.3

    return protection_score


def calculate_coefficient_of_variation(data: list) -> float:
    """Calculate coefficient of variation"""
    if not data or len(data) < 2:
        return 1.0

    mean_val = sum(data) / len(data)
    if mean_val == 0:
        return 1.0

    variance = sum((x - mean_val) ** 2 for x in data) / len(data)
    std_dev = (variance ** 0.5)

    return std_dev / abs(mean_val)


def generate_pershing_output(ticker: str, analysis_data: dict, state: AgentState,
                             agent_id: str) -> PershingSquareSignal:
    """Generate Pershing Square's concentrated activist investment decision"""
    template = ChatPromptTemplate.from_messages([
        ("system", """You are Pershing Square's AI system, implementing Bill Ackman's concentrated activist approach:

        ORGANIZATIONAL STRUCTURE:
        - Research Team: Deep fundamental analysis and intrinsic value calculation
        - Activism Strategy Team: Board changes, strategic restructuring, operational improvements
        - Public Relations Strategy: Media campaigns, shareholder communication, narrative building
        - Risk Management: Concentration risk, liquidity risk, campaign execution risk
        - Bill Ackman Investment Decision: High-conviction concentrated positioning

        PHILOSOPHY:
        1. Concentrated Investing: Large positions (10-25%) in high-conviction ideas
        2. Activist Value Creation: Board changes, strategic improvements, operational efficiency
        3. Public Campaigns: Media engagement and public pressure for change
        4. Long-Term Value Creation: Multi-year investment horizons
        5. Quality Businesses: Strong competitive positions with improvement potential
        6. Intrinsic Value Focus: Significant discount to fair value required
        7. ESG Integration: Environmental, social, and governance improvements

        REASONING STYLE:
        - Express high conviction and concentrated position rationale
        - Discuss specific activist catalysts and value creation opportunities
        - Reference intrinsic value calculations and margin of safety
        - Consider public campaign strategy and media narrative
        - Analyze management quality and governance improvements
        - Assess timeline and expected returns from activist engagement
        - Focus on asymmetric risk/reward from concentrated positions

        Return investment signal with detailed investment thesis and activist strategy."""),

        ("human", """Apply Pershing Square's concentrated activist analysis to {ticker}:

        {analysis_data}

        Provide investment signal in JSON format:
        {{
          "signal": "bullish" | "bearish" | "neutral",
          "confidence": float (0-100),
          "reasoning": "string",
          "investment_thesis": {{
            "intrinsic_value_upside": float,
            "activist_catalysts": ["string"],
            "expected_timeline": "string",
            "position_size_recommendation": "string",
            "public_campaign_strategy": "string"
          }}
        }}""")
    ])

    prompt = template.invoke({"analysis_data": json.dumps(analysis_data, indent=2), "ticker": ticker})

    def create_default_pershing_signal():
        return PershingSquareSignal(
            signal="neutral",
            confidence=0.0,
            reasoning="Analysis error, defaulting to neutral",
            investment_thesis={
                "intrinsic_value_upside": 0.0,
                "activist_catalysts": [],
                "expected_timeline": "12-24 months",
                "position_size_recommendation": "Pass",
                "public_campaign_strategy": "None"
            }
        )

    return call_llm(
        prompt=prompt,
        pydantic_model=PershingSquareSignal,
        agent_name=agent_id,
        state=state,
        default_factory=create_default_pershing_signal,
    )