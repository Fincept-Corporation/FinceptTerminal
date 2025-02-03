from langchain_core.messages import HumanMessage
from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.data.state import AgentState, show_agent_reasoning
from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.utils.progress import progress
import json

from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.data.data_acquisition import get_financial_metrics, get_market_cap, search_line_items

def valuation_agent(state: AgentState):
    data = state["data"]
    end_date = data["end_date"]
    tickers = data["tickers"]
    valuation_analysis = {}

    for ticker in tickers:
        progress.update_status("valuation_agent", ticker, "Fetching financial data")
        financial_metrics = get_financial_metrics(ticker=ticker, end_date=end_date, period="ttm")

        if not financial_metrics:
            progress.update_status("valuation_agent", ticker, "Failed: No financial metrics found")
            continue

        metrics = financial_metrics[0]
        progress.update_status("valuation_agent", ticker, "Gathering line items")
        line_items = search_line_items(
            ticker=ticker,
            line_items=[
                "free_cash_flow",
                "net_income",
                "depreciation_and_amortization",
                "capital_expenditure",
                "working_capital",
            ],
            end_date=end_date,
            period="ttm",
            limit=2,
        )
        if len(line_items) < 2:
            progress.update_status("valuation_agent", ticker, "Failed: Insufficient financial line items")
            continue

        current_item = line_items[0]
        previous_item = line_items[1]
        progress.update_status("valuation_agent", ticker, "Calculating owner earnings")

        working_capital_change = current_item.working_capital - previous_item.working_capital
        owner_earnings_value = calculate_owner_earnings_value(
            net_income=current_item.net_income,
            depreciation=current_item.depreciation_and_amortization,
            capex=current_item.capital_expenditure,
            working_capital_change=working_capital_change,
            growth_rate=metrics.earnings_growth,
            required_return=0.15,
            margin_of_safety=0.25,
        )

        progress.update_status("valuation_agent", ticker, "Calculating DCF value")
        dcf_value = calculate_intrinsic_value(
            free_cash_flow=current_item.free_cash_flow,
            growth_rate=metrics.earnings_growth,
            discount_rate=0.10,
            terminal_growth_rate=0.03,
            num_years=5,
        )

        progress.update_status("valuation_agent", ticker, "Comparing to market value")
        market_cap = get_market_cap(ticker=ticker, end_date=end_date)
        if not market_cap:
            progress.update_status("valuation_agent", ticker, "Failed: No market cap data")
            continue

        dcf_gap = (dcf_value - market_cap) / market_cap
        oe_gap = (owner_earnings_value - market_cap) / market_cap
        valuation_gap = (dcf_gap + oe_gap) / 2

        if valuation_gap > 0.15:
            signal = "bullish"
        elif valuation_gap < -0.15:
            signal = "bearish"
        else:
            signal = "neutral"

        reasoning = {
            "dcf_analysis": {
                "signal": "bullish" if dcf_gap > 0.15 else "bearish" if dcf_gap < -0.15 else "neutral",
                "details": f"Intrinsic Value: ${dcf_value:,.2f}, Market Cap: ${market_cap:,.2f}, Gap: {dcf_gap:.1%}",
            },
            "owner_earnings_analysis": {
                "signal": "bullish" if oe_gap > 0.15 else "bearish" if oe_gap < -0.15 else "neutral",
                "details": f"Owner Earnings Value: ${owner_earnings_value:,.2f}, Market Cap: ${market_cap:,.2f}, Gap: {oe_gap:.1%}",
            },
        }
        confidence = round(abs(valuation_gap), 2) * 100
        valuation_analysis[ticker] = {
            "signal": signal,
            "confidence": confidence,
            "reasoning": reasoning,
        }

        progress.update_status("valuation_agent", ticker, "Done")

    message = HumanMessage(content=json.dumps(valuation_analysis), name="valuation_agent")
    if state.get("metadata", {}).get("show_reasoning", False):
        show_agent_reasoning(valuation_analysis, "Valuation Analysis Agent")

    state["data"]["analyst_signals"]["valuation_agent"] = valuation_analysis
    return {"messages": [message], "data": data}

def calculate_owner_earnings_value(
    net_income: float,
    depreciation: float,
    capex: float,
    working_capital_change: float,
    growth_rate: float = 0.05,
    required_return: float = 0.15,
    margin_of_safety: float = 0.25,
    num_years: int = 5,
) -> float:
    if not all(isinstance(x, (int, float)) for x in [net_income, depreciation, capex, working_capital_change]):
        return 0
    owner_earnings = net_income + depreciation - capex - working_capital_change
    if owner_earnings <= 0:
        return 0
    future_values = []
    for year in range(1, num_years + 1):
        fv = owner_earnings * (1 + growth_rate) ** year
        dv = fv / ((1 + required_return) ** year)
        future_values.append(dv)
    tg = min(growth_rate, 0.03)
    tv = (future_values[-1] * (1 + tg)) / (required_return - tg)
    tv_discounted = tv / ((1 + required_return) ** num_years)
    intrinsic_value = sum(future_values) + tv_discounted
    return intrinsic_value * (1 - margin_of_safety)

def calculate_intrinsic_value(
    free_cash_flow: float,
    growth_rate: float = 0.05,
    discount_rate: float = 0.10,
    terminal_growth_rate: float = 0.02,
    num_years: int = 5,
) -> float:
    cfs = [free_cash_flow * (1 + growth_rate) ** i for i in range(num_years)]
    pvs = []
    for i, cf in enumerate(cfs, start=1):
        pv = cf / ((1 + discount_rate) ** i)
        pvs.append(pv)
    tv = cfs[-1] * (1 + terminal_growth_rate) / (discount_rate - terminal_growth_rate)
    tvd = tv / ((1 + discount_rate) ** num_years)
    return sum(pvs) + tvd

def calculate_working_capital_change(current_wc: float, previous_wc: float) -> float:
    return current_wc - previous_wc
