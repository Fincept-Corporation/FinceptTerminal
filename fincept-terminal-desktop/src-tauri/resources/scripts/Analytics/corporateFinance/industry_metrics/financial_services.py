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

if __name__ == '__main__':
    analyzer = FinancialServicesMetrics()

    bank = analyzer.calculate_bank_metrics(
        total_assets=50_000_000_000,
        deposits=40_000_000_000,
        loans=35_000_000_000,
        net_interest_margin=3.2,
        efficiency_ratio=58,
        npa_ratio=1.2,
        tier1_capital_ratio=12.5,
        roe=13.5
    )

    print("=== BANK M&A METRICS ===\n")
    print(f"Total Assets: ${bank['total_assets']:,.0f}")
    print(f"Loan/Deposit Ratio: {bank['loan_to_deposit_ratio']:.1f}%")
    print(f"ROE: {bank['roe']:.1f}%")
    print(f"Efficiency Ratio: {bank['efficiency_ratio']:.1f}%")
    print(f"NPA Ratio: {bank['npa_ratio']:.1f}%")
    print(f"\nProfitability Score: {bank['profitability_score']}/100")
    print(f"Asset Quality Score: {bank['asset_quality_score']:.0f}/100")
    print(f"\nPrice/Tangible Book Range:")
    ptbv = bank['price_to_tangible_book_range']
    print(f"  {ptbv['low']:.2f}x - {ptbv['high']:.2f}x (Mid: {ptbv['mid']:.2f}x)")
    print(f"Core Deposit Premium: ${bank['core_deposit_premium']:,.0f}")

    wealth = analyzer.calculate_wealth_management_metrics(
        aum=75_000_000_000,
        net_flows=3_000_000_000,
        advisory_fee_rate=0.70,
        client_retention=93,
        advisor_productivity=125_000_000,
        operating_margin=22
    )

    print("\n\n=== WEALTH MANAGEMENT METRICS ===")
    print(f"AUM: ${wealth['aum']:,.0f}")
    print(f"Organic Growth: {wealth['organic_growth_rate']:.1f}%")
    print(f"Advisory Fee Rate: {wealth['advisory_fee_rate']:.2f}%")
    print(f"Quality Score: {wealth['wealth_quality_score']}/100")
    print(f"\nAUM Multiple Range:")
    aum_mult = wealth['aum_multiple_range']
    print(f"  {aum_mult['low']:.2f}% - {aum_mult['high']:.2f}%")

    benchmarks = analyzer.financial_services_benchmarks()
    print("\n\n=== FINANCIAL SERVICES M&A BENCHMARKS ===")
    for sector, data in benchmarks.items():
        print(f"\n{sector.upper()}:")
        print(f"  Key Drivers: {data['key_drivers']}")
