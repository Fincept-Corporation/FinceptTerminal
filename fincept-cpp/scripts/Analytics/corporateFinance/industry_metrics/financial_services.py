"""Financial Services Industry M&A Metrics"""
from typing import Dict, Any, List, Optional

class FinancialServicesMetrics:
    """Financial services sector M&A metrics"""

    def calculate_bank_metrics(self, total_assets: float,
                               deposits: float,
                               loans: float,
                               net_interest_margin: float,
                               efficiency_ratio: float,
                               npa_ratio: float,
                               tier1_capital_ratio: float,
                               roe: float) -> Dict[str, Any]:
        """Calculate banking metrics for M&A"""

        loan_to_deposit_ratio = (loans / deposits) if deposits > 0 else 0

        tangible_book_multiple = self._bank_price_to_tangible_book(roe, efficiency_ratio, npa_ratio)

        asset_quality_score = 100 - (npa_ratio * 1000)

        profitability_score = 0
        if roe >= 15:
            profitability_score += 30
        elif roe >= 12:
            profitability_score += 20

        if net_interest_margin >= 3.5:
            profitability_score += 25
        elif net_interest_margin >= 3.0:
            profitability_score += 15

        if efficiency_ratio <= 50:
            profitability_score += 25
        elif efficiency_ratio <= 60:
            profitability_score += 15

        if npa_ratio <= 1.0:
            profitability_score += 20
        elif npa_ratio <= 2.0:
            profitability_score += 10

        return {
            'total_assets': total_assets,
            'deposits': deposits,
            'loans': loans,
            'loan_to_deposit_ratio': loan_to_deposit_ratio * 100,
            'net_interest_margin': net_interest_margin,
            'efficiency_ratio': efficiency_ratio,
            'npa_ratio': npa_ratio,
            'tier1_capital_ratio': tier1_capital_ratio,
            'roe': roe,
            'asset_quality_score': max(0, asset_quality_score),
            'profitability_score': profitability_score,
            'price_to_tangible_book_range': tangible_book_multiple,
            'core_deposit_premium': self._calculate_deposit_premium(deposits, profitability_score)
        }

    def _bank_price_to_tangible_book(self, roe: float, efficiency: float, npa: float) -> Dict[str, float]:
        """Calculate P/TBV multiple range for banks"""

        base = 1.0

        if roe >= 15:
            base = 1.8
        elif roe >= 12:
            base = 1.4
        elif roe >= 10:
            base = 1.1

        if efficiency <= 50:
            base *= 1.2
        elif efficiency <= 60:
            base *= 1.1

        if npa <= 1.0:
            base *= 1.15
        elif npa >= 3.0:
            base *= 0.85

        return {
            'low': base * 0.85,
            'mid': base,
            'high': base * 1.15
        }

    def _calculate_deposit_premium(self, deposits: float, quality_score: int) -> float:
        """Calculate core deposit intangible premium"""

        base_premium = 0.03

        if quality_score >= 80:
            base_premium = 0.06
        elif quality_score >= 60:
            base_premium = 0.045

        return deposits * base_premium

    def calculate_insurance_metrics(self, gross_written_premium: float,
                                   combined_ratio: float,
                                   investment_yield: float,
                                   loss_ratio: float,
                                   expense_ratio: float,
                                   roe: float,
                                   book_value: float) -> Dict[str, Any]:
        """Calculate insurance company metrics"""

        underwriting_profit_margin = 100 - combined_ratio

        insurance_quality = 0

        if combined_ratio <= 95:
            insurance_quality += 30
        elif combined_ratio <= 100:
            insurance_quality += 20

        if loss_ratio <= 60:
            insurance_quality += 25
        elif loss_ratio <= 70:
            insurance_quality += 15

        if expense_ratio <= 25:
            insurance_quality += 20
        elif expense_ratio <= 30:
            insurance_quality += 10

        if roe >= 12:
            insurance_quality += 25
        elif roe >= 10:
            insurance_quality += 15

        price_to_book = self._insurance_price_to_book(roe, combined_ratio)

        return {
            'gross_written_premium': gross_written_premium,
            'combined_ratio': combined_ratio,
            'underwriting_profit_margin': underwriting_profit_margin,
            'loss_ratio': loss_ratio,
            'expense_ratio': expense_ratio,
            'investment_yield': investment_yield,
            'roe': roe,
            'book_value': book_value,
            'insurance_quality_score': insurance_quality,
            'price_to_book_range': price_to_book,
            'premium_multiple': self._premium_multiple(insurance_quality)
        }

    def _insurance_price_to_book(self, roe: float, combined_ratio: float) -> Dict[str, float]:
        """P/B multiple for insurance companies"""

        base = 1.0

        if roe >= 15:
            base = 1.6
        elif roe >= 12:
            base = 1.3
        elif roe >= 10:
            base = 1.1

        if combined_ratio <= 95:
            base *= 1.2
        elif combined_ratio > 100:
            base *= 0.85

        return {
            'low': base * 0.85,
            'mid': base,
            'high': base * 1.15
        }

    def _premium_multiple(self, quality: int) -> Dict[str, float]:
        """Premium revenue multiple"""

        if quality >= 80:
            base = 1.5
        elif quality >= 60:
            base = 1.2
        else:
            base = 0.9

        return {
            'low': base * 0.85,
            'mid': base,
            'high': base * 1.15
        }

    def calculate_wealth_management_metrics(self, aum: float,
                                           net_flows: float,
                                           advisory_fee_rate: float,
                                           client_retention: float,
                                           advisor_productivity: float,
                                           operating_margin: float) -> Dict[str, Any]:
        """Calculate wealth/asset management metrics"""

        organic_growth_rate = (net_flows / aum * 100) if aum > 0 else 0

        revenue = aum * advisory_fee_rate

        wealth_quality = 0

        if advisory_fee_rate >= 0.75:
            wealth_quality += 25
        elif advisory_fee_rate >= 0.60:
            wealth_quality += 15

        if client_retention >= 95:
            wealth_quality += 25
        elif client_retention >= 90:
            wealth_quality += 15

        if organic_growth_rate >= 5:
            wealth_quality += 20
        elif organic_growth_rate >= 3:
            wealth_quality += 10

        if operating_margin >= 25:
            wealth_quality += 20
        elif operating_margin >= 20:
            wealth_quality += 10

        if advisor_productivity >= 150_000_000:
            wealth_quality += 10

        aum_multiple = self._aum_multiple(wealth_quality, advisory_fee_rate)

        return {
            'aum': aum,
            'net_flows': net_flows,
            'organic_growth_rate': organic_growth_rate,
            'advisory_fee_rate': advisory_fee_rate,
            'estimated_revenue': revenue,
            'client_retention': client_retention,
            'advisor_productivity': advisor_productivity,
            'operating_margin': operating_margin,
            'wealth_quality_score': wealth_quality,
            'aum_multiple_range': aum_multiple,
            'revenue_multiple_range': self._wealth_revenue_multiple(wealth_quality)
        }

    def _aum_multiple(self, quality: int, fee_rate: float) -> Dict[str, float]:
        """AUM-based multiple"""

        base = 0.02

        if quality >= 80:
            base = 0.04
        elif quality >= 60:
            base = 0.03

        if fee_rate >= 0.75:
            base *= 1.2

        return {
            'low': base * 0.85,
            'mid': base,
            'high': base * 1.15
        }

    def _wealth_revenue_multiple(self, quality: int) -> Dict[str, float]:
        """Revenue multiple for wealth management"""

        if quality >= 80:
            base = 4.5
        elif quality >= 60:
            base = 3.5
        else:
            base = 2.5

        return {
            'low': base * 0.85,
            'mid': base,
            'high': base * 1.15
        }

    def calculate_fintech_metrics(self, transaction_volume: float,
                                 take_rate: float,
                                 user_growth: float,
                                 ltv_cac: float,
                                 revenue_growth: float,
                                 regulatory_licenses: List[str]) -> Dict[str, Any]:
        """Calculate fintech metrics"""

        revenue = transaction_volume * take_rate

        fintech_quality = 0

        if revenue_growth >= 100:
            fintech_quality += 30
        elif revenue_growth >= 50:
            fintech_quality += 20

        if ltv_cac >= 3:
            fintech_quality += 25
        elif ltv_cac >= 2:
            fintech_quality += 15

        if user_growth >= 50:
            fintech_quality += 20
        elif user_growth >= 30:
            fintech_quality += 10

        if take_rate >= 2.5:
            fintech_quality += 15
        elif take_rate >= 1.5:
            fintech_quality += 10

        fintech_quality += min(len(regulatory_licenses) * 5, 10)

        revenue_multiple = self._fintech_multiple(fintech_quality, revenue_growth)

        return {
            'transaction_volume': transaction_volume,
            'take_rate': take_rate,
            'revenue': revenue,
            'user_growth': user_growth,
            'ltv_cac': ltv_cac,
            'revenue_growth': revenue_growth,
            'regulatory_licenses': regulatory_licenses,
            'fintech_quality_score': fintech_quality,
            'revenue_multiple_range': revenue_multiple,
            'gmv_multiple': (revenue_multiple['mid'] * take_rate / 100)
        }

    def _fintech_multiple(self, quality: int, growth: float) -> Dict[str, float]:
        """Fintech revenue multiple"""

        if quality >= 80:
            base = 10.0
        elif quality >= 60:
            base = 7.0
        else:
            base = 4.0

        if growth >= 100:
            base *= 1.3

        return {
            'low': base * 0.8,
            'mid': base,
            'high': base * 1.2
        }

    def financial_services_benchmarks(self) -> Dict[str, Any]:
        """Financial services M&A benchmarks"""

        return {
            'banks': {
                'price_to_tangible_book': {'25th': 1.1, 'median': 1.4, '75th': 1.8},
                'price_to_earnings': {'25th': 10, 'median': 13, '75th': 16},
                'key_drivers': 'ROE, efficiency ratio, asset quality'
            },
            'insurance': {
                'price_to_book': {'25th': 0.9, 'median': 1.2, '75th': 1.5},
                'price_to_earnings': {'25th': 8, 'median': 11, '75th': 14},
                'key_drivers': 'Combined ratio, underwriting discipline, ROE'
            },
            'wealth_management': {
                'aum_multiple_pct': {'25th': 2.0, 'median': 3.0, '75th': 4.5},
                'revenue_multiple': {'25th': 2.5, 'median': 3.5, '75th': 5.0},
                'key_drivers': 'Fee rates, client retention, organic growth'
            },
            'fintech': {
                'revenue_multiple': {'25th': 5.0, 'median': 8.0, '75th': 12.0},
                'key_drivers': 'Growth rate, unit economics, regulatory moat'
            }
        }

def _build_financial_standard_output(analysis: dict, sector: str) -> dict:
    """Build valuation_metrics, benchmarks, and insights for financial services output."""
    benchmarks_db = {
        'banking': {
            'net_interest_margin': {'p25': 2.5, 'median': 3.2, 'p75': 3.8},
            'efficiency_ratio': {'p25': 50, 'median': 58, 'p75': 65},
            'roe': {'p25': 8, 'median': 12, 'p75': 16},
            'npa_ratio': {'p25': 0.4, 'median': 0.9, 'p75': 1.8},
        },
        'insurance': {
            'combined_ratio': {'p25': 88, 'median': 95, 'p75': 101},
            'loss_ratio': {'p25': 58, 'median': 65, 'p75': 72},
            'roe': {'p25': 8, 'median': 12, 'p75': 16},
            'investment_yield': {'p25': 3.0, 'median': 4.2, 'p75': 5.5},
        },
        'asset_management': {
            'aum': {'p25': 20, 'median': 80, 'p75': 500},
            'operating_margin': {'p25': 20, 'median': 32, 'p75': 42},
            'fee_rate': {'p25': 25, 'median': 45, 'p75': 70},
        },
    }

    vm = {}
    insights = []

    if sector == 'banking':
        vm = {
            'roe_pct': round(analysis.get('roe', 0), 1),
            'net_interest_margin_pct': round(analysis.get('net_interest_margin', 0), 2),
            'efficiency_ratio_pct': round(analysis.get('efficiency_ratio', 0), 1),
            'npa_ratio_pct': round(analysis.get('npa_ratio', 0), 2),
            'profitability_score': analysis.get('profitability_score', 0),
            'asset_quality_score': round(analysis.get('asset_quality_score', 0), 0),
        }
        mr = analysis.get('price_to_tangible_book_range', {})
        vm['ptbv_mid'] = round(mr.get('mid', 0), 2)
        insights.append(f"Price-to-tangible-book range: {mr.get('low', 0):.2f}x – {mr.get('high', 0):.2f}x based on ROE and asset quality.")
        if analysis.get('roe', 0) >= 12:
            insights.append("ROE >= 12% supports a premium valuation relative to book value for banking M&A.")
        if analysis.get('npa_ratio', 0) <= 1.0:
            insights.append("NPA ratio <= 1% indicates strong asset quality — low credit risk premium in valuation.")

    elif sector == 'insurance':
        vm = {
            'combined_ratio_pct': round(analysis.get('combined_ratio', 0), 1),
            'underwriting_margin_pct': round(analysis.get('underwriting_profit_margin', 0), 1),
            'loss_ratio_pct': round(analysis.get('loss_ratio', 0), 1),
            'roe_pct': round(analysis.get('roe', 0), 1),
            'insurance_quality_score': analysis.get('insurance_quality_score', 0),
        }
        mr = analysis.get('price_to_book_range', {})
        vm['ptb_mid'] = round(mr.get('mid', 0), 2)
        insights.append(f"Price-to-book range: {mr.get('low', 0):.2f}x – {mr.get('high', 0):.2f}x based on underwriting discipline and ROE.")
        if analysis.get('combined_ratio', 100) <= 95:
            insights.append("Combined ratio <= 95% indicates consistent underwriting profitability — key quality signal.")

    elif sector == 'asset_management':
        vm = {
            'aum': round(analysis.get('aum', 0), 1),
            'organic_growth_rate_pct': round(analysis.get('organic_growth_rate', 0), 2),
            'operating_margin_pct': round(analysis.get('operating_margin', 0), 1),
            'estimated_revenue': round(analysis.get('estimated_revenue', 0), 2),
            'wealth_quality_score': analysis.get('wealth_quality_score', 0),
        }
        aum_mr = analysis.get('aum_multiple_range', {})
        vm['aum_multiple_pct_mid'] = round(aum_mr.get('mid', 0) * 100, 2)
        rev_mr = analysis.get('revenue_multiple_range', {})
        vm['revenue_multiple_mid'] = round(rev_mr.get('mid', 0), 1)
        insights.append(f"AUM multiple range: {aum_mr.get('low', 0)*100:.1f}% – {aum_mr.get('high', 0)*100:.1f}% of AUM.")
        insights.append(f"Revenue multiple range: {rev_mr.get('low', 0):.1f}x – {rev_mr.get('high', 0):.1f}x.")
        if analysis.get('organic_growth_rate', 0) >= 5:
            insights.append("Organic flow growth >= 5% demonstrates strong client acquisition and retention momentum.")

    return {
        **analysis,
        'valuation_metrics': vm,
        'benchmarks': benchmarks_db.get(sector, {}),
        'insights': insights,
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

    try:
        if command == "financial":
            if len(sys.argv) < 4:
                raise ValueError("Sector and institution data required")

            sector = sys.argv[2]
            institution_data = json.loads(sys.argv[3])

            analyzer = FinancialServicesMetrics()

            # Map common alternative key names
            key_aliases = {
                'total_deposits': 'deposits', 'total_loans': 'loans',
                'net_interest_income': 'net_interest_margin',
                'tier1_capital': 'tier1_capital_ratio',
                'cet1_ratio': 'tier1_capital_ratio',   # frontend sends cet1_ratio
                'loan_loss_provision_rate': 'npa_ratio',
                'npl_ratio': 'npa_ratio',
                'return_on_equity': 'roe',
                'gross_premiums': 'gross_written_premium',  # frontend sends gross_premiums
            }
            for old_key, new_key in key_aliases.items():
                if old_key in institution_data and new_key not in institution_data:
                    institution_data[new_key] = institution_data.pop(old_key)

            import inspect
            sector_key = sector.lower()

            # Route to appropriate calculation based on sector
            if 'bank' in sector.lower():
                # Provide defaults for required bank params
                institution_data.setdefault('loans', institution_data.get('deposits', 0) * 0.75)
                institution_data.setdefault('npa_ratio', 1.0)
                institution_data.setdefault('roe', 10.0)
                institution_data.setdefault('tier1_capital_ratio', 12.0)
                # Drop unknown keys
                valid_params = set(inspect.signature(analyzer.calculate_bank_metrics).parameters.keys())
                filtered = {k: v for k, v in institution_data.items() if k in valid_params}
                analysis = analyzer.calculate_bank_metrics(**filtered)
                sector_key = 'banking'

            elif 'asset' in sector.lower() or 'asset_management' in sector.lower():
                # Frontend sends: aum, net_flows, fee_rate (bps), operating_margin, active_vs_passive, alpha_generation
                # Python expects: aum, net_flows, advisory_fee_rate (fraction), client_retention, advisor_productivity, operating_margin
                if 'fee_rate' in institution_data and 'advisory_fee_rate' not in institution_data:
                    # Convert bps to fraction (45 bps = 0.0045)
                    institution_data['advisory_fee_rate'] = institution_data.pop('fee_rate') / 10000.0
                institution_data.setdefault('client_retention', 92.0)
                institution_data.setdefault('advisor_productivity', 80_000_000.0)
                # Drop frontend-only keys
                for drop_key in ('active_vs_passive', 'alpha_generation'):
                    institution_data.pop(drop_key, None)
                valid_params = set(inspect.signature(analyzer.calculate_wealth_management_metrics).parameters.keys())
                filtered = {k: v for k, v in institution_data.items() if k in valid_params}
                analysis = analyzer.calculate_wealth_management_metrics(**filtered)
                sector_key = 'asset_management'

            elif 'wealth' in sector.lower():
                valid_params = set(inspect.signature(analyzer.calculate_wealth_management_metrics).parameters.keys())
                filtered = {k: v for k, v in institution_data.items() if k in valid_params}
                analysis = analyzer.calculate_wealth_management_metrics(**filtered)
                sector_key = 'asset_management'

            elif 'insurance' in sector.lower():
                valid_params = set(inspect.signature(analyzer.calculate_insurance_metrics).parameters.keys())
                filtered = {k: v for k, v in institution_data.items() if k in valid_params}
                analysis = analyzer.calculate_insurance_metrics(**filtered)
                sector_key = 'insurance'

            elif 'fintech' in sector.lower():
                valid_params = set(inspect.signature(analyzer.calculate_fintech_metrics).parameters.keys())
                filtered = {k: v for k, v in institution_data.items() if k in valid_params}
                analysis = analyzer.calculate_fintech_metrics(**filtered)
                sector_key = 'fintech'

            else:
                # Default to bank metrics
                institution_data.setdefault('loans', institution_data.get('deposits', 0) * 0.75)
                institution_data.setdefault('npa_ratio', 1.0)
                institution_data.setdefault('tier1_capital_ratio', 12.0)
                valid_params = set(inspect.signature(analyzer.calculate_bank_metrics).parameters.keys())
                filtered = {k: v for k, v in institution_data.items() if k in valid_params}
                analysis = analyzer.calculate_bank_metrics(**filtered)
                sector_key = 'banking'

            analysis_with_standard = _build_financial_standard_output(analysis, sector_key)
            result = {"success": True, "data": analysis_with_standard}
            print(json.dumps(result))

        else:
            result = {"success": False, "error": f"Unknown command: {command}"}
            print(json.dumps(result))
            sys.exit(1)

    except Exception as e:
        result = {"success": False, "error": str(e)}
        print(json.dumps(result))
        sys.exit(1)

if __name__ == '__main__':
    main()
