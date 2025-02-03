from langchain_core.messages import HumanMessage
from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.data.state import AgentState, show_agent_reasoning
from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.utils.progress import progress
from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.data.data_acquisition import get_prices, prices_to_df
import json

def risk_management_agent(state: AgentState):
    """
    Controls position sizing based on real-world risk factors for multiple tickers.
    Fetches price data, determines current position value, and limits additional
    exposure for each ticker.
    """
    portfolio = state["data"]["portfolio"]
    data = state["data"]
    tickers = data["tickers"]
    risk_analysis = {}
    current_prices = {}

    for ticker in tickers:
        progress.update_status("risk_management_agent", ticker, "Analyzing price data")

        prices = get_prices(
            ticker=ticker,
            start_date=data["start_date"],
            end_date=data["end_date"],
        )
        if not prices:
            progress.update_status("risk_management_agent", ticker, "Failed: No price data found")
            continue

        prices_df = prices_to_df(prices)
        progress.update_status("risk_management_agent", ticker, "Calculating position limits")

        current_price = prices_df["close"].iloc[-1]
        current_prices[ticker] = current_price

        current_position_value = portfolio.get("cost_basis", {}).get(ticker, 0)
        total_portfolio_value = portfolio.get("cash", 0) + sum(
            portfolio.get("cost_basis", {}).get(t, 0) for t in portfolio.get("cost_basis", {})
        )

        position_limit = total_portfolio_value * 0.20
        remaining_position_limit = position_limit - current_position_value
        max_position_size = min(remaining_position_limit, portfolio.get("cash", 0))

        risk_analysis[ticker] = {
            "remaining_position_limit": float(max_position_size),
            "current_price": float(current_price),
            "reasoning": {
                "portfolio_value": float(total_portfolio_value),
                "current_position": float(current_position_value),
                "position_limit": float(position_limit),
                "remaining_limit": float(remaining_position_limit),
                "available_cash": float(portfolio.get("cash", 0)),
            },
        }

        progress.update_status("risk_management_agent", ticker, "Done")

    message = HumanMessage(
        content=json.dumps(risk_analysis),
        name="risk_management_agent",
    )

    if state.get("metadata", {}).get("show_reasoning", False):
        show_agent_reasoning(risk_analysis, "Risk Management Agent")

    state["data"]["analyst_signals"]["risk_management_agent"] = risk_analysis
    return {"messages": state["messages"] + [message], "data": data}
