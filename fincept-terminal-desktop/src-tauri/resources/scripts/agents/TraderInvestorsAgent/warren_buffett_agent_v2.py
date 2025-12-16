"""
Warren Buffett Agent - JSON-Driven Configuration
Version 2.0 - Uses finagent_core and JSON config files

User can customize via JSON:
- Scoring weights
- Thresholds
- Data sources
- Analysis rules
"""

import json
import sys
from pathlib import Path
from typing import Dict, Any, List

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from finagent_core.base_agent import AgentRegistry


class WarrenBuffettAgent:
    """JSON-driven Warren Buffett investment analysis agent"""

    def __init__(self, config_path: Path = None):
        if config_path is None:
            config_path = Path(__file__).parent / "configs" / "agent_definitions.json"

        registry = AgentRegistry(config_path)
        self.config = registry.get_agent("warren_buffett_agent")

        if not self.config:
            raise ValueError("Warren Buffett agent config not found")

        # Extract configuration sections
        self.weights = self.config._raw_config.get("scoring_weights", {})
        self.thresholds = self.config._raw_config.get("thresholds", {})
        self.rules = self.config._raw_config.get("analysis_rules", {})
        self.data_sources = self.config._raw_config.get("data_sources", {})

    def analyze(self, tickers: List[str], end_date: str, api_key: str = None) -> Dict:
        """
        Analyze stocks using Buffett's methodology with JSON-driven parameters

        Args:
            tickers: List of stock symbols
            end_date: Analysis end date
            api_key: Financial datasets API key

        Returns:
            Dictionary of analysis results per ticker
        """
        results = {}

        for ticker in tickers:
            try:
                # Fetch data based on JSON config
                metrics = self._fetch_financial_metrics(ticker, end_date, api_key)
                line_items = self._fetch_line_items(ticker, end_date, api_key)
                market_cap = self._fetch_market_cap(ticker, end_date, api_key)

                # Run analyses with JSON-configured parameters
                moat_score = self._analyze_economic_moat(metrics, line_items)
                earnings_score = self._analyze_earnings_predictability(line_items)
                strength_score = self._analyze_financial_strength(metrics, line_items)
                management_score = self._analyze_management_quality(line_items)
                valuation_score = self._analyze_valuation(line_items, market_cap)

                # Calculate total score using JSON weights
                total_score = (
                    moat_score["score"] * self.weights.get("moat_analysis", 0.3) +
                    earnings_score["score"] * self.weights.get("earnings_predictability", 0.25) +
                    strength_score["score"] * self.weights.get("financial_strength", 0.2) +
                    management_score["score"] * self.weights.get("management_quality", 0.15) +
                    valuation_score["score"] * self.weights.get("valuation", 0.1)
                )

                # Determine signal using JSON thresholds
                if total_score >= self.thresholds.get("bullish_score", 8.0):
                    signal = "bullish"
                elif total_score <= self.thresholds.get("bearish_score", 4.0):
                    signal = "bearish"
                else:
                    signal = "neutral"

                results[ticker] = {
                    "signal": signal,
                    "total_score": total_score,
                    "moat_analysis": moat_score,
                    "earnings_predictability": earnings_score,
                    "financial_strength": strength_score,
                    "management_quality": management_score,
                    "valuation_analysis": valuation_score,
                    "configuration_used": {
                        "weights": self.weights,
                        "thresholds": self.thresholds
                    }
                }

            except Exception as e:
                results[ticker] = {
                    "signal": "error",
                    "error": str(e)
                }

        return results

    def _analyze_economic_moat(self, metrics: List, line_items: List) -> Dict:
        """Analyze economic moat using JSON-configured rules"""
        score = 0
        details = []

        if not metrics:
            return {"score": 0, "details": "No metrics data"}

        # Get thresholds from JSON
        roe_threshold = self.thresholds.get("roe_excellence", 0.15)
        roe_years = self.thresholds.get("roe_years_required", 7)
        margin_stability = self.thresholds.get("margin_stability_max", 0.05)
        margin_min = self.thresholds.get("margin_minimum", 0.15)

        # Get points from JSON rules
        moat_rules = self.rules.get("moat_analysis", {})
        high_roe_points = moat_rules.get("high_roe_points", 3)
        good_roe_points = moat_rules.get("good_roe_points", 2)
        stable_margins_points = moat_rules.get("stable_margins_points", 2)
        steady_growth_points = moat_rules.get("steady_growth_points", 2)

        # High ROE analysis (JSON-configured)
        roe_vals = [m.get("return_on_equity") for m in metrics if m.get("return_on_equity")]
        if roe_vals:
            high_roe_count = sum(1 for roe in roe_vals if roe > roe_threshold)
            if high_roe_count >= roe_years:
                score += high_roe_points
                details.append(f"Exceptional ROE: {high_roe_count}/10 years >{roe_threshold:.0%}")
            elif high_roe_count >= 5:
                score += good_roe_points
                details.append(f"Good ROE: {high_roe_count}/10 years >{roe_threshold:.0%}")

        # Margin stability (JSON-configured)
        margins = [item.get("operating_margin") for item in line_items if item.get("operating_margin")]
        if margins and len(margins) >= 5:
            margin_vol = max(margins) - min(margins)
            avg_margin = sum(margins) / len(margins)
            if margin_vol < margin_stability and avg_margin > margin_min:
                score += stable_margins_points
                details.append(f"Stable margins: {avg_margin:.1%} avg")

        # Revenue growth (JSON-configured)
        revenues = [item.get("revenue") for item in line_items if item.get("revenue")]
        if len(revenues) >= 5:
            growth_min = self.thresholds.get("revenue_growth_min", 0.05)
            growth_max = self.thresholds.get("revenue_growth_max", 0.15)
            cagr = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1
            if growth_min <= cagr <= growth_max:
                score += steady_growth_points
                details.append(f"Steady growth: {cagr:.1%} CAGR")

        return {"score": score, "details": "; ".join(details)}

    def _analyze_earnings_predictability(self, line_items: List) -> Dict:
        """Analyze earnings predictability using JSON config"""
        score = 0
        details = []

        earnings = [item.get("net_income") for item in line_items if item.get("net_income")]
        if len(earnings) < 5:
            return {"score": 0, "details": "Insufficient data"}

        rules = self.rules.get("earnings_predictability", {})
        years_required = self.thresholds.get("positive_earnings_years", 8)

        # Consistent profitability
        positive_years = sum(1 for e in earnings if e > 0)
        if positive_years >= years_required:
            score += rules.get("consistent_profit_points", 3)
            details.append(f"Consistent: {positive_years}/10 years profitable")

        # Growth trend
        if len(earnings) >= 10:
            recent_avg = sum(earnings[:5]) / 5
            older_avg = sum(earnings[5:10]) / 5
            if recent_avg > older_avg * 1.5:
                score += rules.get("growth_trend_points", 2)
                details.append("Strong growth trend")

        # Low volatility
        if earnings:
            volatility = (max(earnings) - min(earnings)) / max(abs(max(earnings)), abs(min(earnings)))
            if volatility < 1.0:
                score += rules.get("low_volatility_points", 2)
                details.append(f"Low volatility: {volatility:.2f}")

        return {"score": score, "details": "; ".join(details)}

    def _analyze_financial_strength(self, metrics: List, line_items: List) -> Dict:
        """Analyze financial strength using JSON config"""
        score = 0
        details = []

        if not metrics or not line_items:
            return {"score": 0, "details": "No data"}

        latest = metrics[0] if metrics else {}
        rules = self.rules.get("financial_strength", {})

        # Low debt (JSON-configured)
        de_max = self.thresholds.get("debt_to_equity_max", 0.3)
        if latest.get("debt_to_equity") and latest["debt_to_equity"] < de_max:
            score += rules.get("low_debt_points", 3)
            details.append(f"Low debt: {latest['debt_to_equity']:.2f} D/E")

        # High liquidity (JSON-configured)
        cr_min = self.thresholds.get("current_ratio_min", 1.5)
        if latest.get("current_ratio") and latest["current_ratio"] > cr_min:
            score += rules.get("high_liquidity_points", 2)
            details.append(f"Strong liquidity: {latest['current_ratio']:.1f}")

        # Positive FCF (JSON-configured)
        fcf_vals = [item.get("free_cash_flow") for item in line_items if item.get("free_cash_flow")]
        fcf_years = self.thresholds.get("positive_fcf_years", 8)
        if fcf_vals:
            positive_fcf = sum(1 for fcf in fcf_vals if fcf > 0)
            if positive_fcf >= fcf_years:
                score += rules.get("positive_fcf_points", 2)
                details.append(f"Reliable FCF: {positive_fcf}/10 years")

        return {"score": score, "details": "; ".join(details)}

    def _analyze_management_quality(self, line_items: List) -> Dict:
        """Analyze management quality using JSON config"""
        score = 0
        details = []

        rules = self.rules.get("management_quality", {})

        # Share buybacks
        shares = [item.get("outstanding_shares") for item in line_items if item.get("outstanding_shares")]
        if len(shares) >= 5:
            reduction = (shares[-1] - shares[0]) / shares[-1]
            if reduction > 0.1:
                score += rules.get("buyback_points", 2)
                details.append(f"Buybacks: {reduction:.1%} reduction")

        # Retained earnings growth
        re_vals = [item.get("retained_earnings") for item in line_items if item.get("retained_earnings")]
        if len(re_vals) >= 5:
            re_growth = (re_vals[0] - re_vals[-1]) / abs(re_vals[-1])
            if re_growth > 0.5:
                score += rules.get("reinvestment_points", 2)
                details.append(f"Reinvestment: {re_growth:.1%} RE growth")

        return {"score": score, "details": "; ".join(details)}

    def _analyze_valuation(self, line_items: List, market_cap: float) -> Dict:
        """Placeholder for valuation analysis"""
        # User can add custom valuation logic
        return {"score": 5.0, "details": "Valuation analysis placeholder"}

    def _fetch_financial_metrics(self, ticker: str, end_date: str, api_key: str) -> List[Dict]:
        """Fetch metrics - placeholder for API integration"""
        # TODO: Integrate with actual financial API
        return []

    def _fetch_line_items(self, ticker: str, end_date: str, api_key: str) -> List[Dict]:
        """Fetch line items - placeholder for API integration"""
        # TODO: Integrate with actual financial API
        return []

    def _fetch_market_cap(self, ticker: str, end_date: str, api_key: str) -> float:
        """Fetch market cap - placeholder for API integration"""
        # TODO: Integrate with actual financial API
        return 0.0


def main():
    """CLI entry point for testing"""
    import argparse

    parser = argparse.ArgumentParser(description="Warren Buffett Agent - JSON Configured")
    parser.add_argument("--tickers", nargs="+", required=True, help="Stock tickers")
    parser.add_argument("--end-date", default="2024-12-31", help="Analysis end date")
    parser.add_argument("--api-key", help="Financial datasets API key")

    args = parser.parse_args()

    agent = WarrenBuffettAgent()
    results = agent.analyze(args.tickers, args.end_date, args.api_key)

    print(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
