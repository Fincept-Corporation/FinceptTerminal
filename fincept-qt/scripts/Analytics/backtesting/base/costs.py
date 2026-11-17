"""
Transaction-cost model shared by every engine (decision 3).

    model = CostModel.from_request(request, symbol='SPY')     # request values > preset > default
    fee   = model.commission(qty, price)                       # money per fill (vectorised)
    slip  = model.slippage_fractions(close, volume)            # (n, k) adverse fraction per fill
    model.to_contract()                                        # result `costs.model`

COMPONENTS (each reported separately in the result's `costs` block - never one booked as
another, never silently 0)
  commission   'pct_notional' fraction of traded notional | 'per_share' money per share |
               'per_trade' money per fill; optional minimum per fill and maximum as a fraction
               of notional (IBKR caps fixed-rate commissions at 1 % of trade value).
  slippage     'pct_price' fixed adverse fraction of the reference price | 'vol_scaled' k x the
               asset's daily return volatility over the previous 20 bars (known at the decision) |
               'none'. Applied to the FILL PRICE (buy higher, sell lower).
  spread       full quoted spread in basis points; half of it is paid on every fill (added to
               slippage, reported inside totalSlippage).
  impact       optional square-root impact ESTIMATE: impact_coef x daily vol x sqrt(qty / ADV20).
               Not calibrated - stated as an estimate.
  participation_cap   optional maximum fraction of the bar's volume one fill may take; the
               rest of the order is dropped and counted (capped fills).
  borrow       annual fee on the market value of short positions, accrued daily (ACT/360).
  cash interest / financing   annual rate on free cash (cash net of short proceeds) / on a
               negative cash balance, accrued daily (ACT/360). Off unless requested.

PRESETS (editable from the request: {"costModel": {"preset": "...", <field overrides>}})
Numbers are indicative published retail rates as of 2025-2026 and are labelled as such in
every result. They are starting points, not quotes for the user's account.
  none                  no costs at all (results carry a NO_COSTS warning).
  generic_bps           10 bp commission + 5 bp slippage per fill: the convention the
                        backtesting audit used for cross-engine comparison.
  us_equity_retail      0 commission (zero-commission US brokers - Schwab, Fidelity,
                        Robinhood, E*TRADE - since Oct 2019); 2 bp slippage + 2 bp quoted
                        spread (1 bp half-spread paid) - a typical one-way cost of a liquid
                        US large cap / ETF filled at the open or close on daily bars.
  us_equity_ibkr_fixed  Interactive Brokers Pro "Fixed": USD 0.005 per share, minimum
                        USD 1.00, maximum 1 % of trade value (interactivebrokers.com,
                        Commissions > Stocks, ETFs and Warrants > Fixed); 2 bp slippage +
                        2 bp spread. Regulatory pass-through fees not modelled.
  india_equity_delivery NSE/BSE delivery (cash) equity at a discount broker: 0 brokerage,
                        statutory charges approximated as 12 bp per fill (STT 0.1 % on buy
                        and sell, NSE transaction charge 0.00297 %, stamp duty 0.015 % on
                        buys, SEBI fee, 18 % GST on charges - zerodha.com/charges, 2024
                        schedule, averaged over buy and sell); 5 bp slippage.
  crypto_spot           0.10 % taker fee per fill (Binance spot, regular user / VIP 0,
                        binance.com fee schedule); 5 bp slippage + 2 bp spread.
  short borrow (all equity presets): 0.25 % a year - the usual general-collateral rate for
                        easy-to-borrow US large caps (hard-to-borrow names cost far more;
                        set borrowRateAnnual in the request).
DEFAULT when a request carries neither `costModel` nor `commission` / `slippage`: the
preset for the first symbol's asset class (crypto pairs -> crypto_spot, .NS/.BO ->
india_equity_delivery, everything else -> us_equity_retail). When the request carries the
screen's `commission` / `slippage` fractions they are used as pct_notional / pct_price
(preset 'request') - what the screen asked for is what runs.
"""

from __future__ import annotations

import math
from dataclasses import asdict, dataclass, field, replace
from typing import Any, Dict, List, Optional

import numpy as np

COMMISSION_TYPES = ('pct_notional', 'per_share', 'per_trade')
SLIPPAGE_TYPES = ('none', 'pct_price', 'vol_scaled')
VOL_WINDOW = 20


@dataclass
class CostModel:
    commission_type: str = 'pct_notional'
    commission_value: float = 0.0
    min_commission: float = 0.0
    max_commission_pct: Optional[float] = None
    slippage_type: str = 'pct_price'
    slippage_value: float = 0.0
    spread_bps: float = 0.0
    impact_coef: float = 0.0
    participation_cap: Optional[float] = None
    borrow_rate_annual: float = 0.0
    cash_rate_annual: float = 0.0
    financing_rate_annual: float = 0.0
    day_count: float = 360.0
    preset: Optional[str] = None
    source: Optional[str] = None
    notes: list = field(default_factory=list)

    # ── validation ──────────────────────────────────────────────────────────
    def __post_init__(self):
        if self.commission_type not in COMMISSION_TYPES:
            raise ValueError(f'commission type must be one of {", ".join(COMMISSION_TYPES)}, '
                             f'not {self.commission_type!r}')
        if self.slippage_type not in SLIPPAGE_TYPES:
            raise ValueError(f'slippage type must be one of {", ".join(SLIPPAGE_TYPES)}, '
                             f'not {self.slippage_type!r}')
        for name in ('commission_value', 'min_commission', 'slippage_value', 'spread_bps', 'impact_coef',
                     'borrow_rate_annual'):
            v = float(getattr(self, name) or 0.0)
            if not math.isfinite(v) or v < 0:
                raise ValueError(f'{name} must be a non-negative number, not {getattr(self, name)!r}')
            setattr(self, name, v)
        if self.participation_cap is not None:
            pc = float(self.participation_cap)
            if not 0 < pc <= 1:
                raise ValueError('participation cap must be in (0, 1]')
            self.participation_cap = pc

    # ── per-fill amounts ────────────────────────────────────────────────────
    def commission(self, qty, price):
        """Commission in money for fills of |qty| at `price` (scalars or arrays)."""
        q = np.abs(np.asarray(qty, dtype=float))
        p = np.asarray(price, dtype=float)
        notional = q * p
        if self.commission_type == 'pct_notional':
            fee = notional * self.commission_value
        elif self.commission_type == 'per_share':
            fee = q * self.commission_value
        else:
            fee = np.where(q > 0, self.commission_value, 0.0)
        if self.min_commission > 0:
            fee = np.where(q > 0, np.maximum(fee, self.min_commission), 0.0)
        if self.max_commission_pct is not None:
            fee = np.minimum(fee, notional * float(self.max_commission_pct))
        return float(fee) if np.ndim(fee) == 0 else fee

    def half_spread(self) -> float:
        return self.spread_bps / 2.0 / 1e4

    def slippage_fractions(self, close, volume=None) -> np.ndarray:
        """(n, k) adverse fraction applied to a fill on each bar (before market impact).

        pct_price : slippage_value everywhere; vol_scaled: slippage_value x std of the asset's
        close-to-close returns over the VOL_WINDOW bars BEFORE the bar (no look-ahead; the
        first bars use the expanding window, 0 before 2 returns exist). Half the spread is added.
        """
        c = np.asarray(close, dtype=float)
        if c.ndim == 1:
            c = c[:, None]
        base = np.zeros_like(c)
        if self.slippage_type == 'pct_price':
            base[:] = self.slippage_value
        elif self.slippage_type == 'vol_scaled':
            base = self.slippage_value * daily_vol(c)
        return base + self.half_spread()

    def to_contract(self) -> Dict[str, Any]:
        """The result's `costs.model` block (contract v2 + the extra fields)."""
        slip_type = self.slippage_type if (self.slippage_value > 0 or self.slippage_type == 'none') else 'none'
        return {
            'commissionType': self.commission_type,
            'commissionValue': self.commission_value,
            'minCommission': self.min_commission or None,
            'maxCommissionPct': self.max_commission_pct,
            'slippageType': slip_type,
            'slippageValue': self.slippage_value,
            'spreadBps': self.spread_bps,
            'impactCoef': self.impact_coef or None,
            'participationCap': self.participation_cap,
            'borrowRateAnnual': self.borrow_rate_annual,
            'financingRateAnnual': self.financing_rate_annual or None,
            'cashRateAnnual': self.cash_rate_annual or None,
            'dayCount': f'ACT/{int(self.day_count)}',
            'preset': self.preset,
            'source': self.source,
        }

    def describe(self) -> str:
        """One plain sentence for assumptions.costs."""
        if self.commission_type == 'pct_notional':
            c = f'commission {self.commission_value * 1e4:.2f} bp of notional per fill'
        elif self.commission_type == 'per_share':
            c = f'commission {self.commission_value:g} per share per fill'
        else:
            c = f'commission {self.commission_value:g} per fill'
        if self.min_commission:
            c += f' (minimum {self.min_commission:g})'
        if self.max_commission_pct is not None:
            c += f' (maximum {self.max_commission_pct * 100:g}% of notional)'
        if self.slippage_type == 'pct_price':
            s = f'slippage {self.slippage_value * 1e4:.2f} bp of price per fill'
        elif self.slippage_type == 'vol_scaled':
            s = f'slippage {self.slippage_value:g} x daily volatility per fill'
        else:
            s = 'no slippage'
        if self.spread_bps:
            s += f', half of a {self.spread_bps:g} bp spread'
        if self.impact_coef:
            s += f', square-root market impact estimate (k={self.impact_coef:g})'
        extra = []
        if self.borrow_rate_annual:
            extra.append(f'short borrow {self.borrow_rate_annual * 100:.2f}%/yr')
        if self.cash_rate_annual:
            extra.append(f'cash interest {self.cash_rate_annual * 100:.2f}%/yr')
        if self.financing_rate_annual:
            extra.append(f'financing {self.financing_rate_annual * 100:.2f}%/yr')
        tail = ('; ' + ', '.join(extra)) if extra else ''
        return f'{c}; {s}{tail}' + (f' (preset {self.preset})' if self.preset else '')

    def is_zero(self) -> bool:
        return (self.commission_value == 0 and self.min_commission == 0 and self.slippage_value == 0
                and self.spread_bps == 0 and self.impact_coef == 0)

    def scaled(self, multiple: float) -> 'CostModel':
        """The same model with every TRADING cost (commission, minimum, slippage, spread, impact)
        multiplied by `multiple` - for the cost-sensitivity table. Borrow / interest / financing
        rates and the participation cap are not trading costs and stay as they are."""
        m = float(multiple)
        if not math.isfinite(m) or m < 0:
            raise ValueError('a cost multiple must be a non-negative number')
        return replace(self, commission_value=self.commission_value * m, min_commission=self.min_commission * m,
                       slippage_value=self.slippage_value * m, spread_bps=self.spread_bps * m,
                       impact_coef=self.impact_coef * m, notes=list(self.notes))

    # ── construction ────────────────────────────────────────────────────────
    @classmethod
    def preset_named(cls, name: str) -> 'CostModel':
        key = str(name or '').strip().lower()
        if key not in PRESETS:
            raise ValueError(f'unknown cost preset {name!r}; choose one of {", ".join(sorted(PRESETS))}')
        return replace(PRESETS[key], notes=list(PRESETS[key].notes))

    @classmethod
    def from_request(cls, request: Optional[Dict[str, Any]], symbol: Optional[str] = None) -> 'CostModel':
        """Cost model of a run. Precedence: request `costModel` fields > request preset >
        the screen's `commission` / `slippage` fractions > default preset of the asset class."""
        req = request if isinstance(request, dict) else {}
        spec = req.get('costModel') if isinstance(req.get('costModel'), dict) else {}
        preset = spec.get('preset') or req.get('costPreset')
        if preset:
            model = cls.preset_named(preset)
        elif 'commission' in req or 'slippage' in req:
            model = CostModel(commission_type='pct_notional', commission_value=_f(req.get('commission')),
                              slippage_type='pct_price', slippage_value=_f(req.get('slippage')),
                              borrow_rate_annual=DEFAULT_BORROW, preset='request',
                              source="the screen's commission and slippage settings")
        else:
            model = cls.preset_named(default_preset_for(symbol or _first_symbol(req)))
        overrides = {}
        for camel, snake in _FIELD_MAP.items():
            if camel in spec:
                overrides[snake] = spec[camel]
        for camel in ('borrowRateAnnual', 'cashRateAnnual', 'financingRateAnnual'):
            if camel in req and camel not in spec:
                overrides[_FIELD_MAP[camel]] = req[camel]
        if overrides:
            model = replace(model, **{k: (v if k in ('commission_type', 'slippage_type', 'preset', 'source')
                                          or v is None else float(v)) for k, v in overrides.items()})
        return model


def _f(v) -> float:
    try:
        x = float(v)
    except (TypeError, ValueError):
        return 0.0
    return x if math.isfinite(x) and x > 0 else 0.0


def _first_symbol(req) -> str:
    syms = req.get('symbols') or []
    return str(syms[0]) if syms else ''


_FIELD_MAP = {
    'commissionType': 'commission_type', 'commissionValue': 'commission_value', 'minCommission': 'min_commission',
    'maxCommissionPct': 'max_commission_pct', 'slippageType': 'slippage_type', 'slippageValue': 'slippage_value',
    'spreadBps': 'spread_bps', 'impactCoef': 'impact_coef', 'participationCap': 'participation_cap',
    'borrowRateAnnual': 'borrow_rate_annual', 'cashRateAnnual': 'cash_rate_annual',
    'financingRateAnnual': 'financing_rate_annual',
}

DEFAULT_BORROW = 0.0025

PRESETS: Dict[str, CostModel] = {
    'none': CostModel(preset='none', source='no costs (explicit)'),
    'generic_bps': CostModel(commission_value=0.001, slippage_value=0.0005, borrow_rate_annual=DEFAULT_BORROW,
                             preset='generic_bps', source='backtesting audit convention: 10 bp + 5 bp per fill'),
    'us_equity_retail': CostModel(commission_value=0.0, slippage_value=0.0002, spread_bps=2.0,
                                  borrow_rate_annual=DEFAULT_BORROW, preset='us_equity_retail',
                                  source='zero-commission US retail brokers (since Oct 2019); indicative '
                                         'slippage and spread for liquid US large caps / ETFs'),
    'us_equity_ibkr_fixed': CostModel(commission_type='per_share', commission_value=0.005, min_commission=1.0,
                                      max_commission_pct=0.01, slippage_value=0.0002, spread_bps=2.0,
                                      borrow_rate_annual=DEFAULT_BORROW, preset='us_equity_ibkr_fixed',
                                      source='Interactive Brokers Pro Fixed: USD 0.005/share, min USD 1, '
                                             'max 1% of trade value'),
    'india_equity_delivery': CostModel(commission_value=0.0012, slippage_value=0.0005,
                                       borrow_rate_annual=DEFAULT_BORROW, preset='india_equity_delivery',
                                       source='NSE delivery statutory charges (STT, exchange, stamp, SEBI, GST) '
                                              'approximated per fill; zero brokerage'),
    'crypto_spot': CostModel(commission_value=0.001, slippage_value=0.0005, spread_bps=2.0,
                             preset='crypto_spot', source='Binance spot regular-user taker fee 0.10%'),
}


def default_preset_for(symbol: str) -> str:
    s = str(symbol or '').upper()
    if s.endswith(('-USD', '-USDT', '-EUR', '-BTC')) or s.endswith('USDT'):
        return 'crypto_spot'
    if s.endswith(('.NS', '.BO')) or s.startswith(('NSE:', 'BSE:')):
        return 'india_equity_delivery'
    return 'us_equity_retail'


def daily_vol(close: np.ndarray, window: int = VOL_WINDOW) -> np.ndarray:
    """(n, k) std of close-to-close returns over the `window` returns ending at the PREVIOUS
    bar (known before the bar trades). Expanding window at the start; 0 before 2 returns."""
    import pandas as pd
    c = np.asarray(close, dtype=float)
    if c.ndim == 1:
        c = c[:, None]
    with np.errstate(divide='ignore', invalid='ignore'):
        r = c[1:] / c[:-1] - 1.0
    r = np.vstack([np.full((1, c.shape[1]), np.nan), r])
    df = pd.DataFrame(r)
    sd = df.rolling(window, min_periods=2).std(ddof=1).shift(1).to_numpy()
    return np.where(np.isfinite(sd), sd, 0.0)


def adv(volume: np.ndarray, window: int = VOL_WINDOW) -> np.ndarray:
    """(n, k) average volume over the previous `window` bars (shares), NaN-safe, 0 when unknown."""
    import pandas as pd
    v = np.asarray(volume, dtype=float)
    if v.ndim == 1:
        v = v[:, None]
    a = pd.DataFrame(v).rolling(window, min_periods=1).mean().shift(1).to_numpy()
    return np.where(np.isfinite(a), a, 0.0)


SENSITIVITY_MULTIPLES = (0, 1, 2, 3)


def cost_sensitivity(simulate_equity, model: CostModel, dates, multiples=SENSITIVITY_MULTIPLES, rf: Any = 0.0,
                     ppy: Optional[float] = None, base_equity=None) -> List[Dict[str, Any]]:
    """Contract `costs.sensitivity`: the run repeated with the trading costs at 0x, 1x, 2x, 3x the
    model ([{multiple, totalReturn, cagr, sharpeRatio}]). simulate_equity(cost_model) -> per-bar
    equity of the SAME strategy and fills under that model (cheap with base.simulator). base_equity:
    the 1x curve already computed (saves one run). rf: annual rate (scalar) for the Sharpe column -
    a constant rate, stated by the caller; the headline Sharpe may use a yield series instead.
    A multiple whose run fails is reported with null figures."""
    from base import metrics as M
    idx = M.to_datetime_index(dates)
    p = ppy or M.infer_periods_per_year(idx)
    rows = []
    for m in multiples:
        try:
            eq = base_equity if (m == 1 and base_equity is not None) else simulate_equity(model.scaled(m))
            eq = np.asarray(eq, dtype=float)
            r = M.simple_returns(eq)
            rows.append({'multiple': m, 'totalReturn': M.total_return(eq), 'cagr': M.cagr(eq, idx),
                         'sharpeRatio': M.sharpe_ratio(r, M.rf_per_bar(len(r), rf, p), p)})
        except Exception:   # one failing multiple must not cost the user the run
            rows.append({'multiple': m, 'totalReturn': None, 'cagr': None, 'sharpeRatio': None})
    return rows


def costs_by_year(fills: Dict[str, Any], dates, equity) -> List[Dict[str, Any]]:
    """Contract `costs.byYear` straight from fills (bar, qty, price, ref_price, commission arrays):
    [{year, commission, slippage, tradedNotional, turnover}] with turnover = traded notional / the
    year's average equity. The normaliser otherwise rebuilds it from the trades' entry / exit legs,
    which is exact for round trips but not for trades that hold many fills (position episodes of a
    rebalancing portfolio) - engines pass this as costs['byYear'] then."""
    import pandas as pd
    idx = pd.DatetimeIndex(dates)
    bar = np.asarray(fills.get('bar', []), dtype=np.int64)
    q = np.abs(np.asarray(fills.get('qty', np.zeros(len(bar))), dtype=float))
    px = np.asarray(fills.get('price', np.zeros(len(bar))), dtype=float)
    ref = np.asarray(fills.get('ref_price', px), dtype=float)
    comm = np.asarray(fills.get('commission', np.zeros(len(bar))), dtype=float)
    eq = pd.Series(np.asarray(equity, dtype=float), index=idx)
    years = idx.year.to_numpy()
    fy = years[bar] if len(bar) else np.zeros(0, dtype=int)
    out = []
    for y in sorted(set(int(x) for x in fy)):
        m = fy == y
        notional = float(np.sum(q[m] * px[m]))
        ey = eq[years == y]
        avg = float(ey.mean()) if len(ey) else 0.0
        out.append({'year': int(y), 'commission': float(np.sum(comm[m])),
                    'slippage': float(np.sum(np.abs(px[m] - ref[m]) * q[m])), 'tradedNotional': notional,
                    'turnover': (notional / avg) if avg > 0 else None})
    return out


def contract_dict(model: Optional[CostModel]) -> Optional[Dict[str, Any]]:
    return model.to_contract() if model is not None else None


def as_dict(model: CostModel) -> Dict[str, Any]:
    return asdict(model)
