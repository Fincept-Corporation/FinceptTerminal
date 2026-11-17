# ============================================================================
# Fincept Terminal - Strategy Engine: splits and dividends (session extension)
#
# Yahoo reports each equity's stock splits (ratio, effective date) and cash dividends
# (amount, ex-date). The engine turns them into LEAN events on the slice of the day they
# take effect: slice.splits[symbol] (a SplitType.WARNING the trading day before, then
# SPLIT_OCCURRED on the effective date) and slice.dividends[symbol], and calls on_splits /
# on_dividends before on_data. Only events up to the current day are delivered.
#
# Prices are split- and dividend-adjusted by default (LEAN's ADJUSTED mode), so holdings
# are not changed by these events. For a security the strategy subscribed in RAW or
# SPLIT_ADJUSTED mode the portfolio applies them like a broker: a split multiplies the
# quantity and divides the average price; a dividend pays quantity x amount in cash.
# ============================================================================

from datetime import datetime

from .enums import SecurityType, DataNormalizationMode
from .session import SessionExtension


class SplitType:
    WARNING = 0
    SPLIT_OCCURRED = 1
    Warning = 0
    SplitOccurred = 1


class Split:
    def __init__(self, symbol=None, time=None, price=0.0, split_factor=1.0, type_=SplitType.SPLIT_OCCURRED):
        self.symbol = symbol
        self.time = time
        self.end_time = time
        self.reference_price = float(price)
        self.split_factor = float(split_factor)
        self.type = type_
        self.value = self.split_factor
        self.price = self.reference_price

    def __str__(self):
        kind = 'Warning' if self.type == SplitType.WARNING else 'Split'
        return f"{kind}: {self.symbol}: {self.split_factor:g} | {self.reference_price:g}"

    SplitFactor = property(lambda self: self.split_factor)
    ReferencePrice = property(lambda self: self.reference_price)
    Type = property(lambda self: self.type)


class Dividend:
    def __init__(self, symbol=None, time=None, distribution=0.0, reference_price=0.0):
        self.symbol = symbol
        self.time = time
        self.end_time = time
        self.distribution = float(distribution)
        self.reference_price = float(reference_price)
        self.value = self.distribution
        self.price = self.distribution

    def __str__(self):
        return f"Dividend: {self.symbol}: {self.distribution:g} | {self.reference_price:g}"

    Distribution = property(lambda self: self.distribution)


class CorporateActions(SessionExtension):
    """Delivers split / dividend events of the run's equities (from Yahoo's actions)."""

    def __init__(self, actions_fetcher):
        # actions_fetcher(yahoo_ticker) -> {'dividends': [[date, amount]], 'splits': [[date, ratio]]}
        self._fetch = actions_fetcher
        self._events = {}      # key -> ({date: ratio}, {date: amount})
        self.applied = []      # notes about portfolio adjustments in raw modes

    def _for(self, session, key):
        if key in self._events:
            return self._events[key]
        s = session.series.get(key)
        ev = ({}, {}, {})
        sec = session.algo.securities.get(key)
        if s is not None and sec is not None and sec.symbol.security_type == SecurityType.EQUITY \
                and getattr(s, 'yahoo', None):
            try:
                acts = self._fetch(s.yahoo) or {}
            except Exception:
                acts = {}
            for d, r in acts.get('splits', []):
                ev[0][datetime.strptime(str(d)[:10], '%Y-%m-%d')] = float(r)
            for d, a in acts.get('dividends', []):
                ev[1][datetime.strptime(str(d)[:10], '%Y-%m-%d')] = float(a)
            # cash per share as paid at the time (a RAW-mode holding holds shares as traded then)
            for d, a in acts.get('dividends_raw', []):
                ev[2][datetime.strptime(str(d)[:10], '%Y-%m-%d')] = float(a)
        self._events[key] = ev
        return ev

    @staticmethod
    def _mode(session, key):
        sec = session.algo.securities.get(key)
        mode = getattr(sec, '_data_normalization_mode', None)
        if mode is None:
            for c in session.algo.subscription_manager.get_subscription_data_configs(key):
                m = getattr(c, '__dict__', {}).get('data_normalization_mode')
                if m is not None:
                    mode = m
        return int(mode) if mode is not None else int(DataNormalizationMode.ADJUSTED)

    def decorate_slice(self, session, sl, day):
        algo = session.algo
        cal_next = None
        for key, bar in list(sl.bars.items()):
            k = str(key).upper()
            splits, divs, divs_raw = self._for(session, k)
            if not splits and not divs:
                continue
            sec = algo.securities.get(k)
            sym = sec.symbol
            s = session.series.get(k)
            i = s.index_of_date(day) if s is not None else None
            prev_close = s.close[i - 1] if (i is not None and i > 0) else bar.close
            # warning the trading day before the effective date
            if s is not None and i is not None and i + 1 < len(s.dates) and s.dates[i + 1] in splits:
                r = splits[s.dates[i + 1]]
                sl.splits[sym] = Split(sym, sl.time, bar.close, 1.0 / r if r else 1.0, SplitType.WARNING)
            if day in splits:
                r = splits[day]
                sl.splits[sym] = Split(sym, sl.time, prev_close, 1.0 / r if r else 1.0, SplitType.SPLIT_OCCURRED)
                if self._mode(session, k) in (int(DataNormalizationMode.RAW),) and r:
                    h = algo.portfolio[k]
                    if h.quantity:
                        h.quantity = h.quantity * r
                        h.average_price = h.average_price / r
                        broker = getattr(algo, '_broker', None)
                        if broker is not None and hasattr(broker, 'record_split'):
                            broker.record_split(k, r, day)
                        self.applied.append(f"{k}: {r:g}-for-1 split on {day:%Y-%m-%d} applied to the holding")
            if day in divs:
                mode = self._mode(session, k)
                # RAW prices are as traded then: the dividend is the cash per share paid then; otherwise
                # the split-adjusted amount matches split-adjusted prices
                amt = divs_raw.get(day, divs[day]) if mode == int(DataNormalizationMode.RAW) else divs[day]
                sl.dividends[sym] = Dividend(sym, sl.time, amt, prev_close)
                if mode in (int(DataNormalizationMode.RAW), int(DataNormalizationMode.SPLIT_ADJUSTED)):
                    h = algo.portfolio[k]
                    if h.quantity:
                        cash = h.quantity * amt * h.multiplier
                        algo.portfolio._cash += cash
                        broker = getattr(algo, '_broker', None)
                        if broker is not None and hasattr(broker, 'record_carry'):
                            broker.record_carry(k, 'dividends', cash)
                        self.applied.append(f"{k}: dividend {amt:g} per share on {day:%Y-%m-%d} paid in cash")

    def notes(self, session):
        out = []
        if self.applied:
            out.append("Corporate actions applied to raw-price holdings: " + "; ".join(self.applied[:4])
                       + ("..." if len(self.applied) > 4 else ""))
        return out
