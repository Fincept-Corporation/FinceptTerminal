"""M&A Deal Parser - Extract structured data from SEC filings"""
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

# Use absolute imports instead of relative imports
from corporateFinance.config import DEAL_TYPES, PAYMENT_METHODS, INDUSTRIES
from corporateFinance.deal_database.database_schema import MADatabase
from corporateFinance.deal_database.edgar_fetcher import EdgarFetcher

class MADealParser:
    def __init__(self, db: Optional[MADatabase] = None, fetcher=None):
        self.db = db or MADatabase()
        self.fetcher = fetcher or EdgarFetcher()

        self.value_patterns = [
            r'\$\s*(\d+(?:\.\d+)?)\s*(?:billion|bn)',
            r'\$\s*(\d+(?:\.\d+)?)\s*(?:million|mn|mm)',
            r'consideration\s+of\s+\$\s*([\d,]+(?:\.\d+)?)',
            r'purchase\s+price\s+of\s+\$\s*([\d,]+(?:\.\d+)?)',
            r'aggregate\s+(?:purchase\s+)?price\s+of\s+\$\s*([\d,]+(?:\.\d+)?)'
        ]

        self.date_patterns = [
            r'(?:dated|entered\s+into|effective)\s+(?:as\s+of\s+)?(\w+\s+\d{1,2},\s*\d{4})',
            r'(\d{1,2}/\d{1,2}/\d{4})',
            r'(\w+\s+\d{4})'
        ]

        self.payment_indicators = {
            'cash': ['cash', 'cash consideration', 'cash payment', 'all cash'],
            'stock': ['stock', 'shares', 'common stock', 'exchange ratio', 'stock consideration'],
            'mixed': ['cash and stock', 'combination of cash and', 'mixed consideration']
        }

    def parse_filing(self, accession_number: str) -> Optional[Dict[str, Any]]:
        """Parse complete M&A deal from SEC filing"""
        try:
            conn = self.db._get_connection()
            cursor = conn.cursor()

            cursor.execute("""
                SELECT * FROM sec_filings
                WHERE accession_number = ?
            """, (accession_number,))

            filing_record = cursor.fetchone()
            if not filing_record:
                return None

            filing_record = dict(filing_record)

            text = self._get_filing_text(filing_record)
            if not text:
                return None

            deal_data = self._extract_deal_structure(text, filing_record)

            if deal_data:
                deal_id = self._generate_deal_id(deal_data)
                deal_data['deal_id'] = deal_id
                deal_data['data_source'] = 'SEC EDGAR'
                deal_data['filing_url'] = filing_record['filing_url']

                # Populate CIKs via name resolution (best-effort)
                self._resolve_party_ciks(deal_data)

                self.db.insert_deal(deal_data)

                # XBRL enrichment - optional, adds financials + valuation multiples
                self._enrich_deal_with_xbrl(deal_data, deal_id)

                cursor.execute("""
                    UPDATE sec_filings
                    SET parsed_flag = 1, deal_id = ?
                    WHERE accession_number = ?
                """, (deal_id, accession_number))
                conn.commit()

                return deal_data

        except Exception as e:
            print(f"Error parsing {accession_number}: {e}", file=sys.stderr)

        return None

    def _get_filing_text(self, filing_record: Dict[str, Any]) -> str:
        """Fetch filing text via EdgarFetcher."""
        if filing_record.get('raw_text'):
            return filing_record['raw_text']

        accession_number = filing_record.get('accession_number', '')
        cik = str(filing_record.get('cik', ''))
        if accession_number and cik:
            return self.fetcher.get_filing_text(accession_number, cik)

        return ""
    def _extract_deal_structure(self, text: str, filing_record: Dict[str, Any]) -> Dict[str, Any]:
        """Extract structured deal data from text"""
        deal_data = {}

        acquirer, target = self._extract_parties(text, filing_record)
        deal_data['acquirer_name'] = acquirer
        deal_data['target_name'] = target

        deal_value = self._extract_deal_value(text)
        if deal_value:
            deal_data['deal_value'] = deal_value
            deal_data['deal_value_currency'] = 'USD'

        announcement_date = self._extract_announcement_date(text, filing_record)
        if announcement_date:
            deal_data['announcement_date'] = announcement_date

        payment_method = self._extract_payment_method(text)
        deal_data['payment_method'] = payment_method
        # Derive percentage split from payment method string
        if payment_method == 'Cash':
            deal_data['cash_percentage'] = 100.0
            deal_data['stock_percentage'] = 0.0
        elif payment_method == 'Stock':
            deal_data['cash_percentage'] = 0.0
            deal_data['stock_percentage'] = 100.0
        elif payment_method == 'Mixed':
            deal_data['cash_percentage'] = 50.0
            deal_data['stock_percentage'] = 50.0

        deal_type = self._infer_deal_type(text)
        deal_data['deal_type'] = deal_type

        deal_data['deal_status'] = self._infer_status(text, filing_record)

        premiums = self._extract_premiums(text)
        deal_data.update(premiums)

        synergies = self._extract_synergies(text)
        if synergies:
            deal_data['synergies_disclosed'] = synergies

        rationale = self._extract_rationale(text)
        if rationale:
            deal_data['deal_rationale'] = rationale

        industry = self._infer_industry(text, filing_record)
        deal_data['industry'] = industry

        self._extract_advisors(text)

        # New extraction methods
        per_share = self._extract_per_share_price(text)
        if per_share:
            deal_data['offer_price_per_share'] = per_share

        expected_close = self._extract_expected_close(text)
        if expected_close:
            deal_data['expected_close_date'] = expected_close

        breakup_fee = self._extract_breakup_fee(text)
        if breakup_fee:
            deal_data['breakup_fee'] = breakup_fee

        flags = self._extract_deal_flags(text, filing_record)
        deal_data.update(flags)

        # Require at least one real company name
        acquirer_ok = bool(deal_data.get('acquirer_name')) and deal_data['acquirer_name'] != 'Unknown Acquirer'
        target_ok = bool(deal_data.get('target_name')) and deal_data['target_name'] != 'Unknown Target'
        if not acquirer_ok and not target_ok:
            return None

        # For 8-K filings, require an identified target (Unknown Target = noise)
        if filing_record.get('filing_type') == '8-K' and not target_ok:
            return None

        return deal_data

    def _extract_parties(self, text: str, filing_record: Dict[str, Any]) -> Tuple[str, str]:
        """Extract acquirer and target names from filing text."""
        company_name = filing_record.get('company_name', '')
        filing_type = filing_record.get('filing_type', '')

        # For proxy/merger forms the filer is typically the TARGET company
        target_filer_forms = {'DEFM14A', 'PREM14A', 'S-4', 'SC 13E-3', 'SC TO-T'}
        # SC TO-I = issuer tender offer — company buys back its own shares (self-tender)
        # There is no separate target; both acquirer and target are the same entity.
        self_tender_forms = {'SC TO-I'}
        # 8-K filer completed or announced an acquisition of another company
        acquirer_filer_forms = {'8-K'}

        # SC TO-I: self-tender / share buyback — no separate target company
        if filing_type in self_tender_forms:
            return company_name, company_name

        # Search in the first 15000 chars for better coverage
        search_text = text[:15000]

        # Patterns to find the acquirer name (used when filer is the target)
        acquirer_patterns = [
            # "Agreement and Plan of Merger, by and among [target], [acquirer], ..." — common in DEFM14A
            # The first named company after the filer in the by-and-among list is the acquirer
            r'by\s+and\s+among\s+\S[^,]+,\s+([A-Z][A-Za-z0-9\s,\.]+?(?:Inc\.|Corp\.|LLC|Ltd\.))',
            # "acquired by X Inc." / "will be acquired by X Corp."
            r'(?:will\s+be\s+acquired\s+by|to\s+be\s+acquired\s+by|acquired\s+by)\s+([A-Z][A-Za-z0-9\s&,\.]+?(?:Inc\.|Corp\.|LLC|Ltd\.|Company|Corporation|Holdings|Partners|Group|Media|Networks?))',
            r'(?:acquir(?:ed|es|ing))\s+by\s+([A-Z][A-Za-z0-9\s&,\.]+?(?:Inc\.|Corp\.|LLC|Ltd\.|Holdings|Media|Networks?))',
            # "Parent" / "Acquiror" defined terms in proxy filings
            r'(?:Parent|Acquiror|Acquirer|Buyer)[,\s]+([A-Z][A-Za-z0-9\s&,\.]+?(?:Inc\.|Corp\.|LLC|Ltd\.|Company|Corporation|Holdings))',
            r'(?:Parent|Acquiror|Acquirer|Buyer)[\s"]+(?:means|is|refers\s+to)\s+([A-Z][A-Za-z0-9\s&,\.]+?(?:Inc\.|Corp\.|LLC|Ltd\.))',
            # "X agreed to acquire" / "X will acquire all"
            r'([A-Z][A-Za-z0-9\s&,\.]+?(?:Inc\.|Corp\.|LLC|Ltd\.|Holdings|Media))\s+(?:has\s+)?agreed\s+to\s+acquire',
            r'([A-Z][A-Za-z0-9\s&,\.]+?(?:Inc\.|Corp\.|LLC|Ltd\.|Holdings|Media))\s+(?:will\s+)?acquire[ds]?\s+(?:all|the)',
            # Without suffix — 1-4 title-case words (catches "Skydance", "Apollo Global Management")
            r'(?:merger\s+with|combination\s+with|transaction\s+with)\s+([A-Z][A-Za-z0-9]+(?:\s+[A-Z][A-Za-z0-9]+){0,3})',
            r'(?:will\s+be\s+acquired\s+by|to\s+be\s+acquired\s+by|acquired\s+by)\s+([A-Z][A-Za-z0-9]+(?:\s+[A-Z][A-Za-z0-9]+){0,3})',
        ]

        # Patterns to find the target name (used when filer is the acquirer)
        target_patterns = [
            # "completed its previously announced merger with X" (8-K Item 2.01 completion)
            r'completed\s+(?:its\s+)?(?:previously\s+announced\s+)?merger\s+(?:\([^)]+\)\s+)?with\s+([A-Z][A-Za-z0-9\s&,\.]+?(?:Inc\.|Corp\.|LLC|Ltd\.|Company|Corporation|Holdings))',
            # "completed acquisition of X" / "completion of the acquisition of X" (8-K Item 2.01)
            r'completed\s+(?:the\s+)?acquisition\s+of\s+([A-Z][A-Za-z0-9\s&,\.]+?(?:Inc\.|Corp\.|LLC|Ltd\.|Company|Corporation|Holdings))',
            r'completion\s+of\s+(?:the\s+)?acquisition\s+of\s+([A-Z][A-Za-z0-9\s&,\.]+?(?:Inc\.|Corp\.|LLC|Ltd\.|Holdings))',
            # "acquired all/the outstanding shares of X"
            r'acquired\s+(?:all\s+(?:of\s+)?(?:the\s+)?(?:outstanding\s+)?(?:shares|stock)\s+of\s+)?([A-Z][A-Za-z0-9\s&,\.]+?(?:Inc\.|Corp\.|LLC|Ltd\.|Company|Corporation|Holdings))',
            r'acquire[ds]?\s+([A-Z][A-Za-z0-9\s&,\.]+?(?:Inc\.|Corp\.|LLC|Ltd\.|Company|Corporation|Holdings))',
            # Merger / tender offer descriptions
            r'merger\s+(?:of|with|between)\s+([A-Z][A-Za-z0-9\s&,\.]+?(?:Inc\.|Corp\.|LLC|Ltd\.))',
            r'purchase\s+(?:of|from)\s+([A-Z][A-Za-z0-9\s&,\.]+?(?:Inc\.|Corp\.|LLC))',
            r'tender\s+offer\s+for\s+(?:all\s+(?:outstanding\s+)?shares\s+of\s+)?([A-Z][A-Za-z0-9\s&,\.]+?(?:Inc\.|Corp\.|LLC|Ltd\.))',
            # "Target" defined term common in 8-K acquisition filings
            r'(?:the\s+)?(?:Target|Company)[,\s]+([A-Z][A-Za-z0-9\s&,\.]+?(?:Inc\.|Corp\.|LLC|Ltd\.|Company|Corporation|Holdings))',
            # Without corporate suffix — 1-4 title-case words
            r'completed\s+(?:the\s+)?acquisition\s+of\s+([A-Z][A-Za-z0-9]+(?:\s+[A-Z][A-Za-z0-9]+){0,3})',
            r'acquired\s+([A-Z][A-Za-z0-9]+(?:\s+[A-Z][A-Za-z0-9]+){0,3})\s+(?:for|in\s+a\s+deal|in\s+a\s+transaction)',
        ]

        acquirer = ""
        target = ""

        if filing_type in target_filer_forms:
            # Filer is the target — search text for acquirer name
            target = company_name
            for pattern in acquirer_patterns:
                match = re.search(pattern, search_text)
                if match:
                    candidate = match.group(1).strip().rstrip(',.')
                    if candidate.lower() != target.lower() and self._is_valid_company_name(candidate):
                        acquirer = candidate
                        break
            if not acquirer:
                acquirer = "Unknown Acquirer"

        elif filing_type in acquirer_filer_forms:
            # Filer is the acquirer — search text for target name
            acquirer = company_name
            for pattern in target_patterns:
                match = re.search(pattern, search_text)
                if match:
                    candidate = match.group(1).strip().rstrip(',.')
                    if candidate.lower() != acquirer.lower() and self._is_valid_company_name(candidate):
                        target = candidate
                        break
            if not target:
                target = "Unknown Target"

        else:
            # Unknown form type — treat filer as acquirer, try to find target
            acquirer = company_name
            for pattern in target_patterns:
                match = re.search(pattern, search_text)
                if match:
                    candidate = match.group(1).strip().rstrip(',.')
                    if self._is_valid_company_name(candidate):
                        target = candidate
                        break
            if not target:
                target = "Unknown Target"

        return acquirer, target
    def _extract_deal_value(self, text: str) -> Optional[float]:
        """Extract deal value from text"""
        for pattern in self.value_patterns:
            match = re.search(pattern, text, re.IGNORECASE)
            if match:
                value_str = match.group(1).replace(',', '')
                value = float(value_str)

                if 'billion' in match.group(0).lower() or 'bn' in match.group(0).lower():
                    return value * 1_000_000_000
                elif 'million' in match.group(0).lower():
                    return value * 1_000_000
                else:
                    if value > 1_000_000:
                        return value
                    elif value > 1000:
                        return value * 1_000_000
                    else:
                        return value * 1_000_000_000

        return None

    def _extract_announcement_date(self, text: str, filing_record: Dict[str, Any]) -> str:
        """Extract announcement date"""
        for pattern in self.date_patterns:
            match = re.search(pattern, text[:3000])
            if match:
                date_str = match.group(1)
                try:
                    if '/' in date_str:
                        date_obj = datetime.strptime(date_str, '%m/%d/%Y')
                    else:
                        date_obj = datetime.strptime(date_str, '%B %d, %Y')
                    return date_obj.strftime('%Y-%m-%d')
                except:
                    pass

        return filing_record['filing_date']

    def _extract_payment_method(self, text: str) -> str:
        """Extract payment method"""
        text_lower = text.lower()

        scores = {method: 0 for method in ['cash', 'stock', 'mixed']}

        for method, indicators in self.payment_indicators.items():
            for indicator in indicators:
                scores[method] += text_lower.count(indicator)

        if scores['mixed'] > 0 or (scores['cash'] > 0 and scores['stock'] > 0):
            return 'Mixed'
        elif scores['cash'] > scores['stock']:
            return 'Cash'
        elif scores['stock'] > 0:
            return 'Stock'

        return 'Cash'

    def _infer_deal_type(self, text: str) -> str:
        """Infer deal type from text"""
        text_lower = text.lower()

        if 'merger of equals' in text_lower or 'merger agreement' in text_lower:
            return 'Merger'
        elif 'tender offer' in text_lower:
            return 'Takeover'
        elif 'asset purchase' in text_lower or 'sale of assets' in text_lower:
            return 'Asset Purchase'
        elif 'joint venture' in text_lower:
            return 'Joint Venture'
        elif 'acquisition' in text_lower or 'acquire' in text_lower:
            return 'Acquisition'

        return 'Acquisition'

    def _infer_status(self, text: str, filing_record: Dict[str, Any]) -> str:
        """Infer deal status"""
        text_lower = text.lower()

        if filing_record['filing_type'] in ['DEFM14A', 'PREM14A']:
            return 'Pending'

        if 'closed' in text_lower or 'completed' in text_lower or 'consummated' in text_lower:
            return 'Completed'
        elif 'terminated' in text_lower or 'abandoned' in text_lower:
            return 'Terminated'
        elif 'pending' in text_lower or 'subject to' in text_lower:
            return 'Pending'

        return 'Announced'

    def _extract_premiums(self, text: str) -> Dict[str, float]:
        """Extract acquisition premiums"""
        premiums = {}

        # Multiple patterns per field; first match wins
        pattern_groups = {
            'premium_1day': [
                r'(\d+(?:\.\d+)?)\s*%\s*premium.*?(?:prior|previous|closing)\s+(?:trading\s+)?day',
                r'(\d+(?:\.\d+)?)\s*%\s*premium\s+(?:to|over|above)\s+(?:the\s+)?(?:closing|last|prior)\s+(?:share\s+)?price',
                r'premium\s+of\s+(?:approximately\s+)?(\d+(?:\.\d+)?)\s*%',
                r'represents\s+(?:a\s+)?(?:premium\s+of\s+)?(?:approximately\s+)?(\d+(?:\.\d+)?)\s*%\s+(?:premium|above|over)',
            ],
            'premium_1week': [
                r'(\d+(?:\.\d+)?)\s*%\s*premium.*?(?:prior|previous)\s+week',
                r'(\d+(?:\.\d+)?)\s*%\s*premium.*?(?:7|seven)[- ]day',
            ],
            'premium_4week': [
                r'(\d+(?:\.\d+)?)\s*%\s*premium.*?(?:prior|previous)\s+(?:four|4)\s+week',
                r'(\d+(?:\.\d+)?)\s*%\s*premium.*?(?:30|thirty)[- ]day',
            ],
        }

        for key, patterns in pattern_groups.items():
            for pattern in patterns:
                match = re.search(pattern, text, re.IGNORECASE)
                if match:
                    try:
                        val = float(match.group(1))
                        if 0 < val < 200:  # sanity check
                            premiums[key] = val
                            break
                    except ValueError:
                        pass

        return premiums

    def _extract_synergies(self, text: str) -> Optional[float]:
        """Extract disclosed synergies"""
        patterns = [
            r'synergies?\s+of\s+\$\s*(\d+(?:\.\d+)?)\s*(?:billion|million)',
            r'cost\s+savings?\s+of\s+\$\s*(\d+(?:\.\d+)?)\s*(?:billion|million)',
            r'annual\s+(?:run-rate\s+)?synergies?\s+of\s+\$\s*(\d+(?:\.\d+)?)\s*(?:billion|million)'
        ]

        for pattern in patterns:
            match = re.search(pattern, text, re.IGNORECASE)
            if match:
                value = float(match.group(1))
                if 'billion' in match.group(0).lower():
                    return value * 1_000_000_000
                else:
                    return value * 1_000_000

        return None

    def _extract_rationale(self, text: str) -> str:
        """Extract deal rationale"""
        rationale_keywords = [
            'strategic rationale', 'benefits of the transaction', 'reasons for the merger',
            'transaction rationale', 'compelling strategic fit'
        ]

        for keyword in rationale_keywords:
            idx = text.lower().find(keyword)
            if idx != -1:
                snippet = text[idx:idx+500]
                return snippet.replace('\n', ' ')[:300]

        return ""

    def _infer_industry(self, text: str, filing_record: Dict[str, Any]) -> str:
        """Infer industry from text"""
        text_lower = text.lower()

        industry_keywords = {
            'Technology': ['software', 'technology', 'cloud', 'saas', 'digital', 'platform'],
            'Healthcare': ['healthcare', 'pharmaceutical', 'biotech', 'medical', 'health'],
            'Financial Services': ['financial', 'banking', 'insurance', 'fintech', 'asset management'],
            'Industrials': ['manufacturing', 'industrial', 'aerospace', 'defense'],
            'Consumer Discretionary': ['retail', 'consumer', 'e-commerce', 'restaurant'],
            'Energy': ['energy', 'oil', 'gas', 'renewable', 'power'],
            'Real Estate': ['real estate', 'property', 'reit']
        }

        scores = {industry: 0 for industry in industry_keywords.keys()}

        for industry, keywords in industry_keywords.items():
            for keyword in keywords:
                scores[industry] += text_lower[:5000].count(keyword)

        if max(scores.values()) > 0:
            return max(scores, key=scores.get)

        return 'Diversified'

    def _extract_advisors(self, text: str) -> List[Dict[str, Any]]:
        """Extract financial and legal advisors"""
        advisors = []

        advisor_patterns = [
            r'([A-Z][A-Za-z\s&]+(?:LLC|LLP|Inc\.))\s+(?:is\s+)?(?:acting\s+as|served\s+as)\s+financial\s+advisor',
            r'financial\s+advisor[s]?[:\s]+([A-Z][A-Za-z\s&,]+(?:LLC|LLP|Inc\.))',
            r'([A-Z][A-Za-z\s&]+(?:LLC|LLP))\s+(?:is\s+)?(?:acting\s+as|served\s+as)\s+legal\s+counsel'
        ]

        for pattern in advisor_patterns:
            matches = re.findall(pattern, text[:10000])
            for match in matches:
                advisors.append({
                    'advisor_name': match.strip(),
                    'advisor_type': 'Financial' if 'financial' in pattern else 'Legal'
                })

        return advisors[:10]

    def _is_valid_company_name(self, name: str) -> bool:
        """Check if a candidate company name is a real company name (not boilerplate text)."""
        if not name or len(name) < 4:
            return False
        # Reject names that are clearly boilerplate/fragment text (too long)
        if len(name) > 80:
            return False
        # Reject names that start with lowercase (boilerplate sentence fragments)
        stripped = name.strip()
        if stripped and stripped[0].islower():
            return False
        # Reject names containing newlines or multiple spaces (formatting artifacts)
        if any(c in name for c in (chr(10), chr(13))) or '   ' in name:
            return False
        # Reject names that are just legal/SEC boilerplate terms
        boilerplate_keywords = [
            'entity emerging growth', 'agreement of', 'common shares sufficient',
            'additional written', 'preferred stock conversion', 'more information about',
            'surviving compan', 'pursuant to', 'in connection with', 'following the',
            'the company', 'the corporation', 'the acquiror', 'the acquirer', 'the parent',
            'each share of', 'all outstanding', 'table of contents', 'capitalization',
        ]
        name_lower = name.lower()
        for kw in boilerplate_keywords:
            if kw in name_lower:
                return False
        return True

    # --- New extraction helpers ---

    def _extract_per_share_price(self, text):
        """Extract the stated offer price per share."""
        patterns = [
            r'\$\s*(\d+\.\d{2})\s+per\s+(?:common\s+)?share',
            r'(\d+\.\d{2})\s+per\s+(?:common\s+)?share\s+in\s+cash',
            r'price\s+of\s+\$\s*(\d+\.\d{2})\s+per\s+share',
            r'consideration\s+of\s+\$\s*(\d+\.\d{2})\s+per\s+share',
            r'per\s+share\s+(?:merger\s+)?consideration\s+of\s+\$\s*(\d+\.\d{2})',
        ]
        for pattern in patterns:
            match = re.search(pattern, text, re.IGNORECASE)
            if match:
                try:
                    return float(match.group(1))
                except ValueError:
                    pass
        return None

    def _extract_expected_close(self, text):
        """Extract expected deal close date/period."""
        patterns = [
            r'expected\s+to\s+(?:close|be\s+completed?)\s+(?:in|by|during)\s+(?:the\s+)?([A-Za-z0-9\s]+?\d{4})',
            r'anticipated\s+to\s+(?:close|be\s+completed?)\s+(?:in|by|during)\s+(?:the\s+)?([A-Za-z0-9\s]+?\d{4})',
            r'expected\s+to\s+close\s+(?:by|on)\s+([A-Za-z]+\s+\d{1,2},\s*\d{4})',
        ]
        for pattern in patterns:
            match = re.search(pattern, text, re.IGNORECASE)
            if match:
                raw2 = match.group(1).strip().rstrip('.')
                if len(raw2) <= 40:
                    return raw2
        return None

    def _extract_breakup_fee(self, text):
        """Extract termination / break-up fee."""
        patterns = [
            r'termination\s+fee\s+of\s+\$\s*(\d+(?:\.\d+)?)\s*(billion|million|mn|bn)',
            r'break[\-\s]?up\s+fee\s+of\s+\$\s*(\d+(?:\.\d+)?)\s*(billion|million|mn|bn)',
            r'reverse\s+termination\s+fee\s+of\s+\$\s*(\d+(?:\.\d+)?)\s*(billion|million|mn|bn)',
            r'\$\s*(\d+(?:\.\d+)?)\s*(billion|million|mn|bn)\s+termination\s+fee',
        ]
        for pattern in patterns:
            match = re.search(pattern, text, re.IGNORECASE)
            if match:
                try:
                    value = float(match.group(1))
                    unit = match.group(2).lower()
                    if unit in ('billion', 'bn'):
                        return value * 1_000_000_000
                    return value * 1_000_000
                except (ValueError, IndexError):
                    pass
        return None

    def _extract_deal_flags(self, text, filing_record):
        """Detect deal characteristic flags from filing text."""
        text_lower = text.lower()
        flags = {}
        hostile_terms = ['unsolicited', 'without the approval of', 'hostile', 'unwanted bid', 'bear hug']
        flags['hostile_flag'] = 1 if any(t in text_lower for t in hostile_terms) else 0
        tender_terms = ['tender offer', 'sc to-t', 'all outstanding shares', 'any and all outstanding shares']
        ft = filing_record.get('filing_type', '')
        is_tender = any(t in text_lower for t in tender_terms) or ft in ('SC TO-T', 'SC TO-I')
        flags['tender_offer_flag'] = 1 if is_tender else 0
        cross_border_terms = [
            'foreign private issuer', 'cayman islands', 'bermuda', 'british columbia',
            'ontario', 'united kingdom', 'germany', 'france', 'japan', 'china', 'australia',
            'netherlands', 'ireland', 'switzerland', 'singapore', 'hong kong',
        ]
        flags['cross_border_flag'] = 1 if any(t in text_lower for t in cross_border_terms) else 0
        return flags

    # --- XBRL enrichment ---

    def _resolve_party_ciks(self, deal_data):
        """Populate acquirer_cik and target_cik via CIK resolution (best-effort)."""
        acquirer = deal_data.get('acquirer_name', '')
        target = deal_data.get('target_name', '')
        if acquirer == target:
            return
        if acquirer and acquirer != 'Unknown Acquirer':
            try:
                cik = self.fetcher.resolve_cik(acquirer)
                if cik:
                    deal_data['acquirer_cik'] = cik
            except Exception:
                pass
        if target and target != 'Unknown Target':
            try:
                cik = self.fetcher.resolve_cik(target)
                if cik:
                    deal_data['target_cik'] = cik
            except Exception:
                pass

    def _enrich_deal_with_xbrl(self, deal_data, deal_id):
        """Fetch XBRL financials for target company and store deal multiples (optional)."""
        target_cik = deal_data.get('target_cik')
        deal_value = deal_data.get('deal_value')
        target = deal_data.get('target_name', '')
        acquirer = deal_data.get('acquirer_name', '')
        if target == acquirer or not target_cik or not deal_value:
            return
        try:
            financials = self.fetcher.get_company_financials(target_cik)
            if not financials:
                return
            annual = financials.get('annual', {})
            announcement_date = deal_data.get('announcement_date', '')
            fin_record = {
                'deal_id': deal_id, 'period_type': 'LTM', 'party': 'Target',
                'data_date': financials.get('period_end', announcement_date),
            }
            for key, col in [
                ('revenue', 'revenue'), ('ebitda', 'ebitda'), ('net_income', 'net_income'),
                ('total_debt', 'total_debt'), ('cash', 'cash_and_equivalents'),
                ('shares_outstanding', 'shares_outstanding'), ('total_assets', 'total_assets')
            ]:
                if annual.get(key) is not None:
                    fin_record[col] = annual[key]
            if len(fin_record) > 4:
                self.db.insert_financials(fin_record)
            multiples = self.fetcher.calculate_deal_multiples(
                deal_value=deal_value, target_cik=target_cik, announcement_date=announcement_date)
            if multiples:
                mult_record = {
                    'deal_id': deal_id,
                    'calculation_date': announcement_date or datetime.now().strftime('%Y-%m-%d'),
                    'ltm_or_ntm': 'LTM',
                }
                if multiples.get('ev_revenue') is not None:
                    mult_record['ev_revenue'] = multiples['ev_revenue']
                    mult_record['target_ev_revenue'] = multiples['ev_revenue']
                if multiples.get('ev_ebitda') is not None:
                    mult_record['ev_ebitda'] = multiples['ev_ebitda']
                    mult_record['target_ev_ebitda'] = multiples['ev_ebitda']
                if multiples.get('implied_pe') is not None:
                    mult_record['price_earnings'] = multiples['implied_pe']
                    mult_record['target_pe'] = multiples['implied_pe']
                if multiples.get('implied_equity_value') is not None:
                    mult_record['implied_equity_value'] = multiples['implied_equity_value']
                    mult_record['implied_enterprise_value'] = deal_value
                if len(mult_record) > 3:
                    self.db.insert_multiples(mult_record)
                print(
                    "[Parser] XBRL enriched " + deal_id + ": EV/Rev=" + str(multiples.get('ev_revenue')) +
                    ", EV/EBITDA=" + str(multiples.get('ev_ebitda')),
                    file=sys.stderr
                )
        except Exception as e:
            print("[Parser] XBRL enrichment failed for " + deal_id + ": " + str(e), file=sys.stderr)


    def _generate_deal_id(self, deal_data: Dict[str, Any]) -> str:
        """Generate unique deal ID"""
        acquirer = deal_data.get('acquirer_name', 'UNK')[:10].upper().replace(' ', '')
        target = deal_data.get('target_name', 'UNK')[:10].upper().replace(' ', '')
        date = deal_data.get('announcement_date', '').replace('-', '')[:8]

        return f"{acquirer}_{target}_{date}"

    def parse_batch(self, accession_numbers: List[str], max_workers: int = 5) -> Dict[str, Any]:
        """Parse multiple filings in batch"""
        from concurrent.futures import ThreadPoolExecutor, as_completed

        results = {
            'parsed': [],
            'failed': [],
            'total': len(accession_numbers)
        }

        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            futures = {executor.submit(self.parse_filing, acc): acc for acc in accession_numbers}

            for future in as_completed(futures):
                acc = futures[future]
                try:
                    deal_data = future.result()
                    if deal_data:
                        results['parsed'].append(deal_data)
                    else:
                        results['failed'].append(acc)
                except Exception as e:
                    results['failed'].append(acc)
                    print(f"Error parsing {acc}: {e}")

        return results

def main():
    """CLI entry point - outputs JSON for Tauri integration"""
    import json

    if len(sys.argv) < 2:
        result = {
            "success": False,
            "error": "No command specified. Usage: deal_parser.py <command> [args...]"
        }
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]
    parser = MADealParser()

    try:
        if command == "parse":
            # parse_ma_filing(filing_url, filing_type)
            if len(sys.argv) < 4:
                raise ValueError("Filing URL and filing type required")

            filing_url = sys.argv[2]
            filing_type = sys.argv[3]

            # Extract accession number from URL
            import re
            acc_match = re.search(r'accession[_-]?number=([0-9-]+)', filing_url)
            if acc_match:
                accession_number = acc_match.group(1)
            else:
                # Try to use the URL as an accession number directly
                accession_number = filing_url

            deal_data = parser.parse_filing(accession_number)

            if deal_data:
                result = {
                    "success": True,
                    "data": deal_data
                }
            else:
                result = {
                    "success": False,
                    "error": "Failed to parse filing - no M&A deal data found"
                }

            print(json.dumps(result))

        elif command == "parse_batch":
            # Parse multiple filings
            if len(sys.argv) < 3:
                raise ValueError("Accession numbers JSON array required")

            accession_numbers = json.loads(sys.argv[2])
            max_workers = int(sys.argv[3]) if len(sys.argv) > 3 else 5

            results = parser.parse_batch(accession_numbers, max_workers)

            result = {
                "success": True,
                "data": results
            }
            print(json.dumps(result))

        else:
            result = {
                "success": False,
                "error": f"Unknown command: {command}. Available: parse, parse_batch"
            }
            print(json.dumps(result))
            sys.exit(1)

    except Exception as e:
        result = {
            "success": False,
            "error": str(e),
            "command": command
        }
        print(json.dumps(result))
        sys.exit(1)

if __name__ == '__main__':
    main()
