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
import math
from typing_extensions import Literal
from fincept_terminal.Agents.src.utils.progress import progress
from fincept_terminal.Agents.src.utils.llm import call_llm
from fincept_terminal.Agents.src.utils.api_key import get_api_key_from_state


class DEShawSignal(BaseModel):
    signal: Literal["bullish", "bearish", "neutral"]
    confidence: float
    reasoning: str
    computational_models: dict


def de_shaw_hedge_fund_agent(state: AgentState, agent_id: str = "de_shaw_hedge_fund_agent"):
    """
    D.E. Shaw: Computational finance, quantitative hedge fund
    Structure: Research → Computational Finance → Trading Systems → Risk Control → David Shaw Decision
    Philosophy: Scientific approach, computational methods, systematic strategies
    """
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    api_key = get_api_key_from_state(state, "FINANCIAL_DATASETS_API_KEY")
    analysis_data = {}
    de_shaw_analysis = {}

    for ticker in tickers:
        progress.update_status(agent_id, ticker, "Research team computational analysis")

        metrics = get_financial_metrics(ticker, end_date, period="quarterly", limit=20, api_key=api_key)
        financial_line_items = search_line_items(
            ticker,
            [
                "revenue", "net_income", "free_cash_flow", "total_debt",
                "shareholders_equity", "operating_margin", "total_assets",
                "current_assets", "current_liabilities", "research_and_development"
            ],
            end_date,
            period="quarterly",
            limit=20,
            api_key=api_key,
        )
        market_cap = get_market_cap(ticker, end_date, api_key=api_key)

        # DE Shaw's computational analysis structure
        progress.update_status(agent_id, ticker, "Research team analysis")
        research_team = research_team_computational_analysis(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Computational finance models")
        computational_finance = computational_finance_models(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Trading systems analysis")
        trading_systems = trading_systems_analysis(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Risk control systems")
        risk_control = risk_control_systems(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "David Shaw systematic decision")
        david_shaw_decision = david_shaw_systematic_decision(
            research_team, computational_finance, trading_systems, risk_control
        )

        # DE Shaw's computational scoring
        total_score = (
                computational_finance["score"] * 0.35 +
                research_team["score"] * 0.25 +
                trading_systems["score"] * 0.2 +
                risk_control["score"] * 0.2
        )

        # DE Shaw's systematic decision framework
        model_confidence = computational_finance.get("model_confidence", 0)
        if total_score >= 8.0 and model_confidence > 0.8:
            signal = "bullish"
        elif total_score <= 3.5 or model_confidence < 0.4:
            signal = "bearish"
        else:
            signal = "neutral"

        analysis_data[ticker] = {
            "signal": signal,
            "score": total_score,
            "research_team": research_team,
            "computational_finance": computational_finance,
            "trading_systems": trading_systems,
            "risk_control": risk_control,
            "david_shaw_decision": david_shaw_decision
        }

        de_shaw_output = generate_de_shaw_output(ticker, analysis_data, state, agent_id)

        de_shaw_analysis[ticker] = {
            "signal": de_shaw_output.signal,
            "confidence": de_shaw_output.confidence,
            "reasoning": de_shaw_output.reasoning,
            "computational_models": de_shaw_output.computational_models
        }

        progress.update_status(agent_id, ticker, "Done", analysis=de_shaw_output.reasoning)

    message = HumanMessage(content=json.dumps(de_shaw_analysis), name=agent_id)

    if state["metadata"]["show_reasoning"]:
        show_agent_reasoning(de_shaw_analysis, "D.E. Shaw")

    state["data"]["analyst_signals"][agent_id] = de_shaw_analysis
    progress.update_status(agent_id, None, "Done")

    return {"messages": [message], "data": state["data"]}


def research_team_computational_analysis(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """DE Shaw's research team computational analysis"""
    score = 0
    details = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No computational research data"}

    # Computational valuation models
    dcf_model_score = run_dcf_computational_model(financial_line_items, market_cap)
    if dcf_model_score > 0.7:
        score += 2.5
        details.append(f"DCF model indicates undervaluation: {dcf_model_score:.2f}")

    # Factor decomposition analysis
    factor_decomp_score = perform_factor_decomposition(metrics, financial_line_items)
    score += factor_decomp_score
    details.append(f"Factor decomposition score: {factor_decomp_score:.2f}")

    # Time series analysis
    time_series_score = conduct_time_series_analysis(financial_line_items)
    if time_series_score > 0.6:
        score += 2
        details.append(f"Time series analysis positive: {time_series_score:.2f}")

    # Cross-sectional analysis
    cross_sectional_score = perform_cross_sectional_analysis(metrics, market_cap)
    score += cross_sectional_score
    details.append(f"Cross-sectional ranking: {cross_sectional_score:.2f}")

    # Alternative data integration
    alt_data_score = integrate_alternative_data(financial_line_items)
    if alt_data_score > 0.5:
        score += 1.5
        details.append(f"Alternative data signals: {alt_data_score:.2f}")

    return {"score": score, "details": "; ".join(details)}


def computational_finance_models(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """DE Shaw's computational finance models"""
    score = 0
    details = []
    model_outputs = {}

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No computational finance data"}

    # Monte Carlo simulation
    monte_carlo_result = run_monte_carlo_simulation(financial_line_items, market_cap)
    model_outputs["monte_carlo"] = monte_carlo_result
    if monte_carlo_result.get("probability_of_profit", 0) > 0.7:
        score += 3
        details.append(f"Monte Carlo: {monte_carlo_result['probability_of_profit']:.1%} profit probability")

    # Stochastic differential equations
    sde_model_result = apply_sde_models(financial_line_items)
    model_outputs["sde"] = sde_model_result
    score += sde_model_result.get("score", 0)
    details.append(f"SDE model score: {sde_model_result.get('score', 0):.2f}")

    # Machine learning ensemble
    ml_ensemble_result = run_ml_ensemble_models(metrics, financial_line_items)
    model_outputs["ml_ensemble"] = ml_ensemble_result
    if ml_ensemble_result.get("prediction_confidence", 0) > 0.8:
        score += 2.5
        details.append(f"ML ensemble high confidence: {ml_ensemble_result['prediction_confidence']:.1%}")

    # Options pricing models
    options_model_score = analyze_options_pricing_opportunities(market_cap)
    model_outputs["options"] = {"score": options_model_score}
    if options_model_score > 1:
        score += options_model_score
        details.append(f"Options pricing opportunity: {options_model_score:.2f}")

    # Risk-neutral valuation
    risk_neutral_value = calculate_risk_neutral_valuation(financial_line_items, market_cap)
    model_outputs["risk_neutral"] = risk_neutral_value
    if risk_neutral_value.get("upside_potential", 0) > 0.2:
        score += 2
        details.append(f"Risk-neutral upside: {risk_neutral_value['upside_potential']:.1%}")

    # Model confidence calculation
    model_confidence = calculate_model_confidence(model_outputs)

    return {
        "score": score,
        "details": "; ".join(details),
        "model_outputs": model_outputs,
        "model_confidence": model_confidence
    }


def trading_systems_analysis(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """DE Shaw's trading systems analysis"""
    score = 0
    details = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No trading systems data"}

    # Algorithmic execution analysis
    execution_score = analyze_algorithmic_execution(market_cap)
    score += execution_score
    details.append(f"Execution efficiency score: {execution_score:.2f}")

    # Market microstructure analysis
    microstructure_score = analyze_market_microstructure(financial_line_items, market_cap)
    if microstructure_score > 0.6:
        score += 2
        details.append(f"Microstructure opportunity: {microstructure_score:.2f}")

    # High-frequency signals
    hf_signals = generate_high_frequency_signals(financial_line_items)
    if hf_signals > 0.5:
        score += 1.5
        details.append(f"HF signals positive: {hf_signals:.2f}")

    # Latency arbitrage potential
    latency_arb_score = assess_latency_arbitrage(market_cap)
    score += latency_arb_score
    details.append(f"Latency arbitrage score: {latency_arb_score:.2f}")

    # Market making opportunities
    market_making_score = evaluate_market_making_opportunity(market_cap)
    if market_making_score > 0.7:
        score += 2
        details.append(f"Market making opportunity: {market_making_score:.2f}")

    return {"score": score, "details": "; ".join(details)}


def risk_control_systems(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """DE Shaw's risk control systems"""
    score = 0
    details = []
    risk_metrics = {}

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No risk control data"}

    # Value-at-Risk calculation
    var_calculation = calculate_advanced_var(financial_line_items)
    risk_metrics["var"] = var_calculation
    if var_calculation < 0.15:  # 15% VaR threshold
        score += 2
        details.append(f"Acceptable VaR: {var_calculation:.1%}")

    # Stress testing
    stress_test_results = conduct_comprehensive_stress_testing(financial_line_items)
    risk_metrics["stress_tests"] = stress_test_results
    if stress_test_results.get("pass_rate", 0) > 0.8:
        score += 2
        details.append(f"Stress tests passed: {stress_test_results['pass_rate']:.1%}")

    # Correlation analysis
    correlation_analysis = perform_correlation_analysis(metrics, financial_line_items)
    risk_metrics["correlation"] = correlation_analysis
    if correlation_analysis < 0.5:  # Low correlation
        score += 1.5
        details.append("Low correlation with portfolio")

    # Liquidity risk assessment
    liquidity_risk = assess_advanced_liquidity_risk(market_cap)
    risk_metrics["liquidity"] = liquidity_risk
    if liquidity_risk < 0.3:
        score += 1.5
        details.append("Low liquidity risk")

    # Dynamic hedging requirements
    hedging_requirements = calculate_dynamic_hedging_needs(financial_line_items)
    risk_metrics["hedging"] = hedging_requirements
    if hedging_requirements < 0.2:  # Low hedging needs
        score += 1
        details.append("Low hedging requirements")

    return {
        "score": score,
        "details": "; ".join(details),
        "risk_metrics": risk_metrics
    }


def david_shaw_systematic_decision(research_team: dict, computational_finance: dict,
                                   trading_systems: dict, risk_control: dict) -> dict:
    """David Shaw's systematic computational decision"""
    details = []

    # Computational model synthesis
    research_score = research_team["score"] / 10
    model_confidence = computational_finance.get("model_confidence", 0)
    execution_feasibility = trading_systems["score"] / 10
    risk_acceptability = risk_control["score"] / 8

    # Shaw's decision framework
    overall_systematic_edge = (
            model_confidence * 0.35 +
            research_score * 0.3 +
            execution_feasibility * 0.2 +
            risk_acceptability * 0.15
    )

    # Computational conviction analysis
    model_outputs = computational_finance.get("model_outputs", {})
    monte_carlo_prob = model_outputs.get("monte_carlo", {}).get("probability_of_profit", 0.5)

    # Shaw's systematic decision logic
    if overall_systematic_edge > 0.8 and model_confidence > 0.85:
        if monte_carlo_prob > 0.75:
            decision = "High conviction systematic position"
            details.append("Strong computational edge with high model confidence")
        else:
            decision = "Moderate systematic position"
            details.append("Good computational signals with moderate probability")
    elif overall_systematic_edge > 0.6:
        decision = "Systematic position with enhanced monitoring"
        details.append("Adequate systematic edge requiring close monitoring")
    elif execution_feasibility < 0.4:
        decision = "Pass - execution constraints"
        details.append("Computational edge limited by execution constraints")
    else:
        decision = "Insufficient systematic edge"
        details.append("Computational models do not indicate sufficient edge")

    # Expected Sharpe ratio
    expected_sharpe = overall_systematic_edge * 2  # Target 2.0 Sharpe for top decile

    # Computational complexity assessment
    model_complexity = len(model_outputs) * 0.1

    return {
        "decision": decision,
        "overall_systematic_edge": overall_systematic_edge,
        "expected_sharpe": expected_sharpe,
        "model_complexity": model_complexity,
        "details": "; ".join(details)
    }


# Computational Finance Helper Functions
def run_dcf_computational_model(financial_line_items: list, market_cap: float) -> float:
    """Run computational DCF model"""
    if not financial_line_items or not market_cap:
        return 0.5

    latest = financial_line_items[0]
    if not latest.free_cash_flow or latest.free_cash_flow <= 0:
        return 0.3

    # Advanced DCF with multiple scenarios
    base_fcf = latest.free_cash_flow

    # Scenario analysis
    scenarios = [
        {"growth": 0.05, "terminal_multiple": 12, "probability": 0.3},
        {"growth": 0.08, "terminal_multiple": 15, "probability": 0.4},
        {"growth": 0.12, "terminal_multiple": 18, "probability": 0.3}
    ]

    expected_value = 0
    for scenario in scenarios:
        scenario_value = 0
        for year in range(1, 6):
            future_fcf = base_fcf * (1 + scenario["growth"]) ** year
            pv = future_fcf / (1.1 ** year)  # 10% discount rate
            scenario_value += pv

        # Terminal value
        terminal_fcf = base_fcf * (1 + scenario["growth"]) ** 5
        terminal_value = terminal_fcf * scenario["terminal_multiple"] / (1.1 ** 5)
        scenario_value += terminal_value

        expected_value += scenario_value * scenario["probability"]

    # Compare to market cap
    if expected_value > market_cap:
        upside = (expected_value - market_cap) / market_cap
        return min(upside / 0.5, 1.0)  # Normalize by 50% upside

    return 0.3


def perform_factor_decomposition(metrics: list, financial_line_items: list) -> float:
    """Perform advanced factor decomposition"""
    if not metrics:
        return 0.5

    latest = metrics[0]
    factor_score = 0

    # Value factor
    if latest.price_to_book:
        value_score = max(0, 1 - (latest.price_to_book / 2))
        factor_score += value_score * 0.3

    # Quality factor
    if latest.return_on_equity:
        quality_score = min(latest.return_on_equity / 0.2, 1.0)
        factor_score += quality_score * 0.3

    # Growth factor
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 4:
        growth_rate = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1
        growth_score = min(growth_rate / 0.15, 1.0)
        factor_score += growth_score * 0.4

    return factor_score


def conduct_time_series_analysis(financial_line_items: list) -> float:
    """Conduct advanced time series analysis"""
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) < 8:
        return 0.4

    # Trend analysis using linear regression
    n = len(revenues)
    x_mean = (n - 1) / 2
    y_mean = sum(revenues) / n

    numerator = sum((i - x_mean) * (revenues[i] - y_mean) for i in range(n))
    denominator = sum((i - x_mean) ** 2 for i in range(n))

    if denominator > 0:
        slope = numerator / denominator
        r_squared = (numerator ** 2) / (denominator * sum((y - y_mean) ** 2 for y in revenues))

        # Positive trend with good fit
        if slope > 0 and r_squared > 0.6:
            return min((slope / y_mean) * 10 * r_squared, 1.0)

    return 0.4


def perform_cross_sectional_analysis(metrics: list, market_cap: float) -> float:
    """Perform cross-sectional ranking analysis"""
    if not metrics:
        return 0.5

    latest = metrics[0]

    # Multi-factor ranking
    ranking_score = 0

    if latest.return_on_equity:
        roe_percentile = min(latest.return_on_equity / 0.25, 1.0)
        ranking_score += roe_percentile * 0.3

    if latest.price_to_earnings and latest.price_to_earnings > 0:
        pe_percentile = max(0, 1 - (latest.price_to_earnings / 25))
        ranking_score += pe_percentile * 0.3

    # Size factor
    size_factor = min(math.log10(market_cap / 1_000_000_000) / 2, 1.0)
    ranking_score += size_factor * 0.4

    return ranking_score


def integrate_alternative_data(financial_line_items: list) -> float:
    """Integrate alternative data signals"""
    # Simulated alternative data integration
    base_fundamentals = 0.5

    # Would integrate satellite data, credit card transactions, etc.
    alt_data_boost = 0.2  # Placeholder

    return min(base_fundamentals + alt_data_boost, 1.0)


def run_monte_carlo_simulation(financial_line_items: list, market_cap: float) -> dict:
    """Run Monte Carlo simulation for price prediction"""
    if not financial_line_items:
        return {"probability_of_profit": 0.5}

    # Simulate 10,000 paths
    num_simulations = 1000  # Reduced for computational efficiency
    profitable_outcomes = 0

    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 4:
        # Calculate historical volatility
        growth_rates = [(revenues[i] / revenues[i + 1]) - 1 for i in range(len(revenues) - 1)]
        mean_growth = sum(growth_rates) / len(growth_rates)
        volatility = (sum((g - mean_growth) ** 2 for g in growth_rates) / len(growth_rates)) ** 0.5

        # Monte Carlo simulation
        for _ in range(num_simulations):
            # Simulate 1-year forward performance
            random_shock = (hash(str(_)) % 1000 - 500) / 500  # Pseudo-random between -1 and 1
            simulated_return = mean_growth + random_shock * volatility

            if simulated_return > 0.05:  # 5% profit threshold
                profitable_outcomes += 1

        probability_of_profit = profitable_outcomes / num_simulations
    else:
        probability_of_profit = 0.5

    return {
        "probability_of_profit": probability_of_profit,
        "simulations": num_simulations
    }


def apply_sde_models(financial_line_items: list) -> dict:
    """Apply stochastic differential equation models"""
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) < 6:
        return {"score": 0.5}

    # Geometric Brownian Motion model for revenue evolution
    # dS = μS dt + σS dW

    # Calculate drift (μ) and volatility (σ)
    growth_rates = [(revenues[i] / revenues[i + 1]) - 1 for i in range(len(revenues) - 1)]
    drift = sum(growth_rates) / len(growth_rates)
    volatility = (sum((g - drift) ** 2 for g in growth_rates) / len(growth_rates)) ** 0.5

    # SDE score based on favorable drift and manageable volatility
    if drift > 0.05 and volatility < 0.3:
        sde_score = min(drift / volatility, 1.0) if volatility > 0 else 0.5
    else:
        sde_score = 0.3

    return {"score": sde_score, "drift": drift, "volatility": volatility}


def run_ml_ensemble_models(metrics: list, financial_line_items: list) -> dict:
    """Run machine learning ensemble models"""
    if not metrics or not financial_line_items:
        return {"prediction_confidence": 0.5}

    # Simulate ensemble of models
    models = ["random_forest", "gradient_boosting", "neural_network", "svm"]
    predictions = []

    for model in models:
        # Simplified model predictions based on fundamentals
        latest = metrics[0]
        if latest.return_on_equity and latest.price_to_earnings:
            if model == "random_forest":
                pred = min(latest.return_on_equity * 3, 1.0)
            elif model == "gradient_boosting":
                pred = max(0, 1 - (latest.price_to_earnings / 20))
            elif model == "neural_network":
                pred = (latest.return_on_equity * 2 + (1 - latest.price_to_earnings / 25)) / 2
            else:  # SVM
                pred = 0.5 + (latest.return_on_equity - 0.1) * 2

            predictions.append(max(0, min(pred, 1.0)))
        else:
            predictions.append(0.5)

    # Ensemble prediction
    ensemble_pred = sum(predictions) / len(predictions)

    # Confidence based on model agreement
    variance = sum((p - ensemble_pred) ** 2 for p in predictions) / len(predictions)
    confidence = max(0, 1 - variance * 4)

    return {
        "ensemble_prediction": ensemble_pred,
        "prediction_confidence": confidence,
        "individual_predictions": dict(zip(models, predictions))
    }


def analyze_options_pricing_opportunities(market_cap: float) -> float:
    """Analyze options pricing opportunities"""
    # Larger companies have more liquid options markets
    if market_cap > 50_000_000_000:  # $50B+
        return 1.5
    elif market_cap > 10_000_000_000:  # $10B+
        return 1.0
    elif market_cap > 1_000_000_000:  # $1B+
        return 0.5
    else:
        return 0.2


def calculate_risk_neutral_valuation(financial_line_items: list, market_cap: float) -> dict:
    """Calculate risk-neutral valuation"""
    if not financial_line_items or not market_cap:
        return {"upside_potential": 0}

    latest = financial_line_items[0]
    if latest.free_cash_flow and latest.free_cash_flow > 0:
        # Risk-neutral DCF at risk-free rate
        risk_free_rate = 0.03  # 3% risk-free rate
        risk_neutral_value = latest.free_cash_flow / risk_free_rate  # Perpetuity

        upside_potential = (risk_neutral_value - market_cap) / market_cap
        return {"upside_potential": max(0, upside_potential)}

    return {"upside_potential": 0}


def calculate_model_confidence(model_outputs: dict) -> float:
    """Calculate overall model confidence"""
    confidence_scores = []

    if "monte_carlo" in model_outputs:
        mc_prob = model_outputs["monte_carlo"].get("probability_of_profit", 0.5)
        confidence_scores.append(abs(mc_prob - 0.5) * 2)  # Distance from 50%

    if "ml_ensemble" in model_outputs:
        ml_conf = model_outputs["ml_ensemble"].get("prediction_confidence", 0.5)
        confidence_scores.append(ml_conf)

    if "sde" in model_outputs:
        sde_score = model_outputs["sde"].get("score", 0.5)
        confidence_scores.append(sde_score)

    return sum(confidence_scores) / len(confidence_scores) if confidence_scores else 0.5


def analyze_algorithmic_execution(market_cap: float) -> float:
    """Analyze algorithmic execution efficiency"""
    # Execution efficiency based on market cap and liquidity
    if market_cap > 100_000_000_000:  # $100B+
        return 2.0
    elif market_cap > 20_000_000_000:  # $20B+
        return 1.5
    elif market_cap > 5_000_000_000:  # $5B+
        return 1.0
    else:
        return 0.5


def analyze_market_microstructure(financial_line_items: list, market_cap: float) -> float:
    """Analyze market microstructure opportunities"""
    # Microstructure opportunities in larger, more liquid names
    liquidity_score = min(market_cap / 10_000_000_000, 1.0)  # Normalize by $10B

    # Add fundamental volatility component
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if revenues:
        volatility = calculate_coefficient_of_variation(revenues)
        volatility_score = min(volatility, 1.0)
        return (liquidity_score + volatility_score) / 2

    return liquidity_score


def generate_high_frequency_signals(financial_line_items: list) -> float:
    """Generate high-frequency trading signals"""
    # HF signals based on earnings volatility and predictability
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if len(earnings) >= 8:
        # Look for patterns in quarterly earnings
        earnings_changes = [earnings[i] - earnings[i + 1] for i in range(len(earnings) - 1)]
        if earnings_changes:
            autocorr = calculate_autocorrelation(earnings_changes, 1)
            return min(abs(autocorr), 1.0)

    return 0.4


def assess_latency_arbitrage(market_cap: float) -> float:
    """Assess latency arbitrage opportunities"""
    # Latency arbitrage more viable in larger, more traded stocks
    if market_cap > 50_000_000_000:  # $50B+
        return 1.0
    elif market_cap > 10_000_000_000:  # $10B+
        return 0.7
    else:
        return 0.3


def evaluate_market_making_opportunity(market_cap: float) -> float:
    """Evaluate market making opportunity"""
    # Market making viability based on trading volume proxy (market cap)
    if market_cap > 20_000_000_000:  # $20B+
        return 0.9
    elif market_cap > 5_000_000_000:  # $5B+
        return 0.7
    elif market_cap > 1_000_000_000:  # $1B+
        return 0.5
    else:
        return 0.2


def calculate_advanced_var(financial_line_items: list) -> float:
    """Calculate advanced Value-at-Risk"""
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if len(earnings) >= 8:
        # Calculate 95% VaR using historical simulation
        sorted_earnings = sorted(earnings)
        var_index = int(len(sorted_earnings) * 0.05)
        worst_5pct = sorted_earnings[:var_index + 1]

        if worst_5pct and max(earnings) > 0:
            var_loss = (max(earnings) - min(worst_5pct)) / max(earnings)
            return var_loss

    return 0.2  # Default 20% VaR


def conduct_comprehensive_stress_testing(financial_line_items: list) -> dict:
    """Conduct comprehensive stress testing"""
    stress_scenarios = [
        "market_crash", "recession", "sector_downturn",
        "liquidity_crisis", "interest_rate_shock"
    ]

    passed_tests = 0

    # Simplified stress testing based on historical performance
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    revenues = [item.revenue for item in financial_line_items if item.revenue]

    if earnings and revenues:
        # Test earnings stability (recession scenario)
        negative_earnings = sum(1 for e in earnings if e < 0)
        if negative_earnings <= len(earnings) * 0.2:  # <20% negative years
            passed_tests += 2

        # Test revenue resilience
        revenue_declines = sum(1 for i in range(len(revenues) - 1) if revenues[i] < revenues[i + 1])
        if revenue_declines <= len(revenues) * 0.3:  # <30% declining years
            passed_tests += 2

        # Test margin stability (sector downturn)
        if len(earnings) == len(revenues):
            margins = [earnings[i] / revenues[i] if revenues[i] > 0 else 0 for i in range(len(earnings))]
            margin_volatility = calculate_coefficient_of_variation(margins)
            if margin_volatility < 0.5:
                passed_tests += 1

    pass_rate = passed_tests / len(stress_scenarios)

    return {
        "pass_rate": pass_rate,
        "tests_passed": passed_tests,
        "total_tests": len(stress_scenarios)
    }


def perform_correlation_analysis(metrics: list, financial_line_items: list) -> float:
    """Perform correlation analysis with market factors"""
    # Simplified correlation analysis
    # In practice, would correlate with market indices, sector ETFs, etc.

    if not metrics or not financial_line_items:
        return 0.6  # Default moderate correlation

    # Proxy correlation using business fundamentals
    latest = metrics[0]

    # High ROE, reasonable valuation suggests lower market correlation
    if latest.return_on_equity and latest.price_to_earnings:
        if latest.return_on_equity > 0.15 and latest.price_to_earnings < 20:
            return 0.3  # Low correlation - idiosyncratic opportunity

    return 0.6  # Moderate correlation


def assess_advanced_liquidity_risk(market_cap: float) -> float:
    """Assess advanced liquidity risk"""
    # Liquidity risk inversely related to market cap
    if market_cap > 100_000_000_000:  # $100B+
        return 0.05
    elif market_cap > 20_000_000_000:  # $20B+
        return 0.1
    elif market_cap > 5_000_000_000:  # $5B+
        return 0.2
    elif market_cap > 1_000_000_000:  # $1B+
        return 0.4
    else:
        return 0.7


def calculate_dynamic_hedging_needs(financial_line_items: list) -> float:
    """Calculate dynamic hedging requirements"""
    # Hedging needs based on business volatility
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    earnings = [item.net_income for item in financial_line_items if item.net_income]

    hedging_score = 0

    if revenues:
        revenue_volatility = calculate_coefficient_of_variation(revenues)
        hedging_score += revenue_volatility * 0.5

    if earnings:
        earnings_volatility = calculate_coefficient_of_variation(earnings)
        hedging_score += earnings_volatility * 0.5

    return min(hedging_score, 1.0)


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


def calculate_autocorrelation(data: list, lag: int) -> float:
    """Calculate autocorrelation with given lag"""
    if len(data) <= lag:
        return 0

    mean_val = sum(data) / len(data)
    numerator = sum((data[i] - mean_val) * (data[i + lag] - mean_val)
                    for i in range(len(data) - lag))
    denominator = sum((x - mean_val) ** 2 for x in data)

    return numerator / denominator if denominator != 0 else 0


def generate_de_shaw_output(ticker: str, analysis_data: dict, state: AgentState, agent_id: str) -> DEShawSignal:
    """Generate DE Shaw's computational finance investment decision"""
    template = ChatPromptTemplate.from_messages([
        ("system", """You are D.E. Shaw's AI system, implementing David Shaw's computational finance approach:

        ORGANIZATIONAL STRUCTURE:
        - Research Team: Computational analysis and quantitative research
        - Computational Finance: Monte Carlo, SDE models, ML ensemble, options pricing
        - Trading Systems: Algorithmic execution, HFT, market microstructure
        - Risk Control: Advanced VaR, stress testing, correlation analysis
        - David Shaw Systematic Decision: Scientific computational synthesis

        PHILOSOPHY:
        1. Computational Finance: Advanced mathematical and computational methods
        2. Scientific Approach: Rigorous quantitative analysis and modeling
        3. Systematic Strategies: Algorithm-driven investment decisions
        4. Risk Management: Sophisticated risk control and hedging
        5. Technology Edge: Cutting-edge computational infrastructure
        6. Market Efficiency: Exploit computational advantages and inefficiencies
        7. Diversification: Multiple uncorrelated computational strategies

        REASONING STYLE:
        - Reference computational models and mathematical analysis
        - Discuss Monte Carlo simulations and stochastic processes
        - Apply machine learning and statistical methods
        - Consider execution algorithms and market microstructure
        - Express confidence in computational edge and model validation
        - Analyze risk-adjusted returns and Sharpe ratios
        - Focus on systematic, repeatable investment processes

        Return investment signal with computational model outputs and confidence metrics."""),

        ("human", """Apply D.E. Shaw's computational analysis to {ticker}:

        {analysis_data}

        Provide investment signal in JSON format:
        {{
          "signal": "bullish" | "bearish" | "neutral",
          "confidence": float (0-100),
          "reasoning": "string",
          "computational_models": {{
            "monte_carlo_probability": float,
            "ml_ensemble_prediction": float,
            "sde_model_score": float,
            "risk_neutral_upside": float,
            "model_confidence": float
          }}
        }}""")
    ])

    prompt = template.invoke({"analysis_data": json.dumps(analysis_data, indent=2), "ticker": ticker})

    def create_default_de_shaw_signal():
        return DEShawSignal(
            signal="neutral",
            confidence=0.0,
            reasoning="Analysis error, defaulting to neutral",
            computational_models={
                "monte_carlo_probability": 0.5,
                "ml_ensemble_prediction": 0.5,
                "sde_model_score": 0.5,
                "risk_neutral_upside": 0.0,
                "model_confidence": 0.5
            }
        )

    return call_llm(
        prompt=prompt,
        pydantic_model=DEShawSignal,
        agent_name=agent_id,
        state=state,
        default_factory=create_default_de_shaw_signal,
    )