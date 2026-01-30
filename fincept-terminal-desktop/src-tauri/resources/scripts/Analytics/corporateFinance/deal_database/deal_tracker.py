"""M&A Deal Tracker - Main API for deal tracking and analysis"""
import sys
from pathlib import Path
from typing import Dict, Any, List, Optional
from datetime import datetime, timedelta
import json

from .database_schema import MADatabase
from .deal_scanner import MADealScanner
from .deal_parser import MADealParser

class MADealTracker:
    def __init__(self, db_path: Optional[Path] = None):
        self.db = MADatabase(db_path)
        self.scanner = MADealScanner(self.db)
        self.parser = MADealParser(self.db)

    def update_deal_database(self, days_back: int = 7, auto_parse: bool = True) -> Dict[str, Any]:
        """Update database with recent M&A deals"""
        print(f"Scanning SEC filings from last {days_back} days...")
        filings = self.scanner.scan_recent_filings(days_back=days_back)

        results = {
            'filings_found': len(filings),
            'deals_parsed': 0,
            'parsing_failed': 0,
            'timestamp': datetime.now().isoformat()
        }

        if auto_parse:
            high_confidence = [f for f in filings if f['confidence_score'] > 0.75]
            print(f"Parsing {len(high_confidence)} high-confidence filings...")

            for filing in high_confidence:
                try:
                    deal = self.parser.parse_filing(filing['accession_number'])
                    if deal:
                        results['deals_parsed'] += 1
                except Exception as e:
                    results['parsing_failed'] += 1
                    print(f"Parse error: {e}")

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
    """CLI interface"""
    import argparse

    parser = argparse.ArgumentParser(description='M&A Deal Tracker')
    parser.add_argument('--update', action='store_true', help='Update deal database')
    parser.add_argument('--days', type=int, default=7, help='Days to scan back')
    parser.add_argument('--search', type=str, help='Search deals by industry')
    parser.add_argument('--track', type=str, help='Track company by ticker')
    parser.add_argument('--summary', action='store_true', help='Show recent deals summary')
    parser.add_argument('--comps', type=str, help='Build comp table for industry')
    parser.add_argument('--value', type=float, help='Target value for comps')

    args = parser.parse_args()

    tracker = MADealTracker()

    try:
        if args.update:
            result = tracker.update_deal_database(days_back=args.days)
            print(f"\nUpdate complete:")
            print(f"  Filings found: {result['filings_found']}")
            print(f"  Deals parsed: {result['deals_parsed']}")
            print(f"  Failed: {result['parsing_failed']}")

        elif args.search:
            deals = tracker.search_deals(industry=args.search)
            print(f"\nFound {len(deals)} deals in {args.search}")
            for deal in deals[:10]:
                print(f"  {deal['announcement_date']}: {deal['acquirer_name']} -> {deal['target_name']}")
                if deal.get('deal_value'):
                    print(f"    Value: ${deal['deal_value']:,.0f}")

        elif args.track:
            result = tracker.track_company(args.track)
            print(f"\nM&A Activity for {args.track}:")
            print(f"  Total acquisitions: {result['total_acquisitions']}")
            print(f"  Filings found: {result['filings_found']}")

        elif args.summary:
            summary = tracker.get_recent_deals_summary(days=30)
            print(f"\nRecent M&A Activity (Last 30 days):")
            print(f"  Total deals: {summary['total_deals']}")
            print(f"  Total value: ${summary['total_value']:,.0f}")
            print(f"\nBy Industry:")
            for industry, count in summary['by_industry'].items():
                print(f"    {industry}: {count}")

        elif args.comps and args.value:
            comp_table = tracker.build_transaction_comp_table(args.comps, args.value)
            print(f"\nTransaction Comparables - {args.comps}")
            print(f"  Found {comp_table['comparable_count']} comparable deals")
            print(f"  Median EV/Revenue: {comp_table['valuation_multiples']['ev_revenue_median']:.2f}x")
            print(f"  Median EV/EBITDA: {comp_table['valuation_multiples']['ev_ebitda_median']:.2f}x")

        else:
            parser.print_help()

    finally:
        tracker.close()

if __name__ == '__main__':
    main()
