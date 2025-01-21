import logging

class ValuationAgent:
    def __init__(self):
        self.logger = logging.getLogger(self.__class__.__name__)

    def calculate_owner_earnings_value(
        self,
        net_income: float,
        depreciation: float,
        capex: float,
        working_capital_change: float,
        growth_rate: float = 0.05,
        required_return: float = 0.15,
        margin_of_safety: float = 0.25,
        num_years: int = 5
    ) -> float:
        oe = net_income + depreciation - capex - working_capital_change
        if oe <= 0:
            return 0
        discounted = 0.0
        for year in range(1, num_years + 1):
            fv = oe * (1 + growth_rate) ** year
            dv = fv / ((1 + required_return) ** year)
            discounted += dv
        tg = min(growth_rate, 0.03)
        tv = (oe * (1 + tg) ** num_years) / (required_return - tg)
        tvd = tv / ((1 + required_return) ** num_years)
        iv = discounted + tvd
        return iv * (1 - margin_of_safety)

    def calculate_dcf_value(
        self,
        free_cash_flow: float,
        growth_rate: float = 0.05,
        discount_rate: float = 0.10,
        terminal_growth_rate: float = 0.02,
        num_years: int = 5
    ) -> float:
        cfs = [free_cash_flow * (1 + growth_rate) ** i for i in range(num_years)]
        pvs = []
        for i, cf in enumerate(cfs, start=1):
            pv = cf / ((1 + discount_rate) ** i)
            pvs.append(pv)
        tv = cfs[-1] * (1 + terminal_growth_rate) / (discount_rate - terminal_growth_rate)
        tvd = tv / ((1 + discount_rate) ** num_years)
        return sum(pvs) + tvd

    def compare_to_market(self, intrinsic_value: float, market_cap: float) -> float:
        gap = (intrinsic_value - market_cap) / (market_cap if market_cap else 1)
        self.logger.info("Valuation gap: %.2f%%", gap * 100)
        return gap

    def get_signal_from_gap(self, gap: float, threshold=0.15) -> str:
        if gap > threshold:
            return "bullish"
        elif gap < -threshold:
            return "bearish"
        else:
            return "neutral"

    def run_valuation_analysis(
        self,
        market_cap: float,
        net_income: float,
        depreciation: float,
        capex: float,
        working_capital_change: float,
        free_cash_flow: float,
        growth_rate: float = 0.05
    ) -> dict:
        oe_val = self.calculate_owner_earnings_value(net_income, depreciation, capex, working_capital_change, growth_rate)
        dcf_val = self.calculate_dcf_value(free_cash_flow, growth_rate)
        gap_oe = self.compare_to_market(oe_val, market_cap)
        gap_dcf = self.compare_to_market(dcf_val, market_cap)
        combined_gap = (gap_oe + gap_dcf) / 2
        signal = self.get_signal_from_gap(combined_gap)
        confidence = round(abs(combined_gap) * 100, 2)
        return {
            "oe_value": round(oe_val, 2),
            "dcf_value": round(dcf_val, 2),
            "market_cap": round(market_cap, 2),
            "gap_oe": round(gap_oe, 3),
            "gap_dcf": round(gap_dcf, 3),
            "combined_gap": round(combined_gap, 3),
            "signal": signal,
            "confidence": confidence,
        }
