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


class BridgewaterSignal(BaseModel):
    signal: Literal["bullish", "bearish", "neutral"]
    confidence: float
    reasoning: str
    all_weather_allocation: dict


def bridgewater_associates_agent(state: AgentState, agent_id: str = "bridgewater_associates_agent"):
    """
    Bridgewater Associates: All Weather portfolio, economic machine principles
    Structure: Economic Research → Portfolio Construction → Risk Management → Ray Dalio Final Decision
    Philosophy: Diversification across economic environments, systematic risk parity
    """
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    api_key = get_api_key_from_state(state, "FINANCIAL_DATASETS_API_KEY")
    analysis_data = {}
    bridgewater_analysis = {}

    for ticker in tickers:
        progress.update_status(agent_id, ticker, "Economic research team analyzing macro environment")

        metrics = get_financial_metrics(ticker, end_date, period="annual", limit=10, api_key=api_key)
        financial_line_items = search_line_items(
            ticker,
            [
                "revenue", "net_income", "free_cash_flow", "total_debt",
                "shareholders_equity", "operating_margin", "total_assets",
                "current_assets", "current_liabilities", "interest_expense"
            ],
            end_date,
            period="annual",
            limit=10,
            api_key=api_key,
        )
        market_cap = get_market_cap(ticker, end_date, api_key=api_key)

        # Bridgewater's multi-team analysis structure
        progress.update_status(agent_id, ticker, "Economic research team analysis")
        economic_research = economic_research_team_analysis(metrics, financial_line_items)

        progress.update_status(agent_id, ticker, "Portfolio construction team analysis")
        portfolio_construction = portfolio_construction_team_analysis(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Risk management team analysis")
        risk_management = risk_management_team_analysis(financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "All Weather framework analysis")
        all_weather_analysis = all_weather_framework_analysis(metrics, financial_line_items)

        progress.update_status(agent_id, ticker, "Ray Dalio's synthesis and final decision")
        ray_dalio_synthesis = ray_dalio_final_decision(
            economic_research, portfolio_construction, risk_management, all_weather_analysis
        )

        # Bridgewater's risk-parity weighted scoring
        total_score = (
                economic_research["score"] * 0.3 +
                all_weather_analysis["score"] * 0.25 +
                risk_management["score"] * 0.25 +
                portfolio_construction["score"] * 0.2
        )

        # Bridgewater's systematic approach
        if total_score >= 7.5:
            signal = "bullish"
        elif total_score <= 4.0:
            signal = "bearish"
        else:
            signal = "neutral"

        analysis_data[ticker] = {
            "signal": signal,
            "score": total_score,
            "economic_research": economic_research,
            "portfolio_construction": portfolio_construction,
            "risk_management": risk_management,
            "all_weather_analysis": all_weather_analysis,
            "ray_dalio_synthesis": ray_dalio_synthesis
        }

        bridgewater_output = generate_bridgewater_output(ticker, analysis_data, state, agent_id)

        bridgewater_analysis[ticker] = {
            "signal": bridgewater_output.signal,
            "confidence": bridgewater_output.confidence,
            "reasoning": bridgewater_output.reasoning,
            "all_weather_allocation": bridgewater_output.all_weather_allocation
        }

        progress.update_status(agent_id, ticker, "Done", analysis=bridgewater_output.reasoning)

    message = HumanMessage(content=json.dumps(bridgewater_analysis), name=agent_id)

    if state["metadata"]["show_reasoning"]:
        show_agent_reasoning(bridgewater_analysis, "Bridgewater Associates")

    state["data"]["analyst_signals"][agent_id] = bridgewater_analysis
    progress.update_status(agent_id, None, "Done")

    return {"messages": [message], "data": state["data"]}


def economic_research_team_analysis(metrics: list, financial_line_items: list) -> dict:
    """Bridgewater's economic research team analyzing macro environment impact"""
    score = 0
    details = []

    if not metrics or not financial_line_items:
        return {"score": 0, "details": "No economic data"}

    # Economic cycle positioning
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 5:
        # Growth trend analysis
        recent_growth = (revenues[0] / revenues[2]) ** (1 / 2) - 1  # 2-year CAGR
        longer_growth = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1  # Full period

        # Economic machine principle: Growth and inflation environment
        if 0.02 <= recent_growth <= 0.08:  # Goldilocks growth
            score += 2
            details.append(f"Sustainable growth environment: {recent_growth:.1%} recent growth")
        elif recent_growth > 0.15:  # Overheating risk
            score -= 1
            details.append(f"Potential overheating: {recent_growth:.1%} growth")

    # Debt cycle analysis
    debt_metrics = [m.debt_to_equity for m in metrics if m.debt_to_equity]
    if debt_metrics:
        current_leverage = debt_metrics[0]
        avg_leverage = sum(debt_metrics) / len(debt_metrics)

        # Bridgewater's debt cycle framework
        if current_leverage < avg_leverage * 0.8:  # Deleveraging phase
            score += 1
            details.append("Company in deleveraging phase - defensive positioning")
        elif current_leverage > avg_leverage * 1.3:  # High leverage risk
            score -= 2
            details.append("High leverage in debt cycle - risk concern")

    # Currency and inflation hedging capability
    international_exposure = analyze_international_exposure(financial_line_items)
    if international_exposure > 0.3:  # 30%+ international
        score += 1
        details.append("Natural currency hedging through international exposure")

    # Interest rate sensitivity
    interest_coverage = calculate_interest_coverage(financial_line_items)
    if interest_coverage > 5:  # Low interest rate sensitivity
        score += 1
        details.append(f"Low interest rate risk: {interest_coverage:.1f}x coverage")
    elif interest_coverage < 2:
        score -= 1
        details.append(f"High interest rate sensitivity: {interest_coverage:.1f}x coverage")

    return {"score": score, "details": "; ".join(details)}


def portfolio_construction_team_analysis(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Bridgewater's portfolio construction team analysis"""
    score = 0
    details = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No portfolio construction data"}

    latest_metrics = metrics[0]

    # Risk-adjusted returns
    if latest_metrics.return_on_equity:
        roe_consistency = len([m.return_on_equity for m in metrics if m.return_on_equity and m.return_on_equity > 0.1])
        if roe_consistency >= len(metrics) * 0.8:  # Consistent returns
            score += 2
            details.append(f"Consistent risk-adjusted returns: {roe_consistency}/{len(metrics)} years ROE >10%")

    # Diversification benefits
    revenue_stability = analyze_revenue_diversification(financial_line_items)
    if revenue_stability > 0.8:  # Stable revenue streams
        score += 1
        details.append("Revenue diversification provides portfolio stability")

    # Correlation with economic factors
    earnings_correlation = analyze_economic_correlation(financial_line_items)
    if earnings_correlation < 0.7:  # Low correlation
        score += 2
        details.append("Low correlation with economic cycles - diversification benefit")

    # Liquidity for rebalancing
    if latest_metrics.current_ratio and latest_metrics.current_ratio > 1.5:
        score += 1
        details.append(f"Adequate liquidity for dynamic rebalancing: {latest_metrics.current_ratio:.1f}")

    # Valuation relative to fair value
    if latest_metrics.price_to_earnings and latest_metrics.price_to_earnings < 18:
        score += 1
        details.append(f"Reasonable valuation for portfolio inclusion: {latest_metrics.price_to_earnings:.1f} P/E")

    return {"score": score, "details": "; ".join(details)}


def risk_management_team_analysis(financial_line_items: list, market_cap: float) -> dict:
    """Bridgewater's risk management team analysis"""
    score = 0
    details = []
    risk_factors = []

    if not financial_line_items or not market_cap:
        return {"score": 0, "details": "No risk data"}

    latest = financial_line_items[0]

    # Balance sheet risk
    if latest.total_debt and latest.shareholders_equity:
        leverage_ratio = latest.total_debt / latest.shareholders_equity
        if leverage_ratio < 0.4:  # Low leverage risk
            score += 2
            details.append(f"Low balance sheet risk: {leverage_ratio:.2f} D/E")
        elif leverage_ratio > 1.0:
            risk_factors.append("High leverage risk")
            score -= 2

    # Liquidity risk
    if latest.current_assets and latest.current_liabilities:
        liquidity_ratio = latest.current_assets / latest.current_liabilities
        if liquidity_ratio > 2.0:
            score += 1
            details.append(f"Low liquidity risk: {liquidity_ratio:.1f} current ratio")
        elif liquidity_ratio < 1.2:
            risk_factors.append("Liquidity concerns")
            score -= 1

    # Earnings volatility risk
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings and len(earnings) >= 5:
        earnings_volatility = (max(earnings) - min(earnings)) / max(abs(max(earnings)), abs(min(earnings)))
        if earnings_volatility < 0.5:
            score += 1
            details.append(f"Low earnings volatility: {earnings_volatility:.2f}")
        elif earnings_volatility > 1.0:
            risk_factors.append("High earnings volatility")
            score -= 1

    # Concentration risk
    revenue_concentration = analyze_business_concentration(financial_line_items)
    if revenue_concentration < 0.6:  # Diversified business
        score += 1
        details.append("Low business concentration risk")
    else:
        risk_factors.append("Business concentration risk")

    # Tail risk assessment
    worst_performance = analyze_worst_case_scenarios(financial_line_items)
    if worst_performance > -0.2:  # Limited downside
        score += 2
        details.append("Limited tail risk exposure")

    if risk_factors:
        details.append(f"Risk factors identified: {'; '.join(risk_factors)}")

    return {"score": score, "details": "; ".join(details), "risk_factors": risk_factors}


def all_weather_framework_analysis(metrics: list, financial_line_items: list) -> dict:
    """Bridgewater's All Weather framework analysis"""
    score = 0
    details = []
    environment_scores = {}

    if not metrics or not financial_line_items:
        return {"score": 0, "details": "No All Weather data"}

    # Four economic environments analysis
    # 1. Rising Growth
    growth_performance = analyze_growth_environment_performance(financial_line_items)
    environment_scores["rising_growth"] = growth_performance
    if growth_performance > 0.7:
        score += 2
        details.append("Strong performance in rising growth environment")

    # 2. Falling Growth
    recession_resilience = analyze_recession_resilience(financial_line_items)
    environment_scores["falling_growth"] = recession_resilience
    if recession_resilience > 0.6:
        score += 2
        details.append("Good resilience in falling growth environment")

    # 3. Rising Inflation
    inflation_protection = analyze_inflation_protection(financial_line_items)
    environment_scores["rising_inflation"] = inflation_protection
    if inflation_protection > 0.5:
        score += 1
        details.append("Some inflation protection characteristics")

    # 4. Falling Inflation
    deflation_performance = analyze_deflation_performance(metrics, financial_line_items)
    environment_scores["falling_inflation"] = deflation_performance
    if deflation_performance > 0.6:
        score += 1
        details.append("Good performance in falling inflation environment")

    # All Weather balance score
    environment_balance = min(environment_scores.values()) / max(environment_scores.values())
    if environment_balance > 0.7:  # Balanced across environments
        score += 3
        details.append("Well-balanced performance across economic environments")

    return {
        "score": score,
        "details": "; ".join(details),
        "environment_scores": environment_scores
    }


def ray_dalio_final_decision(economic_research: dict, portfolio_construction: dict,
                             risk_management: dict, all_weather_analysis: dict) -> dict:
    """Ray Dalio's final synthesis and decision"""
    details = []

    # Synthesis of team inputs
    total_team_score = (
                               economic_research["score"] +
                               portfolio_construction["score"] +
                               risk_management["score"] +
                               all_weather_analysis["score"]
                       ) / 4

    # Ray's decision framework
    if total_team_score >= 7:
        decision = "Strong conviction buy"
        details.append("All teams align on strong fundamental and risk-adjusted opportunity")
    elif total_team_score >= 5:
        decision = "Moderate allocation"
        details.append("Mixed signals from teams - moderate position appropriate")
    elif len(risk_management.get("risk_factors", [])) > 2:
        decision = "Risk concerns override"
        details.append("Risk management team identifies too many risk factors")
    else:
        decision = "Pass on opportunity"
        details.append("Insufficient conviction across research teams")

    # Economic machine principles
    if all_weather_analysis.get("environment_scores", {}).get("rising_growth", 0) > 0.8:
        details.append("Strong positioning for current economic environment")

    return {
        "decision": decision,
        "team_alignment": total_team_score,
        "details": "; ".join(details)
    }


# Helper functions for economic analysis
def analyze_international_exposure(financial_line_items: list) -> float:
    """Estimate international exposure (simplified)"""
    # In practice, would analyze geographic revenue breakdown
    return 0.4  # Placeholder - 40% international exposure


def calculate_interest_coverage(financial_line_items: list) -> float:
    """Calculate interest coverage ratio"""
    latest = financial_line_items[0]
    if latest.interest_expense and latest.interest_expense > 0:
        operating_income = latest.net_income + latest.interest_expense  # Simplified
        return operating_income / latest.interest_expense
    return 10.0  # High coverage if no debt


def analyze_revenue_diversification(financial_line_items: list) -> float:
    """Analyze revenue stream diversification"""
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if revenues:
        volatility = (max(revenues) - min(revenues)) / max(revenues)
        return 1 - min(volatility, 1.0)  # Higher score for more stable revenue
    return 0.5


def analyze_economic_correlation(financial_line_items: list) -> float:
    """Analyze correlation with economic cycles (simplified)"""
    return 0.6  # Placeholder correlation coefficient


def analyze_business_concentration(financial_line_items: list) -> float:
    """Analyze business line concentration risk"""
    return 0.5  # Placeholder - would analyze business segments


def analyze_worst_case_scenarios(financial_line_items: list) -> float:
    """Analyze worst-case performance"""
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings:
        worst_earnings = min(earnings)
        avg_earnings = sum(earnings) / len(earnings)
        return worst_earnings / avg_earnings if avg_earnings > 0 else -1.0
    return -0.5


def analyze_growth_environment_performance(financial_line_items: list) -> float:
    """Performance in rising growth environment"""
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 3:
        growth_periods = sum(1 for i in range(len(revenues) - 1) if revenues[i] > revenues[i + 1])
        return growth_periods / (len(revenues) - 1)
    return 0.5


def analyze_recession_resilience(financial_line_items: list) -> float:
    """Resilience in falling growth environment"""
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings:
        negative_years = sum(1 for e in earnings if e < 0)
        return 1 - (negative_years / len(earnings))
    return 0.5


def analyze_inflation_protection(financial_line_items: list) -> float:
    """Inflation protection characteristics"""
    # Would analyze pricing power, asset-light model, etc.
    return 0.5  # Placeholder


def analyze_deflation_performance(metrics: list, financial_line_items: list) -> float:
    """Performance in falling inflation environment"""
    # Would analyze bond-like characteristics, stable cash flows
    return 0.6  # Placeholder


def generate_bridgewater_output(ticker: str, analysis_data: dict, state: AgentState,
                                agent_id: str) -> BridgewaterSignal:
    """Generate Bridgewater's systematic investment decision"""
    template = ChatPromptTemplate.from_messages([
        ("system", """You are Bridgewater Associates' AI system, implementing Ray Dalio's All Weather and economic machine principles:

        ORGANIZATIONAL STRUCTURE:
        - Economic Research Team: Macro environment and debt cycle analysis
        - Portfolio Construction Team: Risk-adjusted returns and diversification
        - Risk Management Team: Comprehensive risk assessment
        - All Weather Framework: Performance across four economic environments
        - Ray Dalio Synthesis: Final decision integration

        PHILOSOPHY:
        1. All Weather: Balanced performance across economic environments (growth/inflation up/down)
        2. Economic Machine: Understanding debt cycles and economic principles
        3. Risk Parity: Equal risk contribution, not equal dollar allocation
        4. Systematic Approach: Remove emotion, follow systematic principles
        5. Diversification: True diversification across uncorrelated return streams
        6. Transparency: Radical transparency in decision-making process

        REASONING STYLE:
        - Reference team analyses and organizational structure
        - Apply economic machine principles to company analysis
        - Consider All Weather framework and environmental balance
        - Discuss risk parity and diversification benefits
        - Synthesize multiple team perspectives
        - Express systematic, principle-based reasoning
        - Consider position sizing based on risk contribution

        Return the investment signal with All Weather allocation recommendations."""),

        ("human", """Apply Bridgewater's systematic analysis to {ticker}:

        {analysis_data}

        Provide investment signal in JSON format:
        {{
          "signal": "bullish" | "bearish" | "neutral",
          "confidence": float (0-100),
          "reasoning": "string",
          "all_weather_allocation": {{
            "rising_growth_weight": float,
            "falling_growth_weight": float,
            "rising_inflation_weight": float,
            "falling_inflation_weight": float
          }}
        }}""")
    ])

    prompt = template.invoke({"analysis_data": json.dumps(analysis_data, indent=2), "ticker": ticker})

    def create_default_bridgewater_signal():
        return BridgewaterSignal(
            signal="neutral",
            confidence=0.0,
            reasoning="Analysis error, defaulting to neutral",
            all_weather_allocation={
                "rising_growth_weight": 0.25,
                "falling_growth_weight": 0.25,
                "rising_inflation_weight": 0.25,
                "falling_inflation_weight": 0.25
            }
        )

    return call_llm(
        prompt=prompt,
        pydantic_model=BridgewaterSignal,
        agent_name=agent_id,
        state=state,
        default_factory=create_default_bridgewater_signal,
    )