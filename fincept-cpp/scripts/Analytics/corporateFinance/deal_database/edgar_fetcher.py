"""SEC EDGAR Fetcher - All HTTP operations, CIK resolution, XBRL financial data.

Zero external dependencies — uses stdlib urllib only.
Designed for M&A deal enrichment in Fincept Terminal.
"""
import json
import re
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

_HEADERS = {
    "User-Agent": "Fincept Terminal contact@fincept.in",
    "Accept": "application/json,text/html,text/plain",
    "Accept-Encoding": "gzip, deflate",
}

# XBRL tag fallback lists — companies use many different tag names for the same concept
REVENUE_TAGS = [
    "RevenueFromContractWithCustomerExcludingAssessedTax",
    "RevenueFromContractWithCustomerIncludingAssessedTax",
    "Revenues",
    "RevenueNet",
    "SalesRevenueNet",
    "SalesRevenueGoodsNet",
    "RevenuesNetOfInterestExpense",        # banks
    "InterestAndDividendIncomeOperating",  # financial companies
    "HealthCareOrganizationRevenue",       # healthcare
    "RealEstateRevenueNet",                # REITs
    "OilAndGasRevenue",                    # energy
    "ElectricUtilityRevenue",              # utilities
    "GrossProfit",                         # last resort
]

NET_INCOME_TAGS = [
    "NetIncomeLoss",
    "NetIncomeLossAvailableToCommonStockholdersBasic",
    "ProfitLoss",
    "IncomeLossFromContinuingOperations",
    "NetIncome",
]

INTEREST_EXPENSE_TAGS = [
    "InterestExpense",
    "InterestExpenseDebt",
    "InterestAndDebtExpense",
    "FinancingInterestExpense",
]

DA_TAGS = [
    "DepreciationDepletionAndAmortization",
    "Depreciation",
    "DepreciationAndAmortization",
    "AmortizationOfIntangibleAssets",
    "DepreciationAmortizationAndAccretionNet",
]

INCOME_TAX_TAGS = [
    "IncomeTaxExpenseBenefit",
    "CurrentIncomeTaxExpenseBenefit",
]

TOTAL_DEBT_TAGS = [
    "LongTermDebt",
    "LongTermDebtNoncurrent",
    "DebtLongtermAndShorttermCombinedAmount",
    "LongTermDebtAndCapitalLeaseObligations",
    "LongTermNotesPayable",
    "NotesPayable",
    "ConvertibleNotesPayable",
]

CASH_TAGS = [
    "CashAndCashEquivalentsAtCarryingValue",
    "CashCashEquivalentsAndShortTermInvestments",
    "CashCashEquivalentsRestrictedCashAndRestrictedCashEquivalents",
    "Cash",
]

SHARES_TAGS = [
    "CommonStockSharesOutstanding",
    "CommonStockSharesIssued",
    "EntityCommonStockSharesOutstanding",
]

TOTAL_ASSETS_TAGS = [
    "Assets",
    "AssetsCurrent",
]

EBIT_TAGS = [
    "OperatingIncomeLoss",
    "IncomeLossFromContinuingOperationsBeforeIncomeTaxesExtraordinaryItemsNoncontrollingInterest",
]


class EdgarFetcher:
    """All SEC EDGAR HTTP operations for M&A deal enrichment."""

    def __init__(self):
        # In-memory cache: cik -> company-facts dict (large JSON, avoid re-fetching)
        self._facts_cache: Dict[str, Optional[dict]] = {}
        # Ticker -> CIK cache
        self._ticker_cik_cache: Dict[str, str] = {}
        self._last_request_time: float = 0.0

    # -----------------------------------------------------------------------
    # Core HTTP helpers
    # -----------------------------------------------------------------------

    def _rate_limit(self) -> None:
        """Ensure minimum 0.15 s between requests (SEC allows ~10 req/sec)."""
        elapsed = time.time() - self._last_request_time
        if elapsed < 0.15:
            time.sleep(0.15 - elapsed)
        self._last_request_time = time.time()

    def _get(self, url: str, timeout: int = 15, retries: int = 3) -> Optional[dict]:
        """Fetch JSON from URL with retry on 429/503."""
        for attempt in range(retries):
            self._rate_limit()
            try:
                req = Request(url, headers=_HEADERS)
                with urlopen(req, timeout=timeout) as resp:
                    return json.loads(resp.read().decode("utf-8"))
            except HTTPError as e:
                if e.code in (429, 503) and attempt < retries - 1:
                    time.sleep(2 ** attempt)
                    continue
                return None
            except Exception:
                return None
        return None

    def _get_text(self, url: str, timeout: int = 20, retries: int = 3) -> str:
        """Fetch plain/HTML text from URL with retry on 429/503."""
        headers = {**_HEADERS, "Accept": "text/html,text/plain"}
        for attempt in range(retries):
            self._rate_limit()
            try:
                req = Request(url, headers=headers)
                with urlopen(req, timeout=timeout) as resp:
                    raw = resp.read()
                    try:
                        return raw.decode("utf-8")[:120000]
                    except UnicodeDecodeError:
                        return raw.decode("latin-1")[:120000]
            except HTTPError as e:
                if e.code in (429, 503) and attempt < retries - 1:
                    time.sleep(2 ** attempt)
                    continue
                return ""
            except Exception:
                return ""
        return ""

    # -----------------------------------------------------------------------
    # Company resolution
    # -----------------------------------------------------------------------

    def resolve_cik(self, company_name_or_ticker: str) -> Optional[str]:
        """Resolve company name or ticker to a 10-digit zero-padded CIK string.

        Strategy:
          1. If it looks like a ticker (1-5 uppercase letters), use the ticker JSON file.
          2. Otherwise search the EFTS full-text search endpoint.
          3. Fallback: search the ticker JSON by partial name match.
        """
        if not company_name_or_ticker:
            return None

        query = company_name_or_ticker.strip()

        # Check ticker cache first
        ticker_upper = query.upper()
        if ticker_upper in self._ticker_cik_cache:
            return self._ticker_cik_cache[ticker_upper]

        # 1. Try as ticker
        if re.match(r'^[A-Z]{1,5}$', ticker_upper):
            cik = self._cik_from_ticker_file(ticker_upper)
            if cik:
                self._ticker_cik_cache[ticker_upper] = cik
                return cik

        # 2. Try EFTS entity search
        cik = self._cik_from_efts_search(query)
        if cik:
            self._ticker_cik_cache[ticker_upper] = cik
            return cik

        # 3. Fuzzy match in ticker file by company name
        cik = self._cik_from_company_name_fuzzy(query)
        if cik:
            self._ticker_cik_cache[ticker_upper] = cik
        return cik

    def _cik_from_ticker_file(self, ticker: str) -> Optional[str]:
        """Look up CIK from SEC company_tickers.json (fast, cached file)."""
        data = self._get("https://www.sec.gov/files/company_tickers.json")
        if not data:
            return None
        for entry in data.values():
            if str(entry.get("ticker", "")).upper() == ticker:
                return str(entry["cik_str"]).zfill(10)
        return None

    def _cik_from_efts_search(self, query: str) -> Optional[str]:
        """Search SEC EFTS for company by name, return CIK."""
        import urllib.parse
        encoded = urllib.parse.quote(query)
        url = f"https://efts.sec.gov/LATEST/search-index?q=%22{encoded}%22&dateRange=custom&startdt=2000-01-01&forms=10-K"
        data = self._get(url)
        if data and data.get("hits", {}).get("hits"):
            hit = data["hits"]["hits"][0]
            entity_id = hit.get("_source", {}).get("entity_id") or hit.get("_source", {}).get("cik")
            if entity_id:
                return str(entity_id).zfill(10)

        # Try the submissions search endpoint
        url2 = f"https://efts.sec.gov/LATEST/search-index?q={encoded}&forms=DEFM14A,PREM14A,8-K"
        data2 = self._get(url2)
        if data2 and data2.get("hits", {}).get("hits"):
            hit = data2["hits"]["hits"][0]
            src = hit.get("_source", {})
            if src.get("entity_id"):
                return str(src["entity_id"]).zfill(10)

        return None

    def _cik_from_company_name_fuzzy(self, query: str) -> Optional[str]:
        """Fuzzy match company name against the SEC ticker list."""
        data = self._get("https://www.sec.gov/files/company_tickers.json")
        if not data:
            return None
        query_lower = query.lower()
        # Remove common legal suffixes for matching
        query_clean = re.sub(
            r'\s*(,\s*)?(inc\.?|corp\.?|llc\.?|ltd\.?|co\.?|company|corporation|holdings?)\s*$',
            '', query_lower, flags=re.IGNORECASE
        ).strip()
        for entry in data.values():
            name = str(entry.get("title", "")).lower()
            name_clean = re.sub(
                r'\s*(,\s*)?(inc\.?|corp\.?|llc\.?|ltd\.?|co\.?|company|corporation|holdings?)\s*$',
                '', name, flags=re.IGNORECASE
            ).strip()
            if query_clean and name_clean and (
                query_clean == name_clean
                or query_clean in name_clean
                or name_clean in query_clean
            ):
                return str(entry["cik_str"]).zfill(10)
        return None

    def get_company_metadata(self, cik: str) -> Dict[str, Any]:
        """Fetch company metadata from SEC submissions API.

        Returns: {name, tickers, sic, sic_description, country, state_of_incorporation, fiscal_year_end}
        """
        padded = str(cik).zfill(10)
        url = f"https://data.sec.gov/submissions/CIK{padded}.json"
        data = self._get(url)
        if not data:
            return {}
        tickers = data.get("tickers", [])
        return {
            "name": data.get("name", ""),
            "ticker": tickers[0] if tickers else "",
            "tickers": tickers,
            "sic": data.get("sic", ""),
            "sic_description": data.get("sicDescription", ""),
            "country": data.get("insiderTransactionForOwnerExists", ""),  # not ideal field
            "state_of_incorporation": data.get("stateOfIncorporation", ""),
            "fiscal_year_end": data.get("fiscalYearEnd", ""),
            "entity_type": data.get("entityType", ""),
            "cik": padded,
        }

    # -----------------------------------------------------------------------
    # Filing text fetching (moved from deal_parser)
    # -----------------------------------------------------------------------

    def get_filing_text(self, accession_number: str, cik: str) -> str:
        """Fetch primary document text for a filing from SEC EDGAR Archives.

        Strategy:
          1. Fetch the filing -index.htm page, parse document links.
          2. Download the primary document (non-exhibit), strip HTML.
          3. Fallback: directory listing.
        Returns up to 100 000 chars of plain text.
        """
        acc_clean = accession_number.replace("-", "")
        cik_int = str(int(cik)) if cik.isdigit() else cik.lstrip("0") or "0"
        base_url = f"https://www.sec.gov/Archives/edgar/data/{cik_int}/{acc_clean}"

        # 1. Fetch the index HTML page
        index_url = f"{base_url}/{accession_number}-index.htm"
        index_html = self._get_text(index_url)

        if index_html:
            doc_links = re.findall(
                r'href="(/Archives/edgar/data/[^"]+\.(?:htm|html|txt))"',
                index_html, re.IGNORECASE
            )
            rel_links = re.findall(
                r'href="([^/"][^"]*\.(?:htm|html|txt))"',
                index_html, re.IGNORECASE
            )

            candidates: List[str] = []
            for link in doc_links:
                candidates.append(f"https://www.sec.gov{link}")
            for link in rel_links:
                if not link.startswith("http") and not link.startswith("/"):
                    candidates.append(f"{base_url}/{link}")

            candidates = [u for u in candidates if "-index" not in u.lower()]

            # Sort: exhibits last
            primary_first = sorted(candidates, key=lambda u: (
                1 if any(x in u.lower() for x in ["ex-", "exhibit", "ex99", "ex10"]) else 0
            ))

            for doc_url in primary_first[:5]:
                text = self._get_text(doc_url)
                if text and len(text) > 1000:
                    return re.sub(r"<[^>]+>", " ", text)[:100000]

        # 2. Fallback: directory listing
        dir_html = self._get_text(f"{base_url}/")
        if dir_html:
            links = re.findall(r'href="([^"]+\.(?:htm|txt))"', dir_html, re.IGNORECASE)
            for link in links[:5]:
                full = (
                    link if link.startswith("http")
                    else (
                        f"https://www.sec.gov{link}" if link.startswith("/")
                        else f"{base_url}/{link}"
                    )
                )
                if "-index" not in full.lower():
                    text = self._get_text(full)
                    if text and len(text) > 1000:
                        return re.sub(r"<[^>]+>", " ", text)[:100000]

        return ""

    def get_filing_documents(self, accession_number: str, cik: str) -> List[Dict[str, Any]]:
        """Return list of documents in a filing from -index.json.

        Each item: {filename, type, description, url}
        """
        acc_clean = accession_number.replace("-", "")
        cik_int = str(int(cik)) if cik.isdigit() else cik.lstrip("0") or "0"
        url = f"https://www.sec.gov/Archives/edgar/data/{cik_int}/{acc_clean}/{accession_number}-index.json"
        data = self._get(url)
        if not data:
            return []

        base = f"https://www.sec.gov/Archives/edgar/data/{cik_int}/{acc_clean}/"
        docs = []
        for item in data.get("directory", {}).get("item", []):
            name = item.get("name", "")
            docs.append({
                "filename": name,
                "type": item.get("type", ""),
                "description": item.get("name", ""),
                "url": base + name,
            })
        return docs

    # -----------------------------------------------------------------------
    # XBRL Financial data
    # -----------------------------------------------------------------------

    def get_company_financials(self, cik: str, periods: int = 4) -> Dict[str, Any]:
        """Fetch M&A-relevant financials from SEC XBRL company-facts API.

        Returns:
          {
            'annual': {revenue, ebitda, net_income, total_debt, cash, shares_outstanding, total_assets},
            'ttm': {...},   # trailing-12-month aggregate of last 4 quarters
            'period_end': '2024-09-30',
            'currency': 'USD'
          }
        If XBRL fetch fails, returns empty dict (XBRL enrichment is optional).
        """
        padded = str(cik).zfill(10)

        # Use cached facts if available
        if padded in self._facts_cache:
            facts = self._facts_cache[padded]
        else:
            url = f"https://data.sec.gov/api/xbrl/companyfacts/CIK{padded}.json"
            facts = self._get(url, timeout=30)
            self._facts_cache[padded] = facts

        if not facts:
            return {}

        us_gaap = facts.get("facts", {}).get("us-gaap", {})
        if not us_gaap:
            return {}

        # Annual (12-month duration periods)
        revenue = self._get_xbrl_value(us_gaap, REVENUE_TAGS, "annual")
        net_income = self._get_xbrl_value(us_gaap, NET_INCOME_TAGS, "annual")
        interest = self._get_xbrl_value(us_gaap, INTEREST_EXPENSE_TAGS, "annual")
        da = self._get_xbrl_value(us_gaap, DA_TAGS, "annual")
        tax = self._get_xbrl_value(us_gaap, INCOME_TAX_TAGS, "annual")
        ebit = self._get_xbrl_value(us_gaap, EBIT_TAGS, "annual")

        # Balance sheet (instantaneous / point-in-time)
        total_debt = self._get_xbrl_value(us_gaap, TOTAL_DEBT_TAGS, "instant")
        cash = self._get_xbrl_value(us_gaap, CASH_TAGS, "instant")
        shares = self._get_xbrl_value(us_gaap, SHARES_TAGS, "instant")
        total_assets = self._get_xbrl_value(us_gaap, TOTAL_ASSETS_TAGS, "instant")

        # Derive EBITDA
        ebitda = None
        if net_income is not None and interest is not None and da is not None and tax is not None:
            ebitda = self._derive_ebitda(net_income, tax, interest, da)
        elif ebit is not None and da is not None:
            ebitda = ebit + da

        # TTM from last 4 quarters
        ttm_revenue = self._get_xbrl_ttm(us_gaap, REVENUE_TAGS)
        ttm_net_income = self._get_xbrl_ttm(us_gaap, NET_INCOME_TAGS)

        period_end = self._get_latest_period_end(us_gaap, REVENUE_TAGS + NET_INCOME_TAGS)

        annual = {}
        if revenue is not None:
            annual["revenue"] = revenue
        if ebitda is not None:
            annual["ebitda"] = ebitda
        if net_income is not None:
            annual["net_income"] = net_income
        if total_debt is not None:
            annual["total_debt"] = total_debt
        if cash is not None:
            annual["cash"] = cash
        if shares is not None:
            annual["shares_outstanding"] = shares
        if total_assets is not None:
            annual["total_assets"] = total_assets
        if interest is not None:
            annual["interest_expense"] = interest
        if da is not None:
            annual["depreciation_amortization"] = da

        ttm = {}
        if ttm_revenue is not None:
            ttm["revenue"] = ttm_revenue
        if ttm_net_income is not None:
            ttm["net_income"] = ttm_net_income

        return {
            "annual": annual,
            "ttm": ttm,
            "period_end": period_end or "",
            "currency": "USD",
            "cik": padded,
        }

    def _get_xbrl_value(
        self, us_gaap: dict, tags: List[str], period_type: str
    ) -> Optional[float]:
        """Try multiple XBRL tags until one returns a value for the requested period type.

        period_type:
          'annual'  → duration units with months=12
          'instant' → point-in-time (balance sheet items)
        """
        for tag in tags:
            tag_data = us_gaap.get(tag, {})
            if not tag_data:
                continue
            units = tag_data.get("units", {})
            # Try USD first, then pure numbers (shares)
            for unit_key in ("USD", "shares", "pure"):
                entries = units.get(unit_key, [])
                if not entries:
                    continue
                if period_type == "annual":
                    # Want 12-month duration entries (form 10-K annual period)
                    annual_entries = [
                        e for e in entries
                        if e.get("form") in ("10-K", "10-K/A", "20-F", "20-F/A")
                        and e.get("start") and e.get("end")
                        and self._months_between(e["start"], e["end"]) in range(11, 14)
                    ]
                    if annual_entries:
                        latest = max(annual_entries, key=lambda e: e.get("end", ""))
                        val = latest.get("val")
                        if val is not None:
                            return float(val)
                elif period_type == "instant":
                    instant_entries = [
                        e for e in entries
                        if not e.get("start") and e.get("end")
                        and e.get("form") in ("10-K", "10-K/A", "10-Q", "10-Q/A", "20-F")
                    ]
                    if instant_entries:
                        latest = max(instant_entries, key=lambda e: e.get("end", ""))
                        val = latest.get("val")
                        if val is not None:
                            return float(val)
        return None

    def _get_xbrl_ttm(self, us_gaap: dict, tags: List[str]) -> Optional[float]:
        """Aggregate last 4 quarterly values for a trailing-12-month figure."""
        for tag in tags:
            tag_data = us_gaap.get(tag, {})
            if not tag_data:
                continue
            units = tag_data.get("units", {})
            entries = units.get("USD", [])
            quarterly = [
                e for e in entries
                if e.get("form") in ("10-Q", "10-Q/A")
                and e.get("start") and e.get("end")
                and self._months_between(e["start"], e["end"]) in range(2, 5)
            ]
            if len(quarterly) >= 4:
                quarterly.sort(key=lambda e: e.get("end", ""), reverse=True)
                ttm = sum(e.get("val", 0) or 0 for e in quarterly[:4])
                return float(ttm)
        return None

    def _get_latest_period_end(self, us_gaap: dict, tags: List[str]) -> Optional[str]:
        """Return the latest period end date among a set of XBRL tags."""
        latest = ""
        for tag in tags:
            tag_data = us_gaap.get(tag, {})
            if not tag_data:
                continue
            for unit_entries in tag_data.get("units", {}).values():
                for e in unit_entries:
                    end = e.get("end", "")
                    if end > latest:
                        latest = end
        return latest or None

    @staticmethod
    def _months_between(start: str, end: str) -> int:
        """Approximate months between two YYYY-MM-DD date strings."""
        try:
            sy, sm = int(start[:4]), int(start[5:7])
            ey, em = int(end[:4]), int(end[5:7])
            return (ey - sy) * 12 + (em - sm)
        except Exception:
            return 0

    @staticmethod
    def _derive_ebitda(
        net_income: float, tax: float, interest: float, da: float
    ) -> float:
        """EBITDA = Net Income + Tax Expense + Interest Expense + D&A."""
        return net_income + tax + interest + da

    # -----------------------------------------------------------------------
    # Valuation multiples
    # -----------------------------------------------------------------------

    def calculate_deal_multiples(
        self,
        deal_value: float,
        target_cik: str,
        announcement_date: str = "",
    ) -> Dict[str, Any]:
        """Calculate EV/Revenue, EV/EBITDA, implied P/E and price-per-share.

        Args:
            deal_value: Total deal value in USD (enterprise value proxy).
            target_cik: CIK of the target company.
            announcement_date: Deal announcement date (YYYY-MM-DD) — informational only.

        Returns dict with multiples. All fields are optional (None if data unavailable).
        """
        financials = self.get_company_financials(target_cik)
        if not financials:
            return {}

        annual = financials.get("annual", {})
        ttm = financials.get("ttm", {})

        revenue = ttm.get("revenue") or annual.get("revenue")
        ebitda = annual.get("ebitda")
        net_income = ttm.get("net_income") or annual.get("net_income")
        total_debt = annual.get("total_debt", 0) or 0
        cash = annual.get("cash", 0) or 0
        shares = annual.get("shares_outstanding")

        result: Dict[str, Any] = {}

        if revenue and revenue > 0:
            result["ev_revenue"] = round(deal_value / revenue, 2)

        if ebitda and ebitda > 0:
            result["ev_ebitda"] = round(deal_value / ebitda, 2)

        # Implied equity value = EV - debt + cash
        implied_equity = deal_value - total_debt + cash
        result["implied_equity_value"] = implied_equity

        if net_income and net_income > 0:
            result["implied_pe"] = round(implied_equity / net_income, 2)

        if shares and shares > 0:
            result["price_per_share"] = round(implied_equity / shares, 2)

        result["period_end"] = financials.get("period_end", "")
        result["currency"] = "USD"
        return result

    # -----------------------------------------------------------------------
    # Deal document discovery
    # -----------------------------------------------------------------------

    def find_merger_agreement(self, accession_number: str, cik: str) -> Optional[str]:
        """Find the merger agreement exhibit URL (EX-2.1 or first large exhibit)."""
        docs = self.get_filing_documents(accession_number, cik)
        # Look for EX-2.1 first (the Agreement and Plan of Merger exhibit)
        for doc in docs:
            desc = (doc.get("description", "") + doc.get("type", "") + doc.get("filename", "")).lower()
            if "ex-2" in desc or "ex2" in desc or "merger agreement" in desc:
                return doc.get("url")
        # Fallback: first non-index .htm file that isn't a small exhibit
        for doc in docs:
            fname = doc.get("filename", "").lower()
            if fname.endswith((".htm", ".html")) and "index" not in fname:
                return doc.get("url")
        return None

    def search_related_filings(
        self,
        company_name: str,
        cik: str,
        filing_types: Optional[List[str]] = None,
        months_back: int = 12,
    ) -> List[Dict[str, Any]]:
        """Find related filings (S-4, SC TO-T, proxy amendments) for a deal.

        Returns list of {accession_number, filing_type, filing_date, url}.
        """
        if filing_types is None:
            filing_types = ["S-4", "SC TO-T", "DEFM14A", "PREM14A", "8-K"]

        padded = str(cik).zfill(10)
        url = f"https://data.sec.gov/submissions/CIK{padded}.json"
        data = self._get(url)
        if not data:
            return []

        recent = data.get("filings", {}).get("recent", {})
        forms = recent.get("form", [])
        dates = recent.get("filingDate", [])
        accessions = recent.get("accessionNumber", [])

        from datetime import datetime, timedelta
        cutoff = (datetime.now() - timedelta(days=months_back * 30)).strftime("%Y-%m-%d")

        results = []
        for form, date, acc in zip(forms, dates, accessions):
            if form in filing_types and date >= cutoff:
                acc_clean = acc.replace("-", "")
                cik_int = str(int(padded))
                results.append({
                    "accession_number": acc,
                    "filing_type": form,
                    "filing_date": date,
                    "url": f"https://www.sec.gov/Archives/edgar/data/{cik_int}/{acc_clean}/{acc}-index.htm",
                })
        return results
