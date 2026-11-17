# ============================================================================
# Fincept Terminal - Strategy Engine: universes in the backtest (session extension)
#
# Runs the algorithm's UniverseManager (universe_selection.py) once per trading day before
# the open with the data known then (universe_data.py builds coarse / fundamental objects
# from the previous session), and turns its additions/removals into securities priced from
# the same DataSource as everything else. The disclosures the manager collects (candidate
# pool, fundamentals snapshot, ETF top holdings) go into the result notes.
# ============================================================================

from datetime import timedelta

from .enums import SecurityType
from .session import SessionExtension
from .types import Symbol


class UniverseExtension(SessionExtension):

    def __init__(self, runner=None):
        self.runner = runner
        self.manager = None
        self.dynamic = False
        self._option_of = {}

    @property
    def free_symbols(self):
        return self.dynamic

    def after_initialize(self, session):
        algo = session.algo
        mgr = algo._universe_manager
        self.manager = mgr
        mgr.post_initialize()
        kinds = {getattr(u, '_kind', '') for u in mgr.universes()}
        self.dynamic = any(k not in ('manual', 'user_defined', 'userdefined') for k in kinds)

    def calendar_series(self, session):
        if not self.dynamic:
            return []
        # universes may start empty: the US equity calendar (SPY's sessions) drives the run
        s = session.data.series(Symbol('SPY', SecurityType.EQUITY, 'usa'), SecurityType.EQUITY, 'usa')
        return [s] if s is not None else []

    def before_open(self, session, day, warming):
        if self.manager is None:
            return [], []
        if self.manager.calendar is None and session.calendar is not None:
            self.manager.calendar = session.calendar
        added, removed = self.manager.select(day)
        session.algo.time = day
        self._universe_options(session, added, removed)
        self._chain_models(session, day)
        return [str(s).upper() for s in added], [str(s).upper() for s in removed]

    def _chain_models(self, session, day):
        """Option / futures chain universe models: run their selector again every refresh_interval."""
        algo = session.algo
        for m in getattr(algo, '_derivative_models', None) or []:
            nxt = getattr(m, '_next_refresh', None)
            if nxt is not None and day < nxt:
                continue
            if nxt is not None:
                algo._derivative_universe_model(m)
            m._next_refresh = day + (getattr(m, 'refresh_interval', None) or timedelta(days=1))

    def _universe_options(self, session, added, removed):
        """add_universe_options(universe, filter): every equity the universe adds gets its option chain
        with the filter; the chain goes when the equity leaves the universe."""
        algo = session.algo
        pairs = getattr(algo, '_universe_option_filters', None)
        if not pairs:
            return
        for sym in added:
            for universe, flt in pairs:
                members = getattr(universe, 'securities', None)
                if members is not None and str(sym).upper() not in members:
                    continue
                s = sym if isinstance(sym, Symbol) else Symbol(str(sym))
                if s.security_type != SecurityType.EQUITY:
                    continue
                res = getattr(getattr(universe, 'universe_settings', None), 'resolution', None)
                opt = algo.add_option(s, res)
                if flt is not None:
                    opt.set_filter(flt)
                self._option_of[str(sym).upper()] = opt.symbol
        for sym in removed:
            can = self._option_of.pop(str(sym).upper(), None)
            if can is not None:
                algo.remove_security(can)

    def notes(self, session):
        return list(self.manager.notes()) if self.manager is not None else []

    def result_flags(self, session):
        if not self.dynamic:
            return {}
        notes = self.notes(session)
        return {'uses_universe_selection': True,
                'uses_fundamental_snapshot': any('Fundamental filters use current values' in n for n in notes)}
