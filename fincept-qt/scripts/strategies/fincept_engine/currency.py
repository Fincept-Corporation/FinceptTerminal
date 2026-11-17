# ============================================================================
# Fincept Terminal - Strategy Engine: cash book conversion rates (session extension)
#
# Every currency balance other than the account currency needs a conversion rate. A pair
# the strategy trades gives it directly (EURUSD's price is EUR's rate in a USD account);
# otherwise the rate comes from Yahoo's daily FX or crypto bars (EURUSD=X, BTC-USD), loaded
# for the run. Before the open of a day the rate is the previous close; after the day's bar
# it is that close (no look-ahead). Stablecoins count 1:1 in a USD account.
# ============================================================================

from datetime import timedelta

from .cashbook import FIAT, STABLECOINS
from .enums import SecurityType
from .session import SessionExtension
from .types import Symbol


class CashBookRates(SessionExtension):

    def __init__(self):
        self._seen = set()     # security keys whose quote currency was checked
        self._source = {}      # currency -> BarSeries (or 'pair' / 'peg' / None)
        self._labels = {}      # currency -> label for the notes
        self._missing = set()

    def _pairs(self, session):
        """currencies whose rate a subscribed pair already provides."""
        acct = session.algo.portfolio.cash_book.account_currency
        out = set()
        from .cashbook import split_pair
        for k, sec in session.algo.securities.items():
            if sec.symbol.security_type in (SecurityType.FOREX, SecurityType.CRYPTO) and k in session.series:
                base, quote = split_pair(k)
                q = session.algo.portfolio.quote_currency(k)
                if q == acct and base:
                    out.add(base)
                elif base == acct and q:
                    out.add(q)
        return out

    def _resolve(self, session):
        pf = session.algo.portfolio
        book = pf.cash_book
        acct = book.account_currency
        # every quote currency of a subscribed security needs a rate
        secs = session.algo.securities._securities
        new = [k for k in secs if k not in self._seen]
        if not new and all(code == acct or code in self._source for code in book.keys()):
            return
        for k in new:
            self._seen.add(k)
            q = pf.quote_currency(k)
            if q != acct:
                book.ensure(q)
        if all(code == acct or code in self._source for code in book.keys()):
            return
        provided = self._pairs(session)
        for code in list(book.keys()):
            if code == acct or code in self._source:
                continue
            if code in provided:
                self._source[code] = 'pair'
                continue
            if code in STABLECOINS and acct == 'USD':
                self._source[code] = 'peg'
                book[code].conversion_rate = 1.0
                continue
            # Yahoo spellings (keys that cannot collide with a strategy's own symbols)
            if code in FIAT and acct in FIAT:
                candidates = [(f"{code}{acct}=X", SecurityType.FOREX, 'direct'),
                              (f"{acct}{code}=X", SecurityType.FOREX, 'inverse')]
            else:
                candidates = [(f"{code}-{acct}", SecurityType.CRYPTO, 'direct'),
                              (f"{acct}-{code}", SecurityType.CRYPTO, 'inverse')]
            found = None
            if code not in FIAT and acct not in FIAT:
                candidates = []      # crypto crosses: through USD (two legs), as LEAN's conversion does
            for y, st, kind in candidates:
                s = session.data.series(Symbol(y, st, 'usa'), st, 'usa')
                if s is not None:
                    found = (kind, s, y)
                    break
            if found is None and acct != 'USD' and code != 'USD':
                # two legs through USD: rate = (code in USD) / (account currency in USD)
                legs = []
                for c in (code, acct):
                    y = f"{c}USD=X" if c in FIAT else f"{c}-USD"
                    st = SecurityType.FOREX if c in FIAT else SecurityType.CRYPTO
                    legs.append((y, session.data.series(Symbol(y, st, 'usa'), st, 'usa')))
                if all(s is not None for _y, s in legs):
                    self._source[code] = ('two_leg', legs[0][1], legs[1][1])
                    self._labels[code] = f"{legs[0][0]} / {legs[1][0]}"
                    self._conversion(book[code], [legs[0][0], legs[1][0]], code, acct)
                    continue
            if found is None:
                self._source[code] = None
                self._missing.add(code)
            else:
                kind, s, y = found
                self._source[code] = (kind, s)
                self._labels[code] = y + (' (inverted)' if kind == 'inverse' else '')
                self._conversion(book[code], [y], code, acct)

    @staticmethod
    def _conversion(cash, yahoo_names, source, dest):
        """LEAN's Cash.currency_conversion: the securities its rate comes from."""
        from types import SimpleNamespace
        secs = []
        for y in yahoo_names:
            t = y.replace('=X', '').replace('-', '')
            st = SecurityType.CRYPTO if '-' in y else SecurityType.FOREX
            secs.append(SimpleNamespace(symbol=Symbol(t, st, 'usa')))
        cash.currency_conversion = SimpleNamespace(conversion_rate_securities=secs, source_currency=source,
                                                   destination_currency=dest)

    def _apply(self, session, t):
        book = session.algo.portfolio.cash_book
        for code, src in self._source.items():
            if not isinstance(src, tuple) or code not in book:
                continue
            if src[0] == 'two_leg':
                a, b = src[1], src[2]
                na, nb = a.count_known_at(t), b.count_known_at(t)
                if na > 0 and nb > 0 and b.close[nb - 1]:
                    book[code].conversion_rate = a.close[na - 1] / b.close[nb - 1]
                continue
            kind, s = src
            n = s.count_known_at(t)
            if n <= 0:
                continue
            px = s.close[n - 1]
            book[code].conversion_rate = (1.0 / px) if kind == 'inverse' and px else px

    def after_initialize(self, session):
        self._resolve(session)
        self._apply(session, session.start)

    def before_open(self, session, day, warming):
        self._resolve(session)
        self._apply(session, day)
        return [], []

    def decorate_slice(self, session, sl, day):
        self._apply(session, sl.time)

    def notes(self, session):
        out = []
        if self._labels:
            acct = session.algo.portfolio.cash_book.account_currency
            out.append("Cash book: balances in " + ", ".join(sorted(self._labels)) + f" are valued in {acct} at "
                       "Yahoo daily closes (" + ", ".join(self._labels[c] for c in sorted(self._labels)) + ").")
        if self._missing:
            out.append("No conversion rate source for " + ", ".join(sorted(self._missing)) +
                       "; those balances count as 0 in the portfolio value.")
        pegged = [c for c, s in self._source.items() if s == 'peg']
        if pegged:
            out.append(f"{', '.join(sorted(pegged))} counted 1:1 with USD (stablecoin peg).")
        return out
