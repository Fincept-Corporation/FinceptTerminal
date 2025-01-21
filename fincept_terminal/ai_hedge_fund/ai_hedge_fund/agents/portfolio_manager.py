import logging

class PortfolioManager:
    def __init__(self):
        self.logger = logging.getLogger(self.__class__.__name__)

    def make_trading_decision(self, signals: dict, market_price: float) -> dict:
        sentiment_signal = signals.get("sentiment", {})
        valuation_signal = signals.get("valuation", {})
        fundamentals_signal = signals.get("fundamentals", {})
        technicals_signal = signals.get("technicals", {})
        risk_signal = signals.get("risk", {})
        reasons = []
        if "Positive" in sentiment_signal.get("sentiment", ""):
            reasons.append("Positive Sentiment")
        if valuation_signal.get("signal") == "bullish":
            reasons.append("Undervalued")
        if fundamentals_signal.get("profit_margin", 0) > 0.15:
            reasons.append("Healthy Fundamentals")
        if technicals_signal.get("ma_signal") == "BUY":
            reasons.append("Technical BUY")
        elif technicals_signal.get("ma_signal") == "SELL":
            reasons.append("Technical SELL")
        if risk_signal.get("VaR", 0) > -0.05:
            reasons.append("Risk Acceptable")
        d = "HOLD"
        if any(r for r in reasons if "SELL" in r):
            d = "SELL"
        elif any(r for r in reasons if "BUY" in r or "Undervalued" in r):
            d = "BUY"
        joined = ", ".join(reasons) if reasons else "No strong signal"
        self.logger.info("Decision: %s, Reasons: %s", d, joined)
        return {"decision": d, "reason": joined, "market_price": market_price, "sentiment_rationale": sentiment_signal.get("rationale", "")}
