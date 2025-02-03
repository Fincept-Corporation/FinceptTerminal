import json
from pydantic import BaseModel, Field
from typing_extensions import Literal
from langchain_core.messages import HumanMessage
from langchain_core.prompts import ChatPromptTemplate
from langchain_openai.chat_models import ChatOpenAI

from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.data.state import AgentState, show_agent_reasoning
from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.utils.progress import progress

class PortfolioDecision(BaseModel):
    action: Literal["buy", "sell", "hold"]
    quantity: int = Field(description="Number of shares to trade")
    confidence: float = Field(description="Confidence, 0.0 to 100.0")
    reasoning: str = Field(description="Decision rationale")

class PortfolioManagerOutput(BaseModel):
    decisions: dict[str, PortfolioDecision]

def portfolio_management_agent(state: AgentState):
    data = state["data"]
    portfolio = data["portfolio"]
    analyst_signals = data["analyst_signals"]
    tickers = data["tickers"]

    progress.update_status("portfolio_management_agent", None, "Aggregating signals")
    agg_signals = {}
    for ticker in tickers:
        progress.update_status("portfolio_management_agent", ticker, "Collecting signals")
        risk_data = analyst_signals.get("risk_management_agent", {}).get(ticker, {})
        pos_limit = risk_data.get("remaining_position_limit", 0)
        price = risk_data.get("current_price", 0)
        max_shares = int(pos_limit / price) if price > 0 else 0
        ticker_signals = {}
        weighted_score = 0.0
        total_conf = 0.0
        for agent, signals in analyst_signals.items():
            if agent == "risk_management_agent":
                continue
            if ticker not in signals:
                continue
            s = signals[ticker]
            sig = s.get("signal", "neutral")
            c = s.get("confidence", 0)
            if sig == "bullish":
                weighted_score += c
            elif sig == "bearish":
                weighted_score -= c
            total_conf += c
            ticker_signals[agent] = {"signal": sig, "confidence": c}
        final_score = weighted_score / (total_conf or 1)
        if final_score > 20:
            overall = "bullish"
            conf = abs(final_score)
        elif final_score < -20:
            overall = "bearish"
            conf = abs(final_score)
        else:
            overall = "neutral"
            conf = abs(final_score)
        agg_signals[ticker] = {
            "aggregated_signal": overall,
            "aggregated_confidence": round(conf, 2),
            "signals": ticker_signals,
            "max_shares": max_shares,
            "current_price": price,
        }

    progress.update_status("portfolio_management_agent", None, "Preparing prompt")
    prompt = ChatPromptTemplate.from_messages(
        [
            (
                "system",
                """You are a portfolio manager. Make trading decisions based on aggregated signals:
                - ...
                """,
            ),
            (
                "human",
                """Aggregated Signals:
                {agg_signals}

                Tickers: {tickers}

                Portfolio:
                Cash: {cash}
                Positions: {positions}

                Return a JSON formatted to match PortfolioManagerOutput.
                """
            ),
        ]
    ).invoke(
        {
            "agg_signals": json.dumps(agg_signals, indent=2),
            "tickers": json.dumps(tickers),
            "cash": f"{portfolio['cash']:.2f}",
            "positions": json.dumps(portfolio["positions"], indent=2),
        }
    )

    progress.update_status("portfolio_management_agent", None, "Invoking LLM")
    result = _invoke_portfolio_llm(prompt, tickers)
    message = HumanMessage(
        content=json.dumps({t: d.model_dump() for t, d in result.decisions.items()}),
        name="portfolio_management",
    )
    if state.get("metadata", {}).get("show_reasoning", False):
        show_agent_reasoning(
            {t: d.model_dump() for t, d in result.decisions.items()},
            "Portfolio Management Agent",
        )
    progress.update_status("portfolio_management_agent", None, "Done")
    return {"messages": state["messages"] + [message], "data": data}

def _invoke_portfolio_llm(prompt, tickers, max_retries=3):
    llm = ChatOpenAI(model="gpt-4o",openai_api_key="").with_structured_output(
        PortfolioManagerOutput, method="function_calling"
    )
    for attempt in range(max_retries):
        try:
            return llm.invoke(prompt)
        except Exception as e:
            progress.update_status("portfolio_management_agent", None, f"Error - retry {attempt+1}/{max_retries}")
            if attempt == max_retries - 1:
                return PortfolioManagerOutput(
                    decisions={
                        t: PortfolioDecision(
                            action="hold",
                            quantity=0,
                            confidence=0.0,
                            reasoning="Error in LLM. Default to hold."
                        )
                        for t in tickers
                    }
                )
