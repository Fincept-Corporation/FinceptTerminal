class Cache:
    def __init__(self):
        self._prices_cache: dict[str, list[dict]] = {}
        self._financial_metrics_cache: dict[str, list[dict]] = {}
        self._line_items_cache: dict[str, list[dict]] = {}
        self._insider_trades_cache: dict[str, list[dict]] = {}
        self._company_news_cache: dict[str, list[dict]] = {}

    def _merge_data(self, existing: list[dict] | None, new_data: list[dict], key_field: str) -> list[dict]:
        if not existing:
            return new_data
        existing_keys = {item[key_field] for item in existing}
        merged = existing.copy()
        merged.extend(x for x in new_data if x[key_field] not in existing_keys)
        return merged

    def get_prices(self, ticker: str) -> list[dict] | None:
        return self._prices_cache.get(ticker)

    def set_prices(self, ticker: str, data: list[dict]):
        self._prices_cache[ticker] = self._merge_data(self._prices_cache.get(ticker), data, key_field="time")

    def get_financial_metrics(self, ticker: str) -> list[dict] | None:
        return self._financial_metrics_cache.get(ticker)

    def set_financial_metrics(self, ticker: str, data: list[dict]):
        self._financial_metrics_cache[ticker] = self._merge_data(
            self._financial_metrics_cache.get(ticker),
            data,
            key_field="report_period",
        )

    def get_line_items(self, ticker: str) -> list[dict] | None:
        return self._line_items_cache.get(ticker)

    def set_line_items(self, ticker: str, data: list[dict]):
        self._line_items_cache[ticker] = self._merge_data(
            self._line_items_cache.get(ticker),
            data,
            key_field="report_period",
        )

    def get_insider_trades(self, ticker: str) -> list[dict] | None:
        return self._insider_trades_cache.get(ticker)

    def set_insider_trades(self, ticker: str, data: list[dict]):
        self._insider_trades_cache[ticker] = self._merge_data(
            self._insider_trades_cache.get(ticker),
            data,
            key_field="filing_date",
        )

    def get_company_news(self, ticker: str) -> list[dict] | None:
        return self._company_news_cache.get(ticker)

    def set_company_news(self, ticker: str, data: list[dict]):
        self._company_news_cache[ticker] = self._merge_data(
            self._company_news_cache.get(ticker),
            data,
            key_field="date",
        )

_cache = Cache()

def get_cache() -> Cache:
    return _cache
