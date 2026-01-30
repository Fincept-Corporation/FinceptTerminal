"""Capital Structure Builder for LBO"""
from typing import Dict, Any, List, Optional
from dataclasses import dataclass

@dataclass
class DebtTranche:
    """Debt tranche structure"""
    name: str
    amount: float
    interest_rate: float
    amortization_years: float
    type: str  # 'Revolver', 'Term Loan A', 'Term Loan B', 'Senior Notes', 'Subordinated', 'Mezzanine'
    mandatory_amortization: float = 0
    sweep_percentage: float = 0

class CapitalStructureBuilder:
    """Build and analyze LBO capital structure"""

    def __init__(self, entry_enterprise_value: float, entry_ebitda: float):
        self.entry_ev = entry_enterprise_value
        self.entry_ebitda = entry_ebitda
        self.debt_tranches: List[DebtTranche] = []
        self.sponsor_equity = 0
        self.rollover_equity = 0
        self.management_equity = 0

    def add_debt_tranche(self, name: str, amount: float, interest_rate: float,
                        amortization_years: float, debt_type: str,
                        mandatory_amortization: float = 0,
                        sweep_percentage: float = 0) -> DebtTranche:
        """Add debt tranche to capital structure"""

        tranche = DebtTranche(
            name=name,
            amount=amount,
            interest_rate=interest_rate,
            amortization_years=amortization_years,
            type=debt_type,
            mandatory_amortization=mandatory_amortization,
            sweep_percentage=sweep_percentage
        )

        self.debt_tranches.append(tranche)
        return tranche

    def set_equity(self, sponsor_equity: float, rollover_equity: float = 0,
                  management_equity: float = 0):
        """Set equity components"""
        self.sponsor_equity = sponsor_equity
        self.rollover_equity = rollover_equity
        self.management_equity = management_equity

    def calculate_leverage_metrics(self) -> Dict[str, float]:
        """Calculate leverage and coverage metrics"""

        total_debt = sum(t.amount for t in self.debt_tranches)
        total_equity = self.sponsor_equity + self.rollover_equity + self.management_equity

        senior_debt = sum(t.amount for t in self.debt_tranches
                         if t.type in ['Revolver', 'Term Loan A', 'Term Loan B', 'Senior Notes'])
        subordinated_debt = total_debt - senior_debt

        return {
            'total_debt': total_debt,
            'total_equity': total_equity,
            'total_capital': total_debt + total_equity,
            'debt_to_ebitda': total_debt / self.entry_ebitda if self.entry_ebitda else 0,
            'senior_debt_to_ebitda': senior_debt / self.entry_ebitda if self.entry_ebitda else 0,
            'total_debt_to_capital': (total_debt / (total_debt + total_equity) * 100) if (total_debt + total_equity) else 0,
            'equity_to_capital': (total_equity / (total_debt + total_equity) * 100) if (total_debt + total_equity) else 0,
            'sponsor_equity_contribution': self.sponsor_equity,
            'rollover_equity': self.rollover_equity,
            'management_equity': self.management_equity
        }

    def build_capital_structure_table(self) -> Dict[str, Any]:
        """Build detailed capital structure table"""

        leverage_metrics = self.calculate_leverage_metrics()

        debt_details = []
        for tranche in self.debt_tranches:
            debt_details.append({
                'name': tranche.name,
                'type': tranche.type,
                'amount': tranche.amount,
                'percentage_of_total': (tranche.amount / leverage_metrics['total_capital'] * 100),
                'interest_rate': tranche.interest_rate * 100,
                'amortization_years': tranche.amortization_years
            })

        equity_details = [
            {
                'name': 'Sponsor Equity',
                'amount': self.sponsor_equity,
                'percentage_of_total': (self.sponsor_equity / leverage_metrics['total_capital'] * 100)
            },
            {
                'name': 'Rollover Equity',
                'amount': self.rollover_equity,
                'percentage_of_total': (self.rollover_equity / leverage_metrics['total_capital'] * 100)
            },
            {
                'name': 'Management Equity',
                'amount': self.management_equity,
                'percentage_of_total': (self.management_equity / leverage_metrics['total_capital'] * 100)
            }
        ]

        return {
            'entry_enterprise_value': self.entry_ev,
            'entry_ebitda': self.entry_ebitda,
            'entry_multiple': self.entry_ev / self.entry_ebitda if self.entry_ebitda else 0,
            'debt_tranches': debt_details,
            'equity_components': equity_details,
            'leverage_metrics': leverage_metrics,
            'total_sources': leverage_metrics['total_capital']
        }

    def calculate_annual_interest_expense(self, year: int = 1) -> float:
        """Calculate annual interest expense"""

        total_interest = 0
        for tranche in self.debt_tranches:
            outstanding_balance = tranche.amount

            if year > 1 and tranche.mandatory_amortization > 0:
                years_elapsed = min(year - 1, tranche.amortization_years)
                amortized = tranche.amount * tranche.mandatory_amortization * years_elapsed
                outstanding_balance = max(0, tranche.amount - amortized)

            total_interest += outstanding_balance * tranche.interest_rate

        return total_interest

    def optimize_capital_structure(self, target_leverage: float,
                                  max_leverage: float = 7.0) -> Dict[str, Any]:
        """Optimize capital structure for target leverage"""

        target_debt = target_leverage * self.entry_ebitda
        target_debt = min(target_debt, max_leverage * self.entry_ebitda)

        equity_needed = self.entry_ev - target_debt

        typical_structure = {
            'Revolver': 0.05,
            'Term Loan A': 0.20,
            'Term Loan B': 0.50,
            'Senior Notes': 0.15,
            'Subordinated': 0.10
        }

        self.debt_tranches = []

        for debt_type, allocation in typical_structure.items():
            amount = target_debt * allocation

            if debt_type == 'Revolver':
                interest_rate = 0.04
                amort = 5
            elif debt_type == 'Term Loan A':
                interest_rate = 0.045
                amort = 5
            elif debt_type == 'Term Loan B':
                interest_rate = 0.055
                amort = 7
            elif debt_type == 'Senior Notes':
                interest_rate = 0.060
                amort = 10
            else:
                interest_rate = 0.085
                amort = 10

            self.add_debt_tranche(
                name=debt_type,
                amount=amount,
                interest_rate=interest_rate,
                amortization_years=amort,
                debt_type=debt_type
            )

        self.set_equity(sponsor_equity=equity_needed * 0.85,
                       rollover_equity=equity_needed * 0.10,
                       management_equity=equity_needed * 0.05)

        return self.build_capital_structure_table()

if __name__ == '__main__':
    builder = CapitalStructureBuilder(
        entry_enterprise_value=5_000_000_000,
        entry_ebitda=625_000_000
    )

    optimized = builder.optimize_capital_structure(target_leverage=5.5)

    print(f"Entry Multiple: {optimized['entry_multiple']:.2f}x")
    print(f"Total Debt/EBITDA: {optimized['leverage_metrics']['debt_to_ebitda']:.2f}x")
    print(f"Debt %: {optimized['leverage_metrics']['total_debt_to_capital']:.1f}%")
    print(f"Equity %: {optimized['leverage_metrics']['equity_to_capital']:.1f}%")
