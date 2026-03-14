# ============================================================================
# Fincept Terminal - Strategy Engine Portfolio Manager
# Tracks holdings, cash, P&L, and portfolio metrics
# ============================================================================

from typing import Dict
from .types import Symbol, SecurityHolding


class SecurityPortfolioManager:
    """Manages portfolio holdings and cash. Dict-like access by ticker."""

    def __init__(self, initial_cash: float = 100000):
        self._holdings: Dict[str, SecurityHolding] = {}
        self._cash = initial_cash
        self._initial_cash = initial_cash
        self._total_fees = 0.0
        self._total_profit = 0.0
        self._margin_used = 0.0

    @property
    def cash(self) -> float:
        return self._cash

    @cash.setter
    def cash(self, value: float):
        self._cash = value

    @property
    def invested(self) -> bool:
        return any(h.invested for h in self._holdings.values())

    @property
    def total_holdings_value(self) -> float:
        return sum(h.holdings_value for h in self._holdings.values())

    @property
    def total_portfolio_value(self) -> float:
        return self._cash + self.total_holdings_value

    @property
    def total_unrealized_profit(self) -> float:
        return sum(h.unrealized_profit for h in self._holdings.values())

    @property
    def total_profit(self) -> float:
        return self._total_profit + self.total_unrealized_profit

    @property
    def total_fees(self) -> float:
        return self._total_fees

    @property
    def margin_remaining(self) -> float:
        return self._cash

    @property
    def total_margin_used(self) -> float:
        return self._margin_used

    def __getitem__(self, key) -> SecurityHolding:
        ticker = str(key).upper()
        if ticker not in self._holdings:
            self._holdings[ticker] = SecurityHolding(Symbol(ticker))
        return self._holdings[ticker]

    def __setitem__(self, key, value):
        self._holdings[str(key).upper()] = value

    def __contains__(self, key) -> bool:
        return str(key).upper() in self._holdings

    def __iter__(self):
        return iter(self._holdings.values())

    def keys(self):
        return self._holdings.keys()

    def values(self):
        return self._holdings.values()

    def items(self):
        return self._holdings.items()

    def set_cash(self, amount: float):
        self._cash = amount
        self._initial_cash = amount

    def process_fill(self, symbol: str, quantity: float, fill_price: float, fee: float = 0.0):
        """Process an order fill and update holdings."""
        ticker = symbol.upper()
        if ticker not in self._holdings:
            self._holdings[ticker] = SecurityHolding(Symbol(ticker))

        holding = self._holdings[ticker]

        # Closing or reducing position
        if holding.quantity != 0:
            if (holding.quantity > 0 and quantity < 0) or (holding.quantity < 0 and quantity > 0):
                close_qty = min(abs(quantity), abs(holding.quantity))
                if holding.quantity > 0:
                    profit = close_qty * (fill_price - holding.average_price)
                else:
                    profit = close_qty * (holding.average_price - fill_price)
                self._total_profit += profit
                holding._total_close_profit += profit

        # Update position
        old_qty = holding.quantity
        new_qty = old_qty + quantity

        if new_qty == 0:
            holding.average_price = 0
        elif (old_qty >= 0 and quantity > 0) or (old_qty <= 0 and quantity < 0):
            # Adding to position - weighted average
            total_cost = (abs(old_qty) * holding.average_price) + (abs(quantity) * fill_price)
            holding.average_price = total_cost / abs(new_qty) if new_qty != 0 else 0
        elif abs(new_qty) > 0 and ((old_qty > 0 and new_qty < 0) or (old_qty < 0 and new_qty > 0)):
            # Flipped sides
            holding.average_price = fill_price

        holding.quantity = new_qty
        holding.market_price = fill_price

        # Update cash
        cost = quantity * fill_price + fee
        self._cash -= cost
        self._total_fees += fee

    def update_market_prices(self, prices: Dict[str, float]):
        """Update market prices for all holdings."""
        for ticker, price in prices.items():
            ticker = ticker.upper()
            if ticker in self._holdings:
                self._holdings[ticker].update_market_price(price)

    def get_holdings_summary(self) -> dict:
        """Get summary of all positions."""
        return {
            ticker: {
                'quantity': h.quantity,
                'avg_price': h.average_price,
                'market_price': h.market_price,
                'unrealized_pnl': h.unrealized_profit,
                'value': h.holdings_value
            }
            for ticker, h in self._holdings.items()
            if h.quantity != 0
        }

    def set_margin_call_model(self, *args, **kwargs):
        """Set margin call model (stub)."""
        pass

    def set_positions(self, positions):
        """Set positions (stub)."""
        pass

    # PascalCase aliases
    def SetMarginCallModel(self, *args, **kwargs):
        return self.set_margin_call_model(*args, **kwargs)

    def SetPositions(self, positions):
        return self.set_positions(positions)

    Cash = property(lambda self: self.cash)
    Invested = property(lambda self: self.invested)
    TotalHoldingsValue = property(lambda self: self.total_holdings_value)
    TotalPortfolioValue = property(lambda self: self.total_portfolio_value)
    TotalUnrealizedProfit = property(lambda self: self.total_unrealized_profit)
    TotalProfit = property(lambda self: self.total_profit)
    TotalFees = property(lambda self: self.total_fees)
    MarginRemaining = property(lambda self: self.margin_remaining)

    def __repr__(self):
        return (f"Portfolio(cash={self._cash:.2f}, "
                f"holdings_value={self.total_holdings_value:.2f}, "
                f"total={self.total_portfolio_value:.2f})")
