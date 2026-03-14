"""M&A Deal Tracker - Main API for deal tracking and analysis"""
import sys
from pathlib import Path
from typing import Dict, Any, List, Optional
from datetime import datetime, timedelta
import json

# Add Analytics path for absolute imports
analytics_path = Path(__file__).parent.parent.parent
sys.path.insert(0, str(analytics_path))

# Use absolute imports instead of relative imports
from corporateFinance.deal_database.database_schema import MADatabase
from corporateFinance.deal_database.deal_scanner import MADealScanner

try:
    from corporateFinance.deal_database.deal_parser import MADealParser
    PARSER_AVAILABLE = True
except ImportError:
    MADealParser = None
    PARSER_AVAILABLE = False


class MADealTracker:
    def __init__(self, db_path: Optional[Path] = None):
        self.db = MADatabase(db_path)
        self.scanner = MADealScanner(self.db)
        self.parser = MADealParser(self.db) if PARSER_AVAILABLE else None

    def update_deal_database(self, days_back: int = 7, auto_parse: bool = True) -> Dict[str, Any]:
        """Update database with recent M&A deals"""
        print(f"[DealTracker] Scanning SEC filings from last {days_back} days...", file=sys.stderr)
        filings = self.scanner.scan_recent_filings(days_back=days_back)

        results = {
            'filings_found': len(filings),
            'deals_parsed': 0,
            'deals_created': 0,
            'parsing_failed': 0,
            'filings': filings,
            'timestamp': datetime.now().isoformat()
        }

        if not filings:
            return results

        # Store filings into sec_filings table
        for filing in filings:
            try:
                self.db.insert_sec_filing({
                    'accession_number': filing['accession_number'],
                    'filing_type': filing['filing_type'],
                    'filing_date': filing['filing_date'],
                    'company_name': filing['company_name'],
                    'cik': filing['cik'],
                    'filing_url': filing.get('filing_url', ''),
                    'confidence_score': filing['confidence_score'],
                    'contains_ma_flag': filing.get('contains_ma_flag', 1),
                    'parsed_flag': 0,
                })
            except Exception:
                pass

        # Try to parse high-confidence filings into actual deals
        if auto_parse and self.parser:
            high_confidence = [f for f in filings if f['confidence_score'] > 0.75]
            print(f"[DealTracker] Parsing {len(high_confidence)} high-confidence filings...", file=sys.stderr)

            for filing in high_confidence:
                try:
                    deal = self.parser.parse_filing(filing['accession_number'])
                    if deal:
                        results['deals_parsed'] += 1
                except Exception as e:
                    results['parsing_failed'] += 1
                    print(f"[DealTracker] Parse error: {e}", file=sys.stderr)

        if results['deals_parsed'] == 0:
            print("[DealTracker] No deals parsed from filings.", file=sys.stderr)

        return results

    def search_deals(self, **filters) -> List[Dict[str, Any]]:
        """Search M&A deals with filters"""
        return self.db.search_deals(filters)

    def get_deal_details(self, deal_id: str) -> Optional[Dict[str, Any]]:
        """Get complete deal details including financials and multiples"""
        deal = self.db.get_deal_by_id(deal_id)
        if not deal:
            return None

        deal['financials'] = self.db.get_deal_financials(deal_id)
        deal['multiples'] = self.db.get_deal_multiples(deal_id)

        return deal

    def get_comparable_transactions(self, industry: str, target_value: float,
                                   date_range_years: int = 3, limit: int = 20) -> List[Dict[str, Any]]:
        """Get comparable M&A transactions"""
        end_date = datetime.now()
        start_date = end_date - timedelta(days=date_range_years*365)

        min_value = target_value * 0.5
        max_value = target_value * 2.0

        return self.db.get_comparable_transactions(
            industry=industry,
            min_value=min_value,
            max_value=max_value,
            start_date=start_date.strftime('%Y-%m-%d'),
            end_date=end_date.strftime('%Y-%m-%d'),
            limit=limit
        )

    def get_industry_statistics(self, industry: str, years_back: int = 3) -> Dict[str, Any]:
        """Get M&A statistics for industry"""
        end_date = datetime.now()
        start_date = end_date - timedelta(days=years_back*365)

        stats = self.db.get_industry_stats(
            industry=industry,
            start_date=start_date.strftime('%Y-%m-%d'),
            end_date=end_date.strftime('%Y-%m-%d')
        )

        deals = self.search_deals(
            industry=industry,
            start_date=start_date.strftime('%Y-%m-%d'),
            end_date=end_date.strftime('%Y-%m-%d')
        )

        if deals:
            values = [d['deal_value'] for d in deals if d.get('deal_value')]
            premiums_1day = [d['premium_1day'] for d in deals if d.get('premium_1day')]

            stats['median_deal_value'] = sorted(values)[len(values)//2] if values else 0
            stats['median_premium_1day'] = sorted(premiums_1day)[len(premiums_1day)//2] if premiums_1day else 0

        return stats

    def track_company(self, ticker: str, years_back: int = 5) -> Dict[str, Any]:
        """Track M&A activity for specific company"""
        print(f"Tracking M&A history for {ticker}...")

        filings = self.scanner.scan_company_ma_history(ticker, years_back)

        deals_as_acquirer = self.search_deals(acquirer_ticker=ticker)
        deals_as_target = self.search_deals(target_ticker=ticker)

        return {
            'ticker': ticker,
            'filings_found': len(filings),
            'deals_as_acquirer': deals_as_acquirer,
            'deals_as_target': deals_as_target,
            'total_acquisitions': len(deals_as_acquirer),
            'acquisition_history': deals_as_acquirer
        }

    def calculate_deal_statistics(self, deals: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Calculate statistical metrics for deals"""
        if not deals:
            return {}

        values = [d['deal_value'] for d in deals if d.get('deal_value')]
        premiums_1day = [d['premium_1day'] for d in deals if d.get('premium_1day')]
        premiums_4week = [d['premium_4week'] for d in deals if d.get('premium_4week')]

        return {
            'deal_count': len(deals),
            'total_value': sum(values),
            'avg_value': sum(values) / len(values) if values else 0,
            'median_value': sorted(values)[len(values)//2] if values else 0,
            'avg_premium_1day': sum(premiums_1day) / len(premiums_1day) if premiums_1day else 0,
            'median_premium_1day': sorted(premiums_1day)[len(premiums_1day)//2] if premiums_1day else 0,
            'avg_premium_4week': sum(premiums_4week) / len(premiums_4week) if premiums_4week else 0,
            'median_premium_4week': sorted(premiums_4week)[len(premiums_4week)//2] if premiums_4week else 0,
            'cash_deals': len([d for d in deals if d.get('payment_method') == 'Cash']),
            'stock_deals': len([d for d in deals if d.get('payment_method') == 'Stock']),
            'mixed_deals': len([d for d in deals if d.get('payment_method') == 'Mixed'])
        }

    def build_transaction_comp_table(self, industry: str, target_value: float,
                                    years_back: int = 3) -> Dict[str, Any]:
        """Build transaction comparable table"""
        comps = self.get_comparable_transactions(industry, target_value, years_back)

        if not comps:
            return {'error': 'No comparable transactions found'}

        stats = self.calculate_deal_statistics(comps)

        return {
            'industry': industry,
            'target_value': target_value,
            'comparable_count': len(comps),
            'comparables': comps,
            'statistics': stats,
            'valuation_multiples': {
                'ev_revenue_median': self._calculate_median([c.get('ev_revenue') for c in comps if c.get('ev_revenue')]),
                'ev_ebitda_median': self._calculate_median([c.get('ev_ebitda') for c in comps if c.get('ev_ebitda')]),
                'premium_1day_median': stats.get('median_premium_1day', 0),
                'premium_4week_median': stats.get('median_premium_4week', 0)
            }
        }

    def _calculate_median(self, values: List[float]) -> float:
        """Calculate median of list"""
        if not values:
            return 0.0
        sorted_vals = sorted(values)
        n = len(sorted_vals)
        if n % 2 == 0:
            return (sorted_vals[n//2-1] + sorted_vals[n//2]) / 2
        return sorted_vals[n//2]

    def export_deals_to_json(self, deals: List[Dict[str, Any]], output_path: str):
        """Export deals to JSON file"""
        with open(output_path, 'w') as f:
            json.dump(deals, f, indent=2, default=str)

    def get_recent_deals_summary(self, days: int = 30, min_value: float = 100_000_000) -> Dict[str, Any]:
        """Get summary of recent significant deals"""
        end_date = datetime.now()
        start_date = end_date - timedelta(days=days)

        deals = self.search_deals(
            start_date=start_date.strftime('%Y-%m-%d'),
            end_date=end_date.strftime('%Y-%m-%d'),
            min_value=min_value
        )

        by_industry = {}
        by_status = {}

        for deal in deals:
            industry = deal.get('industry', 'Unknown')
            status = deal.get('deal_status', 'Unknown')

            by_industry[industry] = by_industry.get(industry, 0) + 1
            by_status[status] = by_status.get(status, 0) + 1

        return {
            'period_days': days,
            'total_deals': len(deals),
            'total_value': sum(d.get('deal_value', 0) for d in deals),
            'by_industry': by_industry,
            'by_status': by_status,
            'top_deals': sorted(deals, key=lambda x: x.get('deal_value', 0), reverse=True)[:10],
            'statistics': self.calculate_deal_statistics(deals)
        }

    def close(self):
        """Close database connection"""
        self.db.close()

def main():
    """CLI entry point - outputs JSON for Tauri integration"""
    import json

    if len(sys.argv) < 2:
        result = {
            "success": False,
            "error": "No command specified. Usage: deal_tracker.py <command> [args...]"
        }
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]
    tracker = MADealTracker()

    try:
        if command == "update":
            # Update deal database
            days_back = int(sys.argv[2]) if len(sys.argv) > 2 else 7
            result = tracker.update_deal_database(days_back=days_back)

            # Return only summary + slim filing list (no full deals query — frontend calls get_all separately)
            filings = result.get('filings', [])
            slim_filings = [
                {
                    'accession_number': f.get('accession_number', ''),
                    'filing_type': f.get('filing_type', ''),
                    'filing_date': f.get('filing_date', ''),
                    'company_name': f.get('company_name', ''),
                    'cik': f.get('cik', ''),
                    'filing_url': f.get('filing_url', ''),
                    'confidence_score': f.get('confidence_score', 0),
                    'deal_indicators': f.get('deal_indicators', ''),
                }
                for f in filings
            ]

            output = {
                "success": True,
                "data": {
                    'filings_found': result.get('filings_found', 0),
                    'deals_parsed': result.get('deals_parsed', 0),
                    'deals_created': result.get('deals_created', 0),
                    'parsing_failed': result.get('parsing_failed', 0),
                    'timestamp': result.get('timestamp', ''),
                    'filings': slim_filings,
                    'deals': [],
                    'deals_count': 0,
                }
            }
            print(json.dumps(output, default=str))

        elif command == "search":
            # Search deals by industry
            if len(sys.argv) < 3:
                raise ValueError("Industry name required")

            industry = sys.argv[2]
            deals = tracker.search_deals(industry=industry)

            result = {
                "success": True,
                "data": deals,
                "count": len(deals)
            }
            print(json.dumps(result))

        elif command == "track":
            # Track company M&A activity
            if len(sys.argv) < 3:
                raise ValueError("Company ticker required")

            ticker = sys.argv[2]
            result = tracker.track_company(ticker)

            output = {
                "success": True,
                "data": result
            }
            print(json.dumps(output))

        elif command == "summary":
            # Get recent deals summary
            days = int(sys.argv[2]) if len(sys.argv) > 2 else 30
            summary = tracker.get_recent_deals_summary(days=days)

            result = {
                "success": True,
                "data": summary
            }
            print(json.dumps(result))

        elif command == "comps":
            # Build transaction comp table
            if len(sys.argv) < 4:
                raise ValueError("Industry and target value required")

            industry = sys.argv[2]
            target_value = float(sys.argv[3])

            comp_table = tracker.build_transaction_comp_table(industry, target_value)

            result = {
                "success": True,
                "data": comp_table
            }
            print(json.dumps(result))

        else:
            result = {
                "success": False,
                "error": f"Unknown command: {command}. Available: update, search, track, summary, comps"
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
    finally:
        tracker.close()

if __name__ == '__main__':
    main()
