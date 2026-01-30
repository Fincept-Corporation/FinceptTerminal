"""Payment Structure Analysis - Cash vs Stock"""
from typing import Dict, Any, Optional, List
from dataclasses import dataclass
from enum import Enum

class PaymentType(Enum):
    ALL_CASH = "all_cash"
    ALL_STOCK = "all_stock"
    MIXED = "mixed"
    CASH_AND_STOCK = "cash_and_stock"

@dataclass
class DealTerms:
    enterprise_value: float
    cash_portion: float = 0
    stock_portion: float = 0
    acquirer_share_price: float = 0
    target_shares_outstanding: float = 0

class PaymentStructureAnalyzer:
    """Analyzes payment structure implications for M&A deals"""

    def __init__(self, acquirer_shares_outstanding: float,
                 acquirer_share_price: float,
                 target_shares_outstanding: float,
                 target_share_price: float):
        self.acquirer_shares = acquirer_shares_outstanding
        self.acquirer_price = acquirer_share_price
        self.target_shares = target_shares_outstanding
        self.target_price = target_share_price

    def analyze_all_cash(self, purchase_price: float,
                        acquirer_cash: float,
                        debt_capacity: float) -> Dict[str, Any]:
        """Analyze all-cash transaction"""

        cash_needed = purchase_price
        use_debt = max(0, cash_needed - acquirer_cash)

        post_deal_shares = self.acquirer_shares
        dilution_pct = 0

        return {
            'payment_type': PaymentType.ALL_CASH.value,
            'purchase_price': purchase_price,
            'cash_required': cash_needed,
            'acquirer_cash_used': min(acquirer_cash, cash_needed),
            'debt_required': use_debt,
            'debt_capacity_available': debt_capacity,
            'debt_feasible': use_debt <= debt_capacity,
            'post_deal_shares_outstanding': post_deal_shares,
            'shareholder_dilution': dilution_pct,
            'target_shareholders_receive': {
                'cash': cash_needed,
                'stock': 0,
                'ownership_pct': 0
            }
        }

    def analyze_all_stock(self, purchase_price: float) -> Dict[str, Any]:
        """Analyze all-stock transaction"""

        new_shares_issued = purchase_price / self.acquirer_price
        post_deal_shares = self.acquirer_shares + new_shares_issued

        acquirer_ownership = (self.acquirer_shares / post_deal_shares) * 100
        target_ownership = (new_shares_issued / post_deal_shares) * 100
        dilution_pct = (new_shares_issued / self.acquirer_shares) * 100

        exchange_ratio = new_shares_issued / self.target_shares

        return {
            'payment_type': PaymentType.ALL_STOCK.value,
            'purchase_price': purchase_price,
            'new_shares_issued': new_shares_issued,
            'exchange_ratio': exchange_ratio,
            'post_deal_shares_outstanding': post_deal_shares,
            'shareholder_dilution': dilution_pct,
            'ownership_split': {
                'acquirer_shareholders': acquirer_ownership,
                'target_shareholders': target_ownership
            },
            'target_shareholders_receive': {
                'cash': 0,
                'shares': new_shares_issued,
                'ownership_pct': target_ownership
            }
        }

    def analyze_mixed_payment(self, purchase_price: float,
                             cash_percentage: float,
                             acquirer_cash: float,
                             debt_capacity: float) -> Dict[str, Any]:
        """Analyze mixed cash/stock transaction"""

        if not 0 <= cash_percentage <= 100:
            raise ValueError("Cash percentage must be between 0 and 100")

        cash_portion = purchase_price * (cash_percentage / 100)
        stock_portion = purchase_price * ((100 - cash_percentage) / 100)

        new_shares_issued = stock_portion / self.acquirer_price
        post_deal_shares = self.acquirer_shares + new_shares_issued

        acquirer_ownership = (self.acquirer_shares / post_deal_shares) * 100
        target_ownership = (new_shares_issued / post_deal_shares) * 100
        dilution_pct = (new_shares_issued / self.acquirer_shares) * 100

        use_debt = max(0, cash_portion - acquirer_cash)

        exchange_ratio = new_shares_issued / self.target_shares

        return {
            'payment_type': PaymentType.MIXED.value,
            'purchase_price': purchase_price,
            'cash_portion': cash_portion,
            'stock_portion': stock_portion,
            'cash_percentage': cash_percentage,
            'stock_percentage': 100 - cash_percentage,
            'cash_required': cash_portion,
            'acquirer_cash_used': min(acquirer_cash, cash_portion),
            'debt_required': use_debt,
            'debt_capacity_available': debt_capacity,
            'debt_feasible': use_debt <= debt_capacity,
            'new_shares_issued': new_shares_issued,
            'exchange_ratio': exchange_ratio,
            'post_deal_shares_outstanding': post_deal_shares,
            'shareholder_dilution': dilution_pct,
            'ownership_split': {
                'acquirer_shareholders': acquirer_ownership,
                'target_shareholders': target_ownership
            },
            'target_shareholders_receive': {
                'cash': cash_portion,
                'shares': new_shares_issued,
                'ownership_pct': target_ownership
            }
        }

    def compare_payment_structures(self, purchase_price: float,
                                   acquirer_cash: float,
                                   debt_capacity: float) -> Dict[str, Any]:
        """Compare all payment structure options"""

        all_cash = self.analyze_all_cash(purchase_price, acquirer_cash, debt_capacity)
        all_stock = self.analyze_all_stock(purchase_price)
        mixed_50_50 = self.analyze_mixed_payment(purchase_price, 50, acquirer_cash, debt_capacity)
        mixed_75_25 = self.analyze_mixed_payment(purchase_price, 75, acquirer_cash, debt_capacity)
        mixed_25_75 = self.analyze_mixed_payment(purchase_price, 25, acquirer_cash, debt_capacity)

        return {
            'purchase_price': purchase_price,
            'scenarios': {
                'all_cash': all_cash,
                'all_stock': all_stock,
                '50_cash_50_stock': mixed_50_50,
                '75_cash_25_stock': mixed_75_25,
                '25_cash_75_stock': mixed_25_75
            },
            'summary': {
                'min_dilution': all_cash['shareholder_dilution'],
                'max_dilution': all_stock['shareholder_dilution'],
                'min_cash_required': 0,
                'max_cash_required': purchase_price
            }
        }

    def optimal_payment_mix(self, purchase_price: float,
                           acquirer_cash: float,
                           debt_capacity: float,
                           max_acceptable_dilution: float,
                           max_leverage_ratio: Optional[float] = None) -> Dict[str, Any]:
        """Find optimal payment mix given constraints"""

        results = []

        for cash_pct in range(0, 101, 5):
            analysis = self.analyze_mixed_payment(purchase_price, cash_pct,
                                                  acquirer_cash, debt_capacity)

            if analysis['shareholder_dilution'] > max_acceptable_dilution:
                continue

            if not analysis['debt_feasible']:
                continue

            results.append({
                'cash_percentage': cash_pct,
                'stock_percentage': 100 - cash_pct,
                'dilution': analysis['shareholder_dilution'],
                'debt_required': analysis['debt_required'],
                'target_ownership': analysis['ownership_split']['target_shareholders']
            })

        if not results:
            return {
                'feasible': False,
                'message': 'No feasible payment structure within constraints'
            }

        min_dilution = min(results, key=lambda x: x['dilution'])
        min_debt = min(results, key=lambda x: x['debt_required'])

        return {
            'feasible': True,
            'optimal_scenarios': {
                'minimize_dilution': min_dilution,
                'minimize_debt': min_debt
            },
            'all_feasible_options': results,
            'constraints': {
                'max_dilution': max_acceptable_dilution,
                'debt_capacity': debt_capacity,
                'available_cash': acquirer_cash
            }
        }

    def tax_implications(self, payment_structure: Dict[str, Any],
                        target_shareholder_basis: float,
                        capital_gains_rate: float = 0.20) -> Dict[str, Any]:
        """Analyze tax implications for target shareholders"""

        cash_received = payment_structure['target_shareholders_receive']['cash']
        stock_received_value = payment_structure['target_shareholders_receive'].get('shares', 0) * self.acquirer_price

        total_consideration = cash_received + stock_received_value
        gain = total_consideration - target_shareholder_basis

        taxable_gain = min(cash_received, gain)

        current_tax = taxable_gain * capital_gains_rate

        deferred_gain = max(0, gain - taxable_gain)

        return {
            'total_consideration': total_consideration,
            'cost_basis': target_shareholder_basis,
            'total_gain': gain,
            'cash_received': cash_received,
            'stock_received_value': stock_received_value,
            'taxable_gain_current': taxable_gain,
            'deferred_gain': deferred_gain,
            'current_tax_liability': current_tax,
            'after_tax_proceeds': cash_received - current_tax,
            'effective_tax_rate': (current_tax / total_consideration * 100) if total_consideration > 0 else 0
        }

if __name__ == '__main__':
    analyzer = PaymentStructureAnalyzer(
        acquirer_shares_outstanding=100_000_000,
        acquirer_share_price=50.00,
        target_shares_outstanding=20_000_000,
        target_share_price=30.00
    )

    purchase_price = 800_000_000

    comparison = analyzer.compare_payment_structures(
        purchase_price=purchase_price,
        acquirer_cash=200_000_000,
        debt_capacity=400_000_000
    )

    print("=== PAYMENT STRUCTURE COMPARISON ===\n")
    print(f"Purchase Price: ${purchase_price:,.0f}\n")

    for scenario_name, scenario_data in comparison['scenarios'].items():
        print(f"\n{scenario_name.replace('_', ' ').title()}:")
        print(f"  Dilution: {scenario_data['shareholder_dilution']:.2f}%")
        if 'cash_required' in scenario_data:
            print(f"  Cash Required: ${scenario_data.get('cash_required', 0):,.0f}")
        if 'debt_required' in scenario_data:
            print(f"  Debt Required: ${scenario_data.get('debt_required', 0):,.0f}")
        print(f"  Target Ownership: {scenario_data['target_shareholders_receive']['ownership_pct']:.2f}%")

    optimal = analyzer.optimal_payment_mix(
        purchase_price=purchase_price,
        acquirer_cash=200_000_000,
        debt_capacity=400_000_000,
        max_acceptable_dilution=15.0
    )

    print("\n\n=== OPTIMAL PAYMENT MIX ===")
    if optimal['feasible']:
        print(f"\nMinimize Dilution: {optimal['optimal_scenarios']['minimize_dilution']['cash_percentage']}% cash")
        print(f"  Dilution: {optimal['optimal_scenarios']['minimize_dilution']['dilution']:.2f}%")
        print(f"\nMinimize Debt: {optimal['optimal_scenarios']['minimize_debt']['cash_percentage']}% cash")
        print(f"  Debt Required: ${optimal['optimal_scenarios']['minimize_debt']['debt_required']:,.0f}")
