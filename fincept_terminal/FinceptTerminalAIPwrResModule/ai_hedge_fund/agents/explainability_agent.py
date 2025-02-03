import logging

class ExplainabilityAgent:
    def __init__(self):
        self.logger = logging.getLogger(self.__class__.__name__)

    def generate_explanation(self, signals: dict) -> str:
        self.logger.info("Generating explanation for signals")
        reasons = []
        if "sentiment" in signals:
            reasons.append(f"Sentiment: {signals['sentiment'].get('sentiment', '')}")
        if "valuation" in signals:
            reasons.append(f"Valuation signal: {signals['valuation'].get('signal', '')}")
        if "fundamentals" in signals:
            reasons.append(f"Profit margin: {signals['fundamentals'].get('profit_margin', 0)}")
        if "technicals" in signals:
            reasons.append(f"MA Signal: {signals['technicals'].get('ma_signal', '')}")
        if "risk" in signals:
            reasons.append(f"VaR: {signals['risk'].get('VaR', 0)}")
        return " | ".join(reasons)
