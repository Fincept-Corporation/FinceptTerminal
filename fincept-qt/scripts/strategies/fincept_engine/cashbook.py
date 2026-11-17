# ============================================================================
# Fincept Terminal - Strategy Engine: multi-currency cash book (LEAN CashBook)
#
# The portfolio's account-currency balance is portfolio.cash; every other currency the
# strategy holds (set_cash("EUR", ...), the base currency bought through a forex or crypto
# pair, the quote currency paid) has its own Cash entry. Each entry has a conversion rate
# to the account currency, refreshed every trading day from real FX / crypto bars (Yahoo
# "EURUSD=X", "BTC-USD", ...) that the session loads for it; the total portfolio value adds
# every balance at its conversion rate.
# ============================================================================

from typing import Dict, Optional

CURRENCY_SYMBOLS = {'USD': '$', 'EUR': '€', 'GBP': '£', 'JPY': '¥', 'INR': '₹', 'CNY': '¥', 'CHF': 'CHF',
                    'AUD': '$', 'CAD': '$', 'HKD': '$', 'NZD': '$', 'BTC': '₿', 'ETH': 'Ξ'}
# stablecoins are treated as their peg (1:1 to USD) unless the strategy sets a rate
STABLECOINS = {'USDT', 'USDC', 'BUSD', 'TUSD', 'DAI', 'USDP', 'FDUSD'}
FIAT = {'USD', 'EUR', 'GBP', 'JPY', 'CHF', 'AUD', 'CAD', 'NZD', 'HKD', 'SGD', 'SEK', 'NOK', 'DKK', 'INR', 'CNY',
        'CNH', 'MXN', 'ZAR', 'TRY', 'BRL', 'KRW', 'PLN', 'CZK', 'HUF', 'ILS', 'THB', 'RUB'}


class Cash:
    """One currency balance with its conversion rate to the account currency."""

    def __init__(self, symbol: str, amount: float = 0.0, conversion_rate: float = 0.0, book=None):
        self.symbol = str(symbol).upper()
        self._amount = float(amount)
        self.conversion_rate = float(conversion_rate)
        self._book = book
        self.currency_conversion = None
        self.rate_source = None          # how the rate is obtained ('account', 'peg', 'EURUSD=X', ...)

    @property
    def amount(self) -> float:
        b = self._book
        if b is not None and self.symbol == b.account_currency and b._portfolio is not None:
            return b._portfolio._cash
        return self._amount

    @amount.setter
    def amount(self, value):
        self.set_amount(value)

    def set_amount(self, value):
        b = self._book
        if b is not None and self.symbol == b.account_currency and b._portfolio is not None:
            b._portfolio._cash = float(value)
        else:
            self._amount = float(value)
        return self.amount

    def add_amount(self, value):
        return self.set_amount(self.amount + float(value))

    @property
    def value_in_account_currency(self) -> float:
        return self.amount * self.conversion_rate

    @property
    def currency_symbol(self) -> str:
        return CURRENCY_SYMBOLS.get(self.symbol, self.symbol)

    def __str__(self):
        return f"{self.symbol}: {self.currency_symbol}{self.amount:,.2f} @ {self.conversion_rate:g} = " \
               f"${self.value_in_account_currency:,.2f}"

    __repr__ = __str__
    Amount = property(lambda self: self.amount)
    ConversionRate = property(lambda self: self.conversion_rate)
    Symbol = property(lambda self: self.symbol)
    ValueInAccountCurrency = property(lambda self: self.value_in_account_currency)
    AddAmount = add_amount
    SetAmount = set_amount


class CashBook(dict):
    """currency code -> Cash. The account currency's entry is the portfolio's cash."""

    def __init__(self, account_currency: str = 'USD', portfolio=None):
        super().__init__()
        self.account_currency = account_currency.upper()
        self._portfolio = portfolio
        super().__setitem__(self.account_currency, Cash(self.account_currency, 0.0, 1.0, self))

    def set_account_currency(self, code: str):
        code = str(code).upper()
        if code == self.account_currency:
            return
        old = self.account_currency
        bal = self[old].amount
        old_cash = Cash(old, bal, 0.0, self)
        self.account_currency = code
        existing = dict.get(self, code)
        if existing is not None and self._portfolio is not None:
            self._portfolio._cash = existing._amount
        elif self._portfolio is not None:
            self._portfolio._cash = 0.0
        super().__setitem__(code, Cash(code, 0.0, 1.0, self))
        if bal:
            super().__setitem__(old, old_cash)
        elif old in self:
            dict.pop(self, old)

    def __getitem__(self, code) -> Cash:
        code = str(code).upper()
        if code not in self:
            raise KeyError(f"This cash symbol ({code}) was not found in your cash book.")
        return dict.__getitem__(self, code)

    def __contains__(self, code):
        return dict.__contains__(self, str(code).upper())

    def get(self, code, default=None):
        return dict.get(self, str(code).upper(), default)

    def contains_key(self, code) -> bool:
        return str(code).upper() in self

    def try_get_value(self, code):
        c = self.get(code)
        return (c is not None, c)

    def add(self, symbol: str, quantity: float = 0.0, conversion_rate: float = 0.0) -> Cash:
        code = str(symbol).upper()
        existing = self.get(code)
        if existing is not None:
            existing.set_amount(quantity)
            if conversion_rate:
                existing.conversion_rate = float(conversion_rate)
            return existing
        rate = 1.0 if code == self.account_currency else float(conversion_rate or 0.0)
        if not rate and code in STABLECOINS and self.account_currency == 'USD':
            rate = 1.0
        c = Cash(code, quantity, rate, self)
        dict.__setitem__(self, code, c)
        return c

    def ensure(self, code: str) -> Cash:
        c = self.get(code)
        return c if c is not None else self.add(code, 0.0, 0.0)

    @property
    def total_value_in_account_currency(self) -> float:
        return sum(c.value_in_account_currency for c in self.values())

    def convert(self, amount: float, source: str, destination: str) -> float:
        s, d = self[source], self[destination]
        if not d.conversion_rate:
            raise ValueError(f"No conversion rate for {destination}")
        return amount * s.conversion_rate / d.conversion_rate

    def convert_to_account_currency(self, amount: float, currency: str) -> float:
        if str(currency).upper() == self.account_currency:
            return amount
        return amount * self[currency].conversion_rate

    def keys_list(self):
        return list(self.keys())

    def __str__(self):
        return '\n'.join(str(c) for c in self.values())

    ContainsKey = contains_key
    TryGetValue = try_get_value
    Add = add
    Convert = convert
    ConvertToAccountCurrency = convert_to_account_currency
    TotalValueInAccountCurrency = property(lambda self: self.total_value_in_account_currency)
    AccountCurrency = property(lambda self: self.account_currency)


def split_pair(ticker: str, security_type=None):
    """(base, quote) currency codes of a forex / crypto pair ticker such as EURUSD, BTCUSDT."""
    from .data import _CRYPTO_QUOTES
    t = str(ticker).upper().replace('/', '').replace('-', '')
    if len(t) == 6 and t[:3] in FIAT and t[3:] in FIAT:
        return t[:3], t[3:]
    for q in _CRYPTO_QUOTES:
        if t.endswith(q) and len(t) > len(q):
            return t[:-len(q)], q
    if len(t) == 6:
        return t[:3], t[3:]
    return None, None
