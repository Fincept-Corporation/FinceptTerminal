# indicator_history(): warms the indicator up from the bars known now and returns the values it
# produced; Bollinger sub-indicators keep their own windows.
import math
from datetime import datetime

from AlgorithmImports import *  # noqa: F401,F403
from engine_fixture import Feed, run, ok, trading_days


def spy(d):
    return 100 + 10 * math.sin((d - datetime(2021, 1, 1)).days / 7.0)


class Probe(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2022, 3, 1)
        self.set_end_date(2022, 3, 10)
        self.spy = self.add_equity('SPY', Resolution.DAILY).symbol
        self.result = None
        type(self).last = self

    def on_data(self, data):
        if self.result is not None:
            return
        self.bb = BollingerBands("BB", 20, 2.0, MovingAverageType.SIMPLE)
        self.ready_before = self.bb.window.is_ready
        self.result = self.indicator_history(self.bb, self.spy, 50)
        self.at = self.time


def test_indicator_history_warms_up_and_returns_the_last_values():
    ok(run(Probe, '2022-03-01', '2022-03-10', Feed({'SPY': spy})))
    a = Probe.last
    h = a.result
    assert a.ready_before is False and a.bb.is_ready
    assert h.count == 50 and len(h.current) == 50
    assert all(p.value != 0 for p in h.current)
    # the last value is the indicator's value on the bar complete when it was asked (no look-ahead)
    assert h.current[-1].time <= a.at
    assert abs(h.current[-1].value - a.bb.current.value) < 1e-12
    upper = h["upper_band"]
    lower = h["lower_band"]
    assert len(upper) == 50 and all(u.value > l.value for u, l in zip(upper, lower))
    df = h.data_frame
    assert list(df.columns)[:1] == ['current'] and len(df) == 50 and 'middle_band' in df.columns
    # the middle band is the mean of the 20 closes up to the bar complete when it was asked
    days = trading_days(datetime(2022, 1, 1), datetime(a.at.year, a.at.month, a.at.day))
    expected = sum(spy(d) for d in days[-20:]) / 20.0
    assert abs(h[-1]["middle_band"].value - expected) < 1e-9
    assert abs(h.current[-1].value - expected) < 1e-9
