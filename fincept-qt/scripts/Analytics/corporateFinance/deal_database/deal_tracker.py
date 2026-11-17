"""M&A Deal Tracker - Main API for deal tracking and analysis

CLI (used by the Qt MAAnalyticsService; every command prints one JSON object on stdout):
  scan <days> [max_filings] [forms_csv]   search EDGAR for M&A filings of the last <days> days (1-90), store them,
                                          and extract deals from up to <max_filings> high-confidence filings not
                                          read before. Progress goes to stderr as one line per step:
                                          @@PROGRESS {"phase", "done", "total", "message", "filings", "deals_created"}
  list                                    every deal, with verified (0/1), source and quality_issues
  search <text>                           deals whose names / industry / tickers contain <text>
  filings [limit]                         stored filings, newest first, with their sec.gov index URL
  upsert <json>                           save a deal the user entered or edited (marks it verified)
  verify <deal_id> <0|1>                  mark a deal verified / unverified
  delete <deal_id>                        delete a deal (its filing stays in the index)
  track <ticker> | summary [days] | comps <industry> <value>
"""
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
from corporateFinance.deal_database.deal_scanner import (MADealScanner, DEFAULT_SCAN_FORMS, filing_index_url,
                                                         sec_throttled, sec_request_count)
from corporateFinance.deal_database.deal_parser import MADealParser, deal_quality_issues

MAX_SCAN_DAYS = 90
DEFAULT_MAX_FILINGS = 40


def _progress(phase: str, done: int, total: int, message: str, **extra):
    """One machine-readable progress line on stderr (the Qt side shows `message` and a done/total bar)."""
    payload = {"phase": phase, "done": done, "total": total, "message": message}
    payload.update(extra)
    print("@@PROGRESS " + json.dumps(payload), file=sys.stderr, flush=True)


class MADealTracker:
    def __init__(self, db_path: Optional[Path] = None):
        self.db = MADatabase(db_path)
        self.scanner = MADealScanner(self.db)
        self.parser = MADealParser(self.db)

    def update_deal_database(self, days_back: int = 7, auto_parse: bool = True,
                             max_filings: int = DEFAULT_MAX_FILINGS,
                             forms: Optional[List[str]] = None) -> Dict[str, Any]:
        """Search EDGAR, store the filings, extract (unverified) deals from the high-confidence ones."""
        days_back = max(1, min(int(days_back), MAX_SCAN_DAYS))
        started = datetime.now()
        print(f"[DealTracker] Scanning SEC filings from last {days_back} days...", file=sys.stderr, flush=True)
        results = {
            'days_scanned': days_back,
            'filings_found': 0,
            'deals_parsed': 0,      # filings that produced a deal row (created + updated)
            'deals_created': 0,     # new deal rows
            'deals_updated': 0,     # existing unverified rows refreshed
            'deals_kept_verified': 0,  # rows the user verified: never overwritten
            'deals_rejected': 0,    # failed the quality guards (not stored)
            'parsing_failed': 0,    # could not read the filing
            'already_parsed': 0,
            'rejections': [],
            'filings': [],
            'timestamp': started.isoformat(),
        }

        state = {'filings': 0}

        def search_progress(phase, done, total, message):
            _progress(phase, done, total, message, filings=state['filings'], deals_created=0)

        filings = self.scanner.scan_recent_filings(days_back=days_back, forms=forms, progress=search_progress)
        results['filings_found'] = len(filings)
        results['filings'] = filings
        state['filings'] = len(filings)
        _progress("stored", len(filings), len(filings), f"{len(filings)} filings stored",
                  filings=len(filings), deals_created=0)

        if auto_parse and filings and not sec_throttled():
            parsed_before = {row['accession_number'] for row in self.db.list_filings(100000)
                             if int(row.get('parsed_flag') or 0) == 1}
            candidates = [f for f in filings if f.get('confidence_score', 0) >= 0.75
                          and f.get('filing_type') != 'SC TO-I']
            fresh = [f for f in candidates if f['accession_number'] not in parsed_before]
            results['already_parsed'] = len(candidates) - len(fresh)
            todo = fresh[:max(0, int(max_filings))]
            results['parse_skipped_over_limit'] = max(0, len(fresh) - len(todo))
            print(f"[DealTracker] Reading {len(todo)} high-confidence filings...", file=sys.stderr, flush=True)

            for i, filing in enumerate(todo, start=1):
                if sec_throttled():
                    break
                _progress("parse", i - 1, len(todo),
                          f"Reading SEC filings - {i} of {len(todo)}: {filing.get('company_name', '')} "
                          f"({filing.get('filing_type', '')})",
                          filings=len(filings), deals_created=results['deals_created'])
                deal = self.parser.parse_filing(filing['accession_number'], filing.get('filing_type'))
                status = self.parser.last_status
                if deal and status == 'created':
                    results['deals_created'] += 1
                    results['deals_parsed'] += 1
                elif deal and status == 'updated':
                    results['deals_updated'] += 1
                    results['deals_parsed'] += 1
                elif deal and status == 'kept_verified':
                    results['deals_kept_verified'] += 1
                elif status == 'rejected':
                    results['deals_rejected'] += 1
                    if len(results['rejections']) < 50:
                        results['rejections'].append({'accession_number': filing['accession_number'],
                                                      'company_name': filing.get('company_name', ''),
                                                      'reason': self.parser.last_error})
                else:
                    results['parsing_failed'] += 1
            _progress("parse", len(todo), len(todo), "Done", filings=len(filings),
                      deals_created=results['deals_created'])

        if sec_throttled():
            results['warning'] = ("SEC stopped answering (HTTP 403/429: request rate limit). The filings and deals "
                                  "found so far are kept - try again in a minute.")
        results['sec_requests'] = sec_request_count()
        results['elapsed_s'] = round((datetime.now() - started).total_seconds(), 1)
        return results

    def list_deals(self, query: Optional[str] = None) -> List[Dict[str, Any]]:
        """Deals for the Deals screen: verified flag, source and, for unverified rows, why they look doubtful."""
        deals = self.db.search_deals({'query': query} if query else {})
        for d in deals:
            d['verified'] = int(d.get('verified') or 0)
            if not d.get('source'):
                d['source'] = 'SEC EDGAR (auto-extracted)' if d.get('data_source') == 'SEC EDGAR' else 'User'
            d['quality_issues'] = [] if d['verified'] else deal_quality_issues(d)
        return deals

    def list_filings(self, limit: int = 500) -> List[Dict[str, Any]]:
        rows = self.db.list_filings(limit)
        for r in rows:
            extra = {}
            try:
                extra = json.loads(r.pop('extracted_data', None) or '{}') or {}
            except ValueError:
                pass
            r.update({k: extra.get(k, '') for k in ('ticker', 'industry', 'deal_indicators')})
            url = r.get('filing_url') or ''
            if not url or 'browse-edgar' in url:
                r['filing_url'] = filing_index_url(r.get('cik', ''), r.get('accession_number', '')) or url
            r['parsed_flag'] = int(r.get('parsed_flag') or 0)
        return rows

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
        print(f"Tracking M&A history for {ticker}...", file=sys.stderr)

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

def _missing_fields_error(fields: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    missing = [k for k, label in (('acquirer_name', 'acquirer'), ('target_name', 'target'))
               if not str(fields.get(k) or '').strip()]
    if not missing:
        return None
    labels = {'acquirer_name': 'Acquirer', 'target_name': 'Target'}
    return {"success": False, "missing_inputs": missing,
            "error": "Enter the " + " and the ".join(labels[k].lower() for k in missing) + " name to save the deal."}


def main():
    """CLI entry point - outputs JSON for C++ integration"""
    if len(sys.argv) < 2:
        print(json.dumps({"success": False,
                          "error": "No command specified. Usage: deal_tracker.py <command> [args...]"}))
        sys.exit(1)

    command = sys.argv[1]
    tracker = MADealTracker()

    try:
        if command in ("update", "scan"):
            # scan <days> [max_filings] [forms_csv]  ("scan" is the name the Qt MAAnalyticsService uses)
            days_back = int(sys.argv[2]) if len(sys.argv) > 2 else 7
            max_filings = int(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_MAX_FILINGS
            forms = [f.strip() for f in sys.argv[4].split(',') if f.strip()] if len(sys.argv) > 4 else None
            result = tracker.update_deal_database(days_back=days_back, max_filings=max_filings, forms=forms)

            slim_filings = [
                {
                    'accession_number': f.get('accession_number', ''),
                    'filing_type': f.get('filing_type', ''),
                    'filing_date': f.get('filing_date', ''),
                    'company_name': f.get('company_name', ''),
                    'ticker': f.get('ticker', ''),
                    'cik': f.get('cik', ''),
                    'items': f.get('items', ''),
                    'industry': f.get('industry', ''),
                    'filing_url': f.get('filing_url', ''),
                    'confidence_score': f.get('confidence_score', 0),
                    'deal_indicators': f.get('deal_indicators', ''),
                }
                for f in result.get('filings', [])
            ]
            data = {k: v for k, v in result.items() if k != 'filings'}
            data['filings'] = slim_filings
            print(json.dumps({"success": True, "data": data}, default=str))

        elif command == "list":
            deals = tracker.list_deals()
            print(json.dumps({"success": True, "data": deals, "count": len(deals),
                              "verified_count": sum(1 for d in deals if d['verified'])}, default=str))

        elif command == "search":
            if len(sys.argv) < 3:
                raise ValueError("Search text required")
            deals = tracker.list_deals(sys.argv[2])
            print(json.dumps({"success": True, "data": deals, "count": len(deals)}, default=str))

        elif command == "filings":
            limit = int(sys.argv[2]) if len(sys.argv) > 2 else 500
            rows = tracker.list_filings(limit)
            print(json.dumps({"success": True, "data": rows, "count": len(rows)}, default=str))

        elif command == "upsert":
            if len(sys.argv) < 3:
                raise ValueError("Deal fields (JSON) required")
            fields = json.loads(sys.argv[2])
            if not isinstance(fields, dict):
                raise ValueError("Deal fields must be a JSON object")
            err = _missing_fields_error(fields)
            if err:
                print(json.dumps(err))
                sys.exit(1)
            deal_id = tracker.db.upsert_user_deal(fields, verified=bool(fields.get('verified', True)))
            deal = tracker.db.get_deal_by_id(deal_id) or {'deal_id': deal_id}
            deal['verified'] = int(deal.get('verified') or 0)
            print(json.dumps({"success": True, "data": deal}, default=str))

        elif command == "verify":
            if len(sys.argv) < 4:
                raise ValueError("Deal id and 0/1 required")
            if not tracker.db.set_verified(sys.argv[2], sys.argv[3] not in ('0', 'false', 'False')):
                raise ValueError(f"Deal {sys.argv[2]} not found")
            print(json.dumps({"success": True, "data": {"deal_id": sys.argv[2],
                                                        "verified": 0 if sys.argv[3] in ('0', 'false', 'False') else 1}}))

        elif command == "delete":
            if len(sys.argv) < 3:
                raise ValueError("Deal id required")
            if not tracker.db.delete_deal(sys.argv[2]):
                raise ValueError(f"Deal {sys.argv[2]} not found")
            print(json.dumps({"success": True, "data": {"deal_id": sys.argv[2], "deleted": True}}))

        elif command == "track":
            if len(sys.argv) < 3:
                raise ValueError("Company ticker required")
            print(json.dumps({"success": True, "data": tracker.track_company(sys.argv[2])}, default=str))

        elif command == "summary":
            days = int(sys.argv[2]) if len(sys.argv) > 2 else 30
            print(json.dumps({"success": True, "data": tracker.get_recent_deals_summary(days=days)}, default=str))

        elif command == "comps":
            if len(sys.argv) < 4:
                raise ValueError("Industry and target value required")
            comp_table = tracker.build_transaction_comp_table(sys.argv[2], float(sys.argv[3]))
            print(json.dumps({"success": True, "data": comp_table}, default=str))

        else:
            print(json.dumps({"success": False,
                              "error": f"Unknown command: {command}. Available: scan, list, search, filings, "
                                       "upsert, verify, delete, track, summary, comps"}))
            sys.exit(1)

    except Exception as e:
        print(json.dumps({"success": False, "error": str(e), "command": command}))
        sys.exit(1)
    finally:
        tracker.close()

if __name__ == '__main__':
    main()
