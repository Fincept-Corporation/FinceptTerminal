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


class CitadelSignal(BaseModel):
    signal: Literal["bullish", "bearish", "neutral"]
    confidence: float
    reasoning: str
    strategy_allocation: dict


def citadel_hedge_fund_agent(state: AgentState, agent_id: str = "citadel_hedge_fund_agent"):
    """
    Citadel: Multi-strategy quantitative approach
    Structure: Fundamental Research → Quantitative Research → Global Macro → Trading → Ken Griffin Final Decision
    Philosophy: Multi-strategy approach, technological edge, risk management, market making
    """
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    api_key = get_api_key_from_state(state, "FINANCIAL_DATASETS_API_KEY")
    analysis_data = {}
    citadel_analysis = {}

    for ticker in tickers:
        progress.update_status(agent_id, ticker, "Fundamental research team analyzing company")

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

        # Citadel's multi-department analysis
        progress.update_status(agent_id, ticker, "Fundamental research department")
        fundamental_research = fundamental_research_department(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Quantitative research department")
        quantitative_research = quantitative_research_department(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Global macro department")
        global_macro = global_macro_department(metrics, financial_line_items)

        progress.update_status(agent_id, ticker, "Trading department analysis")
        trading_department = trading_department_analysis(financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Risk management oversight")
        risk_management = risk_management_oversight(financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Ken Griffin's multi-strategy synthesis")
        ken_griffin_decision = ken_griffin_multi_strategy_synthesis(
            fundamental_research, quantitative_research, global_macro,
            trading_department, risk_management
        )

        # Citadel's multi-strategy scoring
        total_score = (
                fundamental_research["score"] * 0.25 +
                quantitative_research["score"] * 0.25 +
                trading_department["score"] * 0.2 +
                global_macro["score"] * 0.15 +
                risk_management["score"] * 0.15
        )

        # Citadel's multi-strategy decision framework
        if total_score >= 8.0 and ken_griffin_decision.get("risk_adjusted_return", 0) > 0.15:
            signal = "bullish"
        elif total_score <= 4.0 or len(risk_management.get("risk_flags", [])) > 2:
            signal = "bearish"
        else:
            signal = "neutral"

        analysis_data[ticker] = {
            "signal": signal,
            "score": total_score,
            "fundamental_research": fundamental_research,
            "quantitative_research": quantitative_research,
            "global_macro": global_macro,
            "trading_department": trading_department,
            "risk_management": risk_management,
            "ken_griffin_decision": ken_griffin_decision
        }

        citadel_output = generate_citadel_output(ticker, analysis_data, state, agent_id)

        citadel_analysis[ticker] = {
            "signal": citadel_output.signal,
            "confidence": citadel_output.confidence,
            "reasoning": citadel_output.reasoning,
            "strategy_allocation": citadel_output.strategy_allocation
        }

        progress.update_status(agent_id, ticker, "Done", analysis=citadel_output.reasoning)

    message = HumanMessage(content=json.dumps(citadel_analysis), name=agent_id)

    if state["metadata"]["show_reasoning"]:
        show_agent_reasoning(citadel_analysis, "Citadel")

    state["data"]["analyst_signals"][agent_id] = citadel_analysis
    progress.update_status(agent_id, None, "Done")

    return {"messages": [message], "data": state["data"]}


def fundamental_research_department(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Citadel's fundamental research department analysis"""
    score = 0
    details = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No fundamental data"}

    latest_metrics = metrics[0]

    # Deep value analysis
    if latest_metrics.price_to_book and latest_metrics.price_to_book < 1.5:
        score += 2
        details.append(f"Attractive book value: {latest_metrics.price_to_book:.2f} P/B")

    # Quality metrics
    if latest_metrics.return_on_equity and latest_metrics.return_on_equity > 0.15:
        score += 2
        details.append(f"High quality: {latest_metrics.return_on_equity:.1%} ROE")

    # Growth sustainability
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 5:
        revenue_cagr = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1
        if 0.05 <= revenue_cagr <= 0.25:  # Sustainable growth
            score += 2
            details.append(f"Sustainable growth: {revenue_cagr:.1%} revenue CAGR")

    # Competitive position
    margins = [item.operating_margin for item in financial_line_items if item.operating_margin]
    if margins:
        avg_margin = sum(margins) / len(margins)
        if avg_margin > 0.15:  # Strong competitive moat
            score += 2
            details.append(f"Strong margins: {avg_margin:.1%} average operating margin")

    # Management capital allocation
    fcf_vals = [item.free_cash_flow for item in financial_line_items if item.free_cash_flow]
    if fcf_vals:
        positive_fcf_years = sum(1 for fcf in fcf_vals if fcf > 0)
        if positive_fcf_years >= len(fcf_vals) * 0.8:
            score += 1
            details.append(f"Strong capital allocation: {positive_fcf_years}/{len(fcf_vals)} positive FCF years")

    # Catalyst identification
    if len(revenues) >= 3 and revenues[0] > revenues[1] * 1.1:  # 10%+ recent acceleration
        score += 1
        details.append("Recent business acceleration identified")

    return {"score": score, "details": "; ".join(details)}


def quantitative_research_department(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Citadel's quantitative research department analysis"""
    score = 0
    details = []
    quant_metrics = {}

    if not metrics or not financial_line_items:
        return {"score": 0, "details": "No quant data"}

    # Factor model analysis
    factor_loadings = calculate_factor_loadings(metrics, financial_line_items)
    quant_metrics["factors"] = factor_loadings

    if factor_loadings.get("value_factor", 0) > 0.6:
        score += 2
        details.append("Strong value factor loading")

    if factor_loadings.get("quality_factor", 0) > 0.7:
        score += 2
        details.append("High quality factor exposure")

    # Statistical signals
    mean_reversion_signal = calculate_mean_reversion_score(metrics)
    quant_metrics["mean_reversion"] = mean_reversion_signal
    if abs(mean_reversion_signal) > 0.6:
        score += 1.5
        details.append(f"Strong mean reversion signal: {mean_reversion_signal:.2f}")

    # Momentum analysis
    momentum_score = calculate_momentum_score(financial_line_items)
    quant_metrics["momentum"] = momentum_score
    if momentum_score > 0.6:
        score += 1.5
        details.append(f"Positive momentum: {momentum_score:.2f}")

    # Volatility analysis
    volatility_score = analyze_volatility_patterns(financial_line_items)
    quant_metrics["volatility"] = volatility_score
    if volatility_score < 0.4:  # Low volatility is good
        score += 1
        details.append("Low volatility profile")

    # Cross-sectional ranking
    percentile_rank = calculate_cross_sectional_rank(metrics)
    quant_metrics["percentile_rank"] = percentile_rank
    if percentile_rank > 0.8:  # Top 20%
        score += 2
        details.append(f"Top quintile ranking: {percentile_rank:.1%}")

    return {
        "score": score,
        "details": "; ".join(details),
        "quant_metrics": quant_metrics
    }


def global_macro_department(metrics: list, financial_line_items: list) -> dict:
    """Citadel's global macro department analysis"""
    score = 0
    details = []

    if not metrics or not financial_line_items:
        return {"score": 0, "details": "No macro data"}

    # Economic cycle positioning
    latest = financial_line_items[0]

    # Interest rate sensitivity
    if latest.total_debt and latest.total_assets:
        debt_ratio = latest.total_debt / latest.total_assets
        if debt_ratio < 0.3:  # Low interest rate sensitivity
            score += 2
            details.append(f"Low interest rate risk: {debt_ratio:.1%} debt/assets")
        elif debt_ratio > 0.6:
            score -= 1
            details.append(f"High interest rate sensitivity: {debt_ratio:.1%} debt/assets")

    # Currency exposure analysis
    international_revenue_proxy = analyze_international_exposure(financial_line_items)
    if 0.2 <= international_revenue_proxy <= 0.6:  # Balanced exposure
        score += 1
        details.append("Balanced international exposure")

    # Inflation hedging capability
    pricing_power = analyze_pricing_power(financial_line_items)
    if pricing_power > 0.7:
        score += 2
        details.append("Strong pricing power - inflation hedge")

    # Economic cycle resilience
    recession_resilience = analyze_recession_performance(financial_line_items)
    if recession_resilience > 0.6:
        score += 1
        details.append("Good recession resilience")

    # Commodity exposure
    commodity_sensitivity = analyze_commodity_exposure(financial_line_items)
    if commodity_sensitivity < 0.3:  # Low commodity risk
        score += 1
        details.append("Low commodity price sensitivity")

    return {"score": score, "details": "; ".join(details)}


def trading_department_analysis(financial_line_items: list, market_cap: float) -> dict:
    """Citadel's trading department analysis"""
    score = 0
    details = []

    if not financial_line_items or not market_cap:
        return {"score": 0, "details": "No trading data"}

    # Liquidity analysis
    liquidity_score = assess_trading_liquidity(market_cap)
    if liquidity_score > 0.8:
        score += 3
        details.append("High liquidity for large position sizes")
    elif liquidity_score > 0.6:
        score += 2
        details.append("Adequate liquidity for trading")

    # Market making opportunities
    volatility = estimate_trading_volatility(financial_line_items)
    if 0.15 <= volatility <= 0.35:  # Optimal volatility for market making
        score += 2
        details.append(f"Good market making opportunity: {volatility:.1%} volatility")

    # Execution efficiency
    market_impact = estimate_market_impact_cost(market_cap)
    if market_impact < 0.02:  # Low market impact
        score += 2
        details.append(f"Low execution costs: {market_impact:.2%} market impact")

    # Options market opportunities
    options_opportunity = analyze_options_opportunities(financial_line_items, market_cap)
    if options_opportunity > 0.6:
        score += 1
        details.append("Good options trading opportunities")

    # High-frequency trading signals
    hft_signals = analyze_hft_opportunities(financial_line_items)
    if hft_signals > 0.5:
        score += 1
        details.append("Positive high-frequency signals")

    return {"score": score, "details": "; ".join(details)}


def risk_management_oversight(financial_line_items: list, market_cap: float) -> dict:
    """Citadel's risk management oversight"""
    score = 0
    details = []
    risk_flags = []

    if not financial_line_items or not market_cap:
        return {"score": 0, "details": "No risk data"}

    latest = financial_line_items[0]

    # Leverage risk
    if latest.total_debt and latest.shareholders_equity:
        leverage = latest.total_debt / latest.shareholders_equity
        if leverage < 0.5:
            score += 2
            details.append(f"Conservative leverage: {leverage:.2f} D/E")
        elif leverage > 1.5:
            risk_flags.append("High leverage risk")
            score -= 2

    # Liquidity risk
    if latest.current_assets and latest.current_liabilities:
        current_ratio = latest.current_assets / latest.current_liabilities
        if current_ratio > 1.5:
            score += 1
            details.append(f"Strong liquidity: {current_ratio:.1f} current ratio")
        elif current_ratio < 1.0:
            risk_flags.append("Liquidity concerns")
            score -= 1

    # Earnings quality risk
    earnings_quality = assess_earnings_quality(financial_line_items)
    if earnings_quality > 0.7:
        score += 2
        details.append("High earnings quality")
    elif earnings_quality < 0.4:
        risk_flags.append("Earnings quality concerns")
        score -= 1

    # Concentration risk
    business_concentration = assess_business_concentration_risk(financial_line_items)
    if business_concentration < 0.6:
        score += 1
        details.append("Diversified business model")
    elif business_concentration > 0.8:
        risk_flags.append("High business concentration")

    # Tail risk assessment
    tail_risk = calculate_tail_risk_metrics(financial_line_items)
    if tail_risk < 0.3:
        score += 2
        details.append("Low tail risk profile")
    elif tail_risk > 0.7:
        risk_flags.append("High tail risk")
        score -= 1

    # Regulatory risk
    regulatory_risk = assess_regulatory_risk(financial_line_items)
    if regulatory_risk < 0.4:
        score += 1
        details.append("Low regulatory risk")

    if risk_flags:
        details.append(f"Risk flags: {'; '.join(risk_flags)}")

    return {
        "score": score,
        "details": "; ".join(details),
        "risk_flags": risk_flags
    }


def ken_griffin_multi_strategy_synthesis(fundamental_research: dict, quantitative_research: dict,
                                         global_macro: dict, trading_department: dict,
                                         risk_management: dict) -> dict:
    """Ken Griffin's multi-strategy synthesis and final decision"""
    details = []

    # Multi-strategy scoring
    fundamental_score = fundamental_research["score"] / 10
    quant_score = quantitative_research["score"] / 10
    macro_score = global_macro["score"] / 10
    trading_score = trading_department["score"] / 10
    risk_score = risk_management["score"] / 10

    # Ken Griffin's decision framework
    overall_attractiveness = (
            fundamental_score * 0.3 +
            quant_score * 0.25 +
            trading_score * 0.2 +
            macro_score * 0.15 +
            risk_score * 0.1
    )

    # Risk-adjusted return estimation
    risk_adjustment = 1 - (len(risk_management.get("risk_flags", [])) * 0.2)
    risk_adjusted_return = overall_attractiveness * risk_adjustment

    # Strategy allocation decision
    strategy_weights = {
        "equity_long_short": 0.4,
        "quantitative": 0.25,
        "macro": 0.15,
        "market_making": 0.1,
        "convertible_arbitrage": 0.1
    }

    # Adjust weights based on departmental strengths
    if quant_score > 0.7:
        strategy_weights["quantitative"] += 0.1
        strategy_weights["equity_long_short"] -= 0.05
        strategy_weights["macro"] -= 0.05

    if trading_score > 0.8:
        strategy_weights["market_making"] += 0.1
        strategy_weights["equity_long_short"] -= 0.1

    # Final decision logic
    if risk_adjusted_return > 0.15 and len(risk_management.get("risk_flags", [])) <= 1:
        decision = "Multi-strategy allocation"
        details.append("Strong opportunity across multiple strategies")
    elif risk_adjusted_return > 0.1:
        decision = "Selective strategy allocation"
        details.append("Moderate opportunity with selective strategy focus")
    elif len(risk_management.get("risk_flags", [])) > 2:
        decision = "Risk override - pass"
        details.append("Risk management concerns override opportunity")
    else:
        decision = "Insufficient multi-strategy edge"
        details.append("No clear edge across Citadel's strategy platforms")

    return {
        "decision": decision,
        "overall_attractiveness": overall_attractiveness,
        "risk_adjusted_return": risk_adjusted_return,
        "strategy_weights": strategy_weights,
        "details": "; ".join(details)
    }


# Helper functions for Citadel analysis
def calculate_factor_loadings(metrics: list, financial_line_items: list) -> dict:
    """Calculate factor model loadings"""
    if not metrics:
        return {}

    latest = metrics[0]
    factors = {}

    # Value factor
    if latest.price_to_book:
        factors["value_factor"] = max(0, 1 - (latest.price_to_book / 2))  # Lower P/B = higher value

    # Quality factor
    if latest.return_on_equity:
        factors["quality_factor"] = min(latest.return_on_equity / 0.2, 1.0)  # Normalize by 20%

    # Growth factor
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 3:
        growth = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1
        factors["growth_factor"] = min(growth / 0.15, 1.0)  # Normalize by 15%

    return factors


def calculate_mean_reversion_score(metrics: list) -> float:
    """Calculate mean reversion signal"""
    if len(metrics) < 4:
        return 0.0

    pe_ratios = [m.price_to_earnings for m in metrics if m.price_to_earnings and m.price_to_earnings > 0]
    if len(pe_ratios) >= 4:
        current_pe = pe_ratios[0]
        historical_mean = sum(pe_ratios[1:]) / len(pe_ratios[1:])

        if historical_mean > 0:
            deviation = (current_pe - historical_mean) / historical_mean
            return -deviation  # Negative deviation = positive signal

    return 0.0


def calculate_momentum_score(financial_line_items: list) -> float:
    """Calculate momentum signal"""
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 4:
        recent_growth = (revenues[0] / revenues[1]) - 1
        longer_growth = (revenues[1] / revenues[-1]) ** (1 / (len(revenues) - 2)) - 1

        if longer_growth > 0:
            momentum = (recent_growth - longer_growth) / longer_growth
            return max(min(momentum, 1.0), -1.0)

    return 0.0


def analyze_volatility_patterns(financial_line_items: list) -> float:
    """Analyze earnings volatility"""
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if len(earnings) >= 4:
        mean_earnings = sum(earnings) / len(earnings)
        if mean_earnings != 0:
            variance = sum((e - mean_earnings) ** 2 for e in earnings) / len(earnings)
            cv = (variance ** 0.5) / abs(mean_earnings)
            return min(cv, 1.0)

    return 0.5


def calculate_cross_sectional_rank(metrics: list) -> float:
    """Calculate cross-sectional ranking (simplified)"""
    if not metrics:
        return 0.5

    latest = metrics[0]
    rank_components = []

    if latest.return_on_equity:
        rank_components.append(min(latest.return_on_equity / 0.2, 1.0))

    if latest.price_to_earnings and latest.price_to_earnings > 0:
        rank_components.append(max(0, 1 - (latest.price_to_earnings / 25)))

    if rank_components:
        return sum(rank_components) / len(rank_components)

    return 0.5


def analyze_international_exposure(financial_line_items: list) -> float:
    """Estimate international revenue exposure"""
    return 0.4  # Placeholder - would analyze geographic segments


def analyze_pricing_power(financial_line_items: list) -> float:
    """Analyze pricing power through margin stability"""
    margins = []
    for item in financial_line_items:
        if item.operating_margin:
            margins.append(item.operating_margin)

    if len(margins) >= 3:
        margin_stability = 1 - ((max(margins) - min(margins)) / max(margins))
        return max(margin_stability, 0)

    return 0.5


def analyze_recession_performance(financial_line_items: list) -> float:
    """Analyze performance during economic downturns"""
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings:
        negative_years = sum(1 for e in earnings if e < 0)
        return 1 - (negative_years / len(earnings))

    return 0.5


def analyze_commodity_exposure(financial_line_items: list) -> float:
    """Analyze commodity price sensitivity"""
    return 0.3  # Placeholder - would analyze input cost sensitivity


def assess_trading_liquidity(market_cap: float) -> float:
    """Assess trading liquidity based on market cap"""
    if market_cap > 50_000_000_000:  # $50B+
        return 0.95
    elif market_cap > 10_000_000_000:  # $10B+
        return 0.85
    elif market_cap > 1_000_000_000:  # $1B+
        return 0.65
    else:
        return 0.3


def estimate_trading_volatility(financial_line_items: list) -> float:
    """Estimate trading volatility"""
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if revenues:
        mean_rev = sum(revenues) / len(revenues)
        if mean_rev > 0:
            variance = sum((r - mean_rev) ** 2 for r in revenues) / len(revenues)
            return (variance ** 0.5) / mean_rev

    return 0.25


def estimate_market_impact_cost(market_cap: float) -> float:
    """Estimate market impact costs"""
    if market_cap > 100_000_000_000:  # $100B+
        return 0.005
    elif market_cap > 10_000_000_000:  # $10B+
        return 0.01
    elif market_cap > 1_000_000_000:  # $1B+
        return 0.03
    else:
        return 0.08


def analyze_options_opportunities(financial_line_items: list, market_cap: float) -> float:
    """Analyze options trading opportunities"""
    if market_cap > 5_000_000_000:  # Large cap with active options
        return 0.8
    elif market_cap > 1_000_000_000:
        return 0.6
    else:
        return 0.3


def analyze_hft_opportunities(financial_line_items: list) -> float:
    """Analyze high-frequency trading opportunities"""
    return 0.6  # Placeholder for HFT signal analysis


def assess_earnings_quality(financial_line_items: list) -> float:
    """Assess earnings quality"""
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    fcf_vals = [item.free_cash_flow for item in financial_line_items if item.free_cash_flow]

    if earnings and fcf_vals and len(earnings) == len(fcf_vals):
        total_earnings = sum(earnings)
        total_fcf = sum(fcf_vals)

        if total_earnings > 0:
            cash_conversion = total_fcf / total_earnings
            return min(max(cash_conversion, 0), 1.0)

    return 0.5


def assess_business_concentration_risk(financial_line_items: list) -> float:
    """Assess business concentration risk"""
    return 0.5  # Placeholder - would analyze business segment data


def calculate_tail_risk_metrics(financial_line_items: list) -> float:
    """Calculate tail risk metrics"""
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings:
        sorted_earnings = sorted(earnings)
        worst_5pct = sorted_earnings[:max(1, len(earnings) // 20)]
        if worst_5pct:
            avg_earnings = sum(earnings) / len(earnings)
            worst_performance = min(worst_5pct)
            if avg_earnings > 0:
                return abs(worst_performance) / avg_earnings

    return 0.5


def assess_regulatory_risk(financial_line_items: list) -> float:
    """Assess regulatory risk"""
    return 0.3  # Placeholder - would analyze regulatory environment


def generate_citadel_output(ticker: str, analysis_data: dict, state: AgentState, agent_id: str) -> CitadelSignal:
    """Generate Citadel's multi-strategy investment decision"""
    template = ChatPromptTemplate.from_messages([
        ("system", """You are Citadel's AI system, implementing Ken Griffin's multi-strategy hedge fund approach:

        ORGANIZATIONAL STRUCTURE:
        - Fundamental Research Department: Deep value and quality analysis
        - Quantitative Research Department: Factor models and statistical signals
        - Global Macro Department: Economic cycle and currency analysis
        - Trading Department: Liquidity, execution, and market making
        - Risk Management: Comprehensive risk oversight
        - Ken Griffin Multi-Strategy Synthesis: Integration across all platforms

        PHILOSOPHY:
        1. Multi-Strategy Approach: Diversify across equity long/short, quant, macro, market making
        2. Technological Edge: Advanced technology and data analytics
        3. Risk Management: Sophisticated risk controls and position sizing
        4. Market Making: Provide liquidity while capturing spreads
        5. Systematic Execution: Minimize market impact and transaction costs
        6. Global Perspective: Opportunities across markets and asset classes
        7. Performance Focus: Risk-adjusted returns and alpha generation

        REASONING STYLE:
        - Reference multiple departmental analyses and perspectives
        - Integrate fundamental, quantitative, and macro insights
        - Consider trading feasibility and execution efficiency
        - Apply rigorous risk management overlay
        - Discuss multi-strategy allocation and position sizing
        - Express confidence in technological and analytical edge
        - Consider market making and liquidity provision opportunities

        Return investment signal with multi-strategy allocation recommendations."""),

        ("human", """Apply Citadel's multi-strategy analysis to {ticker}:

        {analysis_data}

        Provide investment signal in JSON format:
        {{
          "signal": "bullish" | "bearish" | "neutral",
          "confidence": float (0-100),
          "reasoning": "string",
          "strategy_allocation": {{
            "equity_long_short": float,
            "quantitative": float,
            "global_macro": float,
            "market_making": float,
            "convertible_arbitrage": float
          }}
        }}""")
    ])

    prompt = template.invoke({"analysis_data": json.dumps(analysis_data, indent=2), "ticker": ticker})

    def create_default_citadel_signal():
        return CitadelSignal(
            signal="neutral",
            confidence=0.0,
            reasoning="Analysis error, defaulting to neutral",
            strategy_allocation={
                "equity_long_short": 0.4,
                "quantitative": 0.25,
                "global_macro": 0.15,
                "market_making": 0.1,
                "convertible_arbitrage": 0.1
            }
        )

    return call_llm(
        prompt=prompt,
        pydantic_model=CitadelSignal,
        agent_name=agent_id,
        state=state,
        default_factory=create_default_citadel_signal,
    )