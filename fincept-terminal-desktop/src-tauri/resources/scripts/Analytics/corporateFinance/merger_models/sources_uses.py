"""Sources & Uses of Funds Analysis"""
from typing import Dict, Any, List, Optional
from dataclasses import dataclass, field

@dataclass
class SourcesUsesBuilder:
    """Build sources and uses of funds for M&A transaction"""

    purchase_price: float
    target_debt_refinanced: float = 0
    transaction_fees: float = 0
    financing_fees: float = 0
    other_uses: float = 0

    acquirer_cash: float = 0
    new_debt: float = 0
    new_equity: float = 0
    rollover_equity: float = 0
    other_sources: float = 0

    def calculate_uses(self) -> Dict[str, Any]:
        """Calculate total uses of funds"""

        uses = {
            'purchase_price': self.purchase_price,
            'target_debt_refinanced': self.target_debt_refinanced,
            'transaction_fees': self.transaction_fees,
            'financing_fees': self.financing_fees,
            'other_uses': self.other_uses
        }

        total_uses = sum(uses.values())

        uses_breakdown = {
            item: {
                'amount': amount,
                'percentage': (amount / total_uses * 100) if total_uses else 0
            }
            for item, amount in uses.items()
        }

        return {
            'uses_breakdown': uses_breakdown,
            'total_uses': total_uses
        }

    def calculate_sources(self) -> Dict[str, Any]:
        """Calculate total sources of funds"""

        sources = {
            'acquirer_cash': self.acquirer_cash,
            'new_debt': self.new_debt,
            'new_equity': self.new_equity,
            'rollover_equity': self.rollover_equity,
            'other_sources': self.other_sources
        }

        total_sources = sum(sources.values())

        sources_breakdown = {
            item: {
                'amount': amount,
                'percentage': (amount / total_sources * 100) if total_sources else 0
            }
            for item, amount in sources.items()
        }

        return {
            'sources_breakdown': sources_breakdown,
            'total_sources': total_sources
        }

    def build_sources_uses_table(self) -> Dict[str, Any]:
        """Build complete sources & uses table"""

        uses = self.calculate_uses()
        sources = self.calculate_sources()

        imbalance = sources['total_sources'] - uses['total_uses']
        balanced = abs(imbalance) < 0.01

        return {
            'uses': uses,
            'sources': sources,
            'balanced': balanced,
            'imbalance': imbalance
        }

    def auto_balance(self, method: str = 'cash') -> Dict[str, Any]:
        """Auto-balance sources and uses"""

        uses = self.calculate_uses()
        sources = self.calculate_sources()

        imbalance = sources['total_sources'] - uses['total_uses']

        if abs(imbalance) < 0.01:
            return self.build_sources_uses_table()

        if imbalance < 0:
            shortfall = abs(imbalance)
            if method == 'cash':
                self.acquirer_cash += shortfall
            elif method == 'debt':
                self.new_debt += shortfall
            elif method == 'equity':
                self.new_equity += shortfall
        else:
            excess = imbalance
            if method == 'cash' and self.acquirer_cash >= excess:
                self.acquirer_cash -= excess
            elif method == 'debt' and self.new_debt >= excess:
                self.new_debt -= excess

        return self.build_sources_uses_table()

    def calculate_financing_mix(self) -> Dict[str, float]:
        """Calculate debt/equity financing mix"""

        total_financing = self.new_debt + self.new_equity

        if total_financing == 0:
            return {
                'debt_percentage': 0,
                'equity_percentage': 0,
                'cash_percentage': 100,
                'debt_to_equity': 0
            }

        debt_pct = (self.new_debt / total_financing) * 100
        equity_pct = (self.new_equity / total_financing) * 100
        cash_pct = (self.acquirer_cash / (total_financing + self.acquirer_cash)) * 100

        debt_to_equity = self.new_debt / self.new_equity if self.new_equity else float('inf')

        return {
            'debt_percentage': debt_pct,
            'equity_percentage': equity_pct,
            'cash_percentage': cash_pct,
            'debt_to_equity': debt_to_equity,
            'total_financing': total_financing
        }

    def estimate_transaction_fees(self, deal_value: float, fee_structure: Optional[Dict[str, float]] = None) -> float:
        """Estimate transaction fees"""

        if fee_structure is None:
            fee_structure = {
                'financial_advisory': 0.01,
                'legal': 0.003,
                'accounting': 0.001,
                'other': 0.001
            }

        total_fees = sum(
            deal_value * rate
            for rate in fee_structure.values()
        )

        self.transaction_fees = total_fees
        return total_fees

    def estimate_financing_fees(self, debt_amount: float, fee_rate: float = 0.02) -> float:
        """Estimate debt financing fees"""

        financing_fees = debt_amount * fee_rate
        self.financing_fees = financing_fees
        return financing_fees

@dataclass
class DetailedSourcesUses(SourcesUsesBuilder):
    """Extended sources & uses with detailed breakdown"""

    advisory_fees: Dict[str, float] = field(default_factory=dict)
    legal_fees: Dict[str, float] = field(default_factory=dict)
    debt_tranches: Dict[str, float] = field(default_factory=dict)
    equity_issuances: Dict[str, float] = field(default_factory=dict)

    def add_advisory_fee(self, advisor: str, fee: float):
        """Add financial advisory fee"""
        self.advisory_fees[advisor] = fee
        self.transaction_fees = sum(self.advisory_fees.values())

    def add_debt_tranche(self, tranche_name: str, amount: float):
        """Add debt tranche"""
        self.debt_tranches[tranche_name] = amount
        self.new_debt = sum(self.debt_tranches.values())

    def add_equity_issuance(self, issuance_type: str, amount: float):
        """Add equity issuance"""
        self.equity_issuances[issuance_type] = amount
        self.new_equity = sum(self.equity_issuances.values())

    def detailed_breakdown(self) -> Dict[str, Any]:
        """Get detailed breakdown of all items"""

        base_table = self.build_sources_uses_table()

        base_table['detailed_fees'] = {
            'advisory': self.advisory_fees,
            'legal': self.legal_fees,
            'total_transaction_fees': self.transaction_fees
        }

        base_table['detailed_financing'] = {
            'debt_tranches': self.debt_tranches,
            'equity_issuances': self.equity_issuances,
            'total_debt': self.new_debt,
            'total_equity': self.new_equity
        }

        return base_table

if __name__ == '__main__':
    builder = SourcesUsesBuilder(
        purchase_price=5_000_000_000,
        target_debt_refinanced=500_000_000,
        acquirer_cash=2_000_000_000,
        new_debt=2_500_000_000,
        new_equity=1_000_000_000
    )

    builder.estimate_transaction_fees(5_000_000_000)
    builder.estimate_financing_fees(2_500_000_000)

    table = builder.auto_balance()

    print(f"Total Uses: ${table['uses']['total_uses']:,.0f}")
    print(f"Total Sources: ${table['sources']['total_sources']:,.0f}")
    print(f"Balanced: {table['balanced']}")

    financing_mix = builder.calculate_financing_mix()
    print(f"\nFinancing Mix:")
    print(f"  Debt: {financing_mix['debt_percentage']:.1f}%")
    print(f"  Equity: {financing_mix['equity_percentage']:.1f}%")
    print(f"  Cash: {financing_mix['cash_percentage']:.1f}%")
