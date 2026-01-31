"""Premium Analysis for Fairness Opinions"""
from typing import Dict, Any, List, Optional
from datetime import datetime, timedelta
import numpy as np

class PremiumAnalysis:
    """Analyze acquisition premiums for fairness opinion"""

    def __init__(self, offer_price_per_share: float,
                 shares_outstanding: float):
        self.offer_price_per_share = offer_price_per_share
        self.shares_outstanding = shares_outstanding
        self.offer_value = offer_price_per_share * shares_outstanding

    def calculate_premiums(self, historical_prices: Dict[str, float]) -> Dict[str, Any]:
        """
        Calculate premiums to various historical prices

        historical_prices: {'1_day': price, '1_week': price, '1_month': price, ...}
        """

        premiums = {}

        for period, price in historical_prices.items():
            premium_pct = ((self.offer_price_per_share - price) / price * 100) if price > 0 else 0
            premium_dollar = self.offer_price_per_share - price

            premiums[period] = {
                'historical_price': price,
                'premium_pct': premium_pct,
                'premium_dollar': premium_dollar
            }

        return {
            'offer_price_per_share': self.offer_price_per_share,
            'premiums': premiums,
            'offer_value': self.offer_value
        }

    def unaffected_price_analysis(self, daily_prices: List[float],
                                  rumor_date: Optional[int] = None) -> Dict[str, Any]:
        """Determine unaffected stock price before deal rumors"""

        if rumor_date is None:
            rumor_date = len(daily_prices)

        unaffected_prices = daily_prices[:rumor_date]

        if not unaffected_prices:
            raise ValueError("No unaffected prices available")

        avg_1_day = unaffected_prices[-1] if len(unaffected_prices) >= 1 else 0
        avg_5_day = np.mean(unaffected_prices[-5:]) if len(unaffected_prices) >= 5 else avg_1_day
        avg_20_day = np.mean(unaffected_prices[-20:]) if len(unaffected_prices) >= 20 else avg_5_day
        avg_60_day = np.mean(unaffected_prices[-60:]) if len(unaffected_prices) >= 60 else avg_20_day

        premium_1_day = ((self.offer_price_per_share - avg_1_day) / avg_1_day * 100) if avg_1_day > 0 else 0
        premium_5_day = ((self.offer_price_per_share - avg_5_day) / avg_5_day * 100) if avg_5_day > 0 else 0
        premium_20_day = ((self.offer_price_per_share - avg_20_day) / avg_20_day * 100) if avg_20_day > 0 else 0
        premium_60_day = ((self.offer_price_per_share - avg_60_day) / avg_60_day * 100) if avg_60_day > 0 else 0

        return {
            'unaffected_prices': {
                '1_day_prior': avg_1_day,
                '5_day_avg': avg_5_day,
                '20_day_avg': avg_20_day,
                '60_day_avg': avg_60_day
            },
            'premiums_to_unaffected': {
                '1_day': premium_1_day,
                '5_day': premium_5_day,
                '20_day': premium_20_day,
                '60_day': premium_60_day
            },
            'offer_price': self.offer_price_per_share,
            'rumor_date_index': rumor_date
        }

    def benchmark_premiums(self, industry: str = 'general') -> Dict[str, Any]:
        """Provide benchmark acquisition premiums by industry"""

        industry_benchmarks = {
            'general': {
                '1_day': {'25th': 15, 'median': 30, '75th': 45},
                '1_week': {'25th': 18, 'median': 33, '75th': 48},
                '4_week': {'25th': 22, 'median': 37, '75th': 52}
            },
            'technology': {
                '1_day': {'25th': 20, 'median': 35, '75th': 50},
                '1_week': {'25th': 23, 'median': 38, '75th': 53},
                '4_week': {'25th': 27, 'median': 42, '75th': 57}
            },
            'healthcare': {
                '1_day': {'25th': 25, 'median': 40, '75th': 55},
                '1_week': {'25th': 28, 'median': 43, '75th': 58},
                '4_week': {'25th': 32, 'median': 47, '75th': 62}
            },
            'financial': {
                '1_day': {'25th': 12, 'median': 25, '75th': 38},
                '1_week': {'25th': 15, 'median': 28, '75th': 41},
                '4_week': {'25th': 19, 'median': 32, '75th': 45}
            },
            'industrials': {
                '1_day': {'25th': 18, 'median': 32, '75th': 46},
                '1_week': {'25th': 21, 'median': 35, '75th': 49},
                '4_week': {'25th': 25, 'median': 39, '75th': 53}
            }
        }

        benchmarks = industry_benchmarks.get(industry.lower(), industry_benchmarks['general'])

        return {
            'industry': industry,
            'benchmark_premiums': benchmarks,
            'source': 'Historical M&A transaction data'
        }

    def compare_to_benchmarks(self, actual_premium: float,
                             period: str = '1_day',
                             industry: str = 'general') -> Dict[str, Any]:
        """Compare actual premium to industry benchmarks"""

        benchmarks = self.benchmark_premiums(industry)
        period_benchmarks = benchmarks['benchmark_premiums'].get(period, benchmarks['benchmark_premiums']['1_day'])

        percentile_25 = period_benchmarks['25th']
        percentile_50 = period_benchmarks['median']
        percentile_75 = period_benchmarks['75th']

        if actual_premium < percentile_25:
            position = "below 25th percentile"
        elif actual_premium < percentile_50:
            position = "between 25th and 50th percentile"
        elif actual_premium < percentile_75:
            position = "between 50th and 75th percentile"
        else:
            position = "above 75th percentile"

        return {
            'actual_premium': actual_premium,
            'period': period,
            'industry': industry,
            'benchmarks': period_benchmarks,
            'relative_position': position,
            'above_median': actual_premium > percentile_50
        }

    def premium_adequacy_analysis(self, actual_premium: float,
                                 control_premium_justified: bool,
                                 synergy_value: Optional[float] = None,
                                 standalone_value: Optional[float] = None) -> Dict[str, Any]:
        """Analyze whether premium is adequate"""

        typical_control_premium = 30.0

        if control_premium_justified:
            min_expected_premium = typical_control_premium
        else:
            min_expected_premium = 15.0

        premium_adequate = actual_premium >= min_expected_premium

        if synergy_value and standalone_value:
            total_value = standalone_value + synergy_value
            premium_as_pct_of_synergies = ((self.offer_value - standalone_value) / synergy_value * 100) if synergy_value > 0 else 0
            synergy_capture_fair = 20 <= premium_as_pct_of_synergies <= 50
        else:
            premium_as_pct_of_synergies = None
            synergy_capture_fair = None

        return {
            'actual_premium_pct': actual_premium,
            'control_premium_expected': control_premium_justified,
            'minimum_expected_premium': min_expected_premium,
            'premium_adequate': premium_adequate,
            'synergy_analysis': {
                'synergy_value': synergy_value,
                'premium_as_pct_of_synergies': premium_as_pct_of_synergies,
                'synergy_split_reasonable': synergy_capture_fair
            } if synergy_value and standalone_value else None
        }

    def fifty_two_week_analysis(self, week_52_high: float,
                                week_52_low: float,
                                current_price: float) -> Dict[str, Any]:
        """Analyze offer relative to 52-week trading range"""

        premium_to_52w_high = ((self.offer_price_per_share - week_52_high) / week_52_high * 100) if week_52_high > 0 else 0
        premium_to_52w_low = ((self.offer_price_per_share - week_52_low) / week_52_low * 100) if week_52_low > 0 else 0

        range_position = ((self.offer_price_per_share - week_52_low) / (week_52_high - week_52_low) * 100) if (week_52_high - week_52_low) > 0 else 0

        above_52w_high = self.offer_price_per_share > week_52_high

        current_vs_52w_high = ((current_price - week_52_high) / week_52_high * 100) if week_52_high > 0 else 0

        return {
            '52_week_high': week_52_high,
            '52_week_low': week_52_low,
            'current_price': current_price,
            'offer_price': self.offer_price_per_share,
            'premium_to_52w_high': premium_to_52w_high,
            'premium_to_52w_low': premium_to_52w_low,
            'position_in_52w_range_pct': range_position,
            'above_52w_high': above_52w_high,
            'current_vs_52w_high_pct': current_vs_52w_high,
            'interpretation': 'strong offer' if above_52w_high else 'within historical range'
        }

    def volume_weighted_average_price(self, prices: List[float],
                                     volumes: List[int],
                                     days: int = 30) -> Dict[str, Any]:
        """Calculate VWAP and premium"""

        if len(prices) != len(volumes):
            raise ValueError("Prices and volumes must have same length")

        recent_prices = prices[-days:]
        recent_volumes = volumes[-days:]

        vwap = sum(p * v for p, v in zip(recent_prices, recent_volumes)) / sum(recent_volumes) if sum(recent_volumes) > 0 else 0

        premium_to_vwap = ((self.offer_price_per_share - vwap) / vwap * 100) if vwap > 0 else 0

        return {
            'period_days': days,
            'vwap': vwap,
            'offer_price': self.offer_price_per_share,
            'premium_to_vwap': premium_to_vwap,
            'total_volume': sum(recent_volumes)
        }

    def analyst_target_comparison(self, analyst_targets: List[float]) -> Dict[str, Any]:
        """Compare offer to analyst price targets"""

        if not analyst_targets:
            return {'error': 'No analyst targets provided'}

        mean_target = np.mean(analyst_targets)
        median_target = np.median(analyst_targets)
        min_target = min(analyst_targets)
        max_target = max(analyst_targets)

        premium_to_mean = ((self.offer_price_per_share - mean_target) / mean_target * 100) if mean_target > 0 else 0
        premium_to_median = ((self.offer_price_per_share - median_target) / median_target * 100) if median_target > 0 else 0

        above_all_targets = self.offer_price_per_share > max_target
        below_all_targets = self.offer_price_per_share < min_target

        return {
            'analyst_targets': {
                'count': len(analyst_targets),
                'mean': mean_target,
                'median': median_target,
                'min': min_target,
                'max': max_target
            },
            'offer_price': self.offer_price_per_share,
            'premium_to_mean': premium_to_mean,
            'premium_to_median': premium_to_median,
            'above_all_targets': above_all_targets,
            'below_all_targets': below_all_targets,
            'within_analyst_range': min_target <= self.offer_price_per_share <= max_target
        }

def main():
    """CLI entry point - outputs JSON for Tauri integration"""
    import sys
    import json

    if len(sys.argv) < 2:
        result = {"success": False, "error": "No command specified"}
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "premium":
            if len(sys.argv) < 4:
                raise ValueError("Daily prices (JSON), offer price, and announcement date required")

            daily_prices = json.loads(sys.argv[2])
            offer_price = float(sys.argv[3])
            announcement_date = int(sys.argv[4]) if len(sys.argv) > 4 else None

            shares_outstanding = 50_000_000  # Default or could be passed as param

            analyzer = PremiumAnalysis(
                offer_price_per_share=offer_price,
                shares_outstanding=shares_outstanding
            )

            if announcement_date:
                analysis = analyzer.unaffected_price_analysis(daily_prices, rumor_date=announcement_date)
            else:
                # Use last price as current price
                historical_prices = {
                    '1_day': daily_prices[-1] if daily_prices else 0,
                    '1_week': daily_prices[-5] if len(daily_prices) >= 5 else daily_prices[0],
                    '1_month': daily_prices[-20] if len(daily_prices) >= 20 else daily_prices[0]
                }
                analysis = analyzer.calculate_premiums(historical_prices)

            result = {"success": True, "data": analysis}
            print(json.dumps(result))

        else:
            result = {"success": False, "error": f"Unknown command: {command}"}
            print(json.dumps(result))
            sys.exit(1)

    except Exception as e:
        result = {"success": False, "error": str(e)}
        print(json.dumps(result))
        sys.exit(1)

if __name__ == '__main__':
    main()
