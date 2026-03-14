"""Comprehensive Valuation Summary"""
import sys
from pathlib import Path
from typing import Dict, Any, List, Optional
from dataclasses import asdict
from datetime import datetime

scripts_path = Path(__file__).parent.parent.parent.parent
sys.path.append(str(scripts_path))

# Add Analytics path for absolute imports
analytics_path = Path(__file__).parent.parent.parent
sys.path.insert(0, str(analytics_path))

# Use absolute imports instead of relative imports
from corporateFinance.valuation.precedent_transactions import PrecedentTransactionAnalyzer
from corporateFinance.valuation.trading_comps import TradingCompsAnalyzer
from corporateFinance.valuation.football_field import FootballFieldChart, ValuationRange
from corporateFinance.deal_database.database_schema import MADatabase

class ValuationSummary:
    def __init__(self, db: Optional[MADatabase] = None):
        self.db = db or MADatabase()
        self.precedent_analyzer = PrecedentTransactionAnalyzer(self.db)
        self.trading_analyzer = TradingCompsAnalyzer()
        self.chart_generator = FootballFieldChart()

    def comprehensive_valuation(self, target_metrics: Dict[str, Any],
                                include_methods: Optional[List[str]] = None) -> Dict[str, Any]:
        """Perform comprehensive valuation using multiple methods"""

        methods = include_methods or ['precedent_transactions', 'trading_comps', 'dcf']

        results = {
            'target_company': target_metrics.get('company_name', 'Target'),
            'analysis_date': datetime.now().strftime('%Y-%m-%d'),
            'target_financials': target_metrics,
            'valuation_methods': {}
        }

        if 'precedent_transactions' in methods:
            precedent_result = self._run_precedent_transactions(target_metrics)
            results['valuation_methods']['precedent_transactions'] = precedent_result

        if 'trading_comps' in methods:
            trading_result = self._run_trading_comps(target_metrics)
            results['valuation_methods']['trading_comps'] = trading_result

        if 'dcf' in methods and target_metrics.get('dcf_valuation'):
            results['valuation_methods']['dcf'] = target_metrics['dcf_valuation']

        valuation_ranges = self._build_valuation_ranges(results['valuation_methods'])
        results['valuation_ranges'] = [asdict(vr) for vr in valuation_ranges]

        if valuation_ranges:
            results['weighted_valuation'] = self.chart_generator.calculate_weighted_valuation(valuation_ranges)
            results['dispersion_analysis'] = self.chart_generator.analyze_valuation_dispersion(valuation_ranges)

        if target_metrics.get('current_market_cap'):
            results['premium_analysis'] = self._calculate_premiums(
                target_metrics['current_market_cap'],
                results.get('weighted_valuation', {}).get('weighted_midpoint', 0)
            )

        return results

    def _run_precedent_transactions(self, target_metrics: Dict[str, Any]) -> Dict[str, Any]:
        """Run precedent transaction analysis"""

        comps = self.precedent_analyzer.find_comparables(target_metrics)

        if not comps:
            return {'error': 'No comparable transactions found'}

        target_financials = {
            'revenue': target_metrics.get('revenue', 0),
            'ebitda': target_metrics.get('ebitda', 0),
            'ebit': target_metrics.get('ebit', 0),
            'net_income': target_metrics.get('net_income', 0)
        }

        valuation_result = self.precedent_analyzer.apply_multiples_valuation(target_financials, comps)
        comp_table = self.precedent_analyzer.build_comp_table(comps)

        return {
            'method': 'Precedent Transactions',
            'comp_count': len(comps),
            'valuation_result': valuation_result,
            'summary_multiples': comp_table['summary_statistics'],
            'comparable_deals': [
                {
                    'date': c.announcement_date,
                    'acquirer': c.acquirer_name,
                    'target': c.target_name,
                    'value': c.deal_value
                }
                for c in comps[:10]
            ]
        }

    def _run_trading_comps(self, target_metrics: Dict[str, Any]) -> Dict[str, Any]:
        """Run trading comps analysis"""

        industry = target_metrics.get('industry')
        custom_tickers = target_metrics.get('comp_tickers')

        comps = self.trading_analyzer.find_comparables(industry, custom_tickers)

        if not comps:
            return {'error': 'No trading comparables found'}

        target_financials = {
            'revenue': target_metrics.get('revenue', 0),
            'ebitda': target_metrics.get('ebitda', 0),
            'ebit': target_metrics.get('ebit', 0),
            'net_income': target_metrics.get('net_income', 0)
        }

        valuation_result = self.trading_analyzer.apply_trading_multiples(target_financials, comps)
        comp_table = self.trading_analyzer.build_comp_table(comps)

        return {
            'method': 'Trading Comparables',
            'comp_count': len(comps),
            'valuation_result': valuation_result,
            'summary_multiples': comp_table['summary_statistics'],
            'comparable_companies': [
                {
                    'ticker': c.ticker,
                    'company': c.company_name,
                    'market_cap': c.market_cap,
                    'ev_ebitda': c.ev_ebitda
                }
                for c in comps[:10]
            ]
        }

    def _build_valuation_ranges(self, valuation_methods: Dict[str, Any]) -> List[ValuationRange]:
        """Build valuation ranges from all methods"""

        ranges = []

        if 'precedent_transactions' in valuation_methods:
            pt = valuation_methods['precedent_transactions']
            if 'valuation_result' in pt and 'valuations' in pt['valuation_result']:
                vals = pt['valuation_result']['valuations']

                if 'ev_ebitda_q1' in vals and 'ev_ebitda_q3' in vals:
                    low = vals['ev_ebitda_q1']
                    high = vals['ev_ebitda_q3']
                    mid = vals.get('ev_ebitda_median', (low + high) / 2)
                    ranges.append(ValuationRange('Transaction Comps', low, high, mid, weight=1.2))

        if 'trading_comps' in valuation_methods:
            tc = valuation_methods['trading_comps']
            if 'valuation_result' in tc and 'valuations' in tc['valuation_result']:
                vals = tc['valuation_result']['valuations']

                if 'ev_ebitda_q1' in vals and 'ev_ebitda_q3' in vals:
                    low = vals['ev_ebitda_q1']
                    high = vals['ev_ebitda_q3']
                    mid = vals.get('ev_ebitda_median', (low + high) / 2)
                    ranges.append(ValuationRange('Trading Comps', low, high, mid, weight=1.0))

        if 'dcf' in valuation_methods:
            dcf = valuation_methods['dcf']
            if 'valuation_range' in dcf:
                vr = dcf['valuation_range']
                ranges.append(ValuationRange('DCF', vr['low'], vr['high'], vr['base'], weight=1.5))

        return ranges

    def _calculate_premiums(self, current_value: float, implied_value: float) -> Dict[str, float]:
        """Calculate implied premiums"""

        premium_pct = ((implied_value - current_value) / current_value) * 100 if current_value else 0
        premium_dollars = implied_value - current_value

        return {
            'current_value': current_value,
            'implied_value': implied_value,
            'premium_percentage': premium_pct,
            'premium_dollars': premium_dollars
        }

    def generate_football_field(self, comprehensive_result: Dict[str, Any]) -> Dict[str, Any]:
        """Generate football field chart from comprehensive valuation"""

        raw_ranges = comprehensive_result.get('valuation_ranges', [])

        if not raw_ranges:
            return {'error': 'No valuation ranges available'}

        # Reconstruct ValuationRange objects from dicts
        valuation_ranges = []
        for vr in raw_ranges:
            if isinstance(vr, dict):
                valuation_ranges.append(ValuationRange(**vr))
            else:
                valuation_ranges.append(vr)

        current_price = comprehensive_result.get('target_financials', {}).get('current_market_cap')
        offer_price = comprehensive_result.get('target_financials', {}).get('offer_price')

        return self.chart_generator.generate_chart_data(
            valuation_ranges,
            current_price=current_price,
            offer_price=offer_price,
        )

    def generate_executive_summary(self, comprehensive_result: Dict[str, Any]) -> Dict[str, Any]:
        """Generate executive summary of valuation"""

        weighted_val = comprehensive_result.get('weighted_valuation', {})
        dispersion = comprehensive_result.get('dispersion_analysis', {})
        premium = comprehensive_result.get('premium_analysis', {})

        summary = {
            'valuation_date': comprehensive_result['analysis_date'],
            'target_company': comprehensive_result['target_company'],
            'valuation_range': {
                'low': weighted_val.get('weighted_low', 0),
                'midpoint': weighted_val.get('weighted_midpoint', 0),
                'high': weighted_val.get('weighted_high', 0)
            },
            'methods_used': list(comprehensive_result['valuation_methods'].keys()),
            'valuation_dispersion': {
                'coefficient_of_variation': dispersion.get('coefficient_of_variation', 0),
                'range_pct': dispersion.get('range_pct', 0)
            }
        }

        if premium:
            summary['implied_premium'] = {
                'percentage': premium.get('premium_percentage', 0),
                'dollars': premium.get('premium_dollars', 0)
            }

        summary['key_observations'] = self._generate_observations(comprehensive_result)

        return summary

    def _generate_observations(self, comprehensive_result: Dict[str, Any]) -> List[str]:
        """Generate key observations from valuation"""

        observations = []

        dispersion = comprehensive_result.get('dispersion_analysis', {})
        cv = dispersion.get('coefficient_of_variation', 0)

        if cv < 10:
            observations.append("High consensus across valuation methodologies (CV < 10%)")
        elif cv > 20:
            observations.append("Significant dispersion across methodologies suggests valuation uncertainty")

        methods = comprehensive_result.get('valuation_methods', {})

        if 'precedent_transactions' in methods:
            pt = methods['precedent_transactions']
            if pt.get('comp_count', 0) < 5:
                observations.append("Limited precedent transaction data - use with caution")

        if 'trading_comps' in methods:
            tc = methods['trading_comps']
            if tc.get('comp_count', 0) > 10:
                observations.append("Strong trading comps coverage provides robust market benchmark")

        premium = comprehensive_result.get('premium_analysis', {})
        if premium:
            prem_pct = premium.get('premium_percentage', 0)
            if prem_pct > 30:
                observations.append(f"Implied premium of {prem_pct:.1f}% is above typical M&A premiums")
            elif prem_pct < 15:
                observations.append(f"Implied premium of {prem_pct:.1f}% is below market averages")

        return observations

def main():
    """CLI entry point - outputs JSON for Tauri integration"""
    import json

    if len(sys.argv) < 2:
        result = {
            "success": False,
            "error": "No command specified. Usage: valuation_summary.py <command> [args...]"
        }
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]
    summary = ValuationSummary()

    try:
        if command == "comprehensive":
            # comprehensive_valuation(target_metrics)
            if len(sys.argv) < 3:
                raise ValueError("Target metrics JSON required")

            target_metrics = json.loads(sys.argv[2])
            valuation_result = summary.comprehensive_valuation(target_metrics)

            result = {
                "success": True,
                "data": valuation_result
            }
            print(json.dumps(result))

        elif command == "executive_summary":
            # generate_executive_summary(valuation_results)
            if len(sys.argv) < 3:
                raise ValueError("Valuation results JSON required")

            valuation_results = json.loads(sys.argv[2])
            exec_summary = summary.generate_executive_summary(valuation_results)

            result = {
                "success": True,
                "data": exec_summary
            }
            print(json.dumps(result))

        elif command == "football_field":
            # create_football_field_chart(valuation_results)
            if len(sys.argv) < 3:
                raise ValueError("Valuation results JSON required")

            valuation_results = json.loads(sys.argv[2])
            chart_data = summary.generate_football_field(valuation_results)

            result = {
                "success": True,
                "data": chart_data
            }
            print(json.dumps(result))

        else:
            result = {
                "success": False,
                "error": f"Unknown command: {command}. Available: comprehensive, executive_summary, football_field"
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
