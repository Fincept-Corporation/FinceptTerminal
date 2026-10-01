"""Sources & Uses of Funds Analysis"""
from typing import Dict, Any, List, Optional
from dataclasses import dataclass, field
import sys

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

# ---- service ABI shim (MAAnalyticsService) BEGIN ----
# The Qt MAAnalyticsService calls `sources_uses.py calculate <flat-params-json>` (argv length 3). The native form is
# `sources_uses deal_structure financing_structure`, so a service-style call is translated here and then falls
# through to the native dispatch. Any other argv shape is untouched.
_SERVICE_COMMANDS = ("calculate",)


def _svc_num(p, *keys, default=None):
    for k in keys:
        v = p.get(k)
        if isinstance(v, (int, float)) and not isinstance(v, bool):
            return float(v)
    return default


def _service_argv(argv):
    import json
    if len(argv) != 3 or argv[1] not in _SERVICE_COMMANDS:
        return argv
    try:
        p = json.loads(argv[2])
    except ValueError:
        return argv
    if not isinstance(p, dict):
        return argv
    deal = {
        "purchase_price": _svc_num(p, "deal_value", "purchase_price", "equity_offered", default=0.0),
        "target_debt_refinanced": _svc_num(p, "target_debt", "target_debt_refinanced", "target_debt_assumed",
                                           default=0.0),
    }
    fees = _svc_num(p, "fees", "transaction_fees", default=None)
    if fees is not None:
        deal["transaction_fees"] = fees
    financing = {
        "acquirer_cash": _svc_num(p, "cash", "cash_on_hand", "acquirer_cash", "cash_offered", default=0.0),
        "new_debt": _svc_num(p, "new_debt", "debt_raised", default=0.0),
        "new_equity": _svc_num(p, "stock_issuance", "new_equity", default=0.0),
    }
    if "equity_offered" in p and "deal_value" not in p and "purchase_price" not in p:
        # MCP shape: price = cash + equity consideration.
        deal["purchase_price"] = (_svc_num(p, "equity_offered", default=0.0) + _svc_num(p, "cash_offered", default=0.0))
        financing["new_equity"] = _svc_num(p, "equity_offered", default=0.0)
    return [argv[0], "sources_uses", json.dumps(deal), json.dumps(financing)]
# ---- service ABI shim (MAAnalyticsService) END ----


def main():
    """CLI entry point - outputs JSON for C++ integration"""
    import json

    sys.argv = _service_argv(sys.argv)
    if len(sys.argv) < 2:
        result = {"success": False, "error": "No command specified. Usage: sources_uses.py <command> [args...]"}
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "sources_uses":
            if len(sys.argv) < 4:
                raise ValueError("Deal structure and financing structure required")
            deal_structure = json.loads(sys.argv[2])
            financing_structure = json.loads(sys.argv[3])

            # Handle frontend field name aliases
            builder = SourcesUsesBuilder(
                purchase_price=deal_structure.get('purchase_price', 0),
                target_debt_refinanced=deal_structure.get('target_debt_refinanced', deal_structure.get('target_debt', 0)),
                acquirer_cash=financing_structure.get('acquirer_cash', financing_structure.get('cash_on_hand', 0)),
                new_debt=financing_structure.get('new_debt', 0),
                new_equity=financing_structure.get('new_equity', financing_structure.get('stock_issuance', 0))
            )

            if 'transaction_fees' in deal_structure:
                builder.transaction_fees = deal_structure['transaction_fees']
            else:
                builder.estimate_transaction_fees(deal_structure.get('purchase_price', 0))

            if 'financing_fees' in deal_structure:
                builder.financing_fees = deal_structure['financing_fees']
            elif 'financing_fees' in financing_structure:
                builder.financing_fees = financing_structure['financing_fees']
            else:
                builder.estimate_financing_fees(financing_structure.get('new_debt', 0))

            table = builder.auto_balance()
            financing_mix = builder.calculate_financing_mix()

            # Flatten output for frontend: {sources: {key: amount}, uses: {key: amount}, total_sources, total_uses, ...}
            sources_flat = {
                k: v['amount']
                for k, v in table.get('sources', {}).get('sources_breakdown', {}).items()
                if v.get('amount', 0) > 0
            }
            uses_flat = {
                k: v['amount']
                for k, v in table.get('uses', {}).get('uses_breakdown', {}).items()
                if v.get('amount', 0) > 0
            }

            result = {"success": True, "data": {
                "sources": sources_flat,
                "uses": uses_flat,
                "total_sources": table.get('sources', {}).get('total_sources', 0),
                "total_uses": table.get('uses', {}).get('total_uses', 0),
                "balanced": table.get('balanced', False),
                "imbalance": table.get('imbalance', 0),
                "financing_mix": financing_mix,
                "table": table,  # Keep raw table for detailed view
            }}
            print(json.dumps(result))

        else:
            result = {"success": False, "error": f"Unknown command: {command}. Available: sources_uses"}
            print(json.dumps(result))
            sys.exit(1)

    except Exception as e:
        result = {"success": False, "error": str(e), "command": command}
        print(json.dumps(result))
        sys.exit(1)

if __name__ == '__main__':
    main()
