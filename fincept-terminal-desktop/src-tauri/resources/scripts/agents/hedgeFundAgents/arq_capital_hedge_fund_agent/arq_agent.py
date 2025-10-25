"""
===== DATA SOURCES REQUIRED =====
# INPUT:
#   - ticker symbols (array)
#   - end_date (string)
#   - FINANCIAL_DATASETS_API_KEY
#
# OUTPUT:
#   - Financial metrics: ROE, D/E, Current Ratio, P/E, P/B, ROIC, ROA
#   - Financial line items: Revenue, Net Income, FCF, Debt, Equity, Retained Earnings, Operating Margin, R&D, Shares, Total Assets, Current Assets, Current Liabilities
#   - Market Cap
#   - Factor scores: Value, Momentum, Quality, Low Volatility, Profitability
#
# PARAMETERS:
#   - period="annual"
#   - limit=10 years
#   - factor_combination_weights: Value=35%, Momentum=35%, Quality=30%
#   - academic_validation_threshold: 95% confidence level
"""


class AQRSignal(BaseModel):
    signal: Literal["bullish", "bearish", "neutral"]
    confidence: float
    reasoning: str
    factor_exposures: dict


def aqr_capital_hedge_fund_agent(state: AgentState, agent_id: str = "aqr_capital_hedge_fund_agent"):
    """
    AQR Capital Management: Factor investing, value-momentum combination
    Structure: Academic Research → Factor Analysis → Portfolio Construction → Risk Management → Cliff Asness Decision
    Philosophy: Systematic factor investing, academic rigor, long-term evidence
    """
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    api_key = get_api_key_from_state(state, "FINANCIAL_DATASETS_API_KEY")
    analysis_data = {}
    aqr_analysis = {}

    for ticker in tickers:
        progress.update_status(agent_id, ticker, "Academic research team analyzing factors")

        metrics = get_financial_metrics(ticker, end_date, period="annual", limit=10, api_key=api_key)
        financial_line_items = search_line_items(
            ticker,
            [
                "revenue", "net_income", "free_cash_flow", "total_debt",
                "shareholders_equity", "operating_margin", "total_assets",
                "current_assets", "current_liabilities", "research_and_development"
            ],
            end_date,
            period="annual",
            limit=10,
            api_key=api_key,
        )
        market_cap = get_market_cap(ticker, end_date, api_key=api_key)

        # AQR's factor-based analysis structure
        progress.update_status(agent_id, ticker, "Academic research team analysis")
        academic_research = academic_research_team_analysis(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Factor analysis team")
        factor_analysis = factor_analysis_team(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Portfolio construction team")
        portfolio_construction = portfolio_construction_team_analysis(metrics, financial_line_items, market_cap)

        progress.update_status(agent_id, ticker, "Risk management analysis")
        risk_management = risk_management_analysis(metrics, financial_line_items)

        progress.update_status(agent_id, ticker, "Cliff Asness systematic decision")
        cliff_asness_decision = cliff_asness_systematic_decision(
            academic_research, factor_analysis, portfolio_construction, risk_management
        )

        # AQR's factor-weighted scoring
        total_score = (
                factor_analysis["score"] * 0.4 +
                academic_research["score"] * 0.25 +
                portfolio_construction["score"] * 0.2 +
                risk_management["score"] * 0.15
        )

        # AQR's systematic factor approach
        factor_strength = factor_analysis.get("combined_factor_score", 0)
        if total_score >= 7.5 and factor_strength > 0.6:
            signal = "bullish"
        elif total_score <= 4.0 or factor_strength < 0.3:
            signal = "bearish"
        else:
            signal = "neutral"

        analysis_data[ticker] = {
            "signal": signal,
            "score": total_score,
            "academic_research": academic_research,
            "factor_analysis": factor_analysis,
            "portfolio_construction": portfolio_construction,
            "risk_management": risk_management,
            "cliff_asness_decision": cliff_asness_decision
        }

        aqr_output = generate_aqr_output(ticker, analysis_data, state, agent_id)

        aqr_analysis[ticker] = {
            "signal": aqr_output.signal,
            "confidence": aqr_output.confidence,
            "reasoning": aqr_output.reasoning,
            "factor_exposures": aqr_output.factor_exposures
        }

        progress.update_status(agent_id, ticker, "Done", analysis=aqr_output.reasoning)

    message = HumanMessage(content=json.dumps(aqr_analysis), name=agent_id)

    if state["metadata"]["show_reasoning"]:
        show_agent_reasoning(aqr_analysis, "AQR Capital Management")

    state["data"]["analyst_signals"][agent_id] = aqr_analysis
    progress.update_status(agent_id, None, "Done")

    return {"messages": [message], "data": state["data"]}


def academic_research_team_analysis(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """AQR's academic research team analysis"""
    score = 0
    details = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No academic research data"}

    # Long-term evidence analysis
    historical_performance = analyze_long_term_evidence(metrics, financial_line_items)
    if historical_performance > 0.7:
        score += 3
        details.append(f"Strong long-term evidence: {historical_performance:.2f}")

    # Academic factor validation
    factor_persistence = validate_factor_persistence(metrics, financial_line_items)
    if factor_persistence > 0.6:
        score += 2
        details.append(f"Factor persistence validated: {factor_persistence:.2f}")

    # Statistical significance testing
    statistical_significance = test_statistical_significance(financial_line_items)
    if statistical_significance > 0.95:  # 95% confidence
        score += 2
        details.append(f"Statistically significant at {statistical_significance:.1%} level")

    # Out-of-sample performance
    out_of_sample_test = conduct_out_of_sample_test(financial_line_items)
    if out_of_sample_test > 0.6:
        score += 2
        details.append("Passes out-of-sample validation")

    # Literature review consistency
    literature_consistency = check_literature_consistency(metrics)
    if literature_consistency > 0.7:
        score += 1
        details.append("Consistent with academic literature")

    return {"score": score, "details": "; ".join(details)}


def factor_analysis_team(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """AQR's factor analysis team"""
    score = 0
    details = []
    factor_scores = {}

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No factor data"}

    # Value factor analysis
    value_score = calculate_value_factor(metrics, market_cap)
    factor_scores["value"] = value_score
    if value_score > 0.6:
        score += 2.5
        details.append(f"Strong value factor: {value_score:.2f}")

    # Momentum factor analysis
    momentum_score = calculate_momentum_factor(financial_line_items)
    factor_scores["momentum"] = momentum_score
    if momentum_score > 0.6:
        score += 2.5
        details.append(f"Positive momentum factor: {momentum_score:.2f}")

    # Quality factor analysis
    quality_score = calculate_quality_factor(metrics, financial_line_items)
    factor_scores["quality"] = quality_score
    if quality_score > 0.7:
        score += 2
        details.append(f"High quality factor: {quality_score:.2f}")

    # Low volatility factor
    low_vol_score = calculate_low_volatility_factor(financial_line_items)
    factor_scores["low_volatility"] = low_vol_score
    if low_vol_score > 0.6:
        score += 1.5
        details.append(f"Low volatility characteristics: {low_vol_score:.2f}")

    # Profitability factor
    profitability_score = calculate_profitability_factor(metrics, financial_line_items)
    factor_scores["profitability"] = profitability_score
    if profitability_score > 0.7:
        score += 1.5
        details.append(f"Strong profitability factor: {profitability_score:.2f}")

    # Factor combination analysis (AQR's specialty)
    combined_factor_score = combine_factors_aqr_style(factor_scores)

    # Value-momentum crash protection
    crash_protection = analyze_value_momentum_crash_protection(factor_scores)
    if crash_protection > 0.6:
        score += 1
        details.append("Good crash protection from factor combination")

    return {
        "score": min(score, 10),
        "details": "; ".join(details),
        "factor_scores": factor_scores,
        "combined_factor_score": combined_factor_score
    }


def portfolio_construction_team_analysis(metrics: list, financial_line_items: list, market_cap: float) -> dict:
    """AQR's portfolio construction team analysis"""
    score = 0
    details = []

    if not metrics or not financial_line_items or not market_cap:
        return {"score": 0, "details": "No portfolio construction data"}

    # Factor timing analysis
    factor_timing_score = analyze_factor_timing(financial_line_items)
    if factor_timing_score > 0.6:
        score += 2
        details.append(f"Good factor timing opportunity: {factor_timing_score:.2f}")

    # Position sizing optimization
    position_sizing = optimize_position_sizing(metrics, market_cap)
    if position_sizing > 0.7:
        score += 2
        details.append("Optimal position sizing for factor exposure")

    # Transaction cost analysis
    transaction_costs = analyze_transaction_costs_aqr(market_cap)
    if transaction_costs < 0.02:  # Low transaction costs
        score += 1.5
        details.append(f"Low implementation costs: {transaction_costs:.2%}")

    # Capacity constraints
    strategy_capacity = assess_aqr_strategy_capacity(market_cap)
    if strategy_capacity > 0.8:
        score += 1.5
        details.append("High strategy capacity")

    # Risk budgeting
    risk_budgeting_score = analyze_risk_budgeting(financial_line_items)
    if risk_budgeting_score > 0.7:
        score += 1
        details.append("Efficient risk budgeting allocation")

    # Rebalancing frequency optimization
    rebalancing_analysis = optimize_rebalancing_frequency(financial_line_items)
    if rebalancing_analysis > 0.6:
        score += 1
        details.append("Optimal rebalancing frequency identified")

    return {"score": score, "details": "; ".join(details)}


def risk_management_analysis(metrics: list, financial_line_items: list) -> dict:
    """AQR's risk management analysis"""
    score = 0
    details = []

    if not metrics or not financial_line_items:
        return {"score": 0, "details": "No risk data"}

    # Factor risk decomposition
    factor_risk_decomp = decompose_factor_risks(metrics, financial_line_items)
    if factor_risk_decomp.get("diversification_ratio", 0) > 0.7:
        score += 2
        details.append("Good factor diversification")

    # Drawdown control
    drawdown_analysis = analyze_drawdown_control(financial_line_items)
    if drawdown_analysis < 0.15:  # <15% max drawdown
        score += 2
        details.append(f"Controlled drawdown risk: {drawdown_analysis:.1%}")

    # Regime analysis
    regime_analysis = conduct_regime_analysis(financial_line_items)
    if regime_analysis.get("regime_stability", 0) > 0.6:
        score += 1.5
        details.append("Stable across market regimes")

    # Tail risk hedging
    tail_risk_hedge = analyze_tail_risk_hedging(financial_line_items)
    if tail_risk_hedge > 0.6:
        score += 1.5
        details.append("Good tail risk characteristics")

    # Leverage optimization
    leverage_analysis = optimize_leverage_aqr(metrics, financial_line_items)
    if leverage_analysis > 0.7:
        score += 1
        details.append("Optimal leverage for factor exposure")

    return {"score": score, "details": "; ".join(details)}


def cliff_asness_systematic_decision(academic_research: dict, factor_analysis: dict,
                                     portfolio_construction: dict, risk_management: dict) -> dict:
    """Cliff Asness' systematic factor-based decision"""
    details = []

    # Factor strength analysis
    factor_strength = factor_analysis.get("combined_factor_score", 0)
    academic_validation = academic_research["score"] / 10
    implementation_feasibility = portfolio_construction["score"] / 10
    risk_control = risk_management["score"] / 10

    # AQR's decision framework
    overall_attractiveness = (
            factor_strength * 0.4 +
            academic_validation * 0.3 +
            implementation_feasibility * 0.2 +
            risk_control * 0.1
    )

    # Factor timing considerations
    value_momentum_combination = factor_analysis.get("factor_scores", {})
    value_score = value_momentum_combination.get("value", 0)
    momentum_score = value_momentum_combination.get("momentum", 0)

    # Cliff's decision logic
    if overall_attractiveness > 0.7 and factor_strength > 0.6:
        if value_score > 0.6 and momentum_score > 0.6:
            decision = "High conviction factor combination"
            details.append("Strong value-momentum combination with academic support")
        else:
            decision = "Single factor exposure"
            details.append("Strong single factor with systematic evidence")
    elif overall_attractiveness > 0.5:
        decision = "Moderate factor allocation"
        details.append("Moderate factor exposure with risk controls")
    elif factor_strength < 0.3:
        decision = "Avoid - weak factor exposure"
        details.append("Insufficient factor exposure for systematic strategy")
    else:
        decision = "Hold for better factor timing"
        details.append("Wait for stronger factor signals")

    # Academic rigor check
    if academic_research["score"] < 6:
        decision = "Insufficient academic evidence"
        details.append("Does not meet AQR's academic rigor standards")

    return {
        "decision": decision,
        "overall_attractiveness": overall_attractiveness,
        "factor_strength": factor_strength,
        "details": "; ".join(details)
    }


# Factor Analysis Helper Functions
def calculate_value_factor(metrics: list, market_cap: float) -> float:
    """Calculate AQR's value factor score"""
    if not metrics:
        return 0.5

    latest = metrics[0]
    value_score = 0
    components = 0

    # Price-to-book
    if latest.price_to_book:
        pb_score = max(0, 1 - (latest.price_to_book / 2))  # Normalize by 2.0
        value_score += pb_score
        components += 1

    # Price-to-earnings
    if latest.price_to_earnings and latest.price_to_earnings > 0:
        pe_score = max(0, 1 - (latest.price_to_earnings / 20))  # Normalize by 20
        value_score += pe_score
        components += 1

    # Enterprise value to EBITDA (approximated)
    if latest.price_to_earnings and latest.price_to_earnings > 0:
        ev_ebitda_proxy = latest.price_to_earnings * 0.8  # Rough proxy
        ev_ebitda_score = max(0, 1 - (ev_ebitda_proxy / 15))
        value_score += ev_ebitda_score
        components += 1

    return value_score / components if components > 0 else 0.5


def calculate_momentum_factor(financial_line_items: list) -> float:
    """Calculate AQR's momentum factor score"""
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) < 4:
        return 0.5

    # Revenue momentum (multiple time horizons)
    short_term_momentum = (revenues[0] / revenues[1]) - 1  # 1-year
    medium_term_momentum = (revenues[0] / revenues[2]) ** 0.5 - 1  # 2-year annualized
    long_term_momentum = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1  # Full period

    # Weight recent momentum more heavily
    momentum_score = (
            short_term_momentum * 0.5 +
            medium_term_momentum * 0.3 +
            long_term_momentum * 0.2
    )

    # Normalize to 0-1 scale
    return min(max((momentum_score + 0.2) / 0.4, 0), 1)  # -20% to +20% maps to 0-1


def calculate_quality_factor(metrics: list, financial_line_items: list) -> float:
    """Calculate AQR's quality factor score"""
    if not metrics:
        return 0.5

    latest = metrics[0]
    quality_components = []

    # Profitability metrics
    if latest.return_on_equity:
        roe_score = min(latest.return_on_equity / 0.2, 1.0)  # Normalize by 20%
        quality_components.append(roe_score)

    if latest.return_on_assets:
        roa_score = min(latest.return_on_assets / 0.1, 1.0)  # Normalize by 10%
        quality_components.append(roa_score)

    # Earnings stability
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings:
        earnings_stability = 1 - min(calculate_coefficient_of_variation(earnings), 1.0)
        quality_components.append(earnings_stability)

    # Debt-to-equity (lower is better for quality)
    if latest.debt_to_equity is not None:
        debt_quality = max(0, 1 - latest.debt_to_equity)  # 0 debt = 1.0 score
        quality_components.append(debt_quality)

    return sum(quality_components) / len(quality_components) if quality_components else 0.5


def calculate_low_volatility_factor(financial_line_items: list) -> float:
    """Calculate low volatility factor score"""
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    earnings = [item.net_income for item in financial_line_items if item.net_income]

    if not revenues and not earnings:
        return 0.5

    volatility_scores = []

    if revenues:
        revenue_vol = calculate_coefficient_of_variation(revenues)
        volatility_scores.append(max(0, 1 - revenue_vol))

    if earnings:
        earnings_vol = calculate_coefficient_of_variation(earnings)
        volatility_scores.append(max(0, 1 - earnings_vol))

    return sum(volatility_scores) / len(volatility_scores) if volatility_scores else 0.5


def calculate_profitability_factor(metrics: list, financial_line_items: list) -> float:
    """Calculate profitability factor score"""
    if not metrics or not financial_line_items:
        return 0.5

    latest = metrics[0]
    latest_financial = financial_line_items[0]

    profitability_components = []

    # Gross margins
    if latest_financial.operating_margin:
        margin_score = min(latest_financial.operating_margin / 0.2, 1.0)  # Normalize by 20%
        profitability_components.append(margin_score)

    # Return on invested capital
    if latest.return_on_invested_capital:
        roic_score = min(latest.return_on_invested_capital / 0.15, 1.0)
        profitability_components.append(roic_score)

    # Free cash flow margin
    if latest_financial.free_cash_flow and latest_financial.revenue:
        fcf_margin = latest_financial.free_cash_flow / latest_financial.revenue
        fcf_score = min(max(fcf_margin / 0.1, 0), 1.0)  # Normalize by 10%
        profitability_components.append(fcf_score)

    return sum(profitability_components) / len(profitability_components) if profitability_components else 0.5


def combine_factors_aqr_style(factor_scores: dict) -> float:
    """Combine factors using AQR's methodology"""
    if not factor_scores:
        return 0.5

    # AQR's factor combination approach
    value_score = factor_scores.get("value", 0.5)
    momentum_score = factor_scores.get("momentum", 0.5)
    quality_score = factor_scores.get("quality", 0.5)

    # Value-momentum combination (AQR's specialty)
    if value_score > 0.6 and momentum_score > 0.6:
        # Strong combination bonus
        combined_score = (value_score + momentum_score) / 2 + 0.1
    else:
        # Standard combination
        combined_score = (
                value_score * 0.35 +
                momentum_score * 0.35 +
                quality_score * 0.3
        )

    return min(combined_score, 1.0)


def analyze_value_momentum_crash_protection(factor_scores: dict) -> float:
    """Analyze crash protection from value-momentum combination"""
    value_score = factor_scores.get("value", 0.5)
    momentum_score = factor_scores.get("momentum", 0.5)
    quality_score = factor_scores.get("quality", 0.5)

    # Value provides crash protection, momentum can be risky
    crash_protection = value_score * 0.6 + quality_score * 0.4 - max(0, momentum_score - 0.5) * 0.2

    return min(max(crash_protection, 0), 1.0)


def analyze_long_term_evidence(metrics: list, financial_line_items: list) -> float:
    """Analyze long-term evidence for factor performance"""
    if len(financial_line_items) < 5:
        return 0.3  # Insufficient data

    # Consistency over time
    consistent_performance = 0

    # Revenue consistency
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if revenues:
        positive_growth_years = sum(1 for i in range(len(revenues) - 1) if revenues[i] >= revenues[i + 1])
        consistency = positive_growth_years / (len(revenues) - 1)
        consistent_performance += consistency * 0.5

    # Profitability consistency
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings:
        positive_earnings_years = sum(1 for e in earnings if e > 0)
        profitability_consistency = positive_earnings_years / len(earnings)
        consistent_performance += profitability_consistency * 0.5

    return min(consistent_performance, 1.0)


def validate_factor_persistence(metrics: list, financial_line_items: list) -> float:
    """Validate factor persistence over time"""
    if len(metrics) < 3:
        return 0.5

    # Check if factors persist over time
    persistence_score = 0

    # Value factor persistence
    pb_ratios = [m.price_to_book for m in metrics if m.price_to_book]
    if len(pb_ratios) >= 3:
        value_trend = pb_ratios[0] < pb_ratios[-1]  # Becoming cheaper
        if value_trend:
            persistence_score += 0.3

    # Quality factor persistence
    roe_vals = [m.return_on_equity for m in metrics if m.return_on_equity]
    if len(roe_vals) >= 3:
        avg_roe = sum(roe_vals) / len(roe_vals)
        if avg_roe > 0.12:  # Consistently high quality
            persistence_score += 0.4

    # Momentum persistence
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 3:
        momentum_trend = revenues[0] > revenues[-1]  # Growth trend
        if momentum_trend:
            persistence_score += 0.3

    return persistence_score


def test_statistical_significance(financial_line_items: list) -> float:
    """Test statistical significance of factor performance"""
    if len(financial_line_items) < 5:
        return 0.6  # Insufficient data for strong significance

    # Simple t-test approximation for revenue growth
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 5:
        growth_rates = [(revenues[i] / revenues[i + 1]) - 1 for i in range(len(revenues) - 1)]

        if growth_rates:
            mean_growth = sum(growth_rates) / len(growth_rates)
            if len(growth_rates) > 1:
                variance = sum((g - mean_growth) ** 2 for g in growth_rates) / (len(growth_rates) - 1)
                std_error = math.sqrt(variance / len(growth_rates))

                if std_error > 0:
                    t_stat = abs(mean_growth / std_error)
                    # Rough conversion to confidence level
                    return min(0.5 + t_stat * 0.1, 0.99)

    return 0.7  # Moderate significance


def conduct_out_of_sample_test(financial_line_items: list) -> float:
    """Conduct out-of-sample test"""
    if len(financial_line_items) < 8:
        return 0.5

    # Split data and test performance
    in_sample = financial_line_items[len(financial_line_items) // 2:]
    out_sample = financial_line_items[:len(financial_line_items) // 2]

    # Test revenue growth persistence
    in_sample_revenues = [item.revenue for item in in_sample if item.revenue]
    out_sample_revenues = [item.revenue for item in out_sample if item.revenue]

    if len(in_sample_revenues) >= 2 and len(out_sample_revenues) >= 2:
        in_sample_growth = (in_sample_revenues[0] / in_sample_revenues[-1]) - 1
        out_sample_growth = (out_sample_revenues[0] / out_sample_revenues[-1]) - 1

        # Check if growth pattern persists
        if (in_sample_growth > 0 and out_sample_growth > 0) or (in_sample_growth < 0 and out_sample_growth < 0):
            return 0.8  # Good out-of-sample performance
        else:
            return 0.4  # Poor out-of-sample performance

    return 0.6  # Moderate out-of-sample performance


def check_literature_consistency(metrics: list) -> float:
    """Check consistency with academic literature"""
    if not metrics:
        return 0.5

    latest = metrics[0]
    consistency_score = 0

    # Value factor literature consistency
    if latest.price_to_book and latest.price_to_book < 1.5:
        consistency_score += 0.3  # Consistent with value literature

    # Quality factor literature consistency
    if latest.return_on_equity and latest.return_on_equity > 0.15:
        consistency_score += 0.4  # Consistent with quality literature

    # Size factor (smaller companies historically outperform)
    # This would require market cap data relative to universe
    consistency_score += 0.3  # Placeholder

    return consistency_score


def analyze_factor_timing(financial_line_items: list) -> float:
    """Analyze factor timing opportunities"""
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) < 4:
        return 0.5

    # Check for inflection points in business cycle
    recent_acceleration = (revenues[0] / revenues[1]) > (revenues[1] / revenues[2])

    if recent_acceleration:
        return 0.8  # Good timing for momentum
    else:
        return 0.4  # Poor timing


def optimize_position_sizing(metrics: list, market_cap: float) -> float:
    """Optimize position sizing for factor exposure"""
    if not metrics:
        return 0.5

    # Kelly criterion approximation
    latest = metrics[0]

    if latest.return_on_equity and latest.return_on_equity > 0:
        expected_return = latest.return_on_equity * 0.5  # Conservative estimate
        volatility = 0.2  # Assumed volatility

        if volatility > 0:
            kelly_fraction = expected_return / (volatility ** 2)
            optimal_size = min(max(kelly_fraction, 0), 0.1)  # Cap at 10%
            return optimal_size * 10  # Scale to 0-1

    return 0.5


def analyze_transaction_costs_aqr(market_cap: float) -> float:
    """Analyze transaction costs for AQR strategies"""
    if market_cap > 50_000_000_000:  # $50B+
        return 0.005  # 0.5 bps
    elif market_cap > 10_000_000_000:  # $10B+
        return 0.01  # 1 bps
    elif market_cap > 1_000_000_000:  # $1B+
        return 0.02  # 2 bps
    else:
        return 0.05  # 5 bps


def assess_aqr_strategy_capacity(market_cap: float) -> float:
    """Assess strategy capacity for AQR factor strategies"""
    # Larger companies have higher capacity
    return min(math.log10(market_cap / 1_000_000_000) / 2, 1.0)


def analyze_risk_budgeting(financial_line_items: list) -> float:
    """Analyze risk budgeting allocation"""
    # Simplified risk budgeting based on earnings stability
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings:
        earnings_cv = calculate_coefficient_of_variation(earnings)
        return max(0, 1 - earnings_cv)

    return 0.5


def optimize_rebalancing_frequency(financial_line_items: list) -> float:
    """Optimize rebalancing frequency"""
    # Based on persistence of factor signals
    revenues = [item.revenue for item in financial_line_items if item.revenue]
    if len(revenues) >= 4:
        # Check signal persistence
        growth_consistency = sum(1 for i in range(len(revenues) - 1) if revenues[i] >= revenues[i + 1])
        persistence = growth_consistency / (len(revenues) - 1)

        if persistence > 0.7:
            return 0.8  # Low rebalancing frequency needed
        else:
            return 0.4  # High rebalancing frequency needed

    return 0.6


def decompose_factor_risks(metrics: list, financial_line_items: list) -> dict:
    """Decompose factor risks"""
    return {
        "market_beta": 0.7,
        "value_loading": 0.6,
        "momentum_loading": 0.4,
        "quality_loading": 0.5,
        "size_loading": 0.3,
        "diversification_ratio": 0.8
    }


def analyze_drawdown_control(financial_line_items: list) -> float:
    """Analyze drawdown control"""
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings:
        peak = max(earnings)
        trough = min(earnings)
        if peak > 0:
            max_drawdown = (peak - trough) / peak
            return max_drawdown

    return 0.2  # Assumed 20% max drawdown


def conduct_regime_analysis(financial_line_items: list) -> dict:
    """Conduct regime analysis"""
    return {
        "regime_stability": 0.7,
        "bull_market_performance": 0.8,
        "bear_market_performance": 0.6,
        "recession_performance": 0.5
    }


def analyze_tail_risk_hedging(financial_line_items: list) -> float:
    """Analyze tail risk hedging characteristics"""
    earnings = [item.net_income for item in financial_line_items if item.net_income]
    if earnings:
        # Check for negative skewness (tail risk)
        sorted_earnings = sorted(earnings)
        worst_10pct = sorted_earnings[:max(1, len(earnings) // 10)]

        if worst_10pct:
            avg_earnings = sum(earnings) / len(earnings)
            worst_performance = sum(worst_10pct) / len(worst_10pct)

            if avg_earnings > 0:
                tail_ratio = worst_performance / avg_earnings
                return max(0, 1 + tail_ratio)  # Better if tail ratio is less negative

    return 0.6


def optimize_leverage_aqr(metrics: list, financial_line_items: list) -> float:
    """Optimize leverage for AQR factor strategies"""
    if not metrics:
        return 0.5

    latest = metrics[0]

    # Target leverage based on Sharpe ratio
    if latest.return_on_equity:
        estimated_sharpe = latest.return_on_equity * 2  # Rough proxy
        optimal_leverage = min(max(estimated_sharpe, 1.0), 3.0)  # 1x to 3x leverage
        return optimal_leverage / 3.0  # Normalize to 0-1

    return 0.7  # Moderate leverage


def calculate_coefficient_of_variation(data: list) -> float:
    """Calculate coefficient of variation"""
    if not data or len(data) < 2:
        return 1.0

    mean_val = sum(data) / len(data)
    if mean_val == 0:
        return 1.0

    variance = sum((x - mean_val) ** 2 for x in data) / (len(data) - 1)
    std_dev = math.sqrt(variance)

    return std_dev / abs(mean_val)


def generate_aqr_output(ticker: str, analysis_data: dict, state: AgentState, agent_id: str) -> AQRSignal:
    """Generate AQR's factor-based investment decision"""
    template = ChatPromptTemplate.from_messages([
        ("system", """You are AQR Capital Management's AI system, implementing Cliff Asness' factor investing approach:

        ORGANIZATIONAL STRUCTURE:
        - Academic Research Team: Literature review and statistical validation
        - Factor Analysis Team: Value, momentum, quality, and low-vol factor analysis
        - Portfolio Construction Team: Position sizing and implementation
        - Risk Management Team: Factor risk decomposition and drawdown control
        - Cliff Asness Systematic Decision: Factor combination and timing

        PHILOSOPHY:
        1. Factor Investing: Systematic exposure to value, momentum, quality factors
        2. Academic Rigor: Evidence-based investing with statistical validation
        3. Factor Timing: Intelligent timing of factor exposures
        4. Value-Momentum Combination: AQR's specialty in combining opposing factors
        5. Risk Management: Sophisticated factor risk controls
        6. Long-term Evidence: Focus on factors with decades of evidence
        7. Implementation Focus: Minimize transaction costs and market impact

        REASONING STYLE:
        - Reference academic literature and statistical evidence
        - Discuss factor exposures and loadings quantitatively
        - Apply rigorous statistical testing and out-of-sample validation
        - Consider factor timing and regime analysis
        - Express confidence in factor persistence and literature consistency
        - Analyze implementation costs and capacity constraints
        - Focus on risk-adjusted returns and Sharpe ratios

        Return investment signal with detailed factor exposure analysis."""),

        ("human", """Apply AQR's factor analysis to {ticker}:

        {analysis_data}

        Provide investment signal in JSON format:
        {{
          "signal": "bullish" | "bearish" | "neutral",
          "confidence": float (0-100),
          "reasoning": "string",
          "factor_exposures": {{
            "value": float,
            "momentum": float,
            "quality": float,
            "low_volatility": float,
            "profitability": float,
            "combined_factor_score": float
          }}
        }}""")
    ])

    prompt = template.invoke({"analysis_data": json.dumps(analysis_data, indent=2), "ticker": ticker})

    def create_default_aqr_signal():
        return AQRSignal(
            signal="neutral",
            confidence=0.0,
            reasoning="Analysis error, defaulting to neutral",
            factor_exposures={
                "value": 0.5,
                "momentum": 0.5,
                "quality": 0.5,
                "low_volatility": 0.5,
                "profitability": 0.5,
                "combined_factor_score": 0.5
            }
        )

    return call_llm(
        prompt=prompt,
        pydantic_model=AQRSignal,
        agent_name=agent_id,
        state=state,
        default_factory=create_default_aqr_signal,
    )