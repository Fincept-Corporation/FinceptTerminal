import logging

from agents.sentiment_agent import SentimentAgent
from agents.valuation_agent import valuation_agent
from agents.fundamentals_agent import fundamentals_agent
from agents.technical_analyst import technical_analyst_agent
from agents.risk_manager import risk_management_agent
from agents.portfolio_manager import PortfolioManagementAgent

from data.data_acquisition import get_historical_data
from data.data_preprocessing import preprocess_data
from utils.config import API_KEY

from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.data.state import AgentState

def setup_logger():
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(name)s - %(message)s"
    )

def main():
    setup_logger()
    logger = logging.getLogger("MainApp")
    logger.info("Initializing pipeline")

    # Example usage of a class-based sentiment agent
    sentiment_agent = SentimentAgent(api_key=API_KEY)
    logger.info("Running sentiment analysis")
    text = "Company overcame supply chain disruptions and posted robust quarterly earnings."
    sentiment_result = sentiment_agent.analyze_sentiment(text)

    # Ticker and date range for data acquisition
    ticker = "AAPL"
    start_date = "2024-01-01"
    end_date = "2024-12-31"

    logger.info("Fetching historical data")
    hist_df = get_historical_data(ticker, start_date, end_date)

    logger.info("hist_df columns: %s", list(hist_df.columns))
    logger.info("hist_df head:\n%s", hist_df.head(3))

    clean_df = preprocess_data(hist_df)
    logger.info("clean_df columns: %s", list(clean_df.columns))
    logger.info("clean_df head:\n%s", clean_df.head(3))

    # We'll maintain a single AgentState throughout this pipeline
    # so each agent's signals accumulate in analyst_signals
    # while we reuse start_date/end_date/tickers/portfolio.
    state = AgentState(
        messages=[],
        data={
            "start_date": start_date,
            "end_date": end_date,
            "tickers": [ticker],
            "analyst_signals": {},
            "portfolio": {
                "cash": 100000.0,
                "positions": {
                    ticker: {
                        "cash": 0.0,
                        "shares": 200,
                        "ticker": ticker
                    }
                },
                # If your risk manager uses cost_basis, define it:
                "cost_basis": {
                    ticker: 200 * 150.0  # Example cost basis
                }
            }
        },
        metadata={"show_reasoning": True}
    )

    logger.info("Running fundamentals_agent")
    state = fundamentals_agent(state)

    logger.info("Running valuation_agent")
    state = valuation_agent(state)

    logger.info("Running technical_analyst_agent")
    state = technical_analyst_agent(state)

    logger.info("Running risk_management_agent")
    state = risk_management_agent(state)

    logger.info("Sentiment result: %s", sentiment_result)

    # Combine sentiment into the aggregator if you like
    # For demonstration, we store it in state["data"]["analyst_signals"]["sentiment_agent"]
    state["data"]["analyst_signals"]["sentiment_agent"] = {
        ticker: {
            "signal": "bullish" if "Positive" in sentiment_result["sentiment"] else "neutral",
            "confidence": 70.0,
            "rationale": sentiment_result["rationale"]
        }
    }

    # We can show final signals so far
    logger.info("Analyst Signals so far: %s", state["data"]["analyst_signals"])

    logger.info("Running portfolio_management_agent")
    # Initialize the Portfolio Management Agent with the Gemini API Key
    portfolio_manager = PortfolioManagementAgent(api_key=API_KEY)

    # Invoke the decision-making process
    final_state = portfolio_manager.make_decision(state)

    logger.info("Final portfolio management output:")
    logger.info(final_state["data"]["analyst_signals"].get("portfolio_management", {}))
    logger.info("Messages:\n%s", final_state["messages"])

if __name__ == "__main__":
    main()



























