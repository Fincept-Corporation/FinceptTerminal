"""M&A Deal Parser - Extract structured data from SEC filings

Every deal this parser writes is AUTO-EXTRACTED: it is stored with verified = 0 and the Deals screen shows it as
"unverified" until the user checks it against the filing and saves or marks it verified. A deal the user verified
is never overwritten by a later parse.

Quality guards (a filing that fails one is NOT stored as a deal; the filing itself stays in the filing index):
  * both parties must be named - no "Unknown Acquirer" / "Unknown Target";
  * a party name must look like a company name, not a sentence fragment ("Stock Option. Each Company",
    "Inc. On October 30, 2026, Broadwind, Inc");
  * acquirer and target must differ - an issuer tender offer or a fund buying back its own shares is not M&A.
Amounts, percentages and flags are only filled when the filing states them; nothing is defaulted
(no "50/50 cash and stock" when the split is not stated).

CLI: python deal_parser.py parse <accession_number|EDGAR URL> <form_type>   parse one filing the scan stored
     (deal_tracker.py scan) from that filing's own primary document on EDGAR; form_type fills a stored row
     that has none.  parse_batch <accession_numbers_json> [max_workers].
"""
import sys
from pathlib import Path
from typing import Dict, Any, Optional, List, Tuple
import re
from datetime import datetime
import json

scripts_path = Path(__file__).parent.parent.parent.parent
sys.path.append(str(scripts_path))

# Add Analytics path for absolute imports
analytics_path = Path(__file__).parent.parent.parent
sys.path.insert(0, str(analytics_path))

from corporateFinance.deal_database.database_schema import MADatabase
from corporateFinance.deal_database import deal_scanner as _sec


def _primary_document_text(submission: str) -> str:
    """Plain text of the first <DOCUMENT> (the form itself) of an EDGAR complete-submission file."""
    import html
    doc = re.search(r"<DOCUMENT>(.*?)(?:</DOCUMENT>|$)", submission, re.S | re.I)
    body = doc.group(1) if doc else submission
    text = re.search(r"<TEXT>(.*?)(?:</TEXT>|$)", body, re.S | re.I)
    body = text.group(1) if text else body
    body = re.sub(r"(?is)<(script|style)[^>]*>.*?</\1>", " ", body)
    body = html.unescape(re.sub(r"<[^>]+>", " ", body))
    return re.sub(r"\s+", " ", body).strip()


class _EdgarAdapter:
    """EDGAR access for the parser: plain HTTPS through deal_scanner's rate-limited client (declared User-Agent,
    under 10 requests/s). No edgartools: importing it cost ~5 s per run, and with EDGAR_IDENTITY unset (the
    terminal never sets it) it refuses every request (IdentityNotSetError), so the CIK lookups and XBRL enrichment
    that used to go through it never returned anything."""

    def resolve_company(self, name: str) -> Optional[Dict[str, str]]:
        return _sec.lookup_company(name)

    def resolve_cik(self, name: str):
        hit = _sec.lookup_company(name)
        return hit.get("cik") if hit else None

    def get_filing_text(self, accession_number: str, cik: str) -> str:
        """Text of THIS filing: the primary document of the accession's complete-submission file on EDGAR."""
        acc = accession_number.strip()
        cik_path = str(cik).strip().lstrip("0") or "0"
        url = f"https://www.sec.gov/Archives/edgar/data/{cik_path}/{acc.replace('-', '')}/{acc}.txt"
        submission = _sec.http_get_text(url)
        return _primary_document_text(submission)[:100_000] if submission else ""


# ── Company-name sanity ──────────────────────────────────────────────────────

_CORP_SUFFIX = (r"(?:Inc\.?|Incorporated|Corp\.?|Corporation|LLC|L\.L\.C\.|L\.P\.|LP|Ltd\.?|Limited|plc|PLC|"
                r"N\.V\.|S\.A\.|AG|SE|Holdings|Company|Co\.|Trust|Bancorp|Bancshares|Group)")
_NAME_CHARS = r"[A-Z][A-Za-z0-9&'\.,\- ]{1,70}?"
_MONTHS = r"(?:January|February|March|April|May|June|July|August|September|October|November|December)"
# Words that never appear inside a company's name but always inside a sentence.
_SENTENCE_WORDS = {
    'were', 'was', 'is', 'are', 'be', 'been', 'shall', 'will', 'would', 'may', 'each', 'issued', 'pursuant',
    'upon', 'which', 'that', 'such', 'its', 'their', 'this', 'herein', 'hereby', 'thereof', 'whereby', 'into',
    'by', 'from', 'with', 'between', 'among', 'under', 'any', 'all', 'outstanding', 'entered', 'completed',
    'announced', 'filed', 'agreed', 'became', 'effective', 'subject', 'following', 'prior', 'after', 'before',
    'meeting', 'stockholders', 'shareholders', 'holders', 'proxy', 'notice', 'statement', 'dated', 'special',
}
# Defined terms of merger agreements that are not a party's name on their own.
_DEFINED_TERMS = {
    'company', 'parent', 'merger', 'sub', 'subsidiary', 'stock', 'option', 'options', 'common', 'preferred',
    'shares', 'share', 'purchaser', 'buyer', 'seller', 'target', 'acquiror', 'acquirer', 'effective', 'time',
    'closing', 'agreement', 'plan', 'board', 'directors', 'transaction', 'transactions', 'offer', 'tender',
    'surviving', 'corporation', 'holdings', 'inc', 'corp', 'llc', 'the', 'and', 'of', 'unit', 'units', 'award',
    'awards', 'restricted', 'equity', 'notes', 'note', 'warrant', 'warrants', 'trust', 'fund', 'group', 'co',
    'or', 'acquisition', 'acquisitions', 'ltd', 'parties', 'party', 'entity', 'issuer', 'registrant',
}


def name_issue(name: str) -> Optional[str]:
    """Why `name` is not a usable company name, or None when it looks like one."""
    if not name or name.strip().lower() in ('unknown acquirer', 'unknown target', 'unknown'):
        return "not identified"
    n = name.strip()
    if len(n) < 3 or len(n) > 80:
        return "not a company name (length)"
    if n[0].islower() or not n[0].isalnum():
        return "starts like a sentence fragment"
    if re.match(r"^(?:Inc|Corp|LLC|Ltd|Co|Company|Corporation|L\.P)\b\.?", n):
        return "starts with a corporate suffix (sentence fragment)"
    if re.search(r"\.\s+(?!(?:Ltd|Limited|Inc|Corp|Co|LLC|Holdings|Group|Company)\b)[A-Z][a-z]+\s", n + " ") \
            and not re.search(r"\b[A-Z]\.\s?[A-Z]\.", n):
        return "spans more than one sentence"
    if re.search(_MONTHS + r"\s+\d", n) or re.search(r"\b(?:19|20)\d{2}\b", n):
        return "contains a date (sentence fragment)"
    if re.search(r"merger\s?sub|merger\s+(?:co|corp)\b|acquisition\s+sub\b", n, re.I):
        return "is a merger subsidiary (acquisition vehicle), not the buyer"
    words = re.findall(r"[A-Za-z]+", n)
    if any(w.lower() in _SENTENCE_WORDS for w in words):
        return "contains sentence words (fragment)"
    if words and all(w.lower() in _DEFINED_TERMS for w in words):
        return "a defined term of the agreement, not a name"
    for kw in ('entity emerging growth', 'common shares sufficient', 'more information about', 'table of contents',
               'capitalization', 'in connection with'):
        if kw in n.lower():
            return "boilerplate text"
    return None


def same_party(a: str, b: str) -> bool:
    na, nb = _sec.normalize_company_name(a), _sec.normalize_company_name(b)
    return bool(na) and na == nb


def deal_quality_issues(deal: Dict[str, Any]) -> List[str]:
    """Plain-language problems with an auto-extracted deal row (shown next to the 'unverified' badge)."""
    issues = []
    for role, key in (('Acquirer', 'acquirer_name'), ('Target', 'target_name')):
        why = name_issue(str(deal.get(key) or ''))
        if why:
            issues.append(f"{role} {why}")
    if deal.get('acquirer_name') and same_party(str(deal.get('acquirer_name')), str(deal.get('target_name') or '')):
        issues.append("Acquirer and target are the same company (self-tender or buyback, not M&A)")
    return issues


class MADealParser:
    # Forms filed by the TARGET (the filer is being acquired). An S-4 is filed by the acquirer registering the
    # shares it issues, an SC TO-T by the bidder; 8-K and 425 can come from either side.
    TARGET_FILER_FORMS = {'DEFM14A', 'PREM14A', 'SC 13E-3', 'SC 14D9'}
    SELF_TENDER_FORMS = {'SC TO-I'}

    def __init__(self, db: Optional[MADatabase] = None, fetcher=None):
        self.db = db or MADatabase()
        self.fetcher = fetcher or _EdgarAdapter()
        self.last_error: Optional[str] = None   # why the last parse_filing() returned None
        self.last_status: Optional[str] = None  # created | updated | kept_verified | rejected | failed
        self.last_draft: Optional[Dict[str, Any]] = None  # what was found when the guards rejected the filing

        # Specific "consideration" phrases first; a bare "$X billion" only counts near deal words.
        self.value_patterns_specific = [
            r'(?:aggregate|total)\s+(?:purchase\s+price|merger\s+consideration|consideration|transaction\s+value|'
            r'equity\s+value|enterprise\s+value)\s+(?:of\s+)?(?:approximately\s+)?\$\s*([\d,]+(?:\.\d+)?)\s*'
            r'(billion|million)?',
            r'(?:purchase\s+price|transaction\s+value|enterprise\s+value|equity\s+value)\s+of\s+(?:approximately\s+)?'
            r'\$\s*([\d,]+(?:\.\d+)?)\s*(billion|million)?',
            r'valued\s+at\s+(?:approximately\s+)?\$\s*([\d,]+(?:\.\d+)?)\s*(billion|million)',
        ]
        self.value_pattern_generic = r'\$\s*(\d+(?:\.\d+)?)\s*(billion|million)'

        self.date_patterns = [
            r'(?:dated|entered\s+into|effective)\s+(?:as\s+of\s+)?(' + _MONTHS + r'\s+\d{1,2},\s*\d{4})',
            r'On\s+(' + _MONTHS + r'\s+\d{1,2},\s*\d{4})',
        ]

    def filing_record(self, accession_number: str) -> Optional[Dict[str, Any]]:
        """The stored sec_filings row for `accession_number` (written by the scan), or None."""
        cursor = self.db._get_connection().cursor()
        cursor.execute("SELECT * FROM sec_filings WHERE accession_number = ?", (accession_number,))
        row = cursor.fetchone()
        if not row:
            return None
        rec = dict(row)
        try:
            extra = json.loads(rec.get('extracted_data') or '{}')
            if isinstance(extra, dict):
                for k in ('ticker', 'sic', 'industry'):
                    rec.setdefault(k, extra.get(k, ''))
        except ValueError:
            pass
        return rec

    def parse_filing(self, accession_number: str, filing_type: Optional[str] = None,
                     store: bool = True) -> Optional[Dict[str, Any]]:
        """Extract the deal a stored filing describes and (when `store`) save it as an unverified deal.
        Returns the deal, or None with `last_error` / `last_draft` saying why."""
        self.last_error, self.last_status, self.last_draft = None, None, None
        try:
            filing_record = self.filing_record(accession_number)
            if not filing_record:
                self.last_error = "not in the local filing index"
                self.last_status = 'failed'
                return None
            if filing_type and not filing_record.get('filing_type'):
                filing_record['filing_type'] = filing_type

            text = self._get_filing_text(filing_record)
            if not text:
                self.last_error = ("SEC refused the request (rate limit) - try again in a minute"
                                   if _sec.sec_throttled() else "could not fetch the filing text from SEC EDGAR")
                self.last_status = 'failed'
                return None

            deal_data = self._extract_deal_structure(text, filing_record)
            deal_data['filing_url'] = filing_record.get('filing_url') or _sec.filing_index_url(
                filing_record.get('cik', ''), accession_number)
            if 'browse-edgar' in deal_data['filing_url']:   # rows stored by older scans
                deal_data['filing_url'] = _sec.filing_index_url(filing_record.get('cik', ''), accession_number)
            issues = deal_quality_issues(deal_data)
            if issues:
                self.last_error = "; ".join(issues)
                self.last_status = 'rejected'
                self.last_draft = deal_data
                return None

            deal_id = self._generate_deal_id(deal_data)
            deal_data['deal_id'] = deal_id
            deal_data['data_source'] = 'SEC EDGAR'
            deal_data['source'] = 'SEC EDGAR (auto-extracted)'
            deal_data['verified'] = 0
            self._resolve_party_ciks(deal_data, filing_record)

            if not store:
                self.last_status = 'draft'
                return deal_data

            existing = self.db.get_deal_by_id(deal_id)
            if existing and int(existing.get('verified') or 0) == 1:
                # The user checked this deal: a re-parse must not overwrite it.
                self.last_status = 'kept_verified'
                existing_deal = dict(existing)
                self._mark_filing_parsed(accession_number, deal_id)
                return existing_deal
            self.db.insert_deal(deal_data)
            self.last_status = 'updated' if existing else 'created'
            self._mark_filing_parsed(accession_number, deal_id)
            return deal_data

        except Exception as e:
            self.last_error = str(e)
            self.last_status = 'failed'
            print(f"Error parsing {accession_number}: {e}", file=sys.stderr, flush=True)
        return None

    def _mark_filing_parsed(self, accession_number: str, deal_id: str):
        conn = self.db._get_connection()
        conn.execute("UPDATE sec_filings SET parsed_flag = 1, deal_id = ? WHERE accession_number = ?",
                     (deal_id, accession_number))
        conn.commit()

    def _get_filing_text(self, filing_record: Dict[str, Any]) -> str:
        if filing_record.get('raw_text'):
            return filing_record['raw_text']
        accession_number = filing_record.get('accession_number', '')
        cik = str(filing_record.get('cik', ''))
        if accession_number and cik:
            return self.fetcher.get_filing_text(accession_number, cik)
        return ""

    def _extract_deal_structure(self, text: str, filing_record: Dict[str, Any]) -> Dict[str, Any]:
        """Extract structured deal data from text (only what the filing states)."""
        deal_data: Dict[str, Any] = {}

        acquirer, target = self._extract_parties(text, filing_record)
        deal_data['acquirer_name'] = acquirer
        deal_data['target_name'] = target

        deal_value = self._extract_deal_value(text)
        if deal_value:
            deal_data['deal_value'] = deal_value
            deal_data['deal_value_currency'] = 'USD'

        deal_data['announcement_date'] = self._extract_announcement_date(text, filing_record)

        payment_method, cash_pct, stock_pct = self._extract_payment(text)
        if payment_method:
            deal_data['payment_method'] = payment_method
        if cash_pct is not None:
            deal_data['cash_percentage'] = cash_pct
            deal_data['stock_percentage'] = stock_pct

        deal_data['deal_type'] = self._infer_deal_type(text, filing_record)
        deal_data['deal_status'] = self._infer_status(text, filing_record)
        deal_data.update(self._extract_premiums(text))

        synergies = self._extract_synergies(text)
        if synergies:
            deal_data['synergies_disclosed'] = synergies
        rationale = self._extract_rationale(text)
        if rationale:
            deal_data['deal_rationale'] = rationale

        industry = filing_record.get('industry') or _sec.industry_from_sic(filing_record.get('sic', ''))
        if industry:
            deal_data['industry'] = industry

        per_share = self._extract_per_share_price(text)
        if per_share:
            deal_data['offer_price_per_share'] = per_share
        expected_close = self._extract_expected_close(text)
        if expected_close:
            deal_data['expected_close_date'] = expected_close
        breakup_fee = self._extract_breakup_fee(text)
        if breakup_fee:
            deal_data['breakup_fee'] = breakup_fee
        deal_data.update(self._extract_deal_flags(text, filing_record))
        return deal_data

    # ── Parties ──────────────────────────────────────────────────────────────

    def _first_valid(self, patterns: List[str], text: str, exclude: str = '') -> str:
        """First match of `patterns` that is a usable name. A match that starts too early ("On November 10, 2026,
        GoPro, Inc.") is cut back to its longest valid tail ("GoPro, Inc.")."""
        for pattern in patterns:
            for match in re.finditer(pattern, text):
                candidate = re.sub(r"\s+", " ", match.group(1).strip()).rstrip(',;:')
                tails = [candidate] + [candidate[m.start():] for m in re.finditer(r'(?<=[\s,.(])[A-Z]', candidate)]
                for tail in tails:
                    tail = tail.strip()
                    if exclude and same_party(tail, exclude):
                        continue
                    if name_issue(tail) is None:
                        return tail
        return ""

    def _extract_parties(self, text: str, filing_record: Dict[str, Any]) -> Tuple[str, str]:
        """(acquirer, target). Roles come from the agreement's defined terms ("Parent", "the Company") when the
        filing has them, otherwise from the form type. An unidentified party is returned as ''."""
        filer = filing_record.get('company_name', '') or ''
        filing_type = filing_record.get('filing_type', '')
        if filing_type in self.SELF_TENDER_FORMS:
            return filer, filer   # issuer tender offer: rejected by the same-party guard

        search_text = text[:20000]
        defined = r'\s*(?:,\s*an?\s+[A-Za-z ]{3,40}?(?:corporation|company|partnership)\s*)?\(\s*(?:the\s+)?["“”]?'
        parent_patterns = [
            r'(' + _NAME_CHARS + _CORP_SUFFIX + r')' + defined + r'(?:Parent|Acquiror|Acquirer|Buyer|Purchaser)["“”]?\s*\)',
        ]
        company_patterns = [
            r'(' + _NAME_CHARS + _CORP_SUFFIX + r')' + defined + r'(?:Company|Target)["“”]?\s*\)',
        ]
        parent = self._first_valid(parent_patterns, search_text)
        company = self._first_valid(company_patterns, search_text, exclude=parent)

        # The defined terms settle the roles when they name both sides or name the filer.
        if parent and company and not same_party(parent, company):
            return parent, company
        if parent and not same_party(parent, filer):
            return parent, filer
        if company and not same_party(company, filer):
            return filer, company

        acquirer_patterns = [
            r'(?:will\s+be\s+acquired\s+by|to\s+be\s+acquired\s+by|acquired\s+by)\s+(' + _NAME_CHARS + _CORP_SUFFIX + r')',
            r'(?:merger|combination)\s+(?:agreement\s+)?with\s+(' + _NAME_CHARS + _CORP_SUFFIX + r')',
            r'(' + _NAME_CHARS + _CORP_SUFFIX + r')\s+(?:has\s+)?agreed\s+to\s+acquire',
            r'(' + _NAME_CHARS + _CORP_SUFFIX + r')\s+(?:will\s+)?acquire[ds]?\s+(?:all|the)\s',
        ]
        target_patterns = [
            r'completed\s+(?:its\s+|the\s+)?(?:previously\s+announced\s+)?(?:acquisition\s+of|merger\s+with)\s+(' + _NAME_CHARS + _CORP_SUFFIX + r')',
            r'completion\s+of\s+(?:the\s+|its\s+)?acquisition\s+of\s+(' + _NAME_CHARS + _CORP_SUFFIX + r')',
            r'(?:agreed|agreement)\s+to\s+acquire\s+(?:all\s+of\s+the\s+outstanding\s+(?:shares|stock)\s+of\s+)?(' + _NAME_CHARS + _CORP_SUFFIX + r')',
            r'tender\s+offer\s+(?:to\s+purchase|for)\s+(?:all\s+(?:of\s+the\s+)?(?:outstanding\s+)?shares\s+of\s+(?:common\s+stock\s+of\s+)?)?(' + _NAME_CHARS + _CORP_SUFFIX + r')',
            r'acquisition\s+of\s+(' + _NAME_CHARS + _CORP_SUFFIX + r')',
        ]
        if filing_type in self.TARGET_FILER_FORMS:
            return self._first_valid(acquirer_patterns, search_text, exclude=filer), filer
        if filing_type == 'SC TO-T':
            # Filed by the bidder (the filer) for the subject company's shares.
            return filer, self._first_valid(target_patterns, search_text, exclude=filer)
        # 8-K / 425 / others: the filer may be either side - "acquired by X" makes it the target.
        acquirer = self._first_valid(acquirer_patterns[:1], search_text, exclude=filer)
        if acquirer:
            return acquirer, filer
        return filer, self._first_valid(target_patterns, search_text, exclude=filer)

    # ── Terms ────────────────────────────────────────────────────────────────

    @staticmethod
    def _amount(number: str, unit: Optional[str]) -> Optional[float]:
        try:
            value = float(number.replace(',', ''))
        except ValueError:
            return None
        unit = (unit or '').lower()
        if unit == 'billion':
            return value * 1e9
        if unit == 'million':
            return value * 1e6
        return value if value >= 1e6 else None   # a bare small number is not a deal value

    def _extract_deal_value(self, text: str) -> Optional[float]:
        for pattern in self.value_patterns_specific:
            m = re.search(pattern, text, re.IGNORECASE)
            if m:
                v = self._amount(m.group(1), m.group(2) if m.lastindex and m.lastindex >= 2 else None)
                if v:
                    return v
        deal_words = re.compile(r'merger|acquisition|acquire|purchase\s+price|consideration|transaction', re.I)
        for m in re.finditer(self.value_pattern_generic, text, re.IGNORECASE):
            window = text[max(0, m.start() - 150):m.end() + 150]
            if deal_words.search(window) and not re.search(r'credit\s+(?:facility|agreement)|notes\s+due|revolving|'
                                                            r'term\s+loan|indebtedness|bridge', window, re.I):
                return self._amount(m.group(1), m.group(2))
        return None

    def _extract_announcement_date(self, text: str, filing_record: Dict[str, Any]) -> str:
        for pattern in self.date_patterns:
            match = re.search(pattern, text[:5000])
            if match:
                try:
                    return datetime.strptime(re.sub(r"\s+", " ", match.group(1)), '%B %d, %Y').strftime('%Y-%m-%d')
                except ValueError:
                    pass
        return filing_record.get('filing_date') or datetime.now().strftime('%Y-%m-%d')

    def _extract_payment(self, text: str) -> Tuple[Optional[str], Optional[float], Optional[float]]:
        """(method, cash %, stock %) only as far as the filing states them; mixed deals leave the split unknown."""
        t = text.lower()
        cash = bool(re.search(r'per\s+share\s+in\s+cash|in\s+cash,?\s+without\s+interest|all[\s-]cash', t))
        stock = bool(re.search(r'exchange\s+ratio|shares?\s+of\s+(?:parent|acquir\w+)\s+common\s+stock\s+for\s+each|'
                               r'all[\s-]stock|stock[\s-]for[\s-]stock', t))
        mixed = bool(re.search(r'cash\s+and\s+(?:shares|stock)|combination\s+of\s+cash\s+and|mixed\s+consideration|'
                               r'cash\s+and\s+\d+(?:\.\d+)?\s+shares', t))
        if mixed or (cash and stock):
            return 'Mixed', None, None
        if cash:
            return 'Cash', 100.0, 0.0
        if stock:
            return 'Stock', 0.0, 100.0
        return None, None, None

    def _infer_deal_type(self, text: str, filing_record: Dict[str, Any]) -> str:
        t = text.lower()
        if filing_record.get('filing_type') in ('SC TO-T', 'SC 14D9') or 'tender offer' in t:
            return 'Takeover'
        if 'merger of equals' in t or 'agreement and plan of merger' in t or 'merger agreement' in t:
            return 'Merger'
        if 'asset purchase agreement' in t or 'sale of substantially all' in t:
            return 'Asset Purchase'
        return 'Acquisition'

    def _infer_status(self, text: str, filing_record: Dict[str, Any]) -> str:
        t = text.lower()
        if filing_record.get('filing_type') in ('DEFM14A', 'PREM14A', 'S-4', 'SC TO-T', 'SC 14D9', 'SC 13E-3'):
            return 'Pending'
        items = str(filing_record.get('items') or '')
        if '2.01' in items or re.search(r'completed\s+(?:its\s+|the\s+)?(?:previously\s+announced\s+)?'
                                        r'(?:acquisition|merger)|consummated\s+the\s+merger', t):
            return 'Completed'
        # "Termination of the Merger Agreement" is a section of every merger agreement: only an actual
        # termination, stated near the top (the 8-K item text), counts.
        if re.search(r'(?:has|have|was|were)\s+(?:been\s+)?terminated|terminated\s+the\s+(?:merger\s+)?agreement|'
                     r'mutually\s+agreed\s+to\s+terminate', t[:6000]):
            return 'Terminated'
        return 'Announced'

    def _extract_premiums(self, text: str) -> Dict[str, float]:
        premiums = {}
        pattern_groups = {
            'premium_1day': [
                r'(\d+(?:\.\d+)?)\s*%\s*premium\s+(?:to|over)\s+(?:the\s+)?(?:closing|last|unaffected)\s+(?:share\s+|stock\s+)?price',
                r'premium\s+of\s+(?:approximately\s+)?(\d+(?:\.\d+)?)\s*%\s+(?:to|over)\s+(?:the\s+)?(?:closing|unaffected)',
            ],
            'premium_4week': [
                r'(\d+(?:\.\d+)?)\s*%\s*premium\s+(?:to|over)\s+(?:the\s+)?(?:30|thirty)[- ](?:trading[- ])?day',
            ],
        }
        for key, patterns in pattern_groups.items():
            for pattern in patterns:
                match = re.search(pattern, text, re.IGNORECASE)
                if match:
                    try:
                        val = float(match.group(1))
                        if 0 < val < 200:
                            premiums[key] = val
                            break
                    except ValueError:
                        pass
        return premiums

    def _extract_synergies(self, text: str) -> Optional[float]:
        for pattern in (r'(?:annual\s+(?:run-rate\s+)?)?(?:cost\s+)?synergies\s+of\s+(?:approximately\s+)?\$\s*(\d+(?:\.\d+)?)\s*(billion|million)',
                        r'cost\s+savings?\s+of\s+(?:approximately\s+)?\$\s*(\d+(?:\.\d+)?)\s*(billion|million)'):
            match = re.search(pattern, text, re.IGNORECASE)
            if match:
                return self._amount(match.group(1), match.group(2))
        return None

    def _extract_rationale(self, text: str) -> str:
        for keyword in ('strategic rationale', 'benefits of the transaction', 'reasons for the merger',
                        'transaction rationale', 'compelling strategic fit'):
            idx = text.lower().find(keyword)
            if idx != -1:
                return text[idx:idx + 500].replace('\n', ' ')[:300]
        return ""

    def _extract_per_share_price(self, text):
        patterns = [
            r'\$\s*(\d+\.\d{2})\s+per\s+share\s+in\s+cash',
            r'(?:offer|merger|purchase)\s+price\s+of\s+\$\s*(\d+\.\d{2})\s+per\s+share',
            r'consideration\s+of\s+\$\s*(\d+\.\d{2})\s+per\s+share',
            r'per\s+share\s+(?:merger\s+)?consideration\s+of\s+\$\s*(\d+\.\d{2})',
            r'right\s+to\s+receive\s+\$\s*(\d+\.\d{2})',
        ]
        for pattern in patterns:
            for match in re.finditer(pattern, text, re.IGNORECASE):
                if re.search(r'par\s+value', text[max(0, match.start() - 40):match.start()], re.IGNORECASE):
                    continue
                try:
                    return float(match.group(1))
                except ValueError:
                    pass
        return None

    def _extract_expected_close(self, text):
        patterns = [
            r'expected\s+to\s+(?:close|be\s+completed?)\s+(?:in|by|during)\s+(?:the\s+)?([A-Za-z0-9\s]+?\d{4})',
            r'anticipated\s+to\s+(?:close|be\s+completed?)\s+(?:in|by|during)\s+(?:the\s+)?([A-Za-z0-9\s]+?\d{4})',
        ]
        for pattern in patterns:
            match = re.search(pattern, text, re.IGNORECASE)
            if match:
                raw2 = match.group(1).strip().rstrip('.')
                if len(raw2) <= 40:
                    return raw2
        return None

    def _extract_breakup_fee(self, text):
        patterns = [
            r'termination\s+fee\s+of\s+\$\s*(\d+(?:\.\d+)?)\s*(billion|million)',
            r'break[\-\s]?up\s+fee\s+of\s+\$\s*(\d+(?:\.\d+)?)\s*(billion|million)',
            r'\$\s*(\d+(?:\.\d+)?)\s*(billion|million)\s+termination\s+fee',
        ]
        for pattern in patterns:
            match = re.search(pattern, text, re.IGNORECASE)
            if match:
                return self._amount(match.group(1), match.group(2))
        return None

    def _extract_deal_flags(self, text, filing_record):
        """Flags only on explicit statements. "Unsolicited" alone is the no-shop clause of almost every merger
        agreement, so it no longer marks a deal hostile."""
        t = text.lower()
        flags = {}
        hostile = ('hostile' in t and re.search(r'hostile\s+(?:bid|offer|takeover|tender)', t)) or \
            re.search(r'recommends?\s+that\s+(?:the\s+)?(?:stockholders|shareholders)\s+reject', t)
        flags['hostile_flag'] = 1 if hostile else 0
        ft = filing_record.get('filing_type', '')
        flags['tender_offer_flag'] = 1 if (ft in ('SC TO-T', 'SC 14D9') or 'tender offer' in t) else 0
        flags['cross_border_flag'] = 1 if re.search(r'foreign\s+private\s+issuer|incorporated\s+(?:under\s+the\s+laws\s+of|in)\s+'
                                                    r'(?:the\s+)?(?:cayman|bermuda|ireland|united\s+kingdom|england|'
                                                    r'canada|ontario|british\s+columbia|germany|france|japan|'
                                                    r'netherlands|switzerland|singapore|israel|luxembourg|jersey)', t) else 0
        return flags

    def _resolve_party_ciks(self, deal_data, filing_record):
        """Fill acquirer/target CIK and ticker: the filer's from the filing itself, the other side only on an exact
        name match in SEC's ticker list."""
        filer = filing_record.get('company_name', '')
        for role in ('acquirer', 'target'):
            name = deal_data.get(f'{role}_name', '')
            if not name:
                continue
            if same_party(name, filer):
                if filing_record.get('cik'):
                    deal_data[f'{role}_cik'] = str(filing_record.get('cik'))
                if filing_record.get('ticker'):
                    deal_data[f'{role}_ticker'] = filing_record.get('ticker')
                continue
            try:
                hit = self.fetcher.resolve_company(name) if hasattr(self.fetcher, 'resolve_company') else None
            except Exception:
                hit = None
            if hit:
                deal_data[f'{role}_cik'] = hit.get('cik')
                if hit.get('ticker'):
                    deal_data[f'{role}_ticker'] = hit.get('ticker')

    def _generate_deal_id(self, deal_data: Dict[str, Any]) -> str:
        acquirer = re.sub(r'[^A-Z0-9]', '', (deal_data.get('acquirer_name') or 'UNK').upper())[:10]
        target = re.sub(r'[^A-Z0-9]', '', (deal_data.get('target_name') or 'UNK').upper())[:10]
        date = (deal_data.get('announcement_date') or '').replace('-', '')[:8]
        return f"{acquirer}_{target}_{date}"

    def parse_batch(self, accession_numbers: List[str], max_workers: int = 1) -> Dict[str, Any]:
        """Parse several filings (sequentially: the SEC client is rate-limited anyway)."""
        results = {'parsed': [], 'failed': [], 'total': len(accession_numbers)}
        for acc in accession_numbers:
            deal_data = self.parse_filing(acc)
            if deal_data:
                results['parsed'].append(deal_data)
            else:
                results['failed'].append({'accession_number': acc, 'reason': self.last_error})
        return results


def main():
    """CLI entry point - outputs JSON for C++ integration"""
    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "No command specified. Usage: deal_parser.py <command> [args...]"}))
        sys.exit(1)

    command = sys.argv[1]
    parser = MADealParser()

    try:
        if command == "parse":
            if len(sys.argv) < 4:
                raise ValueError("Filing URL and filing type required")
            filing_url, filing_type = sys.argv[2], sys.argv[3]
            acc_match = re.search(r'(\d{10}-\d{2}-\d{6})', filing_url) or \
                re.search(r'accession[_-]?number=([0-9-]+)', filing_url)
            accession_number = acc_match.group(1) if acc_match else filing_url.strip()

            deal_data = parser.parse_filing(accession_number, filing_type)
            if deal_data:
                result = {"success": True, "data": deal_data, "status": parser.last_status,
                          "accession_number": accession_number}
            elif parser.last_status == 'rejected':
                # Not stored: the user can complete the draft and save it as a verified deal.
                draft = dict(parser.last_draft or {})
                for key in ('acquirer_name', 'target_name'):
                    if name_issue(str(draft.get(key) or '')):
                        draft[key] = ''
                result = {"success": True, "status": "rejected", "accession_number": accession_number,
                          "reason": parser.last_error, "data": draft}
            elif parser.last_error == "not in the local filing index":
                result = {"success": False,
                          "error": f"Filing {accession_number} is not in the local filing index - scan SEC filings "
                                   "first, then pick it from the list."}
            else:
                result = {"success": False,
                          "error": f"Could not read filing {accession_number}: {parser.last_error or 'no M&A deal data found'}"}
            print(json.dumps(result, default=str))

        elif command == "parse_batch":
            if len(sys.argv) < 3:
                raise ValueError("Accession numbers JSON array required")
            accession_numbers = json.loads(sys.argv[2])
            results = parser.parse_batch(accession_numbers)
            print(json.dumps({"success": True, "data": results}, default=str))

        else:
            print(json.dumps({"success": False, "error": f"Unknown command: {command}. Available: parse, parse_batch"}))
            sys.exit(1)

    except Exception as e:
        print(json.dumps({"success": False, "error": str(e), "command": command}))
        sys.exit(1)
    finally:
        parser.db.close()


if __name__ == '__main__':
    main()
