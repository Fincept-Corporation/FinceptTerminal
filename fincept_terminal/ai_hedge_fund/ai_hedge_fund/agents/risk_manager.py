import logging

class RiskManager:
    def __init__(self):
        self.logger = logging.getLogger(self.__class__.__name__)

    def assess_risk(self, returns, confidence_level=0.95) -> dict:
        if len(returns) < 2:
            return {"VaR": 0.0, "Confidence": confidence_level}
        sorted_returns = sorted(returns)
        idx = int((1 - confidence_level) * len(sorted_returns))
        var = sorted_returns[idx]
        return {"VaR": round(var, 4), "Confidence": confidence_level}
