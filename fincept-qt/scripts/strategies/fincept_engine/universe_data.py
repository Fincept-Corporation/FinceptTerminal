# ============================================================================
# Fincept Terminal - Strategy Engine: data behind dynamic universes
#
#   * the candidate pool: resources/us_equity_pool.json (a fixed list of liquid US
#     stocks + major ETFs, with its as-of date and how it was compiled)
#   * daily bars of the pool through an injected fetch function (default: the shared
#     price store via yahoo_cache's adapters, 'raw' mode = split-adjusted OHLC + adjusted
#     close + every split event up to the store's last bar), loaded once per run up to the
#     end of the run's last month and kept as compact series (nothing extra is requested for
#     the split events after that window)
#   * coarse / Fundamental objects as of a selection day: built from the LAST SESSION
#     BEFORE that day only (LEAN selects at midnight with the previous session's data).
#     price and volume are raw values (Yahoo's split-adjusted series x the later split
#     ratios), dollar_volume = price x volume, adjusted_price = Yahoo's adjusted close
#   * the fundamentals snapshot: yfinance Ticker.info, cached on disk per ticker with a
#     TTL, fetched only when a selector touches a fine attribute, a whole selection batch
#     at a time, gently (few threads, back-off on rate limits). Price-dependent fields
#     (market cap, P/E, P/B, EV/EBITDA, yields...) are rescaled to the close before the
#     selection day; everything else is today's snapshot (disclosed).
#   * ETF holdings: yfinance funds_data.top_holdings (cached with a TTL), else the static
#     table resources/etf_top_holdings.json (with its as-of date)
#   * the disclosure texts of whatever a run used (notes()).
# Nothing here imports pandas / numpy / yfinance at module level.
# ============================================================================

import bisect
import json
import math
import re
import sys
import time
from datetime import datetime, timedelta
from pathlib import Path

from .enums import SecurityType, Market
from .errors import not_supported
from .types import Symbol
from .universe import (Fundamental, ETFConstituentData, MorningstarIndustryCode, MorningstarSectorCode, NO_DATE)

RESOURCES = Path(__file__).resolve().parent / 'resources'
POOL_FILE = RESOURCES / 'us_equity_pool.json'
ETF_HOLDINGS_FILE = RESOURCES / 'etf_top_holdings.json'

FUNDAMENTAL_DISCLOSURE = ("Fundamental filters use current values (Yahoo Finance snapshot), not values as of each "
                          "backtest date; results have survivorship and look-ahead bias.")
PRICE_DISCLOSURE = ("Universe selection ranks the pool with real daily bars known before each selection day: price "
                    "and volume are raw values rebuilt from Yahoo's split-adjusted bars, dollar volume = price x "
                    "volume, adjusted_price is Yahoo's dividend-adjusted close.")
ETF_DISCLOSURE = ("ETF constituent universes use only each ETF's current top holdings ({detail}), not its historical "
                  "membership and not the full basket.")


def pool_disclosure(pool) -> str:
    return (f"Coarse/fundamental universes select from a fixed pool of {len(pool.equities)} liquid US stocks and "
            f"{len(pool.etfs)} ETFs (today's large caps; list as of {pool.as_of}), not the full historical US "
            f"market: results have survivorship bias.")


def _log(msg: str) -> None:
    print(f"[universe-data] {msg}", file=sys.stderr)


def _day(x) -> datetime:
    return datetime(x.year, x.month, x.day)


def _num(v):
    if v is None or isinstance(v, bool):
        return None
    try:
        f = float(v)
    except (TypeError, ValueError):
        return None
    return f if math.isfinite(f) else None


def _str(v):
    if v is None:
        return None
    s = str(v).strip()
    return s or None


_WINDOWS_RESERVED = {'CON', 'PRN', 'AUX', 'NUL', *(f'COM{i}' for i in range(1, 10)), *(f'LPT{i}' for i in range(1, 10))}


def _safe_name(ticker: str) -> str:
    """A file name for a ticker on any OS: other characters become '_', and Windows device names
    (CON, NUL, COM1...) - reserved even with an extension - get a leading '_'."""
    name = ''.join(c if c.isalnum() or c in '-_.' else '_' for c in ticker) or '_'
    return '_' + name if name.split('.')[0].upper() in _WINDOWS_RESERVED else name


def yahoo_name(ticker: str) -> str:
    """Yahoo spelling of a US equity ticker (BRK.B -> BRK-B)."""
    from .data import yahoo_ticker
    y, _ = yahoo_ticker(ticker, SecurityType.EQUITY, Market.USA)
    return y or ticker


def lean_name(yahoo: str) -> str:
    """LEAN spelling of a Yahoo US equity ticker (BRK-B -> BRK.B)."""
    t = str(yahoo).strip().upper()
    return re.sub(r'^([A-Z]+)-([A-Z])$', r'\1.\2', t)


# ----------------------------------------------------------------------------
# The candidate pool
# ----------------------------------------------------------------------------

class EquityPool:
    """The fixed candidate list (equities have fundamental data, ETFs do not)."""

    def __init__(self, data: dict):
        self.as_of = data.get('as_of')
        self.compiled = data.get('compiled', '')
        self.verified = data.get('verified', '')
        self.equities = [str(t).strip().upper() for t in data.get('equities', [])]
        self.etfs = [str(t).strip().upper() for t in data.get('etfs', [])]
        self._equity_set = set(self.equities)
        self._all = list(dict.fromkeys(self.equities + self.etfs))
        self._all_set = set(self._all)

    @classmethod
    def load(cls, path=None):
        with open(path or POOL_FILE, 'r', encoding='utf-8') as f:
            return cls(json.load(f))

    @property
    def tickers(self):
        return self._all

    def __contains__(self, ticker):
        return str(ticker).upper() in self._all_set

    def has_fundamental_data(self, ticker) -> bool:
        return str(ticker).upper() in self._equity_set


# ----------------------------------------------------------------------------
# Compact daily series
# ----------------------------------------------------------------------------

class PoolSeries:
    """One ticker's daily bars in the loaded window, oldest first.

    close/open/high/low/volume: Yahoo split-adjusted values; adj: split + dividend adjusted close;
    factor[i]: product of the split ratios after dates[i] (raw price = close x factor)."""

    __slots__ = ('ticker', 'yahoo', 'dates', 'open', 'high', 'low', 'close', 'adj', 'volume', 'factor',
                 'first_bar')

    def __init__(self, ticker, yahoo):
        self.ticker, self.yahoo = ticker, yahoo
        self.dates, self.open, self.high, self.low = [], [], [], []
        self.close, self.adj, self.volume, self.factor = [], [], [], []
        self.first_bar = None

    def __len__(self):
        return len(self.dates)

    def index_of(self, d):
        i = bisect.bisect_left(self.dates, d)
        return i if i < len(self.dates) and self.dates[i] == d else None

    def last_before(self, t):
        """Index of the last bar complete at time t (a session's bar completes at 16:00), or -1."""
        cut = t - timedelta(hours=16)
        return bisect.bisect_right(self.dates, cut) - 1


def build_series(ticker, yahoo, entry, keep_from: str, keep_to: str):
    """PoolSeries from a yahoo_cache 'raw' entry {'bars': [[time, o, h, l, c, v, adj]], 'splits': [[date, ratio]]}.
    Bars outside [keep_from, keep_to] are dropped; split ratios after the window still count."""
    bars = entry.get('bars') or []
    splits = []
    for sp in entry.get('splits') or []:
        r = _num(sp[1]) if len(sp) > 1 else None
        if r and r > 0:
            splits.append((str(sp[0])[:10], r))
    splits.sort()
    split_dates = [d for d, _ in splits]
    suffix = [1.0] * (len(splits) + 1)
    for j in range(len(splits) - 1, -1, -1):
        suffix[j] = suffix[j + 1] * splits[j][1]
    s = PoolSeries(ticker, yahoo)
    for b in bars:
        ds = str(b[0])[:10]
        if s.first_bar is None:
            s.first_bar = ds
        if ds < keep_from or ds > keep_to:
            continue
        c = _num(b[4])
        if c is None or c <= 0:
            continue
        o, h, l = _num(b[1]) or c, _num(b[2]) or c, _num(b[3]) or c
        a = _num(b[6]) if len(b) > 6 else None
        v = _num(b[5]) or 0.0
        d = datetime(int(ds[:4]), int(ds[5:7]), int(ds[8:10]))
        if s.dates and s.dates[-1] >= d:
            continue
        s.dates.append(d)
        s.open.append(o)
        s.high.append(max(h, o, c))
        s.low.append(min(l, o, c))
        s.close.append(c)
        s.adj.append(a if a and a > 0 else c)
        s.volume.append(v)
        s.factor.append(suffix[bisect.bisect_right(split_dates, ds)])
    return s


def _default_fetch_bars(tickers, start, end_exclusive):
    """Pool bars from the shared price store (yahoo_cache adapters over price_feed): split-adjusted
    OHLC + adjusted close, and EVERY split from `start` up to the store's last bar - so the raw
    prices can be rebuilt without asking for the later split events separately."""
    from . import yahoo_cache
    return yahoo_cache.fetch_bars(tickers, start, end_exclusive, '1d', 'raw')


def _default_fetch_splits(tickers, start, end_exclusive):
    """Split events after the bar window. The default bar source already lists every later split
    with the window's bars (see _default_fetch_bars), so nothing more is requested: an empty list per
    ticker (an injected bar source without that property keeps its own fetch_splits)."""
    return {t: [] for t in tickers}


# ----------------------------------------------------------------------------
# Yahoo -> LEAN fundamentals mapping
# ----------------------------------------------------------------------------

INFO_KEYS = (
    'symbol', 'quoteType', 'exchange', 'currency', 'financialCurrency', 'shortName', 'longName', 'sector',
    'industry', 'sectorKey', 'industryKey', 'country', 'city', 'state', 'zip', 'address1', 'phone', 'website',
    'fullTimeEmployees', 'longBusinessSummary', 'firstTradeDateMilliseconds', 'firstTradeDateEpochUtc',
    'currentPrice', 'regularMarketPrice', 'previousClose', 'marketCap', 'enterpriseValue', 'sharesOutstanding',
    'impliedSharesOutstanding', 'trailingPE', 'forwardPE', 'priceToBook', 'priceToSalesTrailing12Months',
    'trailingPegRatio', 'pegRatio', 'enterpriseToEbitda', 'enterpriseToRevenue', 'trailingEps', 'forwardEps',
    'bookValue', 'revenuePerShare', 'totalCashPerShare', 'trailingAnnualDividendRate', 'dividendRate',
    'payoutRatio', 'returnOnEquity', 'returnOnAssets', 'grossMargins', 'operatingMargins', 'profitMargins',
    'ebitdaMargins', 'revenueGrowth', 'earningsGrowth', 'earningsQuarterlyGrowth', 'currentRatio', 'quickRatio',
    'debtToEquity', 'totalRevenue', 'grossProfits', 'ebitda', 'netIncomeToCommon', 'totalDebt', 'totalCash',
    'operatingCashflow', 'freeCashflow',
)

# How each LEAN field is sourced (documentation; '*' = rescaled to the close before the selection day).
FIELD_SOURCES = {
    'market_cap / company_profile.market_cap': 'marketCap * (close / snapshot price) *',
    'company_profile.enterprise_value': 'enterpriseValue + change in market cap *',
    'company_profile.shares_outstanding': 'marketCap / snapshot price (implied shares)',
    'valuation_ratios.pe_ratio, forward_pe_ratio, pb_ratio, ps_ratio, peg_ratio':
        'trailingPE, forwardPE, priceToBook, priceToSalesTrailing12Months, trailingPegRatio *',
    'valuation_ratios.ev_to_ebitda, ev_to_revenue': 'enterpriseToEbitda, enterpriseToRevenue *',
    'valuation_ratios.pcf_ratio, fcf_ratio, price_to_ebitda': 'market cap / operatingCashflow, freeCashflow, ebitda *',
    'valuation_ratios yields': 'inverse of the ratios above; dividend rates / close *',
    'valuation_ratios.book_value_per_share, sales_per_share, payout_ratio': 'bookValue, revenuePerShare, payoutRatio',
    'operation_ratios.roe/roa/gross_margin/operation_margin/net_margin/ebitda_margin (one_year)':
        'trailing-twelve-month returnOnEquity, returnOnAssets, grossMargins, operatingMargins, profitMargins, '
        'ebitdaMargins',
    'operation_ratios.revenue_growth, net_income_growth (three_months)': 'revenueGrowth, earningsQuarterlyGrowth '
                                                                         '(latest quarter, year over year)',
    'operation_ratios.current_ratio, quick_ratio, total_debt_equity_ratio (three_months)':
        'currentRatio, quickRatio, debtToEquity / 100 (latest quarter)',
    'earning_ratios.diluted_eps_growth (three_months)': 'earningsGrowth',
    'earning_reports.basic_eps / diluted_eps (twelve_months)': 'trailingEps (Yahoo reports diluted EPS)',
    'earning_reports.basic/diluted_average_shares (three_months, twelve_months)': 'implied shares outstanding',
    'earning_reports.dividend_per_share (twelve_months)': 'trailingAnnualDividendRate',
    'financial_statements (twelve_months = trailing twelve months, three_months = latest quarter)':
        'totalRevenue, grossProfits, ebitda, netIncomeToCommon, totalDebt, totalCash, operatingCashflow, '
        'freeCashflow',
    'asset_classification': 'Yahoo sector/industry (Morningstar taxonomy) mapped to Morningstar codes',
    'company_reference.industry_template_code': 'derived from the industry: B banks, I insurance, U utilities, '
                                                 'T transportation, M metals/mining/coal, N otherwise',
    'company_reference / security_reference / company_profile text fields': 'country (ISO3), exchange '
        '(NYS/NAS/ASE/ARCX/BATS), names, address, firstTradeDate (ipo_date), employees',
}


def _norm_name(s: str) -> str:
    return re.sub(r'[^a-z0-9]', '', str(s).lower().replace('&', 'and'))


_INDUSTRY_BY_NAME = {_norm_name(n): code for n, code in MorningstarIndustryCode._PAIRS}
_INDUSTRY_BY_NAME[_norm_name('Furnishings, Fixtures & Appliances')] = MorningstarIndustryCode.FURNISHINGS
_SECTOR_BY_NAME = {_norm_name(n): code for n, code in MorningstarSectorCode._PAIRS}

_EXCHANGES = {'NMS': 'NAS', 'NGM': 'NAS', 'NCM': 'NAS', 'NAS': 'NAS', 'NASDAQ': 'NAS', 'NYQ': 'NYS', 'NYS': 'NYS',
              'NYSE': 'NYS', 'ASE': 'ASE', 'AMEX': 'ASE', 'PCX': 'ARCX', 'BTS': 'BATS', 'PNK': 'PINX'}

_COUNTRIES = {
    'united states': 'USA', 'usa': 'USA', 'ireland': 'IRL', 'united kingdom': 'GBR', 'netherlands': 'NLD',
    'switzerland': 'CHE', 'canada': 'CAN', 'bermuda': 'BMU', 'china': 'CHN', 'taiwan': 'TWN', 'japan': 'JPN',
    'germany': 'DEU', 'france': 'FRA', 'israel': 'ISR', 'luxembourg': 'LUX', 'jersey': 'JEY',
    'cayman islands': 'CYM', 'singapore': 'SGP', 'uruguay': 'URY', 'brazil': 'BRA', 'australia': 'AUS',
    'denmark': 'DNK', 'sweden': 'SWE', 'india': 'IND', 'mexico': 'MEX', 'argentina': 'ARG', 'hong kong': 'HKG',
    'south korea': 'KOR', 'spain': 'ESP', 'italy': 'ITA', 'belgium': 'BEL', 'norway': 'NOR', 'finland': 'FIN',
    'panama': 'PAN', 'guernsey': 'GGY', 'puerto rico': 'PRI', 'chile': 'CHL', 'peru': 'PER', 'colombia': 'COL',
    'south africa': 'ZAF', 'greece': 'GRC', 'monaco': 'MCO', 'austria': 'AUT', 'bahamas': 'BHS',
}


def industry_template_code(industry_code: int, sector_code: int):
    """Morningstar statement template, derived from the industry (approximation, documented)."""
    if not sector_code:
        return None
    group = industry_code // 1000 if industry_code else 0
    if group == 10320:
        return 'B'
    if group == 10340:
        return 'I'
    if sector_code == 207:
        return 'U'
    if group == 31080:
        return 'T'
    if group in (10150, 10160) or industry_code in (30920010, 30920020):
        return 'M'
    return 'N'


def classify(info: dict):
    """(sector, industry_group, industry, economy_sphere) Morningstar codes from Yahoo's taxonomy (0 = missing)."""
    ind = 0
    for key in ('industry', 'industryKey'):
        v = info.get(key)
        if v:
            ind = _INDUSTRY_BY_NAME.get(_norm_name(v), 0)
            if ind:
                break
    if ind:
        sector = ind // 100000
    else:
        sector = 0
        for key in ('sector', 'sectorKey'):
            v = info.get(key)
            if v:
                sector = _SECTOR_BY_NAME.get(_norm_name(v), 0)
                if sector:
                    break
    group = ind // 1000 if ind else 0
    sphere = sector // 100 if sector else 0
    return sector, group, ind, sphere


def _ipo_date(info):
    ms = _num(info.get('firstTradeDateMilliseconds'))
    if ms is None:
        sec = _num(info.get('firstTradeDateEpochUtc'))
        ms = sec * 1000.0 if sec is not None else None
    if ms is None:
        return None
    try:
        d = datetime(1970, 1, 1) + timedelta(milliseconds=ms)
    except (OverflowError, ValueError):
        return None
    return datetime(d.year, d.month, d.day)


def _int(v):
    f = _num(v)
    return int(round(f)) if f is not None else None


def map_fundamentals(info: dict, price, ticker: str) -> dict:
    """LEAN fine-attribute values from a Yahoo info snapshot. `price` is the split-adjusted close
    of the session before the selection day (Yahoo's units: per current share); price-dependent
    fields are rescaled to it. Missing inputs give None (the section shows LEAN's missing value)."""
    if not info:
        return {}
    g = lambda k: _num(info.get(k))
    price = _num(price)
    snap = g('currentPrice') or g('regularMarketPrice') or g('previousClose')
    r = price / snap if (price and snap) else None

    def scaled(v):
        return v * r if (v is not None and r is not None) else None

    def inv(x):
        return 1.0 / x if x else None

    mcap_snap = g('marketCap')
    shares = (mcap_snap / snap) if (mcap_snap and snap) else (g('impliedSharesOutstanding') or g('sharesOutstanding'))
    mcap = scaled(mcap_snap) if mcap_snap else (shares * price if (shares and price) else None)
    ev_snap = g('enterpriseValue')
    ev = ev_snap + (mcap - mcap_snap) if (ev_snap is not None and mcap is not None and mcap_snap) else None
    er = ev / ev_snap if (ev is not None and ev_snap) else None
    same_ccy = info.get('financialCurrency') in (None, info.get('currency'))
    ocf, fcf, ebitda = g('operatingCashflow'), g('freeCashflow'), g('ebitda')

    pe, fpe = scaled(g('trailingPE')), scaled(g('forwardPE'))
    pb, ps = scaled(g('priceToBook')), scaled(g('priceToSalesTrailing12Months'))
    peg = scaled(g('trailingPegRatio') if g('trailingPegRatio') is not None else g('pegRatio'))
    pcf = mcap / ocf if (same_ccy and mcap and ocf) else None
    pfcf = mcap / fcf if (same_ccy and mcap and fcf) else None
    eps = g('trailingEps')
    if pe:
        ey = 1.0 / pe
    else:
        ey = eps / price if (eps is not None and price and same_ccy) else None
    tdr, dr = g('trailingAnnualDividendRate'), g('dividendRate')

    def ev_scaled(k):
        v = g(k)
        return v * er if (v is not None and er is not None) else None

    valuation = {
        'pe_ratio': pe, 'forward_pe_ratio': fpe, 'pb_ratio': pb, 'ps_ratio': ps, 'pcf_ratio': pcf,
        'fcf_ratio': pfcf, 'peg_ratio': peg, 'ev_to_ebitda': ev_scaled('enterpriseToEbitda'),
        'ev_to_revenue': ev_scaled('enterpriseToRevenue'),
        'price_to_ebitda': mcap / ebitda if (same_ccy and mcap and ebitda) else None,
        'earning_yield': ey, 'forward_earning_yield': inv(fpe), 'sales_yield': inv(ps),
        'book_value_yield': inv(pb), 'cf_yield': inv(pcf), 'fcf_yield': inv(pfcf),
        'trailing_dividend_yield': tdr / price if (tdr is not None and price) else None,
        'forward_dividend_yield': dr / price if (dr is not None and price) else None,
        'forward_dividend': dr, 'payout_ratio': g('payoutRatio'), 'book_value_per_share': g('bookValue'),
        'sales_per_share': g('revenuePerShare'),
        'cash_cash_equivalents_and_short_term_investments_per_share': g('totalCashPerShare'),
    }
    dte = g('debtToEquity')
    operation = {
        'roe': {'one_year': g('returnOnEquity')}, 'roa': {'one_year': g('returnOnAssets')},
        'gross_margin': {'one_year': g('grossMargins')}, 'operation_margin': {'one_year': g('operatingMargins')},
        'net_margin': {'one_year': g('profitMargins')}, 'ebitda_margin': {'one_year': g('ebitdaMargins')},
        'revenue_growth': {'three_months': g('revenueGrowth')},
        'net_income_growth': {'three_months': g('earningsQuarterlyGrowth')},
        'current_ratio': {'three_months': g('currentRatio')}, 'quick_ratio': {'three_months': g('quickRatio')},
        'total_debt_equity_ratio': {'three_months': dte / 100.0 if dte is not None else None},
    }
    earning_ratios = {'diluted_eps_growth': {'three_months': g('earningsGrowth')}}
    earning_reports = {
        'basic_eps': {'twelve_months': eps}, 'diluted_eps': {'twelve_months': eps},
        'basic_average_shares': {'three_months': shares, 'twelve_months': shares},
        'diluted_average_shares': {'three_months': shares, 'twelve_months': shares},
        'dividend_per_share': {'twelve_months': tdr},
    }
    statements = {
        'income_statement': {
            'total_revenue': {'twelve_months': g('totalRevenue')}, 'gross_profit': {'twelve_months': g('grossProfits')},
            'ebitda': {'twelve_months': ebitda}, 'net_income': {'twelve_months': g('netIncomeToCommon')},
            'net_income_common_stockholders': {'twelve_months': g('netIncomeToCommon')}},
        'balance_sheet': {
            'total_debt': {'three_months': g('totalDebt')},
            'cash_cash_equivalents_and_short_term_investments': {'three_months': g('totalCash')}},
        'cash_flow_statement': {
            'operating_cash_flow': {'twelve_months': ocf}, 'free_cash_flow': {'twelve_months': fcf}},
    }
    sector, group, industry, sphere = classify(info)
    country = _COUNTRIES.get(str(info.get('country') or '').strip().lower())
    exchange = _EXCHANGES.get(str(info.get('exchange') or '').strip().upper())
    company_reference = {
        'country_id': country, 'primary_exchange_id': exchange,
        'industry_template_code': industry_template_code(industry, sector),
        'short_name': _str(info.get('shortName')), 'standard_name': _str(info.get('longName')),
        'legal_name': _str(info.get('longName')), 'primary_symbol': ticker, 'business_country_id': country,
        'is_reit': bool(industry and industry // 10000 == 1042),
    }
    security_reference = {
        'security_symbol': ticker, 'exchange_id': exchange, 'currency_id': _str(info.get('currency')),
        'ipo_date': _ipo_date(info),
    }
    profile = {
        'headquarter_address_line1': _str(info.get('address1')), 'headquarter_city': _str(info.get('city')),
        'headquarter_province': _str(info.get('state')), 'headquarter_country': country,
        'headquarter_postal_code': _str(info.get('zip')), 'headquarter_phone': _str(info.get('phone')),
        'headquarter_homepage': _str(info.get('website')),
        'total_employee_number': _int(info.get('fullTimeEmployees')),
        'full_time_employee_number': _int(info.get('fullTimeEmployees')),
        'shares_outstanding': _int(shares), 'market_cap': _int(mcap), 'enterprise_value': _int(ev),
        'long_description': _str(info.get('longBusinessSummary')),
    }
    classification = {
        'morningstar_sector_code': sector or None, 'morningstar_industry_group_code': group or None,
        'morningstar_industry_code': industry or None, 'morningstar_economy_sphere_code': sphere or None,
    }
    return {'valuation_ratios': valuation, 'operation_ratios': operation, 'earning_ratios': earning_ratios,
            'earning_reports': earning_reports, 'financial_statements': statements,
            'company_reference': company_reference, 'security_reference': security_reference,
            'company_profile': profile, 'asset_classification': classification}


# ----------------------------------------------------------------------------
# Default Yahoo fetchers (fundamentals snapshot, ETF holdings)
# ----------------------------------------------------------------------------

_RETRIES = 4
_HOLDINGS_TTL_S = 86400   # fincept_yahoo's shared cache of an ETF's top holdings (on top of the 7-day file)


def _fy():
    """fincept_yahoo (shared session, limiter / back-off / breaker, daily profile cache); the
    scripts folder is three levels up (fincept_engine -> strategies -> scripts)."""
    scripts_root = str(Path(__file__).resolve().parents[2])
    if scripts_root not in sys.path:
        sys.path.append(scripts_root)
    import fincept_yahoo
    return fincept_yahoo


def _is_rate_limit(e) -> bool:
    try:
        from yfinance.exceptions import YFRateLimitError
        if isinstance(e, YFRateLimitError):
            return True
    except Exception:
        pass
    s = str(e)
    return '429' in s or 'Too Many Requests' in s or 'Rate limited' in s


def _with_backoff(fn, what):
    delay = 2.0
    for attempt in range(_RETRIES):
        try:
            return fn()
        except Exception as e:  # noqa: BLE001 - classified below
            if type(e).__name__ == 'YahooRateLimited':
                raise  # nothing was sent: fincept_yahoo's shared breaker / limiter decides when to retry
            if _is_rate_limit(e) and attempt < _RETRIES - 1:
                _log(f"rate limited by Yahoo ({what}); waiting {delay:.0f}s")
                time.sleep(delay)
                delay *= 2
                continue
            raise
    raise RuntimeError(f"Yahoo Finance kept rate-limiting {what}")


def default_fetch_info(yahoo: str) -> dict:
    """Ticker.info reduced to the keys the mapping uses ({} = Yahoo has nothing), from
    fincept_yahoo's daily profile cache shared by every script and process."""
    fy = _fy()
    info = _with_backoff(lambda: fy.info(yahoo), f"fundamentals of {yahoo}")
    out = {k: info[k] for k in INFO_KEYS if info.get(k) is not None}
    if not out.get('quoteType') and not out.get('marketCap'):
        return {}
    return out


def default_fetch_holdings(yahoo: str):
    """[(ticker, name, weight)] of an ETF's top holdings from Yahoo funds_data, or None. Kept a
    day in fincept_yahoo's shared cache (an empty list = Yahoo lists none)."""
    fy = _fy()

    def fetch():
        th = fy.yf().Ticker(yahoo).funds_data.top_holdings
        rows = []
        if th is not None and len(th) > 0:
            for sym, r in th.iterrows():
                w = _num(r.get('Holding Percent'))
                if w is None:
                    continue
                rows.append([str(sym), str(r.get('Name') or ''), w])
        return rows

    rows = _with_backoff(lambda: fy.cached('etf_holdings', yahoo, fetch, max_age=_HOLDINGS_TTL_S),
                         f"holdings of {yahoo}")
    return [tuple(r) for r in rows or []] or None


# ----------------------------------------------------------------------------
# The provider
# ----------------------------------------------------------------------------

class UniverseDataProvider:
    """Pool bars, coarse/Fundamental objects as of a day, fundamentals snapshot cache, ETF holdings.

    fetch_bars(tickers, start, end_exclusive) -> {yahoo_ticker: {'bars': [[time, o, h, l, c, v, adj]],
    'splits': [[date, ratio]]}} (yahoo_cache.fetch_bars 'raw' format); fetch_info(yahoo) -> dict;
    fetch_holdings(yahoo) -> [(ticker, name, weight)] | None; fetch_splits(tickers, start, end_exclusive)
    -> {yahoo_ticker: [[date, ratio]]} (split events after the bar window; defaults to monthly Yahoo
    bars, or to fetch_bars when only fetch_bars is injected). All default to Yahoo.
    lookback_days: calendar days of bars loaded before the first selection day (the fetch start is
    rounded down to January 1 so different runs share the cache). Pass 730 to also serve the
    runner's history window from adjusted_daily()."""

    CHUNK = 40

    def __init__(self, fetch_bars=None, fetch_info=None, fetch_holdings=None, pool=None, lookback_days=30,
                 info_ttl_days=7.0, holdings_ttl_days=7.0, today=None, cache_dir=None, info_workers=4,
                 static_holdings_file=None, fetch_splits=None):
        self._fetch_bars = fetch_bars or _default_fetch_bars
        if fetch_splits is None:
            if fetch_bars is None:
                fetch_splits = _default_fetch_splits
            else:   # an injected bar source also supplies the later split events (daily bars)
                fetch_splits = lambda t, s, e: {y: d.get('splits') or [] for y, d in (fetch_bars(t, s, e) or {}).items()}
        self._fetch_splits = fetch_splits
        self._fetch_info = fetch_info or default_fetch_info
        self._fetch_holdings = fetch_holdings or default_fetch_holdings
        if pool is None:
            pool = EquityPool.load()
        elif isinstance(pool, (str, Path)):
            pool = EquityPool.load(pool)
        elif isinstance(pool, dict):
            pool = EquityPool(pool)
        self.pool = pool
        self.lookback_days = int(lookback_days)
        self.info_ttl = timedelta(days=float(info_ttl_days))
        self.holdings_ttl = timedelta(days=float(holdings_ttl_days))
        self.info_workers = max(1, int(info_workers))
        self._today = _day(today) if today is not None else None
        self._cache_dir = Path(cache_dir) if cache_dir else None
        self._static_holdings_file = Path(static_holdings_file) if static_holdings_file else ETF_HOLDINGS_FILE
        self._series = {}            # ticker -> PoolSeries | None
        self._window = None          # (first_day, last_day) of the pool load
        self._fetch_start = None
        self._ref_days = []
        self._coarse_day = None
        self._coarse_list = []
        self._symbols = {}
        self._info = {}              # ticker -> info dict ({} = none)
        self._info_failed = []
        self._info_stale = []
        self._holdings = {}          # etf -> record | None
        self._used = set()
        self._etf_used = {}
        self.missing_pool = []
        self.timings = {'bars_seconds': 0.0, 'info_seconds': 0.0, 'info_fetched': 0}

    # ---------------------------------------------------------------- helpers
    @property
    def today(self) -> datetime:
        return self._today or _day(datetime.now())

    def cache_dir(self) -> Path:
        if self._cache_dir is None:
            from .yahoo_cache import cache_root
            self._cache_dir = cache_root() / 'universe'
        return self._cache_dir

    def symbol(self, ticker) -> Symbol:
        t = str(ticker).upper()
        s = self._symbols.get(t)
        if s is None:
            s = Symbol.create(t, SecurityType.EQUITY, Market.USA)
            self._symbols[t] = s
        return s

    @staticmethod
    def ticker_of(symbol) -> str:
        return str(getattr(symbol, 'value', symbol)).upper()

    # ---------------------------------------------------------------- bars
    def _fetch_start_for(self, first_day):
        d = first_day - timedelta(days=self.lookback_days)
        return datetime(d.year, 1, 1)

    def _load(self, tickers, fetch_start, keep_to):
        """Daily bars from fetch_start to the end of keep_to's month; the split events after that
        (needed to rebuild raw prices) come from a light monthly tail up to today."""
        today_excl = self.today + timedelta(days=1)
        month_after = datetime(keep_to.year + keep_to.month // 12, keep_to.month % 12 + 1, 1)
        tail = month_after < today_excl
        daily_end = month_after if tail else max(keep_to + timedelta(days=1), today_excl)
        s_str, e_str = fetch_start.strftime('%Y-%m-%d'), daily_end.strftime('%Y-%m-%d')
        tail_s, tail_e = month_after.strftime('%Y-%m-%d'), today_excl.strftime('%Y-%m-%d')
        keep_from, keep_to_s = s_str, keep_to.strftime('%Y-%m-%d')
        names = [(t, yahoo_name(t)) for t in tickers]
        t_all = time.monotonic()
        for i in range(0, len(names), self.CHUNK):
            chunk = names[i:i + self.CHUNK]
            t0 = time.monotonic()
            ys = [y for _, y in chunk]
            got = self._fetch_bars(ys, s_str, e_str) or {}
            later = {}
            if tail:
                have = [y for y in ys if got.get(y)]
                later = (self._fetch_splits(have, tail_s, tail_e) or {}) if have else {}
                missing = [y for y in have if y not in later]
                if missing:   # no monthly data: take the split events from daily bars instead
                    daily = self._fetch_bars(missing, tail_s, tail_e) or {}
                    for y in missing:
                        later[y] = (daily.get(y) or {}).get('splits') or []
            for t, y in chunk:
                entry = got.get(y)
                if entry and tail:
                    entry = dict(entry)
                    entry['splits'] = list(entry.get('splits') or []) + list(later.get(y) or [])
                s = build_series(t, y, entry, keep_from, keep_to_s) if entry else None
                self._series[t] = s if s is not None and len(s) else None
            if time.monotonic() - t0 > 2.0 and i + self.CHUNK < len(names):
                time.sleep(0.5)   # a chunk was downloaded: pause before the next one
        self.timings['bars_seconds'] += time.monotonic() - t_all

    def prepare(self, first_day, last_day=None):
        """Load the pool's bars for selections from first_day to last_day (once per run; reloads
        only when a later call reaches outside the loaded window)."""
        first = _day(first_day)
        last = _day(last_day) if last_day is not None else first
        if last < first:
            last = first
        if self._window is not None and self._window[0] <= first and last <= self._window[1]:
            return
        if self._window is not None:
            first, last = min(first, self._window[0]), max(last, self._window[1])
        fetch_start = self._fetch_start_for(first)
        self._load(self.pool.tickers, fetch_start, last)
        self._window = (first, last)
        self._fetch_start = fetch_start
        loaded = {t: s for t, s in self._series.items() if s is not None and t in self.pool}
        self.missing_pool = [t for t in self.pool.tickers if self._series.get(t) is None]
        if not loaded:
            raise not_supported("universe selection data",
                                "Yahoo Finance returned no daily bars for the candidate pool (network or "
                                "rate limit); try again later.")
        ref = self._series.get('SPY')
        days = set(ref.dates) if ref is not None else set()
        if not days:
            for s in loaded.values():
                days.update(s.dates)
        self._ref_days = sorted(days)
        self._coarse_day, self._coarse_list = None, []

    def series(self, ticker, around=None):
        """Bars of any US equity ticker (pool or not); None if Yahoo has none. Uses the pool's window
        when it is loaded, else a window from before `around` (default today) up to today."""
        t = str(ticker).upper()
        if t in self._series:
            return self._series[t]
        if self._window is not None:
            self._load([t], self._fetch_start, self._window[1])
        else:
            ref = _day(around) if around is not None else self.today
            self._load([t], self._fetch_start_for(ref), max(ref, self.today))
        return self._series.get(t)

    def trading_days(self):
        return list(self._ref_days)

    def previous_session(self, day):
        """The last pool session strictly before `day` (whose data is known at day 00:00)."""
        i = bisect.bisect_left(self._ref_days, _day(day)) - 1
        return self._ref_days[i] if i >= 0 else None

    def _make(self, t, s, i, session):
        close = s.close[i]
        f = s.factor[i] or 1.0
        vol = s.volume[i]
        raw = round(close * f, 4) if f != 1.0 else close
        return Fundamental._make(self.symbol(t), session, raw, int(round(vol / f)), close * vol,
                                 self.pool.has_fundamental_data(t), s.adj[i] / close, 1.0 / f, self._loader, close)

    def coarse(self, day):
        """Fundamental objects (coarse fields filled, fine fields lazy) of every pool security that
        traded in the last session before `day`. Only data complete before `day` is used.
        Call prepare(first_day, last_day) once for the run first (UniverseManager does); a day
        outside the loaded window reloads the pool."""
        day = _day(day)
        if self._window is None or not (self._window[0] <= day <= self._window[1]):
            last = max(day, self._window[1]) if self._window is not None else day
            self.prepare(day, last)
        session = self.previous_session(day)
        if session is None:
            return []
        if self._coarse_day != session:
            out = []
            for t in self.pool.tickers:
                s = self._series.get(t)
                if s is None:
                    continue
                i = s.index_of(session)
                if i is not None:
                    out.append(self._make(t, s, i, session))
            self._coarse_day, self._coarse_list = session, out
        self._used.add('pool')
        return list(self._coarse_list)

    def fundamental(self, symbol, as_of):
        """LEAN Fundamental.ForDate(as_of, symbol): coarse fields from the last bar complete at as_of,
        fine fields lazily from the snapshot (algorithm.fundamentals(), security.fundamentals)."""
        t = self.ticker_of(symbol)
        sym = symbol if isinstance(symbol, Symbol) else self.symbol(t)
        s = self.series(t, around=as_of)
        i = s.last_before(as_of) if s is not None else -1
        if t in self.pool:
            has_fd = self.pool.has_fundamental_data(t)
        else:
            self.prefetch_info([t])
            has_fd = (self._info.get(t) or {}).get('quoteType') == 'EQUITY'
        if i >= 0:
            obj = self._make(t, s, i, s.dates[i])
            obj.__dict__['symbol'] = sym
            obj.__dict__['has_fundamental_data'] = has_fd
        else:
            obj = Fundamental._make(sym, as_of, 0.0, 0, 0.0, has_fd, 1.0, 1.0, self._loader, None)
        obj.time = as_of
        obj.end_time = as_of
        return obj

    def adjusted_daily(self, ticker):
        """Split- and dividend-adjusted bars of a loaded ticker in yahoo_cache.fetch_daily_dicts format
        (lets the runner reuse the pool download), or None."""
        s = self._series.get(str(ticker).upper())
        if s is None:
            return None
        out = []
        for i, d in enumerate(s.dates):
            k = s.adj[i] / s.close[i]
            out.append({'time': d.strftime('%Y-%m-%d 00:00:00'), 'open': s.open[i] * k, 'high': s.high[i] * k,
                        'low': s.low[i] * k, 'close': s.adj[i], 'volume': s.volume[i]})
        return out

    # ---------------------------------------------------------------- fundamentals snapshot
    def _info_path(self, t):
        return self.cache_dir() / 'info' / f"{_safe_name(t)}.json"

    def _read_info_cache(self, t):
        from .yahoo_cache import read_json
        rec = read_json(self._info_path(t))
        if not isinstance(rec, dict) or 'fetched' not in rec or not isinstance(rec.get('info'), dict):
            return None
        return rec

    def _fresh(self, rec) -> bool:
        try:
            age = datetime.now() - datetime.fromisoformat(rec['fetched'])
        except (TypeError, ValueError):
            return False
        ttl = self.info_ttl if rec.get('info') else min(self.info_ttl, timedelta(days=1))
        return age < ttl

    def prefetch_info(self, tickers):
        """Load the snapshot of each ticker (memory, then disk cache, then Yahoo - a few at a time)."""
        todo, stale = [], {}
        for t in dict.fromkeys(str(x).upper() for x in tickers):
            if t in self._info:
                continue
            rec = self._read_info_cache(t)
            if rec is not None and self._fresh(rec):
                self._info[t] = rec['info']
                continue
            if rec is not None:
                stale[t] = rec
            todo.append(t)
        if not todo:
            return
        t0 = time.monotonic()
        results, errors = {}, {}

        def one(t):
            return t, self._fetch_info(yahoo_name(t))

        if len(todo) == 1 or self.info_workers == 1:
            for t in todo:
                try:
                    results[t] = one(t)[1]
                except Exception as e:  # noqa: BLE001 - reported in notes
                    errors[t] = e
        else:
            from concurrent.futures import ThreadPoolExecutor
            with ThreadPoolExecutor(max_workers=min(self.info_workers, len(todo))) as ex:
                futs = {ex.submit(one, t): t for t in todo}
                for fut, t in futs.items():
                    try:
                        results[t] = fut.result()[1]
                    except Exception as e:  # noqa: BLE001 - reported in notes
                        errors[t] = e
        from .yahoo_cache import atomic_write_json
        stamp = datetime.now().isoformat(timespec='seconds')
        for t, info in results.items():
            info = info or {}
            p = self._info_path(t)
            try:
                p.parent.mkdir(parents=True, exist_ok=True)
                atomic_write_json(p, {'v': 1, 'ticker': t, 'fetched': stamp, 'info': info})
            except OSError:
                pass
            self._info[t] = info
        for t, e in errors.items():
            if t in stale:
                self._info[t] = stale[t]['info']
                self._info_stale.append(t)
            else:
                self._info[t] = {}
                self._info_failed.append(t)
            _log(f"fundamentals of {t} unavailable: {e}")
        self.timings['info_seconds'] += time.monotonic() - t0
        self.timings['info_fetched'] += len(results)

    def info(self, ticker) -> dict:
        t = str(ticker).upper()
        if t not in self._info:
            self.prefetch_info([t])
        return self._info.get(t) or {}

    def _loader(self, f):
        """Fine-attribute values of Fundamental `f` (called on its first fine-attribute access)."""
        t = self.ticker_of(f.symbol)
        if t not in self._info:
            batch = f.__dict__.get('_batch') or [f]
            want = [self.ticker_of(x.symbol) for x in batch if getattr(x, 'has_fundamental_data', False)]
            self.prefetch_info(want + [t])
        self._used.add('fine')
        return map_fundamentals(self._info.get(t) or {}, f.__dict__.get('_ref_close'), t)

    # ---------------------------------------------------------------- ETF holdings
    def _holdings_path(self, etf):
        return self.cache_dir() / 'etf_holdings' / f"{_safe_name(etf)}.json"

    def _static_holdings(self):
        from .yahoo_cache import read_json
        data = read_json(self._static_holdings_file)
        return data if isinstance(data, dict) else {}

    def etf_holdings(self, etf):
        """{'holdings': [(ticker, name, weight)], 'source': ..., 'as_of': 'YYYY-MM-DD'} or None."""
        etf = str(etf).upper()
        if etf in self._holdings:
            return self._holdings[etf]
        from .yahoo_cache import read_json, atomic_write_json
        path = self._holdings_path(etf)
        rec = read_json(path)
        fresh = False
        if isinstance(rec, dict) and 'fetched' in rec:
            try:
                fresh = datetime.now() - datetime.fromisoformat(rec['fetched']) < self.holdings_ttl
            except (TypeError, ValueError):
                fresh = False
        out = None
        if fresh and rec.get('holdings'):
            out = {'holdings': [tuple(h) for h in rec['holdings']], 'source': 'Yahoo Finance top holdings',
                   'as_of': rec['fetched'][:10]}
        if out is None:
            rows = None
            try:
                rows = self._fetch_holdings(yahoo_name(etf))
            except Exception as e:  # noqa: BLE001 - falls back below
                _log(f"holdings of {etf} unavailable from Yahoo: {e}")
            if rows:
                rows = [(lean_name(t), n, float(w)) for t, n, w in rows]
                stamp = datetime.now().isoformat(timespec='seconds')
                try:
                    path.parent.mkdir(parents=True, exist_ok=True)
                    atomic_write_json(path, {'v': 1, 'etf': etf, 'fetched': stamp, 'holdings': rows})
                except OSError:
                    pass
                out = {'holdings': rows, 'source': 'Yahoo Finance top holdings', 'as_of': stamp[:10]}
            elif isinstance(rec, dict) and rec.get('holdings'):
                out = {'holdings': [tuple(h) for h in rec['holdings']], 'source': 'Yahoo Finance top holdings',
                       'as_of': rec['fetched'][:10]}
        if out is None:
            table = self._static_holdings()
            rows = (table.get('holdings') or {}).get(etf)
            if rows:
                out = {'holdings': [(lean_name(t), n, float(w)) for t, n, w in rows],
                       'source': 'static table of top holdings', 'as_of': table.get('as_of')}
        self._holdings[etf] = out
        return out

    def etf_constituents(self, etf_symbol, day):
        """ETFConstituentData of the ETF's current top holdings (time = the previous day)."""
        etf = self.ticker_of(etf_symbol)
        rec = self.etf_holdings(etf)
        if rec is None:
            raise not_supported(f"the constituents of the ETF {etf}",
                                "Yahoo Finance has no holdings data for it and it is not in the engine's static "
                                "holdings table.")
        t = _day(day) - timedelta(days=1)
        out = [ETFConstituentData(self.symbol(h), weight=w, time=t, name=n or None) for h, n, w in rec['holdings']]
        self._used.add('etf')
        self._etf_used[etf] = (rec['source'], rec['as_of'], len(out))
        return out

    # ---------------------------------------------------------------- disclosures
    def notes(self):
        out = []
        if 'pool' in self._used:
            out.append(pool_disclosure(self.pool))
            out.append(PRICE_DISCLOSURE)
            if self.missing_pool:
                out.append(f"{len(self.missing_pool)} pool ticker(s) had no Yahoo bars in the window and were "
                           f"never candidates: {', '.join(self.missing_pool[:8])}"
                           f"{'...' if len(self.missing_pool) > 8 else ''}.")
        if 'fine' in self._used:
            out.append(FUNDAMENTAL_DISCLOSURE)
        if self._etf_used:
            detail = '; '.join(f"{etf}: top {n} holdings, {src}, as of {asof}"
                               for etf, (src, asof, n) in self._etf_used.items())
            out.append(ETF_DISCLOSURE.format(detail=detail))
        if self._info_failed:
            out.append(f"Yahoo Finance fundamentals could not be loaded for {', '.join(self._info_failed[:8])}"
                       f"{'...' if len(self._info_failed) > 8 else ''}; their fine fields read as missing values.")
        if self._info_stale:
            out.append(f"Older cached Yahoo fundamentals were used for {', '.join(self._info_stale[:8])} "
                       f"(the refresh failed).")
        return out
