"""M&A Deal Parser - Extract structured data from SEC filings"""
import sys
from pathlib import Path
from typing import Dict, Any, Optional, List, Tuple
import re
from datetime import datetime
import json

scripts_path = Path(__file__).parent.parent.parent.parent
sys.path.append(str(scripts_path))

try:
    from edgar_tools import Company, Filing
except ImportError:
    from edgartools import Company, Filing

from ..config import DEAL_TYPES, PAYMENT_METHODS, INDUSTRIES
from .database_schema import MADatabase

class MADealParser:
    def __init__(self, db: Optional[MADatabase] = None):
        self.db = db or MADatabase()

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

                self.db.insert_deal(deal_data)

                cursor.execute("""
                    UPDATE sec_filings
                    SET parsed_flag = 1, deal_id = ?
                    WHERE accession_number = ?
                """, (deal_id, accession_number))
                conn.commit()

                return deal_data

        except Exception as e:
            print(f"Error parsing {accession_number}: {e}")

        return None

    def _get_filing_text(self, filing_record: Dict[str, Any]) -> str:
        """Get filing text from record or fetch from SEC"""
        if filing_record.get('raw_text'):
            return filing_record['raw_text']

        try:
            cik = filing_record['cik']
            company = Company(cik)
            filings = company.get_filings(form=filing_record['filing_type'])

            for filing in filings:
                if filing.accession_number == filing_record['accession_number']:
                    if hasattr(filing, 'text'):
                        return filing.text()[:100000]
                    elif hasattr(filing, 'html'):
                        html = filing.html()
                        return re.sub('<[^<]+?>', ' ', html)[:100000]
        except:
            pass

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

        advisors = self._extract_advisors(text)

        if not deal_data.get('acquirer_name') or not deal_data.get('target_name'):
            return None

        return deal_data

    def _extract_parties(self, text: str, filing_record: Dict[str, Any]) -> Tuple[str, str]:
        """Extract acquirer and target names"""
        acquirer = filing_record['company_name']
        target = ""

        patterns = [
            r'acquire[ds]?\s+([A-Z][A-Za-z\s&]+(?:Inc\.|Corp\.|LLC|Ltd\.|Company|Corporation))',
            r'merger\s+(?:of|with)\s+([A-Z][A-Za-z\s&]+(?:Inc\.|Corp\.|LLC|Ltd\.))',
            r'purchase\s+(?:of|from)\s+([A-Z][A-Za-z\s&]+(?:Inc\.|Corp\.|LLC))',
        ]

        for pattern in patterns:
            match = re.search(pattern, text[:5000])
            if match:
                target = match.group(1).strip()
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

        patterns = {
            'premium_1day': r'(\d+(?:\.\d+)?)\s*%\s*premium.*(?:prior|previous)\s+day',
            'premium_1week': r'(\d+(?:\.\d+)?)\s*%\s*premium.*(?:prior|previous)\s+week',
            'premium_4week': r'(\d+(?:\.\d+)?)\s*%\s*premium.*(?:prior|previous)\s+(?:four|4)\s+week'
        }

        for key, pattern in patterns.items():
            match = re.search(pattern, text, re.IGNORECASE)
            if match:
                premiums[key] = float(match.group(1))

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

if __name__ == '__main__':
    parser = MADealParser()

    db = MADatabase()
    high_conf_filings = db._get_connection().execute("""
        SELECT accession_number FROM sec_filings
        WHERE confidence_score > 0.75 AND parsed_flag = 0
        LIMIT 5
    """).fetchall()

    for filing in high_conf_filings:
        acc = filing[0]
        print(f"\nParsing {acc}...")
        deal = parser.parse_filing(acc)
        if deal:
            print(f"  Deal: {deal['acquirer_name']} acquiring {deal['target_name']}")
            print(f"  Value: ${deal.get('deal_value', 0):,.0f}")
