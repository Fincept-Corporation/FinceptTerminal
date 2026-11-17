# ============================================================================
# Fincept Terminal - Strategy Engine: slice chain collections
# Empty option / future chain collections for slices of runs without derivatives, so
# `slice.option_chains.get(symbol)` answers None instead of failing.
# ============================================================================

from .types import SymbolDict


class FuturesChains(SymbolDict):
    """canonical future Symbol -> FuturesChain."""

    def get_value(self, key, default=None):
        return self.get(key, default)

    @property
    def data_frame(self):
        import pandas as pd
        rows, idx = [], []
        for canonical, chain in dict.items(self):
            for c in chain:
                rows.append({'expiry': c.expiry, 'lastprice': c.last_price, 'bidprice': c.bid_price,
                             'askprice': c.ask_price, 'openinterest': c.open_interest, 'volume': c.volume})
                idx.append((canonical, c.symbol))
        if not rows:
            return pd.DataFrame()
        return pd.DataFrame(rows, index=pd.MultiIndex.from_tuples(idx, names=['canonical', 'symbol']))

    GetValue = get_value


def empty_option_chains():
    try:
        from .option_chain import OptionChains
        return OptionChains()
    except Exception:  # option module unavailable: a plain empty mapping
        class _Empty(SymbolDict):
            def get_value(self, key, default=None):
                return self.get(key, default)
        return _Empty()


def empty_future_chains():
    return FuturesChains()
