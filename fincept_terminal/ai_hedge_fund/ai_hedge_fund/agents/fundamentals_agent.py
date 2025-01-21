import logging

class FundamentalsAgent:
    def __init__(self):
        self.logger = logging.getLogger(self.__class__.__name__)

    def evaluate_fundamentals(self, statements: dict) -> dict:
        rev = statements.get("revenue", [0])
        net_inc = statements.get("net_income", [0])
        debt = statements.get("total_debt", [0])
        assets = statements.get("total_assets", [1])
        g = 0
        if rev[0] != 0:
            g = (rev[-1] - rev[0]) / abs(rev[0])
        pm = 0
        if net_inc[-1] and rev[-1]:
            pm = net_inc[-1] / rev[-1]
        dr = 0
        if debt[-1] and assets[-1]:
            dr = debt[-1] / assets[-1]
        return {"revenue_growth": round(g, 3), "profit_margin": round(pm, 3), "debt_ratio": round(dr, 3)}
