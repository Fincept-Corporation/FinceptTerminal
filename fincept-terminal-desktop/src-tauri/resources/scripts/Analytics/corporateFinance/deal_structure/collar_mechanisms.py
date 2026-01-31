"""Collar Mechanisms for Stock Deal Protection"""
from typing import Dict, Any, List, Optional
import numpy as np

class CollarType:
    FIXED = "fixed"
    FLOATING = "floating"
    SYMMETRIC = "symmetric"
    ASYMMETRIC = "asymmetric"

class CollarMechanism:
    """Analyze collar structures in stock-for-stock M&A transactions"""

    def __init__(self, announcement_price: float,
                 target_shares: float,
                 base_exchange_ratio: float):
        self.announcement_price = announcement_price
        self.target_shares = target_shares
        self.base_exchange_ratio = base_exchange_ratio

    def fixed_collar(self, floor_price: float,
                    cap_price: float,
                    price_scenarios: Optional[List[float]] = None) -> Dict[str, Any]:
        """
        Fixed collar: Exchange ratio adjusts to maintain value within price band

        If acquirer price falls below floor: ratio increases (more shares)
        If acquirer price rises above cap: ratio decreases (fewer shares)
        """

        if price_scenarios is None:
            price_scenarios = np.linspace(
                self.announcement_price * 0.7,
                self.announcement_price * 1.3,
                20
            ).tolist()

        scenarios = []
        base_value = self.base_exchange_ratio * self.announcement_price

        for price in price_scenarios:
            if price < floor_price:
                effective_ratio = base_value / floor_price
                value_per_share = floor_price
                protection_type = "floor_active"
            elif price > cap_price:
                effective_ratio = base_value / cap_price
                value_per_share = cap_price
                protection_type = "cap_active"
            else:
                effective_ratio = self.base_exchange_ratio
                value_per_share = price
                protection_type = "within_collar"

            actual_value = effective_ratio * price

            total_deal_value = actual_value * self.target_shares
            total_shares_issued = effective_ratio * self.target_shares

            scenarios.append({
                'acquirer_price': price,
                'effective_exchange_ratio': effective_ratio,
                'value_per_target_share': actual_value,
                'total_deal_value': total_deal_value,
                'shares_issued': total_shares_issued,
                'protection_active': protection_type,
                'price_change_pct': ((price - self.announcement_price) / self.announcement_price) * 100
            })

        return {
            'collar_type': 'fixed',
            'announcement_price': self.announcement_price,
            'base_exchange_ratio': self.base_exchange_ratio,
            'floor_price': floor_price,
            'cap_price': cap_price,
            'floor_pct_below': ((self.announcement_price - floor_price) / self.announcement_price) * 100,
            'cap_pct_above': ((cap_price - self.announcement_price) / self.announcement_price) * 100,
            'scenarios': scenarios,
            'value_range': {
                'min': min(s['value_per_target_share'] for s in scenarios),
                'max': max(s['value_per_target_share'] for s in scenarios),
                'base': self.base_exchange_ratio * self.announcement_price
            }
        }

    def floating_collar(self, min_exchange_ratio: float,
                       max_exchange_ratio: float,
                       price_scenarios: Optional[List[float]] = None) -> Dict[str, Any]:
        """
        Floating collar: Exchange ratio bounded, value floats with stock price

        Min ratio: Floor on shares received (downside protection for target)
        Max ratio: Cap on shares issued (dilution protection for acquirer)
        """

        if price_scenarios is None:
            price_scenarios = np.linspace(
                self.announcement_price * 0.7,
                self.announcement_price * 1.3,
                20
            ).tolist()

        scenarios = []

        for price in price_scenarios:
            if self.base_exchange_ratio < min_exchange_ratio:
                effective_ratio = min_exchange_ratio
                protection_type = "floor_active"
            elif self.base_exchange_ratio > max_exchange_ratio:
                effective_ratio = max_exchange_ratio
                protection_type = "cap_active"
            else:
                effective_ratio = self.base_exchange_ratio
                protection_type = "within_collar"

            value_per_share = effective_ratio * price
            total_deal_value = value_per_share * self.target_shares
            total_shares_issued = effective_ratio * self.target_shares

            scenarios.append({
                'acquirer_price': price,
                'effective_exchange_ratio': effective_ratio,
                'value_per_target_share': value_per_share,
                'total_deal_value': total_deal_value,
                'shares_issued': total_shares_issued,
                'protection_active': protection_type
            })

        return {
            'collar_type': 'floating',
            'base_exchange_ratio': self.base_exchange_ratio,
            'min_exchange_ratio': min_exchange_ratio,
            'max_exchange_ratio': max_exchange_ratio,
            'scenarios': scenarios,
            'ratio_range': {
                'min': min_exchange_ratio,
                'max': max_exchange_ratio,
                'base': self.base_exchange_ratio
            }
        }

    def asymmetric_collar(self, floor_price: Optional[float] = None,
                         cap_price: Optional[float] = None,
                         price_scenarios: Optional[List[float]] = None) -> Dict[str, Any]:
        """
        Asymmetric collar: Only floor or only cap protection

        One-sided protection for target (floor only) or acquirer (cap only)
        """

        if floor_price is None and cap_price is None:
            raise ValueError("Must specify either floor_price or cap_price")

        if price_scenarios is None:
            price_scenarios = np.linspace(
                self.announcement_price * 0.7,
                self.announcement_price * 1.3,
                20
            ).tolist()

        scenarios = []
        base_value = self.base_exchange_ratio * self.announcement_price

        for price in price_scenarios:
            protection_type = "no_protection"

            if floor_price and price < floor_price:
                effective_ratio = base_value / floor_price
                protection_type = "floor_active"
            elif cap_price and price > cap_price:
                effective_ratio = base_value / cap_price
                protection_type = "cap_active"
            else:
                effective_ratio = self.base_exchange_ratio

            value_per_share = effective_ratio * price
            total_deal_value = value_per_share * self.target_shares

            scenarios.append({
                'acquirer_price': price,
                'effective_exchange_ratio': effective_ratio,
                'value_per_target_share': value_per_share,
                'total_deal_value': total_deal_value,
                'protection_active': protection_type
            })

        collar_structure = "floor_only" if floor_price and not cap_price else "cap_only"

        return {
            'collar_type': 'asymmetric',
            'collar_structure': collar_structure,
            'floor_price': floor_price,
            'cap_price': cap_price,
            'base_exchange_ratio': self.base_exchange_ratio,
            'scenarios': scenarios
        }

    def walk_away_collar(self, floor_price: float,
                        cap_price: float,
                        termination_fee: float,
                        price_scenarios: Optional[List[float]] = None) -> Dict[str, Any]:
        """
        Collar with walk-away rights if price exceeds bounds

        Either party can terminate if price outside collar bounds
        """

        if price_scenarios is None:
            price_scenarios = np.linspace(
                self.announcement_price * 0.6,
                self.announcement_price * 1.4,
                20
            ).tolist()

        scenarios = []

        for price in price_scenarios:
            if price < floor_price or price > cap_price:
                deal_proceeds = False
                value_per_share = 0
                termination_cost = termination_fee
                outcome = "walk_away_triggered"
            else:
                deal_proceeds = True
                value_per_share = self.base_exchange_ratio * price
                termination_cost = 0
                outcome = "deal_proceeds"

            scenarios.append({
                'acquirer_price': price,
                'deal_proceeds': deal_proceeds,
                'value_per_target_share': value_per_share,
                'termination_fee': termination_cost,
                'outcome': outcome
            })

        return {
            'collar_type': 'walk_away',
            'floor_price': floor_price,
            'cap_price': cap_price,
            'termination_fee': termination_fee,
            'base_exchange_ratio': self.base_exchange_ratio,
            'scenarios': scenarios,
            'deal_completion_rate': sum(1 for s in scenarios if s['deal_proceeds']) / len(scenarios) * 100
        }

    def collar_valuation_impact(self, collar_structure: Dict[str, Any],
                                volatility: float,
                                time_to_close: float) -> Dict[str, Any]:
        """Estimate collar impact on deal valuation using option pricing concepts"""

        base_value = self.base_exchange_ratio * self.announcement_price * self.target_shares

        scenarios = collar_structure.get('scenarios', [])
        if not scenarios:
            return {'error': 'No scenarios in collar structure'}

        price_changes = [s.get('price_change_pct', 0) for s in scenarios if 'price_change_pct' in s]
        values = [s['total_deal_value'] for s in scenarios]

        value_std = np.std(values)
        value_mean = np.mean(values)

        protection_scenarios = [s for s in scenarios if s.get('protection_active') not in ['within_collar', 'no_protection', None]]
        protection_value = value_mean - base_value if protection_scenarios else 0

        return {
            'base_deal_value': base_value,
            'expected_value_with_collar': value_mean,
            'value_standard_deviation': value_std,
            'collar_protection_value': protection_value,
            'coefficient_of_variation': (value_std / value_mean * 100) if value_mean > 0 else 0,
            'scenarios_with_protection': len(protection_scenarios),
            'total_scenarios': len(scenarios)
        }

    def compare_collar_structures(self, price_scenarios: List[float]) -> Dict[str, Any]:
        """Compare different collar structures"""

        no_collar = {
            'type': 'no_collar',
            'scenarios': [
                {
                    'acquirer_price': price,
                    'value_per_target_share': self.base_exchange_ratio * price,
                    'total_deal_value': self.base_exchange_ratio * price * self.target_shares
                }
                for price in price_scenarios
            ]
        }

        fixed = self.fixed_collar(
            floor_price=self.announcement_price * 0.9,
            cap_price=self.announcement_price * 1.1,
            price_scenarios=price_scenarios
        )

        floating = self.floating_collar(
            min_exchange_ratio=self.base_exchange_ratio * 0.9,
            max_exchange_ratio=self.base_exchange_ratio * 1.1,
            price_scenarios=price_scenarios
        )

        floor_only = self.asymmetric_collar(
            floor_price=self.announcement_price * 0.9,
            price_scenarios=price_scenarios
        )

        structures = {
            'no_collar': no_collar,
            'fixed_collar': fixed,
            'floating_collar': floating,
            'floor_only': floor_only
        }

        comparison = []
        for name, structure in structures.items():
            scenarios = structure.get('scenarios', [])
            values = [s['total_deal_value'] for s in scenarios]

            comparison.append({
                'structure': name,
                'mean_value': np.mean(values),
                'std_dev': np.std(values),
                'min_value': min(values),
                'max_value': max(values),
                'coefficient_of_variation': (np.std(values) / np.mean(values) * 100) if np.mean(values) > 0 else 0
            })

        return {
            'comparison': comparison,
            'structures': structures
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
        if command == "collar":
            if len(sys.argv) < 6:
                raise ValueError("Announcement price, target shares, base exchange ratio, floor price, and cap price required")

            announcement_price = float(sys.argv[2])
            target_shares = float(sys.argv[3])
            base_exchange_ratio = float(sys.argv[4])
            floor_price = float(sys.argv[5])
            cap_price = float(sys.argv[6])

            collar = CollarMechanism(
                announcement_price=announcement_price,
                target_shares=target_shares,
                base_exchange_ratio=base_exchange_ratio
            )

            analysis = collar.fixed_collar(
                floor_price=floor_price,
                cap_price=cap_price
            )

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
