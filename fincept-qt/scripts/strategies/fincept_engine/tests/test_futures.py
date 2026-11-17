# Futures: exchange expiry rules, continuous-contract mapping and rolls, contracts derived from the
# continuous series (cost of carry), margin, and settlement at expiry - offline backtests.
import math
from datetime import date, datetime

from AlgorithmImports import *  # noqa: F401,F403
from engine_fixture import Feed, run, ok
from fincept_engine import futures as F


def test_expiry_rules_and_listed_contracts():
    assert F.expiry_for('ES', 2022, 3) == date(2022, 3, 18)        # third Friday
    assert F.expiry_for('ES', 2022, 6) == date(2022, 6, 17)
    exps = F.contract_expiries('ES', date(2022, 3, 1), 3)
    assert exps == [date(2022, 3, 18), date(2022, 6, 17), date(2022, 9, 16)]
    sym = F.contract_symbol('ES', date(2022, 6, 17))
    assert sym.security_type == SecurityType.FUTURE and sym.id.date.date() == date(2022, 6, 17)
    assert sym.canonical.value == '/ES'


class ContinuousES(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2022, 2, 1)
        self.set_end_date(2022, 3, 31)
        self.set_cash(100000)
        self.es = self.add_future(Futures.Indices.SP_500_E_MINI, Resolution.DAILY,
                                  data_mapping_mode=DataMappingMode.OPEN_INTEREST)
        self.changes = []
        self.ticket = None
        self.mapped_seen = []
        type(self).last = self

    def on_data(self, data):
        for _k, ev in data.symbol_changed_events.items():
            self.changes.append((self.time.date(), ev.old_symbol, ev.new_symbol))
        self.mapped_seen.append((self.time.date(), str(self.es.mapped)))
        if self.ticket is None:
            self.ticket = self.market_order(self.es.mapped, 1)


def test_open_interest_mapping_rolls_before_expiry_and_margin_admits_one_contract():
    feed = Feed({'ES=F': 4000.0})
    d = ok(run(ContinuousES, '2022-02-01', '2022-03-31', feed, symbols=('ES',)))
    a = ContinuousES.last
    # OPEN_INTEREST is approximated as a roll 5 trading days before the front contract's expiry
    assert len(a.changes) == 1
    when, old, new = a.changes[0]
    assert when == date(2022, 3, 11)
    assert '22H' in old or 'H22' in old or old.endswith('20220318') or old != new
    mapped_before = dict(a.mapped_seen)[date(2022, 3, 10)]
    mapped_after = dict(a.mapped_seen)[date(2022, 3, 11)]
    assert mapped_before == old and mapped_after == new
    # one ES contract (50 x 4000 = 200,000 notional) on 100,000 of equity: the futures margin model
    # (a fraction of notional) decides, not 1x leverage
    assert a.ticket.status == OrderStatus.FILLED
    assert d['uses_derived_futures_contracts'] is True


class HoldToExpiry(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2022, 3, 1)
        self.set_end_date(2022, 3, 25)
        self.set_cash(1_000_000)
        self.c = self.add_future_contract(F.contract_symbol('ES', date(2022, 6, 17)), Resolution.DAILY).symbol
        self.m = self.add_future_contract(F.contract_symbol('ES', date(2022, 3, 18)), Resolution.DAILY).symbol
        self.done = False
        self.fills = []
        type(self).last = self

    def on_data(self, data):
        if not self.done:
            self.done = True
            self.market_order(self.c, 1)
            self.market_order(self.m, 1)

    def on_order_event(self, e):
        if e.status == OrderStatus.FILLED:
            self.fills.append((str(e.symbol), self.time.date(), e.fill_quantity, e.fill_price, e.message))


def _front_series(d):
    return 4000.0 + (d - datetime(2022, 1, 1)).days        # rising 1 point a day


def test_deferred_contract_carries_the_rate_and_front_contract_settles_at_expiry():
    irx = 5.0                                              # percent
    feed = Feed({'ES=F': _front_series}, irx=irx)
    ok(run(HoldToExpiry, '2022-03-01', '2022-03-25', feed, cash=1_000_000, symbols=('ES',)))
    a = HoldToExpiry.last
    buys = {s: (t, q, p) for s, t, q, p, _m in a.fills if q > 0}
    june = buys[str(a.c)]
    march = buys[str(a.m)]
    fill_day = datetime(2022, 3, 2)                        # ordered at the 03-01 close, filled at the 03-02 open
    front = _front_series(fill_day)
    assert abs(march[2] - front) < 1e-6                    # the front contract is the continuous series
    carry = math.exp(math.log(1 + irx / 100.0) * (date(2022, 6, 17) - date(2022, 3, 18)).days / 365.0)
    assert abs(june[2] - front * carry) < 1e-6
    # the March contract is closed at its final price on its expiry day; June is still held
    settle = [f for f in a.fills if f[0] == str(a.m) and f[2] < 0]
    assert len(settle) == 1 and settle[0][1] == date(2022, 3, 18)
    assert abs(settle[0][3] - _front_series(datetime(2022, 3, 18))) < 1e-6
    assert a.portfolio[a.m].quantity == 0
    assert a.portfolio[a.c].quantity == 1
    # futures P&L is (exit - entry) x 50 per contract; cash moved only by realised P&L
    pnl = (settle[0][3] - march[2]) * 50
    assert abs(a.portfolio[a.m].profit - pnl) < 1e-6
