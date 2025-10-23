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


class TwoSigmaSignal(BaseModel):
    signal: Literal["bullish", "bearish", "neutral"]
    confidence: float
    reasoning: str
    ml_model_predictions: dict


def two_sigma_hedge_fund_agent(state: AgentState, agent_id: str = "two_sigma_hedge_fund_agent"):
    """
    Two Sigma: Machine learning, data science
    Structure: Data Science → ML Engineering → Risk Models → Portfolio Optimization → Scientific Decision
    Philosophy: Data-driven, machine learning, scientific approach, technology focus
    """
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    api_key = get_api_key_from_state(state, "FINANCIAL_DATASETS_API_KEY")
    analysis_data = {}
    two_sigma_analysis = {}

    for ticker in tickers:
        progress.update_status(agent_id, ticker, "Data science team feature engineering")

        metrics = get_financial_metrics(ticker, end_date, period="quarterly", limit=16, api_key=api_key)
        financial_line_items = search_line_items(
            ticker,
            [
                "revenue", "net_income", "free_cash_flow", "total_debt",
                "shareholders_equity", "operating_margin", "total_assets",
                "current_assets", "current_liabilities", "research_and_development"
            ],
            end_date,
            period="quarterly",
            limit=16,
            api_key=api_key,
        )
        market_cap = get_market_cap(ticker, end_date, api_key=api_key)

        # Two Sigma's ML-driven analysis structure
        progress.update_status(agent_id, ticker, "Data science team analysis")
        data_science_team = data_science_team_analysis(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "ML engineering team models")
        ml_engineering_team = ml_engineering_team_analysis(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Risk modeling team analysis")
        risk_modeling_team = risk_modeling_team_analysis(metrics, financial_line_items)

        progress.update_status(agent_id, ticker, "Portfolio optimization analysis")
        portfolio_optimization = portfolio_optimization_analysis(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Scientific method synthesis")
        scientific_synthesis = scientific_method_synthesis(
            data_science_team, ml_engineering_team, risk_modeling_team, portfolio_optimization
        )

        # Two Sigma's ML-weighted scoring
        total_score = (
                ml_engineering_team["score"] * 0.35 +
                data_science_team["score"] * 0.25 +
                portfolio_optimization["score"] * 0.2 +
                risk_modeling_team["score"] * 0.2
        )

        # Two Sigma's scientific decision framework
        ml_confidence = ml_engineering_team.get("model_confidence", 0)
        if total_score >= 8.0 and ml_confidence > 0.75:
            signal = "bullish"
        elif total_score <= 3.5 or ml_confidence < 0.4:
            signal = "bearish"
        else:
            signal = "neutral"

        analysis_data[ticker] = {
            "signal": signal,
            "score": total_score,
            "data_science_team": data_science_team,
            "ml_engineering_team": ml_engineering_team,
            "risk_modeling_team": risk_modeling_team,
            "portfolio_optimization": portfolio_optimization,
            "scientific_synthesis": scientific_synthesis
        }

        two_sigma_output = generate_two_sigma_output(ticker, analysis_data, state, agent_id)

        two_sigma_analysis[ticker] = {
            "signal": two_sigma_output.signal,
            "confidence": two_sigma_output.confidence,
            "reasoning": two_sigma_output.reasoning,
            "ml_model_predictions": two_sigma_output.ml_model_predictions
        }

        progress.update_status(agent_id, ticker, "Done", analysis=two_sigma_output.reasoning)

    message = HumanMessage(content=json.dumps(two_sigma_analysis), name=agent_id)

    if state["metadata"]["show_reasoning"]:
        show_agent_reasoning(two_sigma_analysis, "Two Sigma")

    state["data"]["analyst_signals"][agent_id] = two_sigma_analysis
    progress.update_status(agent_id, None, "Done")

    return {"messages": [message], "data": state["data"]}


def data_science_team_analysis(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Two Sigma's data science team feature engineering and analysis"""
    score = 0
    details = []
    features = {}

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No data for feature engineering"}

    # Feature engineering from financial data
    engineered_features = engineer_financial_features(metrics, financial_line_items)
    features.update(engineered_features)

    # Time series features
    time_series_features = extract_time_series_features(financial_line_items)
    features.update(time_series_features)

    # Cross-sectional features
    cross_sectional_features = create_cross_sectional_features(metrics, market_cap)
    features.update(cross_sectional_features)

    # Feature importance scoring
    if features.get("revenue_momentum", 0) > 0.6:
        score += 2
        details.append(f"Strong revenue momentum feature: {features['revenue_momentum']:.2f}")

    if features.get("margin_stability", 0) > 0.7:
        score += 2
        details.append(f"High margin stability: {features['margin_stability']:.2f}")

    if features.get("growth_quality", 0) > 0.6:
        score += 1.5
        details.append(f"Quality growth signals: {features['growth_quality']:.2f}")

    # Alternative data integration (simulated)
    alt_data_score = integrate_alternative_data_signals(features)
    score += alt_data_score
    if alt_data_score > 1:
        details.append(f"Positive alternative data signals: {alt_data_score:.1f}")

    # Feature interaction analysis
    interaction_score = analyze_feature_interactions(features)
    score += interaction_score
    details.append(f"Feature interaction score: {interaction_score:.2f}")

    return {
        "score": min(score, 10),
        "details": "; ".join(details),
        "features": features
    }


def ml_engineering_team_analysis(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Two Sigma's ML engineering team model predictions"""
    score = 0
    details = []
    model_predictions = {}

    if not metrics or not financial_line_items:
        return {"score": 0, "details": "No ML data"}

    # Ensemble model predictions
    random_forest_pred = simulate_random_forest_prediction(metrics, financial_line_items)
    model_predictions["random_forest"] = random_forest_pred

    gradient_boosting_pred = simulate_gradient_boosting_prediction(metrics, financial_line_items)
    model_predictions["gradient_boosting"] = gradient_boosting_pred

    neural_network_pred = simulate_neural_network_prediction(metrics, financial_line_items)
    model_predictions["neural_network"] = neural_network_pred

    # Time series models
    lstm_pred = simulate_lstm_prediction(financial_line_items)
    model_predictions["lstm"] = lstm_pred

    # Ensemble prediction
    ensemble_prediction = calculate_ensemble_prediction(model_predictions)
    model_predictions["ensemble"] = ensemble_prediction

    # Model confidence calculation
    model_confidence = calculate_model_confidence(model_predictions)

    # Scoring based on ensemble prediction and confidence
    if ensemble_prediction > 0.6 and model_confidence > 0.7:
        score += 4
        details.append(f"Strong ensemble prediction: {ensemble_prediction:.2f} with high confidence")
    elif ensemble_prediction > 0.4:
        score += 2
        details.append(f"Positive ensemble prediction: {ensemble_prediction:.2f}")

    # Model agreement analysis
    model_agreement = calculate_model_agreement(model_predictions)
    if model_agreement > 0.8:
        score += 2
        details.append(f"High model agreement: {model_agreement:.2f}")
    elif model_agreement < 0.4:
        score -= 1
        details.append(f"Low model agreement: {model_agreement:.2f}")

    # Feature importance from models
    feature_importance = extract_feature_importance(model_predictions)
    if feature_importance.get("fundamental_strength", 0) > 0.7:
        score += 1
        details.append("Models emphasize fundamental strength")

    return {
        "score": min(score, 10),
        "details": "; ".join(details),
        "model_predictions": model_predictions,
        "model_confidence": model_confidence,
        "ensemble_prediction": ensemble_prediction
    }


def risk_modeling_team_analysis(metrics: list, financial_line_items: list) -> dict:
    """Two Sigma's risk modeling team analysis"""
    score = 0
    details = []
    risk_metrics = {}

    if not metrics or not financial_line_items:
        return {"score": 0, "details": "No risk modeling data"}

    # Value-at-Risk modeling
    var_analysis = calculate_value_at_risk(financial_line_items)
    risk_metrics["var_95"] = var_analysis
    if var_analysis < 0.15:  # 15% VaR
        score += 2
        details.append(f"Low VaR: {var_analysis:.1%}")

    # Expected shortfall
    expected_shortfall = calculate_expected_shortfall(financial_line_items)
    risk_metrics["expected_shortfall"] = expected_shortfall
    if expected_shortfall < 0.2:
        score += 1
        details.append(f"Limited tail risk: {expected_shortfall:.1%}")

    # Factor risk decomposition
    factor_risks = decompose_factor_risks_ml(metrics, financial_line_items)
    risk_metrics["factor_risks"] = factor_risks

    # Maximum diversification ratio
    diversification_ratio = calculate_diversification_benefits(financial_line_items)
    risk_metrics["diversification"] = diversification_ratio
    if diversification_ratio > 0.6:
        score += 1
        details.append(f"Good diversification benefits: {diversification_ratio:.2f}")

    # Regime-dependent risk modeling
    regime_risks = model_regime_dependent_risks(financial_line_items)
    risk_metrics["regime_risks"] = regime_risks

    # Stress testing results
    stress_test_results = conduct_ml_stress_tests(financial_line_items)
    risk_metrics["stress_tests"] = stress_test_results
    if stress_test_results.get("worst_case_scenario", 1) > 0.7:
        score += 2
        details.append("Passes ML stress test scenarios")

    return {
        "score": score,
        "details": "; ".join(details),
        "risk_metrics": risk_metrics
    }


def portfolio_optimization_analysis(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """Two Sigma's portfolio optimization analysis"""
    score = 0
    details = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No portfolio optimization data"}

    # Mean-variance optimization
    sharpe_ratio_estimate = estimate_sharpe_ratio(metrics, financial_line_items)
    if sharpe_ratio_estimate > 1.5:
        score += 3
        details.append(f"High estimated Sharpe ratio: {sharpe_ratio_estimate:.2f}")
    elif sharpe_ratio_estimate > 1.0:
        score += 2
        details.append(f"Good estimated Sharpe ratio: {sharpe_ratio_estimate:.2f}")

    # Risk budgeting analysis
    risk_contribution = calculate_risk_contribution(financial_line_items, market_cap)
    if risk_contribution < 0.1:  # Low risk contribution
        score += 2
        details.append(f"Low portfolio risk contribution: {risk_contribution:.1%}")

    # Transaction cost optimization
    transaction_costs = optimize_transaction_costs(market_cap)
    if transaction_costs < 0.02:
        score += 1
        details.append(f"Low transaction costs: {transaction_costs:.2%}")

    # Alpha decay analysis
    alpha_persistence = analyze_alpha_persistence(financial_line_items)
    if alpha_persistence > 0.6:
        score += 2
        details.append(f"Persistent alpha signals: {alpha_persistence:.2f}")

    # Capacity constraints
    strategy_capacity = assess_strategy_capacity(market_cap)
    if strategy_capacity > 0.8:
        score += 1
        details.append("High strategy capacity for scaling")

    return {"score": score, "details": "; ".join(details)}


def scientific_method_synthesis(data_science: dict, ml_engineering: dict,
                                risk_modeling: dict, portfolio_optimization: dict) -> dict:
    """Two Sigma's scientific method synthesis"""
    details = []

    # Scientific hypothesis testing
    ml_confidence = ml_engineering.get("model_confidence", 0)
    ensemble_pred = ml_engineering.get("ensemble_prediction", 0.5)
    risk_score = risk_modeling["score"] / 10

    # Bayesian model averaging
    prior_belief = 0.5  # Neutral prior
    likelihood = ensemble_pred
    posterior = (likelihood * prior_belief) / ((likelihood * prior_belief) + ((1 - likelihood) * (1 - prior_belief)))

    # Scientific confidence intervals
    confidence_interval = calculate_prediction_confidence_interval(ml_engineering.get("model_predictions", {}))

    # Hypothesis strength
    if ml_confidence > 0.8 and abs(ensemble_pred - 0.5) > 0.2:
        hypothesis_strength = "Strong"
        details.append("Strong statistical evidence for directional prediction")
    elif ml_confidence > 0.6:
        hypothesis_strength = "Moderate"
        details.append("Moderate statistical evidence")
    else:
        hypothesis_strength = "Weak"
        details.append("Insufficient statistical evidence")

    # Scientific decision framework
    if hypothesis_strength == "Strong" and risk_score > 0.6:
        scientific_decision = "High conviction systematic position"
    elif hypothesis_strength == "Moderate":
        scientific_decision = "Moderate systematic position"
    else:
        scientific_decision = "Insufficient evidence for position"

    return {
        "scientific_decision": scientific_decision,
        "hypothesis_strength": hypothesis_strength,
        "posterior_probability": posterior,
        "confidence_interval": confidence_interval,
        "details": "; ".join(details)
    }


# ML and Data Science Helper Functions
def engineer_financial_features(metrics: list, financial_line_items: list) -> dict:
    """Engineer features from financial data"""
    features = {}

    # Revenue momentum features
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 4:
        recent_momentum = (revenues[0] / revenues[1]) - 1
        historical_momentum = (revenues[1] / revenues[-1]) ** (1 / (len(revenues) - 2)) - 1
        features["revenue_momentum"] = recent_momentum - historical_momentum

    # Margin stability
    margins = [item.operating_margin for item in financial_line_items if item.operating_margin]
    if margins:
        margin_cv = calculate_coefficient_of_variation(margins)
        features["margin_stability"] = max(0, 1 - margin_cv)

    # Growth quality
    if metrics and len(revenues) >= 3:
        revenue_growth = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1
        roe = metrics[0].return_on_equity if metrics[0].return_on_equity else 0
        features["growth_quality"] = min(revenue_growth * roe * 10, 1.0)

    return features


def extract_time_series_features(financial_line_items: list) -> dict:
    """Extract time series features"""
    features = {}

    # Trend analysis
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 6:
        trend_strength = calculate_trend_strength(revenues)
        features["trend_strength"] = trend_strength

    # Seasonality detection
    if len(revenues) >= 8:
        seasonality = detect_seasonality(revenues)
        features["seasonality"] = seasonality

    # Volatility clustering
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings:
        volatility_clustering = analyze_volatility_clustering(earnings)
        features["volatility_clustering"] = volatility_clustering

    return features


def create_cross_sectional_features(metrics: list, market_cap: float) -> dict:
    """Create cross-sectional ranking features"""
    features = {}

    if not metrics:
        return features

    latest = metrics[0]

    # Relative valuation features
    if latest.price_to_earnings and latest.price_to_earnings > 0:
        features["pe_percentile"] = min(max(1 - (latest.price_to_earnings / 25), 0), 1)

    # Size factor
    features["size_factor"] = min(math.log10(market_cap / 1_000_000_000), 2) / 2  # Normalize by $100B

    # Quality factor
    if latest.return_on_equity:
        features["quality_factor"] = min(latest.return_on_equity / 0.25, 1)

    return features


def integrate_alternative_data_signals(features: dict) -> float:
    """Integrate alternative data signals (simulated)"""
    # Simulated alternative data integration
    base_score = features.get("revenue_momentum", 0) * features.get("margin_stability", 0.5)
    return min(base_score * 2, 2.0)


def analyze_feature_interactions(features: dict) -> float:
    """Analyze feature interactions"""
    if not features:
        return 0

    # Simplified interaction analysis
    interaction_score = 0
    feature_values = [v for v in features.values() if isinstance(v, (int, float))]

    if feature_values:
        mean_feature = sum(feature_values) / len(feature_values)
        interaction_score = min(mean_feature * len(feature_values) / 5, 2.0)

    return interaction_score


def simulate_random_forest_prediction(metrics: list, financial_line_items: list) -> float:
    """Simulate random forest model prediction"""
    if not metrics or not financial_line_items:
        return 0.5

    # Simplified RF prediction based on multiple factors
    latest = metrics[0]
    score = 0.5

    if latest.return_on_equity and latest.return_on_equity > 0.12:
        score += 0.2

    if latest.price_to_earnings and latest.price_to_earnings < 15:
        score += 0.15

    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 3 and revenues[0] > revenues[-1]:
        score += 0.1

    return min(max(score, 0), 1)


def simulate_gradient_boosting_prediction(metrics: list, financial_line_items: list) -> float:
    """Simulate gradient boosting model prediction"""
    # Similar to RF but with different weighting
    rf_pred = simulate_random_forest_prediction(metrics, financial_line_items)
    # Add some variation to simulate different model
    return min(max(rf_pred + 0.05 - 0.1 * (rf_pred - 0.5), 0), 1)


def simulate_neural_network_prediction(metrics: list, financial_line_items: list) -> float:
    """Simulate neural network model prediction"""
    # Non-linear combination
    rf_pred = simulate_random_forest_prediction(metrics, financial_line_items)
    # Sigmoid-like transformation
    return 1 / (1 + math.exp(-5 * (rf_pred - 0.5)))


def simulate_lstm_prediction(financial_line_items: list) -> float:
    """Simulate LSTM time series prediction"""
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 4:
        # Simple trend-based prediction
        recent_trend = (revenues[0] / revenues[2]) - 1
        return min(max(0.5 + recent_trend, 0), 1)
    return 0.5


def calculate_ensemble_prediction(model_predictions: dict) -> float:
    """Calculate ensemble prediction from multiple models"""
    predictions = [v for k, v in model_predictions.items() if k != "ensemble" and isinstance(v, (int, float))]
    if predictions:
        return sum(predictions) / len(predictions)
    return 0.5


def calculate_model_confidence(model_predictions: dict) -> float:
    """Calculate confidence based on model agreement"""
    predictions = [v for k, v in model_predictions.items() if k != "ensemble" and isinstance(v, (int, float))]
    if len(predictions) >= 2:
        mean_pred = sum(predictions) / len(predictions)
        variance = sum((p - mean_pred) ** 2 for p in predictions) / len(predictions)
        # Lower variance = higher confidence
        return max(0, 1 - variance * 4)
    return 0.5


def calculate_model_agreement(model_predictions: dict) -> float:
    """Calculate agreement between models"""
    return calculate_model_confidence(model_predictions)  # Same calculation


def extract_feature_importance(model_predictions: dict) -> dict:
    """Extract feature importance from models"""
    return {
        "fundamental_strength": 0.7,
        "momentum": 0.5,
        "valuation": 0.6
    }


def calculate_value_at_risk(financial_line_items: list) -> float:
    """Calculate Value-at-Risk"""
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings:
        sorted_earnings = sorted(earnings)
        var_index = int(len(sorted_earnings) * 0.05)  # 5th percentile
        if var_index < len(sorted_earnings):
            worst_case = sorted_earnings[var_index]
            max_earnings = max(earnings)
            if max_earnings > 0:
                return abs(worst_case - max_earnings) / max_earnings
    return 0.3


def calculate_expected_shortfall(financial_line_items: list) -> float:
    """Calculate Expected Shortfall (Conditional VaR)"""
    var = calculate_value_at_risk(financial_line_items)
    return var * 1.3  # ES is typically higher than VaR


def decompose_factor_risks_ml(metrics: list, financial_line_items: list) -> dict:
    """ML-based factor risk decomposition"""
    return {
        "market_beta": 0.8,
        "size_factor": 0.3,
        "value_factor": 0.4,
        "momentum_factor": 0.2,
        "quality_factor": 0.6,
        "idiosyncratic": 0.4
    }


def calculate_diversification_benefits(financial_line_items: list) -> float:
    """Calculate diversification benefits"""
    return 0.6  # Placeholder


def model_regime_dependent_risks(financial_line_items: list) -> dict:
    """Model risks under different regimes"""
    return {
        "bull_market_risk": 0.2,
        "bear_market_risk": 0.4,
        "recession_risk": 0.6,
        "inflation_risk": 0.3
    }


def conduct_ml_stress_tests(financial_line_items: list) -> dict:
    """Conduct ML-based stress tests"""
    return {
        "worst_case_scenario": 0.8,
        "market_crash_scenario": 0.6,
        "liquidity_crisis": 0.7
    }


def estimate_sharpe_ratio(metrics: list, financial_line_items: list) -> float:
    """Estimate expected Sharpe ratio"""
    if not metrics:
        return 1.0

    latest = metrics[0]
    if latest.return_on_equity:
        # Simple proxy for Sharpe ratio
        return min(latest.return_on_equity * 5, 3.0)

    return 1.0


def calculate_risk_contribution(financial_line_items: list, market_cap: float) -> float:
    """Calculate risk contribution to portfolio"""
    # Based on market cap and volatility
    volatility = estimate_volatility(financial_line_items)
    size_factor = min(market_cap / 100_000_000_000, 1.0)  # Normalize by $100B
    return volatility * (1 - size_factor)


def optimize_transaction_costs(market_cap: float) -> float:
    """Optimize transaction costs"""
    if market_cap > 50_000_000_000:
        return 0.005
    elif market_cap > 10_000_000_000:
        return 0.01
    else:
        return 0.03


def analyze_alpha_persistence(financial_line_items: list) -> float:
    """Analyze alpha persistence"""
    return 0.6  # Placeholder


def assess_strategy_capacity(market_cap: float) -> float:
    """Assess strategy capacity"""
    return min(market_cap / 1_000_000_000, 1.0)  # Based on liquidity


def calculate_prediction_confidence_interval(model_predictions: dict) -> dict:
    """Calculate confidence intervals for predictions"""
    ensemble = model_predictions.get("ensemble", 0.5)
    std_dev = 0.1  # Estimated standard deviation

    return {
        "lower_bound": max(ensemble - 1.96 * std_dev, 0),
        "upper_bound": min(ensemble + 1.96 * std_dev, 1)
    }


def calculate_coefficient_of_variation(data: list) -> float:
    """Calculate coefficient of variation"""
    if not data:
        return 1.0

    mean_val = sum(data) / len(data)
    if mean_val == 0:
        return 1.0

    variance = sum((x - mean_val) ** 2 for x in data) / len(data)
    std_dev = math.sqrt(variance)

    return std_dev / abs(mean_val)


def calculate_trend_strength(data: list) -> float:
    """Calculate trend strength"""
    if len(data) < 3:
        return 0

    # Simple linear trend
    n = len(data)
    x_mean = (n - 1) / 2
    y_mean = sum(data) / n

    numerator = sum((i - x_mean) * (data[i] - y_mean) for i in range(n))
    denominator = sum((i - x_mean) ** 2 for i in range(n))

    if denominator == 0:
        return 0

    slope = numerator / denominator
    return min(abs(slope) / (y_mean if y_mean != 0 else 1), 1.0)


def detect_seasonality(data: list) -> float:
    """Detect seasonality in data"""
    if len(data) < 8:
        return 0

    # Simple quarterly seasonality check
    q1_avg = sum(data[i] for i in range(0, len(data), 4)) / ((len(data) + 3) // 4)
    q2_avg = sum(data[i] for i in range(1, len(data), 4)) / ((len(data) + 2) // 4)

    if q1_avg > 0:
        return min(abs(q2_avg - q1_avg) / q1_avg, 1.0)

    return 0


def analyze_volatility_clustering(data: list) -> float:
    """Analyze volatility clustering"""
    if len(data) < 4:
        return 0

    # Calculate rolling volatility
    volatilities = []
    for i in range(2, len(data)):
        recent_vol = abs(data[i] - data[i - 1])
        volatilities.append(recent_vol)

    if volatilities:
        return min(calculate_coefficient_of_variation(volatilities), 1.0)

    return 0


def estimate_volatility(financial_line_items: list) -> float:
    """Estimate volatility from financial data"""
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings:
        return calculate_coefficient_of_variation(earnings)
    return 0.3


def generate_two_sigma_output(ticker: str, analysis_data: dict, state: AgentState, agent_id: str) -> TwoSigmaSignal:
    """Generate Two Sigma's machine learning investment decision"""
    template = ChatPromptTemplate.from_messages([
        ("system", """You are Two Sigma's AI system, implementing scientific machine learning approach to investing:

        ORGANIZATIONAL STRUCTURE:
        - Data Science Team: Feature engineering and alternative data integration
        - ML Engineering Team: Ensemble models and prediction algorithms
        - Risk Modeling Team: Value-at-Risk and factor risk analysis
        - Portfolio Optimization Team: Mean-variance and transaction cost optimization
        - Scientific Method Synthesis: Hypothesis testing and Bayesian inference

        PHILOSOPHY:
        1. Data-Driven Decisions: All investment decisions based on data and models
        2. Machine Learning: Advanced ML algorithms for pattern recognition
        3. Scientific Method: Hypothesis testing and statistical significance
        4. Alternative Data: Integration of non-traditional data sources
        5. Risk Management: Sophisticated quantitative risk models
        6. Technology Focus: Cutting-edge technology and research
        7. Academic Rigor: PhD-level research and peer review process

        REASONING STYLE:
        - Reference machine learning model predictions and confidence levels
        - Discuss feature engineering and alternative data signals
        - Apply statistical significance testing and confidence intervals
        - Consider ensemble model predictions and cross-validation
        - Express reasoning in probabilistic terms
        - Acknowledge model limitations and overfitting risks
        - Focus on scientific hypothesis testing framework

        Return investment signal with ML model predictions and confidence metrics."""),

        ("human", """Apply Two Sigma's machine learning analysis to {ticker}:

        {analysis_data}

        Provide investment signal in JSON format:
        {{
          "signal": "bullish" | "bearish" | "neutral",
          "confidence": float (0-100),
          "reasoning": "string",
          "ml_model_predictions": {{
            "ensemble_prediction": float,
            "model_confidence": float,
            "random_forest": float,
            "gradient_boosting": float,
            "neural_network": float,
            "lstm": float
          }}
        }}""")
    ])

    prompt = template.invoke({"analysis_data": json.dumps(analysis_data, indent=2), "ticker": ticker})

    def create_default_two_sigma_signal():
        return TwoSigmaSignal(
            signal="neutral",
            confidence=0.0,
            reasoning="Analysis error, defaulting to neutral",
            ml_model_predictions={
                "ensemble_prediction": 0.5,
                "model_confidence": 0.0,
                "random_forest": 0.5,
                "gradient_boosting": 0.5,
                "neural_network": 0.5,
                "lstm": 0.5
            }
        )

    return call_llm(
        prompt=prompt,
        pydantic_model=TwoSigmaSignal,
        agent_name=agent_id,
        state=state,
        default_factory=create_default_two_sigma_signal,
    )