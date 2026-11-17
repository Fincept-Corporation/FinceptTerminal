# The multi-currency cash book: balances, conversion rates, forex fills that move two currencies,
# and conversion rates the run obtains from (synthetic) Yahoo FX series.
from datetime import datetime

import pytest

from AlgorithmImports import *  # noqa: F401,F403
from engine_fixture import Feed, run, ok
from fincept_engine.cashbook import CashBook, split_pair


class _Portfolio:
    _cash = 0.0


def test_account_entry_is_the_portfolio_cash_and_other_currencies_convert():
    pf = _Portfolio()
    book = CashBook('USD', pf)
    book['USD'].set_amount(5000)
    assert pf._cash == 5000                                   # the account entry is the portfolio's cash
    book.add('EUR', 1000, 1.1)
    assert book['EUR'].value_in_account_currency == pytest.approx(1100)
    assert book.total_value_in_account_currency == pytest.approx(6100)
    assert book.convert(100, 'EUR', 'USD') == pytest.approx(110)
    assert book.convert(110, 'USD', 'EUR') == pytest.approx(100)
    assert book.convert_to_account_currency(10, 'EUR') == pytest.approx(11)
    with pytest.raises(KeyError):
        book['JPY']
    assert book.add('USDT', 50).conversion_rate == 1.0         # stablecoins pegged to the dollar


def test_changing_the_account_currency_keeps_the_old_balance():
    pf = _Portfolio()
    book = CashBook('USD', pf)
    book['USD'].set_amount(2000)
    book.set_account_currency('EUR')
    assert book.account_currency == 'EUR'
    assert book['USD'].amount == 2000 and book['EUR'].conversion_rate == 1.0
    assert pf._cash == 0.0


def test_split_pair():
    assert split_pair('EURUSD') == ('EUR', 'USD')
    assert split_pair('BTCUSDT')[0] == 'BTC'


class BuyEuro(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2022, 1, 3)
        self.set_end_date(2022, 1, 14)
        self.set_cash(100000)
        self.pair = self.add_forex('EURUSD', Resolution.DAILY).symbol
        self.done = False
        type(self).last = self

    def on_data(self, data):
        if not self.done:
            self.done = True
            self.market_order(self.pair, 10000)


def eurusd(d):
    return 1.10 if d < datetime(2022, 1, 10) else 1.20


def test_forex_fill_moves_both_currencies_and_revalues_with_the_rate():
    d = ok(run(BuyEuro, '2022-01-03', '2022-01-14', Feed({'EURUSD=X': eurusd}), symbols=('EURUSD',)))
    a = BuyEuro.last
    book = a.portfolio.cash_book
    assert book['EUR'].amount == pytest.approx(10000)
    assert book['USD'].amount == pytest.approx(100000 - 11000)
    assert book['EUR'].conversion_rate == pytest.approx(1.20)
    assert a.portfolio.total_portfolio_value == pytest.approx(100000 + 10000 * 0.10)
    assert d['performance']['final_equity'] == pytest.approx(101000)


class BuyEuroInPounds(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2022, 1, 3)
        self.set_end_date(2022, 1, 14)
        self.set_cash(100000)
        self.pair = self.add_forex('EURGBP', Resolution.DAILY).symbol
        self.done = False
        type(self).last = self

    def on_data(self, data):
        if not self.done:
            self.done = True
            self.market_order(self.pair, 1000)


def test_cross_pair_quote_currency_is_converted_with_its_usd_rate():
    feed = Feed({'EURGBP=X': 0.88, 'GBPUSD=X': 1.25, 'EURUSD=X': 1.10})
    ok(run(BuyEuroInPounds, '2022-01-03', '2022-01-14', feed, symbols=('EURGBP',)))
    a = BuyEuroInPounds.last
    book = a.portfolio.cash_book
    assert book['EUR'].amount == pytest.approx(1000)
    assert book['GBP'].amount == pytest.approx(-880)
    assert book['GBP'].conversion_rate == pytest.approx(1.25)
    assert book['EUR'].conversion_rate == pytest.approx(1.10)
    # consistent cross rates: buying EUR with GBP leaves the account value unchanged
    assert a.portfolio.total_portfolio_value == pytest.approx(100000)
