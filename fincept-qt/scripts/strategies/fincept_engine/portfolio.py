# ============================================================================
# Fincept Terminal - Strategy Engine Portfolio Manager
# Tracks holdings, cash (one balance per currency: the cash book), P&L, and portfolio metrics
#
# Accounting (as LEAN):
#   * equities, ETFs, indices, options, CFDs: buying q at p pays q * p * multiplier of the quote
#     currency; the holding's value counts in the total portfolio value
#   * forex / crypto pairs (BASEQUOTE): the quote currency pays q * p, the base currency
#     balance receives q; the cash book carries the value (the holding is not counted twice)
#   * futures (margined contracts): no cash changes hands on a fill except fees; realised
#     profit is paid into cash when a position is reduced; open positions count with their
#     unrealised profit
# Stablecoin quotes (USDT, USDC, ...) settle in USD when the account currency is USD.
# ============================================================================

from typing import Dict
from .types import Symbol, SecurityHolding
from .enums import SecurityType, OrderStatus, OrderType
from .cashbook import CashBook, split_pair, STABLECOINS

_OPEN_STATUSES = (OrderStatus.SUBMITTED, OrderStatus.UPDATE_SUBMITTED, OrderStatus.PARTIALLY_FILLED)
_SYNC_FILL = (OrderType.MARKET, OrderType.COMBO_MARKET)

_PAIR_TYPES = (SecurityType.FOREX, SecurityType.CRYPTO)
_MARGINED_TYPES = (SecurityType.FUTURE, SecurityType.CRYPTO_FUTURE)


class _KeyValuePair:
    """What iterating a LEAN dictionary yields in Python: .key / .value (and unpacking). Attribute
    reads fall through to the value, so `for h in portfolio: h.invested` keeps working."""
    __slots__ = ('key', 'value')

    def __init__(self, key, value):
        self.key = key
        self.value = value

    def __iter__(self):
        yield self.key
        yield self.value

    def __getattr__(self, name):
        return getattr(self.value, name)

    Key = property(lambda self: self.key)
    Value = property(lambda self: self.value)

    def __repr__(self):
        return f"[{self.key}, {self.value}]"


class SecurityPositionGroupBuyingPowerModel:
    """Buying power of a single-security position group (the security's own margin rules)."""


class OptionStrategyPositionGroupBuyingPowerModel:
    """Buying power of an option strategy group (options.OptionManager.group_margin)."""


class _Position:
    __slots__ = ('symbol', 'quantity', 'unit_quantity')

    def __init__(self, symbol, quantity, unit_quantity=1.0):
        self.symbol = symbol
        self.quantity = quantity
        self.unit_quantity = unit_quantity

    Symbol = property(lambda self: self.symbol)
    Quantity = property(lambda self: self.quantity)


class _PositionGroup:
    def __init__(self, key, positions, buying_power_model, quantity):
        self.key = key
        self.positions = positions
        self.buying_power_model = buying_power_model
        self.quantity = quantity

    def __len__(self):
        return len(self.positions)

    def __iter__(self):
        return iter(self.positions)

    Positions = property(lambda self: self.positions)
    BuyingPowerModel = property(lambda self: self.buying_power_model)


class _PositionGroups:
    """portfolio.positions: holdings grouped as LEAN's position manager does for margin - all option
    positions of one underlying together with that underlying's shares form one option-strategy group
    (margined together, see options.OptionManager.group_margin); every other holding is its own group."""

    def __init__(self, portfolio):
        self._pf = portfolio

    @property
    def groups(self):
        from .enums import SecurityType
        opt_types = (SecurityType.OPTION, SecurityType.INDEX_OPTION, SecurityType.FUTURE_OPTION)
        by_u, single = {}, []
        for t, h in self._pf._holdings.items():
            if not h.quantity:
                continue
            sym = h.symbol
            if getattr(sym, 'security_type', None) in opt_types and sym.underlying is not None:
                by_u.setdefault(str(sym.underlying).upper(), []).append(h)
            else:
                single.append(h)
        groups = []
        used = set()
        for uk, hs in by_u.items():
            positions = [_Position(h.symbol, h.quantity) for h in hs]
            stock = self._pf._holdings.get(uk)
            if stock is not None and stock.quantity:
                positions.append(_Position(stock.symbol, stock.quantity, 100.0))
                used.add(uk)
            qty = min(abs(h.quantity) for h in hs)
            groups.append(_PositionGroup(uk, positions, OptionStrategyPositionGroupBuyingPowerModel(), qty))
        for h in single:
            if str(h.symbol).upper() in used:
                continue
            groups.append(_PositionGroup(str(h.symbol).upper(), [_Position(h.symbol, h.quantity)],
                                         SecurityPositionGroupBuyingPowerModel(), abs(h.quantity)))
        return groups

    Groups = groups

    def __iter__(self):
        return iter(self.groups)


class SecurityPortfolioManager:
    """Manages portfolio holdings and cash. Dict-like access by ticker."""

    def __init__(self, initial_cash: float = 100000):
        self._holdings: Dict[str, SecurityHolding] = {}
        self._active = set()        # keys of holdings with a non-zero quantity (SecurityHolding keeps it)
        self._cash = initial_cash
        self._initial_cash = initial_cash
        self._total_fees = 0.0
        self._total_profit = 0.0
        self._margin_used = 0.0
        self._securities = None   # SecurityManager (set by the algorithm)
        self._algo = None
        self.cash_book = CashBook('USD', self)
        self.unsettled_cash_book = CashBook('USD', None)
        self.margin_call_model = None
        self.positions = _PositionGroups(self)

    # ---- currency helpers ----
    def _sec(self, ticker):
        s = self._securities
        return s.get(ticker) if s is not None else None

    def quote_currency(self, ticker) -> str:
        acct = self.cash_book.account_currency
        sec = self._sec(ticker)
        q = None
        if sec is not None:
            q = getattr(getattr(sec, 'quote_currency', None), 'symbol', None)
        q = (q or acct).upper()
        if q in STABLECOINS and acct == 'USD':
            return 'USD'
        return q

    def _rate(self, code) -> float:
        code = str(code).upper()
        if code == self.cash_book.account_currency:
            return 1.0
        c = self.cash_book.get(code)
        return c.conversion_rate if c is not None else 0.0

    def _kind(self, ticker):
        sec = self._sec(ticker)
        st = getattr(getattr(sec, 'symbol', None), 'security_type', None)
        if st in _MARGINED_TYPES:
            return 'margined'
        if st in _PAIR_TYPES:
            return 'pair'
        return 'asset'

    @property
    def cash(self) -> float:
        return self._cash

    @cash.setter
    def cash(self, value: float):
        self._cash = value

    def _attach(self, ticker, h):
        h._key = ticker
        h._active = self._active
        if h.quantity:
            self._active.add(ticker)
        else:
            self._active.discard(ticker)
        return h

    def _held(self):
        """(key, holding) of every non-zero position."""
        hs = self._holdings
        return [(t, hs[t]) for t in list(self._active) if t in hs]

    @property
    def invested(self) -> bool:
        """Holdings plus open market orders, any non-zero. LEAN fills a market order the moment it is
        placed; this engine fills it at the next open, so an order placed earlier in the same step
        already counts here (a loop guarded by `if self.portfolio.invested` places one order, as in
        LEAN; a liquidation placed this step means not invested)."""
        broker = getattr(self, '_broker', None)
        book = getattr(broker, 'book', None) if broker is not None else None
        if not book:
            return bool(self._active)
        proj = {k: self._holdings[k].quantity for k in self._active if k in self._holdings}
        pending = False
        for tk in book:
            if tk.status in _OPEN_STATUSES and tk.order_type in _SYNC_FILL:
                k = str(tk.symbol).upper()
                proj[k] = proj.get(k, 0.0) + (tk.quantity - tk.quantity_filled)
                pending = True
        if not pending:
            return bool(self._active)
        return any(abs(q) > 1e-12 for q in proj.values())

    @property
    def total_holdings_value(self) -> float:
        total = 0.0
        for t, h in self._held():
            if h.quantity:
                total += h.holdings_value * self._rate(self.quote_currency(t))
        return total

    @property
    def total_portfolio_value(self) -> float:
        acct = self.cash_book.account_currency
        value = self._cash
        for code, c in self.cash_book.items():
            if code != acct and c.amount:
                value += c.amount * c.conversion_rate
        for t, h in self._held():
            if not h.quantity:
                continue
            kind = self._kind(t)
            rate = self._rate(self.quote_currency(t))
            if kind == 'asset':
                value += h.holdings_value * rate
            elif kind == 'margined':
                value += h.unrealized_profit * rate
            # pairs: the base currency balance in the cash book carries the value
        return value

    @property
    def total_unrealized_profit(self) -> float:
        return sum(h.unrealized_profit * self._rate(self.quote_currency(t)) for t, h in self._held())

    total_unrealised_profit = total_unrealized_profit

    @property
    def total_profit(self) -> float:
        return self._total_profit + self.total_unrealized_profit

    @property
    def total_net_profit(self) -> float:
        return self._total_profit - self._total_fees

    @property
    def total_fees(self) -> float:
        return self._total_fees

    @property
    def total_sale_volume(self) -> float:
        return sum(getattr(h, 'total_sale_volume', 0.0) for h in self._holdings.values())

    @property
    def total_margin_used(self) -> float:
        """Initial margin of all positions: sum(|quantity * price| / leverage)."""
        broker = getattr(self, '_broker', None)
        if broker is not None:
            return broker.margin_used()
        return sum(abs(h.holdings_value) for h in self._holdings.values())

    @property
    def margin_remaining(self) -> float:
        return self.total_portfolio_value - self.total_margin_used

    def get_buying_power(self, symbol=None, direction=None) -> float:
        return max(0.0, self.margin_remaining)

    def get_margin_remaining(self, symbol=None, direction=None) -> float:
        return self.margin_remaining

    @property
    def unsettled_cash(self) -> float:
        return self.unsettled_cash_book.total_value_in_account_currency

    @property
    def total_absolute_holdings_cost(self) -> float:
        return sum(abs(h.quantity * h.average_price * h.multiplier) for _t, h in self._held())

    def get(self, key, default=None):
        """The holding of a subscribed (or once held) security; `default` for anything else (LEAN)."""
        if key is None:
            return default
        ticker = str(key).upper()
        if ticker in self._holdings or (self._securities is not None and ticker in self._securities):
            return self[key]
        if self._securities is None:
            return self[key]
        return default

    def _new_holding(self, key):
        ticker = str(key).upper()
        sec = self._sec(ticker)
        sym = sec.symbol if sec is not None else (key if isinstance(key, Symbol) else Symbol(ticker))
        h = SecurityHolding(sym)
        if sec is not None:
            h._security = sec
        return self._attach(ticker, h)

    def __getitem__(self, key) -> SecurityHolding:
        ticker = str(key).upper()
        if ticker not in self._holdings:
            self._holdings[ticker] = self._new_holding(key)
        return self._holdings[ticker]

    def __setitem__(self, key, value):
        k = str(key).upper()
        old = self._holdings.get(k)
        if old is not None and old is not value:
            old._active = None
            self._active.discard(k)
        self._holdings[k] = value
        if isinstance(value, SecurityHolding):
            self._attach(k, value)

    def __contains__(self, key) -> bool:
        return str(key).upper() in self._holdings

    def __iter__(self):
        return iter([_KeyValuePair(h.symbol, h) for h in self._holdings.values()])

    def __len__(self):
        return len(self._holdings)

    def keys(self):
        """The holdings' Symbols (LEAN Portfolio.Keys)."""
        return [h.symbol for h in self._holdings.values()]

    def values(self):
        return self._holdings.values()

    def items(self):
        return self._holdings.items()

    def copy(self):
        return {h.symbol: h for h in self._holdings.values()}

    def clear(self):
        raise NotImplementedError("Portfolio object is an adaptor for Security Manager. This method is not "
                                  "applicable for PortfolioManager class.")

    def pop(self, *args):
        raise NotImplementedError("Portfolio object is an adaptor for Security Manager. This method is not "
                                  "applicable for PortfolioManager class.")

    def contains_key(self, key):
        return key is not None and str(key).upper() in self._holdings

    def set_cash(self, amount_or_symbol, amount=None, conversion_rate=None):
        """set_cash(amount) sets the account currency balance; set_cash("EUR", amount[, rate]) another one."""
        if isinstance(amount_or_symbol, str) and amount is not None:
            code = amount_or_symbol.upper()
            if code == self.cash_book.account_currency:
                self._cash = float(amount)
                self._initial_cash = float(amount)
            else:
                self.cash_book.add(code, float(amount), float(conversion_rate or 0.0))
            return
        self._cash = float(amount_or_symbol)
        self._initial_cash = float(amount_or_symbol)

    def process_fill(self, symbol: str, quantity: float, fill_price: float, fee: float = 0.0):
        """Process an order fill: holdings, average price, realised profit, cash (see module notes).
        `fee` is in the account currency."""
        ticker = symbol.upper()
        if ticker not in self._holdings:
            self._holdings[ticker] = self._new_holding(ticker)
        holding = self._holdings[ticker]
        mult = holding.multiplier
        kind = self._kind(ticker)
        qcode = self.quote_currency(ticker)
        acct = self.cash_book.account_currency

        realized = 0.0
        if holding.quantity != 0:
            if (holding.quantity > 0 and quantity < 0) or (holding.quantity < 0 and quantity > 0):
                close_qty = min(abs(quantity), abs(holding.quantity))
                if holding.quantity > 0:
                    realized = close_qty * (fill_price - holding.average_price) * mult
                else:
                    realized = close_qty * (holding.average_price - fill_price) * mult
                rate = 1.0 if qcode == acct else self._rate(qcode)
                self._total_profit += realized * rate
                holding._total_close_profit += realized * rate

        old_qty = holding.quantity
        new_qty = old_qty + quantity
        if new_qty == 0:
            holding.average_price = 0
        elif (old_qty >= 0 and quantity > 0) or (old_qty <= 0 and quantity < 0):
            total_cost = (abs(old_qty) * holding.average_price) + (abs(quantity) * fill_price)
            holding.average_price = total_cost / abs(new_qty) if new_qty != 0 else 0
        elif abs(new_qty) > 0 and ((old_qty > 0 and new_qty < 0) or (old_qty < 0 and new_qty > 0)):
            holding.average_price = fill_price

        holding.quantity = new_qty
        holding.market_price = fill_price
        holding.total_sale_volume = getattr(holding, 'total_sale_volume', 0.0) + abs(quantity * fill_price * mult)
        holding.total_fees = getattr(holding, 'total_fees', 0.0) + fee

        # cash
        if kind == 'margined':
            settled = realized
        else:
            settled = -quantity * fill_price * mult
        if qcode == acct:
            self._cash += settled
        else:
            self.cash_book.ensure(qcode).add_amount(settled)
        if kind == 'pair':
            base, _q = split_pair(ticker)
            if base:
                bc = self.cash_book.ensure(base)
                bc.add_amount(quantity * mult)
                if not bc.conversion_rate or not self._rate(qcode):
                    self._pair_rates(ticker, fill_price)
        self._cash -= fee
        self._total_fees += fee

    def update_market_prices(self, prices: Dict[str, float]):
        """Update market prices for all holdings (and the conversion rate of pair base currencies)."""
        acct = self.cash_book.account_currency
        for ticker, price in prices.items():
            ticker = ticker.upper()
            if ticker in self._holdings:
                self._holdings[ticker].update_market_price(price)
            if self._kind(ticker) == 'pair' and price:
                self._pair_rates(ticker, price)

    def _pair_rates(self, ticker, price):
        """A pair's price (QUOTE per BASE) gives a conversion rate when one side is the account currency."""
        acct = self.cash_book.account_currency
        base, _q = split_pair(ticker)
        qcode = self.quote_currency(ticker)
        if not base:
            return
        if base == acct and qcode in self.cash_book:
            self.cash_book[qcode].conversion_rate = 1.0 / price
        elif base != acct and base in self.cash_book:
            qr = 1.0 if qcode == acct else self._rate(qcode)
            if qr:
                self.cash_book[base].conversion_rate = price * qr

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

    def set_margin_call_model(self, model=None, *args, **kwargs):
        self.margin_call_model = model

    def set_positions(self, positions):
        self._custom_positions = positions

    def set_account_currency(self, code: str, starting_cash=None):
        self.cash_book.set_account_currency(code)
        if starting_cash is not None:
            self._cash = float(starting_cash)
            self._initial_cash = float(starting_cash)

    # PascalCase aliases
    def SetMarginCallModel(self, *args, **kwargs):
        return self.set_margin_call_model(*args, **kwargs)

    def SetPositions(self, positions):
        return self.set_positions(positions)

    Cash = property(lambda self: self.cash)
    CashBook = property(lambda self: self.cash_book)
    # C# dictionary members (Portfolio.Values / Keys / Count in LEAN's Python examples)
    Values = property(lambda self: list(self._holdings.values()))
    Keys = property(lambda self: [h.symbol for h in self._holdings.values()])
    Count = property(lambda self: len(self._holdings))
    Invested = property(lambda self: self.invested)
    TotalHoldingsValue = property(lambda self: self.total_holdings_value)
    TotalPortfolioValue = property(lambda self: self.total_portfolio_value)
    TotalUnrealizedProfit = property(lambda self: self.total_unrealized_profit)
    TotalProfit = property(lambda self: self.total_profit)
    TotalFees = property(lambda self: self.total_fees)
    MarginRemaining = property(lambda self: self.margin_remaining)
    TotalMarginUsed = property(lambda self: self.total_margin_used)

    def __repr__(self):
        return (f"Portfolio(cash={self._cash:.2f}, "
                f"holdings_value={self.total_holdings_value:.2f}, "
                f"total={self.total_portfolio_value:.2f})")
