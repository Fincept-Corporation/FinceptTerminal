"""Deal Comparison and Analysis Tool"""
from typing import Dict, Any, List, Optional
import numpy as np
from dataclasses import dataclass

@dataclass
class Deal:
    deal_id: str
    target_name: str
    acquirer_name: str
    deal_value: float
    offer_price_per_share: Optional[float]
    premium_1day: float
    payment_cash_pct: float
    payment_stock_pct: float
    ev_revenue: Optional[float]
    ev_ebitda: Optional[float]
    synergies: Optional[float]
    status: str
    industry: str
    announced_date: str

class DealComparator:
    """Compare multiple M&A deals side-by-side"""

    def compare_deals(self, deals: List[Deal]) -> Dict[str, Any]:
        """Compare multiple deals across key metrics"""

        if len(deals) < 2:
            raise ValueError("Need at least 2 deals to compare")

        comparison_table = []

        for deal in deals:
            comparison_table.append({
                'deal_id': deal.deal_id,
                'target': deal.target_name,
                'acquirer': deal.acquirer_name,
                'deal_value': deal.deal_value,
                'offer_price': deal.offer_price_per_share,
                'premium_1day': deal.premium_1day,
                'payment_cash_pct': deal.payment_cash_pct,
                'payment_stock_pct': deal.payment_stock_pct,
                'ev_revenue': deal.ev_revenue,
                'ev_ebitda': deal.ev_ebitda,
                'synergies': deal.synergies,
                'status': deal.status,
                'industry': deal.industry,
                'announced_date': deal.announced_date
            })

        deal_values = [d.deal_value for d in deals]
        premiums = [d.premium_1day for d in deals]
        ev_revenues = [d.ev_revenue for d in deals if d.ev_revenue is not None]
        ev_ebitdas = [d.ev_ebitda for d in deals if d.ev_ebitda is not None]

        statistics = {
            'deal_value': {
                'min': min(deal_values),
                'max': max(deal_values),
                'mean': np.mean(deal_values),
                'median': np.median(deal_values)
            },
            'premium_1day': {
                'min': min(premiums),
                'max': max(premiums),
                'mean': np.mean(premiums),
                'median': np.median(premiums)
            }
        }

        if ev_revenues:
            statistics['ev_revenue'] = {
                'min': min(ev_revenues),
                'max': max(ev_revenues),
                'mean': np.mean(ev_revenues),
                'median': np.median(ev_revenues)
            }

        if ev_ebitdas:
            statistics['ev_ebitda'] = {
                'min': min(ev_ebitdas),
                'max': max(ev_ebitdas),
                'mean': np.mean(ev_ebitdas),
                'median': np.median(ev_ebitdas)
            }

        return {
            'num_deals': len(deals),
            'comparison_table': comparison_table,
            'statistics': statistics
        }

    def rank_deals(self, deals: List[Deal],
                  criteria: str = 'premium') -> Dict[str, Any]:
        """Rank deals by specific criteria"""

        ranking_map = {
            'premium': lambda d: d.premium_1day,
            'deal_value': lambda d: d.deal_value,
            'ev_revenue': lambda d: d.ev_revenue if d.ev_revenue else 0,
            'ev_ebitda': lambda d: d.ev_ebitda if d.ev_ebitda else 0,
            'synergies': lambda d: d.synergies if d.synergies else 0
        }

        if criteria not in ranking_map:
            raise ValueError(f"Invalid criteria. Choose from: {list(ranking_map.keys())}")

        sorted_deals = sorted(deals, key=ranking_map[criteria], reverse=True)

        rankings = []
        for rank, deal in enumerate(sorted_deals, 1):
            rankings.append({
                'rank': rank,
                'deal_id': deal.deal_id,
                'target': deal.target_name,
                'acquirer': deal.acquirer_name,
                'metric_value': ranking_map[criteria](deal)
            })

        return {
            'ranking_criteria': criteria,
            'rankings': rankings
        }

    def payment_structure_analysis(self, deals: List[Deal]) -> Dict[str, Any]:
        """Analyze payment structures across deals"""

        all_cash_count = sum(1 for d in deals if d.payment_cash_pct == 100)
        all_stock_count = sum(1 for d in deals if d.payment_stock_pct == 100)
        mixed_count = len(deals) - all_cash_count - all_stock_count

        avg_cash_pct = np.mean([d.payment_cash_pct for d in deals])
        avg_stock_pct = np.mean([d.payment_stock_pct for d in deals])

        return {
            'total_deals': len(deals),
            'all_cash_deals': all_cash_count,
            'all_stock_deals': all_stock_count,
            'mixed_deals': mixed_count,
            'average_cash_pct': avg_cash_pct,
            'average_stock_pct': avg_stock_pct,
            'distribution': {
                'all_cash_pct': (all_cash_count / len(deals) * 100) if deals else 0,
                'all_stock_pct': (all_stock_count / len(deals) * 100) if deals else 0,
                'mixed_pct': (mixed_count / len(deals) * 100) if deals else 0
            }
        }

    def industry_analysis(self, deals: List[Deal]) -> Dict[str, Any]:
        """Analyze deals by industry"""

        industry_groups = {}

        for deal in deals:
            if deal.industry not in industry_groups:
                industry_groups[deal.industry] = []
            industry_groups[deal.industry].append(deal)

        industry_stats = {}

        for industry, industry_deals in industry_groups.items():
            deal_values = [d.deal_value for d in industry_deals]
            premiums = [d.premium_1day for d in industry_deals]

            industry_stats[industry] = {
                'count': len(industry_deals),
                'total_value': sum(deal_values),
                'avg_deal_value': np.mean(deal_values),
                'avg_premium': np.mean(premiums),
                'median_premium': np.median(premiums)
            }

        return {
            'industries': list(industry_groups.keys()),
            'industry_statistics': industry_stats
        }

    def premium_benchmarking(self, target_deal: Deal,
                           comparable_deals: List[Deal]) -> Dict[str, Any]:
        """Benchmark target deal premium against comparables"""

        comp_premiums = [d.premium_1day for d in comparable_deals]

        percentile_25 = np.percentile(comp_premiums, 25)
        percentile_50 = np.percentile(comp_premiums, 50)
        percentile_75 = np.percentile(comp_premiums, 75)

        target_premium = target_deal.premium_1day

        if target_premium < percentile_25:
            position = "below 25th percentile"
        elif target_premium < percentile_50:
            position = "between 25th and 50th percentile"
        elif target_premium < percentile_75:
            position = "between 50th and 75th percentile"
        else:
            position = "above 75th percentile"

        return {
            'target_deal': {
                'deal_id': target_deal.deal_id,
                'target': target_deal.target_name,
                'premium': target_premium
            },
            'comparable_premiums': {
                '25th_percentile': percentile_25,
                'median': percentile_50,
                '75th_percentile': percentile_75,
                'mean': np.mean(comp_premiums),
                'min': min(comp_premiums),
                'max': max(comp_premiums)
            },
            'target_position': position,
            'above_median': target_premium > percentile_50,
            'num_comparables': len(comparable_deals)
        }

    def valuation_multiple_comparison(self, deals: List[Deal],
                                     multiple_type: str = 'ev_ebitda') -> Dict[str, Any]:
        """Compare valuation multiples across deals"""

        if multiple_type == 'ev_ebitda':
            multiples = [(d.target_name, d.ev_ebitda) for d in deals if d.ev_ebitda is not None]
        elif multiple_type == 'ev_revenue':
            multiples = [(d.target_name, d.ev_revenue) for d in deals if d.ev_revenue is not None]
        else:
            raise ValueError("Invalid multiple_type. Use 'ev_ebitda' or 'ev_revenue'")

        if not multiples:
            return {'error': f'No {multiple_type} data available for deals'}

        companies, values = zip(*multiples)

        return {
            'multiple_type': multiple_type,
            'deals': [{'company': c, 'multiple': v} for c, v in multiples],
            'statistics': {
                'min': min(values),
                'max': max(values),
                'mean': np.mean(values),
                'median': np.median(values),
                'std': np.std(values)
            },
            'num_deals_with_data': len(multiples)
        }

    def synergy_analysis(self, deals: List[Deal]) -> Dict[str, Any]:
        """Analyze synergies across deals"""

        deals_with_synergies = [d for d in deals if d.synergies is not None]

        if not deals_with_synergies:
            return {'error': 'No synergy data available'}

        synergies = [d.synergies for d in deals_with_synergies]
        synergy_pct = [(d.synergies / d.deal_value * 100) for d in deals_with_synergies]

        return {
            'deals_with_synergy_data': len(deals_with_synergies),
            'total_synergies': sum(synergies),
            'synergy_statistics': {
                'min': min(synergies),
                'max': max(synergies),
                'mean': np.mean(synergies),
                'median': np.median(synergies)
            },
            'synergy_as_pct_of_deal': {
                'min': min(synergy_pct),
                'max': max(synergy_pct),
                'mean': np.mean(synergy_pct),
                'median': np.median(synergy_pct)
            }
        }

def main():
    """CLI entry point - outputs JSON for Tauri integration"""
    import sys
    import json

    if len(sys.argv) < 2:
        result = {"success": False, "error": "No command specified"}
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]

    def _parse_deals(deals_data):
        """Parse deal dicts into Deal objects"""
        deals = []
        for d in deals_data:
            deals.append(Deal(
                deal_id=d.get('deal_id', ''),
                target_name=d.get('target_name', d.get('target', '')),
                acquirer_name=d.get('acquirer_name', d.get('acquirer', '')),
                deal_value=d.get('deal_value', 0),
                offer_price_per_share=d.get('offer_price_per_share', d.get('offer_price', None)),
                premium_1day=d.get('premium_1day', d.get('premium', 0)),
                payment_cash_pct=d.get('payment_cash_pct', d.get('cash_pct', 0)),
                payment_stock_pct=d.get('payment_stock_pct', d.get('stock_pct', 0)),
                ev_revenue=d.get('ev_revenue', None),
                ev_ebitda=d.get('ev_ebitda', None),
                synergies=d.get('synergies', None),
                status=d.get('status', d.get('deal_status', '')),
                industry=d.get('industry', ''),
                announced_date=d.get('announced_date', d.get('announcement_date', ''))
            ))
        return deals

    try:
        if command == "compare":
            # Rust sends: "compare" deals_json
            if len(sys.argv) < 3:
                raise ValueError("Deals data required")

            deals_data = json.loads(sys.argv[2])
            deals = _parse_deals(deals_data)
            comparator = DealComparator()

            if len(deals) >= 2:
                analysis = comparator.compare_deals(deals)
            else:
                analysis = {"deals_count": len(deals), "deals_data": deals_data}

            result = {"success": True, "data": analysis}
            print(json.dumps(result, default=str))

        elif command == "rank":
            # Rust sends: "rank" deals_json criteria
            if len(sys.argv) < 4:
                raise ValueError("Deals data and criteria required")

            deals_data = json.loads(sys.argv[2])
            criteria = sys.argv[3]
            deals = _parse_deals(deals_data)
            comparator = DealComparator()

            analysis = comparator.rank_deals(deals, criteria=criteria)
            result = {"success": True, "data": analysis}
            print(json.dumps(result, default=str))

        elif command == "benchmark":
            # Rust sends: "benchmark" target_deal_json comparable_deals_json
            if len(sys.argv) < 4:
                raise ValueError("Target deal and comparable deals required")

            target_deal_data = json.loads(sys.argv[2])
            comparable_deals_data = json.loads(sys.argv[3])

            target_deals = _parse_deals([target_deal_data])
            comparable_deals = _parse_deals(comparable_deals_data)
            comparator = DealComparator()

            if target_deals and comparable_deals:
                analysis = comparator.premium_benchmarking(target_deals[0], comparable_deals)
            else:
                analysis = {"error": "Insufficient data for benchmarking"}

            result = {"success": True, "data": analysis}
            print(json.dumps(result, default=str))

        elif command == "payment_analysis":
            # Rust sends: "payment_analysis" deals_json
            if len(sys.argv) < 3:
                raise ValueError("Deals data required")

            deals_data = json.loads(sys.argv[2])
            deals = _parse_deals(deals_data)
            comparator = DealComparator()

            analysis = comparator.payment_structure_analysis(deals)
            result = {"success": True, "data": analysis}
            print(json.dumps(result, default=str))

        elif command == "industry_analysis":
            # Rust sends: "industry_analysis" deals_json
            if len(sys.argv) < 3:
                raise ValueError("Deals data required")

            deals_data = json.loads(sys.argv[2])
            deals = _parse_deals(deals_data)
            comparator = DealComparator()

            analysis = comparator.industry_analysis(deals)
            result = {"success": True, "data": analysis}
            print(json.dumps(result, default=str))

        elif command == "valuation_multiples":
            # Rust sends: "valuation_multiples" deals_json multiple_type
            if len(sys.argv) < 3:
                raise ValueError("Deals data required")

            deals_data = json.loads(sys.argv[2])
            multiple_type = sys.argv[3] if len(sys.argv) > 3 else 'ev_ebitda'
            deals = _parse_deals(deals_data)
            comparator = DealComparator()

            analysis = comparator.valuation_multiple_comparison(deals, multiple_type=multiple_type)
            result = {"success": True, "data": analysis}
            print(json.dumps(result, default=str))

        elif command == "synergy_analysis":
            # Rust sends: "synergy_analysis" deals_json
            if len(sys.argv) < 3:
                raise ValueError("Deals data required")

            deals_data = json.loads(sys.argv[2])
            deals = _parse_deals(deals_data)
            comparator = DealComparator()

            analysis = comparator.synergy_analysis(deals)
            result = {"success": True, "data": analysis}
            print(json.dumps(result, default=str))

        else:
            result = {"success": False, "error": f"Unknown command: {command}. Available: compare, rank, benchmark, payment_analysis, industry_analysis, valuation_multiples, synergy_analysis"}
            print(json.dumps(result))
            sys.exit(1)

    except Exception as e:
        result = {"success": False, "error": str(e)}
        print(json.dumps(result))
        sys.exit(1)

if __name__ == '__main__':
    main()
