# ============================================================================
# Fincept Terminal - Universe Selection Models (LEAN Algorithm.Framework.Selection)
#
# Each model's create_universes(algorithm) returns the universes of fincept_engine.universe;
# universe_selection.UniverseManager runs them on real data (pool bars, the Yahoo
# fundamentals snapshot, current ETF top holdings). Subclasses override select /
# select_coarse / select_fine as in LEAN (both the Python and the C# constructor
# signatures of FundamentalUniverseSelectionModel are accepted).
# Option and future universe models are placeholders (not supported when used).
# ============================================================================

from datetime import datetime, timedelta

from ..algorithm import UniverseSelectionModel
from ..enums import Market, SecurityType
from ..errors import not_supported
from ..types import Symbol
from ..universe import (UNCHANGED, UniverseSettings, FundamentalUniverseFactory, CoarseFundamentalUniverse,
                        FineFundamentalFilteredUniverse, ETFConstituentsUniverse, CustomUniverse, ScheduledUniverse,
                        ManualUniverse, _is_date_rule)
from ..universe_selection import call_flexible

_UNSET = object()
ONE_DAY = timedelta(days=1)


def _own_method(obj, pascal):
    """A PascalCase override defined by a strategy subclass (Select, SelectCoarse, SelectFine)."""
    fn = getattr(obj, pascal, None)
    return fn if callable(fn) else None


class FundamentalUniverseSelectionModel(UniverseSelectionModel):
    """Base for fundamental selection models (LEAN).

    Fundamental (single pass): FundamentalUniverseSelectionModel(), (universe_settings),
    (market, universe_settings), (selector[, universe_settings]); subclasses override
    select(algorithm, fundamental).
    Coarse/fine (obsolete in LEAN, still common): (filter_fine_data: bool[, universe_settings]);
    subclasses override select_coarse(algorithm, coarse) and select_fine(algorithm, fine)."""

    def __init__(self, *args, **kwargs):
        filter_fine = kwargs.pop('filter_fine_data', kwargs.pop('filterFineData', _UNSET))
        settings = kwargs.pop('universe_settings', kwargs.pop('universeSettings', None))
        selector = kwargs.pop('selector', None)
        market = kwargs.pop('market', None)
        for a in args:
            if isinstance(a, bool):
                if filter_fine is _UNSET:
                    filter_fine = a
            elif isinstance(a, UniverseSettings):
                settings = a
            elif isinstance(a, str):
                market = a
            elif a is not None and callable(a) and selector is None:
                selector = a
        self.filter_fine_data = None if filter_fine is _UNSET else filter_fine
        self.fundamental_data = self.filter_fine_data is None
        self.market = market or Market.USA
        self.universe_settings = settings
        self._selector = selector
        self._filter_fine = self.filter_fine_data     # earlier engine attribute

    def _settings(self, algorithm):
        return self.universe_settings if self.universe_settings is not None else algorithm.universe_settings

    def create_universes(self, algorithm):
        if self.fundamental_data:
            return [FundamentalUniverseFactory(self.market, self._settings(algorithm),
                                               lambda fundamental: self._run_select(algorithm, fundamental))]
        universe = self.create_coarse_fundamental_universe(algorithm)
        if self.filter_fine_data:
            if universe.asynchronous:
                raise ValueError("Asynchronous universe setting is not supported for coarse & fine selections, "
                                 "please use the new Fundamental single pass selection")
            universe = FineFundamentalFilteredUniverse(universe, lambda fine: self._run_select_fine(algorithm, fine))
        return [universe]

    def create_coarse_fundamental_universe(self, algorithm):
        return CoarseFundamentalUniverse(self._settings(algorithm),
                                         lambda coarse: self.filtered_select_coarse(algorithm, coarse))

    def filtered_select_coarse(self, algorithm, fundamental):
        """Coarse selection; with fine filtering, securities without fundamental data are dropped first."""
        if self.filter_fine_data:
            fundamental = [c for c in fundamental if c.has_fundamental_data]
        fn = _own_method(self, 'SelectCoarse') or self.select_coarse
        return call_flexible(fn, algorithm, fundamental)

    def _run_select(self, algorithm, fundamental):
        fn = _own_method(self, 'Select') or self.select
        return call_flexible(fn, algorithm, fundamental)

    def _run_select_fine(self, algorithm, fine):
        fn = _own_method(self, 'SelectFine') or self.select_fine
        return call_flexible(fn, algorithm, fine)

    def select(self, algorithm, fundamental):
        if self._selector is None:
            raise NotImplementedError("If inheriting, please override the 'select' fundamental function, "
                                      "else provide it as a constructor parameter")
        return call_flexible(self._selector, fundamental)

    def select_coarse(self, algorithm, coarse):
        raise NotImplementedError("Please override the 'select' fundamental function")

    def select_fine(self, algorithm, fine):
        return [f.symbol for f in fine]

    @staticmethod
    def coarse(coarse_selector):
        return CoarseFundamentalUniverseSelectionModel(coarse_selector)

    @staticmethod
    def fine(coarse_selector, fine_selector):
        return FineFundamentalUniverseSelectionModel(coarse_selector, fine_selector)

    @staticmethod
    def fundamental(selector):
        return FundamentalUniverseSelectionModel(selector)

    Coarse = coarse
    Fine = fine
    Fundamental = fundamental


class CoarseFundamentalUniverseSelectionModel(FundamentalUniverseSelectionModel):
    """Coarse selection from a function: CoarseFundamentalUniverseSelectionModel(coarse_selector[, settings])."""

    def __init__(self, coarse_selector=None, universe_settings=None, *args, **kwargs):
        super().__init__(False, universe_settings if isinstance(universe_settings, UniverseSettings) else None)
        self._coarse_selector = coarse_selector

    def select_coarse(self, algorithm, coarse):
        if self._coarse_selector is None:
            raise NotImplementedError("Please override the 'select_coarse' function")
        return self._coarse_selector(coarse)


class FineFundamentalUniverseSelectionModel(FundamentalUniverseSelectionModel):
    """Coarse then fine selection from two functions:
    FineFundamentalUniverseSelectionModel(coarse_selector, fine_selector[, settings])."""

    def __init__(self, coarse_selector=None, fine_selector=None, universe_settings=None, *args, **kwargs):
        super().__init__(True, universe_settings if isinstance(universe_settings, UniverseSettings) else None)
        self._coarse_selector = coarse_selector
        self._fine_selector = fine_selector

    def select_coarse(self, algorithm, coarse):
        if self._coarse_selector is None:
            raise NotImplementedError("Please override the 'select_coarse' function")
        return self._coarse_selector(coarse)

    def select_fine(self, algorithm, fine):
        if self._fine_selector is None:
            return [f.symbol for f in fine]
        return self._fine_selector(fine)


class QC500UniverseSelectionModel(FundamentalUniverseSelectionModel):
    """LEAN's QC500: top 1000 by dollar volume with fundamental data, then US companies on NYSE/NASDAQ,
    listed > 180 days, market cap > $500M, the most liquid ~500 spread across industry templates;
    refreshed monthly. Runs on the engine's candidate pool."""

    def __init__(self, filter_fine_data=True, universe_settings=None):
        super().__init__(filter_fine_data, universe_settings)
        self.number_of_symbols_coarse = 1000
        self.number_of_symbols_fine = 500
        self.dollar_volume_by_symbol = {}
        self.last_month = -1

    def select_coarse(self, algorithm, fundamental):
        if algorithm.time.month == self.last_month:
            return UNCHANGED
        top = sorted([x for x in fundamental if x.has_fundamental_data and x.volume > 0 and x.price > 0],
                     key=lambda x: x.dollar_volume, reverse=True)[:self.number_of_symbols_coarse]
        self.dollar_volume_by_symbol = {x.symbol: x.dollar_volume for x in top}
        if len(self.dollar_volume_by_symbol) == 0:
            return UNCHANGED
        return list(self.dollar_volume_by_symbol.keys())

    def select_fine(self, algorithm, fundamental):
        from itertools import groupby
        from math import ceil
        sorted_by_sector = sorted([x for x in fundamental if x.company_reference.country_id == "USA"
                                   and self.is_nyse_or_nasdaq(x)
                                   and (algorithm.time - x.security_reference.ipo_date).days > 180
                                   and x.market_cap > 5e8],
                                  key=lambda x: x.company_reference.industry_template_code or '')
        count = len(sorted_by_sector)
        if count == 0:
            return UNCHANGED
        self.last_month = algorithm.time.month
        percent = self.number_of_symbols_fine / count
        picked = []
        for _code, g in groupby(sorted_by_sector, lambda x: x.company_reference.industry_template_code or ''):
            y = sorted(g, key=lambda x: self.dollar_volume_by_symbol[x.symbol], reverse=True)
            picked.extend(y[:ceil(len(y) * percent)])
        picked = sorted(picked, key=lambda x: self.dollar_volume_by_symbol[x.symbol], reverse=True)
        return [x.symbol for x in picked[:self.number_of_symbols_fine]]

    @staticmethod
    def is_nyse_or_nasdaq(fundamental) -> bool:
        exchange = fundamental.company_reference.primary_exchange_id or fundamental.security_reference.exchange_id
        return exchange in ("NYS", "NYSE", "NAS")


class ETFConstituentsUniverseSelectionModel(UniverseSelectionModel):
    """Constituents of an ETF (its current top holdings): (etf_symbol_or_ticker[, settings][, filter])."""

    def __init__(self, etf_symbol, universe_settings=None, universe_filter_func=None):
        if isinstance(etf_symbol, str):
            etf_symbol = Symbol.create(etf_symbol.strip().upper(), SecurityType.EQUITY, Market.USA)
        if callable(universe_settings) and not isinstance(universe_settings, UniverseSettings) \
                and universe_filter_func is None:
            universe_settings, universe_filter_func = None, universe_settings
        self.etf_symbol = etf_symbol
        self.universe_settings = universe_settings
        self.universe_filter_function = universe_filter_func
        self.universe = None

    def create_universes(self, algorithm):
        if self.universe is None:
            settings = self.universe_settings if self.universe_settings is not None else algorithm.universe_settings
            self.universe = ETFConstituentsUniverse(self.etf_symbol, settings, self.universe_filter_function)
        return [self.universe]


class EmaCrossUniverseSelectionModel(FundamentalUniverseSelectionModel):
    """Securities whose fast EMA is above the slow EMA by > 1%, largest gap first (LEAN). The EMAs are
    updated with each day's adjusted price as the selection runs (no prior warm-up, as in LEAN)."""

    def __init__(self, fastPeriod=100, slowPeriod=300, universeCount=500, universeSettings=None, **kwargs):
        super().__init__(False, kwargs.get('universe_settings', universeSettings))
        self.fast_period = int(kwargs.get('fast_period', fastPeriod))
        self.slow_period = int(kwargs.get('slow_period', slowPeriod))
        self.universe_count = int(kwargs.get('universe_count', universeCount))
        self.tolerance = 0.01
        self.averages = {}

    def select_coarse(self, algorithm, fundamental):
        filtered = []
        for cf in fundamental:
            avg = self.averages.get(cf.symbol)
            if avg is None:
                avg = self.averages[cf.symbol] = self.SelectionData(cf.symbol, self.fast_period, self.slow_period)
            if avg.update(cf.end_time, cf.adjusted_price) and avg.fast > avg.slow * (1 + self.tolerance):
                filtered.append(avg)
        filtered = sorted(filtered, key=lambda a: a.scaled_delta, reverse=True)
        return [x.symbol for x in filtered[:self.universe_count]]

    class SelectionData:
        def __init__(self, symbol, fast_period, slow_period):
            from ..indicators import ExponentialMovingAverage
            self.symbol = symbol
            # name + period passed explicitly (the engine's EMA(int) shortcut does not set the period)
            self.fast_ema = ExponentialMovingAverage(f"EMA{fast_period}", int(fast_period))
            self.slow_ema = ExponentialMovingAverage(f"EMA{slow_period}", int(slow_period))

        @property
        def fast(self):
            return float(self.fast_ema.current.value)

        @property
        def slow(self):
            return float(self.slow_ema.current.value)

        @property
        def scaled_delta(self):
            return (self.fast - self.slow) / ((self.fast + self.slow) / 2)

        def update(self, time, value):
            return self.slow_ema.update(time, value) & self.fast_ema.update(time, value)


class ScheduledUniverseSelectionModel(UniverseSelectionModel):
    """selector(datetime) -> symbols on a date rule / time rule (LEAN). A leading time zone is accepted."""

    def __init__(self, *args, **kwargs):
        a = list(args)
        if a and not _is_date_rule(a[0]) and len(a) > 1 and _is_date_rule(a[1]):
            self._time_zone = a.pop(0)
        else:
            self._time_zone = kwargs.pop('time_zone', None)
        self._date_rule = a[0] if len(a) > 0 else kwargs.get('date_rule')
        self._time_rule = a[1] if len(a) > 1 else kwargs.get('time_rule')
        self._selector = a[2] if len(a) > 2 else kwargs.get('selector')
        self._settings = a[3] if len(a) > 3 else kwargs.get('settings', kwargs.get('universe_settings'))
        if not callable(self._selector):
            raise ValueError("Unable to create a scheduled universe selector: it is not a function. Please provide "
                             "a function that takes the firing date time and returns the selected symbols.")

    def create_universes(self, algorithm):
        settings = self._settings if isinstance(self._settings, UniverseSettings) else algorithm.universe_settings
        return [ScheduledUniverse(self._date_rule, self._time_rule, self._selector, settings)]


class CustomUniverseSelectionModel(UniverseSelectionModel):
    """Tickers from select(algorithm, date) or a selector(date), once per trading day (LEAN):
    CustomUniverseSelectionModel(name, selector) or (security_type, name, market, selector,
    universe_settings, interval)."""

    def __init__(self, *args, **kwargs):
        a = list(args)
        if a and isinstance(a[0], SecurityType):
            st = a[0]
            name = a[1] if len(a) > 1 else kwargs.get('name', 'custom')
            market = a[2] if len(a) > 2 else kwargs.get('market', Market.USA)
            selector = a[3] if len(a) > 3 else kwargs.get('selector')
            settings = a[4] if len(a) > 4 else kwargs.get('universe_settings')
            interval = a[5] if len(a) > 5 else kwargs.get('interval', ONE_DAY)
        else:
            st, market, interval = SecurityType.EQUITY, Market.USA, ONE_DAY
            name = a[0] if len(a) > 0 else kwargs.get('name', 'custom')
            selector = a[1] if len(a) > 1 else kwargs.get('selector')
            settings = a[2] if len(a) > 2 else kwargs.get('universe_settings', kwargs.get('settings'))
        self._security_type = st
        self._market = market or Market.USA
        self._name = str(name)
        self._selector = selector
        self._universe_settings = settings if isinstance(settings, UniverseSettings) else None
        self._interval = interval if isinstance(interval, timedelta) else ONE_DAY

    def create_universes(self, algorithm):
        settings = self._universe_settings if self._universe_settings is not None else algorithm.universe_settings
        name = f"{self._name}-{getattr(self._security_type, 'name', self._security_type)}-{self._market}"
        return [CustomUniverse(name=name, security_type=self._security_type, market=self._market,
                               universe_settings=settings, interval=self._interval,
                               selector=lambda date: self._run_select(algorithm, date))]

    def _run_select(self, algorithm, date):
        fn = _own_method(self, 'Select') or self.select
        return call_flexible(fn, algorithm, date)

    def select(self, algorithm, date):
        if self._selector is None:
            raise ValueError("CustomUniverseSelectionModel needs a selector function or a select() override")
        return self._selector(date)

    def __str__(self):
        return f"{self._name}-{getattr(self._security_type, 'name', self._security_type)}-{self._market}"


class ManualUniverseSelectionModel(UniverseSelectionModel):
    """A fixed list of symbols, subscribed when the model is added (LEAN ManualUniverseSelectionModel)."""

    def __init__(self, symbols=None, *args, **kwargs):
        settings = kwargs.get('universe_settings')
        flat = []
        for x in [symbols] + list(args):
            if isinstance(x, UniverseSettings):
                settings = x
            elif x is None:
                continue
            elif isinstance(x, (list, tuple, set)):
                flat.extend(x)
            else:
                flat.append(x)
        self._symbols = flat
        self.universe_settings = settings

    def symbols_list(self):
        return list(self._symbols)

    def create_universes(self, algorithm):
        settings = self.universe_settings if self.universe_settings is not None else algorithm.universe_settings
        return [ManualUniverse(self._symbols, settings)]


class InceptionDateUniverseSelectionModel(CustomUniverseSelectionModel):
    """Adds each ticker once the date reaches its inception date and keeps it (LEAN):
    InceptionDateUniverseSelectionModel(name, {ticker: datetime})."""

    def __init__(self, name, tickers_by_date):
        super().__init__(name, None)
        items = tickers_by_date.items() if hasattr(tickers_by_date, 'items') else list(tickers_by_date)
        self._queue = [(str(k), v if isinstance(v, datetime) else datetime(v.year, v.month, v.day))
                       for k, v in items]
        self._symbols = []

    def select(self, algorithm, date):
        added = []
        while self._queue and self._queue[0][1] <= date:
            added.append(self._queue.pop(0)[0])
        if not added:
            return UNCHANGED
        self._symbols.extend(added)
        return list(self._symbols)


# ---- ETF baskets (LEAN Algorithm.Framework/Selection/*ETFUniverse.cs; inception dates as in LEAN) ----

_ENERGY = {"XLE": (1998, 12, 22), "IYE": (2000, 6, 16), "VDE": (2004, 9, 29), "USO": (2006, 4, 10),
           "XES": (2006, 6, 22), "XOP": (2006, 6, 22), "UNG": (2007, 4, 18), "ICLN": (2008, 6, 25),
           "ERX": (2008, 11, 6), "ERY": (2008, 11, 6), "SCO": (2008, 11, 25), "UCO": (2008, 11, 25),
           "AMJ": (2009, 6, 2), "BNO": (2010, 6, 2), "AMLP": (2010, 8, 25), "OIH": (2011, 12, 21),
           "DGAZ": (2012, 2, 8), "UGAZ": (2012, 2, 8), "TAN": (2012, 2, 15)}
_METALS = {"GLD": (2004, 11, 18), "IAU": (2005, 1, 28), "SLV": (2006, 4, 28), "GDX": (2006, 5, 22),
           "AGQ": (2008, 12, 4), "GDXJ": (2009, 11, 11), "PPLT": (2010, 1, 8), "NUGT": (2010, 12, 8),
           "DUST": (2010, 12, 8), "USLV": (2011, 10, 17), "UGLD": (2011, 10, 17), "JNUG": (2013, 10, 3),
           "JDST": (2013, 10, 3)}
_TECHNOLOGY = {"XLK": (1998, 12, 22), "QQQ": (1999, 3, 10), "SOXX": (2001, 7, 13), "IGV": (2001, 7, 13),
               "VGT": (2004, 1, 30), "QTEC": (2006, 4, 25), "FDN": (2006, 6, 23), "FXL": (2007, 5, 10),
               "TECL": (2008, 12, 17), "TECS": (2008, 12, 17), "SOXL": (2010, 3, 11), "SOXS": (2010, 3, 11),
               "SKYY": (2011, 7, 6), "SMH": (2011, 12, 21), "KWEB": (2013, 8, 1), "FTEC": (2013, 10, 24)}
_TREASURIES = {"IEF": (2002, 7, 26), "SHY": (2002, 7, 26), "TLT": (2002, 7, 26), "IEI": (2007, 1, 11),
               "SHV": (2007, 1, 11), "TLH": (2007, 1, 11), "EDV": (2007, 12, 10), "BIL": (2007, 5, 30),
               "SPTL": (2007, 5, 30), "TBT": (2008, 5, 1), "TMF": (2009, 4, 16), "TMV": (2009, 4, 16),
               "TBF": (2009, 8, 20), "VGSH": (2009, 11, 23), "VGIT": (2009, 11, 23), "VGLT": (2009, 11, 24),
               "SCHO": (2010, 8, 6), "SCHR": (2010, 8, 6), "SPTS": (2011, 12, 1), "GOVT": (2012, 2, 24)}
_VOLATILITY = {"SQQQ": (2010, 2, 11), "TQQQ": (2010, 2, 11), "TVIX": (2010, 11, 30), "VIXY": (2011, 1, 4),
               "SPLV": (2011, 5, 5), "SVXY": (2011, 10, 4), "UVXY": (2011, 10, 4), "EEMV": (2011, 10, 20),
               "EFAV": (2011, 10, 20), "USMV": (2011, 10, 20)}
_SP500_SECTORS = {t: (1998, 12, 22) for t in ("XLB", "XLE", "XLF", "XLI", "XLK", "XLP", "XLU", "XLV", "XLY")}


def _dates(table):
    return {t: datetime(*ymd) for t, ymd in table.items()}


class _Basket(InceptionDateUniverseSelectionModel):
    _NAME = 'qc-etf-basket'
    _TABLE = {}

    def __init__(self):
        super().__init__(self._NAME, _dates(self._TABLE))


class EnergyETFUniverse(_Basket):
    _NAME, _TABLE = 'qc-energy-etf-basket', _ENERGY


class MetalsETFUniverse(_Basket):
    _NAME, _TABLE = 'qc-metals-etf-basket', _METALS


class TechnologyETFUniverse(_Basket):
    _NAME, _TABLE = 'qc-technology-etf-basket', _TECHNOLOGY


class USTreasuriesETFUniverse(_Basket):
    _NAME, _TABLE = 'qc-us-treasuries-etf-basket', _TREASURIES


class VolatilityETFUniverse(_Basket):
    _NAME, _TABLE = 'qc-volatility-etf-basket', _VOLATILITY


class SP500SectorsETFUniverse(_Basket):
    _NAME, _TABLE = 'qc-sp500-sectors-etf-basket', _SP500_SECTORS


class _Grouping(list):
    """A LiquidETFUniverse group: the long ETFs then the inverse ones (.long / .inverse)."""

    def __init__(self, long_tickers, inverse_tickers):
        self.long = [Symbol.create(t, SecurityType.EQUITY, Market.USA) for t in long_tickers]
        self.inverse = [Symbol.create(t, SecurityType.EQUITY, Market.USA) for t in inverse_tickers]
        super().__init__(self.long + self.inverse)

    Long = property(lambda self: self.long)
    Inverse = property(lambda self: self.inverse)

    def __str__(self):
        if not self:
            return "No Symbols"
        lng = f" Long: {','.join(s.value for s in self.long)}" if self.long else ''
        inv = f" Inverse: {','.join(s.value for s in self.inverse)}" if self.inverse else ''
        return f"{len(self)} symbols:{lng}{inv}"

    __repr__ = __str__


_ALL_INCEPTION = {}
for _t in (_ENERGY, _METALS, _TECHNOLOGY, _TREASURIES, _VOLATILITY, _SP500_SECTORS):
    _ALL_INCEPTION.update(_dates(_t))


class LiquidETFUniverse(InceptionDateUniverseSelectionModel):
    """LEAN's liquid ETF basket: each ETF joins on its inception date. ETFs Yahoo no longer lists
    (delisted products such as TVIX, UGAZ, DGAZ) cannot be priced and are skipped (noted)."""

    ENERGY = Energy = _Grouping(["VDE", "USO", "XES", "XOP", "UNG", "ICLN", "ERX", "UCO", "AMJ", "BNO", "AMLP",
                                 "UGAZ", "TAN"], ["ERY", "SCO", "DGAZ"])
    METALS = Metals = _Grouping(["GLD", "IAU", "SLV", "GDX", "AGQ", "PPLT", "NUGT", "USLV", "UGLD", "JNUG"],
                                ["DUST", "JDST"])
    TECHNOLOGY = Technology = _Grouping(["QQQ", "IGV", "QTEC", "FDN", "FXL", "TECL", "SOXL", "SKYY", "KWEB"],
                                        ["TECS", "SOXS"])
    TREASURIES = Treasuries = _Grouping(["IEF", "SHY", "TLT", "IEI", "TLH", "BIL", "SPTL", "TMF", "SCHO", "SCHR",
                                         "SPTS", "GOVT"], ["SHV", "TBT", "TBF", "TMV"])
    VOLATILITY = Volatility = _Grouping(["TVIX", "VIXY", "SPLV", "UVXY", "EEMV", "EFAV", "USMV"], ["SVXY"])
    SP_500_SECTORS = SP500_SECTORS = SP500Sectors = _Grouping(["XLB", "XLE", "XLF", "XLI", "XLK", "XLP", "XLU",
                                                               "XLV", "XLY"], [])
    SP500 = "SPY"   # kept from the earlier engine

    def __init__(self):
        groups = (self.SP_500_SECTORS, self.ENERGY, self.METALS, self.TECHNOLOGY, self.TREASURIES, self.VOLATILITY)
        table = {}
        for g in groups:
            for s in g:
                table.setdefault(s.value, _ALL_INCEPTION[s.value])
        super().__init__("qc-liquid-etf-basket", table)


# ---- placeholders (implemented elsewhere) ----

class _ChainUniverseSelectionModel(UniverseSelectionModel):
    """Base of the option / futures chain universe models: every `refresh_interval` the selector
    returns canonical symbols; each gets its chain (model-priced options, derived futures contracts)
    filtered by `filter()`. The algorithm subscribes them (QCAlgorithm._derivative_universe_model)."""

    def __init__(self, refresh_interval=None, chain_symbol_selector=None, universe_settings=None, *args):
        if chain_symbol_selector is None and callable(refresh_interval) and not isinstance(refresh_interval, timedelta):
            refresh_interval, chain_symbol_selector = timedelta(days=1), refresh_interval
        self.refresh_interval = refresh_interval if isinstance(refresh_interval, timedelta) else timedelta(days=1)
        self._selector = chain_symbol_selector
        self.universe_settings = universe_settings
        self._next_refresh = None

    def select_chain_symbols(self, utc_time):
        sel = self._selector
        if sel is None:
            return []
        try:
            return list(sel(utc_time) or [])
        except TypeError:
            return list(sel() or [])

    def filter(self, filter):
        """Override to narrow the chain (LEAN default: the universe as given)."""
        return filter

    def create_universes(self, algorithm):
        return []


class OptionUniverseSelectionModel(_ChainUniverseSelectionModel):
    """LEAN OptionUniverseSelectionModel(refresh_interval, option_chain_symbol_selector[, universe_settings])."""

    @property
    def _option_chain_symbol_selector(self):
        return self._selector


class FutureUniverseSelectionModel(_ChainUniverseSelectionModel):
    """LEAN FutureUniverseSelectionModel(refresh_interval, future_chain_symbol_selector[, universe_settings])."""

    @property
    def _future_chain_symbol_selector(self):
        return self._selector
