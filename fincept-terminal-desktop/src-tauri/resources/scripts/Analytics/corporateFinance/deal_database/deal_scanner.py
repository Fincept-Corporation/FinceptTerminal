"""SEC Filing Scanner for M&A Deal Detection"""
import sys
from pathlib import Path
from typing import List, Dict, Any, Optional, Tuple
from datetime import datetime, timedelta
import re
from concurrent.futures import ThreadPoolExecutor, as_completed

scripts_path = Path(__file__).parent.parent.parent.parent
sys.path.append(str(scripts_path))

try:
    from edgar_tools import Company, get_filings, Filing
except ImportError:
    from edgartools import Company, get_filings, Filing

from ..config import MA_FILING_FORMS, MATERIAL_EVENT_ITEMS
from .database_schema import MADatabase

class MADealScanner:
    def __init__(self, db: Optional[MADatabase] = None):
        self.db = db or MADatabase()
        self.ma_keywords = [
            'merger', 'acquisition', 'acquire', 'purchase', 'takeover',
            'combination', 'consolidation', 'tender offer', 'definitive agreement',
            'merger agreement', 'stock purchase', 'asset purchase', 'amalgamation'
        ]
        self.ma_patterns = [
            r'(?:definitive|merger)\s+agreement',
            r'acquire[ds]?\s+(?:all|substantially all)',
            r'tender\s+offer',
            r'cash\s+and\s+stock\s+(?:transaction|deal)',
            r'purchase\s+price\s+of\s+\$[\d,\.]+\s*(?:million|billion)',
            r'(?:merger|acquisition)\s+consideration'
        ]

    def scan_recent_filings(self, days_back: int = 30, max_workers: int = 5) -> List[Dict[str, Any]]:
        """Scan recent SEC filings for M&A activity"""
        end_date = datetime.now()
        start_date = end_date - timedelta(days=days_back)

        potential_deals = []

        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            futures = []
            for form_type in MA_FILING_FORMS:
                future = executor.submit(self._scan_filing_type, form_type, start_date, end_date)
                futures.append(future)

            for future in as_completed(futures):
                try:
                    results = future.result()
                    potential_deals.extend(results)
                except Exception as e:
                    print(f"Error scanning filings: {e}")

        return potential_deals

    def _scan_filing_type(self, form_type: str, start_date: datetime, end_date: datetime) -> List[Dict[str, Any]]:
        """Scan specific filing type for M&A deals"""
        potential_deals = []

        try:
            filings = get_filings(
                form=form_type,
                start_date=start_date.strftime('%Y-%m-%d'),
                end_date=end_date.strftime('%Y-%m-%d')
            )

            for filing in filings[:100]:
                deal_info = self._analyze_filing(filing)
                if deal_info and deal_info['confidence_score'] > 0.5:
                    potential_deals.append(deal_info)
                    self._store_filing(filing, deal_info)

        except Exception as e:
            print(f"Error scanning {form_type}: {e}")

        return potential_deals

    def _analyze_filing(self, filing) -> Optional[Dict[str, Any]]:
        """Analyze if filing contains M&A activity"""
        try:
            text = self._extract_filing_text(filing)
            if not text:
                return None

            text_lower = text.lower()
            confidence_score = 0.0
            deal_indicators = []

            for keyword in self.ma_keywords:
                if keyword in text_lower:
                    confidence_score += 0.1
                    deal_indicators.append(keyword)

            for pattern in self.ma_patterns:
                if re.search(pattern, text_lower):
                    confidence_score += 0.2
                    deal_indicators.append(pattern)

            if filing.form in ['S-4', 'DEFM14A', 'PREM14A']:
                confidence_score += 0.4

            if filing.form == '8-K':
                items = self._extract_8k_items(text)
                if any(item in ['1.01', '2.01'] for item in items):
                    confidence_score += 0.3

            confidence_score = min(confidence_score, 1.0)

            if confidence_score > 0.5:
                return {
                    'accession_number': filing.accession_number,
                    'filing_type': filing.form,
                    'filing_date': filing.filing_date,
                    'company_name': filing.company,
                    'cik': filing.cik,
                    'filing_url': f"https://www.sec.gov/cgi-bin/browse-edgar?action=getcompany&CIK={filing.cik}&type={filing.form}&dateb=&owner=exclude&count=100",
                    'confidence_score': confidence_score,
                    'contains_ma_flag': 1,
                    'deal_indicators': ','.join(deal_indicators[:5])
                }

        except Exception as e:
            print(f"Error analyzing filing {filing.accession_number}: {e}")

        return None

    def _extract_filing_text(self, filing) -> str:
        """Extract text from SEC filing"""
        try:
            if hasattr(filing, 'text'):
                return filing.text()[:50000]
            elif hasattr(filing, 'html'):
                html = filing.html()
                text = re.sub('<[^<]+?>', ' ', html)
                return text[:50000]
            return ""
        except Exception as e:
            print(f"Error extracting text: {e}")
            return ""

    def _extract_8k_items(self, text: str) -> List[str]:
        """Extract Item numbers from 8-K filing"""
        items = []
        pattern = r'Item\s+(\d+\.\d+)'
        matches = re.findall(pattern, text)
        return list(set(matches))

    def scan_company_ma_history(self, ticker: str, years_back: int = 5) -> List[Dict[str, Any]]:
        """Scan specific company's M&A history"""
        try:
            company = Company(ticker)
            end_date = datetime.now()
            start_date = end_date - timedelta(days=years_back*365)

            all_deals = []

            for form_type in MA_FILING_FORMS:
                try:
                    filings = company.get_filings(
                        form=form_type
                    ).filter(
                        date=f"{start_date.strftime('%Y-%m-%d')}:{end_date.strftime('%Y-%m-%d')}"
                    )

                    for filing in filings:
                        deal_info = self._analyze_filing(filing)
                        if deal_info and deal_info['confidence_score'] > 0.5:
                            all_deals.append(deal_info)
                            self._store_filing(filing, deal_info)

                except Exception as e:
                    print(f"Error getting {form_type} for {ticker}: {e}")

            return all_deals

        except Exception as e:
            print(f"Error scanning company {ticker}: {e}")
            return []

    def scan_industry_ma_activity(self, industry_ciks: List[str], days_back: int = 90) -> List[Dict[str, Any]]:
        """Scan M&A activity for specific industry"""
        all_deals = []

        with ThreadPoolExecutor(max_workers=10) as executor:
            futures = {executor.submit(self._scan_cik, cik, days_back): cik for cik in industry_ciks}

            for future in as_completed(futures):
                try:
                    deals = future.result()
                    all_deals.extend(deals)
                except Exception as e:
                    print(f"Error scanning CIK: {e}")

        return all_deals

    def _scan_cik(self, cik: str, days_back: int) -> List[Dict[str, Any]]:
        """Scan specific CIK for M&A filings"""
        try:
            company = Company(cik)
            end_date = datetime.now()
            start_date = end_date - timedelta(days=days_back)

            deals = []
            for form_type in ['8-K', 'S-4', 'DEFM14A']:
                try:
                    filings = company.get_filings(form=form_type)
                    for filing in filings[:10]:
                        if filing.filing_date >= start_date.date():
                            deal_info = self._analyze_filing(filing)
                            if deal_info and deal_info['confidence_score'] > 0.6:
                                deals.append(deal_info)
                except:
                    continue

            return deals

        except Exception as e:
            return []

    def _store_filing(self, filing, deal_info: Dict[str, Any]):
        """Store filing information in database"""
        try:
            filing_data = {
                'accession_number': deal_info['accession_number'],
                'filing_type': deal_info['filing_type'],
                'filing_date': deal_info['filing_date'],
                'company_name': deal_info['company_name'],
                'cik': deal_info['cik'],
                'filing_url': deal_info['filing_url'],
                'confidence_score': deal_info['confidence_score'],
                'contains_ma_flag': deal_info['contains_ma_flag'],
                'parsed_flag': 0
            }

            self.db.insert_sec_filing(filing_data)

        except Exception as e:
            print(f"Error storing filing: {e}")

    def get_high_confidence_deals(self, min_confidence: float = 0.75) -> List[Dict[str, Any]]:
        """Get high confidence M&A deals from database"""
        conn = self.db._get_connection()
        cursor = conn.cursor()

        cursor.execute("""
            SELECT * FROM sec_filings
            WHERE contains_ma_flag = 1
            AND confidence_score >= ?
            AND parsed_flag = 0
            ORDER BY filing_date DESC
        """, (min_confidence,))

        return [dict(row) for row in cursor.fetchall()]

    def batch_scan_sp500(self, days_back: int = 30) -> Dict[str, Any]:
        """Scan all S&P 500 companies for M&A activity"""
        sp500_tickers = self._get_sp500_tickers()

        total_deals = []
        successful_scans = 0
        failed_scans = 0

        with ThreadPoolExecutor(max_workers=10) as executor:
            futures = {executor.submit(self.scan_company_ma_history, ticker, days_back//365): ticker
                      for ticker in sp500_tickers[:50]}

            for future in as_completed(futures):
                try:
                    deals = future.result()
                    total_deals.extend(deals)
                    successful_scans += 1
                except:
                    failed_scans += 1

        return {
            'total_deals_found': len(total_deals),
            'companies_scanned': successful_scans,
            'failed_scans': failed_scans,
            'deals': total_deals
        }

    def _get_sp500_tickers(self) -> List[str]:
        """Get S&P 500 tickers"""
        return [
            'AAPL', 'MSFT', 'GOOGL', 'AMZN', 'NVDA', 'META', 'TSLA', 'BRK.B', 'UNH', 'JNJ',
            'XOM', 'V', 'PG', 'JPM', 'MA', 'HD', 'CVX', 'MRK', 'ABBV', 'PEP', 'COST', 'AVGO',
            'KO', 'LLY', 'WMT', 'MCD', 'TMO', 'ABT', 'ACN', 'CSCO', 'DHR', 'CRM', 'VZ', 'NEE'
        ]

if __name__ == '__main__':
    scanner = MADealScanner()
    print("Scanning recent M&A filings (last 7 days)...")
    deals = scanner.scan_recent_filings(days_back=7)
    print(f"Found {len(deals)} potential M&A deals")
    for deal in deals[:5]:
        print(f"  - {deal['company_name']}: {deal['filing_type']} (Confidence: {deal['confidence_score']:.2f})")
