# ============================================================================
# Fincept Terminal - Strategy Engine: sector-aware framework models
#
# SectorWeightingPortfolioConstructionModel and MaximumSectorExposureRiskManagementModel need each
# security's sector. It comes from security.fundamentals (universe_data.py: Yahoo Finance snapshot of
# today's classification, mapped to Morningstar sector codes) - a security's sector as of now, not as
# of each backtest date. The run notes say so.
# ============================================================================

from ..algorithm import (EqualWeightingPortfolioConstructionModel, RiskManagementModel, PortfolioTarget,
                         _insight_sign)

SECTOR_NOTE = ("Sector classifications come from a current Yahoo Finance snapshot mapped to Morningstar sector "
               "codes (today's sector, not the sector as of each backtest date).")
GROUPING_NOTE = ("MaximumSectorExposureRiskManagementModel groups securities by Morningstar sector code (LEAN "
                 "groups by the industry template code, which free data does not provide).")


def _note(algorithm, text):
    notes = getattr(algorithm, '_engine_notes', None)
    if notes is not None and text not in notes:
        notes.append(text)


def sector_of(algorithm, symbol):
    """Morningstar sector code of a security, or None when it has none (ETFs, unknown)."""
    sec = algorithm.securities.get(str(symbol).upper()) if algorithm is not None else None
    if sec is None:
        return None
    try:
        f = sec.fundamentals
        code = f.asset_classification.morningstar_sector_code if f is not None else None
    except Exception:
        code = None
    _note(algorithm, SECTOR_NOTE)
    try:
        code = int(code) if code is not None else None
    except (TypeError, ValueError):
        return None
    return code if code else None


class SectorWeightingPortfolioConstructionModel(EqualWeightingPortfolioConstructionModel):
    """Equal weight per sector, then equal weight per insight inside the sector (LEAN); insights of
    securities without a sector get no target."""

    def __init__(self, rebalance=None, *args, **kwargs):
        super().__init__(rebalance, *args, **kwargs)
        self._sector = {}
        self._algorithm = None

    def on_securities_changed(self, algorithm, changes):
        super().on_securities_changed(algorithm, changes)
        self._algorithm = algorithm
        for s in getattr(changes, 'removed_securities', None) or []:
            self._sector.pop(str(s.symbol).upper(), None)
        for s in getattr(changes, 'added_securities', None) or []:
            code = sector_of(algorithm, s.symbol)
            if code is not None:
                self._sector[str(s.symbol).upper()] = code

    def determine_target_percent(self, insights):
        buckets = {}
        for i in insights:
            code = self._sector.get(str(i.symbol).upper())
            if code is None and self._algorithm is not None:
                code = sector_of(self._algorithm, i.symbol)
                if code is not None:
                    self._sector[str(i.symbol).upper()] = code
            if code is not None:
                buckets.setdefault(code, []).append(i)
        out = {}
        if not buckets:
            return out
        sector_pct = 1.0 / len(buckets)
        for group in buckets.values():
            pct = sector_pct / len(group)
            for i in group:
                out[i] = self._allowed(_insight_sign(i)) * pct
        return out

    def create_targets(self, algorithm, insights):
        self._algorithm = algorithm
        return super().create_targets(algorithm, insights)


class MaximumSectorExposureRiskManagementModel(RiskManagementModel):
    """Scales down the targets of a sector whose absolute exposure (targets, else current holdings)
    exceeds `maximum_sector_exposure` of the portfolio value."""

    def __init__(self, maximum_sector_exposure=0.20):
        if maximum_sector_exposure <= 0:
            raise ValueError("MaximumSectorExposureRiskManagementModel: the maximum sector exposure cannot be a "
                             "non-positive value.")
        self._max = float(maximum_sector_exposure)
        self._targets = {}

    def manage_risk(self, algorithm, targets):
        _note(algorithm, GROUPING_NOTE)
        for t in targets:
            self._targets[str(t.symbol).upper()] = t
        limit = algorithm.portfolio.total_portfolio_value * self._max
        groups = {}
        for key, sec in list(algorithm.securities.items()):
            code = sector_of(algorithm, sec.symbol)
            if code is not None:
                groups.setdefault(code, []).append((key, sec))
        out = []
        for code, members in groups.items():
            total = 0.0
            for key, sec in members:
                t = self._targets.get(key)
                price = float(getattr(sec, 'price', 0.0) or 0.0)
                if t is not None:
                    total += abs(float(t.quantity) * price)
                else:
                    total += abs(algorithm.portfolio[key].holdings_value)
            if total > limit > 0:
                ratio = limit / total
                for key, sec in members:
                    t = self._targets.get(key)
                    if t is not None:
                        q = float(t.quantity) * ratio
                        q = float(int(q)) if abs(q) >= 1 else 0.0
                        out.append(PortfolioTarget(sec.symbol, q))
        return out

    def on_securities_changed(self, algorithm, changes):
        for s in getattr(changes, 'removed_securities', None) or []:
            self._targets.pop(str(s.symbol).upper(), None)
