# Exercise, assignment and expiry of model-priced option contracts in an (offline) backtest:
# physical delivery for equity options, cash settlement for index options (LEAN rules).
from datetime import datetime

from AlgorithmImports import *  # noqa: F401,F403
from engine_fixture import Feed, run, ok

EXPIRY = datetime(2022, 1, 21)      # third Friday of January 2022


class _OptionAlgo(QCAlgorithm):
    contracts = ()          # (right, strike, quantity)
    index = False

    def initialize(self):
        self.set_start_date(2022, 1, 3)
        self.set_end_date(2022, 1, 28)
        self.set_cash(100000)
        if self.index:
            self.und = self.add_index('SPX', Resolution.DAILY).symbol
            style = OptionStyle.EUROPEAN
        else:
            self.und = self.add_equity('SPY', Resolution.DAILY).symbol
            style = OptionStyle.AMERICAN
        self.syms = []
        for right, strike, qty in self.contracts:
            sym = Symbol.create_option(self.und, Market.USA, style, right, strike, EXPIRY)
            if self.index:
                self.add_index_option_contract(sym, Resolution.DAILY)
            else:
                self.add_option_contract(sym, Resolution.DAILY)
            self.syms.append((sym, qty))
        self.done = False
        self.events = []
        self.assignments = []
        type(self).last = self

    def on_data(self, data):
        if not self.done:
            self.done = True
            for sym, qty in self.syms:
                self.market_order(sym, qty)

    def on_order_event(self, e):
        if e.status == OrderStatus.FILLED:
            self.events.append((str(e.symbol), e.fill_quantity, e.fill_price, e.is_assignment, e.message))

    def on_assignment_order_event(self, e):
        self.assignments.append((str(e.symbol), e.fill_quantity))


def equity_feed():
    return Feed({'SPY': 100.0})


class LongITMCall(_OptionAlgo):
    contracts = ((OptionRight.CALL, 90, 1),)


def test_long_itm_equity_call_is_exercised_into_shares_at_the_strike():
    ok(run(LongITMCall, '2022-01-03', '2022-01-28', equity_feed()))
    a = LongITMCall.last
    opt = a.syms[0][0]
    assert a.portfolio[opt].quantity == 0
    assert a.portfolio[a.und].quantity == 100
    assert a.portfolio[a.und].average_price == 90
    exercise = [e for e in a.events if e[0] == str(opt) and e[1] == -1]
    assert exercise and exercise[0][2] == 0.0          # LEAN: the option leg of an exercise fills at 0
    delivery = [e for e in a.events if e[0] == str(a.und)]
    assert delivery == [(str(a.und), 100, 90.0, False, delivery[0][4])]
    assert a.assignments == []


class ShortITMPut(_OptionAlgo):
    contracts = ((OptionRight.PUT, 110, -1),)


def test_short_itm_equity_put_is_assigned_and_reported():
    ok(run(ShortITMPut, '2022-01-03', '2022-01-28', equity_feed()))
    a = ShortITMPut.last
    opt = a.syms[0][0]
    assert a.portfolio[opt].quantity == 0
    assert a.portfolio[a.und].quantity == 100          # put assigned: shares bought at 110
    assert a.portfolio[a.und].average_price == 110
    assert any(e[3] for e in a.events)                 # an assignment fill (is_assignment)
    assert a.assignments and a.assignments[0][0] == str(opt)


class LongOTMCall(_OptionAlgo):
    contracts = ((OptionRight.CALL, 120, 1),)


def test_otm_contract_expires_worthless_without_delivery():
    ok(run(LongOTMCall, '2022-01-03', '2022-01-28', equity_feed()))
    a = LongOTMCall.last
    opt = a.syms[0][0]
    assert a.portfolio[opt].quantity == 0
    assert a.portfolio[a.und].quantity == 0
    expiry = [e for e in a.events if e[0] == str(opt) and e[1] == -1]
    assert expiry and expiry[0][2] == 0.0


class LongITMIndexCall(_OptionAlgo):
    index = True
    contracts = ((OptionRight.CALL, 3900, 1),)


def test_index_call_is_cash_settled_at_intrinsic_value():
    feed = Feed({'^GSPC': 4000.0})
    d = ok(run(LongITMIndexCall, '2022-01-03', '2022-01-28', feed, cash=1_000_000, symbols=('SPX',)))
    a = LongITMIndexCall.last
    opt = a.syms[0][0]
    assert a.portfolio[opt].quantity == 0
    assert a.portfolio[a.und].quantity == 0            # nothing delivered
    buy = [e for e in a.events if e[0] == str(opt) and e[1] == 1][0]
    settle = [e for e in a.events if e[0] == str(opt) and e[1] == -1][0]
    assert abs(settle[2] - 100.0) < 1e-9               # 4000 - 3900
    # P&L = (intrinsic - premium) x 100 (no costs in this run)
    assert abs(a.portfolio.total_portfolio_value - (1_000_000 + (100.0 - buy[2]) * 100)) < 1e-6
    assert d['uses_model_priced_derivatives'] is True
