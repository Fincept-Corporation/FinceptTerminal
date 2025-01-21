import logging
import numpy as np
from agents.sentiment_agent import SentimentAgent
from agents.valuation_agent import ValuationAgent
from agents.fundamentals_agent import FundamentalsAgent
from agents.technical_analyst import TechnicalAnalyst
from agents.risk_manager import RiskManager
from agents.portfolio_manager import PortfolioManager
from data.data_acquisition import get_historical_data
from data.data_preprocessing import preprocess_data
from utils.config import API_KEY

def setup_logger():
    logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(name)s - %(message)s")

def main():
    setup_logger()
    logger = logging.getLogger("MainApp")
    logger.info("Initializing agents")
    sentiment_agent = SentimentAgent(api_key=API_KEY)
    valuation_agent = ValuationAgent()
    fundamentals_agent = FundamentalsAgent()
    technical_agent = TechnicalAnalyst()
    risk_manager = RiskManager()
    portfolio_manager = PortfolioManager()
    txt = "This company smashed quarterly earnings estimates and expanded into new markets."
    logger.info("Running sentiment analysis")
    sent_result = sentiment_agent.analyze_sentiment(txt)
    t = "RITES.NS"
    s = "2024-01-01"
    e = "2024-12-31"
    hist_data = get_historical_data(t, s, e)
    clean_data = preprocess_data(hist_data)
    fundamentals_data = {
        "revenue": [50e9, 55e9, 60e9],
        "net_income": [10e9, 12e9, 14e9],
        "total_debt": [20e9, 22e9, 21e9],
        "total_assets": [100e9, 110e9, 120e9],
    }
    fund_result = fundamentals_agent.evaluate_fundamentals(fundamentals_data)
    tech_result = {}
    if not clean_data.empty:
        tech_result = technical_agent.analyze_technical_indicators(clean_data)
    recent_close = clean_data["Close"].iloc[-1] if not clean_data.empty else 150.0
    cap = 2.1e12
    net_inc = 14e9
    dep = 5e9
    capex = 3e9
    wc = 1e9
    fcf = 20e9
    val_result = valuation_agent.run_valuation_analysis(
        market_cap=cap,
        net_income=net_inc,
        depreciation=dep,
        capex=capex,
        working_capital_change=wc,
        free_cash_flow=fcf,
        growth_rate=0.07
    )
    logger.info("Calculating returns for risk assessment")
    daily_returns = clean_data["Close"].pct_change().dropna().tolist() if not clean_data.empty else []
    risk_result = risk_manager.assess_risk(daily_returns)
    signals = {
        "sentiment": sent_result,
        "valuation": val_result,
        "fundamentals": fund_result,
        "technicals": tech_result,
        "risk": risk_result,
    }
    logger.info("Making final trading decision")
    decision = portfolio_manager.make_trading_decision(signals, market_price=recent_close)
    logger.info("Signals: %s", signals)
    logger.info("Decision: %s", decision)

if __name__ == "__main__":
    main()
